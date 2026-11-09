/* anim_scenario_rider.c -- the rider animation object's small entry points (0x0061xxxx / 0x0062xxxx): the CharacterEntity /
 * AnimationEntity / RiderCharacter virtuals, the worker-job kick / wait, the state resets of the rider animation (RA, the object
 * the animation worker works on), the RiderCharacter teardown, and the rider's sorted std::list helpers.
 *
 * CharacterEntity E: +0x148 character animation state (anim_character.c), +0x180 / +0x1c0 node sets (posed / bind), +0x1f4
 * looping slot, +0x210 root node record. Rider animation RA (EDI / ESI / EAX): +0x144 RiderCharacter, +0x46c / +0x48c two float
 * listener sets, +0x564..+0x598 the motion switch outputs (byte flag, u16 index, three clamped weights with their bounds),
 * +0x59c switch animator, +0x5bc / +0x5c4 sub objects, +0xaee0 stance mode, +0xaf08 "frozen", +0xc930 pose override active,
 * +0x5b578 frozen flag, +0x5b590 / +0x5b594 state now / before. Worker tables (0xa94080 jobs in flight, per rider 0xa9408c
 * riders, 0xa940bc / 0xa940ec "go" events). (core/notes/anim.md) */
#include "build_rider_misc.h"
#include "build_activity_util.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "../src/rx_os.h"
#include "crt.h"
#include "anim.h"
#include "anim_extern.h"
#include "anim_rider_a.h"
#include "anim_ik.h"
#include "loop_platform.h"
#include "loop_jobs.h"
#include "loop_vehicle.h"
#include "race_activity.h"
#include "race_misc.h"
#include "bike_sim.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "rider_cap.h"

/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr rider_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* ---- the CharacterEntity / AnimationEntity / RiderCharacter virtuals ---- */

/* 00617e10 AnimationEntity::vf29: the posed node set (this + 0x180). */
rx_addr rx_animation_entity_nodes(rx_addr e) { return e + 0x180; }
RX_FUNCTION(0x00617e10, rx_animation_entity_nodes)
/* 006180c0 CharacterEntity::vf28: the root node record slot (this + 0x210). */
rx_addr rx_character_entity_root_slot(rx_addr e) { return e + 0x210; }
RX_FUNCTION(0x006180c0, rx_character_entity_root_slot)
/* 006180d0 RiderCharacter::vf22: the entity type 0x1081. */
uint32_t rx_rider_character_type(rx_addr e) { (void)e; return 0x1081; }
RX_FUNCTION(0x006180d0, rx_rider_character_type)

static rx_addr root_local_matrix(rx_addr e) {
    rx_addr rec = U32(e, 0x210);
    return U32(U32(rec, 0), 0x18) + 0x40u * U8(rec, 0xb);
}
/* 00617fd0 CharacterEntity::vf14: the root node's local matrix. */
rx_addr rx_character_entity_root_matrix(rx_addr e) { return root_local_matrix(e); }
RX_FUNCTION(0x00617fd0, rx_character_entity_root_matrix)
/* 00617ff0 CharacterEntity::vf15 (out; RET 4): copies the root's local matrix (64 bytes) out. */
void rx_character_entity_get_root_matrix(rx_addr e, rx_addr out) {
    rx_addr m = root_local_matrix(e);
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(out, i, U32(m, i));
}
RX_FUNCTION(0x00617ff0, rx_character_entity_get_root_matrix)
/* 00618020 CharacterEntity::vf17 (out; RET 4): the root's local translation (x87 copies). */
void rx_character_entity_get_root_position(rx_addr e, rx_addr out) {
    rx_addr m = root_local_matrix(e);
    anim_x87_copy_f32(out, m + 0x30); anim_x87_copy_f32(out + 4, m + 0x34); anim_x87_copy_f32(out + 8, m + 0x38);
}
RX_FUNCTION(0x00618020, rx_character_entity_get_root_position)
/* 00618070 CharacterEntity::vf16 (matrix; RET 4): sets the root's local matrix and invalidates the node. */
void rx_character_entity_set_root_matrix(rx_addr e, rx_addr m) {
    rx_addr rec = U32(e, 0x210);
    rx_v4 r0 = rx_ld4(m), r1 = rx_ld4(m + 0x10), r2 = rx_ld4(m + 0x20), r3 = rx_ld4(m + 0x30);
    rx_addr d = U32(U32(rec, 0), 0x18) + 0x40u * U8(rec, 0xb);
    rx_st4(d, r0); rx_st4(d + 0x10, r1); rx_st4(d + 0x20, r2); rx_st4(d + 0x30, r3);
    anim_node_invalidate(rec);
}
RX_FUNCTION(0x00618070, rx_character_entity_set_root_matrix)
/* 00618100 CharacterEntity::vf14_o90: the bind node set (+0x1c0) copied into the posed one (+0x180). */
void rx_character_entity_reset_nodes(rx_addr e) {
    ANIM_CALL_FRAME(4); rx_nodeset_copy_from(e + 0x1c0, e + 0x180); ANIM_CALL_FRAME_END(4);
}
RX_FUNCTION(0x00618100, rx_character_entity_reset_nodes)

/* 00624ad0 RiderCharacter::vf19 (dt; RET 4): unless the game is paused (context +0xc), a looping slot's motion is marked looping
 * (+0x54 of its data, in use around it); then CharacterEntity::vf19. */
void rx_rider_character_animate(rx_addr e, float dt) {
    if (U32(U32(U32(0, 0x00a95844), 0x10), 0xc) != 0) return;
    rx_addr slot = U32(e, 0x1f4);
    if (slot && U32(slot, 0)) {
        rx_addr h = U32(slot, 0);
        SU32(h, 0x4c, 1);
        SU32(U32(h, 0x44), 0x54, 1);
        SU32(U32(U32(e, 0x1f4), 0), 0x4c, 0);
    }
    ANIM_CALL_FRAME(4); rx_character_entity_animate(e, dt); ANIM_CALL_FRAME_END(4);
}
RX_FUNCTION(0x00624ad0, rx_rider_character_animate)

/* 00616f30 (EAX = object, stack float; RET 4): the float handed to the vf7 (+0x1c) of the +0x28 member of the 4 objects at +4. */
void rx_rider_parts_set_float(rx_addr o, float x) {
    for (uint32_t i = 0; i < 4; i++) {
        rx_addr c = U32(U32(o, 4 + 4 * i), 0x28);
        const uint32_t a = rx_fb(x);
        const rx_addr target = U32(U32(c, 0), 0x1c);
        void (*set_float)(rx_addr,float) = RX_FN(target,void (*)(rx_addr,float));
        if(set_float) { const rx_addr site=rider_callsite(1,&a);set_float(c,x);rx_set_sp(site); }
        else rx_call_original(target,RX_THISCALL,c,1,&a);
    }
}
RX_FUNCTION(0x00616f30, rx_rider_parts_set_float)

/* ---- the animation worker jobs ---- */

/* One pass of a rider animation worker (the work between the waits of 0061cdb0 RiderAnimation_WorkerThread, rx_loop's thread
 * entry): with the exit event (0xaf6b50) not set and the rider's job pending (+0x5bb5c): the skeleton update (0082ff20), the head
 * node's world matrix (node +0xaeec of the hierarchy +0x2b4, locked +0x4c meanwhile) gives the wobble axes, the noise wobble
 * (00623c80) unless the rider is frozen (+0x5b578) or posed (+0x5b1fc), the nod clock (+0x5b514 += +0x5b538, 00623f30 when it
 * reaches +0x5b518), then the job count (0xa94080) drops under its lock (0xaf6c98). The exit event set instead: the thread's
 * "exited" event (0xaf6b80). The product build runs it in place of the worker thread (loop_wake_worker); the oracle's emulated
 * worker runs the original. */
static void rider_animation_worker_pass(rx_addr slot) {
    if (loop_wait_event_timeout(U32(rx_rc_tab(0x00af6b50, 4, slot), 0), 0) != 0x102) {
        if (!loop_set_event(U32(rx_rc_tab(0x00af6b80, 4, slot), 0))) loop_get_last_error();
        return;
    }
    const rx_addr r = U32(rx_rc_tab(0x00a9408c, 4, slot), 0);
    if (U32(r, 0x5bb5c) == 0) return;
    ANIM_FRAME_ENTER(A, 0xa0);                                 /* the worker's frame: sub esp, 0x94; push ebx, esi, edi */
    const rx_addr base = A - 0xa0, axis2 = base + 0x20, axis0 = base + 0x30, axis1 = base + 0x40, m = base + 0x50;
    SU32(base, 0x14, r);
    { const rx_addr sp0 = rider_callsite(0, 0); rx_skeleton_animate(base + 0x90, U32(r, 0x144)); rx_set_sp(sp0); }
    float amount = rx_muls(F32(r, 0x5bb30), F32(0, 0x00977abc));
    if (amount > 1.0f /* 0x931f24 */) amount = 1.0f;
    amount = rx_muls(amount, F32(0, 0x009c4b4c));
    SF32(base, 0x18, amount);
    rx_addr h = U32(r, 0x2b4);
    SU32J(h, 0x4c, 1);                                         /* (the riders share the hierarchy's handle: SU32J, loop_jobs.h) */
    rx_addr node = U32(U32(h, 0x44), 0x18) + 16u * U32(r, 0xaeec);
    anim_node_evaluate_world(node, 0, 0x0061cea0);
    rx_addr w = U32(U32(node, 0), 0x1c) + 64u * U8(node, 0xb);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(m, k, U32(w, k));
    SF32(axis1, 0, rx_muls(F32(m, 0x20), -1.0f)); SF32(axis1, 4, rx_muls(F32(m, 0x24), -1.0f));
    SU32(axis2, 0, U32(m, 0x10)); SU32(axis2, 4, U32(m, 0x14)); SU32(axis2, 8, U32(m, 0x1c));
    SU32(axis0, 0, U32(m, 0x00)); SU32(axis0, 4, U32(m, 0x04)); SU32(axis0, 8, U32(m, 0x0c));
    SU32J(U32(r, 0x2b4), 0x4c, 0);
    float rate = rx_muls(rx_muls(F32(r, 0x5bb30), F32(0, 0x00977ab8)), F32(0, 0x009322c0));
    SF32(axis1, 8, rx_muls(F32(m, 0x28), -1.0f));
    SF32(base, 0x1c, rate);
    if (U8(r, 0x5b578) == 0 && U32(r, 0x5b1fc) == 0) {
        rx_addr q = base - 0x14;                                /* the fld / fstp copies of its float arguments */
        anim_x87_copy_f32(q + 8, base + 0x18); anim_x87_copy_f32(q + 4, base + 0x1c); anim_x87_copy_f32(q, r + 0x5b538);
        ANIM_CALL_FRAME(0x14);
        rx_rider_anim_noise_wobble(r, axis0, F32(q, 0), F32(q, 4), F32(q, 8), axis1, axis2);
        ANIM_CALL_FRAME_END(0x14);
    }
    float t = rx_adds(F32(r, 0x5b514), F32(r, 0x5b538));
    SF32(r, 0x5b514, t);
    if (t >= F32(r, 0x5b518)) { ANIM_CALL_FRAME(4); rx_rider_anim_nod(r); ANIM_CALL_FRAME_END(4); }
    loop_enter_cs(0x00af6c98);
    SU32(0, 0x00a94080, U32(0, 0x00a94080) - 1);
    loop_leave_cs(0x00af6c98);
    ANIM_FRAME_LEAVE(A);
}

/* 0061d060 RiderAnimation_KickJobs: when jobs are pending (0xa94080 > 0), wakes the worker of every rider that is not in state 3
 * (rider +0x2c0), then those in state 3 ("go" events 0xa940bc), then the second event set (0xa940ec); a failing SetEvent asks
 * GetLastError. Clears 0xa9417c and 0xa94180. (w-rc-core) Slots 12..31 (rider_cap.h side storage) after the original's 12. Oracle: SetEvent, the emulated worker runs inside it; product: the worker's pass
 * runs in place (loop_wake_worker), in the same order. */
void rx_rider_animation_kick_jobs(void) {
    if (I32(0, 0x00a94080) <= 0) return;
#ifndef RX_ORACLE
    if (rx_jobs_on(RX_JOB_RIDERS)) {   /* w-jobthreads: each pass as its own rider thread (loop_jobs.h), in parallel, in the same order */
        rx_job jobs[RX_RC_MAX];
        int n = 0;
        for (int pass = 0; pass < 2; pass++)
            for (uint32_t i = 0; i < RX_RC_MAX; i++) {
                rx_addr rider = U32(rx_rc_tab(0x00a9408c, 4, i), 0);
                if (rider == 0 || (U32(rider, 0x2c0) == 3) != (pass == 1)) continue;
                jobs[n].body = rider_animation_worker_pass; jobs[n].arg = i; jobs[n].ctx = i < RX_RC_ORIG ? RX_JOB_CTX_RIDER + (int)i : RX_JOB_CTX_RIDER2 + (int)(i - RX_RC_ORIG);
                jobs[n].tls = loop_worker_tls(U32(U32(rx_rc_tab(0x00a940bc, 4, i), 0), 0));
                n++;
            }
        rx_jobs_run(jobs, n);
    } else
#endif
    for (int pass = 0; pass < 2; pass++)
        for (uint32_t i = 0; i < RX_RC_MAX; i++) {
            rx_addr rider = U32(rx_rc_tab(0x00a9408c, 4, i), 0);
            if (rider == 0 || (U32(rider, 0x2c0) == 3) != (pass == 1)) continue;
            loop_wake_worker(U32(U32(rx_rc_tab(0x00a940bc, 4, i), 0), 0), rider_animation_worker_pass, i);
        }
    SU32(0, 0x00a9417c, 0);
    for (uint32_t i = 0; i < RX_RC_MAX; i++) {
        if (U32(rx_rc_tab(0x00a9408c, 4, i), 0) == 0 || U32(rx_rc_tab(0x00a940ec, 4, i), 0) == 0) continue;
        if (!loop_set_event(U32(U32(rx_rc_tab(0x00a940ec, 4, i), 0), 0))) loop_get_last_error();   /* (no thread waits on these) */
    }
    SU32(0, 0x00a94180, 0);
}
RX_FUNCTION(0x0061d060, rx_rider_animation_kick_jobs)

/* 0061d130 RiderAnimation_WaitJobs: Sleep(0) until no job is in flight, then clears the count. */
void rx_rider_animation_wait_jobs(void) {
    if (I32(0, 0x00a94080) > 0) {
        do RX_OS(0x00916020, 0); while (I32(0, 0x00a94080) > 0);   /* Sleep(0) (product: the jobs ran synchronously, count 0) */
    }
    SU32(0, 0x00a94080, 0);
}
RX_FUNCTION(0x0061d130, rx_rider_animation_wait_jobs)

/* ---- the rider animation's state resets ---- */

/* 0061c5f0 RiderAnimation_ResetStunt (EAX = RA): state 4 (+0x5b590 / +0x5b594), the stunt blend timers and weights cleared, the
 * blend rate +0x5b5a8 = 0.01. */
void rx_rider_animation_reset_stunt(rx_addr ra) {
    SU32(ra, 0x5b590, 4); SU32(ra, 0x5b594, 4);
    SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0);
    SF32(ra, 0x5b5f0, 0.0f); SF32(ra, 0x5b5f4, 0.0f); SF32(ra, 0x5b5f8, 0.0f); SF32(ra, 0x5b5d8, 0.0f);
    SF32(ra, 0x5b5a8, 0.01f /* 0x9322ac */);
    SU32(ra, 0x5b760, 0);
    SF32(ra, 0x5b790, 0.0f); SF32(ra, 0x5b794, 0.0f); SF32(ra, 0x5b798, 0.0f); SF32(ra, 0x5b7a0, 0.0f);
    SU32(ra, 0x5b850, 0);
    SF32(ra, 0x5b880, 0.0f); SF32(ra, 0x5b884, 0.0f); SF32(ra, 0x5b888, 0.0f); SF32(ra, 0x5b890, 0.0f);
}
RX_FUNCTION(0x0061c5f0, rx_rider_animation_reset_stunt)

/* min(v, hi) then max(., lo), as the original's comiss pairs do it (a NaN takes hi). */
static float clamp_hi_lo(float v, float hi, float lo) {
    if (!(hi >= v)) v = hi;
    if (lo >= v) v = lo;
    return v;
}
/* A weight output: its value (+o) clamped to [lo +o-4, hi], written through the pointer +o+4 when there is one. */
static void weight_out(rx_addr ra, uint32_t o, float hi) {
    float v = clamp_hi_lo(F32(ra, o), hi, F32(ra, o - 4));
    if (U32(ra, o + 4)) SF32(U32(ra, o + 4), 0, v);
}

/* 006215e0 RiderAnimation_SetMotionOutputs (EAX = RA): the switch flag (+0x564 -> 1), the motion index (+0x568 <- u16 +0x5a0) and
 * the weight +0x588 clamped to [+0x584, 0] into *(+0x58c). */
void rx_rider_animation_set_motion_outputs(rx_addr ra) {
    if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
    if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, U16(ra, 0x5a0));
    weight_out(ra, 0x588, 0.0f);
}
RX_FUNCTION(0x006215e0, rx_rider_animation_set_motion_outputs)

/* 00621640 (EAX = rider animation): stance 2's two bounded motion outputs and
 * three controller parameters. COMISS/JB leaves the chosen value unchanged on an
 * unordered comparison; preserve that order instead of using generic min/max. */
void rx_rider_animation_stance2_outputs(rx_addr ra) {
    float x=0.0f,y=0.0f;
    uint32_t p=0,q=0,on=0;
    if(U32(ra,0x5bab8)==0) {
        x=0.5f /* 0x9322cc */;y=F32(0,0x009d78b0);
        q=U32(0,0x00d94ab0);p=0x3f000000u;on=0x3f800000u;
    } else if(U32(ra,0x5bab8)==1) {
        x=0.4f /* 0x935860 */;y=F32(0,0x009d78a0);
        q=0x3ecccccdu;p=0x3f000000u;on=0x3f800000u;
    }
    if(x>=F32(ra,0x3f0))x=F32(ra,0x3f0);
    if(F32(ra,0x3ec)>=x)x=F32(ra,0x3ec);
    if(U32(ra,0x3f4))SF32(U32(ra,0x3f4),0,x);
    if(y>=F32(ra,0x3fc))y=F32(ra,0x3fc);
    if(F32(ra,0x3f8)>=y)y=F32(ra,0x3f8);
    if(U32(ra,0x400))SF32(U32(ra,0x400),0,y);
    SU32(ra,0x3b4,p);SU32(ra,0x304,q);SU32(ra,0x35c,on);
}
RX_FUNCTION(0x00621640, rx_rider_animation_stance2_outputs)

static void ra_call_ecx(rx_addr fn, rx_addr ra) { rx_call_original(fn, RX_THISCALL, ra, 0, 0); }
/* 006237a0 (not in the list): ECX = RA, EAX = mode, ESI = 0, stack blend time; RET 4. */
static void ra_set_blend(rx_addr ra, uint32_t mode, float t) {
    const uint32_t a = rx_fb(t);
    const rx_addr sp0 = rider_callsite(1, &a);
    rx_world_rider_chains_fade_in(ra, mode, 0, t);
    rx_set_sp(sp0);
}

/* 00621720 RiderAnimation_SetStance (EDI = RA, stack stance; RET 4): unless frozen (+0xaf08): leaving stance 1 starts blend 5
 * over 0.25; then the stance's entry (0: 006215e0, 1: 00621570, 2: 00621640) and +0xaee0 = the stance (any other value: only
 * stored). */
void rx_rider_animation_set_stance(rx_addr ra, uint32_t stance) {
    if (U32(ra, 0xaf08) != 0) return;
    if (U32(ra, 0xaee0) == 1) ra_set_blend(ra, 5, 0.25f /* 0x92ff84 */);
    if (stance == 0) { ANIM_CALL_FRAME(0); rx_rider_animation_set_motion_outputs(ra); ANIM_CALL_FRAME_END(0); }
    else if (stance == 1) ra_call_ecx(0x00621570, ra);
    else if (stance == 2) { ANIM_CALL_FRAME(0); rx_rider_animation_stance2_outputs(ra); ANIM_CALL_FRAME_END(0); }
    SU32(ra, 0xaee0, stance);
}
RX_FUNCTION(0x00621720, rx_rider_animation_set_stance)

/* 00434ef0 Simulation_EndPoseOverride (EDI = simulation). */
static void end_pose_override(rx_addr sim) {
    const rx_addr sp0 = rider_callsite(0, 0);
    bike_end_pose_override(sim);
    rx_set_sp(sp0);
}
/* The "stop" sequence of 00621d70 / 00623500: the pose override released, a state-1 stunt clears its timers, state -> 4 with the
 * previous one kept, the blend rate +0x5b640 = 1.5. */
static void ra_leave_state(rx_addr ra) {
    if (U32(ra, 0xc930) != 0) {
        rx_addr sim = U32(U32(ra, 0xaf1c), 0x234);
        SU32(ra, 0xc930, 0);
        end_pose_override(sim);
    }
    if (U32(ra, 0x5b594) == 1) { SU32(ra, 0x5b5e8, 0); SU32(ra, 0x5b5ec, 0); }
    uint32_t now = U32(ra, 0x5b590);
    SF32(ra, 0x5b640, 1.5f /* 0x9322a4 */);
    SU32(ra, 0x5b594, now);
    SU32(ra, 0x5b590, 4);
}

/* x87 round trip of a float (fld m32 / fstp m32 from a register value): an SNaN comes back quiet. */
static uint32_t x87_f32_bits(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu)) b |= 0x00400000u;
    return b;
}

/* 00621d70 RiderAnimation_StopAnimation (stdcall: RA; RET 4): when an animation plays (+0xc824): its end (00620fc0 with the
 * listener value +0x46c and the clip's +0x24), the switch back to the base motion (unless its weight is already 1: then the
 * outputs are just re-asserted), then the stop sequence. */
void rx_rider_animation_stop(rx_addr ra) {
    if (U32(ra, 0xc824) == 0) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x10);                                   /* push ebx, ebp, esi, edi */
    const uint32_t list = ra + 0x46c;
    rx_addr sp = rider_callsite(1, &list);
    const float lv = rx_world_float_listeners_get(list);
    rx_set_sp(sp);
    const uint32_t t1b = x87_f32_bits(rx_fb(lv)), t2b = x87_f32_bits(U32(U32(ra, 0xc820), 0x24));
    const uint32_t a[4] = {ra, U32(ra, 0xc824), t1b, t2b};
    sp = rider_callsite(4, a);
    rx_world_rider_play_clip_events(ra, U32(ra, 0xc824), rx_bf(t1b), rx_bf(t2b));
    rx_set_sp(sp);
    SU32(ra, 0x5b57c, 0xc); SU32(ra, 0x5b580, 3); SU32(ra, 0x5b6d0, 0);
    rx_addr wp = U32(ra, 0x58c);
    if (wp == 0 || !(F32(wp, 0) == 1.0f)) {
        SU16(U32(ra, 0x59c), 0xd0, U32(ra, 0x568) ? U16(U32(ra, 0x568), 0) : 0);
        SU16(U32(ra, 0x59c), 0xd2, U16(ra, 0x5ac));
        wp = U32(ra, 0x58c);
        float v = 1.0f - (wp ? F32(wp, 0) : 0.0f);
        if (v >= F32(ra, 0x588)) v = F32(ra, 0x588);
        if (F32(ra, 0x584) >= v) v = F32(ra, 0x584);
        if (wp) SF32(wp, 0, v);
        ANIM_CALL_FRAME(0); rx_switch_animator_start_transition(U32(ra, 0x59c)); ANIM_CALL_FRAME_END(0);
    } else {
        if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
        if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
        weight_out(ra, 0x588, 0.0f);
    }
    SU32(ra, 0x5b584, 0xc); SU32(ra, 0x5b588, 0);
    ra_leave_state(ra);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00621d70, rx_rider_animation_stop)

/* 006b04d0 (rx_anim: cluster ik): ECX = the RA's +0x5bc controller; resets it. */
static void ra_reset_controller(rx_addr c) {
    const rx_addr sp0 = rider_callsite(0, 0);
    rx_ik_animator_engage_goals(c);
    rx_set_sp(sp0);
}
static void float_listeners_set(rx_addr list, rx_addr value) {             /* 0063b830 FloatListeners_Set: stdcall (list, &v) */
    const uint32_t a[2] = {list, value};
    const rx_addr sp0 = rider_callsite(2, a);
    rx_world_float_listeners_set(list, value);
    rx_set_sp(sp0);
}

/* 00619050 RiderAnimation_ResetPose (EDI = RA): re-asserts the motion switch outputs, then (unless frozen) resets the lean,
 * steering and stunt state (00621400 / 00621320 / 00621490), the listener values, the balance weights (0.5 / 0) and timers;
 * then blend 6 over 0.25, the +0x5bc controller, the stunt state (0061c5f0). */
void rx_rider_animation_reset_pose(rx_addr ra) {
    const rx_addr sp0 = rx_sp(), A8 = (sp0 - 4u) & ~7u, local = A8 - 4;
    rx_set_sp(A8 - 0x10);
    if (U32(ra, 0x564)) SU8(U32(ra, 0x564), 0, 1);
    if (U32(ra, 0x568)) SU16(U32(ra, 0x568), 0, U16(ra, 0x5ac));
    if (U32(ra, 0x57c)) SU8(U32(ra, 0x57c), 0, 1);
    if (U32(ra, 0x580)) SU16(U32(ra, 0x580), 0, U16(ra, 0x5a0));
    weight_out(ra, 0x588, 1.0f);
    weight_out(ra, 0x594, 1.0f);
    if (U32(ra, 0xaf08) == 0) {
        { const rx_addr sp1 = rider_callsite(0, 0); rx_world_rider_end_stunt(ra); rx_set_sp(sp1); }
        { const rx_addr sp1 = rider_callsite(0, 0); rx_world_rider_end_trick(ra); rx_set_sp(sp1); }
        uint32_t a = ra;
        { const rx_addr sp1 = rider_callsite(1, &a); rx_world_rider_end_pose_override(ra); rx_set_sp(sp1); }
        SF32(local, 0, clamp_hi_lo(F32(ra, 0x468), 0.0f, F32(ra, 0x464)));
        float_listeners_set(ra + 0x46c, local);
        SF32(local, 0, clamp_hi_lo(F32(ra, 0x488), 0.0f, F32(ra, 0x484)));
        float_listeners_set(ra + 0x48c, local);
        SF32(ra, 0x3b4, 0.5f); SF32(ra, 0x388, 0.5f); SF32(ra, 0x2d8, 0.0f); SF32(ra, 0x330, 0.0f);
        SF32(ra, 0x35c, 1.0f); SF32(ra, 0xaf50, 0.5f);
        weight_out(ra, 0x3e4, 0.5f); weight_out(ra, 0x3f0, 0.5f); weight_out(ra, 0x3fc, 0.5f); weight_out(ra, 0x3d8, 0.5f);
        weight_out(ra, 0x3cc, 0.0f); weight_out(ra, 0x3c0, 0.0f);
        if (U32(ra, 0x5bab8) == 0) { SF32(ra, 0x414, 0.5f); SF32(ra, 0x42c, 0.5f); SF32(ra, 0x444, 0.5f); SF32(ra, 0x45c, 0.5f); }
        a = ra + 0x46c; { const rx_addr sp1 = rider_callsite(1, &a); rx_rider_vec8_mark_all(a); rx_set_sp(sp1); }
        a = ra + 0x48c; { const rx_addr sp1 = rider_callsite(1, &a); rx_rider_vec8_clear_all(a); rx_set_sp(sp1); }
        SF32(ra, 0xaf40, 0.5f);
        SU32(ra, 0xaf10, 0); SU32(ra, 0xaf30, 0); SU32(ra, 0xaf34, 1); SU32(ra, 0xaf14, 0); SU32(ra, 0xaf18, 0);
        SU32(ra, 0x5b2e8, 0); SU32(ra, 0x5b2dc, 0);
        SF32(ra, 0xaf44, 0.2f /* 0x92e388 */); SF32(ra, 0xaf48, 0.2f); SF32(ra, 0xaf4c, 0.2f);
        SF32(ra, 0x5bb58, 0.0f); SU32(ra, 0x5b2d4, 0); SU32(ra, 0x5b584, 0xc); SU32(ra, 0x5b588, 0); SU32(ra, 0x5bb50, 0);
        SF32(ra, 0x5baac, 0.0f); SF32(ra, 0x5bab4, 0.0f); SF32(ra, 0x5bab0, 0.0f); SU32(ra, 0x5b2e0, 0); SF32(ra, 0x5b1f4, 0.0f);
    }
    SU32(ra, 0xaf54, 0); SU32(ra, 0x5bb54, 0); SU8(ra, 0x5b578, 0); SU32(ra, 0x5b6d0, 0);
    ra_set_blend(ra, 6, 0.25f /* 0x92ff84 */);
    SU8(U32(ra, 0x5c4), 0x5c, 0);
    ra_reset_controller(U32(ra, 0x5bc));
    SU32(ra, 0xaf00, 0); SU32(ra, 0x5b57c, 0xc); SU32(ra, 0x5b580, 3);
    SF32(ra, 0x5b528, 0.0f); SF32(ra, 0x5b52c, 0.0f); SF32(ra, 0x5b530, 0.0f); SF32(ra, 0x5b534, 0.0f);
    SF32(ra, 0x5ba64, 0.0f); SF32(ra, 0x5ba60, 0.0f);
    ANIM_CALL_FRAME(0); rx_rider_animation_reset_stunt(ra); ANIM_CALL_FRAME_END(0);
    SU32(ra, 0x5b568, 0); SF32(ra, 0x5b570, 0.0f);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00619050, rx_rider_animation_reset_pose)

/* 00623500 RiderAnimation_Restart (ESI = RA): unless in stance 2: the stop sequence, the pose reset, the controller reset, the
 * frozen / camera flags cleared, the rider's +0x1f4 flag set, 0042a1c0 on +0x5d0, and the player's active camera entity told
 * (vf35) when there is one. */
void rx_rider_animation_restart(rx_addr ra) {
    if (U32(ra, 0xaee0) == 2) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0xc);                                    /* push ecx, ebx, edi */
    ra_leave_state(ra);
    ANIM_CALL_FRAME(0); rx_rider_animation_reset_pose(ra); ANIM_CALL_FRAME_END(0);
    ra_reset_controller(U32(ra, 0x5bc));
    SU8(ra, 0x5b578, 0); SU32(ra, 0x668, 0);
    SU8(U32(ra, 0x144), 0x1f4, 1);
    SU32(ra, 0xae90, 0);
    const uint32_t ch = ra + 0x5d0;
    { const rx_addr sp1 = rider_callsite(1, &ch); rx_vehicle_clear_handles(ra + 0x5d0); rx_set_sp(sp1); }
    SU32(ra, 0x5b6d0, 0);
    rx_addr player = U32(U32(ra, 0x5b554), 0x190);
    if (player) {
        const rx_addr sp1 = rider_callsite(0, 0);
        const rx_addr cam = rx_world_player_active_camera_entity(player);
        rx_set_sp(sp1);
        if (cam) {
            const rx_addr sp2 = rider_callsite(0, 0);
            const rx_addr cam2 = rx_world_player_active_camera_entity(player);
            rx_set_sp(sp2);
            anim_vcall0(cam2, 0x8c);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00623500, rx_rider_animation_restart)

/* ---- RiderCharacter teardown ---- */

/* 006248c0 RiderCharacter_ReleaseAnimation (EDI = rider character): its model instance (+0x200) destroyed, its character animation
 * (+0x148) released (its name cache 007fa1e0, its slot array freed), the base vtables restored (CharacterEntity 0x93708c /
 * 0x95a2e4), and the two entity registries 0xa95824 / 0xa95828 told (vf1). */
void rx_rider_character_release_animation(rx_addr e) {
    ANIM_CALL_FRAME(0); rx_model_instance_destroy(e + 0x200, 0); ANIM_CALL_FRAME_END(0);
    rx_addr c = e + 0x148;
    SU32(c, 0, 0x00975e6c);
    ANIM_CALL_FRAME(0); rx_character_anim_release(c); ANIM_CALL_FRAME_END(0);
    { const rx_addr a = c + 0x8c; const rx_addr sp1 = rider_callsite(0, 0); rx_name_hash_free(a); rx_set_sp(sp1); }
    anim_aligned_free(U32(c, 4));
    SU32(e, 0x90, 0x0093708c);
    SU32(e, 0, 0x0095a2e4);
    for (int k = 0; k < 2; k++) {
        rx_addr reg = U32(0, k ? 0x00a95828 : 0x00a95824);
        if (reg) {
            const rx_addr target = U32(U32(reg, 0), 4);
            void (*remove)(rx_addr, rx_addr) = RX_FN(target, void (*)(rx_addr, rx_addr));
            if (remove) {
                const rx_addr saved_sp = rider_callsite(1, &e);
                remove(reg, e);
                rx_set_sp(saved_sp);
            } else rx_call_original(target, RX_THISCALL, reg, 1, &e);
        }
    }
}
RX_FUNCTION(0x006248c0, rx_rider_character_release_animation)

/* 006b9b20 AnimEntitySingleAnimLoop::sdtor (thiscall, flags byte, RET 4).
 * Shared by the base loop and its particles subclass: emitter ownership is released by vf9,
 * not here. The original rewrites exactly these three vtables, tears down CharacterEntity,
 * and conditionally frees the allocation under the same allocator lock as 00624870. */
rx_addr rx_anim_single_loop_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4); /* push edi */
    SU32(e, 0, 0x0095f674);
    SU32(e, 0x90, 0x0095f6f4);
    SU32(e, 0x140, 0x0095f734);
    ANIM_CALL_FRAME(0); rx_rider_character_release_animation(e); ANIM_CALL_FRAME_END(0);
    if (flags & 1) {
        if (U8(0, 0x00da67d8)) anim_enter_cs(0x00d61704);
        SU32(rx_sp() - 4, 0, e); crt_aligned_free(e); /* IAT 0x916350 */
        if (U8(0, 0x00da67d8)) anim_leave_cs(0x00d61704);
    }
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x006b9b20, rx_anim_single_loop_sdtor)

/* 00624930 RiderCharacter_Destroy (thiscall): the RiderCharacter vtables (0x95881c / 0x9588a4 / 0x9588e4), its +0x380 tree
 * emptied (0068f300 from the root's first child, the header relinked to itself, size 0), 006893b0 (ESI), then the release. */
void rx_rider_character_destroy(rx_addr e) {
    SU32(e, 0, 0x0095881c); SU32(e, 0x90, 0x009588a4); SU32(e, 0x140, 0x009588e4);
    rx_addr tree = e + 0x380;
    uint32_t a = U32(U32(e, 0x398), 4);
    ANIM_CALL_FRAME(4); rx_tree_free_subtree(tree, a); ANIM_CALL_FRAME_END(4);   /* 0068f300 (ECX = the tree, stack node; anim_scenario_misc.c) */
    rx_addr head = U32(tree, 0x18);
    SU32(head, 4, head);
    SU32(tree, 0x1c, 0);
    SU32(U32(tree, 0x18), 0, U32(tree, 0x18));
    SU32(U32(tree, 0x18), 8, U32(tree, 0x18));
    ANIM_CALL_FRAME(0); rx_rider_tree_release(tree); ANIM_CALL_FRAME_END(0);   /* 006893b0 (ESI = the tree) */
    ANIM_CALL_FRAME(0); rx_rider_character_release_animation(e); ANIM_CALL_FRAME_END(0);
}
RX_FUNCTION(0x00624930, rx_rider_character_destroy)

/* 00624870 RiderCharacter::sdtor (thiscall: flags; RET 4): destroys; with flag bit 0 frees the object (_aligned_free, under the
 * allocator lock 0xd61704 when 0xda67d8 says so). Returns the object. */
rx_addr rx_rider_character_sdtor(rx_addr e, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ANIM_CALL_FRAME(0); rx_rider_character_destroy(e); ANIM_CALL_FRAME_END(0);
    if (flags & 1) {
        if (U8(0, 0x00da67d8)) anim_enter_cs(0x00d61704);
        SU32(rx_sp() - 4, 0, e); crt_aligned_free(e);   /* _aligned_free (IAT 0x916350) */
        if (U8(0, 0x00da67d8)) anim_leave_cs(0x00d61704);
    }
    rx_set_sp(sp0);
    return e;
}
RX_FUNCTION(0x00624870, rx_rider_character_sdtor)

/* ---- the rider's checked std::list<int> helpers (debug-style frames: EBP = entry ESP - 4, locals below it) ----
 * List L: +0x14 head node, +0x18 size; node: +0 next, +4 prev, +8 value; iterator: (container proxy, node). The node / prev / next
 * accessors of the iterators (007fa6c0 [it + 4], 005c0970 node + 4, 007e3be0 node) are written inline. */

/* 005c0470 std::list::begin (fastcall: ECX = the list, EAX = the iterator out; plain ret): out = {the container proxy [list], the first node
 * (the head node's next)}. ECX / EDX are left holding the proxy and the first node. */
void rx_rider_list_begin(rx_addr list, rx_addr out) {
    const uint32_t first = U32(U32(list, 0x14), 0);
    const uint32_t proxy = U32(list, 0);
    SU32(out, 4, first);
    SU32(out, 0, proxy);
}
RX_FUNCTION(0x005c0470, rx_rider_list_begin)

/* 005c0540 std::list::end (EDI = the list, EAX = its head node, ESI = the iterator out; plain ret) -> EAX = out: out = {the container proxy,
 * the head node}; a null list reports through _invalid_parameter_noinfo. */
rx_addr rx_rider_list_end(rx_addr list, rx_addr head, rx_addr out) {
    SU32(out, 0, 0);
    SU32(out, 4, head);
    if (list == 0) anim_invalid_parameter();
    SU32(out, 0, U32(list, 0));
    return out;
}
RX_FUNCTION(0x005c0540, rx_rider_list_end)

/* 005bb950 checked iterator inequality (EDI = a, ESI = b; plain ret) -> AL = a.node != b.node: both iterators must belong to the same
 * container (a's proxy non-null and equal to b's), else _invalid_parameter_noinfo. EAX keeps the upper bytes of a's node word. */
uint32_t rx_rider_list_iter_ne(rx_addr a, rx_addr b) {
    const uint32_t proxy = U32(a, 0);
    if (proxy == 0 || proxy != U32(b, 0)) anim_invalid_parameter();
    const uint32_t node = U32(a, 4);
    const uint32_t ne = node != U32(b, 4) ? 1u : 0u;
    return (node & 0xffffff00u) | ne;
}
RX_FUNCTION(0x005bb950, rx_rider_list_iter_ne)

/* 00706600 checked iterator increment (ESI = the iterator {proxy, node}; plain ret; saves EDI) -> EAX = the iterator: the container (the
 * proxy's first word) must exist and the node must not be its head node (container +0x14); then node = node->next. */
rx_addr rx_rider_list_iter_next(rx_addr it) {
    SU32(rx_sp(), -4, anim_reg_get(ANIM_EDI));              /* push edi */
    uint32_t proxy = U32(it, 0);
    uint32_t container;
    if (proxy == 0) {
        anim_invalid_parameter();
        proxy = U32(it, 0);
        container = proxy == 0 ? 0 : U32(proxy, 0);
    } else {
        container = U32(proxy, 0);
    }
    if (U32(it, 4) == U32(container, 0x14)) anim_invalid_parameter();
    SU32(it, 4, U32(U32(it, 4), 0));
    return it;
}
RX_FUNCTION(0x00706600, rx_rider_list_iter_next)

/* The pre-increment / begin / end calls at the call sites: the callee entered at the original's ESP, below the return address. */
static void list_begin(rx_addr list, rx_addr out) {            /* 005c0470: ECX = list, EAX = iterator out */
    ANIM_CALL_FRAME(0); rx_rider_list_begin(list, out); ANIM_CALL_FRAME_END(0);
}
static void list_end(rx_addr list, rx_addr out) {              /* 005c0540: EDI = list, EAX = head, ESI = iterator out */
    ANIM_CALL_FRAME(0); (void)rx_rider_list_end(list, U32(list, 0x14), out); ANIM_CALL_FRAME_END(0);
}
static uint32_t list_iter_ne(rx_addr a, rx_addr b) {           /* 005bb950: EDI = a, ESI = b; al = a != b (checked) */
    uint32_t r;
    ANIM_CALL_FRAME(0); r = rx_rider_list_iter_ne(a, b); ANIM_CALL_FRAME_END(0);
    return r & 0xff;
}
static rx_addr list_iter_deref(rx_addr it) {                   /* 00585c60: ESI = iterator; the value's address (checked) */
    const rx_addr sp0 = rider_callsite(0, 0);
    const rx_addr r = rx_map_iterator_value(it);
    rx_set_sp(sp0);
    return r;
}
static void list_iter_next(rx_addr it) {                       /* 00706600: ESI = iterator; ++ (checked) */
    ANIM_CALL_FRAME(0); (void)rx_rider_list_iter_next(it); ANIM_CALL_FRAME_END(0);
}
static rx_addr it_node(rx_addr it) { return U32(it, 4); }
static rx_addr node_prev(rx_addr n) { return U32(n, 4); }

/* 005c0900 std::list size check (fastcall: ECX = count, EDX = the list; plain ret; saves EBP / ESI) -> EAX = the new size: the size (+0x18)
 * grows by count, unless the list would exceed 0x1fffffff nodes - then the original builds a std::length_error("list<T> too long") and
 * throws it (_CxxThrowException): unreachable for a real list, fatal here. Frame: push ebp; and esp, -8; sub esp, 0x44; push esi. */
uint32_t rx_rider_list_grow_check(uint32_t count, rx_addr list) {
    const rx_addr sp0 = rx_sp();
    const rx_addr top = (sp0 - 4u) & ~7u;                   /* push ebp; and esp, -8 */
    SU32(sp0, -4, anim_reg_get(ANIM_EBP));
    SU32(top - 0x48u, 0, anim_reg_get(ANIM_ESI));           /* sub esp, 0x44; push esi */
    const uint32_t size = U32(list, 0x18);
    if (0x1fffffffu - size < count) {
        fprintf(stderr, "rxcore: list<T> too long (005c0900, std::length_error)\n");
        abort();
    }
    SU32(list, 0x18, size + count);
    return size + count;
}
RX_FUNCTION(0x005c0900, rx_rider_list_grow_check)

/* 00612c50 RiderList_Splice (thiscall; stack: where (2), other list, first (2), last (2), count, flag; RET 0x24): moves the nodes
 * [first, last) of `other` before `where` (the size moves when the lists differ: 005c0900 checks the new size). `args` = the
 * address of the stack arguments (the iterators are used in place). */
void rx_rider_list_splice(rx_addr self, rx_addr args) {
    const rx_addr where = args, other = U32(args, 8), first = args + 0xc, last = args + 0x14;
    const uint32_t count = U32(args, 0x1c);
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x98);
    if (self != other) {
        ANIM_CALL_FRAME(0); (void)rx_rider_list_grow_check(count, self); ANIM_CALL_FRAME_END(0);   /* 005c0900 (ECX = count, EDX = the list) */
        SU32(other, 0x18, U32(other, 0x18) - count);
    }
    SU32(node_prev(it_node(first)), 0, it_node(last));         /* next(prev(first)) = last */
    SU32(node_prev(it_node(last)), 0, it_node(where));         /* next(prev(last)) = where */
    SU32(node_prev(it_node(where)), 0, it_node(first));        /* next(prev(where)) = first */
    rx_addr t = node_prev(it_node(where));
    SU32(it_node(where), 4, node_prev(it_node(last)));         /* prev(where) = prev(last) */
    SU32(it_node(last), 4, node_prev(it_node(first)));         /* prev(last) = prev(first) */
    SU32(it_node(first), 4, t);                                /* prev(first) = the old prev(where) */
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00612c50, rx_rider_list_splice)

/* A direct call of 00612c50 with its 9 stack arguments laid out as the original pushes them. */
static void splice_call(rx_addr self, const uint32_t a[9]) {
    rx_addr sp = rx_sp();
    for (int i = 0; i < 9; i++) SU32(sp - 0x24 + 4 * i, 0, a[i]);
    rx_set_sp(sp - 0x28);
    rx_rider_list_splice(self, sp - 0x24);
    rx_set_sp(sp);
}

/* 00594080 std::list container proxy (ESI = the list; stdcall with one dead argument, RET 4) -> EAX = the list: a 4-byte block whose word is the
 * list, the list's +0 pointing at it; a failed allocation leaves the list's +0 zero. */
rx_addr rx_rider_list_proxy_construct(rx_addr list) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4u);
    SU32(rx_sp(), 0, 4);                                    /* push 4 */
    rx_set_sp(sp0 - 8u);
    SU32(rx_sp(), 0, 0x00594087);
    const rx_addr proxy = anim_aligned_alloc(4);            /* ReflexAlignedAlloc */
    rx_set_sp(sp0);
    if (proxy != 0) {
        SU32(proxy, 0, list);
        SU32(list, 0, proxy);
        return list;
    }
    SU32(list, 0, 0);
    return list;
}
RX_FUNCTION(0x00594080, rx_rider_list_proxy_construct)

/* 00842df0 std::list head node allocator (cdecl, no arguments) -> EAX = the node: a 16-byte block (ReflexAlignedAlloc) whose next (+0) and prev
 * (+4) both point at itself. A failed allocation writes its null link at address 4 (the original does not test the +4 address). Frame: push
 * ebp; the SEH record (-1, handler 0x902070, the TEB's list head fs:[0]); sub esp, 0xc; push ebx / esi / edi; the cookie; EBP = sp0 - 4. */
#define ANIM_FS0 rx_teb()    /* the TEB's exception list head (fs:[0]) */
rx_addr rx_rider_list_head_alloc(void) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4, E = sp0 - 0x2c;
    SU32(ebp, 0, anim_reg_get(ANIM_EBP));
    SU32(sp0, -8, 0xffffffffu);
    SU32(sp0, -0xc, 0x00902070);
    const uint32_t seh_prev = U32(ANIM_FS0, 0);
    SU32(sp0, -0x10, seh_prev);
    SU32(sp0, -0x20, anim_reg_get(ANIM_EBX));
    SU32(sp0, -0x24, anim_reg_get(ANIM_ESI));
    SU32(sp0, -0x28, anim_reg_get(ANIM_EDI));
    SU32(sp0, -0x2c, U32(0, 0x009b45e4) ^ ebp);
    SU32(ANIM_FS0, 0, ebp - 0xc);
    SU32(ebp, -0x10, E);
    rx_set_sp(E - 4u);
    SU32(rx_sp(), 0, 0x10);                                 /* push 0x10 */
    rx_set_sp(E - 8u);
    SU32(rx_sp(), 0, 0x00842e22);
    const rx_addr node = anim_aligned_alloc(0x10);          /* ReflexAlignedAlloc */
    rx_set_sp(E);
    SU32(ebp, -0x14, node);
    SU32(ebp, -4, 0);
    if (node != 0) SU32(node, 0, node);
    SU32(ebp, -0x18, 1);
    if (node + 4u != 0) SU32(node + 4u, 0, node);
    SU32(ebp, -4, 0xffffffffu);
    SU32(ANIM_FS0, 0, U32(ebp, -0xc));
    rx_set_sp(sp0);
    return node;
}
RX_FUNCTION(0x00842df0, rx_rider_list_head_alloc)

/* 00612850 RiderList_Construct (thiscall; RET 4): the head node (00594080 allocates it, the iterator proxy 00842df0), size 0. */
rx_addr rx_rider_list_construct(rx_addr self) {
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4;
    SU32(ebp - 0x18, 0, self);
    SU32(ebp - 0x20, 0, self);                                 /* push ecx: 00594080's argument (its RET 4 pops it) */
    rx_set_sp(ebp - 0x1c);
    const uint32_t esi = anim_reg_get(ANIM_ESI);
    anim_reg_set(ANIM_ESI, self);
    {   /* 00594080 (ESI = the list; stdcall, one dead argument): the container proxy */
        const uint32_t a[1] = {self};
        const rx_addr s = rider_callsite(1, a);
        SU32(rx_sp(), 0, 0x00612865);
        (void)rx_rider_list_proxy_construct(self);
        rx_set_sp(s);
    }
    uint32_t p;
    {   /* 00842df0 (cdecl, no arguments): the head node */
        const rx_addr s = rider_callsite(0, 0);
        SU32(rx_sp(), 0, 0x0061286a);
        p = rx_rider_list_head_alloc();
        rx_set_sp(s);
    }
    SU32(self, 0x14, p);
    SU32(self, 0x18, 0);
    anim_reg_set(ANIM_ESI, esi);
    rx_set_sp(sp0);
    return self;
}
RX_FUNCTION(0x00612850, rx_rider_list_construct)

/* 006084d0 (cdecl a, b; plain ret): swaps the dwords at the two addresses, unless they are the same (the twin of 00613420, read in another
 * order). Frame: push ebp; push ecx (the temporary). */
void rx_rider_swap_dwords(rx_addr a, rx_addr b) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, anim_reg_get(ANIM_EBP));
    if (a == b) return;
    const uint32_t t = U32(a, 0);
    SU32(sp0, -8, t);                                       /* [ebp - 4] */
    SU32(a, 0, U32(b, 0));
    SU32(b, 0, t);
}
RX_FUNCTION(0x006084d0, rx_rider_swap_dwords)

/* 005f3520 std::list proxy swap (ESI = other, EDI = self; plain ret): the containers' proxy words are exchanged (00613420 on the addresses EDI /
 * ESI, which the function's own two register saves push as its arguments), then the proxies' own container words (006084d0 on the swapped
 * values, pushed after). */
void rx_rider_list_swap_proxies(rx_addr other, rx_addr self) {
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -4, other);                                   /* push esi */
    SU32(sp0, -8, self);                                    /* push edi */
    SU32(sp0, -0xc, 0x005f3527);
    rx_set_sp(sp0 - 0xc);
    rx_world_swap_dwords(self, other);                      /* 00613420 (cdecl: the arguments stay) */
    rx_set_sp(sp0 - 8u);
    const uint32_t eax = U32(other, 0), ecx = U32(self, 0);
    SU32(sp0, -0xc, eax);                                   /* push eax; push ecx */
    SU32(sp0, -0x10, ecx);
    SU32(sp0, -0x14, 0x005f3532);
    rx_set_sp(sp0 - 0x14);
    rx_rider_swap_dwords(ecx, eax);                         /* 006084d0 */
    rx_set_sp(sp0);                                         /* add esp, 0x10; pop edi; pop esi (reloaded by the caller's frame) */
}
RX_FUNCTION(0x005f3520, rx_rider_list_swap_proxies)

/* 00612890 RiderList_Swap (thiscall: other; RET 4): proxies swapped (005f3520), heads and sizes exchanged. */
void rx_rider_list_swap(rx_addr self, rx_addr other) {
    if (self == other) return;
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 0x110);
    {   /* 005f3520 (ESI = other, EDI = self; plain ret): the proxies swap */
        const uint32_t esi = anim_reg_get(ANIM_ESI), edi = anim_reg_get(ANIM_EDI);
        anim_reg_set(ANIM_ESI, other);
        anim_reg_set(ANIM_EDI, self);
        ANIM_CALL_FRAME(0);
        rx_rider_list_swap_proxies(other, self);
        ANIM_CALL_FRAME_END(0);
        anim_reg_set(ANIM_ESI, esi);
        anim_reg_set(ANIM_EDI, edi);
    }
    uint32_t t = U32(self, 0x14); SU32(self, 0x14, U32(other, 0x14)); SU32(other, 0x14, t);
    t = U32(self, 0x18); SU32(self, 0x18, U32(other, 0x18)); SU32(other, 0x18, t);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00612890, rx_rider_list_swap)

/* 00612a00 RiderList_SpliceAll (thiscall: where (2), other; RET 0xc): all of other before where. */
void rx_rider_list_splice_all(rx_addr self, uint32_t where0, uint32_t where1, rx_addr other) {
    if (self == other || U32(other, 0x18) == 0) return;
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4;
    rx_set_sp(ebp - 0x9c);
    list_end(other, ebp - 0x10);
    SU32(ebp - 8, 0, U32(ebp - 0x10, 0)); SU32(ebp - 4, 0, U32(ebp - 0xc, 0));
    list_begin(other, ebp - 0x20);
    SU32(ebp - 0x18, 0, U32(ebp - 0x20, 0)); SU32(ebp - 0x14, 0, U32(ebp - 0x1c, 0));
    uint32_t a[9] = {where0, where1, other, U32(ebp - 0x18, 0), U32(ebp - 0x14, 0), U32(ebp - 8, 0), U32(ebp - 4, 0),
                     U32(other, 0x18), 0};
    splice_call(self, a);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00612a00, rx_rider_list_splice_all)

/* 00612aa0 RiderList_Merge (thiscall: other; RET 4): merges the sorted other into this (ascending values). */
void rx_rider_list_merge(rx_addr self, rx_addr other) {
    if (other == self) return;
    const rx_addr sp0 = rx_sp(), ebp = sp0 - 4;
    const rx_addr first1 = ebp - 0x20, last1 = ebp - 0x18, first2 = ebp - 8, last2 = ebp - 0x10, nx = ebp - 0x28;
    rx_set_sp(ebp - 0xc0);
    list_begin(self, first1);
    list_end(self, last1);
    list_begin(other, first2);
    list_end(other, last2);
    while (list_iter_ne(first1, last1) && list_iter_ne(first2, last2)) {
        rx_addr p1 = list_iter_deref(first1);
        rx_addr p2 = list_iter_deref(first2);
        if (I32(p2, 0) < I32(p1, 0)) {
            SU32(nx, 0, U32(first2, 0)); SU32(nx, 4, U32(first2, 4));
            list_iter_next(nx);
            uint32_t a[9] = {U32(first1, 0), U32(first1, 4), other, U32(first2, 0), U32(first2, 4), U32(nx, 0), U32(nx, 4), 1, 0};
            splice_call(self, a);
            SU32(first2, 0, U32(nx, 0)); SU32(first2, 4, U32(nx, 4));
        } else list_iter_next(first1);
    }
    if (list_iter_ne(first2, last2)) {
        uint32_t a[9] = {U32(last1, 0), U32(last1, 4), other, U32(first2, 0), U32(first2, 4), U32(last2, 0), U32(last2, 4),
                         U32(other, 0x18), 0};
        splice_call(self, a);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00612aa0, rx_rider_list_merge)

/* ---- the std::map ("tree") the rider character owns (+0x380): its whole-range erase and release ------------------------------- */

/* 00705150 (EAX = the map {+0 the allocator, +0x18 the header node, +0x1c the count}; stdcall: out iterator*, the begin and end
 * {container, node} pairs; RET 0x14): the range erased and the out iterator written. The whole-map form (what the map's
 * destructors pass: both ends the map's allocator and the header) frees the header's subtree (0068f300), relinks the header to
 * itself, clears the count and writes {allocator, header}. The node-by-node subrange form (the std::map iterator erase 00706030,
 * with its MSVC exception strings) is NOT ported: no caller in the product passes a subrange (the only caller here passes the
 * whole map); it aborts naming 00706030 (plan/QUEUE.md "chain round 3"). */
void rx_stl_map_erase_range(rx_addr map, rx_addr out, uint32_t begin_c, uint32_t begin_n, uint32_t end_c, uint32_t end_n) {
    const rx_addr entry0 = rx_sp(), E = entry0 - 24u;          /* sub esp,8; push ebx, ebp, esi, edi */
    rx_set_sp(E);
    const rx_addr hdr = U32(map, 0x18);
    const uint32_t alloc = U32(map, 0);
    if (begin_c == 0 || begin_c != alloc) crt_invalid_parameter_noinfo();
    if (begin_n == U32(hdr, 0) && (end_c != 0 && end_c == alloc) && end_n == hdr) {
        { const rx_addr s = rx_sp(); rx_set_sp(s - 8u); rx_tree_free_subtree(map, U32(hdr, 4)); rx_set_sp(s); }
        SU32(hdr, 4, hdr);
        SU32(map, 0x1c, 0);
        SU32(hdr, 0, hdr);
        SU32(hdr, 8, hdr);
        SU32(out, 4, U32(hdr, 0));
        SU32(out, 0, U32(map, 0));
        rx_set_sp(entry0);
        return;
    }
    rx_set_sp(entry0);
    act_unreached(0x00706030, "a std::map subrange erase (00705150's iterator loop)");
}
RX_FUNCTION(0x00705150, rx_stl_map_erase_range)

/* 006893b0 (ESI = the tree): the map emptied (00705150 with the whole range), its header node (+0x18) freed and +0x18 / +0x1c
 * cleared, then the map block itself (+0) freed. Frame: push ebp; and esp, -8; sub esp, 8; push (the out pair at the frame). */
void rx_rider_tree_release(rx_addr tree) {
    const rx_addr entry0 = rx_sp();
    const rx_addr E = ((entry0 - 4u) & ~7u) - 8u;              /* push ebp; and esp,-8; sub esp,8 */
    rx_set_sp(E);
    const rx_addr hdr = U32(tree, 0x18);
    ANIM_CALL_FRAME(20); rx_stl_map_erase_range(tree, E, U32(tree, 0), U32(hdr, 0), U32(tree, 0), hdr); ANIM_CALL_FRAME_END(20);
    const rx_addr h = U32(tree, 0x18);
    ANIM_CALL_FRAME(4); crt_aligned_free(h); ANIM_CALL_FRAME_END(4);
    SU32(tree, 0x18, 0);
    SU32(tree, 0x1c, 0);
    ANIM_CALL_FRAME(4); crt_aligned_free(U32(tree, 0)); ANIM_CALL_FRAME_END(4);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x006893b0, rx_rider_tree_release)
