// raceend_probe.cpp -- the oracle's probe for the finish / results chain (development only): a frame hook (group "thunk",
// Engine::Update 0084ed20) that at one chosen frame calls the finish functions' original addresses with the live activity, so each
// installed rx_race port is verified per call (the core runs, then the original on the same state) without waiting for a 30,000
// frame race:
//   REFLEX_PORT=rx_race,rx_RaceEndProbeHook,... REFLEX_PORT_VERIFY=1 RX_RACEEND_PROBE=1 [RX_RACEEND_PROBE_FRAME=1200]
// The probe only builds the calls / argument slots. Every call is verified and the framework restores the entry state, so the
// world continues unchanged after the probe.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
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
uint32_t bitsf(float f) { return rx_oracle::bits(f); }

void call_records(uint32_t rec, uint32_t slot) {
    if (!rec) return;
    const uint32_t e = rec + 0x18 * slot;
    const uint32_t table = rd(e), row = rd(e + 4);
    if (!table) return;
    const uint32_t fbits = bitsf(6.0f);
    rx_regs in = regs(); in.r[1] = rec; in.r[0] = slot;
    { const uint32_t a[1] = {5}; call(0x00764cb0, in, 1, a); }                      /* Placing */
    { const uint32_t a[1] = {fbits}; call(0x00764d80, in, 1, a); }                  /* Score */
    { const uint32_t a[1] = {bitsf(70.0f)}; call(0x00764e50, in, 1, a); }          /* BestLap */
    if (row != 0xffffffffu) {                                                       /* AddTotalTime */
        const uint32_t sp = rx_sp();
        const uint32_t ref = sp - 0x10;
        wr(ref, row); wr(ref + 4, 0); wr(ref + 8, table);
        rx_set_sp(ref);
        const uint32_t a[2] = {ref, bitsf(75.0f)};
        rx_regs in2 = regs();
        rx_regs o2 = regs();
        rx_call_original_ex(0x00765020, &in2, &o2, 2, a);
        rx_set_sp(sp);
    }
}
}  // namespace

RX_PORT(0x0084ed20, RaceEndProbeHook, "thunk", 0) {
    static const bool on = std::getenv("RX_RACEEND_PROBE") != nullptr;
    if (!on) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RACEEND_PROBE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RACEEND_PROBE_FRAME"))) : 1200;
    static uint32_t frame = 0;
    if (++frame != at) return reflex_vm::RUN_ORIGINAL;

    const uint32_t G = rd(0xa946b8);
    const uint32_t P = G + 0x7460;
    const uint32_t A = rd(P + 0x94);
    if (!A) { std::fprintf(stderr, "[raceend probe] no activity at frame %u\n", frame); return reflex_vm::RUN_ORIGINAL; }
    const uint32_t list = rd(A + 0x268), n = rd(A + 0x26c);
    const uint32_t d0 = list ? rd(list) : 0;
    const uint32_t d1 = (list && n > 1) ? rd(list + 4) : d0;
    const uint32_t dt = bitsf(1.0f / 60.0f);
    std::fprintf(stderr, "[raceend probe] frame %u act %08x state %d drivers %u\n", frame, A, int(rd(A + 0x21c)), n);

    /* WindDown enter (Activity::vf40 / stub vf40) */
    for (uint32_t fn : {0x00559600u, 0x00568f60u}) {
        rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {2}; call(fn, in, 1, a);
    }
    /* WindDown update with the page-wipe timer run out */
    { const uint32_t s = rd(A + 0x2c4); wr(A + 0x2c4, 0);
      for (uint32_t fn : {0x00565260u, 0x00559720u}) { rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {dt}; call(fn, in, 1, a); }
      wr(A + 0x2c4, s); }
    /* Post enter (Activity::vf41, stub vf41 / its vtable thunk) */
    for (uint32_t fn : {0x00559900u, 0x00565560u, 0x00568da0u}) {
        rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {3}; call(fn, in, 1, a);
    }
    /* the Post helpers */
    { rx_regs in = regs(); in.r[1] = A; call(0x0055a440, in); }
    { rx_regs in = regs(); in.r[2] = A; in.r[6] = 1; call(0x0055a470, in); }
    /* page wipe raise + transition (state 3 so the transition enters Post) */
    { rx_regs in = regs(); in.r[1] = 0x00941de8; const uint32_t a[1] = {A}; call(0x0055e990, in, 1, a); }
    { const uint32_t st = rd(A + 0x21c); wr(A + 0x21c, 3);
      rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {0x00941de8}; call(0x0055e9e0, in, 1, a);
      wr(A + 0x21c, st); }
    /* Post update and its sub-states */
    { const uint32_t s = rd(A + 0xff064); wr(A + 0xff064, 0);
      rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {dt}; call(0x00568d50, in, 1, a);
      wr(A + 0xff064, s); }
    for (uint32_t sub = 0; sub <= 3; sub++) {
        const uint32_t s = rd(A + 0x13e640); wr(A + 0x13e640, sub);
        { rx_regs in = regs(); in.r[0] = A; in.r[6] = A; const uint32_t a[1] = {dt}; call(0x00569a90, in, 1, a); }
        wr(A + 0x13e640, s);
    }
    { rx_regs in = regs(); in.r[6] = A; in.r[0] = 2; call(0x00569a40, in); }
    /* The post scenario event raisers (00587300 "Post State Complete", 00587ea0 "Post Scenarios Start/End") are NOT called here:
     * at a mid-race state their UI / scenario handlers dereference objects the finish page would have built (probed: 008519a9
     * from 00587325 through 004020d9). They are covered by the long world's natural transition (builds/raceend verify). */
    /* the standings' finishing order, the estimated gap, the records */
    { rx_regs in = regs(); in.r[0] = A; call(0x00569770, in); }
    if (d0 && d1) { rx_regs in = regs(); in.r[6] = A; in.r[7] = d0; const uint32_t a[1] = {d1}; call(0x00567d50, in, 1, a); }
    call_records(rd(0xa946c0), 0);
    /* the exits: WindDown -> Post, Post -> Inactive */
    { rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {4}; call(0x00565500, in, 1, a); }
    for (uint32_t fn : {0x0057e870u, 0x00568fb0u}) { rx_regs in = regs(); in.r[1] = A; const uint32_t a[1] = {5}; call(fn, in, 1, a); }
    std::fprintf(stderr, "[raceend probe] done\n");
    return reflex_vm::RUN_ORIGINAL;
}
