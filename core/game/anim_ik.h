/* anim_ik.h -- rx_anim cluster "ik": the 0x006a / 0x006b animators (InverseKinematics / Procedural / ExpressionBone /
 * Featherstone) and the animated scene entities (AnimEntity* loops, ThirtySecondBoard, DropGate, LeaderBoard, CrewChief).
 * Files: game/anim_ik.c (IK animator and its chain solvers), game/anim_ik_anim.c (Procedural / Expression / Featherstone
 * animators), game/anim_ik_ent.c (entities). Adapters: oracle/anim_ik*.cpp. Layouts: core/notes/anim.md. */
#pragma once
#include "rx_game.h"
#include "anim_extern.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- .rdata constants (bit exact) and helpers shared by the cluster's files */
#define K_INV_2PI   rx_bf(0x3e22f983u)   /* 0x935ad0 / 0x9774a0: 1 / 2pi */
#define K_2PI       rx_bf(0x40c90fdbu)   /* 0x935ae0 / 0x97749c */
#define K_PI        rx_bf(0x40490fdbu)   /* 0x935af0 / 0x977498 */
#define K_PI_2      rx_bf(0x3fc90fdbu)   /* 0x935b00 */
#define K_EPS       rx_bf(0x38d1b717u)   /* 0x9775a4: 1e-4 */
#define K_SQRT_HALF rx_bf(0x3f350481u)   /* 0x9ddfc8: 0.7071 */
/* the cos(t) polynomial (0x935b10.. packed, 0x935d3c.. scalar) and the sin(t) / t one (0x935d54..) */
#define K_C6 rx_bf(0xb46beed0u)
#define K_C5 rx_bf(0x37cb076bu)
#define K_C4 rx_bf(0xbab5a662u)
#define K_C3 rx_bf(0x3d2aa372u)
#define K_C2 rx_bf(0xbeffff3du)
#define K_C1 rx_bf(0x3f7ffff3u)
#define K_S6 rx_bf(0xb2aee90au)
#define K_S5 rx_bf(0x36352cc0u)
#define K_S4 rx_bf(0xb94fb563u)
#define K_S3 rx_bf(0x3c0884a2u)
#define K_S2 rx_bf(0xbe2aaa5cu)
#define K_S1 rx_bf(0x3f7ffff9u)

/* msvcr90 _invalid_parameter_noinfo through the IAT (0x916224): a checked iterator / index failed (never in practice). */
static inline void ik_invalid_parameter(rx_addr ret) { anim_invalid_parameter(); SU32(rx_sp() - 4, 0, ret); }

/* ---- frames and calls with dead-stack fidelity ---- */

/* The stack cookie the original's prologue stores into its frame (cookie ^ ESP after `sub esp`). */
static inline void ik_frame_cookie(rx_addr slot, rx_addr esp) { SU32(slot, 0, U32(0, 0x009b45e4) ^ esp); }

/* rx_call_original_ex, then the original's return address into the return slot it leaves on the dead stack. */
static inline void ik_call_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int n, const uint32_t* args, rx_addr ret) {
    rx_call_original_ex(fn, in, out, n, args);
    SU32(rx_sp() - 4u * (uint32_t)n - 4u, 0, ret);
}
/* rx_call_original (cdecl / stdcall / thiscall), then the original's return address into the return slot. */
static inline uint32_t ik_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* args, rx_addr ret) {
    uint32_t r = rx_call_original(fn, conv, ecx, n, args);
    SU32(rx_sp() - 4u * (uint32_t)n - 4u, 0, ret);
    return r;
}

/* A direct call to a core function at an original call site: the callee enters with its `n` stack arguments and the return
 * address `ret` below the caller's stack pointer (where the replaced call left them). Returns the caller's SP to restore. */
static inline rx_addr ik_enter(int n, const uint32_t* args, rx_addr ret) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * (uint32_t)n - 4u;
    SU32(sp, 0, ret);
    for (int i = 0; i < n; i++) SU32(sp + 4, 4 * (uint32_t)i, args[i]);
    rx_set_sp(sp);
    return sp0;
}

/* A float passed through the x87 (fld m32; fstp m32): the same bits, signaling NaNs quieted. */
static inline uint32_t ik_x87_bits(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}

/* A child animator's evaluate (vtable slot 4, thiscall (character, root delta, pose), RET 0xc) called from the stack pointer
 * rx_sp(): the core function when registered (with the original's pushes and return address on the game stack, so its frame
 * lands where the original's does), else the original. */
static inline void ik_eval_child(rx_addr animator, rx_addr ch, rx_addr delta, rx_addr pose, rx_addr ret) {
    const rx_addr sp = rx_sp(), target = U32(U32(animator, 0), 0x10);
    anim_eval_fn f = RX_FN(target, anim_eval_fn);
    if (f) {
        SU32(sp - 4, 0, pose); SU32(sp - 8, 0, delta); SU32(sp - 12, 0, ch); SU32(sp - 16, 0, ret);
        rx_set_sp(sp - 16); f(animator, ch, delta, pose); rx_set_sp(sp);
        return;
    }
    uint32_t a[3] = {ch, delta, pose};
    rx_call_original(target, RX_THISCALL, animator, 3, a);
    SU32(sp - 16, 0, ret);
}

/* SkeletonPose_Reset / SkeletonPosePool_Acquire (anim_extern.h) with the return slot written. */
static inline void ik_pose_reset(rx_addr ch, rx_addr pose, rx_addr ret) { anim_pose_reset(ch, pose); SU32(rx_sp() - 4, 0, ret); }
static inline rx_addr ik_pose_acquire(rx_addr ch, rx_addr ret) { rx_addr p = anim_pose_acquire(ch); SU32(rx_sp() - 4, 0, ret); return p; }

/* The pool pose handed back: used flag [+0x22c][slot] = 0 under the pool's critical section (+0x208), slot = ((pose - pool
 * base +0x228) / 0x30, signed) / bones (+0x234). The imports' return slots are written (rx_sp() = ESP before the pushes). */
static inline void ik_pose_release(rx_addr ch, rx_addr pool, rx_addr ret_enter, rx_addr ret_leave) {
    anim_enter_cs(ch + 0x208);
    SU32(rx_sp() - 8, 0, ret_enter);
    const int32_t records = (int32_t)(pool - U32(ch, 0x228)) / 0x30;
    const uint32_t slot = (uint32_t)records / U32(ch, 0x234);
    SU32(U32(ch, 0x22c), 4u * slot, 0);
    anim_leave_cs(ch + 0x208);
    SU32(rx_sp() - 8, 0, ret_leave);
}

/* The child's pose copied into the output pose bone by bone: valid bytes, rotation (raw), translation through the x87. */
static inline void ik_pose_copy(rx_addr dst, rx_addr src, uint32_t bones) {
    for (uint32_t i = 0; i < bones; i++) {
        const rx_addr s = src + 0x30u * i, d = dst + 0x30u * i;
        SU8(d, 0x20, U8(s, 0x20));
        SU32(d, 0x00, U32(s, 0x00)); SU32(d, 0x04, U32(s, 0x04)); SU32(d, 0x08, U32(s, 0x08)); SU32(d, 0x0c, U32(s, 0x0c));
        SU8(d, 0x21, U8(s, 0x21));
        SU32(d, 0x10, ik_x87_bits(U32(s, 0x10))); SU32(d, 0x14, ik_x87_bits(U32(s, 0x14))); SU32(d, 0x18, ik_x87_bits(U32(s, 0x18)));
    }
}

/* Animator vtable slot 3 (thiscall (rate); RET 4): passes a playback rate / time scale down the tree. The cluster's vf3s share
 * this signature. */
typedef void (*anim_ik_rate_fn)(rx_addr self, float rate);

/* A child animator's vf3 called with the rate's bits (as the x87 leaves them) from the stack pointer rx_sp(): the cluster's own
 * vf3s directly (the original's push and return address written, so the callee's frame lands at its original address), every
 * other one through the original. */
static inline void anim_set_rate_child(rx_addr child, uint32_t rate_bits, rx_addr ret) {
    const rx_addr sp = rx_sp(), target = U32(U32(child, 0), 0xc);
    if (target == 0x006b57a0u || target == 0x006ac630u || target == 0x006b5cd0u) {
        anim_ik_rate_fn f = RX_FN(target, anim_ik_rate_fn);
        if (f) {
            SU32(sp - 4, 0, rate_bits); SU32(sp - 8, 0, ret);
            rx_set_sp(sp - 8); f(child, rx_bf(rate_bits)); rx_set_sp(sp);
            return;
        }
    }
    rx_call_original(target, RX_THISCALL, child, 1, &rate_bits);
    SU32(sp - 8, 0, ret);
}

/* ---- anim_ik.c: InverseKinematicsAnimator (vtable 0095efd8) and its chain solvers */
void rx_ik_chain_ccd(rx_addr skel, rx_addr chain, rx_addr pose, float dt);                     /* 006b0530 */
void rx_ik_two_bone_solve(rx_addr skel, rx_addr limb, rx_addr pose, float dt);                 /* 006b0eb0 */
void rx_ik_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);           /* 006b5300 vf4 */
void rx_ik_animator_set_rate(rx_addr self, float rate);                                        /* 006b57a0 vf3 */
uint32_t rx_ik_animator_engage_goals(rx_addr self);                                            /* 006b04d0 */

/* ---- anim_ik_anim.c: Procedural / ExpressionBone / Featherstone animators */
void rx_procedural_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);           /* 006b5870 vf4 */
void rx_procedural_animator_set_rate(rx_addr self, float rate);                                        /* 006b5cd0 vf3 */
void rx_expression_bone_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);      /* 006ad090 vf4 */
void rx_expression_bone_animator_set_rate(rx_addr self, float rate);                                   /* 006ac630 vf3 */
void rx_featherstone_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose);         /* 006afa50 vf4 */

/* ---- anim_ik_ent.c: animated scene entities */
void rx_spline_entity_sample(rx_addr ent, float t, rx_addr out);                                       /* 006bb6e0 */
void rx_spline_entity_job(rx_addr job);                                                                /* 006bb010 */
void rx_single_anim_loop_job(rx_addr job);                                                             /* 006ba170 */
rx_addr rx_state_record_assign(rx_addr dst, rx_addr src);                                              /* 006a5b30 */
void rx_board_walk_step(rx_addr self, float dt, uint32_t to_b);                                        /* 006b85e0 */
void rx_board_job(rx_addr job);                                                                        /* 006b7c70 */
uint32_t rx_anim_alias_pick(rx_addr flag_out, rx_addr avoid, rx_addr name);                            /* 006b6a30 */
void rx_board_setup(rx_addr self);                                                                     /* 006b7fa0 */
void rx_thirty_second_board_update(rx_addr self, float dt);                                            /* 006b7f00 vf19 */
uint32_t rx_drop_gate_swing(rx_addr gate, rx_addr angle, uint32_t negate, float target, float step);   /* 006be330 */
void rx_drop_gate_update(rx_addr self, float dt);                                                      /* 006be1c0 vf19 */
void rx_leader_board_set_position(rx_addr self, uint32_t index, rx_addr name);                         /* 006bf990 */
void rx_leader_board_set_lap(rx_addr self, int32_t lap);                                               /* 006bfcc0 */
void rx_leader_board_update(rx_addr self, float dt);                                                   /* 006bf760 vf19 */
/* the entity classes' vf22: their type id (EAX) */
uint32_t rx_anim_entity_single_anim_loop_type(rx_addr self);                                           /* 006b9ac0 = 0x1050 */
uint32_t rx_anim_entity_spline_anim_loop_type(rx_addr self);                                           /* 006ba560 = 0x1052 */
uint32_t rx_anim_entity_spline_particles_type(rx_addr self);                                           /* 006bbdd0 = 0x1059 */
uint32_t rx_crew_chief_type(rx_addr self);                                                             /* 006bd480 = 0x103b */
uint32_t rx_thirty_second_board_type(rx_addr self);                                                    /* 006b76c0 = 0x103a */
uint32_t rx_drop_gate_type(rx_addr self);                                                              /* 006bd900 = 0x1033 */
uint32_t rx_leader_board_type(rx_addr self);                                                           /* 006bf530 = 0x1037 */

#ifdef __cplusplus
}
#endif
