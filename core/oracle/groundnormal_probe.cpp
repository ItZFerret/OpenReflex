/* groundnormal_probe.cpp -- the oracle's probe for the idle rider's fidget tail: 00508460 Body_Up (the vehicle's ground
 * normal) and 0061ea90 RiderAnim_StartIdleGesture, both called by the ported Player_UpdateIdleRider 0054cec0.
 *
 * The five reference worlds do not ride (the local player stands at the grid), so the fidget's 30 s idle clock never runs
 * there and the two functions are 0 calls in a normal Verify. The probe takes the live local human player at a chosen frame,
 * builds exactly the state 0054cec0's tail reads (the vehicle in the 0xa96dc0 speed table with a stored speed >= 0.6, the
 * rider's clip +0x580 equal to the state +0x5a0, the idle clock +0x10b8 >= 30 s, the upright clock +0x10b0 > 0.1, the try
 * latch +0x10b4 = 0, the bike's grounded word +0x8d98 nonzero (vf43, the 0xac slot) and its up vector +0x100..+0x10c as
 * (0, 1, 0, 0) - the grid world image's stale lanes there would fail the 0.3 test), and calls the ORIGINAL 0054cec0 so the
 * original reaches the installed ports: Verify compares the core port with the original per call, memory and registers. The
 * game's random seed 0x9cb860 is set so that the 0x10000 bit of the drawn value is clear (only Body_Up) or set (Body_Up +
 * the gesture start); variant 4 patches the vehicle's normal lane 1 below 0.707 (0x009cd99c) so the RNG and the gesture are
 * not reached.
 *
 *   RX_IDLE_PROBE=1 RX_IDLE_FRAME=<n, 1200> REFLEX_PORT_VERIFY=1 REFLEX_PORT="rx_world,rx_GroundNormalProbeHook"
 *     1: seed bit clear, the live listener value (no gesture decision)
 *     2: seed bit set, the +0x38c listener target at 0.5 -> the gesture starts (+0x5b6d4 = 1)
 *     3: seed bit set, the +0x38c listener target at 0.0 -> no gesture
 *     4: seed bit set, the vehicle's normal (0, 0.5, 0) -> below 0x009cd99c, no RNG, no gesture
 * State construction and original calls only; every touched game word is restored, no game logic. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/world_rider.h"
}

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
uint32_t g_frame = 0;

/* saved words, in write order; restored in reverse */
struct saved { uint32_t a, v; };
saved g_save[96];
int g_nsave = 0;
uint32_t get(uint32_t a) {
    for (int i = 0; i < g_nsave; i++) if (g_save[i].a == a) return g_save[i].v;
    return rd(a);
}
void set(uint32_t a, uint32_t v) {
    if (g_nsave < (int)(sizeof g_save / sizeof g_save[0])) g_save[g_nsave++] = {a, get(a)};
    wr(a, v);
}
void restore(void) {
    for (int i = g_nsave; i-- > 0;) wr(g_save[i].a, g_save[i].v);
    g_nsave = 0;
}

/* the first local human player of the race (kind 0) with a driver, a vehicle and a rider animation */
uint32_t pick_player(void) {
    const uint32_t g = rd(0x00a946b8u);
    if (!g) return 0;
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t p = rx_world_players_find(g + 0x7460u, i, 0);
        if (p && rd(p + 4) && rd(p + 0x94) && rd(p + 0x1ac) && rd(p + 0x1b4)) return p;
    }
    return 0;
}

/* the first enabled listener's target of the float property v (0 when none) */
uint32_t first_target(uint32_t v) {
    const uint32_t begin = rd(v + 0xc), end = rd(v + 0x10);
    for (uint32_t e = begin; e != end; e += 8) if (rd(e + 4)) return rd(e);
    return 0;
}

/* 0054cec0's tail state: the vehicle's speed-table entry >= 0.6, the rider's current clip, the 30 s idle clock, the upright
 * clock past 0.1 and no try yet. The bike object's vf37 vector (+0x100, the up axis the original normalizes) is built as
 * (0, 1, 0, 0) and its vf43 word (+0x8d98, the 0xac "on the ground" slot) as nonzero: in the grid world image the local bike
 * never rode, so those live lanes are stale (ai11's +0x8d98 is 0 there, Concord's up vector is garbage).
 * `gesture` < 0 leaves the +0x38c listener as it is. */
void idle_state(uint32_t p, int gesture) {
    const uint32_t v = rd(p + 0x1ac), r = rd(p + 0x1b4);
    const uint32_t b = rd(v + 0x16d4);
    uint32_t idx = 0;
    for (; idx < 12; idx++) if (rd(0x00a96dc0u + 0x40u * idx) == v) break;
    if (idx == 12) { set(0x00a96dc0u, v); idx = 0; }   /* not in the table: use entry 0 */
    set(0x00a96d90u + 0x40u * idx, fb(1.0f));
    set(r + 0x580, r + 0x5a0);                          /* the clip word is the state word: the test passes */
    set(p + 0x10b8, fb(30.0f));
    set(p + 0x10b0, fb(1.0f));
    set(p + 0x10b4, 0);
    if (b) {
        set(b + 0x100, fb(0.0f));
        set(b + 0x104, fb(1.0f));
        set(b + 0x108, fb(0.0f));
        set(b + 0x10c, fb(0.0f));
        set(b + 0x8d98, 1);                             /* vf43 (0xac): on the ground (nonzero) */
    }
    if (gesture >= 0) {
        const uint32_t t = first_target(r + 0x38c);
        if (t) set(t, fb(gesture ? 0.5f : 0.0f));
        else std::fprintf(stderr, "[idle] no +0x38c listener target (gesture value not set)\n");
    }
}

/* the drawn value of the LCG after one step */
uint32_t next_seed(uint32_t s) { return s * 0x41c64e6du + 0x3039u; }

void call_cec0(uint32_t p) {
    rx_regs in, out;
    std::memset(&in, 0, sizeof in); std::memset(&out, 0, sizeof out);
    in.r[7] = p;                                       /* EDI = p */
    const uint32_t a[1] = {fb(1.0f / 60.0f)};          /* stack dt; RET 4 */
    rx_call_original_ex(0x0054cec0u, &in, &out, 1, a);
}

/* a seed whose drawn value has / has not the 0x10000 bit */
uint32_t seed_with(uint32_t mask) {
    for (uint32_t s = 1; s < 0x1000u; s++) if ((next_seed(s) & 0x10000u) == mask) return s;
    return 1;
}

void variant(uint32_t p, const char* name, uint32_t seed, int gesture, int low_normal) {
    const uint32_t v = rd(p + 0x1ac), r = rd(p + 0x1b4);
    idle_state(p, gesture);
    if (low_normal) {
        set(v + 0x140, fb(0.0f));
        set(v + 0x144, fb(0.5f));
        set(v + 0x148, fb(0.0f));
    }
    const float ny = rx_bf(rd(v + 0x144));
    set(0x009cb860u, seed);
    set(0x009cb864u, 0);
    set(r + 0x5b6d4, 0);
    call_cec0(p);
    std::fprintf(stderr, "[idle] %s: normal.y %g, idle clock %g, upright %g, try %u, gesture state %u, seed %08x -> %08x\n",
                 name, (double)ny, (double)rx_bf(rd(p + 0x10b8)), (double)rx_bf(rd(p + 0x10b0)), rd(p + 0x10b4),
                 rd(r + 0x5b6d4), seed, rd(0x009cb860u));
    restore();
}
}  // namespace

RX_PORT(0x0084ed20, GroundNormalProbeHook, "thunk", 0) {
    if (!std::getenv("RX_IDLE_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_IDLE_FRAME") ? uint32_t(std::atoi(std::getenv("RX_IDLE_FRAME"))) : 1200u;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t p = pick_player();
    if (!p) {
        std::fprintf(stderr, "[idle] frame %u: no local human player with a vehicle and a rider animation\n", g_frame);
        return reflex_vm::RUN_ORIGINAL;
    }
    std::fprintf(stderr, "[idle] frame %u: player %08x vehicle %08x rider %08x\n", g_frame, p, rd(p + 0x1ac), rd(p + 0x1b4));
    variant(p, "1 seed clear", seed_with(0), -1, 0);
    variant(p, "2 gesture 0.5", seed_with(0x10000u), 1, 0);
    variant(p, "3 gesture 0.0", seed_with(0x10000u), 0, 0);
    variant(p, "4 low normal", seed_with(0x10000u), 1, 1);
    return reflex_vm::RUN_ORIGINAL;
}
