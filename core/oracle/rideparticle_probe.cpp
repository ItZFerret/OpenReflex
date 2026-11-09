/* rideparticle_probe.cpp -- the oracle's probe for the rider's lens-swipe effect vf3 0063c040
 * (AttachableParticleEffect_ParticleEmitter::vf3, rider + 0x5b6dc, vtable 0x00958908), the function the idle gesture 0061eac0
 * attaches in its state 2 once the clamped float +0x3bc has passed the attach threshold [0x9d78c4].
 *
 * The five reference worlds never reach it (the local player stands at the grid and the fidget's 30 s idle clock never runs), so
 * the probe builds the gesture state at a chosen frame and calls the ORIGINAL 0061eac0 with the live local human's rider: the
 * original's state-2 block evaluates the rider's model node (its vf74), writes it to the effect's +0x100 and calls the effect's
 * vtable slot 3, so the port adapter at 0063c040 runs per call under Verify (memory and registers). Every word the probe
 * writes is restored; the function's own writes (the effect object, the emitter state, the node's world matrix, the LCG and the
 * manager) stay, identically in the reference and the port run.
 *
 *   RX_RIDEP_PROBE=1 RX_RIDEP_FRAME=<n, 300>
 *   REFLEX_PORT="rx_fx,rx_RideParticleProbeHook" REFLEX_PORT_VERIFY=1   (0063c040 alone, 0061eac0 original)
 *   REFLEX_PORT="rx_fx,rx_world,rx_RideParticleProbeHook" ...           (the core caller + the core callee composed)
 *
 * Cases per frame: the effect unbound (+4 == 0) with a non-additive and (when the set has one) an additive type record; bound
 * with the attached flag set (+0x2c == 1, no +0x30 write) and clear (the first attach). */
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
saved g_save[128];
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

/* the local human player (kind 0) with a driver, a vehicle, a rider animation and a lens effect object */
uint32_t pick_player(void) {
    const uint32_t g = rd(0x00a946b8u);
    if (!g) return 0;
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t p = rx_world_players_find(g + 0x7460u, i, 0);
        if (!p || !rd(p + 4) || !rd(p + 0x94) || !rd(p + 0x1ac) || !rd(p + 0x1b4)) continue;
        const uint32_t r = rd(p + 0x1b4);
        if (rd(r + 0x5b6dc)) return p;
    }
    return 0;
}

/* the scene's effect set (the particle entity 0xa94264 [scene + 0x2cc4]) and one type record index with flag byte `byte`
 * (-1: none) */
uint32_t effect_set(void) {
    const uint32_t c = rd(0x00a95844u);
    if (!c) return 0;
    const uint32_t sc = rd(c + 0x1cu + 4u * rd(c + 4));
    if (!sc) return 0;
    return rd(0x00a94264u + 4u * rd(sc + 0x2cc4));
}

int index_with_byte(uint32_t ent, int byte) {
    if (!ent) return -1;
    const uint32_t tab = rd(ent + 0xb0);
    if (!tab) return -1;
    const uint32_t types = rd(tab + 0x44);
    if (!types) return -1;
    const int n = (int)rd(types + 0x14);
    for (int i = 0; i < n; i++) if ((rd(rd(types + 0x10) + 0x1f0u * (uint32_t)i + 0x160) & 0xff) == (uint32_t)byte) return i;
    return -1;
}

/* the rider's clamped float object +0x3c4, set to 1.0 (>= the attach threshold [0x9d78c4]) so the state-2 block attaches */
void gesture_ready(uint32_t r) {
    const uint32_t p = rd(r + 0x3c4);
    if (p) set(p, fb(1.0f));
    set(r + 0x5b6d4, 2);
    set(r + 0x5b6e0, 0);
}

void case_run(uint32_t r, const char* name, uint32_t obj, uint32_t mgr, int type, uint32_t attached) {
    const uint32_t ent = effect_set();
    const int idx = type >= 0 ? index_with_byte(ent, type) : -1;
    gesture_ready(r);
    set(obj + 4, mgr);
    if (idx >= 0) set(obj + 0xc, (uint32_t)idx);
    set(obj + 0x2c, attached);
    set(obj + 0x30, 0);
    set(obj + 0x100, 0);
    const uint32_t lcg0 = rd(0x009cb860u), cnt0 = rd(0x009cb864u);
    rx_regs in, out;
    std::memset(&in, 0, sizeof in); std::memset(&out, 0, sizeof out);
    in.r[7] = r;                                       /* EDI = ra */
    const uint32_t a[1] = {fb(1.0f / 60.0f)};          /* stack dt; RET 4 */
    rx_call_original_ex(0x0061eac0u, &in, &out, 1, a);
    std::fprintf(stderr,
                 "[ridep] %s: type %d (byte %d), mgr %08x -> %08x, attached %u -> %u/%u, node %08x, LCG %08x/%u -> %08x/%u, "
                 "state %u\n",
                 name, idx, type, mgr, rd(obj + 4), attached, rd(obj + 0x2c), rd(obj + 0x30), rd(obj + 0x100),
                 lcg0, cnt0, rd(0x009cb860u), rd(0x009cb864u), rd(r + 0x5b6d4));
    restore();
}
}  // namespace

RX_PORT(0x0084ed20, RideParticleProbeHook, "thunk", 0) {
    if (!std::getenv("RX_RIDEP_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RIDEP_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RIDEP_FRAME"))) : 300u;
    ++g_frame;
    if (g_frame != at) return reflex_vm::RUN_ORIGINAL;
    const uint32_t p = pick_player();
    if (!p) {
        std::fprintf(stderr, "[ridep] frame %u: no local human player with a lens effect object\n", g_frame);
        return reflex_vm::RUN_ORIGINAL;
    }
    const uint32_t r = rd(p + 0x1b4), obj = rd(r + 0x5b6dc), ent = effect_set();
    const uint32_t bound = rd(obj + 4), type = rd(obj + 0xc);
    std::fprintf(stderr, "[ridep] frame %u: player %08x rider %08x effect %08x (mgr %08x type %u attached %u node %08x), "
                         "set %08x, clamp float %08x\n",
                 g_frame, p, r, obj, bound, type, rd(obj + 0x2c), rd(obj + 0x100), ent, rd(r + 0x3c4));
    if (!ent) {
        std::fprintf(stderr, "[ridep] no scene effect set: the unbound branch cannot run\n");
        return reflex_vm::RUN_ORIGINAL;
    }
    /* unbound: both type-record flag bytes (the manager choice); bound: attached and first-attach (a real manager of the set) */
    const uint32_t global_mgr = rd(ent + 0xc0), additive_mgr = rd(ent + 0xc4);
    std::fprintf(stderr, "[ridep] set managers: global %08x additive %08x\n", global_mgr, additive_mgr);
    case_run(r, "unbound global", obj, 0, 0, 0);
    case_run(r, "unbound additive", obj, 0, 1, 0);
    if (global_mgr || additive_mgr) {
        const uint32_t mgr = global_mgr ? global_mgr : additive_mgr;
        case_run(r, "bound attached", obj, mgr, -1, 1);
        case_run(r, "bound first attach", obj, mgr, -1, 0);
    }
    return reflex_vm::RUN_ORIGINAL;
}
