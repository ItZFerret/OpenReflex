/* race_driver_reset.c -- RaceDriver::vf3 0050f090 (a race driver's start / restart; the vtable 0093f8dc slot 3): NetworkDriver::vf3, then the driver's start position
 * (the track's node nearest the vehicle in a session in state 0 of kind 8, else the start slot's position (+0x144) moved back by half the slot's offset times the
 * class's spacing (0x93582c, by the vehicle's class +0x194) and placed with 00460020), the lap / timing fields cleared, the "Hide Arrow" event, the lap and best-lap
 * events of the driver ("Lap Completed" 0053a9e0, "Best Lap" 0053aa40), the per-class lap values (+0x10bc / +0x10c0 from the activity's tables 0xbc4 / 0xe44), the
 * behaviours' and rules' vf6, the pace / navigate behaviours (+0x1114 / +0x1110) and the finish time (+0x1120) reset. The session lock (Quazal 00883d80 / 00883d90,
 * 00884950) is not core: the oracle runs the original, the product nothing (build_activity_driver.c's rule); the networked branches abort naming their address. */
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "build_activity.h"
#include "build_driver_reset.h"
#include "race_driver_reset.h"
#include "race.h"
#include "race_misc.h"
#include "race_activity.h"
#include <string.h>

#include "crt.h"
static uint32_t net_call(rx_addr fn, uint32_t ecx) {
#ifdef RX_ORACLE
    return rx_call_original(fn, RX_THISCALL, ecx, 0, NULL);
#else
    (void)fn; (void)ecx; return 0;
#endif
}
static uint32_t x87_copy32(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }

/* 00508ab0 (EAX = the player): +0x11ac = +0x11b0 = -1, +0x11b4 = 0.0 */
void rx_player_reset_best(rx_addr p) {
    SU32(p, 0x11ac, 0xffffffffu); SU32(p, 0x11b0, 0xffffffffu); SF32(p, 0x11b4, 0.0f);
}
RX_FUNCTION(0x00508ab0, rx_player_reset_best)

/* RaceDriver::vf3 0050f090 (ECX = the driver) */
void rx_race_driver_vf3(rx_addr d) {
    ACT_FRAME_EBP(L, 16, 0x14, 3);
    ACT_CALL(0, rx_network_driver_vf3(d));
    rx_addr P = U32(d, 0x74);
    rx_addr act = U32(P, 0x94);
    if (U32(act, 0x238) == 0 && U32(P, 0x9c) == 8) {
        const rx_addr veh = U32(P, 0x1ac), out = L + 0x10;
        ACT_CALL(3, rx_track_nearest_node_all(act + 0x890, veh + 0x130, 0, veh + 0xc0, out));       /* 00538440 */
        rx_addr sec = U32(out, 0), node = U32(out, 4);
        SU32(d, 0x14, sec); SU32(d, 0x18, node);
        uint32_t nx = U32(node, 0x64);
        SU32(d, 0x1c, nx);
        if (nx == 0) {
            const rx_addr first = U32(U32(sec, 8), 0);
            SU32(d, 0x14, first);
            SU32(d, 0x18, U32(first, 0));
            SU32(d, 0x1c, U32(U32(first, 0), 0x64));
        }
        SU32(d, 0x80, x87_copy32(U32(node, 0))); SU32(d, 0x84, x87_copy32(U32(node, 4))); SU32(d, 0x88, x87_copy32(U32(node, 8)));
        SU32(d, 0x90, U32(d, 0x1c));
    } else {
        const rx_addr slot = U32(d, 0x144);
        float x = F32(slot, 0), y = F32(slot, 4), z = F32(slot, 8);
        SF32(L, 0x10, x); SF32(L, 0x14, y); SF32(L, 0x18, z);
        if (U32(slot, 0x2c) == 0) {
            const float spacing = F32(0, 0x0093582c + 4 * U32(U32(P, 0x1ac), 0x194));
            const float neg0 = -0.0f /* 0x9774a8 */, half = 0.5f /* 0x9322cc */;
            float a = rx_subs(neg0, F32(slot, 0x10)), b = rx_subs(neg0, F32(slot, 0x14)), c = rx_subs(neg0, F32(slot, 0x18));
            a = rx_muls(a, spacing); b = rx_muls(b, spacing); c = rx_muls(c, spacing);
            a = rx_muls(a, half); b = rx_muls(b, half); c = rx_muls(c, half);
            x = rx_adds(a, x); y = rx_adds(b, y); z = rx_adds(c, z);
            SF32(L, 0x10, x); SF32(L, 0x14, y); SF32(L, 0x18, z);
        }
        const rx_addr veh = U32(P, 0x1ac);
        if (veh != 0) {
            ACT_CALL(2, rx_vehicle_effect_query(veh, L + 0x10, slot + 0x10));                          /* 00460020 (ESI = the vehicle) */
            x = F32(L, 0x10); y = F32(L, 0x14); z = F32(L, 0x18);
            SF32(d, 0x3c0, 0.0f); SF32(d, 0x3cc, 0.0f);
        }
        const rx_addr s2 = U32(d, 0x144);
        SU32(d, 0x14, U32(s2, 0x20));
        SU32(d, 0x18, U32(s2, 0x24));
        SU32(d, 0x1c, U32(U32(s2, 0x24), 0x64));
        SF32(d, 0x80, x); SF32(d, 0x84, y); SF32(d, 0x88, z);
        SU32(d, 0x90, U32(d, 0x1c));
    }
    P = U32(d, 0x74);
    SU32(d, 0xf1c, 0); SF32(d, 0xf20, 0.0f);
    SF32(d, 0x10b4, 0.0f); SF32(d, 0x10b8, 0.0f); SF32(d, 0x10b0, 0.0f); SF32(d, 0x10c8, 0.0f);
    SU32(d, 0x10c4, 0x7f7fffffu /* 0x977494 */);
    SF32(d, 0x10e4, 0.0f); SF32(d, 0x10e8, 0.0f); SF32(d, 0x10ec, 0.0f); SF32(d, 0x10d8, 0.0f);
    SU32(d, 0x1124, 0x7f7fffffu);
    SU32(d, 0x310, 0); SU32(d, 0x314, 0);
    SF32(d, 0x10dc, 0.0f);
    SU32(d, 0x10f0, 1);
    act = U32(P, 0x94);
    SU32(d, 0x10f4, U32(act, 0x238) == 3);
    crt_memset(d + 0xf24, 0, 0x18c);
    act = U32(U32(d, 0x74), 0x94);
    { const rx_addr ch = U32(act, 0x290); const uint32_t p[1] = {d}; race_raise_event(ch, 0x0093eef0 /* "Hide Arrow" */, 0x009cda44, p, 1); }
    P = U32(d, 0x74);
    act = U32(P, 0x94);
    const uint32_t st = U32(act, 0x238);
    if (st == 2 || (!(U8(U32(d, 0x10), 0x1b4) & 1) ? 0 : st != 3)) SU32(d, 0xf1c, 0xffffffffu);
    switch (U32(P, 0x10)) {
    case 0:
        if (st == 8) rx_player_reset_best(P);                                                          /* (state 1: nothing) */
        break;
    case 2: {
        rx_addr drv;
        ACT_CALLR(drv, 2, rx_activity_driver_of_kind(act, 0, 0));                                     /* 0055a000 (EAX = the activity) */
        const rx_addr ra = U32(P, 0x1b4);
        if (ra != 0) SU8(ra, 0xa2, 0);
        SU32(d, 0x1118, 0x7f7fffffu);
        SU32(d, 0x24, drv);
        break;
    }
    default: break;
    }
    P = U32(d, 0x74);
    {
        const rx_addr veh = U32(P, 0x1ac), a = U32(P, 0x94);
        const uint32_t cls = U32(veh, 0x194), sub = U32(veh, 0x198);
        const uint32_t i1 = (sub + cls * 4) * 5 + U32(P, 0x8f4);
        SU32(d, 0x10bc, x87_copy32(U32(a, 0xbc4 + 4 * i1)));
        const uint32_t i2 = sub + cls * 4;
        SU32(d, 0x10c0, x87_copy32(U32(a, 0xe44 + 4 * i2)));
        const rx_addr act2 = U32(P, 0x94), mgr = U32(act2, 0x290);
        rx_event_driver_3int(mgr, 0x0093eefc /* "Lap Completed" */, d, U32(d, 0xf1c), U32(act2, 0x238) != 2, U32(act2, 0xff070));
    }
    {
        const float best = F32(d, 0x10c4);
        const float t = (best == F32(0, 0x00977494) ? 0.0f : best);
        const rx_addr mgr = U32(U32(U32(d, 0x74), 0x94), 0x290);
        rx_event_best_lap(mgr, d, t);
    }
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
    if (U32(0, 0x00da6d08) != 0) act_unreached(0x0050f4d2, "the networked session's lock (00402420)");
    const rx_addr lock = 0x00da6cf0;
    if (U8(0, 0x009df580) == 0) net_call(0x00883d80, lock);
    const rx_addr np = U32(d, 0xef0);
    if (np != 0 && (U8(np, 0x20) & 1) && (net_call(0x00884950, np) & 0xff)) act_unreached(0x0050f53b, "the network player's state (0050bad0)");
    ACT_CALL(1, rx_driver_behaviors_vf6(d));
    ACT_CALL(1, rx_driver_rules_vf6(d));
    rx_addr pace, nav;
    ACT_CALLR(pace, 1, rx_driver_behavior_f(d));
    SU32(d, 0x1114, pace);
    ACT_CALLR(nav, 1, rx_driver_navigate_behavior(d));
    SU32(d, 0x1110, nav);
    SU32(d, 0x1120, 0xffffffffu); SU32(d, 0x1128, 0);
    if (U8(0, 0x009df580) == 0) net_call(0x00883d90, lock);
    ACT_END(L);
}
RX_FUNCTION(0x0050f090, rx_race_driver_vf3)

/* RuleOfftrackTimePenalty::vf6 0050ed00 (ECX = the rule): the restart: +8 = +0x10 = 1, the penalty / timer fields cleared (+0x14 .. +0xd8) */
void rx_rule_offtrack_penalty_vf6(rx_addr r) {
    SU32(r, 8, 1); SU32(r, 0x10, 1); SU32(r, 0x14, 0);
    SF32(r, 0x1c, 0.0f); SF32(r, 0x20, 0.0f); SF32(r, 0x54, 0.0f); SF32(r, 0xd0, 0.0f); SF32(r, 0xc, 0.0f); SF32(r, 0x28, 0.0f);
    SU32(r, 0x30, 0); SU32(r, 0x50, 0);
    SF32(r, 0x40, 0.0f); SF32(r, 0x44, 0.0f); SF32(r, 0x48, 0.0f);
    SU32(r, 0x80, 0); SU32(r, 0x84, 0); SU32(r, 0x88, 0);
    SU8(r, 0xd8, 0); SF32(r, 0xd4, 0.0f);
}
RX_FUNCTION(0x0050ed00, rx_rule_offtrack_penalty_vf6)
