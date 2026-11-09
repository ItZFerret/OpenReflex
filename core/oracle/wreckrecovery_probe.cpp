/* wreckrecovery_probe.cpp -- the oracle's probe for the wreck-recovery-success path (0054ec80 WreckRecoverySuccess and its
 * callees 006ab9b0 ClipRecorder_LogRecover / 006d83d0 highlight progress). Development only, the same device as
 * rideranim2_probe.cpp. The five reference worlds / the crash drive 7 DO reach Player_UpdateWreckRecovery (0054ea30: 1800 calls)
 * and its stick read (0054ee80: 3), but never move the recovery stick far enough in the mode's direction inside the wreck clock,
 * so 0054ec80 is 0 calls there (measured). The probe takes the live local human player at a chosen frame, builds the wreck
 * recovery state 0054ea30 needs (a wreck clock, mode 2 = "y rising", the AI flag so 0054ee80 returns the fixed axes +0xb50 /
 * +0xb54), and calls the ORIGINAL 0054ea30 (or the original 0054ec80 for the branches 0054ea30 cannot set) so the original
 * reaches the installed port: Verify compares the core port with the original per call, memory and registers.
 *
 *   RX_WR_PROBE=1 RX_WR_FRAME=<n, 1200> RX_WR_MODE=<1|2|3> REFLEX_PORT_VERIFY=1 REFLEX_PORT="...,rx_WreckRecoveryProbeHook"
 *     1: 0054ec80 installed (0054ea30 NOT): the success path through the original caller. Variants: rider animation present /
 *        none (+0x1b4 = 0), the waiting-list id (+0x908 = 1).
 *     2: 006ab9b0 installed, 0054ec80 NOT: the original 0054ec80's clip log. Variants: recording on / off (0xd5a9a4), a local
 *        player (+0x10 = 0, the candidate branch) / an AI one (ring entry only), +4 = 0 (no ring entry). The pad slot is -1 so
 *        the highlight branch of the same call is skipped (the world has no profile: 006d7980 faults there; mode 3 replaces it).
 *     3: 006d83d0 installed, 0054ec80 NOT: the original 0054ec80's highlight progress. Driver modes 0, 1, 2, 6, 0xc, 0x13 add
 *        0x2a (+0x2b always); 0x14 (out of the switch) adds only 0x2b; a slot of -1 skips it. The profile call 006d7980 is
 *        replaced by a counting stand-in that both runs see (the same device as the racedrv marker stand-ins): it records the
 *        (index, slot) of every call into a probe-owned heap block, so a dispatch difference is visible to Verify's memory
 *        compare (the world has no profile database to run the real one).
 * State construction and original calls only; every touched game word is restored, no game logic. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/world_rider.h"
#include "../game/build_alloc.h"
}

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
uint32_t g_frame = 0;

/* saved words, in write order; restored in reverse */
struct saved { uint32_t a, v; };
saved g_save[24];
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

/* the profile call 006d7980 as both runs see it in mode 3: the (index, slot) of each call into the probe's block (index * 4 and
 * 0x800 + index * 4; both are written with the slot, so a missing or extra call changes the block) */
uint32_t g_scratch = 0;
uint32_t highlight_standin(reflex_vm::Machine& m) {
    const uint32_t esp = m.r[4];
    const uint32_t pad = m.mem.mapped(esp + 8) ? uint32_t(m.mem.read(esp + 8, 4)) : 0;
    const uint32_t idx = m.mem.mapped(esp + 0xc) ? uint32_t(m.mem.read(esp + 0xc, 4)) : 0;
    if (g_scratch && idx < 0x100) {
        m.mem.write(g_scratch + 4u * idx, 4, pad);
        m.mem.write(g_scratch + 0x800u + 4u * idx, 4, pad ^ 0xa5a5a5a5u);
    }
    std::fprintf(stderr, "[wr] highlight stand-in: slot %u index 0x%x\n", pad, idx);
    return m.ret(0xc, 0);
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

void call_ea30(uint32_t p) {
    rx_regs in, out;
    std::memset(&in, 0, sizeof in); std::memset(&out, 0, sizeof out);
    in.r[0] = p;                                       /* EAX = p (the original's edi) */
    const uint32_t a[1] = {fb(1.0f / 60.0f)};          /* stack dt; RET 4 */
    rx_call_original_ex(0x0054ea30u, &in, &out, 1, a);
}
void call_ec80(uint32_t p) {
    rx_regs in, out;
    std::memset(&in, 0, sizeof in); std::memset(&out, 0, sizeof out);
    const uint32_t a[1] = {p};                         /* stack p; RET 4 */
    rx_call_original_ex(0x0054ec80u, &in, &out, 1, a);
}

/* the state 0054ea30 needs: a pending wreck (+0xb38), mode 2 ("y rising"), the fixed axes of 0054ee80 (+0x10 = 1) with y above
 * the previous value and the accumulator past 0.25 */
void wreck_state(uint32_t p, uint32_t ai, uint32_t rider, uint32_t waiting, int32_t slot) {
    set(p + 0x04, 1);
    set(p + 0x10, ai);
    set(p + 0x1f0, (uint32_t)slot);
    set(p + 0x1b4, rider);
    set(p + 0x0908, waiting);
    set(p + 0x0b38, fb(1.0f));
    set(p + 0x0b40, 0);
    set(p + 0x0b44, 2);
    set(p + 0x0b48, 0);
    set(p + 0x0b4c, 0);
    set(p + 0x0b50, 0);              /* x: the wreck steer, unused by mode 2 */
    set(p + 0x0b54, fb(1.0f));       /* y: the wreck lean, rising above +0xb4c */
    set(p + 0x01ec, 0);
}
}  // namespace

RX_PORT(0x0084ed20, WreckRecoveryProbeHook, "thunk", 0) {
    if (!std::getenv("RX_WR_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_WR_FRAME") ? uint32_t(std::atoi(std::getenv("RX_WR_FRAME"))) : 1200u;
    static const int mode = std::getenv("RX_WR_MODE") ? std::atoi(std::getenv("RX_WR_MODE")) : 1;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t p = pick_player();
    if (!p) {
        std::fprintf(stderr, "[wr] frame %u: no local human player with a vehicle and a rider animation\n", g_frame);
        return reflex_vm::RUN_ORIGINAL;
    }
    const uint32_t rider = rd(p + 0x1b4), drv = rd(p + 0x94), race = rd(0x00a942b8u);
    std::fprintf(stderr, "[wr] frame %u mode %d: player %08x rider %08x driver %08x race %08x run mode %u recording %u\n",
                 g_frame, mode, p, rider, drv, race, rd(0x00a942b4u), rd(0x00d5a9a4u));
    if (mode == 1) {
        /* the rider animation present, then absent: 0054ec80 stops it only when +0x1b4 is set */
        wreck_state(p, 1, rider, 0, 5);
        call_ea30(p);
        std::fprintf(stderr, "[wr] 1 success rider: count %u, wreck clock %g, accumulator %g\n",
                     rd(p + 0x11d0), (double)rx_bf(rd(p + 0x0b38)), (double)rx_bf(rd(p + 0x0b40)));
        restore();
        wreck_state(p, 1, 0, 0, 5);
        call_ea30(p);
        std::fprintf(stderr, "[wr] 1 success no rider: count %u, rider %08x\n", rd(p + 0x11d0), rd(p + 0x1b4));
        restore();
        wreck_state(p, 1, rider, 1, 5);          /* the clip ring's id from the waiting list (+0x27c) */
        call_ea30(p);
        std::fprintf(stderr, "[wr] 1 success waiting id: count %u\n", rd(p + 0x11d0));
        restore();
    } else if (mode == 2) {
        /* the original 0054ec80 directly: the clip log's branches (slot -1: its highlight branch is skipped) */
        wreck_state(p, 0, rider, 0, -1);
        set(0x00d5a9a4u, 1);                  /* recording on: the candidate branch */
        call_ec80(p);
        std::fprintf(stderr, "[wr] 2 log recover local, recording: +0x11d0 %u\n", rd(p + 0x11d0));
        restore();
        wreck_state(p, 1, rider, 0, -1);      /* AI: no candidate, the ring entry only */
        set(0x00d5a9a4u, 1);
        call_ec80(p);
        std::fprintf(stderr, "[wr] 2 log recover ai, recording: +0x11d0 %u\n", rd(p + 0x11d0));
        restore();
        wreck_state(p, 0, rider, 0, -1);      /* recording off */
        set(0x00d5a9a4u, 0);
        call_ec80(p);
        std::fprintf(stderr, "[wr] 2 log recover local, no recording: +0x11d0 %u\n", rd(p + 0x11d0));
        restore();
        wreck_state(p, 0, rider, 0, -1);      /* +4 = 0: not an active rider */
        set(p + 0x04, 0);
        set(0x00d5a9a4u, 1);
        call_ec80(p);
        std::fprintf(stderr, "[wr] 2 log recover inactive, recording: +0x11d0 %u\n", rd(p + 0x11d0));
        restore();
    } else {
        /* the original 0054ec80 directly: the driver mode's highlight progress, 006d7980 as the counting stand-in */
        static const uint32_t modes[] = {0, 1, 2, 6, 0xc, 0x13, 0x14, 3};
        if (!g_scratch) {
            g_scratch = rx_reflex_aligned_alloc(0x1000);
            std::fprintf(stderr, "[wr] 3 highlight stand-in block %08x\n", g_scratch);
        }
        static bool installed = false;
        if (g_scratch && !installed) { reflex_port::runtime_native(m, 0x006d7980u, highlight_standin); installed = true; }
        for (size_t i = 0; i < sizeof modes / sizeof modes[0]; i++) {
            wreck_state(p, 0, rider, 0, 5);
            set(drv + 0x23c, modes[i]);
            call_ec80(p);
            std::fprintf(stderr, "[wr] 3 highlight mode %u: +0x11d0 %u\n", modes[i], rd(p + 0x11d0));
            restore();
        }
        wreck_state(p, 0, rider, 0, -1);       /* slot -1: no highlight progress */
        set(drv + 0x23c, 0);
        call_ec80(p);
        std::fprintf(stderr, "[wr] 3 highlight slot -1: +0x11d0 %u\n", rd(p + 0x11d0));
        restore();
    }
    return reflex_vm::RUN_ORIGINAL;
}
