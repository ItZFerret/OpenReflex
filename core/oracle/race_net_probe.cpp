// race_net_probe.cpp -- the oracle's probe for the race ports that no reference world reaches (development only).
// A frame hook (group "thunk", Engine::Update 0084ed20, outside every verified call) that, at chosen frames of a Verify run, calls the original
// function addresses with live game arguments or objects it builds on the game heap; each call goes through the installed port, so Verify runs the
// core function and then the original on the same state and compares memory and the declared registers exactly as for a call from the game.
//   REFLEX_PORT=rx_race,rx_RaceProbeHook REFLEX_PORT_VERIFY=1 RX_RACE_PROBE=<groups> [RX_RACE_PROBE_FRAME=<n>] [RX_RACE_PROBE_SEED=<n>]
// groups (letters): s sound / channels / world name, f fade + track distance, c cannon (exit / rule update), a AI smoothing / lap time / overlay,
// n network (snapshot / state / net player), l late (destructive: run on the last frame requested RX_RACE_PROBE_LATE)
// No game logic here: only argument construction. The objects are built from what the game's own loaders / constructors leave in memory
// (positions, chains of nodes with the layout of core/notes/race.md); randomised with a fixed seed, edge values (zero, NaN, signalling NaN, huge) included.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
extern "C" {
#include "../game/race.h"
#include "../game/race_beh1.h"
#include "../game/race_misc.h"
}
using namespace reflex_port;
using rx_oracle::bits;
using rx_oracle::f32;

namespace {
uint32_t g_frame = 0, g_rng = 0x2545f491u;
uint32_t rnd() { g_rng = g_rng * 1664525u + 1013904223u; return g_rng >> 8; }
float rf(float lo, float hi) { return lo + (hi - lo) * (float(rnd() & 0xffffff) / 16777216.0f); }
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
void wf(uint32_t a, float v) { rx_wr32(a, bits(v)); }
float rdf(uint32_t a) { return f32(rx_rd32(a)); }
reflex_vm::Machine* g_m = nullptr;
uint32_t g_log = 0;
// The Quazal change marking (006dcc70, 00884c90 / 00884ba0 underneath) needs the session's statistics table, which the offline worlds do not have (the original
// itself faults in its critical section). For the probe both runs see the same stand-in: it logs its register and stack arguments into guest memory (compared by Verify).
void install_marker_stub(uint32_t addr, uint32_t pop) {
    g_m->natives[addr] = [addr, pop](reflex_vm::Machine& mm) -> uint32_t {
        const uint32_t n = rx_rd32(g_log);
        if (n < 64) { rx_wr32(g_log + 4 + 12 * n, addr); rx_wr32(g_log + 8 + 12 * n, mm.r[1]); rx_wr32(g_log + 12 + 12 * n, mm.arg(0)); }
        rx_wr32(g_log, n + 1);
        return mm.ret(pop);
    };
}
// The selection iterator over the other network players (Quazal ObjDup; empty offline): stand-ins for its five entry points over a list in guest memory
// (g_list: [0] count, [1] the index, [2 + i] the elements); iterator it: +0 nonzero, +4 the current element, +8 nonzero while valid.
uint32_t g_list = 0;
uint32_t halloc(uint32_t n);
uint8_t g_saved_flag = 0;
void install_iterator_stubs() {
    // 00508800 is a core port now (rx_net_selection_advance walks the holder's std::set<uint> in game memory): the stand-in iterator's holder (it + 0) is an
    // empty set and the stats lock is off (byte 0x9df580), so the advance assigns the null successor (0088bc40 stays the stand-in below)
    static uint32_t holder = 0;
    holder = halloc(0x60);
    { const uint32_t head = halloc(0x20), proxy = halloc(0x10);
      wr(holder + 4, proxy); wr(proxy, holder + 4); wr(holder + 4 + 0x18, head); rx_wr8(head + 0x15, 1); wr(head, head); wr(head + 4, head); wr(head + 8, head); }
    g_saved_flag = rx_rd8(0x009df580); rx_wr8(0x009df580, 1);
    g_m->natives[0x00703450] = [](reflex_vm::Machine& mm) -> uint32_t { const uint32_t it = mm.r[6]; rx_wr32(it, holder); rx_wr32(it + 4, 0); rx_wr32(it + 8, 0); return mm.ret(0); };
    g_m->natives[0x0088c190] = [](reflex_vm::Machine& mm) -> uint32_t { return mm.ret(0); };
    g_m->natives[0x0088bc40] = [](reflex_vm::Machine& mm) -> uint32_t { return mm.ret(4); };
    g_m->natives[0x0088c1e0] = [](reflex_vm::Machine& mm) -> uint32_t {
        const uint32_t it = mm.r[1], first = mm.arg(0) & 0xff;
        const uint32_t idx = first ? 0 : rx_rd32(g_list + 4) + 1;
        rx_wr32(g_list + 4, idx);
        if (idx < rx_rd32(g_list)) { rx_wr32(it + 4, rx_rd32(g_list + 8 + 4 * idx)); rx_wr32(it + 8, 1); }
        else { rx_wr32(it + 4, 0); rx_wr32(it + 8, 0); }
        return mm.ret(4);
    };
}
void remove_iterator_stubs() { for (uint32_t a : {0x00703450u, 0x0088c190u, 0x0088bc40u, 0x0088c1e0u}) g_m->natives.erase(a); rx_wr8(0x009df580, g_saved_flag); }
uint32_t halloc(uint32_t n) {
    const uint32_t a[1] = {n};
    const uint32_t p = rx_call_original(0x0078fbe0, RX_CDECL, 0, 1, a);
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
// objects of one trial come from a bump arena (reset between trials: the deterministic heap is small)
uint32_t g_arena = 0, g_pos = 0;
const uint32_t kArena = 0x100000;
void arena_reset() { g_pos = 0; }
uint32_t galloc(uint32_t n) {
    if (!g_arena) g_arena = halloc(kArena);
    n = (n + 15) & ~15u;
    if (g_pos + n > kArena) g_pos = 0;
    const uint32_t p = g_arena + g_pos; g_pos += n;
    for (uint32_t i = 0; i < n; i += 4) wr(p + i, 0);
    return p;
}
const uint32_t kSpecial[] = {0x00000000u, 0x80000000u, 0x3f800000u, 0xbf800000u, 0x3f000000u, 0x7fc00000u, 0xffc00000u, 0x7f800001u, 0x7f800000u,
                             0xff800000u, 0x7f7fffffu, 0x00000001u, 0x3727c5acu /* 1e-5 */, 0x38bcbe62u /* 9e-5 */, 0x40000000u, 0x42c80000u, 0xc2c80000u};
// the emulator's x87 (f80) does not keep NaN payloads (the hardware quiets a signalling NaN): x87 copy sources stay free of NaNs
uint32_t nonan(uint32_t b) { return (b & 0x7f800000u) == 0x7f800000u ? (b & ~0x7f800000u) | 0x3f000000u : b; }
float special() { return f32(kSpecial[rnd() % (sizeof kSpecial / sizeof kSpecial[0])]); }
float anyf() { return (rnd() % 6 == 0) ? special() : rf(-120.0f, 120.0f); }
float unitf() { return (rnd() % 8 == 0) ? special() : rf(-0.2f, 1.3f); }

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
void setx(rx_regs& r, int i, float v) { r.xmm[i].v[0] = v; }

// ---- sound / channel / world name ------------------------------------------------------------------------------------------
void probe_sound() {
    for (int t = 0; t < 400; t++) {                       // 00807130 / 00807170: music channel volume / fade
        const uint32_t ch = galloc(0x40);
        for (uint32_t o = 0x18; o <= 0x24; o += 4) wr(ch + o, nonan((rnd() & 3) == 0 ? kSpecial[rnd() % 17] : bits(unitf())));
        rx_regs in = regs();
        if (t & 1) { in.r[0] = ch; setx(in, 0, unitf()); call(0x00807130, in); }
        else { in.r[1] = ch; const uint32_t a[1] = {bits(anyf())}; call(0x00807170, in, 1, a); }
    }
    const uint32_t tid = rx_call_original(rd(0x0091607c), RX_STDCALL, 0, 0, nullptr);
    const uint32_t audio_saved = rd(0x00d67d1c);
    for (int t = 0; t < 120; t++) {                       // 0080b410 / 0080ade0 / 0080ae10: event volume, stop (queued, or on the audio thread)
        const uint32_t ev = galloc(0x1a4);
        wr(ev + 0x138, rnd() % 6); wr(ev + 0x13c, (rnd() & 1) ? 8 : 0);
        wr(ev + 0x14, rnd()); wr(ev + 0x118, (t % 3 == 2 && (t & 4)) ? 0x1000u : 0);
        wr(0x00d67d1c, (t & 2) ? tid : audio_saved);
        rx_regs in = regs();
        switch (t % 3) {
        case 0: in.r[6] = ev; setx(in, 0, unitf()); in.r[5] = rnd(); call(0x0080b410, in); break;
        case 1: in.r[0] = ev; in.r[5] = rnd(); call(0x0080ade0, in); break;
        default: in.r[6] = ev; call(0x0080ae10, in); break;
        }
    }
    wr(0x00d67d1c, audio_saved);
    // 00581350 / 00581490: the world name with its suffixes
    const uint32_t G = rd(0x00a946b8), name = G + 0x39c0;
    uint8_t keep[0x40];
    for (uint32_t i = 0; i < 0x40; i++) keep[i] = uint8_t(rx_rd8(name + i));
    static const char* names[] = {"NAT_Concord", "SX_Springfield_B", "TRX_Verde_C", "A_BW", "A_B_BW", "x_C_BW", "B", "BW", "", "C_BW", "Qq", "B_BW_B",
                                  "0123456789012345678901234567890123456789012345678901", "0123456789012345678901234567890123456789012345678B_BW",
                                  "0123456789012345678901234567890123456789012345678", "012345678901234567890123456789012345678901234567B_BW"};
    for (const char* nm : names) for (int rep = 0; rep < 2; rep++) {
        for (uint32_t i = 0; i < 0x40; i++) rx_wr8(name + i, i < std::strlen(nm) ? uint8_t(nm[i]) : 0);
        const uint32_t out = galloc(0x40);
        for (uint32_t i = 0; i < 0x40; i += 4) wr(out + i, 0xaaaaaaaau);
        const uint32_t a[1] = {out};
        call(rep ? 0x00581490 : 0x00581350, regs(), 1, a);
    }
    for (uint32_t i = 0; i < 0x40; i++) rx_wr8(name + i, keep[i]);
}

// ---- fade layer, track distance ------------------------------------------------------------------------------------------------
uint32_t make_node(float x, float y, float z, float dx, float dz, float len) {
    const uint32_t n = galloc(0x500);
    wf(n + 0, x); wf(n + 4, y); wf(n + 8, z);
    wf(n + 0x10, x - dz * 6.0f); wf(n + 0x14, y); wf(n + 0x18, z + dx * 6.0f);
    wf(n + 0x20, x + dz * 6.0f); wf(n + 0x24, y); wf(n + 0x28, z - dx * 6.0f);
    wf(n + 0x30, x - dz * 9.0f); wf(n + 0x34, y); wf(n + 0x38, z + dx * 9.0f);
    wf(n + 0x40, x + dz * 9.0f); wf(n + 0x44, y); wf(n + 0x48, z - dx * 9.0f);
    wf(n + 0x50, dx); wf(n + 0x54, 0.0f); wf(n + 0x58, dz);
    wf(n + 0x4f0, len);
    return n;
}
// a chain of nodes along a bending line; returns the first, fills `all`
uint32_t make_chain(int count, uint32_t* all, float x0, float z0, float heading) {
    uint32_t prev = 0, first = 0;
    float x = x0, z = z0, h = heading;
    for (int i = 0; i < count; i++) {
        const float len = rf(8.0f, 30.0f);
        const float dx = std::cos(h), dz = std::sin(h);
        const uint32_t n = make_node(x, rf(-1, 1), z, dx, dz, len);
        all[i] = n;
        if (prev) { wr(prev + 0x64, n); wr(n + 0x60, prev); } else first = n;
        prev = n;
        x += dx * len; z += dz * len; h += rf(-0.5f, 0.5f);
    }
    return first;
}
void probe_fade_distance() {
    for (int st = 0; st < 4; st++) {
        const uint32_t f = galloc(0x100);
        wr(f, st);
        rx_regs in = regs(); in.r[1] = f; call(0x00519ed0, in);
    }
    for (int t = 0; t < 200; t++) {                       // 0051d8c0: the distance along the section
        uint32_t all[10];
        const int count = 3 + rnd() % 7;
        const uint32_t first = make_chain(count, all, rf(-50, 50), rf(-50, 50), rf(-3, 3));
        const int k = rnd() % count;
        const uint32_t sec14 = galloc(0x10);       wr(sec14, first);
        const uint32_t d = galloc(0x1000), pl = galloc(0x400), veh = galloc(0x400), rule = galloc(0x40);
        wr(rule + 4, d); wr(d + 0x14, sec14); wr(d + 0x18, all[k]); wr(d + 0x74, pl); wr(pl + 0x1ac, veh);
        const uint32_t node = all[k];
        const float s = rf(-0.3f, 1.4f) * rdf(node + 0x4f0);
        wf(veh + 0xc0, rdf(node) + rdf(node + 0x50) * s + rf(-4, 4));
        wf(veh + 0xc4, rf(-3, 3));
        wf(veh + 0xc8, rdf(node + 8) + rdf(node + 0x58) * s + rf(-4, 4));
        if (rnd() % 10 == 0) wf(node + 0x4f0, special());
        rx_regs in = regs(); in.r[6] = rule; call(0x0051d8c0, in);
    }
}

// ---- the cannonball spline: 0051a110 exit, and the rule's update with a fake vehicle -----------------------------------------------
uint32_t g_T = 0;
uint32_t make_cannon_track(int sections, uint32_t* first_nodes, uint32_t* last_nodes) {
    if (!g_T) g_T = halloc(0x22300);
    arena_reset();
    const uint32_t T = g_T, trk = T + 0x21e10, arr = galloc(0x40);
    wr(trk + 0x130, arr); wr(trk + 0x1d8, sections);
    for (int s = 0; s < sections; s++) {
        uint32_t all[12];
        const int count = 2 + rnd() % 8;
        const uint32_t first = make_chain(count, all, rf(-30, 30), rf(-30, 30), rf(-3, 3));
        const uint32_t sec = galloc(0x200), p1 = galloc(0x10), p0 = galloc(0x10);
        wr(sec, first); wr(sec + 8, p0); wr(p0, p1); wr(p1, first); wr(p1 + 4, all[count - 1]);
        wr(sec + 0x118, (rnd() % 7 == 0) ? 2 : 0);
        wr(arr + 4 * s, sec);
        first_nodes[s] = first; last_nodes[s] = all[count - 1];
    }
    return T;
}
void probe_closest_point() {                              // 00407430
    for (int t = 0; t < 400; t++) {
        const uint32_t a = galloc(0x10), b = galloc(0x10), pt = galloc(0x10), out = galloc(0x10);
        for (int i = 0; i < 3; i++) { wf(a + 4 * i, rf(-50, 50)); wf(b + 4 * i, rf(-50, 50)); wf(pt + 4 * i, rf(-60, 60)); }
        switch (t % 8) {
        case 0: for (int i = 0; i < 3; i++) wr(b + 4 * i, rd(a + 4 * i)); break;                       // A = B
        case 1: for (int i = 0; i < 3; i++) wr(pt + 4 * i, rd(a + 4 * i)); break;                      // P = A
        case 2: for (int i = 0; i < 3; i++) wr(pt + 4 * i, rd(b + 4 * i)); break;                      // P = B
        case 3: for (int i = 0; i < 3; i++) wf(pt + 4 * i, 0.5f * (rdf(a + 4 * i) + rdf(b + 4 * i)) + rf(-1, 1)); break;   // near the middle
        case 4: wr(a + 4 * (rnd() % 3), kSpecial[rnd() % 17]); break;                                  // edge values
        case 5: wr(pt + 4 * (rnd() % 3), kSpecial[rnd() % 17]); break;
        case 6: wf(b + 4 * (rnd() % 3), special()); break;
        default: break;
        }
        for (uint32_t i = 0; i < 0x10; i += 4) wr(out + i, 0xaaaaaaaau);
        rx_regs in = regs(); in.r[1] = out; in.r[6] = pt; in.r[0] = a;
        const uint32_t args[1] = {b};
        call(0x00407430, in, 1, args);
    }
}
void probe_cannon_exit() {
    probe_closest_point();
    const uint32_t G = rd(0x00a946b8), ctx = rd(0x00a95844);
    const uint32_t v20 = rd(ctx + 0x20), v1c = rd(ctx + 0x1c);
    const uint32_t s20 = rd(v20 + 0x34), s1c = rd(v1c + 0x34), g5a = rd(G + 0x5a54), gc8 = rd(G + 0x20c8);
    for (int t = 0; t < 240; t++) {
        uint32_t fn[3], ln[3];
        const uint32_t T = make_cannon_track(1 + rnd() % 3, fn, ln);
        wr(G + 0x5a54, T); wr(G + 0x20c8, T);
        const int mode = t % 3;                            // 0: view set 0x20, 1: view set 0x1c, 2: none
        wr(v20 + 0x34, mode == 0 ? 1 : 0); wr(v1c + 0x34, mode == 1 ? 1 : 0);
        const uint32_t pos = galloc(0x10), out = galloc(0x20);
        const uint32_t base = fn[rnd() % 1];
        wf(pos, rdf(base) + rf(-60, 120)); wf(pos + 4, rf(-3, 3)); wf(pos + 8, rdf(base + 8) + rf(-60, 60));
        if (rnd() % 12 == 0) wf(pos, special());
        for (uint32_t i = 0; i < 0x20; i += 4) wr(out + i, 0xaaaaaaaau);
        rx_regs in = regs(); in.r[0] = pos; in.r[7] = (t % 4 < 2) ? out : 0;
        if (t & 1) { const uint32_t a[1] = {out}; call(0x0051a110, in, 1, a); }
        else { const uint32_t a[1] = {0}; call(0x0051a110, in, 1, a); }
        call(0x00519fa0, in);                              // the entry test (EDI = out point or 0)
    }
    wr(v20 + 0x34, s20); wr(v1c + 0x34, s1c); wr(G + 0x5a54, g5a); wr(G + 0x20c8, gc8);
}

// the rule: a driver whose player is the first local human of the game, a fake vehicle (the real vehicle's block with another position)
void probe_cannon_rule(bool launch_allowed) {
    const uint32_t G = rd(0x00a946b8), ctx = rd(0x00a95844);
    uint32_t player = 0;
    for (uint32_t i = 0; i < 12; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x1ac) != 0) { player = p; break; } }
    if (!player) { std::fprintf(stderr, "[probe] no local human player\n"); return; }
    const uint32_t realv = rd(player + 0x1ac);
    const uint32_t v20 = rd(ctx + 0x20), v1c = rd(ctx + 0x1c);
    const uint32_t s20 = rd(v20 + 0x34), s1c = rd(v1c + 0x34), g5a = rd(G + 0x5a54), gc8 = rd(G + 0x20c8);
    const uint32_t fake = halloc(0x1800);
    for (uint32_t i = 0; i < 0x1800; i += 4) wr(fake + i, rd(realv + i));
    int launches = 0;
    const uint32_t reset_timer = rd(player + 0x116c);
    wf(player + 0x116c, 1.0f);                             // a reset already runs: 00550920 returns at once (the fake vehicle cannot be reset)
    for (int t = 0; t < 1200; t++) {
        uint32_t fn[3], ln[3];
        const uint32_t T = make_cannon_track(1, fn, ln);
        wr(G + 0x5a54, T); wr(G + 0x20c8, T);
        wr(v20 + 0x34, 1);
        const uint32_t base = fn[0];
        // positions along the first segments: before, on, past, beside
        wf(fake + 0xc0, rdf(base) + rf(-10, 60)); wf(fake + 0xc4, rf(-2, 2)); wf(fake + 0xc8, rdf(base + 8) + rf(-60, 60));
        wr(player + 0x1ac, fake);
        wr(fake + 0x280, (rnd() % 30) == 0 ? 1 : 0);
        const uint32_t d = galloc(0x1000), rule = galloc(0x40), vt = galloc(0x100);
        for (uint32_t i = 0; i < 0x100; i += 4) wr(vt + i, 0x008d8dc0u);   // every driver virtual: "xor eax, eax; ret 4" (RaceDriver::vf9 needs a whole race)
        wr(d, vt);
        wr(d + 0x74, player);
        wr(rule + 4, d); wr(rule + 8, (rnd() % 20) ? 1 : 0);
        wr(rule + 0x10, (rnd() % 5 == 0) ? rnd() % 4 : 1);
        wr(rule + 0xc, (rnd() & 1) ? 0 : galloc(0x1a4));      // the warning sound (an event record with no FMOD instance)
        wf(rule + 0x14, rf(0, 1));
        rx_regs in = regs(); in.r[1] = rule;
        const uint32_t a[1] = {bits(0.0166f)};
        if (t % 4 == 0) {                                  // 0051a690 / 0054f150 on their own (inside 0051a300 they run as part of the native)
            const uint32_t r2 = galloc(0x40), d2 = galloc(0x1000);
            wr(d2, vt); wr(d2 + 0x74, player); wr(r2 + 4, d2); wr(r2 + 8, 1); wr(r2 + 0xc, (t & 4) ? 0 : 0x1000u);
            const int32_t li = int32_t(rd(player + 8));
            const uint32_t vp = li >= 0 ? rd(rd(ctx + 0x1c + 4 * rd(ctx + 4)) + 0x19b8 + 4 * uint32_t(li)) : 0;
            const uint32_t fstate = vp ? rd(rd(vp + 0x70)) : 0;
            if (vp) wr(rd(vp + 0x70), rnd() % 4);
            rx_regs e = regs(); e.r[7] = r2; e.r[5] = rnd(); call(0x0051a690, e);
            if (vp) wr(rd(vp + 0x70), fstate);
            const uint32_t pt = galloc(0x10);
            wf(pt, rdf(base) + rf(-80, 80)); wf(pt + 4, rf(-3, 3)); wf(pt + 8, rdf(base + 8) + rf(-80, 80));
            if (t % 16 == 0) wf(pt + 8, rdf(fake + 0xc8));       // zero-length heading
            rx_regs l = regs(); l.r[1] = player; l.r[7] = pt; call(0x0054f150, l);
        }
        call(0x0051a300, in, 1, a);
        if (rd(rule + 0x10) == 0 && rd(rule + 0x14) == bits(0.0f)) launches++;
    }
    wr(player + 0x1ac, realv);
    wr(player + 0x116c, reset_timer);
    wr(v20 + 0x34, s20); wr(v1c + 0x34, s1c); wr(G + 0x5a54, g5a); wr(G + 0x20c8, gc8);
    std::fprintf(stderr, "[probe] cannon rule: launches %d\n", launches);
}

// ---- AI smoothing / lap time text / overlay ----------------------------------------------------------------------------------
void probe_ai() {
    for (int t = 0; t < 300; t++) {                       // 0050b330
        const uint32_t d = galloc(0x1000), c = galloc(0x40);
        static const uint32_t tau[7] = {0x338, 0x34c, 0x360, 0x374, 0x388, 0x39c, 0x3b0};
        for (int i = 0; i < 7; i++) {
            wf(d + tau[i], (rnd() % 9) ? rf(0.02f, 0.6f) : special());
            wf(d + tau[i] - 4, (rnd() % 5) ? rf(-1, 1) : special());
        }
        for (int i = 0; i < 9; i++) wf(c + 4 * i, (rnd() % 6) ? rf(-1, 1) : special());
        rx_regs in = regs(); in.r[1] = d; in.r[2] = c; setx(in, 2, (rnd() % 8) ? rf(0.001f, 0.05f) : special());
        call(0x0050b330, in);
    }
    for (int t = 0; t < 600; t++) {                       // 00790100
        const uint32_t out = galloc(0x40);
        for (uint32_t i = 0; i < 0x40; i += 4) wr(out + i, 0x55555555u);
        float v;
        switch (rnd() % 7) {
        case 0: v = special(); break;
        case 1: v = rf(-7000.0f, 7000.0f); break;
        case 2: v = rf(-100.0f, 600.0f); break;
        case 3: v = float(rnd() % 600000) / 100.0f; break;
        case 4: v = -float(rnd() % 600000) / 100.0f; break;
        case 5: v = rf(0.0f, 5999.995f); break;
        default: v = rf(-0.5f, 0.5f); break;
        }
        rx_regs in = regs(); in.r[0] = out; in.r[7] = rnd() & 1; setx(in, 0, v);
        call(0x00790100, in);
    }
    const uint32_t flag = rd(0x00ae1b90);
    wr(0x00ae1b90, 1);
    for (int t = 0; t < 60; t++) {                        // 00509940: the AI debug overlay
        const uint32_t d = galloc(0x1000), pl = galloc(0x1300);
        wr(d + 0x74, pl); wr(pl + 0x1cc, (rnd() % 5) ? 1 : 0); wr(d + 0xee8, rnd() % 12);
        wr(pl + 0x8f4, rnd() % 4); wr(pl + 0x8f8, rnd() % 3); wr(pl + 0x8fc, (rnd() & 1) ? 0xffffffffu : rnd() % 12);
        for (uint32_t o = 0x150; o < 0x2d0; o += 4) wr(d + o, rnd());
        wr(d + 0x2e0, rnd() % 12); wr(d + 0x2e8, rnd() % 12); wr(d + 0x2dc, rnd() % 10); wr(d + 0x2e4, rnd() % 10);
        rx_regs in = regs(); in.r[6] = d; call(0x00509940, in);
    }
    wr(0x00ae1b90, flag);
    for (int t = 0; t < 300; t++) {                       // 00526c60 / 005352c0 / 00535380: BehaviorSuckRacing's modes and the run distance
        uint32_t all[10], all2[10];
        const int count = 2 + rnd() % 8;
        const uint32_t first = make_chain(count, all, rf(-50, 50), rf(-50, 50), rf(-3, 3));
        const uint32_t first2 = make_chain(2 + rnd() % 4, all2, rf(-50, 50), rf(-50, 50), rf(-3, 3));
        const uint32_t veh = galloc(0x400), pl = galloc(0x1000), act = galloc(0x400);
        wr(veh + 0x194, rnd() % 3); wr(veh + 0x198, rnd() % 4);
        const uint32_t off = 4 * (rd(veh + 0x198) + 4 * rd(veh + 0x194)) + 0x68;
        for (int i = 0; i < count; i++) wr(all[i] + off, (rnd() % 4) ? 0x200 : 0);
        for (int i = 0; i < 2; i++) wr(all2[i] + off, (rnd() % 4) ? 0x200 : 0);
        const uint32_t k = rnd() % (count - 1), n1 = all[k], n2 = rd(n1 + 0x64);   // (a missing next node faults in the original too)
        wr(pl + 0x1ac, veh); wr(pl + 0x94, act); wr(act + 0x23c, (rnd() & 1) ? 6 : (rnd() % 4));
        const float s = rf(-0.3f, 1.3f) * rdf(n1 + 0x4f0);
        wf(veh + 0xc0, rdf(n1) + rdf(n1 + 0x50) * s + rf(-3, 3)); wf(veh + 0xc8, rdf(n1 + 8) + rdf(n1 + 0x58) * s + rf(-3, 3));
        const uint32_t sec2 = galloc(0x40), p2 = galloc(0x10), sec = galloc(0x40);
        wr(sec2, first2); wr(sec2 + 0x10, 0); wr(p2, sec2); wr(sec + 8, p2); wr(sec + 0x10, rnd() & 3); wr(sec, first);
        const uint32_t D12 = galloc(0x1000);
        wr(D12 + 0x14, sec); wr(D12 + 0x18, n1); wr(D12 + 0x1c, n2); wr(D12 + 0x74, pl);
        const uint32_t vt12 = galloc(0x100), vt14 = galloc(0x100), vtb = galloc(0x100);
        for (uint32_t i = 0; i < 0x100; i += 4) { wr(vt12 + i, 0x007a0760u); wr(vt14 + i, 0x007a0760u); wr(vtb + i, 0x007a0760u); }
        wr(vt12 + 20, 0x007607d0u); wr(vt14 + 20, 0x008437f0u);
        const uint32_t b12 = galloc(0x80), b14 = galloc(0x100), b = galloc(0x80), d = galloc(0x1000);
        wr(b12, vt12); wr(b12 + 8, D12); wr(b12 + 0xc, (rnd() % 5) ? 2 : 1); wf(b12 + 0x20, rf(20, 400));
        wr(b14, vt14); wr(b14 + 0xa4, rnd() & 1);
        wr(d + 0x74, pl); wf(d + 0x130, rf(0, 20));
        wr(d + 0xe8, (rnd() & 1) ? b14 : b12); wr(d + 0xec, (rnd() & 1) ? b12 : b14); wr(d + 0xee0, rnd() % 3);
        if (rnd() & 1) { wr(d + 0xe8, b12); wr(d + 0xec, b14); wr(d + 0xee0, 2); }
        wr(b, vtb); wr(b + 8, d); wr(b + 0x10, rnd() % 4); wf(b + 0x14, rf(0, 1)); wf(b + 0x18, rf(0, 25)); wf(b + 0x1c, rf(0, 25)); wr(b + 0x20, 0);
        wr(b + 4, 1); wr(b + 0xc, rnd() % 4);
        rx_regs e = regs(); e.r[7] = b12; call(0x00526c60, e);
        rx_regs m1 = regs(); m1.r[6] = b; call(0x005352c0, m1);
        wr(b + 0x10, rnd() % 4);
        rx_regs m2 = regs(); m2.r[6] = b; call(0x00535380, m2);
    }
    for (int t = 0; t < 200; t++) {                       // 005256d0: the lap pacing gap
        const uint32_t b = galloc(0x80), d = galloc(0x1200), pl = galloc(0x1000), veh = galloc(0x400), act = galloc(0x1400);
        wr(b + 8, d); wr(d + 0x74, pl); wr(pl + 0x1ac, veh); wr(pl + 0x94, act); wr(pl + 0x8f4, rnd() % 5);
        wr(veh + 0x194, rnd() % 3); wr(veh + 0x198, rnd() % 4);
        wf(d + 0x10bc, (rnd() % 8) ? rf(-20, 20) : special());
        for (uint32_t o = 0xbc0; o < 0xd00; o += 4) wf(act + o, (rnd() % 12) ? rf(0, 90) : special());
        rx_regs in = regs(); in.r[0] = b; setx(in, 1, (rnd() % 5) ? rf(-12.0f, 12.0f) : special());
        call(0x005256d0, in);
    }
    {   // 0053aa40: the "Best Lap" event on the game's own event manager
        const uint32_t G = rd(0x00a946b8), act = rd(G + 0x7460 + 0x94);
        if (act) for (int t = 0; t < 4; t++) {
            rx_regs in = regs(); in.r[6] = rd(act + 0x290); setx(in, 0, rf(20.0f, 120.0f));
            const uint32_t a[1] = {G + 0x7460}; call(0x0053aa40, in, 1, a);
        }
    }
    // 00525d60: the lap pacing start fraction on the AI drivers of the game
    const uint32_t G = rd(0x00a946b8), act = rd(G + 0x7460 + 0x94);
    if (act) {
        const uint32_t drv = rd(act + 0x268), n = rd(act + 0x26c);
        for (uint32_t i = 0; i < n && i < 12; i++) {
            const uint32_t dd = rd(drv + 4 * i);
            if (!dd) continue;
            const uint32_t lp = rx_driver_lap_pacing_behavior(dd);
            if (!lp) continue;
            for (int k = 0; k < 8; k++) {
                const float fr = k == 0 ? 0.0f : k == 1 ? 1.0f : k == 2 ? 1.5f : k == 3 ? f32(0x7fc00000u) : k == 4 ? -0.5f : rf(0.0f, 1.0f);
                rx_regs in = regs(); setx(in, 0, fr);
                const uint32_t a[1] = {lp};
                call(0x00525d60, in, 1, a);
            }
        }
    }
}

// ---- network player ----------------------------------------------------------------------------------------------------------
void probe_net() {
    const uint32_t st = rd(0x009dfb98);
    for (int t = 0; t < 120; t++) {                       // 00884950 / 0089c8a0
        const uint32_t obj = galloc(0x40);
        const uint32_t station = (rnd() & 3) ? rnd() | 0x10000000u : 0;
        wr(0x009dfb98, station);
        wr(obj + 0x14, (rnd() % 3) == 0 ? 0 : ((rnd() & 1) ? station : rnd()));
        rx_regs in = regs(); in.r[1] = obj; call(0x00884950, in);
        const uint32_t slot = galloc(0x10);
        const uint32_t a[1] = {slot}; call(0x0089c8a0, regs(), 1, a);
    }
    wr(0x009dfb98, st);
    for (int t = 0; t < 200; t++) {                       // 006feb00 / 006fed10: the network player into a snapshot
        const uint32_t net = galloc(0x400), snap = galloc(0xc0);
        for (uint32_t o = 0x80; o < 0x120; o += 4) wr(net + o, nonan((rnd() % 6) == 0 ? kSpecial[rnd() % 17] : rnd()));
        for (uint32_t o = 0x80; o < 0x120; o += 4) if (rnd() & 1) wr(net + o, nonan(bits(anyf())));
        for (uint32_t i = 0; i < 0xc0; i += 4) wr(snap + i, 0xdeadbeefu);
        wr(0x00a95d50, (t & 1) ? rd(net + 0xd0) : rnd()); wr(0x00a95d54, rd(net + 0xd4)); wr(0x00a95d58, rd(net + 0xd8));
        rx_regs in = regs();
        if (t & 2) { in.r[0] = net; in.r[1] = snap; call(0x006feb00, in); }
        else { in.r[0] = snap; in.r[1] = net; call(0x006fed10, in); }
    }
    for (int t = 0; t < 3; t++) {                         // 0050bc70 -> 00508b90: a snapshot state bound to a player (the constructor / destructor run original)
        const uint32_t net = halloc(0x400), cell = halloc(0x10), p1 = halloc(0x40), p2 = halloc(0x40);
        wr(net + 0x1d4, cell);
        rx_regs a1 = regs(); a1.r[0] = net; const uint32_t x1[1] = {p1}; call(0x0050bc70, a1, 1, x1);       // new
        rx_regs a2 = regs(); a2.r[0] = net; call(0x0050bc70, a2, 1, x1);                                    // the same player: kept
        rx_regs a3 = regs(); a3.r[0] = net; const uint32_t x3[1] = {p2}; call(0x0050bc70, a3, 1, x3);       // another player: rebuilt
        rx_regs a4 = regs(); a4.r[0] = p1; const uint32_t x4[1] = {cell}; call(0x00508b90, a4, 1, x4);
    }
    for (int t = 0; t < 20; t++) {                        // 00431cf0
        const uint32_t src = galloc(0x80), dst = galloc(0x80);
        for (uint32_t i = 0; i < 0x80; i += 4) wr(src + i, rnd());
        rx_regs in = regs(); in.r[1] = src; const uint32_t a[1] = {dst}; call(0x00431cf0, in, 1, a);
    }
    for (int t = 0; t < 8; t++) {                         // 00700310: a neutral snapshot (the clock: 892170)
        const uint32_t snap = galloc(0xc0);
        for (uint32_t i = 0; i < 0xc0; i += 4) wr(snap + i, 0xdeadbeefu);
        rx_regs in = regs(); in.r[6] = snap; call(0x00700310, in);
    }
    // 0045d4d0 / 005084d0 / 0061c6b0 / 0054e110 / 00757f40: the reset helpers on the game's own objects
    const uint32_t G = rd(0x00a946b8);
    uint32_t player = 0;
    for (uint32_t i = 0; i < 12; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 0x1ac) != 0 && rd(p + 0x1b4) != 0) { player = p; break; } }
    if (player) {
        { rx_regs in = regs(); in.r[0] = rd(player + 0x1ac); call(0x0045d4d0, in); }
        { rx_regs in = regs(); in.r[0] = rd(player + 0x1b4); call(0x0061c6b0, in); }
        { const uint32_t keep = rd(player + 0x9ac); rx_regs in = regs(); in.r[6] = player; call(0x0054e110, in); wr(player + 0x9ac, keep); }
        const uint32_t ui = rd(0x00d92efc);
        const uint32_t k16c = rd(ui + 0x16c), k130 = rd(ui + 0x130), pad = rd(player + 0x1f0);
        const uint32_t cur = rx_call_original(0x0075ec80, RX_CDECL, 0, 0, nullptr);   // the game's player mode name: the table entry of the current mode
        const uint32_t table = 0x009dd584, saved[4] = {rd(table), rd(table + 4), rd(table + 8), rd(table + 12)};
        const uint32_t dummy = galloc(0x10); wr(dummy, 0x7a7a7a7au);
        for (uint32_t mode = 1; mode <= 3; mode++) {      // 1: the single player mode, 2 / 3: the network modes (the table made to name the current mode so)
            if (cur < 4 && mode != cur) { for (int i = 0; i < 4; i++) wr(table + 4 * i, dummy); wr(table + 4 * mode, saved[cur]); }
            for (int t = 0; t < 6; t++) {
                wr(ui + 0x16c, t & 1 ? pad : (t & 2 ? pad + 1 : 0xffffffffu)); wr(ui + 0x130, t & 2); wr(ui + 0x10 + 0x48 * ((t & 1) ? pad : 0), t & 4); wr(ui + 0x10 + 0x48 * (pad + 1), t & 1);
                rx_regs in = regs(); in.r[6] = player; call(0x0054e110, in);
            }
            for (int i = 0; i < 4; i++) wr(table + 4 * i, saved[i]);
        }
        wr(ui + 0x16c, k16c); wr(ui + 0x130, k130);
    }
    {   // 00757f40: the debug camera toggle (twice: on, off)
        const uint32_t pl = galloc(0x40);
        const char* nm = "Debug Camera Owner";
        for (uint32_t i = 0; i <= std::strlen(nm); i++) rx_wr8(pl + 0x14 + i, uint8_t(nm[i]));
        const uint32_t keep = rd(G + 0x15b64);
        for (int t = 0; t < 4; t++) { const uint32_t a[1] = {pl}; call(0x00757f40, regs(), 1, a); }
        wr(G + 0x15b64, keep);
    }
}
// the network player's reset / update on the game's own player and vehicle (late: the original leaves the vehicle in its network mode)
void probe_net_late() {
    const uint32_t G = rd(0x00a946b8);
    uint32_t player = 0;
    for (uint32_t i = 0; i < 12; i++) { const uint32_t p = G + 0x7460 + 0x12a0 * i; if (rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x1ac) != 0 && rd(p + 0x1b4) != 0) { player = p; break; } }
    if (!player) return;
    const uint32_t keep = rd(player + 0xb2c), view = rx_game_is_view_track(G);
    // 006f8860 (the session's mode, Quazal: faults offline): the stand-in returns a mode kept in guest memory
    if (!g_log) g_log = halloc(0x400);
    g_m->natives[0x006f8860] = [](reflex_vm::Machine& mm) -> uint32_t { mm.r[0] = rx_rd32(g_log + 0x300); return mm.ret(0); };
    const uint32_t act = rd(player + 0x94), lvl = rd(act + 0x234), lvl_keep = rd(lvl + 0x20018);
    wr(lvl + 0x20018, 1);
    const uint32_t sess = 0x00da6ca0, s8 = rd(sess + 8), blk8 = halloc(0x400);
    wf(blk8 + 0x294, 1.5f);
    wr(sess + 8, blk8);
    for (int t = 0; t < 40; t++) {
        wr(g_log + 0x300, t % 4 == 0 ? 5 : (t % 4 == 1 ? 1 : (t % 4 == 2 ? 8 : 0)));
        const uint32_t rec = halloc(0x200), d = halloc(0x1000), snap = halloc(0x100);
        const int f = t / 4;                               // t % 4 selects the session mode; f the record's flags and the driver's wait state
        rx_wr8(rec + 0x129, uint8_t((f == 1) ? (view ^ 1) : view)); rx_wr8(rec + 0xc8, (f == 2) ? 1 : 0);
        wr(d + 0x74, player); wr(d + 0xefc, (f == 3) ? 0 : 1); wr(d + 0xf00, (f == 4) ? 0 : 1);
        for (uint32_t o = 0x10; o < 0x80; o += 4) wr(snap + o, nonan(bits(rf(-50, 50))));
        wr(player + 0xb2c, rec);
        rx_regs in = regs(); in.r[7] = d; const uint32_t a[1] = {snap};
        call(0x0050e7e0, in, 1, a);
    }
    { rx_regs in = regs(); in.r[1] = rd(player + 0x1ac); call(0x005084d0, in); }   // 005084d0 on its own (it also runs inside 0050e7e0)
    g_m->natives.erase(0x006f8860);
    wr(sess + 8, s8);
    wr(lvl + 0x20018, lvl_keep);
    wr(player + 0xb2c, keep);
    if (std::getenv("RX_RACE_PROBE_UPDATE")) {            // 006fe210 / 005088c0 with the marker stand-ins
        g_log = halloc(0x400);
        install_marker_stub(0x006dcc70, 4);
        install_marker_stub(0x00884c90, 8);
        for (int t = 0; t < 4; t++) {
            const uint32_t N = halloc(0x400);
            wr(N, rnd()); wr(N + 4, rnd());
            wr(0x009df898, (t & 1) ? 0x0088bd90u : 0u);      // the clock hook: a guest function returning EDX:EAX (here the two words at ECX)
            rx_regs sr = regs(); sr.r[1] = N; call(0x005088c0, sr);
        }
        wr(0x009df898, 0);
        g_list = halloc(0x100);
        for (int t = 0; t < 60; t++) {
            if (t == 20) install_iterator_stubs();
            if (t == 20 || t == 40) {                      // a list of other players: positions, ids (one equals ours), flags
                const uint32_t n = t == 20 ? 3 : 5;
                wr(g_list, n); wr(g_list + 4, 0);
                for (uint32_t i = 0; i < n; i++) {
                    const uint32_t pl = halloc(0x200);
                    wr(pl + 0x50, (t == 40 && i == 2) ? 0x1234 : rnd() & 0x3fffff);
                    wr(pl + 0x168, (i == 3) ? 1 : 0);
                    wf(pl + 0xd0, rf(-30, 30) + (t == 40 ? 2000.0f : 0.0f)); wf(pl + 0xd4, rf(-3, 3)); wf(pl + 0xd8, rf(-30, 30));
                    wr(g_list + 8 + 4 * i, pl);
                }
            }
            const uint32_t N = halloc(0x400), cell = halloc(0x100);
            wr(N + 0x1d4, cell);
            if (t == 0) {                                  // the track record the update reads (+0x20018) through each view set
                const uint32_t ctx = rd(0x00a95844), v20 = rd(ctx + 0x20), v1c = rd(ctx + 0x1c);
                const uint32_t X = halloc(0x20100);
                wr(X + 0x20018, 1);
                const uint32_t s20 = rd(v20 + 0x34), s1c = rd(v1c + 0x34), g5a = rd(G + 0x5a54), gc8 = rd(G + 0x20c8);
                wr(G + 0x5a54, X); wr(G + 0x20c8, X);
                for (int v = 0; v < 3; v++) {
                    wr(v20 + 0x34, v == 0); wr(v1c + 0x34, v == 1);
                    const uint32_t N2 = halloc(0x400), c2 = halloc(0x100);
                    wr(N2 + 0x1d4, c2); wr(N2 + 0x1e4, 7);
                    rx_regs b2 = regs(); b2.r[0] = N2; const uint32_t x2[1] = {player}; call(0x0050bc70, b2, 1, x2);
                    wf(N2 + 0x1d8, 0.4f); wf(N2 + 0x1dc, 0.1f); wf(N2 + 0x1d0, 0.0f);
                    for (uint32_t o = 0x10; o < 0x78; o += 4) wf(c2 + o, rf(0.0f, 3.0f));
                    const uint32_t a2[2] = {N2, bits(0.0166f)}; call(0x006fe210, regs(), 2, a2);
                }
                wr(v20 + 0x34, s20); wr(v1c + 0x34, s1c); wr(G + 0x5a54, g5a); wr(G + 0x20c8, gc8);
            }
            rx_regs b = regs(); b.r[0] = N; const uint32_t x[1] = {player}; call(0x0050bc70, b, 1, x);
            wf(N + 0x1d8, rf(0.0f, 0.3f)); wf(N + 0x1dc, rf(-0.2f, 0.3f)); wf(N + 0x1d0, rf(0.0f, 0.2f)); wr(N + 0x1e0, t & 1); wr(N + 0x50, t >= 40 ? 0x1234 : rnd());
            wr(N + 0x240, (t & 2) ? rnd() : 0);
            for (uint32_t o = 0x80; o < 0xc0; o += 4) wr(N + o, 0);
            const uint32_t holder = rd(N + 0x1d4);
            for (uint32_t o = 0x10; o < 0x78; o += 4) wf(holder + o, rf(0.0f, 3.0f));
            const uint32_t a[2] = {N, bits(0.0166f)};
            call(0x006fe210, regs(), 2, a);
        }
        {   // the edges: no state (returns 0); thresholds above every mean (the default interval); the engine's / the session's flags
            const uint32_t N0 = halloc(0x400); wr(N0 + 0x1d4, halloc(0x100));
            const uint32_t a0[2] = {N0, bits(0.0166f)}; call(0x006fe210, regs(), 2, a0);
            const uint32_t ctx = rd(0x00a95844), e10 = rd(ctx + 0x10), sess = 0x00da6ca0;
            const uint32_t e_keep = rd(e10 + 0xc), s10 = rd(sess + 0x10);
            const uint32_t S10 = halloc(0x2000);
            for (int v = 0; v < 6; v++) {
                const uint32_t N = halloc(0x400), cell = halloc(0x100);
                wr(N + 0x1d4, cell);
                rx_regs b = regs(); b.r[0] = N; const uint32_t x[1] = {player}; call(0x0050bc70, b, 1, x);
                wf(N + 0x1d8, 0.4f); wf(N + 0x1dc, 0.1f); wf(N + 0x1d0, 0.0f); wr(N + 0x1e0, v == 3 || v == 4);
                for (uint32_t o = 0x10; o < 0x78; o += 4) wf(cell + o, rf(0.0f, 3.0f));
                if (v < 3) for (uint32_t o = 0x14; o < 0x20; o += 4) wf(cell + o, 1e9f);
                wr(e10 + 0xc, v == 2);
                wr(sess + 0x10, S10); wr(S10 + 0x1c48, v == 1);
                const uint32_t a[2] = {N, bits(0.0166f)}; call(0x006fe210, regs(), 2, a);
            }
            wr(e10 + 0xc, e_keep); wr(sess + 0x10, s10);
        }
        remove_iterator_stubs();
        g_m->natives.erase(0x006dcc70); g_m->natives.erase(0x00884c90);
    }
}
}  // namespace

RX_PORT(0x0084ed20, RaceProbeHook, "thunk", 0) {
    static const char* groups = std::getenv("RX_RACE_PROBE");
    if (!groups || !*groups) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RACE_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RACE_PROBE_FRAME"))) : 200;
    static const uint32_t late = std::getenv("RX_RACE_PROBE_LATE") ? uint32_t(std::atoi(std::getenv("RX_RACE_PROBE_LATE"))) : 0;
    static bool seeded = false;
    if (!seeded) { seeded = true; if (const char* s = std::getenv("RX_RACE_PROBE_SEED")) g_rng ^= uint32_t(std::atoi(s)) * 2654435761u; }
    ++g_frame;
    const bool early = g_frame == at, last = late && g_frame == late;
    if (!early && !last) return reflex_vm::RUN_ORIGINAL;
    const Saved saved(m);
    g_m = &m;
    if (early) {
        if (std::strchr(groups, 's')) probe_sound();
        if (std::strchr(groups, 'f')) probe_fade_distance();
        if (std::strchr(groups, 'c')) probe_cannon_exit();
        if (std::strchr(groups, 'a')) probe_ai();
        if (std::strchr(groups, 'n')) probe_net();
    }
    if (last && std::strchr(groups, 'n')) probe_net_late();
    if (last && std::strchr(groups, 'l')) {
        probe_cannon_rule(true);
        const uint32_t G = rd(0x00a946b8);                 // 00550920 on the game's own player and vehicle (the reset it starts is the original's)
        for (uint32_t i = 0; i < 12; i++) {
            const uint32_t p = G + 0x7460 + 0x12a0 * i;
            if (rd(p + 4) != 0 && rd(p + 0x10) == 0 && rd(p + 0x1ac) != 0) {
                rx_regs in = regs(); in.r[6] = p; call(0x00550920, in);
                break;
            }
        }
    }
    saved.restore(m);
    std::fprintf(stderr, "[probe] frame %u done\n", g_frame);
    return reflex_vm::RUN_ORIGINAL;
}
