// rng_probe.cpp -- the ORIGINAL's random number streams at every frame end, and (optionally) every draw with its caller chain
// (agent w-rng; development only). The product's counterpart is core/tools/rng_frames.c; core/tools/rng_cmp.py compares the two.
//
//   REFLEX_PORT=rx_RngProbeFrame RX_RNG_PROBE=<out file> [RX_RNG_LOG=1] [RX_RNG_SEED_PRE=<hex>] [RX_RNG_RESEED=1] [RX_RNG_FUNCS=a,b,...]
//
// A frame hook at Engine::Update 0084ed20 (the main thread, outside every verified call; a world test runs one per frame, a baton boot
// one per Engine::Frame) closes the frame that ended there: one line
//   F <frame> st <raw activity state A+0x21c of the local player's activity> k <frames since the countdown entry (the first frame ending in state 1 after one ending in 0), -1 before>
//     rs <the main thread's _holdrand (ptd + 0x14)> gs <g_GameRandSeed 0x9cb860> gc <g_GameRandCount 0x9cb864> ps <the activity's Post sub-state, -1 another class> [n <CRT draws> m <LCG draws>]
// and starts the next one as rng_frames.c does: RX_RNG_SEED_PRE sets the main _holdrand at the start of every frame whose state is 0
// (the world test's REFLEX_SEED), RX_RNG_RESEED sets both generators from the countdown on to the frame's k (CRT k * 0x9e3779b9 + 1,
// LCG k * 0x85ebca6b + 7): n / m are then the draws of that frame alone.
// (group "thunk": never verified itself, select it by its name.)
// RX_RNG_LOG=1 adds, per draw, "R <frame> <main|ptd> <its _holdrand before> <rets>" at msvcr90 rand 78594bed (the thread by its _tiddata) and
// "L <frame> <entry> <count> <rets>" at the entry of every LCG-drawing function (RX_RNG_FUNCS, default: the list below = every function
// core/tools/rng_audit.py finds an `imul 0x41c64e6d` in; recovered entries included). <rets>: the first 6 return addresses on the stack
// (RX_RNG_RETS=n: n).
// RX_RNG_CUE_STIM=<frame> (a stimulus for Verify): at the start of that frame the race audio logic's cue handlers that no shipped
// level's activity audio map subscribes (EndOfRace 00582da0, TwoLapsLeft 00582dc0, OneLapLeft 00582de0, Best Lap 00582e00,
// VehicleGettingLapped 00582e20, Player Wins 00583fb0) and the subscribed ones (Holeshot 00582e40, Player Eliminated 00583a70,
// TimedSectionEntered 00582e60 with n 2) are called on the local player's activity's race audio logic (vtable 0x00942cfc, +0x50 = the activity) with its first driver / vehicle,
// as their event delegates call them; with REFLEX_PORT=rx_sfx,... REFLEX_PORT_VERIFY=1 each call is a verified call of its port.
// RX_RNG_FINISH_STIM=<k> (w-resultsrng): the local human finishes at the start of countdown frame k (see below), so an event boot reaches
// the race end and the results page (a Waypoint race: its laps done +0xf10, w-wpfill).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
using namespace reflex_port;

extern const uint32_t rx_rng_lcg_drawers[] = {   // rng_audit.py (lcg); also gridpair_probe.cpp RX_PAIR_LCGLOG
    0x004240e0, 0x004625d0, 0x0050a360, 0x0050b590, 0x0050c7f0, 0x0050d1c0, 0x0050fb00, 0x0050fd30, 0x0050fef0, 0x00510010, 0x00511760,
    0x005118c0, 0x00517800, 0x005192f0, 0x005223e0, 0x00527360, 0x0052ab80, 0x0052b0a0, 0x0052b6e0, 0x0052bcf0, 0x0052d464, 0x0052dc50,
    0x0052ed50, 0x00535090, 0x005352c0, 0x00535380, 0x00535710, 0x00535b20, 0x0054a7b0, 0x0054cec0, 0x00564c90, 0x00571ca0, 0x00581570,
    0x005816e0, 0x00581760, 0x00581830, 0x005818c0, 0x00581950, 0x005819f0, 0x00582520, 0x0059ebc0, 0x0059f3e0, 0x005a00b0, 0x005a9a30,
    0x00613880, 0x00619490, 0x00623f30, 0x006258a0, 0x0062aa30, 0x006373e0, 0x0066cd30, 0x00674540, 0x00677220, 0x00677750, 0x00677840,
    0x0067f6d0, 0x00686870, 0x00686e20, 0x006944c0, 0x00694ba0, 0x00695780, 0x0069a170, 0x0069ad60, 0x006aadc0, 0x006b6a30, 0x006c6650,
    0x006ccc70, 0x006ccd10, 0x006d6e70, 0x006d7700, 0x007510c0, 0x0075f890, 0x0080bbd0, 0x00853220,
};
extern const size_t rx_rng_lcg_drawers_n = sizeof rx_rng_lcg_drawers / sizeof *rx_rng_lcg_drawers;

namespace {
FILE* g_out = nullptr;
uint32_t g_frame = 0, g_main_ptd = 0, g_r0 = 0, g_g0 = 0;
int g_k = -1, g_prev = -1;
bool g_setup = false;

uint32_t rd(reflex_vm::Machine& m, uint32_t a) { return m.mem.mapped(a) ? uint32_t(m.mem.read(a, 4)) : 0u; }
uint32_t ptd_now(reflex_vm::Machine& m) { return uint32_t(m.call_function(0x7854383f, nullptr, 0)); }   // _getptd_noexit
int steps(uint32_t from, uint32_t to, uint32_t a, uint32_t c) {
    for (int n = 0; n < 4000000; n++) { if (from == to) return n; from = from * a + c; }
    return -1;
}
int raw_state(reflex_vm::Machine& m) {   // the local player's activity state, as rx_session_race reads it
    const uint32_t G = rd(m, 0xa946b8);
    if (!G) return -1;
    const uint32_t cnt = m.mem.mapped(G + 0x1541c) ? uint32_t(m.mem.read(G + 0x1541c, 1)) : 0u;
    for (uint32_t k = 0; k < cnt && k < 16; k++) {
        const uint32_t sl = rd(m, G + 0x153ec + 4 * k);
        if (sl >= 16) continue;
        const uint32_t P = G + 0x7460 + sl * 0x12a0;
        if (rd(m, P + 0x10) != 0) continue;
        const uint32_t A = rd(m, P + 0x94);
        return A ? int(rd(m, A + 0x21c)) : -1;
    }
    return -1;
}
int post_state(reflex_vm::Machine& m) {   // the local player's activity's Post sub-state (2 = the results; MXActivityRace / SectionRace +0x13e640,
    const uint32_t G = rd(m, 0xa946b8);  // WaypointRace +0x13e618, Freestyle +0x13e5f8), -1 for another class
    const uint32_t A = G ? rd(m, G + 0x7460 + 0x94) : 0;
    switch (A ? rd(m, A) : 0) {
    case 0x00943554: case 0x00944a74: return int(rd(m, A + 0x13e640));
    case 0x00943c14: return int(rd(m, A + 0x13e618));
    case 0x009442cc: return int(rd(m, A + 0x13e5f8));
    default: return -1;
    }
}
std::string rets(reflex_vm::Machine& m) {
    std::string s; char b[16]; int n = 0;
    static const int nmax = std::getenv("RX_RNG_RETS") ? std::atoi(std::getenv("RX_RNG_RETS")) : 6;   // RX_RNG_RETS=n: n return addresses
    for (uint32_t a = m.r[4], i = 0; i < 64u * uint32_t(nmax > 6 ? nmax / 6 + 1 : 1) && n < nmax; a += 4, i++) {
        const uint32_t v = rd(m, a);
        if (v > 0x401000 && v < 0x916000) { std::snprintf(b, sizeof b, " %08x", v); s += b; n++; }
    }
    return s;
}
void install(reflex_vm::Machine& m) {
    if (!std::getenv("RX_RNG_LOG")) return;
    m.natives[0x78594bed] = [](reflex_vm::Machine& mm) -> uint32_t {   // msvcr90 rand
        const uint32_t p = ptd_now(mm);
        char who[16]; if (p == g_main_ptd) std::snprintf(who, sizeof who, "main"); else std::snprintf(who, sizeof who, "%08x", p);
        if (g_out) std::fprintf(g_out, "R %u %s %08x%s\n", g_frame, who, p ? rd(mm, p + 0x14) : 0u, rets(mm).c_str());
        return reflex_vm::RUN_ORIGINAL;
    };
    std::string list = std::getenv("RX_RNG_FUNCS") ? std::getenv("RX_RNG_FUNCS") : "";
    std::vector<uint32_t> fs;
    if (list.empty()) fs.assign(rx_rng_lcg_drawers, rx_rng_lcg_drawers + rx_rng_lcg_drawers_n);
    else for (size_t at = 0; at < list.size();) {
        size_t c = list.find(',', at); fs.push_back(uint32_t(std::strtoul(list.substr(at, c - at).c_str(), nullptr, 16)));
        at = c == std::string::npos ? list.size() : c + 1;
    }
    for (uint32_t a : fs) {
        if (m.natives.count(a)) { std::fprintf(g_out, "# %08x already has a native: not logged\n", a); continue; }
        m.natives[a] = [a](reflex_vm::Machine& mm) -> uint32_t {
            if (g_out) std::fprintf(g_out, "L %u %08x %u%s\n", g_frame, a, rd(mm, 0x009cb864), rets(mm).c_str());
            return reflex_vm::RUN_ORIGINAL;
        };
    }
}
}  // namespace

RX_PORT(0x0084ed20, RngProbeFrame, "thunk", 0) {
    const char* path = std::getenv("RX_RNG_PROBE");
    if (!path) return reflex_vm::RUN_ORIGINAL;
    if (!g_setup) {
        g_setup = true;
        g_out = std::fopen(path, "w");
        g_main_ptd = ptd_now(m);
        install(m);
        if (const char* d = std::getenv("RX_RNG_DUMP")) {   // development: guest words at the first frame ("addr,addr,...", hex)
            while (*d) { char* e; const uint32_t a = uint32_t(std::strtoul(d, &e, 16)); if (e == d) break;
                         const uint32_t v = rd(m, a); float f; std::memcpy(&f, &v, 4);
                         std::fprintf(g_out, "DUMP %08x %08x %g\n", a, v, double(f)); d = *e ? e + 1 : e; }
        }
        if (const char* d = std::getenv("RX_RNG_DUMP_BLOCK")) {   // development: "pointer,offset,words" (hex): the words at [pointer] + offset
            uint32_t p = 0, off = 0, n = 0;
            if (std::sscanf(d, "%x,%x,%x", &p, &off, &n) == 3) {
                const uint32_t base = rd(m, p) + off;
                std::fprintf(g_out, "BLOCK %08x %u", base, n);
                for (uint32_t i = 0; i < n; i++) std::fprintf(g_out, " %08x", rd(m, base + 4 * i));
                std::fprintf(g_out, "\n");
            }
        }
    } else {   // the frame that ended here
        const int st = raw_state(m);
        if (g_k < 0 && st == 1 && g_prev == 0) g_k = 0;   // the countdown entry: the first frame that ends in state 1 after a Pre frame
        g_prev = st;
        const uint32_t rs = g_main_ptd ? rd(m, g_main_ptd + 0x14) : 0u, gs = rd(m, 0x009cb860);
        std::fprintf(g_out, "F %u st %d k %d rs %08x gs %08x gc %u ps %d", g_frame, st, g_k, rs, gs, rd(m, 0x009cb864), post_state(m));
        if (std::getenv("RX_RNG_RESEED") && g_k >= 0 && (g_r0 || g_g0))
            std::fprintf(g_out, " n %d m %d", steps(g_r0, rs, 0x343fdu, 0x269ec3u), steps(g_g0, gs, 0x41c64e6du, 0x3039u));
        std::fprintf(g_out, "\n");
        std::fflush(g_out);
        if (g_k >= 0) g_k++;
        g_frame++;
    }
    if (const char* cs = std::getenv("RX_RNG_CUE_STIM")) {
        if (g_frame == uint32_t(std::atoi(cs))) {
            const uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0;
            uint32_t L = 0;   // the RaceActivityAudioLogic (vtable 0x00942cfc) whose activity (+0x50) is A
            for (uint32_t pg = 0x00400000; A && !L && pg < 0x40000000; pg += 0x1000) {
                const uint8_t* b = m.mem.page(pg); if (!b) continue;
                for (uint32_t off = 0; off + 0x54 <= 0x1000; off += 4) {
                    uint32_t v, a; std::memcpy(&v, b + off, 4); if (v != 0x00942cfcu) continue;
                    std::memcpy(&a, b + off + 0x50, 4); if (a == A) { L = pg + off; break; }
                }
            }
            const uint32_t list = A ? rd(m, A + 0x268) : 0, D = list ? rd(m, list) : 0, D2 = list ? rd(m, list + 4) : 0;
            const uint32_t P = D ? rd(m, D + 0x74) : 0, V = P ? rd(m, P + 0x1ac) : 0;
            std::fprintf(stderr, "[cuestim] frame %u activity %08x logic %08x (vt %08x) driver %08x vehicle %08x\n", g_frame, A, L, L ? rd(m, L) : 0u, D, V);
            if (L && D) {
                const uint32_t lap = 0x42a00000u;   // 80.0 s
                struct { uint32_t at; uint32_t a[4]; int n; } calls[] = {
                    {0x00582da0, {A, D}, 2}, {0x00582dc0, {A, D}, 2}, {0x00582de0, {A, D}, 2}, {0x00582e40, {A, D}, 2},
                    {0x00582e00, {D, lap}, 2}, {0x00582e20, {A, V}, 2}, {0x00583fb0, {A, D, 0, 0}, 4}, {0x00583a70, {A, D, D2}, 3},
                    {0x00582e60, {A, D, 2}, 3}, {0x00582e60, {A, D, 1}, 3},
                };
                for (auto& c : calls) m.call_function(c.at, c.a, c.n, L);
            }
        }
    }
    if (const char* fs = std::getenv("RX_RNG_FINISH_STIM")) {   // a stimulus to reach the race end (w-resultsrng): the event boots have no
        // rider input, so the local human never finishes and a race never ends. At the start of frame k = <value> (k as above) the local
        // human's driver gets the race's lap count (D+0xf1c = A+0xff070): MXActivityRaceStub::vf61 00566e70 then counts it as done, the
        // end-of-race check 00566ee0 runs its every-human-finished branch (the gaps, the AI estimates) and the race winds down; the UI's
        // own pages follow (WinddownEnd, Post, "Show Activity Results", PostEventResults:Start -> App.PopulateResultsTable).
        if (g_k == std::atoi(fs)) {
            const uint32_t G = rd(m, 0xa946b8), A = G ? rd(m, G + 0x7460 + 0x94) : 0, n = A ? rd(m, A + 0x26c) : 0, list = A ? rd(m, A + 0x268) : 0;
            for (uint32_t i = 0; i < n && i < 16; i++) {
                const uint32_t D = rd(m, list + 4 * i), P = D ? rd(m, D + 0x74) : 0;
                if (P != G + 0x7460) continue;
                if (rd(m, A) == 0x00943c14) {   // the Waypoint race (w-wpfill): its laps done D+0xf10 = A+0xff090 (vf61 0057eb80; vf42 00579300)
                    m.mem.write(D + 0xf10, 4, rd(m, A + 0xff090)); std::fprintf(stderr, "[finishstim] k %d waypoint driver %08x laps %u\n", g_k, D, rd(m, A + 0xff090));
                } else { m.mem.write(D + 0xf1c, 4, rd(m, A + 0xff070)); std::fprintf(stderr, "[finishstim] k %d driver %08x laps %u\n", g_k, D, rd(m, A + 0xff070)); }
            }
        }
    }
    if (const char* ts = std::getenv("RX_RNG_TICKERS")) {   // w-resultsrng: at the start of frame <value> the scenes' tickers in their tick
        // order (Engine::Update ticks the engine context's scenes +0x1c / +0x20 / +0x24, each SceneManager_Tick its tickers +0x2c1c): the
        // ticker, its vtable, vf19, and which one is the UI manager [0xa93fcc]
        if (g_frame == uint32_t(std::atoi(ts)) && g_out) {
            const uint32_t ctx = rd(m, 0xa95844), ui = rd(m, 0xa93fcc);
            for (uint32_t o = 0x1c; o < 0x28; o += 4) {
                const uint32_t sc = rd(m, ctx + o), b = rd(m, sc + 0x2c1c), e = rd(m, sc + 0x2c20);
                std::fprintf(g_out, "T scene +%x %08x active %u tickers %d\n", o, sc, rd(m, sc + 0x34), sc ? int(e - b) / 4 : 0);
                for (uint32_t a = b; sc && a < e && a < b + 4 * 4096; a += 4) {
                    const uint32_t t = rd(m, a), vt = rd(m, t);
                    std::fprintf(g_out, "T  %08x vt %08x vf19 %08x%s\n", t, vt, rd(m, vt + 0x4c), t == ui ? " UI" : "");
                }
            }
        }
    }
    // the frame that starts here
    g_r0 = g_g0 = 0;
    const int st0 = raw_state(m);
    if (const char* pre = std::getenv("RX_RNG_SEED_PRE"))
        if (st0 == 0 && g_main_ptd) m.mem.write(g_main_ptd + 0x14, 4, uint32_t(std::strtoul(pre, nullptr, 16)));
    if (std::getenv("RX_RNG_RESEED") && g_k >= 0 && g_main_ptd) {
        g_r0 = uint32_t(g_k) * 0x9e3779b9u + 1u; g_g0 = uint32_t(g_k) * 0x85ebca6bu + 7u;
        m.mem.write(g_main_ptd + 0x14, 4, g_r0); m.mem.write(0x009cb860, 4, g_g0);
    }
    return reflex_vm::RUN_ORIGINAL;
}
