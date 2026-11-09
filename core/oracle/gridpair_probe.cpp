// gridpair_probe.cpp -- the ORIGINAL's side of the paired product-vs-original ride (agent w-gridpair; development only).
// The product's side is core/tools/bike_parity_probe.c with PROBE_PAIR=<file>; both write core/tools/gridpair_line.inc's line, and
// core/tools/gridpair_cmp.py aligns the two on the countdown entry and compares them field by field.
//
//   REFLEX_PORT=rx_PairFrameTrace[,rx_PairGridTrace][,rx_WhipReversePadHook] RX_PAIR_TRACE=<out file> [RX_PAIR_RESEED=1]
//
// RX_PAIR_RESEED=1 (test hook, as the product driver's PROBE_RESEED): both generators set at the start of every event Pre frame and of
// every frame after the countdown entry (gridpair_line.inc pair_next_seed), so both sides draw from the same state every frame.
//
// rx_PairFrameTrace: a frame hook at Engine::Update 0084ed20 (the main thread, one per Engine::Frame in a baton boot, as rng_probe.cpp's
// frame hook); at each entry it writes the line of the frame that ended there. The product driver writes its line after
// rx_session_advance_dt, i.e. at the same point of the loop (after the frame's render and tail).
// rx_PairGridTrace: one "G <frame> gs <g_GameRandSeed> gc <g_GameRandCount> args ..." line at the entry of the grid pass 0075f890
// (the race branch draws the local StartZone from the game LCG: gridfix).
// Nothing is written to guest memory except under RX_PAIR_RESEED. (group "thunk": never verified itself, select it by its name.)
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;

namespace {
reflex_vm::Machine* g_m = nullptr;
uint32_t pair_rd(uint32_t a) { return g_m->mem.mapped(a) && g_m->mem.mapped(a + 3) ? uint32_t(g_m->mem.read(a, 4)) : 0u; }
#define PAIR_RD(a) pair_rd(uint32_t(a))
}  // namespace
extern "C" void rx_vehicle_bound_centre(uint32_t vehicle, uint32_t centre, uint32_t radius);   // 00533510, the core's port
namespace {
// the core's port on the original's memory (w-hfterrain: pair_bounds_dump), at an ESP well below the hook's
void pair_centre(uint32_t v, uint32_t out) {
    rx_oracle::Bind bind(*g_m);
    const uint32_t sp = g_m->r[4]; g_m->r[4] = sp - 0x4000u;
    rx_vehicle_bound_centre(v, out, out + 0xc);
    g_m->r[4] = sp;
}
#define PAIR_CENTRE(v, out) pair_centre((v), (out))
#include "../tools/gridpair_line.inc"
#undef PAIR_RD
FILE* g_out = nullptr;
uint32_t g_main_ptd = 0;
unsigned long g_frame = 0;
FILE* out() {
    if (!g_out) { const char* p = std::getenv("RX_PAIR_TRACE"); g_out = p && *p ? std::fopen(p, "w") : stderr; }
    return g_out;
}
}  // namespace
extern const uint32_t rx_rng_lcg_drawers[];   // rng_probe.cpp: every function with an inlined game-LCG step
extern const size_t rx_rng_lcg_drawers_n;
namespace {
// RX_PAIR_LCGLOG=1 (w-lcgdraws): "L <frame> <function> <g_GameRandCount at its entry> <return address>" at the entry of every game-LCG
// drawing function, in the pair file between the frame lines (a function's draws = the next line's count - its count), for the
// per-frame draw-by-draw comparison with a traced product build. Functions that already have a native (a port) are not logged.
void pair_lcglog_install(reflex_vm::Machine& m) {
    for (size_t i = 0; i < rx_rng_lcg_drawers_n; i++) {
        const uint32_t a = rx_rng_lcg_drawers[i];
        if (m.natives.count(a)) { std::fprintf(out(), "# %08x already has a native: not logged\n", a); continue; }
        m.natives[a] = [a](reflex_vm::Machine& mm) -> uint32_t {
            std::fprintf(out(), "L %lu %08x %u %08x\n", g_frame, a, pair_rd(0x009cb864), pair_rd(mm.r[4]));
            return reflex_vm::RUN_ORIGINAL;
        };
    }
}
}  // namespace

RX_PORT(0x0084ed20, PairFrameTrace, "thunk", 0) {
    g_m = &m;
    if (std::getenv("RX_PAIR_NODEFORM") && (pair_rd(0x009e30e0) & 1)) {   // test switch, as bike_parity_probe.c's PROBE_NODEFORM: the active
        const uint32_t scene = pair_rd(0x00a95844);                       // terrain's deformation gain (+0xbc) held at 0 (ruts off)
        for (uint32_t k = 0; scene && k < pair_rd(0x009e30dc); k++) {
            const uint32_t t = pair_rd(0x009e30d4 + 4 * k);
            if (t && pair_rd(scene + 0x1c + 4 * pair_rd(scene + 4)) == pair_rd(t + 0x44a8)) m.mem.write(t + 0xbc, 4, 0u);
        }
    }
    if (!g_main_ptd) {
        g_main_ptd = uint32_t(m.call_function(0x7854383f, nullptr, 0));   // _getptd_noexit (the main thread)
        if (std::getenv("RX_PAIR_LCGLOG")) pair_lcglog_install(m);
    } else {
        pair_line(out(), g_frame, g_main_ptd ? pair_rd(g_main_ptd + 0x14) : 0u);
        pair_kpoke(g_frame, [](uint32_t a, uint32_t w) { g_m->mem.write(a, 4, w); });   // w-damagetick PAIR_KPOKE
        if (const char* td = std::getenv("RX_PAIR_TERRAIN"); td && std::strtoul(td, nullptr, 10) == g_frame) pair_terrain_dump(out(), g_frame);
        // RX_PAIR_SIMDUMP=<frame>[,<frame>...]: after those frames' lines, the local sim's dump (gridpair_line.inc pair_simdump)
        for (const char* sd = std::getenv("RX_PAIR_SIMDUMP"); sd && *sd; ) {
            char* e; const unsigned long fr = std::strtoul(sd, &e, 10); if (e == sd) break;
            if (fr == g_frame) pair_simdump(out(), g_frame, std::getenv("RX_PAIR_SIMDUMP_SLOT") ? std::atoi(std::getenv("RX_PAIR_SIMDUMP_SLOT")) : -1);
            sd = *e == ',' ? e + 1 : e;
        }
        for (const char* bd = std::getenv("RX_PAIR_BOUNDS"); bd && *bd; ) {   // w-hfterrain: frame[,frame...] -> pair_bounds_dump
            char* e; const unsigned long fr = std::strtoul(bd, &e, 10); if (e == bd) break;
            if (fr == g_frame) pair_bounds_dump(out(), g_frame, (m.r[4] - 0x200u) & ~15u);
            bd = *e == ',' ? e + 1 : e;
        }
        // RX_PAIR_POKE=frame:addr:word[,...] (w-damagetick, a test stimulus as bike_parity_probe.c's PROBE_POKE): guest words written
        // after that frame's line (e.g. damage manager part states / hit points, so the parts break in a short ride)
        for (const char* pk = std::getenv("RX_PAIR_POKE"); pk && *pk; ) {
            char* e; const unsigned long fr = std::strtoul(pk, &e, 10); if (e == pk || *e != ':') break;
            const unsigned long a = std::strtoul(e + 1, &e, 16); if (*e != ':') break;
            const unsigned long w = std::strtoul(e + 1, &e, 16);
            if (fr == g_frame) m.mem.write(uint32_t(a), 4, uint32_t(w));
            pk = *e == ',' ? e + 1 : e;
        }
        // RX_PAIR_WATCH=<frame>:<addr>:<file> (w-parity2, development only): every guest write to <addr> during that frame (JIT off),
        // as REFLEX_WATCH logs it (pc, esp, the code addresses on the stack), into <file>
        if (const char* wt = std::getenv("RX_PAIR_WATCH")) {
            char* e; const unsigned long fr = std::strtoul(wt, &e, 10);
            if (*e == ':') {
                const unsigned long a = std::strtoul(e + 1, &e, 16);
                if (fr == g_frame + 1 && *e == ':') { m.watch_address = uint32_t(a); m.trace_out = std::fopen(e + 1, "w"); m.trace_from = m.trace_to = 0; m.enable_jit(false); }
                else if (fr == g_frame && m.watch_address) { m.watch_address = 0; if (m.trace_out) std::fclose(static_cast<FILE*>(m.trace_out)); m.trace_out = nullptr; m.enable_jit(true); }
            }
        }
        std::fflush(out());
        uint32_t crt, lcg;
        if (std::getenv("RX_PAIR_RESEED") && pair_next_seed(pair_state(), &crt, &lcg)) {
            if (g_main_ptd) m.mem.write(g_main_ptd + 0x14, 4, crt);
            m.mem.write(0x009cb860, 4, lcg);
        }
    }
    // w-aidrift: RX_PAIR_STACK_POISON=<word> (hex; a test alignment, the oracle's PROBE_STACK_POISON): the 128 KB of the main thread's
    // stack below Engine::Update's entry ESP filled with the word at every frame start. The product has no render pass (Unity draws),
    // so whatever the original's Engine::Render left below that ESP at the end of the previous frame (the D3D9 stand-ins write no
    // guest stack) is dead stack only the original has; with both sides poisoned alike the frame starts from the same dead stack
    // (core/notes/phys.md "The render pass's residue").
    if (const char* sp = std::getenv("RX_PAIR_STACK_POISON")) {
        const uint32_t w = uint32_t(std::strtoul(sp, nullptr, 16)), esp = m.r[4];
        for (uint32_t a = esp - 0x20000u; a < esp; a += 4) if (m.mem.mapped(a)) m.mem.write(a, 4, w);
    }
    g_frame++;
    return reflex_vm::RUN_ORIGINAL;
}

RX_PORT(0x0075f890, PairGridTrace, "thunk", 0) {
    g_m = &m;
    std::fprintf(out(), "G %lu gs %08x gc %u args %08x %08x %08x %08x\n", g_frame, pair_rd(0x009cb860), pair_rd(0x009cb864), m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    std::fflush(out());
    return reflex_vm::RUN_ORIGINAL;
}

// w-parity4w (development): the four-wheel wheel queries' triangle test 00438260 as the UTV's wheel query 004d3140 calls it (return
// 004d3317): "WT <frame> 0 <q 4> <p 4> <e 4> <count> <triangles fnv>" at the entry and "WT <frame> 1 <out point 4> <out normal 4> <EAX>"
// at the return site (the ATV's 00497f00 too: return 004980d7, rx_PairWheelTriLogRetAtv), for the frames RX_PAIR_WT_FROM .. RX_PAIR_WT_TO into RX_PAIR_WT (the product side: a scratch build whose
// bike_wheel_triangle_contact_observed calls a parity_wt_hook). Select both by name: rx_PairWheelTriLog,rx_PairWheelTriLogRet.
namespace {
FILE* g_wt = nullptr;
uint32_t g_wt_op = 0, g_wt_on = 0;
bool wt_on() {
    const char* p = std::getenv("RX_PAIR_WT");
    if (!p) return false;
    const unsigned long lo = std::getenv("RX_PAIR_WT_FROM") ? std::strtoul(std::getenv("RX_PAIR_WT_FROM"), nullptr, 10) : 0;
    const unsigned long hi = std::getenv("RX_PAIR_WT_TO") ? std::strtoul(std::getenv("RX_PAIR_WT_TO"), nullptr, 10) : 0;
    if (g_frame < lo || g_frame > hi) return false;
    if (!g_wt) g_wt = std::fopen(p, "w");
    return g_wt != nullptr;
}
}  // namespace
RX_PORT(0x00438260, PairWheelTriLog, "thunk", 0) {
    g_m = &m;
    const uint32_t ret = pair_rd(m.r[4]);
    if ((ret != 0x004d3317u && ret != 0x004980d7u) || !wt_on()) return reflex_vm::RUN_ORIGINAL;
    g_wt_op = m.r[1]; g_wt_on = m.r[7];
    std::fprintf(g_wt, "WT %lu 0", g_frame);
    for (int a = 0; a < 3; a++) for (uint32_t k = 0; k < 4; k++) std::fprintf(g_wt, " %08x", pair_rd(m.arg(a) + 4 * k));
    const int32_t n = int32_t(m.arg(4));
    uint64_t h = 1469598103934665603ull;
    for (uint32_t o = 0; n > 0 && o < 48u * uint32_t(n); o += 4) { h ^= pair_rd(m.arg(3) + o); h *= 1099511628211ull; }
    std::fprintf(g_wt, " %d %016llx", n, (unsigned long long)h);
    if (std::getenv("RX_PAIR_WT_TRIS")) { std::fprintf(g_wt, " T"); for (uint32_t o = 0; n > 0 && o < 48u * uint32_t(n); o += 4) std::fprintf(g_wt, " %08x", pair_rd(m.arg(3) + o)); }
    std::fprintf(g_wt, "\n");
    return reflex_vm::RUN_ORIGINAL;
}
// w-parity4w (development): "NT <frame> <S+0x60> <n xyz> <c xyz>" at the entry of 0081c590 (the disc against a cylinder: its Newton step
// reads the frame slot S + 0x60 the callers' dead stack leaves; S = ((ESP - 4) & ~15) - 0xb0 after its prologue) for the frames
// RX_PAIR_NT_FROM .. RX_PAIR_NT_TO into RX_PAIR_NT (the product side: a scratch world_node_geom.c calling a parity_newton_hook). Select
// by name: rx_PairNewtonLog.
RX_PORT(0x0081c590, PairNewtonLog, "thunk", 0) {
    g_m = &m;
    const char* p = std::getenv("RX_PAIR_NT");
    const unsigned long lo = std::getenv("RX_PAIR_NT_FROM") ? std::strtoul(std::getenv("RX_PAIR_NT_FROM"), nullptr, 10) : 0;
    const unsigned long hi = std::getenv("RX_PAIR_NT_TO") ? std::strtoul(std::getenv("RX_PAIR_NT_TO"), nullptr, 10) : 0;
    if (!p || g_frame < lo || g_frame > hi) return reflex_vm::RUN_ORIGINAL;
    static FILE* f = nullptr;
    if (!f) f = std::fopen(p, "w");
    const uint32_t S = ((m.r[4] - 4u) & ~15u) - 0xb0u;
    std::fprintf(f, "NT %lu %08x S %08x", g_frame, pair_rd(S + 0x60), S);
    for (uint32_t k = 0; k < 12; k += 4) std::fprintf(f, " %08x", pair_rd(m.r[1] + k));
    for (uint32_t k = 0; k < 12; k += 4) std::fprintf(f, " %08x", pair_rd(m.r[0] + k));
    std::fprintf(f, "\n");
    return reflex_vm::RUN_ORIGINAL;
}
RX_PORT(0x004980d7, PairWheelTriLogRetAtv, "thunk", 0) {   // the ATV's wheel query 00497f00's return site
    g_m = &m;
    if (!g_wt_op || !wt_on()) return reflex_vm::RUN_ORIGINAL;
    std::fprintf(g_wt, "WT %lu 1", g_frame);
    for (uint32_t k = 0; k < 4; k++) std::fprintf(g_wt, " %08x", pair_rd(g_wt_op + 4 * k));
    for (uint32_t k = 0; k < 4; k++) std::fprintf(g_wt, " %08x", pair_rd(g_wt_on + 4 * k));
    std::fprintf(g_wt, " %u\n", m.r[0]);
    g_wt_op = 0;
    return reflex_vm::RUN_ORIGINAL;
}
RX_PORT(0x004d3317, PairWheelTriLogRet, "thunk", 0) {
    g_m = &m;
    if (!g_wt_op || !wt_on()) return reflex_vm::RUN_ORIGINAL;
    std::fprintf(g_wt, "WT %lu 1", g_frame);
    for (uint32_t k = 0; k < 4; k++) std::fprintf(g_wt, " %08x", pair_rd(g_wt_op + 4 * k));
    for (uint32_t k = 0; k < 4; k++) std::fprintf(g_wt, " %08x", pair_rd(g_wt_on + 4 * k));
    std::fprintf(g_wt, " %u\n", m.r[0]);
    g_wt_op = 0;
    return reflex_vm::RUN_ORIGINAL;
}

// w-parity4w (development): RX_DEAD_POISON=<lo>:<hi>:<word> (hex byte offsets below ESP, hex word): at the return site of the ATV's
// air control 00494910 (in 00497a70, 00497ac1) the dead stack [ESP - hi, ESP - lo) is filled with the word, to find what reads the
// words the function leaves (an On-mode run of its port differs from the original only by them). Select by name: rx_PairDeadPoison.
RX_PORT(0x00497ac1, PairDeadPoison, "thunk", 0) {
    const char* e = std::getenv("RX_DEAD_POISON");
    if (!e) return reflex_vm::RUN_ORIGINAL;
    unsigned lo = 0, hi = 0, w = 0;
    if (std::sscanf(e, "%x:%x:%x", &lo, &hi, &w) != 3) return reflex_vm::RUN_ORIGINAL;
    for (uint32_t a = m.r[4] - hi; a < m.r[4] - lo; a += 4) m.mem.write(a, 4, w);
    return reflex_vm::RUN_ORIGINAL;
}
