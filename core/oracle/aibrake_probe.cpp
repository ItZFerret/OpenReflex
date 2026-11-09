// aibrake_probe.cpp -- per-call Verify of BehaviorHandleBraking::vf4 00524900 (the mode dispatch that selects the
// edge cast 005249a0, the centre-line cast 00524a40 and the past-the-side cast 00524af0) on the live AI driver objects
// of a reference world. The reference worlds never update the AI drivers (ridepath's note: no original instructions of
// 0x0050xxxx..0x0054xxxx execute in Concord_ai11), so no world calls the behaviour's frame; this probe is the
// verification of the dispatch switch itself (the three cast bodies are verified by race_ridepath_probe.cpp group b).
// A frame hook (thunk, Engine::Update 0084ed20, outside every verified call) at one frame calls 00524900 on a live
// BehaviorHandleBraking with the state each branch reads, and restores it afterwards. No game logic: fields the
// original reads (enabled +4, the speed +0x150, the pause/mode/edge objects +0x48 / +0x4c / +0x50, the cast count and
// records) are set and put back; the driver's throttle scale +0x64 the casts write is put back too.
//   REFLEX_PORT=rx_race,rx_AIBrakeProbeHook REFLEX_PORT_VERIFY=1 RX_AIBRAKE_PROBE=1 [RX_AIBRAKE_PROBE_FRAME=<n>]
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

namespace {
uint32_t g_frame = 0;
reflex_vm::Machine* g_m = nullptr;
bool mapped(uint32_t a, uint32_t n = 4) { return g_m && g_m->mem.mapped(a) && g_m->mem.mapped(a + n - 1); }
uint32_t rd(uint32_t a) { return mapped(a) ? rx_rd32(a) : 0; }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
static uint32_t fbits(float f) { uint32_t v; std::memcpy(&v, &f, 4); return v; }
// 00524900 (ECX = behaviour, stack dt; RET 4) -> EAX; a registered port, so rx_call_original_ex verifies it per call.
uint32_t call_update(uint32_t b, float dt) {
    const uint32_t a[1] = {fbits(dt)};
    rx_regs in = regs(); in.r[1] = b;   // ECX
    rx_regs out = regs();
    rx_call_original_ex(0x00524900u, &in, &out, 1, a);
    return out.r[0];
}
// a range of live game data a case sets and puts back at the end of its scope
struct Poke {
    uint32_t a, old;
    Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); }
    ~Poke() { wr(a, old); }
};
struct PokeF {
    uint32_t a, old;
    PokeF(uint32_t addr, float v) : a(addr) { uint32_t b; std::memcpy(&b, &v, 4); old = rd(addr); wr(addr, b); }
    ~PokeF() { wr(a, old); }
};
}  // namespace

RX_PORT(0x0084ed20, AIBrakeProbeHook, "thunk", 0) {
    static const char* on = std::getenv("RX_AIBRAKE_PROBE");
    if (!on || !*on) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_AIBRAKE_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_AIBRAKE_PROBE_FRAME"))) : 650;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    // a live BehaviorHandleBraking (vtable 0x009401a8) from the activity's AI drivers
    const uint32_t g = rd(0x00a946b8);
    uint32_t b = 0, drv = 0;
    const uint32_t nslots = g && mapped(g, 0x15420) ? uint8_t(rx_rd8(g + 0x1541c)) : 0;
    for (uint32_t s = 0; s < nslots && s < 16 && !b; s++) {
        const uint32_t p = g + 0x7460 + 0x12a0 * s;
        if (!mapped(p, 0x12a0)) continue;
        const uint32_t act = rd(p + 0x94);
        if (!act || !mapped(act, 0x2c0)) continue;
        const int32_t n = int32_t(rd(act + 0x26c));
        for (int32_t i = 0; i < n && i < 32 && !b; i++) {
            const uint32_t d = rd(rd(act + 0x268) + 4 * uint32_t(i));
            if (!d) continue;
            for (uint32_t k = 0; k < rd(d + 0xee0) && k < 40; k++) {
                const uint32_t o = rd(d + 0xe8 + 4 * k);
                if (o && rd(o) == 0x009401a8u) { b = o; drv = d; break; }
            }
        }
    }
    if (!b) { std::fprintf(stderr, "[aibrake] no braking behaviour\n"); return reflex_vm::RUN_ORIGINAL; }
    const uint32_t veh = rd(rd(drv + 0x74) + 0x1ac), mo = rd(b + 0x4c), ed = rd(b + 0x50), pa = rd(b + 0x48);
    if (!veh || !mo || !ed || !pa) {
        std::fprintf(stderr, "[aibrake] braking %08x drive %08x veh %08x mode %08x edge %08x pause %08x (incomplete)\n", b, drv, veh, mo, ed, pa);
        return reflex_vm::RUN_ORIGINAL;
    }
    std::fprintf(stderr, "[aibrake] behaviour %08x driver %08x veh %08x speed %08x mode +0xc %d +0x1c %d edge +0xc %d pause +4 %d count %u\n",
                 b, drv, veh, rd(veh + 0x150), int(rd(mo + 0xc)), int(rd(mo + 0x1c)), int(rd(ed + 0xc)), int(rd(pa + 4)), rd(b + 0x10));
    int calls = 0;
    // the state the frame reads, set for one case and put back when the block ends: enabled +4, the cast count / records /
    // braking edge, the driver's throttle scale, the pause flag, the mode and edge sides, the vehicle speed
    struct CaseState {
        Poke en, count, f0, s0, f1, s1, meat, side, thr, mode, mside, eside, pause;
        PokeF speed;
        CaseState(uint32_t b, uint32_t drv, uint32_t veh, uint32_t mo, uint32_t ed, uint32_t pa, uint32_t on, uint32_t md,
                  uint32_t sd, float sp, uint32_t paused)
            : en(b + 4, on), count(b + 0x10, 0), f0(b + 0x14, 0), s0(b + 0x18, 0), f1(b + 0x1c, 0), s1(b + 0x20, 0),
              meat(b + 0x40, 0x3f8147aeu), side(b + 0x44, 0), thr(drv + 0x64, 0x3f800000u), mode(mo + 0xc, md),
              mside(mo + 0x1c, sd), eside(ed + 0xc, sd), pause(pa + 4, paused), speed(veh + 0x150, sp) {}
    };
    {   // [b+4] == 0: disabled, the first return
        CaseState s(b, drv, veh, mo, ed, pa, 0, 0, 1, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] disabled -> %u count %u\n", r, rd(b + 0x10));
    }
    {   // mode 0: the edge casts (005249a0)
        CaseState s(b, drv, veh, mo, ed, pa, 1, 0, 1, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] mode 0 -> %u count %u side %08x meat %08x thr %08x\n", r, rd(b + 0x10), rd(b + 0x18), rd(b + 0x40), rd(drv + 0x64));
    }
    {   // mode 1, the mode object's side different from the edge side: 00524a40's edge branch
        CaseState s(b, drv, veh, mo, ed, pa, 1, 1, 0, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] mode 1 edge -> %u count %u side %08x meat %08x thr %08x\n", r, rd(b + 0x10), rd(b + 0x18), rd(b + 0x40), rd(drv + 0x64));
    }
    {   // mode 1, the same side: 00524a40's centre cast (recorded side 0, the tail applies without a common edge)
        CaseState s(b, drv, veh, mo, ed, pa, 1, 1, 1, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] mode 1 centre -> %u count %u side %08x meat %08x thr %08x\n", r, rd(b + 0x10), rd(b + 0x18), rd(b + 0x40), rd(drv + 0x64));
    }
    {   // mode 2, side 0: 00526dd0 never says past -> 00524af0's edge branch
        CaseState s(b, drv, veh, mo, ed, pa, 1, 2, 0, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] mode 2 edge -> %u count %u side %08x meat %08x thr %08x\n", r, rd(b + 0x10), rd(b + 0x18), rd(b + 0x40), rd(drv + 0x64));
    }
    {   // mode 2, the side the node geometry is past: 00524af0's centre branch
        const uint32_t a1[1] = {1};
        rx_regs in = regs(); in.r[0] = mo;
        rx_regs o = regs();
        rx_call_original_ex(0x00526dd0u, &in, &o, 1, a1);   // the same test the cast makes: side +1 past?
        const uint32_t sd = o.r[0] ? 1u : 0xffffffffu;
        CaseState s(b, drv, veh, mo, ed, pa, 1, 2, sd, 40.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] mode 2 past side %d -> %u count %u side %08x meat %08x thr %08x\n", int(int32_t(sd)), r,
                     rd(b + 0x10), rd(b + 0x18), rd(b + 0x40), rd(drv + 0x64));
    }
    {   // below 29.3332: the reset tail (vf6 005248d0)
        CaseState s(b, drv, veh, mo, ed, pa, 1, 0, 1, 10.0f, 0);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] slow -> %u state %d meat %08x side %d count %u\n", r, int(rd(b + 0xc)), rd(b + 0x40), int(rd(b + 0x44)), rd(b + 0x10));
    }
    {   // the pause object's +4: the same reset tail
        CaseState s(b, drv, veh, mo, ed, pa, 1, 0, 1, 40.0f, 1);
        const uint32_t r = call_update(b, 1.0f / 60.0f); ++calls;
        std::fprintf(stderr, "[aibrake] paused -> %u state %d\n", r, int(rd(b + 0xc)));
    }
    std::fprintf(stderr, "[aibrake] probe: %d calls\n", calls);
    saved.restore(m);
    return reflex_vm::RUN_ORIGINAL;
}
