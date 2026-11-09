/* rideranim2_probe.cpp -- the oracle's probe for the rider-animation pose setters 00621100 / 006211d0 (development only; the same
 * device as fp_probe.cpp). The five reference worlds never roll a rider slowly or boost for long enough, so 0061f160's ladder
 * never reaches them (0 calls in the Concord world). At a chosen frame the probe takes a live rider animation object (the
 * workers' table 0x00a9408c), builds the branch state the ladder needs, and calls the original 0061f160 itself at the current
 * stack (the port is peeled, so the original reaches the installed 00621100 / 006211d0: Verify then compares the core port with
 * the original per call, memory and registers).
 *   REFLEX_PORT="rx_world,rx_RiderAnim2ProbeHook,-0061f160" REFLEX_PORT_VERIFY=1 RX_RA2_PROBE=1
 *   [RX_RA2_FRAME=<n> (1200): the slow-roll pose variants; n + 1: the crouch-pose variants]
 * The variants cover the branch set of both: the range below 0 / above 0 / 0 / NaN, the wrecked flag (chain fade or not), the
 * two crouch poses by the lean threshold, and both trick states (+0x5b594 1 / not 1). State construction only, no game logic. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace {
uint32_t rd(uint32_t a) { return rx_rd32(a); }
void wr(uint32_t a, uint32_t v) { rx_wr32(a, v); }
uint32_t fb(float f) { uint32_t b; std::memcpy(&b, &f, 4); return b; }
uint32_t g_frame = 0;

/* the first worker slot's live rider animation with the blend outputs and a vehicle (0x00a9408c: the twelve slots) */
uint32_t pick_ra() {
    for (uint32_t i = 0; i < 12; i++) {
        const uint32_t ra = rd(0x00a9408cu + 4u * i);
        if (ra && rd(ra + 0xaf1cu) && rd(ra + 0x598u) && rd(ra + 0x58cu)) return ra;
    }
    return 0;
}
/* hi / lo are the property range (+0x53c / +0x538) or (+0x4c4 / +0x4c0) as bits; lean_thresh only matters for the crouch pose */
void setup(uint32_t ra, bool crouch, uint32_t hi, uint32_t lo, uint32_t wrecked, uint32_t trick594, float lean_thresh) {
    wr(rd(ra + 0x598u), fb(1.0f));   /* both blend outputs at exactly 1: the ladder's first guard */
    wr(rd(ra + 0x58cu), fb(1.0f));
    wr(ra + 0x5babc, 0);             /* not a turn */
    wr(ra + 0x5bb54, crouch ? 1u : 0u);
    wr(ra + 0xaf54, 0);              /* not crouching yet: 006211d0 is the entering branch */
    wr(ra + 0x5bab8, wrecked);
    if (crouch) {
        wr(ra + 0x538u, lo);
        wr(ra + 0x53cu, hi);
        wr(ra + 0xaf68u, 0);         /* boost speed threshold: not above it, so the crouch-pose branch runs */
        wr(ra + 0xaf78u, fb(lean_thresh));
        wr(ra + 0x5b594u, trick594);
        wr(ra + 0x5b590u, 7);
        wr(ra + 0x5b5e8u, 0xdeadbeefu);   /* the 594 == 1 branch must clear both */
        wr(ra + 0x5b5ecu, 0xdeadbeefu);
    } else {
        const uint32_t veh = rd(ra + 0xaf1cu), sim = rd(veh + 0x234u);
        wr(sim + 0x90u, 0);          /* no pose override */
        wr(ra + 0x4c0u, lo);
        wr(ra + 0x4c4u, hi);
        wr(ra + 0x5bb30u, fb(10.0f));   /* inside the slow band (3, [0x935aac] 20) */
        wr(ra + 0x5bb58u, fb(0.99f));   /* the timer passes 1 s with this frame's dt */
        wr(ra + 0x5a8u, 0);          /* pose != the property's id: 00617f00 returns 0 */
        wr(rd(ra + 0x580u), 0xffffu);
    }
}
void run_variant(uint32_t ra, bool crouch, uint32_t hi, uint32_t lo, uint32_t wrecked, uint32_t trick594, float lean_thresh) {
    setup(ra, crouch, hi, lo, wrecked, trick594, lean_thresh);
    rx_regs in; std::memset(&in, 0, sizeof in);
    rx_regs out; std::memset(&out, 0, sizeof out);
    const uint32_t args[2] = {ra, fb(1.0f / 60.0f)};
    rx_call_original_ex(0x0061f160u, &in, &out, 2, args);
    std::fprintf(stderr, "[ra2] %s ra %08x hi %08x lo %08x wrecked %u 594 %u thr %.3g -> pose %u property %u\n",
                 crouch ? "crouch" : "slow", ra, hi, lo, wrecked, rd(ra + 0x5b594u) & 0xffu, (double)lean_thresh,
                 rd(ra + 0x5a8u) & 0xffffu, rd(rd(ra + 0x580u)) & 0xffffu);
}
}  // namespace

RX_PORT(0x0084ed20, RiderAnim2ProbeHook, "thunk", 0) {
    if (!std::getenv("RX_RA2_PROBE")) return reflex_vm::RUN_ORIGINAL;
    static const uint32_t at = std::getenv("RX_RA2_FRAME") ? uint32_t(std::atoi(std::getenv("RX_RA2_FRAME"))) : 1200u;
    ++g_frame;
    if (g_frame != at && g_frame != at + 1) return reflex_vm::RUN_ORIGINAL;
    const bool crouch = g_frame != at;
    const uint32_t ra = pick_ra();
    if (!ra) {
        std::fprintf(stderr, "[ra2] frame %u: no rider animation with a vehicle\n", g_frame);
        return reflex_vm::RUN_ORIGINAL;
    }
    const uint32_t nan = 0x7fc00000u, neg2 = fb(-2.0f), half = fb(0.5f), half2 = fb(0.25f), zero = 0;
    if (!crouch) {
        run_variant(ra, false, neg2, zero, 0, 0, 0.0f);      /* max below 0: -max; chain fade */
        run_variant(ra, false, half, half2, 2, 0, 0.0f);     /* min above 0: -min + max; 5bab8 != 1 and != 0: no fade */
        run_variant(ra, false, zero, zero, 0, 0, 0.0f);      /* both 0: 0 */
        run_variant(ra, false, nan, nan, 0, 0, 0.0f);        /* NaN: the 0 side of both ladders */
    } else {
        run_variant(ra, true, neg2, zero, 0, 1, 1.0e9f);     /* threshold above the lean: pose +0x5a2; 594 == 1 cleared */
        run_variant(ra, true, half, half2, 1, 0, -1.0e9f);   /* threshold below the lean: pose +0x5a4; wrecked: no fade */
        run_variant(ra, true, zero, zero, 0, 2, -1.0e9f);    /* 594 == 2: both words kept */
        run_variant(ra, true, nan, nan, 0, 0, 1.0e9f);       /* NaN range */
    }
    return reflex_vm::RUN_ORIGINAL;
}
