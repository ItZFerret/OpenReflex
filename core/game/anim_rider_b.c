/* anim_rider_b.c -- the rider's procedural limb states (0x0062a..0x00632: ProceduralAnimator +0x5c / +0x60, called from its vf4 /
 * vf8 through 0062aed0). Limb state L (ESI / EDI / EAX of these functions):
 *   +0x00 mode (1..6), +0x04 side (0 / 1), +0x10..+0x18 bone translation result, +0x20 bone controller / bone index,
 *   +0x2c sub-mode, +0x40 yaw input, +0x4c yaw sign, +0x60 rest position, +0x70 / +0x80 two axes, +0xbc rider animation
 *   (+0x1b4 rider: +0x144 bone controller argument, +0x5b578 frozen flag), +0xc4 timer step, +0xc8 / +0xcc timer reloads,
 *   +0xd0 timer, +0xd4 / +0xd5 contact flag now / before, +0xe0 contact point. (core/notes/anim.md) */
#include "anim_rider_b.h"
#include "anim_extern.h"
#include "anim_math.h"
#include "loop_rider.h"
#include "world_wreck.h"
#include <string.h>

/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr riderb_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}

/* ---- callees outside the list ---- */
static void bone_model_transform(rx_addr ctrl, rx_addr pose, rx_addr rec, rx_addr ch) {   /* 0082e610: ECX, EAX, EDI; RET 4 */
    const rx_addr sp0 = riderb_callsite(1, &ch);
    rx_skeleton_bone_to_model_scalar(ch, ctrl, pose, rec);
    rx_set_sp(sp0);
}
static uint32_t rider_sub_mode_blocked(rx_addr rider, uint32_t sub) {                      /* 00623960: ECX sub-mode, EAX rider */
    const rx_addr sp0 = riderb_callsite(0, 0);
    const uint32_t r = rx_world_rider_chain_group(rider, sub);
    rx_set_sp(sp0);
    return r;
}
static uint32_t limb_find_contact(rx_addr st, rx_addr pose, rx_addr a, rx_addr b) {        /* 00632dc0: EAX state; stdcall 3 */
    const uint32_t x[3] = {pose, a, b};
    const rx_addr sp0 = riderb_callsite(3, x);
    const uint32_t r = rx_rider_reach_probe(st, pose, a, b);
    rx_set_sp(sp0);
    return r;
}
static void limb_reach_contact(rx_addr st, rx_addr pose, rx_addr a, rx_addr b) {           /* 00633010: stdcall 4 */
    const uint32_t x[4] = {st, pose, a, b};
    const rx_addr sp0 = riderb_callsite(4, x);
    rx_rider_limb_reach_contact(st, pose, a, b);
    rx_set_sp(sp0);
}
static void limb_release(rx_addr st, rx_addr pose) {                                        /* 00635e10: stdcall 2 */
    const uint32_t x[2] = {st, pose};
    const rx_addr sp0 = riderb_callsite(2, x);
    rx_rider_limb_release(st, pose);
    rx_set_sp(sp0);
}
static void vec3_cross(rx_addr a, rx_addr b, rx_addr out) {                                 /* 00401630: ECX, EAX; cdecl out */
    const rx_addr sp0 = riderb_callsite(1, &out);
    rx_world_vec3_cross(out, a, b);
    rx_set_sp(sp0);
}


/* ---- 0062d970 RiderLimb_Reach: the reach solver ---- */

static void limb_bone_position(rx_addr st, rx_addr pose, uint32_t bone, rx_addr out) {      /* 00632b00: ESI out; stdcall 3 */
    const uint32_t x[3] = {st, pose, bone};
    const rx_addr sp0 = riderb_callsite(3, x);
    rx_rider_bone_target_dir(out, st, pose, bone);
    rx_set_sp(sp0);
}
static float ranged_float_get(rx_addr rf) {                                                  /* 0063b0e0 RangedFloat_Get: EAX */
    const rx_addr sp0 = riderb_callsite(0, 0);
    const float r = rx_world_ranged_float_get(rf);
    rx_set_sp(sp0);
    return r;
}
/* m = q x p for row-major 4x4 matrices, each element summed k = 0..3 (the original multiplies out the constant 0 / 1 entries). */
static void mat4_mul(float m[16], const float q[16], const float p[16]) {
    for (int i = 0; i < 4; i++)
        for (int j = 0; j < 4; j++) {
            float e = rx_muls(q[4 * i], p[j]);
            for (int k = 1; k < 4; k++) e = rx_adds(e, rx_muls(q[4 * i + k], p[4 * k + j]));
            m[4 * i + j] = e;
        }
}
/* The rotation matrix about the axis a (not normalized) by the angle whose sine / cosine are s / c (t = 1 - c). */
static void mat4_axis_angle(float m[16], const float a[3], float s, float c) {
    const float t = rx_subs(1.0f, c), ta0 = t * a[0], ta1 = t * a[1], ta2 = t * a[2];
    const float sa0 = s * a[0], sa1 = s * a[1], sa2 = s * a[2];
    m[0] = ta0 * a[0] + c;   m[1] = ta0 * a[1] + sa2; m[2] = ta0 * a[2] - sa1;  m[3] = 0.0f;
    m[4] = ta1 * a[0] - sa2; m[5] = ta1 * a[1] + c;   m[6] = ta1 * a[2] + sa0;  m[7] = 0.0f;
    m[8] = ta2 * a[0] + sa1; m[9] = ta2 * a[1] - sa0; m[10] = ta2 * a[2] + c;  m[11] = 0.0f;
    m[12] = 0.0f; m[13] = 0.0f; m[14] = 0.0f; m[15] = 1.0f;
}
/* The matrix through Matrix3_ToQuaternionToEsi (00401740) on the solver's frame: matrix at mat, quaternion out at quat. */
static void frame_quat(rx_addr mat, const float m[16], rx_addr quat) {
    for (int k = 0; k < 16; k++) SF32(mat, 4 * k, m[k]);
    anim_matrix_to_quat(mat, quat);
}
/* A pose bone's rotation turned by the quaternion q: out (the record, or the bone itself) = bone (x) q in double precision, then
 * renormalized (a zero length gives zero). */
static void bone_turn(rx_addr out, rx_addr bone, rx_addr q) {
    const double bx = F32(bone, 0), by = F32(bone, 4), bz = F32(bone, 8), bw = F32(bone, 0xc);
    const double qx = F32(q, 0), qy = F32(q, 4), qz = F32(q, 8), qw = F32(q, 0xc);
    const float w = (float)(((bw * qw - bx * qx) - by * qy) - bz * qz);
    const float x = (float)(((bx * qw + bw * qx) + by * qz) - bz * qy);
    const float y = (float)(((by * qw + bw * qy) + bz * qx) - bx * qz);
    const float z = (float)(((bz * qw + bw * qz) + bx * qy) - by * qx);
    SF32(out, 0, x); SF32(out, 4, y); SF32(out, 8, z); SF32(out, 0xc, w);
    const float len2 = (float)((((double)x * x + (double)w * w) + (double)y * y) + (double)z * z);
    if (len2 == 0.0f) { for (int j = 0; j < 4; j++) SU32(out, 4 * j, 0); return; }
    const float inv = 1.0f / sqrtf(len2);
    SF32(out, 0xc, (float)((double)inv * w));
    SF32(out, 0, (float)((double)inv * x));
    SF32(out, 4, (float)((double)inv * y));
    SF32(out, 8, (float)((double)inv * z));
}

/* 0062d970 RiderLimb_Reach (EAX = limb state; stack: pose, three bone records or 0; RET 0x10): with the reach weight +0x40 > 0,
 * the limb's three bones (+0x28 shoulder / hip, +0x24 elbow / knee, +0x20 hand / foot) bend toward the swing target (0062ab30 into
 * +0x30): the first by 0.75 w of its angle off the x axis (about z then y; the left side +4 == 1 negates both), the second and
 * third about x cross the third's position by w (0.33 + 0.42 (1 - the rider's +0x2f4 limit)) and 0.33 w of theirs (the
 * positions from 00632b00). Each turn goes into its record when given, else into the pose bone. */
void rx_rider_limb_reach(rx_addr st, rx_addr pose, rx_addr rec_a, rx_addr rec_b, rx_addr rec_c) {
    ANIM_FRAME_ENTER(A, 0x140);                               /* push ebp; and esp, -16; sub esp, 0x134; push ebx, esi, edi */
    const rx_addr base = A - 0x140, quat = base + 0x10, mat = base + 0x60;
    const rx_addr p28 = base + 0x30, p20 = base + 0xe0, p24 = base + 0xf0;
    ANIM_CALL_FRAME(4); rx_rider_limb_swing_target(st, st + 0x30); ANIM_CALL_FRAME_END(4);
    limb_bone_position(st, pose, U32(st, 0x28), p28);
    limb_bone_position(st, pose, U32(st, 0x20), p20);
    limb_bone_position(st, pose, U32(st, 0x24), p24);
    const float w = F32(st, 0x40);
    if (!(w > 0.0f)) { ANIM_FRAME_LEAVE(A); return; }
    float m[16];
    {   /* the first bone: about z by a1 then about y by a2 (a2 = -a, a1 = -a on the left side, else a) */
        float d = rx_adds(rx_adds(rx_muls(F32(p28, 4), 0.0f), F32(p28, 0)), rx_muls(F32(p28, 8), 0.0f));
        float a = rx_muls(rx_muls(anim_acos_approx(d), w), 0.75f /* 0x935aa8 */);
        float a2 = rx_subs(-0.0f, a), a1 = U32(st, 4) == 1 ? a2 : a;
        float s1, c1, s2, c2;
        anim_sincos(a1, &s1, &c1);
        anim_sincos(a2, &s2, &c2);
        const float ns1 = rx_subs(-0.0f, s1), ns2 = rx_subs(-0.0f, s2);
        const float p[16] = {c1, s1, 0, 0,  ns1, c1, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
        const float q[16] = {c2, 0, ns2, 0,  0, 1, 0, 0,  s2, 0, c2, 0,  0, 0, 0, 1};
        mat4_mul(m, q, p);
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x28);
        bone_turn(rec_a ? rec_a : bone, bone, quat);
    }
    const float k = rx_adds(rx_muls(rx_subs(1.0f, ranged_float_get(U32(U32(st, 0xbc), 0x1b4) + 0x2f4)), 0.42f /* 0x977ab4 */),
                            0.33f /* 0x9d78d4 */);
    const float y0 = rx_muls(F32(p20, 4), 0.0f), z0 = rx_muls(F32(p20, 8), 0.0f);
    const float axis[3] = {rx_subs(z0, y0), rx_subs(rx_muls(F32(p20, 0), 0.0f), F32(p20, 8)),
                           rx_subs(F32(p20, 4), rx_muls(F32(p20, 0), 0.0f))};   /* x cross p20 */
    {   /* the second bone */
        float d = rx_adds(rx_adds(rx_muls(F32(p24, 4), 0.0f), F32(p24, 0)), rx_muls(F32(p24, 8), 0.0f));
        float a = rx_muls(rx_muls(anim_acos_approx(d), F32(st, 0x40)), k);
        float s, c;
        anim_sincos(a, &s, &c);
        mat4_axis_angle(m, axis, s, c);
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x24);
        bone_turn(rec_b ? rec_b : bone, bone, quat);
    }
    {   /* the third bone */
        float d = rx_adds(rx_adds(F32(p20, 0), y0), z0);
        float a = rx_muls(rx_muls(anim_acos_approx(d), F32(st, 0x40)), 0.33f /* 0x9d78d4 */);
        float s, c;
        anim_sincos(a, &s, &c);
        mat4_axis_angle(m, axis, s, c);
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x20);
        bone_turn(rec_c ? rec_c : bone, bone, quat);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0062d970, rx_rider_limb_reach)

/* ---- 0062fa90 RiderLimb_Rest: the rest solver ---- */

/* The elementary rotations of the rest solver's products (row-major 4x4; c / s = cosine / sine of the angle). */
static void mat4_rot_z(float m[16], float c, float s) {
    const float r[16] = {c, s, 0, 0,  rx_subs(-0.0f, s), c, 0, 0,  0, 0, 1, 0,  0, 0, 0, 1};
    memcpy(m, r, sizeof r);
}
static void mat4_rot_y(float m[16], float c, float s) {
    const float r[16] = {c, 0, rx_subs(-0.0f, s), 0,  0, 1, 0, 0,  s, 0, c, 0,  0, 0, 0, 1};
    memcpy(m, r, sizeof r);
}
static void mat4_rot_x(float m[16], float c, float s) {
    const float r[16] = {1, 0, 0, 0,  0, c, s, 0,  0, rx_subs(-0.0f, s), c, 0,  0, 0, 0, 1};
    memcpy(m, r, sizeof r);
}
/* z(a) (y(a) x(-a)): the right side's twist of the rest solver's first two bones. */
static void rest_twist(float m[16], float a) {
    float s0, c0, s1, c1, s2, c2, z[16], y[16], x[16], yx[16];
    anim_sincos(a, &s0, &c0);
    anim_sincos(a, &s1, &c1);
    anim_sincos(rx_subs(-0.0f, a), &s2, &c2);
    mat4_rot_z(z, c0, s0); mat4_rot_y(y, c1, s1); mat4_rot_x(x, c2, s2);
    mat4_mul(yx, y, x);
    mat4_mul(m, z, yx);
}

/* 0062fa90 RiderLimb_Rest (EAX = limb state; stack: pose, three bone records or 0; RET 0x10): as the reach solver, toward the
 * rest target (0062ae60 into +0x30). With the weight w = +0x40 > 0: the first bone turns by its angle off the x axis, 0.5 w,
 * about z, y and -x (the right side, +4 == 1); on the left side by its angle off the y axis, 0.25 w, about y. The second: its angle
 * off x by w (0.25 + 0.25 (1 - the rider's +0x2f4 limit)) the same way (right), or 0.5 w about -z then y (left). The third about x
 * cross its position by 0.33 w. Each turn goes into its record when given, else into the pose bone. */
void rx_rider_limb_rest(rx_addr st, rx_addr pose, rx_addr rec_a, rx_addr rec_b, rx_addr rec_c) {
    ANIM_FRAME_ENTER(A, 0x180);                               /* push ebp; and esp, -16; sub esp, 0x174; push ebx, esi, edi */
    const rx_addr base = A - 0x180, quat = base + 0x20, mat = base + 0xe0;
    const rx_addr p28 = base + 0x40, p20 = base + 0x120, p24 = base + 0x130;
    ANIM_CALL_FRAME(0); rx_rider_limb_rest_target(st, st + 0x30); ANIM_CALL_FRAME_END(0);
    limb_bone_position(st, pose, U32(st, 0x28), p28);
    limb_bone_position(st, pose, U32(st, 0x20), p20);
    limb_bone_position(st, pose, U32(st, 0x24), p24);
    const float w = F32(st, 0x40);
    if (!(w > 0.0f)) { ANIM_FRAME_LEAVE(A); return; }
    const int right = U32(st, 4) == 1;
    float m[16];
    {   /* the first bone */
        if (right) {
            float d = rx_adds(rx_adds(rx_muls(F32(p28, 4), 0.0f), F32(p28, 0)), rx_muls(F32(p28, 8), 0.0f));
            rest_twist(m, rx_muls(rx_muls(anim_acos_approx(d), w), 0.5f /* 0x9322cc */));
        } else {
            float d = rx_adds(rx_adds(rx_muls(F32(p28, 0), 0.0f), F32(p28, 4)), rx_muls(F32(p28, 8), 0.0f));
            float s, c;
            anim_sincos(rx_muls(rx_muls(anim_acos_approx(d), w), 0.25f /* 0x92ff84 */), &s, &c);
            mat4_rot_y(m, c, s);
        }
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x28);
        bone_turn(rec_a ? rec_a : bone, bone, quat);
    }
    {   /* the second bone */
        float d = rx_adds(rx_adds(rx_muls(F32(p24, 4), 0.0f), F32(p24, 0)), rx_muls(F32(p24, 8), 0.0f));
        const float theta = anim_acos_approx(d);
        if (right) {
            float f = ranged_float_get(U32(U32(st, 0xbc), 0x1b4) + 0x2f4);
            float k = rx_adds(rx_muls(rx_subs(1.0f, f), 0.25f), 0.25f);
            rest_twist(m, rx_muls(rx_muls(F32(st, 0x40), theta), k));
        } else {
            float a = rx_muls(rx_muls(F32(st, 0x40), theta), 0.5f);
            float s1, c1, s2, c2, z[16], y[16];
            anim_sincos(rx_subs(-0.0f, a), &s1, &c1);
            anim_sincos(a, &s2, &c2);
            mat4_rot_z(z, c1, s1); mat4_rot_y(y, c2, s2);
            mat4_mul(m, y, z);
        }
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x24);
        bone_turn(rec_b ? rec_b : bone, bone, quat);
    }
    {   /* the third bone */
        const float y0 = rx_muls(F32(p20, 4), 0.0f), z0 = rx_muls(F32(p20, 8), 0.0f);
        const float axis[3] = {rx_subs(z0, y0), rx_subs(rx_muls(F32(p20, 0), 0.0f), F32(p20, 8)),
                               rx_subs(F32(p20, 4), rx_muls(F32(p20, 0), 0.0f))};
        float d = rx_adds(rx_adds(y0, F32(p20, 0)), z0);
        float s, c;
        anim_sincos(rx_muls(rx_muls(anim_acos_approx(d), F32(st, 0x40)), 0.33f /* 0x9d78d4 */), &s, &c);
        mat4_axis_angle(m, axis, s, c);
        frame_quat(mat, m, quat);
        rx_addr bone = pose + 0x30u * U32(st, 0x20);
        bone_turn(rec_c ? rec_c : bone, bone, quat);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0062fa90, rx_rider_limb_rest)

/* 0062aed0 RiderLimb_Evaluate (ESI = limb state, stack pose; RET 4): the state's mode (1 reach 0062d970, 2 reach 0062fa90,
 * 3 / 4 swing 0062c840 / 0062b710, 5 / 6 yaw 0062b3f0 / 0062b0e0); then the bone's model-space translation (bone controller
 * 0082e610 on an identity record) into +0x10; then, unless the rider is frozen or the sub-mode blocks it, the contact search:
 * found -> contact point +0xe0 and reach (the timer restarts from +0xc8 on a change), lost -> timer from +0xcc and release while
 * it runs; the timer counts down by +0xc4. */
void rx_rider_limb_state_eval(rx_addr st, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x60);
    const rx_addr rec = A - 0x30, pa = A - 0x40, pb = A - 0x50;
    switch (U32(st, 0) - 1) {
    case 0: ANIM_CALL_FRAME(0x10); rx_rider_limb_reach(st, pose, 0, 0, 0); ANIM_CALL_FRAME_END(0x10); break;
    case 1: ANIM_CALL_FRAME(0x10); rx_rider_limb_rest(st, pose, 0, 0, 0); ANIM_CALL_FRAME_END(0x10); break;
    case 2: ANIM_CALL_FRAME(8); rx_rider_limb_swing_blend_b(st, pose); ANIM_CALL_FRAME_END(8); break;
    case 3: ANIM_CALL_FRAME(8); rx_rider_limb_swing_blend_a(st, pose); ANIM_CALL_FRAME_END(8); break;
    case 4: ANIM_CALL_FRAME(4); rx_rider_limb_yaw_signed(st, pose); ANIM_CALL_FRAME_END(4); break;
    case 5: ANIM_CALL_FRAME(4); rx_rider_limb_yaw_fixed(st, pose); ANIM_CALL_FRAME_END(4); break;
    default: break;
    }
    rx_st4(rec, rx_ld4(0x009cb550));
    SF32(rec, 0x10, F32(0, 0xa95d50)); SF32(rec, 0x14, F32(0, 0xa95d54)); SF32(rec, 0x18, F32(0, 0xa95d58));
    SU8(rec, 0x20, 0); SU8(rec, 0x21, 0);
    bone_model_transform(U32(st, 0x20), pose, rec, U32(U32(U32(st, 0xbc), 0x1b4), 0x144));
    SF32(st, 0x10, F32(rec, 0x10)); SF32(st, 0x14, F32(rec, 0x14)); SF32(st, 0x18, F32(rec, 0x18));
    int skip = 0;
    if (U32(st, 0x2c) <= 3 && rider_sub_mode_blocked(U32(U32(st, 0xbc), 0x1b4), U32(st, 0x2c))) skip = 1;
    else if (U8(U32(U32(st, 0xbc), 0x1b4), 0x5b578) != 0) skip = 1;
    if (!skip) {
        if (limb_find_contact(st, pose, pa, pb)) {
            SU8(st, 0xd4, 1);
            SF32(st, 0xe0, F32(pb, 0)); SF32(st, 0xe4, F32(pb, 4)); SF32(st, 0xe8, F32(pb, 8));
            if (U8(st, 0xd4) != U8(st, 0xd5)) anim_x87_copy_f32(st + 0xd0, st + 0xc8);
            limb_reach_contact(st, pose, pa, pb);
        } else {
            SU8(st, 0xd4, 0);
            if (U8(st, 0xd5) != 0) anim_x87_copy_f32(st + 0xd0, st + 0xcc);
            if (F32(st, 0xd0) > 0.0f) limb_release(st, pose);
        }
    }
    SU8(st, 0xd5, U8(st, 0xd4));
    SF32(st, 0xd0, F32(st, 0xd0) - F32(st, 0xc4));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0062aed0, rx_rider_limb_state_eval)

/* 0062ae60 RiderLimb_RestTarget (EAX = state, ECX = vec3 out): side 0 / 1 only: rest +0x60 + axis +0x70 * 4.5. */
void rx_rider_limb_rest_target(rx_addr st, rx_addr out) {
    if (U32(st, 4) != 1 && U32(st, 4) != 0) return;
    const float k = 4.5f /* 0x9cd9e4 */;
    float dx = F32(st, 0x70) * k, dy = F32(st, 0x74) * k, dz = F32(st, 0x78) * k;
    float x = F32(st, 0x60) + dx, y = F32(st, 0x64) + dy, z = F32(st, 0x68) + dz;
    SF32(out, 8, z); SF32(out, 4, y); SF32(out, 0, x);
}
RX_FUNCTION(0x0062ae60, rx_rider_limb_rest_target)

/* 0062ab30 RiderLimb_SwingTarget (EDI = state, stack vec3 out; RET 4): n = 2 * normalize(u x v) of the axes u +0x70, v +0x80;
 * the point rest +0x60 + (or -) ((v x n) x v) * 0.7071061 + (v x n) * k + v (v . n), k = -0.7071072 (side 1) / 0.7071072 (side 0),
 * y raised by 0.5. Other sides: untouched. */
void rx_rider_limb_swing_target(rx_addr st, rx_addr out) {
    ANIM_FRAME_ENTER(A, 0x40);
    const rx_addr body = A - 0x40, n = body + 0x10, c1 = body + 0x20, c2 = body + 0x30;
    float ux = F32(st, 0x70), uy = F32(st, 0x74), uz = F32(st, 0x78);
    float vx = F32(st, 0x80), vy = F32(st, 0x84), vz = F32(st, 0x88);
    float a0 = uz * vy - uy * vz, a1 = ux * vz - vx * uz, a2 = vx * uy - ux * vy;
    SF32(n, 8, a2);
    float len2 = (a2 * a2 + a1 * a1) + a0 * a0;
    SF32(n, 0, a0); SF32(n, 4, a1); SF32(body, 0xc, len2);
    float n0 = a0, n1 = a1, n2 = a2;
    if (len2 > 0.0f) {
        float inv = 1.0f / sqrtf(len2);
        n0 = inv * F32(n, 0); n2 = inv * F32(n, 8); n1 = inv * F32(n, 4);
    }
    n0 = n0 * 2.0f; n1 = n1 * 2.0f; n2 = n2 * 2.0f;
    SF32(n, 0, n0); SF32(n, 4, n1); SF32(n, 8, n2);
    uint32_t side = U32(st, 4);
    if (side != 1 && side != 0) { ANIM_FRAME_LEAVE(A); return; }
    rx_addr v = st + 0x80;
    vec3_cross(v, n, c1);
    vec3_cross(c1, v, c2);
    vx = F32(v, 0); vy = F32(v, 4); vz = F32(v, 8);
    const float k1 = 0.7071061f /* 0x977990 */, k2 = side == 1 ? -0.7071072f /* 0x97798c */ : 0.7071072f /* 0x977988 */;
    float d = (vy * F32(n, 4) + vx * F32(n, 0)) + vz * F32(n, 8);
    float px = vx * d, py = vy * d, pz = vz * d;
    float x = (F32(c2, 0) * k1 + F32(c1, 0) * k2) + px;
    float y = (F32(c2, 4) * k1 + F32(c1, 4) * k2) + py;
    float z = (F32(c2, 8) * k1 + F32(c1, 8) * k2) + pz;
    if (side == 1) { x = F32(st, 0x60) + x; y = F32(st, 0x64) + y; z = F32(st, 0x68) + z; }
    else { x = F32(st, 0x60) - x; y = F32(st, 0x64) - y; z = F32(st, 0x68) - z; }
    y = y + 0.5f /* 0x9322cc */;
    SF32(out, 8, z); SF32(out, 4, y); SF32(out, 0, x);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x0062ab30, rx_rider_limb_swing_target)

/* The yaw modes: the bone (+0x20) of the pose turned about y by `angle` (radians): the rotation matrix (c 0 -s / 0 1 0 / s 0 c)
 * to a quaternion (Matrix3_ToQuaternionToEsi), then bone rotation = bone * that (double precision product). */
static void limb_yaw_bone(rx_addr st, rx_addr pose, float angle) {
    ANIM_FRAME_ENTER(A, 0x70);
    const rx_addr body = A - 0x70, q = body + 0x10, m = body + 0x30;
    float s = anim_sign(angle);
    float t = anim_trig_reduce(rx_muls(s, angle));
    float t2 = t * t, t4 = t2 * t2;
    float c = rx_subs(-0.0f /* 0x9774a8 */, anim_cos_poly(t2, t4));
    float sn = rx_muls(anim_sin_poly(t2, t4) * t, s);
    float nsn = rx_subs(-0.0f, sn);
    const float row[16] = {c, 0.0f, nsn, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, sn, 0.0f, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (int i = 0; i < 16; i++) SF32(m, 4 * i, row[i]);
    anim_matrix_to_quat(m, q);
    rx_addr b = pose + 0x30u * U32(st, 0x20);
    double ax = F32(b, 0), ay = F32(b, 4), az = F32(b, 8), aw = F32(b, 0xc);
    double qx = F32(q, 0), qy = F32(q, 4), qz = F32(q, 8), qw = F32(q, 0xc);
    float w = (float)(((aw * qw - ax * qx) - ay * qy) - az * qz);
    float x = (float)(((ax * qw + aw * qx) + ay * qz) - az * qy);
    float y = (float)(((ay * qw + aw * qy) + az * qx) - ax * qz);
    float z = (float)(((az * qw + aw * qz) + ax * qy) - ay * qx);
    SF32(b, 0, x); SF32(b, 4, y); SF32(b, 8, z); SF32(b, 0xc, w);
    ANIM_FRAME_LEAVE(A);
}

/* 0062b0e0 RiderLimb_YawFixed (EDI = state, stack pose; RET 4): yaw +0x40 * -12 degrees. */
void rx_rider_limb_yaw_fixed(rx_addr st, rx_addr pose) {
    limb_yaw_bone(st, pose, F32(st, 0x40) * -12.0f /* 0x9d78d0 */ * 0.01745329f /* 0x977678 */);
}
RX_FUNCTION(0x0062b0e0, rx_rider_limb_yaw_fixed)

/* 0062b3f0 RiderLimb_YawSigned (EDI = state, stack pose; RET 4): yaw +0x40 * 25 degrees when +0x4c > 0, else * -12. */
void rx_rider_limb_yaw_signed(rx_addr st, rx_addr pose) {
    float k = I32(st, 0x4c) > 0 ? 25.0f /* 0x92e134 */ : -12.0f /* 0x9d78d0 */;
    limb_yaw_bone(st, pose, F32(st, 0x40) * k * 0.01745329f /* 0x977678 */);
}
RX_FUNCTION(0x0062b3f0, rx_rider_limb_yaw_signed)

/* A pose bone's rotation moved toward a solved record's by w (the unsigned slerp: result = s0 * solved + s1 * bone), then
 * renormalized (double precision; a zero quaternion stays zero). */
static void bone_toward(rx_addr solved, rx_addr bone, float w) {
    float d = ((F32(bone, 0) * F32(solved, 0) + F32(solved, 4) * F32(bone, 4)) + F32(solved, 8) * F32(bone, 8))
              + F32(bone, 0xc) * F32(solved, 0xc);
    float s0, s1;
    anim_slerp_weights_unsigned(d, w, &s0, &s1);
    float qw = F32(bone, 0xc) * s1 + F32(solved, 0xc) * s0;
    float qx = F32(bone, 0) * s1 + F32(solved, 0) * s0;
    float qy = F32(solved, 4) * s0 + F32(bone, 4) * s1;
    float qz = F32(solved, 8) * s0 + F32(bone, 8) * s1;
    SF32(bone, 4, qy); SF32(bone, 0xc, qw); SF32(bone, 0, qx); SF32(bone, 8, qz);
    float len2 = (float)((((double)qx * qx + (double)qw * qw) + (double)qy * qy) + (double)qz * qz);
    if (len2 == 0.0f) { for (int j = 0; j < 4; j++) SU32(bone, 4 * j, 0); return; }
    float inv = 1.0f / sqrtf(len2);
    SF32(bone, 0xc, (float)((double)qw * inv));
    SF32(bone, 0, (float)((double)qx * inv));
    SF32(bone, 4, (float)((double)qy * inv));
    SF32(bone, 8, (float)((double)qz * inv));
}

/* 0062b710 / 0062c840 RiderLimb_SwingBlend (stdcall: state, pose; RET 8): the two reach solvers run (0062d970 into three local
 * bone records, 0062fa90 on the pose; 0062c840 in the other order), then the state's three bones (+0x28, +0x24, +0x20) turned
 * toward the records by min(+0xb8, 1). */
static void limb_swing_blend(rx_addr st, rx_addr pose, int reach_first) {
    ANIM_FRAME_ENTER(A, 0xc0);
    const rx_addr body = A - 0xc0, ra = body + 0x30, rb = body + 0x60, rc = body + 0x90;
    rx_v4 id = rx_ld4(0x009cb550);
    float tx = F32(0, 0xa95d50), ty = F32(0, 0xa95d54), tz = F32(0, 0xa95d58);
    for (rx_addr r = ra; r <= rc; r += 0x30) {
        rx_st4(r, id); SF32(r, 0x10, tx); SF32(r, 0x14, ty); SF32(r, 0x18, tz); SU8(r, 0x20, 0); SU8(r, 0x21, 0);
    }
    if (reach_first) {
        ANIM_CALL_FRAME(0x10); rx_rider_limb_reach(st, pose, ra, rb, rc); ANIM_CALL_FRAME_END(0x10);
        ANIM_CALL_FRAME(0x10); rx_rider_limb_rest(st, pose, 0, 0, 0); ANIM_CALL_FRAME_END(0x10);
    } else {
        ANIM_CALL_FRAME(0x10); rx_rider_limb_rest(st, pose, ra, rb, rc); ANIM_CALL_FRAME_END(0x10);
        ANIM_CALL_FRAME(0x10); rx_rider_limb_reach(st, pose, 0, 0, 0); ANIM_CALL_FRAME_END(0x10);
    }
    float w = 1.0f > F32(st, 0xb8) ? F32(st, 0xb8) : 1.0f;
    SF32(body, 0x2c, w);
    rx_addr b28 = pose + 0x30u * U32(st, 0x28), b24 = pose + 0x30u * U32(st, 0x24), b20 = pose + 0x30u * U32(st, 0x20);
    bone_toward(ra, b28, w);
    bone_toward(rb, b24, w);
    bone_toward(rc, b20, w);
    ANIM_FRAME_LEAVE(A);
}
void rx_rider_limb_swing_blend_a(rx_addr st, rx_addr pose) { limb_swing_blend(st, pose, 1); }
RX_FUNCTION(0x0062b710, rx_rider_limb_swing_blend_a)
void rx_rider_limb_swing_blend_b(rx_addr st, rx_addr pose) { limb_swing_blend(st, pose, 0); }
RX_FUNCTION(0x0062c840, rx_rider_limb_swing_blend_b)
