// finishhl_probe.cpp -- the oracle's probe for the race finish's highlight progress (development only): a frame hook (group "thunk",
// Engine::Update 0084ed20) that at one chosen frame builds fake finish states on the live race (a win at AIDifficulty 0x78, unique
// tricks, a car with every panel gone / none lost / a mix, the activity types that switch a highlight off) and calls the ORIGINAL
// 00569930 / 006d8530 / 0047baf0 / 0047bed0 / 0047bf00, so each installed port is verified per call (the core runs, then the original
// on the same state; memory and registers compared):
//   REFLEX_PORT=0x00569930,0x006d8530,0x0047baf0,0x0047bed0,0x0047bf00,rx_FinishHlProbeHook REFLEX_PORT_VERIFY=1 RX_FHL_PROBE=1 [RX_FHL_FRAME=1200]
// The probe pokes the live state and puts every poked word back before the next case, so the world continues unchanged.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* a = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, a);
    return out;
}

// pokes with undo
struct Pokes {
    std::vector<std::pair<uint32_t, uint32_t>> saved;
    void set(uint32_t a, uint32_t v) { saved.push_back({a, rd(a)}); wr(a, v); }
    void undo() { for (auto it = saved.rbegin(); it != saved.rend(); ++it) wr(it->first, it->second); saved.clear(); }
};

const uint32_t TRACKING = 0xd92edc;
const uint32_t MGR = 0xdb1410;
const uint32_t K_AIDIFF = 0x00936e48;

uint32_t ui_get(uint32_t key) {
    const uint32_t slot = rx_sp() - 0x40;
    wr(slot, 0xdeadbeef);
    rx_regs in = regs(); in.r[0] = key;
    const uint32_t a[2] = {rd(0xa93fcc), slot};
    rx_regs o = call(0x005dc0c0, in, 2, a);
    return o.r[0] ? rd(slot) : 0xffffffffu;
}
void ui_set(uint32_t key, uint32_t value) {
    rx_regs in = regs(); in.r[1] = rd(0xa93fcc);
    const uint32_t a[2] = {key, value};
    call(0x005dbfb0, in, 2, a);
}
}  // namespace

RX_PORT(0x0084ed20, FinishHlProbeHook, "thunk", 0) {
    static const bool on = std::getenv("RX_FHL_PROBE") != nullptr;
    if (!on) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_FHL_FRAME") ? uint32_t(std::atoi(std::getenv("RX_FHL_FRAME"))) : 1200;
    static uint32_t frame = 0;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;

    const uint32_t G = rd(0xa946b8);
    const uint32_t A = rd(G + 0x7460 + 0x94);
    if (!A) { std::fprintf(stderr, "[fhl probe] no activity at frame %u\n", frame); return reflex_vm::RUN_ORIGINAL; }
    const uint32_t list = rd(A + 0x268), n = rd(A + 0x26c);
    std::fprintf(stderr, "[fhl probe] frame %u act %08x type %u drivers %u tracking %08x\n", frame, A, rd(A + 0x238), n, rd(TRACKING));
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t d = rd(list + 4 * i), p = rd(d + 0x74);
        std::fprintf(stderr, "[fhl probe]  driver %u d %08x p %08x +4 %u +0x10 %u +0x1f0 %d d+0x70 %u d+0x78 %u veh %08x\n", i, d, p, rd(p + 4), rd(p + 0x10),
                     int(rd(p + 0x1f0)), rd(d + 0x70), rd(d + 0x78), rd(p + 0x1ac));
    }
    // the first driver is the subject: poked into a local human with a controller
    const uint32_t d = rd(list), p = rd(d + 0x74), veh = rd(p + 0x1ac);
    if (!d || !p || !veh) { std::fprintf(stderr, "[fhl probe] no driver / player / vehicle\n"); return reflex_vm::RUN_ORIGINAL; }
    { rx_regs in = regs(); call(0x0047bb40, in); }   // the damage manager singleton constructed
    const uint32_t beh = MGR + 0x10;
    const uint32_t aidiff0 = ui_get(K_AIDIFF);
    std::fprintf(stderr, "[fhl probe] AIDifficulty %u, tracking[1] %08x, mgr beh0 target %08x count %u\n", aidiff0, rd(rd(TRACKING) + 4), rd(beh + 0x3fc), rd(beh));

    int cases = 0;
    auto finish = [&](const char* name) {
        rx_regs in = regs(); const uint32_t a[1] = {A};
        std::fprintf(stderr, "[fhl probe] case %s\n", name);
        call(0x00569930, in, 1, a); cases++;
    };
    auto human = [&](Pokes& k, uint32_t place) {
        k.set(p + 4, 1); k.set(p + 0x10, 0); k.set(p + 0x1f0, 0); k.set(d + 0x70, 1); k.set(d + 0x78, place);
        for (uint32_t off = 0x1074; off <= 0x1084; off += 4) k.set(p + off, 0);
    };
    auto panels = [&](Pokes& k, uint32_t type, uint32_t count, uint32_t state, uint32_t node) {
        k.set(veh + 0x194, type);
        k.set(beh + 0x3fc, veh); k.set(beh, count);
        for (uint32_t i = 0; i < count && i < 0x20; i++) { k.set(beh + 0xc + 4 * i, state); k.set(beh + 0x370 + 4 * i, node); }
    };
    auto guard = [&](Pokes& k) { k.set(A + 0x238, 1); };   // the activity type: neither 7 nor 8

    for (int round = 0; round < 3; round++) {
        // round 0: the bike as it is; 1: AIDifficulty 0x78; 2: AIDifficulty 0x78 and tricks
        Pokes k; guard(k); human(k, 0);
        if (round >= 1) ui_set(K_AIDIFF, 0x78);
        if (round >= 2) { k.set(p + 0x1074, 1); k.set(p + 0x1078, 1); k.set(p + 0x1080, 1); }
        finish(round == 0 ? "win, AIDifficulty as is" : round == 1 ? "win, AIDifficulty 0x78" : "win, AIDifficulty 0x78, 3 tricks");
        // place 1: no win highlight
        k.set(d + 0x78, 1); finish("place 1, AIDifficulty 0x78, 3 tricks");
        k.set(d + 0x78, 0);
        // the activity types 8 (no 0x2c) and 7 (no 0x26)
        k.set(A + 0x238, 8); finish("activity type 8");
        k.set(A + 0x238, 7); finish("activity type 7");
        k.set(A + 0x238, 1);
        // the sum is exactly 2: no 0x26
        k.set(p + 0x1080, 0); finish("two tricks");
        // not a local human / no controller / not a racer
        k.set(p + 4, 0); finish("player +4 clear");
        k.set(p + 4, 1); k.set(p + 0x10, 1); finish("player +0x10 set");
        k.set(p + 0x10, 0); k.set(p + 0x1f0, 0xffffffffu); finish("controller -1");
        k.set(p + 0x1f0, 0); k.set(d + 0x70, 0); finish("driver +0x70 clear");
        k.set(d + 0x70, 1);
        k.undo();
        ui_set(K_AIDIFF, aidiff0);
    }

    // the four-wheeler highlights (vehicle types 3 / 5 / 7)
    for (uint32_t type : {3u, 5u, 7u, 1u}) {
        for (int mix = 0; mix < 5; mix++) {
            Pokes k; guard(k); human(k, 2);
            const uint32_t state = mix == 0 ? 3 : mix == 1 ? 0 : mix == 2 ? 2 : 1;
            panels(k, type, 4, state, mix == 4 ? 0 : 0x1234);
            if (mix == 3) { k.set(beh + 0xc + 8, 3); }            // one panel gone: neither highlight
            if (mix == 4) { k.set(beh + 0xc + 4, 3); }            // type 3 with an absent node, one panel intact elsewhere
            char name[64]; std::snprintf(name, sizeof name, "vehicle type %u mix %d", type, mix);
            finish(name);
            k.undo();
        }
    }
    { Pokes k; guard(k); human(k, 2); panels(k, 3, 0, 0, 0); finish("type 3 with no panels"); k.undo(); }
    { Pokes k; guard(k); human(k, 2); k.set(veh + 0x194, 3); k.set(beh + 0x3fc, 0); finish("type 3 with no behaviour of its own"); k.undo(); }

    // 006d8530 direct: stdcall (tracking, place), ESI = player, EDI = controller
    ui_set(K_AIDIFF, 0x78);
    for (uint32_t place : {0u, 1u}) {
        Pokes k; guard(k); human(k, place); k.set(p + 0x1074, 3);
        rx_regs in = regs(); in.r[6] = p; in.r[7] = 0; const uint32_t a[2] = {rd(TRACKING), place};
        std::fprintf(stderr, "[fhl probe] case 006d8530 place %u\n", place);
        call(0x006d8530, in, 2, a); cases++;
        k.set(p + 0x94, 0);   // no activity: the first test passes, the second is skipped
        std::fprintf(stderr, "[fhl probe] case 006d8530 place %u, no activity\n", place);
        call(0x006d8530, in, 2, a); cases++;
        k.undo();
    }
    ui_set(K_AIDIFF, aidiff0);

    // the panel helpers direct
    {
        Pokes k; panels(k, 3, 4, 3, 0x1234);
        for (uint32_t st : {0u, 2u, 3u}) {
            k.set(beh + 0xc + 8, st);
            { rx_regs in = regs(); in.r[1] = beh; call(0x0047baf0, in); cases++; }
            { rx_regs in = regs(); in.r[6] = MGR; in.r[7] = veh; call(0x0047bed0, in); cases++; }
            { rx_regs in = regs(); in.r[0] = MGR; in.r[7] = veh; call(0x0047bf00, in); cases++; }
        }
        k.set(beh + 0x370 + 4, 0);
        { rx_regs in = regs(); in.r[1] = beh; call(0x0047baf0, in); cases++; }
        k.set(veh + 0x194, 1);
        { rx_regs in = regs(); in.r[1] = beh; call(0x0047baf0, in); cases++; }
        { rx_regs in = regs(); in.r[6] = MGR; in.r[7] = 0x12345678; call(0x0047bed0, in); cases++; }   // a vehicle with no behaviour
        { rx_regs in = regs(); in.r[0] = MGR; in.r[7] = 0x12345678; call(0x0047bf00, in); cases++; }
        k.undo();
    }
    std::fprintf(stderr, "[fhl probe] done, %d calls\n", cases);
    return reflex_vm::RUN_ORIGINAL;
}
