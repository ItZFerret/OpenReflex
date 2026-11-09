/* race_misc_pose.c -- the scripted pose overrides: 12 slots (0xad81d0, 0xa50 bytes each: +0x10 / +0x20 / +0x50 the pose matrix
 * rows, +0x960 the animation pose, +0xa38 duration, +0xa3c time, +0xa40 vehicle) that drive a vehicle's body from an animation for
 * a while (Simulation_EndPoseOverride ends one); and the per-frame update of a rider effect list. */
#include "race.h"
#include "rider_cap.h"
#include "race_extern.h"
#include "race_misc.h"
#include "bike_sim.h"
#include "phys_anim_sample.h"
#include "build_rider.h"

#define POSE_SLOTS 0x00ad81d0u
#define POSE_SLOT_SIZE 0xa50u
#define POSE_SLOT_COUNT 12u

static void end_pose_override(rx_addr sim) {               /* Simulation_EndPoseOverride 00434ef0 (EDI = simulation) */
    RACE_CORE_CALL(0, bike_end_pose_override(sim));
}
static void clear_slot(rx_addr slot) {
    rx_addr v = U32(slot, 0xa40);
    if (v != 0) end_pose_override(U32(v, 0x234));
    SU32(slot, 0xa40, 0);
    SF32(slot, 0xa3c, 0.0f);
    SF32(slot, 0xa38, 0.0f);
}

/* 0047c3f0: end every pose override. */
void rx_pose_overrides_clear(void) {
    for (uint32_t i = 0; i < POSE_SLOT_COUNT; i++) clear_slot(POSE_SLOTS + POSE_SLOT_SIZE * i);
}
RX_FUNCTION(0x0047c3f0, rx_pose_overrides_clear)

/* 0047c0b0 (ECX = slot, args: vehicle, animation, keep; RET 0xc): (re)start a slot: end the previous vehicle's override when the
 * slot is cleared, store the vehicle, the animation's length (+0x44's +0x4c, read with the animation's +0x4c raised) and its first
 * pose (FUN_008403b0 on the vehicle's model +0x1328 + 0x10), the simulation's override mode (+0x2de4: 1 keep, 2 end) and start
 * it (simulation vf33). */
void rx_pose_override_start(rx_addr slot, rx_addr vehicle, rx_addr anim, uint32_t keep) {
    if (vehicle == 0 && U32(slot, 0xa40) != 0) end_pose_override(U32(U32(slot, 0xa40), 0x234));
    SU32(slot, 0xa40, vehicle);
    SF32(slot, 0xa3c, 0.0f);
    SF32(slot, 0xa38, 0.0f);
    if (vehicle == 0) return;
    rx_addr clip = U32(anim, 0x44);
    SU32(anim, 0x4c, 1);
    SF32(slot, 0xa38, F32(clip, 0x4c));
    SU32(anim, 0x4c, 0);
    rx_rider_bind_anim_tracks(U32(U32(slot, 0xa40), 0x1328) + 0x10, anim, 0, slot + 0x960);
    SU32(U32(U32(slot, 0xa40), 0x234), 0x2de4, keep != 0 ? 1 : 2);
    rx_addr sim = U32(U32(slot, 0xa40), 0x234);
    race_thiscall(race_vfn(sim, 33), sim, 0, 0, 0, 0, 0);
}
RX_FUNCTION(0x0047c0b0, rx_pose_override_start)

/* 0047c310 (EDX = vehicle, args: animation, keep; cdecl): the vehicle's slot, else a free one, (re)started. */
void rx_pose_override_set(rx_addr vehicle, rx_addr anim, uint32_t keep) {
    for (uint32_t i = 0; i < POSE_SLOT_COUNT; i++)
        if (U32(POSE_SLOTS + POSE_SLOT_SIZE * i, 0xa40) == vehicle) { rx_pose_override_start(POSE_SLOTS + POSE_SLOT_SIZE * i, vehicle, anim, keep); return; }
    for (uint32_t i = 0; i < POSE_SLOT_COUNT; i++)
        if (U32(POSE_SLOTS + POSE_SLOT_SIZE * i, 0xa40) == 0) { rx_pose_override_start(POSE_SLOTS + POSE_SLOT_SIZE * i, vehicle, anim, keep); return; }
}
RX_FUNCTION(0x0047c310, rx_pose_override_set)

/* 0047c190 (ECX = slot, xmm0 = dt): advance a slot: the animation at time / duration (clamped 0..1; FUN_00840440 into the slot's
 * pose) gives the body's override matrix (rows +0x10 / +0x20 / +0x50 -> simulation vf10); past the duration the override ends. */
void rx_pose_override_update(rx_addr slot, float dt) {
    RACE_FRAME_EBP(F, 16, 0x48, 2);   /* the original's frame: the matrix rows at +0x10, the fraction at +0xc */
    rx_addr v = U32(slot, 0xa40);
    if (v == 0) { RACE_FRAME_END(F); return; }
    const float t = dt + F32(slot, 0xa3c), dur = F32(slot, 0xa38);
    SF32(slot, 0xa3c, t);
    if (!(dur >= t)) { end_pose_override(U32(v, 0x234)); RACE_FRAME_END(F); return; }
    for (uint32_t k = 0; k < 16; k += 4) {
        SU32(F + 0x10, k, U32(0, 0xd61670 + k)); SU32(F + 0x20, k, U32(0, 0xd61640 + k)); SU32(F + 0x30, k, U32(0, 0xd61640 + k));
    }
    float f = t / dur;
    if (f >= 1.0f /* 0x931f24 */) f = 1.0f;
    else if (!(0.0f < f) && f == f) f = 0.0f;
    SF32(F, 0xc, f);
    RACE_CORE_CALL(3, (void)phys_pose_sample(slot + 0x960, f, slot));
    for (uint32_t k = 0; k < 16; k += 4) {
        SU32(F + 0x10, k, U32(slot, 0x10 + k)); SU32(F + 0x20, k, U32(slot, 0x20 + k)); SU32(F + 0x30, k, U32(slot, 0x50 + k));
    }
    rx_addr sim = U32(U32(slot, 0xa40), 0x234);
    race_thiscall(race_vfn(sim, 10), sim, 1, F + 0x10, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0047c190, rx_pose_override_update)

/* 0047c2a0 (arg = dt; cdecl): every active slot advanced; finished ones cleared. */
void rx_pose_overrides_update(float dt) {
    RACE_FRAME_EBP(F, 16, 8, 2);
    for (uint32_t i = 0; i < POSE_SLOT_COUNT; i++) {
        rx_addr slot = POSE_SLOTS + POSE_SLOT_SIZE * i;
        if (U32(slot, 0xa40) == 0) continue;
        RACE_CORE_CALL(0, rx_pose_override_update(slot, dt));
        if (F32(slot, 0xa3c) > F32(slot, 0xa38)) clear_slot(slot);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0047c2a0, rx_pose_overrides_update)

/* 0047bc60 (args: effect list, dt; RET 8): while the view set's track (game +0x5a54 / +0x20c8) is not paused (+0x20014), each of the
 * 12 active entries (+0x10 + 0x410 * i, active +0x3fc) updated (FUN_00479970, ESI = entry). */
void rx_effect_list_update(rx_addr list, float dt) {
    rx_addr ctx = U32(0, 0xa95844), track;
    if (U32(U32(ctx, 0x20), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x5a54);
    else if (U32(U32(ctx, 0x1c), 0x34) == 1) track = U32(U32(0, 0xa946b8), 0x20c8);
    else return;
    if (track == 0 || U32(track, 0x20014) != 0) return;
    for (uint32_t i = 0; i < rx_dmg_n(); i++) {   /* (w-rc-core) + the side entries made */
        rx_addr e = rx_dmg_entry(list, i);
        if (U32(e, 0x3fc) == 0) continue;
        RACE_CORE_CALL(1, rx_damage_entry_update(e, dt));      /* 00479970 (ESI = the entry; stack dt): race_misc_damage.c */
    }
}
RX_FUNCTION(0x0047bc60, rx_effect_list_update)
