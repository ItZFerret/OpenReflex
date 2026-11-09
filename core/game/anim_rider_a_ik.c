/* anim_rider_a_ik.c -- the limb controller of the rider's procedural pose (ProceduralAnimator +0x54, 006b5870 / 006b5c50) and its
 * bone aim solver: a smoothed target point is turned into a direction from a bone's model-space position, kept inside a cone about
 * the bone's z axis (+0xb0) and turned at most +0x30 * +0x34 per evaluation; two bones (+0x14, +0x10) are then aimed at the point
 * by 00626740. All of it is the original's packed SSE math (shared idioms: anim_rider_a_math.h). (core/notes/anim.md) */
#include "anim_rider_a.h"
#include "anim_rider_a_math.h"
#include "fx.h"
#include "fx_stunt_input.h"
#include "phys_math.h"
#include "phys_misc.h"
#include "race.h"

/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr ik_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* ---- callees not in this cluster ---- */
/* 0082e610 (EAX = pose, ECX = bone, EDI = 0x30-byte record out; stack: character; RET 4): the bone's model-space transform. */
static void skeleton_bone_model_transform(rx_addr pose, uint32_t bone, rx_addr out, rx_addr ch) {
    const rx_addr sp0 = ik_callsite(1, &ch);
    rx_skeleton_bone_to_model_scalar(ch, bone, pose, out);
    rx_set_sp(sp0);
}
/* 005089d0 (thiscall record): a pose record back to the identity rotation / 0xa95d50 translation. */
static void pose_record_reset(rx_addr rec) {
    const rx_addr sp0 = ik_callsite(0, 0);
    rx_race_pose_reset(rec);
    rx_set_sp(sp0);
}
/* 0042d6b0 (EAX = smoother {value, time, factor}, XMM2 = dt; stack: target; RET 4; result XMM0): one smoothing step,
 * value += (target - value) * min(dt, time) / time. */
static float smoother_step(rx_addr s, float dt, float target) {
    const uint32_t a = rx_fb(target);
    const rx_addr sp0 = ik_callsite(1, &a);
    const float r = phys_lag_filter_update(s, dt, target);
    rx_set_sp(sp0);
    return r;
}
/* 0041c5c0 Vec3_DotDoubleSplat (XMM0, XMM1; result XMM0 = splat of the xyz dot product in double precision). */
static rx_v4 vec3_dot_double_splat(rx_v4 a, rx_v4 b) {
    const rx_addr sp0 = ik_callsite(0, 0);
    const rx_v4 r = rx_fx_dot3_splat(a, b);
    rx_set_sp(sp0);
    return r;
}
/* 0041c580 Vec4_Int4ToFloatDivScalar (XMM0 = four int32; stack: float; cdecl; result XMM0 = float(lanes) / scalar). */
static rx_v4 vec4_int4_div(int32_t i, float d) {
    const int32_t in[4] = {i, i, i, i};
    const uint32_t a = rx_fb(d);
    const rx_addr sp0 = ik_callsite(1, &a);
    const rx_v4 r = rx_fx_int4_div(in, d, 0);
    rx_set_sp(sp0);
    return r;
}
/* 0042caf0 Vec4_SplatLane (XMM0; stack: lane; cdecl; result XMM0 = splat of that lane). */
static rx_v4 vec4_splat_lane(rx_v4 v, uint32_t lane) {
    const rx_addr sp0 = ik_callsite(1, &lane);
    const rx_v4 r = phys_vec4_splat_lane(v, (int32_t)lane);
    rx_set_sp(sp0);
    return r;
}
/* 00548810 (EAX = player; stack: two floats out; cdecl): the player's input axis 7 pair (the look stick). */
static void input_axis_pair(uint32_t player, rx_addr out) {
    const rx_addr sp0 = ik_callsite(1, &out);
    rx_fx_stunt_gesture_dir(player, out);
    rx_set_sp(sp0);
}
/* 00401740 Matrix3_ToQuaternionToEsi (ESI = quaternion out; stack: matrix; cdecl). */
static void matrix3_to_quaternion(rx_addr quat_out, rx_addr m) {
    const rx_addr sp0 = ik_callsite(1, &m);
    rx_world_matrix3_to_quaternion_to_fr(quat_out, m, anim_reg_get(ANIM_EBP));
    rx_set_sp(sp0);
}

/* A unit axis from a vector by the original's rsqrt idiom: dot = Vec3_DotDoubleSplat(v, v), half = Vec4_Int4ToFloatDivScalar(1, 2),
 * one Newton step r = ((rsqrt * rsqrt) * (dot * half) - half) * -1 * rsqrt + rsqrt, axis = r * v (dot itself where dot == 0). */
static rx_v4 axis_refine(rx_v4 v) {
    rx_v4 dot = vec3_dot_double_splat(v, v), half = vec4_int4_div(1, 2.0f), a;
    for (int i = 0; i < 4; i++) {
        float d = dot.v[i], h = half.v[i], r = rx_rsqrt(d);
        float t = rx_muls(rx_subs(rx_muls(r * r, rx_muls(d, h)), h), -1.0f);
        t = rx_adds(rx_muls(t, r), r);
        t = rx_muls(t, v.v[i]);
        a.v[i] = 0.0f == d ? d : t;
    }
    return a;
}
/* The rotation of 00626740's inline splats: the angle's sin / cos (rider_sincos) into the rows about the axis. */
static void rotation_rows(rx_v4 axis, float angle, rx_v4 r[3]) {
    float s, c;
    rider_sincos(angle, &s, &c);
    rider_rot_rows(axis, s, c, 1.0f - c, r);
}
/* The same with the (s, c, 1 - c, 0) vector's lanes splatted through Vec4_SplatLane (2: 1 - c, 1: c, 0: s), as the original does
 * for its last three rotations. */
static void rotation_rows_splat(rx_v4 axis, float angle, rx_v4 r[3]) {
    float s, c;
    rider_sincos(angle, &s, &c);
    rx_v4 v = rider_v4(s, c, 1.0f - c, 0.0f);
    float omc = vec4_splat_lane(v, 2).v[0], cs = vec4_splat_lane(v, 1).v[0], sn = vec4_splat_lane(v, 0).v[0];
    rider_rot_rows(axis, sn, cs, omc, r);
}
/* v's lanes splatted through Vec4_SplatLane, times three rows: ((x * r0 + y * r1) + z * r2). */
static rx_v4 vec_mat3_splat(rx_v4 v, rx_v4 r0, rx_v4 r1, rx_v4 r2) {
    float x = vec4_splat_lane(v, 0).v[0], y = vec4_splat_lane(v, 1).v[0], z = vec4_splat_lane(v, 2).v[0];
    return rider_vec_mat3(rider_v4(x, y, z, 0.0f), r0, r1, r2);
}
static rx_v4 row3_identity(void) { return rx_ld4(0xd61630); }   /* the global (0 0 0 1) */
static float clamp_sym(float v, float hi, float lo) { return v >= hi ? hi : ((lo < v || v != v) ? v : lo); }

/* 00626740 (fastcall ECX = target point (16 bytes), EDX = limb controller; stack: parent record (model-space rotation +0x00,
 * translation +0x10), bone record of the pose, weight, smoothers 0..2; RET 0x18): aims the bone's local rotation at the target.
 * The target goes into the parent's frame; three swings follow, each smoothed (0042d6b0) or clamped, each a rotation about one of
 * the bone's axes multiplied into the bone's rotation rows B:
 *   1. about x by the angle between the bone's z and the target in the y-z plane (times weight, smoother 0);
 *   2. about y by the angle in the x-z plane (times weight, at most 0.35, smoother 1);
 *   then the y axis is rebuilt toward the parent's up (0 1 0) and the z axis kept within 0.3 of its rest angle (2.5); the frame is
 *   orthonormalized (cross products); 3. about the new x axis by the look stick (00548810, when +0x44; times pi / 4 and weight,
 *   smoother 2). +0x84 set: +0x80 = clamp(+0x88 * 3, +-4) / 4 * 0.35 (stored, otherwise unused). The rows go back to the bone's
 * quaternion (Matrix3_ToQuaternionToEsi), normalized in double precision. */
void rx_rider_limb_solve(rx_addr target, rx_addr limbs, rx_addr parent, rx_addr bone, float weight, rx_addr s0, rx_addr s1, rx_addr s2) {
    ANIM_FRAME_ENTER(A, 0x250);
    const rx_v4 ey = rider_v4(0.0f, 1.0f, 0.0f, 0.0f);

    /* the target direction in the parent's frame (the parent's rotation transposed) */
    rx_v4 R[3], B[4];
    rider_quat_rows(parent, R);
    rx_v4 d = rx_v4_sub(rx_ld4(target), rx_ld4(parent + 0x10));
    rx_v4 col0 = rider_v4(R[0].v[0], R[1].v[0], R[2].v[0], 0.0f);
    rx_v4 col1 = rider_v4(R[0].v[1], R[1].v[1], R[2].v[1], 0.0f);
    rx_v4 col2 = rider_v4(R[0].v[2], R[1].v[2], R[2].v[2], 0.0f);
    rx_v4 dir = rider_normalize(rider_vec_mat3(d, col0, col1, col2));
    rider_quat_rows(bone, B);
    B[3] = rider_v4(0.0f, 0.0f, 0.0f, 1.0f);

    /* 1: about x, from the y-z plane */
    rx_v4 t_yz = rider_normalize(rider_v4(0.0f, dir.v[1], dir.v[2], dir.v[3]));
    rx_v4 z_yz = rider_normalize(rider_v4(0.0f, B[2].v[1], B[2].v[2], B[2].v[3]));
    float a1 = rider_acos(rider_dot3(z_yz, t_yz)) * weight;
    if (0.0f < t_yz.v[1]) a1 = 0.0f - a1;
    a1 = smoother_step(s0, F32(limbs, 0xc), a1);
    rx_v4 x_n = rider_normalize(B[0]);
    rx_v4 Q1[4];
    rotation_rows(axis_refine(x_n), a1, Q1);
    Q1[3] = row3_identity();

    /* 2: about y, from the x-z plane, at most 0.35 */
    rx_v4 z_xz = rider_normalize(rider_v4(B[2].v[0], 0.0f, B[2].v[2], B[2].v[3]));
    rx_v4 t_xz = rider_normalize(rider_v4(dir.v[0], 0.0f, dir.v[2], dir.v[3]));
    float a2 = rider_acos(rider_dot3(z_xz, t_xz)) * weight;
    if (0.0f < t_xz.v[1]) a2 = 0.0f - a2;
    a2 = clamp_sym(a2, 0.35f /* 0x9d78c4 */, -0.35f /* 0x9cd9b4 */);
    a2 = smoother_step(s1, F32(limbs, 0xc), a2);
    rx_v4 Q2[4], C[4], N[4];
    rotation_rows(axis_refine(rider_normalize(B[1])), a2, Q2);
    Q2[3] = row3_identity();
    rider_mat_mul(Q2, Q1, C);
    rider_mat_mul(B, C, N);

    /* the y axis toward the parent's up: rotate x about the new z by the excess of the y axis' angle to up over 1.5 */
    rx_v4 up = rider_vec_mat3(N[1], R[0], R[1], R[2]);
    float a3 = 0.0f - (rider_acos(rider_dot3(up, ey)) - 1.5f /* 0x9322a4 */) * weight;
    rx_v4 z_axis = axis_refine(N[2]);
    rx_v4 Q3[3];
    rotation_rows_splat(z_axis, a3, Q3);
    rx_v4 y_new = rider_normalize(rider_cross(N[2], vec_mat3_splat(x_n, Q3[0], Q3[1], Q3[2])));

    if (U32(limbs, 0x84) != 0) {
        float v = clamp_sym(F32(limbs, 0x88) * 3.0f /* 0x92fe48 */, 4.0f /* 0x9322c0 */, -4.0f /* 0x9776d8 */);
        SF32(limbs, 0x80, v * 0.25f /* 0x92ff84 */ * 0.35f /* 0x9d78c4 */);
    }
    (void)vec4_int4_div(1, 2.0f);   /* (the original also forms the sin / cos of +0x80 here and drops them) */

    /* the z axis kept within 0.3 of 2.5 from the parent's up, about the new y */
    float nzy = vec4_splat_lane(N[2], 1).v[0], nzx = vec4_splat_lane(N[2], 0).v[0], nzz = vec4_splat_lane(N[2], 2).v[0];
    rx_v4 nz = rider_v4(nzx, nzy, nzz, 0.0f);
    rx_v4 zup = rider_vec_mat3(nz, R[0], R[1], R[2]);
    float a5 = (rider_acos(rider_dot3(zup, ey)) - 2.5f /* 0x9d789c */) * weight;
    a5 = clamp_sym(a5, 0.3f /* 0x931da4 */, -0.3f /* 0x9774e0 */);
    rx_v4 Q5[3];
    rotation_rows_splat(axis_refine(y_new), a5, Q5);
    rx_v4 z_new = rider_normalize(rider_vec_mat3(nz, Q5[0], Q5[1], Q5[2]));
    rx_v4 x_new = rider_normalize(rider_cross(y_new, z_new));

    /* 3: about the new x axis by the look stick */
    float look = 0.0f;
    if (U32(limbs, 0x44) != 0) {
        rx_addr pair = A - 0x218;
        input_axis_pair(0, pair);
        float v = F32(pair, 0);
        if (v == 0.0f) v = 0.01f /* 0x9322ac */;
        look = v * 0.78539807f /* 0x9779f8 */ * weight;
    }
    look = smoother_step(s2, F32(limbs, 0xc), look);
    rx_v4 M[4] = {rider_v4(x_new.v[0], x_new.v[1], x_new.v[2], N[0].v[3]), rider_v4(y_new.v[0], y_new.v[1], y_new.v[2], N[1].v[3]),
                  rider_v4(z_new.v[0], z_new.v[1], z_new.v[2], N[2].v[3]), N[3]};
    rx_v4 Q6[4], F[4];
    rotation_rows_splat(axis_refine(x_new), look, Q6);
    Q6[3] = row3_identity();
    rider_mat_mul(M, Q6, F);

    rx_addr m = A - 0x50;
    for (int i = 0; i < 4; i++) rx_st4(m + 0x10u * (uint32_t)i, F[i]);
    matrix3_to_quaternion(bone, m);
    float x = F32(bone, 0), y = F32(bone, 4), z = F32(bone, 8), w = F32(bone, 0xc);
    float n2 = (float)((((double)x * x + (double)w * w) + (double)y * y) + (double)z * z);
    if (n2 == 0.0f) {
        SU32(bone, 0, 0); SU32(bone, 4, 0); SU32(bone, 8, 0); SU32(bone, 0xc, 0);
    } else {
        float inv = 1.0f / sqrtf(n2);
        SF32(bone, 0xc, (float)((double)inv * w));
        SF32(bone, 0, (float)((double)inv * x));
        SF32(bone, 4, (float)((double)inv * y));
        SF32(bone, 8, (float)((double)inv * z));
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00626740, rx_rider_limb_solve)

/* The axis-angle quaternion of 006259c0's cone clamps: the axis normalized once more, (sin(h) * axis, cos(h)) with the packed
 * half-angle pair. */
static rx_v4 half_angle_quat(rx_v4 axis, float h) {
    rx_v4 n = rider_normalize(axis);
    float s = rider_neg_cos(h + 1.5707964f /* 0x935b00 */), c = 0.0f - rider_neg_cos(h);
    return rider_v4(s * n.v[0], s * n.v[1], s * n.v[2], c);
}
/* One step of the target smoothers (+0x12c / +0x130 / +0x134: {value, time, factor}): factor = min(dt, time) / time. */
static float target_smooth(rx_addr s, float target, float dt) {
    float t = F32(s, 4);
    float f = rx_divs(t > dt ? dt : t, t);
    float v = (target - F32(s, 0)) * f + F32(s, 0);
    SF32(s, 8, f); SF32(s, 0, v);
    return v;
}

/* 006259c0 (ECX = character, ESI = limb controller (ProceduralAnimator +0x54); stack: pose; RET 4): the hand / foot target
 * +0x60 (16 bytes) smoothed per axis, the weight +0x18 smoothed (+0x1c value, +0x20 time, +0x24 factor, +0x0c dt). From the
 * model-space transform of bone +0x10 the direction to the target is clamped into the cone of +0xb0 about the bone's z axis, then
 * turned at most +0x30 * +0x34 from the last direction +0x70 (stored back, 16 bytes); the target point = bone position + distance *
 * direction. With a weight > 0, bones +0x14 and +0x10 are aimed at it (00626740 with half the weight, each from its own
 * model-space transform; smoothers +0xdc +0xf0 +0x118 / +0xb4 +0xc8 +0x104). */
void rx_rider_limbs_eval(rx_addr L, rx_addr ch, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0xd0);
    float dt = F32(L, 0xc);
    float tx = target_smooth(U32(L, 0x12c), F32(L, 0x60), dt);
    float ty = target_smooth(U32(L, 0x130), F32(L, 0x64), dt);
    float tz = target_smooth(U32(L, 0x134), F32(L, 0x68), dt);
    SF32(L, 0x60, tx); SF32(L, 0x64, ty); SF32(L, 0x68, tz);
    {
        float t = F32(L, 0x20), d0 = F32(L, 0xc);
        float f = rx_divs(t > d0 ? d0 : t, t);
        float v = (F32(L, 0x18) - F32(L, 0x1c)) * f + F32(L, 0x1c);
        SF32(L, 0x24, f); SF32(L, 0x1c, v); SF32(L, 0x18, v);
    }

    /* the model-space transform of bone +0x10 (record at A - 0xb0; its translation's fourth lane is the frame's dead stack) */
    rx_addr rec = A - 0xb0;
    for (uint32_t i = 0; i < 16; i += 4) SU32(rec, i, U32(0, 0x9cb550 + i));
    SF32(rec, 0x10, F32(0, 0xa95d50)); SF32(rec, 0x14, F32(0, 0xa95d54)); SF32(rec, 0x18, F32(0, 0xa95d58));
    SU8(rec, 0x20, 0); SU8(rec, 0x21, 0);
    skeleton_bone_model_transform(pose, U32(L, 0x10), rec, ch);
    rx_v4 T = rx_ld4(rec + 0x10), goal = rx_ld4(L + 0x60), d;
    for (int i = 0; i < 4; i++) d.v[i] = rx_subs(goal.v[i], T.v[i]);
    float dist = sqrtf((d.v[0] * d.v[0] + d.v[2] * d.v[2]) + d.v[1] * d.v[1]);
    rx_v4 dir = rider_normalize(d);

    /* the bone's z axis (lane 3 is dead stack, never used) */
    float qx = F32(rec, 0), qy = F32(rec, 4), qz = F32(rec, 8), qw = F32(rec, 0xc);
    float x2 = qx * 2.0f /* 0x92e330 */, y2 = qy * 2.0f;
    rx_v4 zaxis = rider_v4(qw * y2 + qz * x2, qz * y2 - qw * x2, 1.0f - (qx * x2 + qy * y2), F32(A - 0x74, 0));

    /* the cone about z */
    float ang = rider_acos(rider_dot3(zaxis, dir)), cone = F32(L, 0xb0);
    if (cone < ang) {
        rx_v4 q = half_angle_quat(rider_normalize(rider_cross(dir, zaxis)), (ang - cone) * 0.5f /* 0x9322cc */);
        dir = rider_normalize(rider_quat_rotate(q, rider_v4(dir.v[0], dir.v[1], dir.v[2], 0.0f)));
    }
    /* the turn rate from the last direction */
    float turn = F32(L, 0x34) * F32(L, 0x30);
    rx_v4 prev = rx_ld4(L + 0x70);
    ang = rider_acos(rider_dot3(prev, dir));
    if (turn < ang) {
        rx_v4 q = half_angle_quat(rider_normalize(rider_cross(prev, dir)), 0.5f * turn);
        dir = rider_normalize(rider_quat_rotate(q, rider_v4(prev.v[0], prev.v[1], prev.v[2], 0.0f)));
    }
    rx_v4 point;
    for (int i = 0; i < 4; i++) point.v[i] = rx_adds(rx_muls(dist, dir.v[i]), T.v[i]);
    rx_st4(L + 0x70, dir);
    rx_addr pt = A - 0x80;
    rx_st4(pt, point);

    if (F32(L, 0x18) > 0.0f) {
        pose_record_reset(rec);
        skeleton_bone_model_transform(pose, U32(L, 0x14), rec, ch);
        rx_set_sp(A - 0xec);   /* as at 00626740's entry: six arguments and the return address */
        rx_rider_limb_solve(pt, L, rec, pose + 0x30u * U32(L, 0x14), F32(L, 0x18) * 0.5f, L + 0xdc, L + 0xf0, L + 0x118);
        rx_set_sp(A - 0xd0);
        pose_record_reset(rec);
        skeleton_bone_model_transform(pose, U32(L, 0x10), rec, ch);
        rx_set_sp(A - 0xec);
        rx_rider_limb_solve(pt, L, rec, pose + 0x30u * U32(L, 0x10), F32(L, 0x18) * 0.5f, L + 0xb4, L + 0xc8, L + 0x104);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006259c0, rx_rider_limbs_eval)
