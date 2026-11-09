/* race_freestyle_rules.c -- the rule and the behaviours MXActivityFreestyleStub::vf47 gives a Freestyle driver that no other mode
 * uses: RuleInFreestyleCrowd (vtable 0x0093fc04: the rider who leaves the arena into the crowd is put back at his start pose),
 * BehaviorRoamTrack (0x0093fb6c), BehaviorGenericBrake (0x009401f8) and the remaining virtuals of the shared ones. The behaviours'
 * per-frame vf4 / vf8 are AI-only: a Freestyle event has no AI riders without the AI debug override 0xae1b90
 * (race_freestyle_driver.c), so those are not ported. Adapters in core/oracle/race_freestyle.cpp (group rx_race). */
#include "race_freestyle_rules.h"
#include "race.h"
#include "race_extern.h"
#include "build_driver_reset.h"
#include <string.h>

/* 006a6e90 (EAX = the arena's crowd region, ECX = the point) -> EAX: 1 when the point (x, z: +0 / +8) lies inside one of the region's
 * triangles (+0x1cc, count +0x1c8; 0x30 bytes each: the corners (x, z) at +0 / +4, +8 / +0xc, +0x10 / +0x14): the three edge
 * functions have the same sign (> 0) */
uint32_t rx_crowd_region_contains(rx_addr region, rx_addr point) {
    const uint32_t n = U32(region, 0x1c8);
    if (n == 0) return 0;
    const float px = F32(point, 0);
    const float pz = F32(point, 8);
    rx_addr t = U32(region, 0x1cc);
    for (uint32_t i = 0; i < n; i++, t += 0x30) {
        const float ax = F32(t, 0), az = F32(t, 4);
        const float bx = F32(t, 8), bz = F32(t, 0xc);
        const float cx = F32(t, 0x10), cz = F32(t, 0x14);
        const float e0 = (bz - az) * (px - ax) - (pz - az) * (bx - ax);
        const float e1 = (cz - bz) * (px - bx) - (pz - bz) * (cx - bx);
        const float e2 = (az - cz) * (px - cx) - (pz - cz) * (ax - cx);
        const int s0 = e0 > 0.0f;
        const int s1 = e1 > 0.0f;
        const int s2 = e2 > 0.0f;
        if (s0 == s1 && s1 == s2) return 1;
    }
    return 0;
}
RX_FUNCTION(0x006a6e90, rx_crowd_region_contains)

/* 0051bc50 RuleInFreestyleCrowd::vf1 (ECX = rule; driver; RET 4): bound to the driver (+4), enabled (+8 = 1), not triggered (+0xc) */
void rx_rule_in_crowd_vf1(rx_addr rule, rx_addr driver) {
    SU32(rule, 4, driver);
    SU32(rule, 8, 1);
    SU32(rule, 0xc, 0);
}
RX_FUNCTION(0x0051bc50, rx_rule_in_crowd_vf1)

/* 0051bc70 RuleInFreestyleCrowd::vf5 (ECX = rule; dt; RET 4): an enabled rule of a driver with a vehicle, in an activity whose world
 * has an arena (+0x21df0) with a crowd region (+0x9c): the vehicle's position (vf17) inside the crowd (006a6e90) triggers the rule
 * (+0xc = 1) and the driver's rule event (vf9: FreestyleDriver::vf9 puts the vehicle back). Frame: push ebp; and esp, -16; sub
 * 0x18; push esi / edi (the position at +0x10). */
void rx_rule_in_crowd_vf5(rx_addr rule, float dt) {
    (void)dt;
    RACE_ORIG_FRAME_ENTER(F, 0x18, 2);
    const rx_addr d = U32(rule, 4);
    const rx_addr p = U32(d, 0x74);
    if (U32(p, 0x1ac) == 0 || U32(rule, 8) == 0 || U32(p, 0x94) == 0 || U32(U32(U32(p, 0x94), 0x234), 0x21df0) == 0) {
        RACE_ORIG_FRAME_LEAVE(F);
        return;
    }
    const rx_addr v = U32(U32(d, 0x74), 0x1ac);
    race_thiscall(race_vfn(v, 17), v, 1, F + 0x10, 0, 0, 0);       /* the vehicle's position */
    const rx_addr driver = U32(rule, 4);
    const rx_addr region = U32(U32(U32(U32(U32(driver, 0x74), 0x94), 0x234), 0x21df0), 0x9c);
    if (region != 0) {
        uint32_t inside;
        RACE_CORE_CALL(0, inside = rx_crowd_region_contains(region, F + 0x10));
        if (inside != 0) {
            SU32(rule, 0xc, 1);
            race_thiscall(race_vfn(driver, 9), driver, 1, rule, 0, 0, 0);
        }
    }
    RACE_ORIG_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0051bc70, rx_rule_in_crowd_vf5)

/* 00519970 RuleBackwards::vf4 (also RuleInFreestyleCrowd's; ECX = rule; enabled; RET 4): +8 = enabled; disabled clears the trigger
 * (+0xc) */
void rx_rule_set_enabled(rx_addr rule, uint32_t enabled) {
    SU32(rule, 8, enabled);
    if (enabled == 0) SU32(rule, 0xc, 0);
}
RX_FUNCTION(0x00519970, rx_rule_set_enabled)

/* 0052b620 BehaviorRoamTrack::vf3 (ECX = behaviour): the driver's (+8) track behaviour (0053a4c0) kept in +0xcc. Frame: push esi. */
void rx_roam_track_vf3(rx_addr b) {
    RACE_FRAME_PLAIN(F, 0, 1);
    rx_addr t;
    RACE_CORE_CALL(1, t = rx_driver_behavior_f(U32(b, 8)));
    SU32(b, 0xcc, t);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0052b620, rx_roam_track_vf3)

/* 0052b640 BehaviorRoamTrack::vf6 (ECX = behaviour): the reset: +0x3c / +0x40 = 0, +0xc / +0x10 / +0x30 = 0, the previous mode +0x34 =
 * the mode +0x38, +0x38 = 0; a mode-1 roam's track behaviour (+0xcc) released (+0x44 = 1, +0x54 = 0, +0x58 = -1, +0x48 = 1); the
 * waypoints +0x44 (0x80 bytes) cleared, +0xc8 / +0xd0 .. +0xe0 / +0xc4 = 0, +0xe4 = 0. Frame: push esi / edi. */
void rx_roam_track_vf6(rx_addr b) {
    const uint32_t mode = U32(b, 0x38);
    SF32(b, 0x40, 0.0f);
    SF32(b, 0x3c, 0.0f);
    SU32(b, 0xc, 0);
    SU32(b, 0x10, 0);
    SU32(b, 0x30, 0);
    SU32(b, 0x34, mode);
    SU32(b, 0x38, 0);
    if (mode == 1) {
        const rx_addr t = U32(b, 0xcc);
        if (t != 0) {
            SU32(t, 0x44, 1);
            SF32(t, 0x54, 0.0f);
            SU32(t, 0x58, 0xffffffffu);
            SU32(U32(b, 0xcc), 0x48, 1);
        }
    }
    for (uint32_t i = 0; i < 0x80; i++) SU8(b + 0x44, i, 0);
    SU32(b, 0xc8, 0);
    for (uint32_t o = 0xd0; o < 0xe0; o += 4) SU32(b, o, 0);
    SU32(b, 0xe0, 0);
    SU32(b, 0xc4, 0);
    SF32(b, 0xe4, 0.0f);
}
RX_FUNCTION(0x0052b640, rx_roam_track_vf6)

/* 00530fd0 (BehaviorGenericBrake / BehaviorLeaveActivity vf7; ECX = behaviour; enabled; RET 4): +4 = enabled, then the behaviour's
 * reset (vf6), a tail call */
void rx_behavior_enable_reset(rx_addr b, uint32_t enabled) {
    SU32(b, 4, enabled);
    race_thiscall(race_vfn(b, 6), b, 0, 0, 0, 0, 0);
}
RX_FUNCTION(0x00530fd0, rx_behavior_enable_reset)

/* 005223c0 BehaviorAvoidWreck::vf7 (ECX = behaviour; enabled; RET 4): +4 = enabled, its state cleared (+0xc = 0.0, +0x14 / +0x18 = 0) */
void rx_avoid_wreck_vf7(rx_addr b, uint32_t enabled) {
    SU32(b, 4, enabled);
    SF32(b, 0xc, 0.0f);
    SU32(b, 0x18, 0);
    SU32(b, 0x14, 0);
}
RX_FUNCTION(0x005223c0, rx_avoid_wreck_vf7)
