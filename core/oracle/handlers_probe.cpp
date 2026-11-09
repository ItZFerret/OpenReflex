// handlers_probe.cpp -- the oracle's probe for the "handlers" ports (core/game/world_scene_handlers.c and the later handler files) that no reference world or boot
// reaches (development only; the same device as misc_probe.cpp): a frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) that, at a
// chosen frame of a Verify run, calls the original handler addresses with live game objects of the world (the scene's bales, the players and their vehicles); every call
// goes through the installed port, so Verify runs the core function and then the original on the same state and compares memory and the declared registers.
//   REFLEX_PORT=rx_world,rx_HandlersProbeHook REFLEX_PORT_VERIFY=1 RX_HANDLERS_PROBE=<groups> [RX_HANDLERS_PROBE_FRAME=<n>] [RX_HANDLERS_PROBE_SEED=<n>]
// groups: b bales (006c14a0 PopUpBale, 006c0b60 PopDownBale "VehicleExited"), c clip recorder stunt listeners (006ab290 / 380 / 410 / 4a0),
// u unloading + the listener-removal templates, e the profile statistics (007538c0 and its leaves; a FreestyleDriver fixture covers the freestyle
// branch), h a read-only histogram of the scene's entity classes (their vtables) — the census of which handler families an event world has
// No game logic here: only argument construction and state variation (fields the handlers read are set before a call, the originals are restored after).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
using rx_oracle::bits;

namespace {
uint32_t g_frame = 0, g_rng = 0x2545f491u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
struct Saved {
    uint32_t r[8]; reflex_vm::Xmm xmm[8]; uint32_t flags; size_t fpu;
    explicit Saved(reflex_vm::Machine& m) : flags(m.flags), fpu(m.fpu.size()) { std::memcpy(r, m.r, sizeof r); std::memcpy(xmm, m.xmm, sizeof xmm); }
    void restore(reflex_vm::Machine& m) const { std::memcpy(m.r, r, sizeof r); std::memcpy(m.xmm, xmm, sizeof xmm); m.flags = flags; m.fpu.resize(fpu); }
};
rx_regs regs() { rx_regs r; std::memset(&r, 0, sizeof r); return r; }
rx_regs call(uint32_t fn, const rx_regs& in, int n = 0, const uint32_t* args = nullptr) {
    rx_regs out = regs();
    rx_call_original_ex(fn, &in, &out, n, args);
    return out;
}
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
// a value the probe sets and restores
struct Poke {
    uint32_t a, old;
    Poke(uint32_t addr, uint32_t v) : a(addr), old(rd(addr)) { wr(addr, v); }
    ~Poke() { wr(a, old); }
};
std::vector<uint32_t> players() {
    std::vector<uint32_t> v;
    const uint32_t G = rd(0x00a946b8);
    for (uint32_t i = 0; i < 12; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 0x94) != 0) v.push_back(p); }
    return v;
}
std::vector<uint32_t> scene_entities(uint32_t scene, uint32_t vtable) {
    std::vector<uint32_t> v;
    for (uint32_t k = 0; k < 0x18; k++) {
        const uint32_t rec = scene + 0x38 + 0x110 * k;
        if (rd(rec + 0x10c) == 0) continue;
        const uint32_t arr = rd(rec + 0x104), n = rd(rec + 0x108);
        for (uint32_t i = 0; i < n; i++) { const uint32_t e = rd(arr + 4 * i); if (e && rd(e) == vtable) v.push_back(e); }
    }
    return v;
}
const uint32_t kNaN = 0x7fc00001u, kNegZero = 0x80000000u;
uint32_t pick_float() {
    switch (rnd() % 5) { case 0: return 0; case 1: return kNegZero; case 2: return kNaN; case 3: return bits(1.25f); default: return bits(float(rnd() % 4000) / 16.0f); }
}

// ---- b: the bales' VehicleExited ---------------------------------------------------------------------------------------------------------------
void probe_bales() {
    const std::vector<uint32_t> ps = players();
    uint32_t P = 0;
    for (uint32_t p : ps) if (rd(p + 0x1ac) && rd(rd(p + 0x1ac) + 0x190) && rd(rd(rd(p + 0x1ac) + 0x190) + 0x94)) { P = p; break; }
    if (!P) { std::fprintf(stderr, "[handlersprobe] no player with a vehicle and an activity\n"); return; }
    const uint32_t veh = rd(P + 0x1ac), checker = rd(veh + 0x190), act = rd(checker + 0x94);
    const uint32_t scene = rd(rd(rd(P + 0x94) + 0x234) + 0x20138);
    const std::vector<uint32_t> ups = scene_entities(scene, 0x0095ffcc), downs = scene_entities(scene, 0x0095ff04);
    std::fprintf(stderr, "[handlersprobe] player %08x vehicle %08x activity %08x: %zu PopUpBale, %zu PopDownBale\n", P, veh, act, ups.size(), downs.size());
    const uint32_t T_same = halloc(0x300), T_other = halloc(0x300);
    for (uint32_t i = 0; i < 8; i++) rx_wr8(T_other + 0xc0 + i, "zz_other"[i]);
    for (int t = 0; t < 400 && !ups.empty(); t++) {
        const uint32_t bale = ups[rnd() % ups.size()];
        // the trigger carries the bale's own name (+0x178) or another
        for (uint32_t i = 0; i < 0x40; i++) rx_wr8(T_same + 0xc0 + i, rx_rd8(bale + 0x178 + i));
        const uint32_t T = (rnd() % 4 == 0) ? T_other : T_same;
        Poke b170(bale + 0x170, (rnd() % 4 == 0) ? 1 : 0), b174(bale + 0x174, rnd() % 2);
        Poke mode(act + 0x238, rnd() % 10), kind(act + 0x23c, (rnd() % 4 == 0) ? 8 : rnd() % 8), c10(checker + 0x10, rnd() % 2);
        const bool live = rnd() % 4 != 0;
        Poke g4(0x00a942b4, live ? 1 : 0), g8(0x00a942b8, act), gd0(0x00a942d0, pick_float());
        Poke ra(act + 0x294, bits(float(rnd() % 600) / 8.0f));
        rx_regs in = regs(); in.r[1] = bale;
        const uint32_t a[2] = {veh, T};
        call(0x006c14a0, in, 2, a);
    }
    for (int t = 0; t < 400 && !downs.empty(); t++) {
        const uint32_t bale = downs[rnd() % downs.size()];
        for (uint32_t i = 0; i < 0x40; i++) rx_wr8(T_same + 0xc0 + i, rx_rd8(bale + 0x17c + i));
        const uint32_t T = (rnd() % 4 == 0) ? T_other : T_same;
        Poke b170(bale + 0x170, (rnd() % 5 == 0) ? 0 : act), b174(bale + 0x174, (rnd() % 3 == 0) ? 1 : 0);
        Poke m21c(act + 0x21c, (rnd() % 4 == 0) ? 3 : 2);
        const bool live = rnd() % 4 != 0;
        Poke g4(0x00a942b4, live ? 1 : 0), g8(0x00a942b8, act), gcc(0x00a942cc, pick_float());
        Poke ra(act + 0x294, bits(float(rnd() % 600) / 8.0f));
        rx_regs in = regs(); in.r[1] = bale;
        const uint32_t a[2] = {veh, T};
        call(0x006c0b60, in, 2, a);
    }
}

// ---- c: the clip recorder's stunt listeners ----------------------------------------------------------------------------------------------------
void probe_clip() {
    const std::vector<uint32_t> ps = players();
    if (ps.empty()) { std::fprintf(stderr, "[handlersprobe] no player with an activity\n"); return; }
    const uint32_t fn[4] = {0x006ab290, 0x006ab380, 0x006ab410, 0x006ab4a0};
    const uint32_t desc = halloc(0x100);
    for (int t = 0; t < 600; t++) {
        const uint32_t P = ps[rnd() % ps.size()], act = rd(P + 0x94);
        Poke p4(P + 4, rnd() % 4 ? rd(P + 4) : 0), p10(P + 0x10, rnd() % 3 ? 0 : 1);
        const bool live = rnd() % 5 != 0;
        Poke g4(0x00a942b4, live ? 1 : 0), g8(0x00a942b8, act), rec(0x00d5a9a4, rnd() % 3 != 0);
        Poke cur(0x00a942c8, (rnd() % 7 == 0) ? 99 : rnd() % 100);
        Poke ra(act + 0x294, bits(float(rnd() % 3000) / 8.0f));
        const uint32_t list = halloc(0x20);                          // a replay player: the waiting list +0x27c / +0x280 holds it
        wr(list, P); wr(list + 4, P);
        const bool replay = rnd() % 4 == 0;
        Poke r908(P + 0x908, replay ? 1 : rd(P + 0x908)), l27c(act + 0x27c, replay ? list : rd(act + 0x27c)), l280(act + 0x280, replay ? 2 : rd(act + 0x280));
        if (rnd() % 2) { Poke c0(0x00a959a8, 0); rx_regs in = regs(); in.r[1] = 0x00d32360; const uint32_t a[2] = {P, desc}; call(fn[t % 4], in, 2, a); }
        else { rx_regs in = regs(); in.r[1] = 0x00d32360; const uint32_t a[2] = {P, desc + 4 * (rnd() % 8)}; call(fn[t % 4], in, 2, a); }
    }
}

// ---- u: unloading (vf9) of the live bales / flag holders / board and the listener-removal templates with arbitrary keys (destructive: the entities are unloaded; run to a few frames after) ----
void probe_unload() {
    const std::vector<uint32_t> ps = players();
    if (ps.empty()) { std::fprintf(stderr, "[handlersprobe] no player with an activity\n"); return; }
    const uint32_t P = ps[0];
    const uint32_t scene = rd(rd(rd(P + 0x94) + 0x234) + 0x20138);
    struct { uint32_t vt, fn; const char* n; } kinds[] = {
        {0x0095ffcc, 0x006c0f50, "PopUpBale"}, {0x0095ff04, 0x006c0730, "PopDownBale"}, {0x00960094, 0x006c1830, "ScoreTower"},
        {0x00960214, 0x006c2ce0, "TrackSide"}, {0x0095f41c, 0x006b7b60, "ThirtySecondBoard"} };
    for (auto& k : kinds) {
        const std::vector<uint32_t> es = scene_entities(scene, k.vt);
        std::fprintf(stderr, "[handlersprobe] %zu %s\n", es.size(), k.n);
        for (uint32_t e : es) { rx_regs in = regs(); in.r[1] = e; call(k.fn, in); }
    }
    // the removal helpers with arbitrary keys on channels / events that exist: the bale's channel (+0x1e4) "VehicleExited", the global channel's "Activity Init" /
    // "Activity UnInit" / "First Human Joined Activity", the activity's "Countdown Step"
    const std::vector<uint32_t> ups = scene_entities(scene, 0x0095ffcc);
    const uint32_t act = rd(P + 0x94), chA = rd(act + 0x290), chG = rd(0x00a93f74), chB = ups.empty() ? chA : rd(ups[0] + 0x1e4);
    const uint32_t gnames[] = {0x00935ea4, 0x00941d90, 0x00941df4};
    for (int t = 0; t < 120; t++) {
        const uint32_t ent = 0x20000000u + 0x100 * (rnd() % 8), cb = 0x006c14a0 + 0x10 * (rnd() % 4), arg = rnd() % 3 ? 0 : 1;
        const uint32_t a[3] = {ent, cb, arg};
        rx_regs in = regs();
        switch (t % 5) {
        case 0: in.r[0] = chB; call(0x006c4450, in, 3, a); break;
        case 1: in.r[0] = chB; call(0x006c4330, in, 3, a); break;
        case 2: if (rnd() % 3) { in.r[1] = chG; in.r[0] = gnames[rnd() % 3]; } else { in.r[1] = chA; in.r[0] = 0x00941bdc; } call(0x006c3c70, in, 3, a); break;
        case 3: in.r[1] = gnames[rnd() % 3]; call(0x006c4a40, in, 3, a); break;
        default: call(0x006c3930, in, 3, a); break;
        }
    }
    for (int t = 0; t < 20; t++) {
        const uint32_t ent = 0x20000000u + 0x100 * (rnd() % 8);
        rx_regs in = regs(); in.r[6] = ent;
        call(t % 2 ? 0x006c1890 : 0x006c2d40, in);
    }
}

// ---- h: the event world's scene entity classes (their vtables) in the loaded scene, so the handlers an MX event subscribes can be enumerated
// from the level itself (development diagnostic; no call) -------------------------------------------------------------------------------------------
void probe_histogram() {
    const std::vector<uint32_t> ps = players();
    if (ps.empty()) { std::fprintf(stderr, "[handlersprobe] no player with an activity\n"); return; }
    const uint32_t P = ps[0];
    const uint32_t scene = rd(rd(rd(P + 0x94) + 0x234) + 0x20138);
    std::fprintf(stderr, "[handlersprobe] scene %08x\n", scene);
    std::vector<std::pair<uint32_t, uint32_t>> hist;   // vtable, count
    for (uint32_t k = 0; k < 0x18; k++) {
        const uint32_t rec = scene + 0x38 + 0x110 * k;
        if (rd(rec + 0x10c) == 0) continue;
        const uint32_t arr = rd(rec + 0x104), n = rd(rec + 0x108);
        for (uint32_t i = 0; i < n; i++) {
            const uint32_t e = rd(arr + 4 * i);
            if (!e) continue;
            const uint32_t vt = rd(e);
            bool found = false;
            for (auto& h : hist) if (h.first == vt) { h.second++; found = true; }
            if (!found) hist.push_back({vt, 1});
        }
    }
    std::fprintf(stderr, "[handlersprobe] %zu entity classes\n", hist.size());
    for (auto& h : hist) std::fprintf(stderr, "[handlersprobe] vtable %08x x%u\n", h.first, h.second);
}

// ---- e: the profile statistics (the records object [0xd92f10]'s "End" handler 007538c0 and its two leaves) --------------------------------------------------------
uint32_t mk_freestyle_driver();
uint32_t rfloat() {
    switch (rnd() % 6) { case 0: return 0; case 1: return kNaN; case 2: return bits(-3.5f); case 3: return bits(5280.0f * float(rnd() % 40)); case 4: return 0x7f800000u; default: return bits(float(rnd() % 100000) / 7.0f); }
}
void probe_stats() {
    const std::vector<uint32_t> ps = players();
    uint32_t P = 0;
    for (uint32_t p : ps) if (rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x1ac) != 0 && rd(p + 0x94) != 0) { P = p; break; }
    if (!P) { std::fprintf(stderr, "[handlersprobe] no human player\n"); return; }
    const uint32_t rec = rd(0x00d92f10), act = rd(P + 0x94), veh = rd(P + 0x1ac), G = rd(0x00a946b8);
    uint32_t drv = 0;
    for (uint32_t i = 0; i < rd(act + 0x26c); i++) { const uint32_t d = rd(rd(act + 0x268) + 4 * i); if (rd(d + 0x74) == P) drv = d; }
    const uint32_t ses = rd(rd(0x00da6ca0) ? 0x00da6ca0 + 0x10 : 0x00da6ca0 + 0x10);
    const uint32_t fd = mk_freestyle_driver();
    std::fprintf(stderr, "[handlersprobe] records %08x activity %08x player %08x driver %08x freestyle %08x session %08x\n", rec, act, P, drv, fd, ses);
    for (int t = 0; t < 400; t++) {
        std::vector<Poke*> pk;
        auto poke = [&](uint32_t a, uint32_t v) { pk.push_back(new Poke(a, v)); };
        if (fd && t % 3 == 0 && rd(act + 0x268) != 0 && rd(act + 0x26c) > 0) {
            /* the freestyle branch: a real FreestyleDriver in the human slot, its player and a "best trick" player (+0x10a0) */
            poke(rd(act + 0x268), fd);
            poke(fd + 0x74, P);
            poke(P + 0x10a0, P);
            for (uint32_t o = 0xc84; o <= 0xc94; o += 4) poke(P + o, rfloat());
        }
        poke(rec + 0x1c, rnd() % 9 ? act : 0);
        const uint32_t kinds[] = {0xffffffffu, 0, 3, 0x1e0, 0x1f3, 0x1f4, 0x300};
        poke(rec + 0x67a0, kinds[rnd() % 7]);
        for (uint32_t s = 0; s < 12; s++) poke(rec + 0x3c + 4 * s, rnd() % 5 != 0);
        poke(veh + 0x154, rfloat());
        poke(P + 0x11d0, (rnd() % 3 == 0) ? 0xfffffffeu : rnd() % 50); poke(P + 0x11d4, (rnd() % 3 == 0) ? 0 : rnd() % 50);
        poke(act + 0x2d0, rnd() % 2 ? drv : 0);
        poke(ses + 0x1c4c, rnd() % 2); poke(ses + 0x1b0, rnd() % 3); poke(ses + 0x1c48, rnd() % 2);
        poke(drv + 0x70, rnd() % 4 != 0);
        rx_regs in = regs(); in.r[1] = rec;
        const uint32_t a[1] = {act};
        call(0x007538c0, in, 1, a);
        for (Poke* q : pk) delete q;
    }
    // the best-score leaf with fake drivers / players
    for (int t = 0; t < 200; t++) {
        const uint32_t d = halloc(0x1000), p = halloc(0x1000);
        for (uint32_t o = 0xc84; o <= 0xc94; o += 4) wr(p + o, rfloat());
        for (uint32_t o = 0xf18; o <= 0xf28; o += 4) wr(d + o, rfloat());
        if (rnd() % 3 == 0) { const uint32_t v = rfloat(); for (uint32_t o = 0xc84; o <= 0xc94; o += 4) if (rnd() % 2) wr(p + o, v); }
        wr(d + 0x74, (t % 2) ? p : 0);
        rx_regs in = regs(); in.r[0] = d;
        call(0x0050de10, in);
    }
    // the table search: the live list and fake lists
    for (int t = 0; t < 100; t++) {
        const uint32_t tab = (t % 3 == 0) ? G + 0x154b0 : halloc(0x40), n = 1 + rnd() % 5;
        if (t % 3) { const uint32_t arr = halloc(0x40); wr(tab + 0x28, arr); wr(tab + 0x2c, (t % 7 == 0) ? 0xffffffffu : n);
            for (uint32_t i = 0; i < n; i++) { const uint32_t e = (rnd() % 5 == 0) ? 0 : halloc(0x20); if (e) { wr(e + 4, rnd() % 6); wr(e + 8, 0x1000 + i); } wr(arr + 4 * i, e); } }
        rx_regs in = regs(); in.r[1] = tab; in.r[7] = rnd() % 6;
        call(0x005411f0, in);
    }
}

// a FreestyleDriver fixture: a block whose vtable word is the original's FreestyleDriver vtable (0x0093fafc, classes.tsv) so the
// original's RTTI cast (009009a2, BaseDriver -> FreestyleDriver) accepts it. The freestyle branch of 007538c0 (the best-trick score)
// needs one; no MX world's race list holds one (its drivers are BaseDriver / FreeRideDriver objects)
uint32_t mk_freestyle_driver() {
    const uint32_t d = halloc(0x100);
    wr(d, 0x0093fafc);
    return d;
}

}  // namespace

RX_PORT(0x0084ed20, HandlersProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_HANDLERS_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_HANDLERS_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_HANDLERS_PROBE_FRAME"))) : 200;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_HANDLERS_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    if (std::strchr(groups, 'b')) probe_bales();
    if (std::strchr(groups, 'c')) probe_clip();
    if (std::strchr(groups, 'u')) probe_unload();
    if (std::strchr(groups, 'e')) probe_stats();
    if (std::strchr(groups, 'h')) probe_histogram();
    saved.restore(m);
    std::fprintf(stderr, "[handlersprobe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
