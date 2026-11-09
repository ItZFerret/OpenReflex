/* anim_rider_a.c -- the RiderCharacter's procedural pose, small parts: the RiderAnimation worker's per-node noise wobble and nod
 * (0061cdb0 RiderAnimation_WorkerThread calls them on the RiderAnimation, whose node hierarchy is the AnimationEntity's, vtable slot
 * 0x74 = AnimationEntity::vf29 = this + 0x180), the head bone's model-space transform (cameras, audio, RiderAnimation::vf19) and the
 * lean controller of the ProceduralAnimator (+0x58: 006b5870 / 006b5c50). (core/notes/anim.md "Rider procedural pose") */
#include "anim_rider_a.h"
#include "anim_rider_a_math.h"
#include "phys_mech.h"
#include "loop_jobs.h"

/* ---- callees not in this cluster ---- */
/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr rider_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}
/* AnimationEntity vtable slot 0x74 (AnimationEntity::vf29 00617e10 for the rider: this + 0x180): the node hierarchy. */
static rx_addr anim_entity_hierarchy(rx_addr self) {
    const rx_addr target = U32(U32(self, 0), 0x74);
    rx_addr (*f)(rx_addr) = RX_FN(target, rx_addr (*)(rx_addr));
    return f ? f(self) : rx_call_original(target, RX_THISCALL, self, 0, 0);
}
/* 00853180 Noise1D (cdecl float; result XMM0): 1D gradient noise. `through_x87`: the worker's first call pushes the phase with
 * fld / fstp (a signalling NaN is quieted), the other two with movss. */
static float noise1d(float x, int through_x87) {
    uint32_t a = rx_fb(x);
    if (through_x87 && (a & 0x7fffffffu) > 0x7f800000u) a |= 0x400000u;
    const rx_addr sp0 = rider_callsite(1, &a);
    const float r = phys_noise_1d(rx_bf(a));
    rx_set_sp(sp0);
    return r;
}
/* 0082e610 (EAX = pose, ECX = bone, EDI = 0x30-byte bone record out; stack: character; RET 4): the bone's model-space rotation and
 * translation, composed up the character's parent table (+0x23c). */
static void skeleton_bone_model_transform(rx_addr pose, uint32_t bone, rx_addr out, rx_addr ch) {
    const rx_addr sp0 = rider_callsite(1, &ch);
    rx_skeleton_bone_to_model_scalar(ch, bone, pose, out);
    rx_set_sp(sp0);
}
/* 00401740 Matrix3_ToQuaternionToEsi (ESI = quaternion out; stack: matrix; cdecl). */
static void matrix3_to_quaternion(rx_addr quat_out, rx_addr m) {
    const rx_addr sp0 = rider_callsite(1, &m);
    rx_world_matrix3_to_quaternion_to_fr(quat_out, m, anim_reg_get(ANIM_EBP));
    rx_set_sp(sp0);
}

static rx_addr node_record(rx_addr hier, uint32_t index) { return U32(hier, 8) + 16u * index; }
static rx_addr node_local_matrix(rx_addr rec) { return U32(U32(rec, 0), 0x18) + 0x40u * U8(rec, 0xb); }

/* 00623bd0 (ECX = RiderAnimation, ESI = position out; stack: rotation out; RET 4): the model-space transform of the head bone
 * (+0xaee4) of the rider's character (+0x144), from its current pose (character +0x228): a bone record starting at the identity /
 * the 0xa95d50 vector is filled by 0082e610; translation to ESI, quaternion (16 bytes) to the argument. */
void rx_rider_anim_head_transform(rx_addr self, rx_addr pos_out, rx_addr quat_out) {
    ANIM_FRAME_ENTER(A, 0x40);
    rx_addr rec = A - 0x30, ch = U32(self, 0x144);
    for (uint32_t i = 0; i < 16; i += 4) SU32(rec, i, U32(0, 0x9cb550 + i));
    SU8(rec, 0x20, 0); SU8(rec, 0x21, 0);
    SF32(rec, 0x10, F32(0, 0xa95d50)); SF32(rec, 0x14, F32(0, 0xa95d54)); SF32(rec, 0x18, F32(0, 0xa95d58));
    skeleton_bone_model_transform(U32(ch, 0x228), U32(self, 0xaee4), rec, ch);
    SF32(pos_out, 0, F32(rec, 0x10)); SF32(pos_out, 4, F32(rec, 0x14)); SF32(pos_out, 8, F32(rec, 0x18));
    for (uint32_t i = 0; i < 16; i += 4) SU32(quat_out, i, U32(rec, i));
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00623bd0, rx_rider_anim_head_transform)

/* 00623c80 (fastcall ECX = RiderAnimation, EDX = axis 0; stack: rate, dt, amount, axis 1, axis 2; RET 0x14): the noise wobble of
 * 11 nodes. Record i (0x30 bytes at +0x5b300): +0x00 phase (advanced by rate * dt), +0x04 node index, +0x10 / +0x14 / +0x18 upper
 * and +0x20 / +0x24 / +0x28 lower amplitudes (along axes 2 / 0 / 1). Three noise samples of the phase (x1, x1.1, x3.3, mapped to 0..1)
 * pick a point between -lower and upper per axis, scaled by amount (at most 1) twice and 2.5; the node's local translation becomes
 * its old value plus the offsets along the three axes. */
void rx_rider_anim_noise_wobble(rx_addr self, rx_addr axis0, float rate, float dt, float amount, rx_addr axis1, rx_addr axis2) {
    ANIM_FRAME_ENTER(A, 0x50);
    if (!(1.0f > amount)) amount = 1.0f;
    rx_addr rec = self + 0x5b300;
    for (int i = 0; i < 11; i++, rec += 0x30) {
        uint32_t index = U32(rec, 4);
        rx_addr node = node_record(anim_entity_hierarchy(self), index);
        if (node == 0) continue;
        float phase = rate * dt + F32(rec, 0);
        SF32(rec, 0, phase);
        rx_addr m = node_local_matrix(node);
        float tx = F32(m, 0x30), ty = F32(m, 0x34), tz = F32(m, 0x38);
        float s1 = (noise1d(phase, 1) + 1.0f) * 0.5f /* 0x9322cc */;
        float s2 = (noise1d((F32(rec, 0) + 1.0f) * 1.1f /* 0x9774c4 */, 0) + 1.0f) * 0.5f;
        float s3 = (noise1d((F32(rec, 0) + 1.0f) * 3.3f /* 0x977760 */, 0) + 1.0f) * 0.5f;
        float lo0 = -0.0f /* 0x9774a8 */ - F32(rec, 0x24), lo2 = -0.0f - F32(rec, 0x20), lo1 = -0.0f - F32(rec, 0x28);
        float k1 = ((F32(rec, 0x18) - lo1) * s1 * amount + lo1) * amount * 2.5f /* 0x9d789c */;
        float x1 = F32(axis1, 0) * k1 + tx, y1 = F32(axis1, 4) * k1 + ty, z1 = F32(axis1, 8) * k1 + tz;
        float k2 = ((F32(rec, 0x10) - lo2) * s3 * amount + lo2) * amount * 2.5f;
        float x2 = F32(axis2, 0) * k2 + x1, y2 = F32(axis2, 4) * k2, z2 = F32(axis2, 8) * k2;
        float k0 = ((F32(rec, 0x14) - lo0) * s2 * amount + lo0) * amount * 2.5f;
        float x = F32(axis0, 0) * k0 + x2;
        float y = F32(axis0, 4) * k0 + (y2 + y1);
        float z = F32(axis0, 8) * k0 + (z2 + z1);
        m = node_local_matrix(node);
        SF32(m, 0x30, x); SF32(m, 0x34, y); SF32(m, 0x38, z);
        anim_node_invalidate(node);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00623c80, rx_rider_anim_noise_wobble)

/* 00623f30 (stdcall RiderAnimation; RET 4): the nod. t = (timer +0x5b514 - start +0x5b518) / 0.175 in 0..1 gives an angle of
 * -10 t degrees up to t = 0.33, then 10 t - 10; the local translation of node +0x5b510 is rotated by it about its parent's local row 1
 * (taken as x y w: +0x10 +0x14 +0x1c of the parent's matrix). When the timer passes start + 0.175 it restarts at 0 with a new random
 * start (game LCG 0x9cb860, count 0x9cb864) of 0..20 s. */
void rx_rider_anim_nod(rx_addr self) {
    ANIM_FRAME_ENTER(A, 0xb0);
    float t = (F32(self, 0x5b514) - F32(self, 0x5b518)) / 0.175f /* 0x9d78b4 */;
    float x = t >= 1.0f ? 1.0f : ((0.0f < t || t != t) ? t : 0.0f);
    float deg = 0.33f /* 0x9d78d4 */ > x ? (-10.0f /* 0x9d78b8 */ - 0.0f) * x + 0.0f
                                         : (0.0f - -10.0f) * x - 10.0f /* 0x92fd88 */;
    rx_addr node = node_record(anim_entity_hierarchy(self), U32(self, 0x5b510));
    float rad = deg * 0.01745329f /* 0x977678 */;
    rx_addr parent = U32(U32(node, 0), 8) + 16u * U8(node, 0xf);
    rx_addr pm = node_local_matrix(parent);
    float a0 = F32(pm, 0x10), a1 = F32(pm, 0x14), a3 = F32(pm, 0x1c);
    float sign = rad < 0.0f || rad != rad ? -1.0f /* 0x9cd9e8 */ : 1.0f;
    float u = rider_wrap_pi(sign * rad), u2 = u * u, u4 = u2 * u2;
    float c = -0.0f /* 0x9774a8 */ - rider_cos_poly(u2, u4);
    float s = rider_sin_poly(u2, u4) * u * sign;
    float sa0 = s * a0, sa1 = s * a1, sa3 = s * a3, omc = 1.0f - c;
    float o0 = omc * a0, o1 = omc * a1, o3 = omc * a3;
    float r00 = o0 * a0 + c, r01 = o0 * a1 + sa3, r02 = o0 * a3 - sa1;
    float r10 = o1 * a0 - sa3, r11 = o1 * a1 + c, r12 = o1 * a3 + sa0;
    float r20 = o3 * a0 + sa1, r21 = o3 * a1 - sa0, r22 = o3 * a3 + c;
    rx_addr m = node_local_matrix(node);
    float tx = F32(m, 0x30), ty = F32(m, 0x34), tz = F32(m, 0x38), tw0 = F32(m, 0x3c) * 0.0f;
    float x1 = ((ty * r10 + r00 * tx) + tz * r20) + tw0;
    float y1 = ((ty * r11 + r01 * tx) + tz * r21) + tw0;
    float z1 = ((ty * r12 + r02 * tx) + tz * r22) + tw0;
    SF32(m, 0x30, x1); SF32(m, 0x34, y1); SF32(m, 0x38, z1);
    anim_node_invalidate(node);
    if (F32(self, 0x5b514) >= F32(self, 0x5b518) + 0.175f) {
        rx_job_ordered();   /* a parallel rider pass (loop_jobs.h): the game LCG drawn in the passes' wake order */
        SF32(self, 0x5b514, 0.0f);
        SU32(0, 0x9cb864, U32(0, 0x9cb864) + 1);
        uint32_t seed = U32(0, 0x9cb860) * 0x41c64e6du + 0x3039u;
        SU32(0, 0x9cb860, seed);
        float r = (float)(int32_t)((seed >> 16) & 0x7fff);
        SF32(self, 0x5b518, (float)((double)r * 3.0517578125e-05 /* 0x9776a0 */) * 20.0f /* 0x935aac */);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00623f30, rx_rider_anim_nod)

/* 00629a60 (ECX = bone quaternion, stack: lean controller; RET 4): the lean. Angle = (smoothed (clamp(+0x14, +-4) / 4) * 1.5
 * + clamp(+0x1c * 3, +-4) / 8) * clamp(1 - |+0x18| / 0.45, 0..1), stored at +0x00 (smoothing +0x5c value, +0x60 time, +0x64 last
 * factor, +0x04 dt). The bone's rotation matrix gets its row 0 rotated by that angle about its row 1 (axis-angle quaternion, packed
 * half-angle sin / cos), back to a quaternion (Matrix3_ToQuaternionToEsi) and normalized in double precision. */
void rx_rider_lean_rotate_bone(rx_addr q, rx_addr lean) {
    ANIM_FRAME_ENTER(A, 0x120);
    float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    float y2 = y * 2.0f /* 0x92e330 */, z2 = z * 2.0f, x2 = x * 2.0f;
    float xx = x2 * x, yy = y2 * y, xw = x2 * w, zw = z2 * w, zz = z2 * z, yz = y2 * z, yw = y2 * w, xz = x2 * z, xy = x2 * y;
    rx_v4 row0 = rider_v4(1.0f - (zz + yy), xy + zw, xz - yw, 0.0f);
    rx_v4 row1 = rider_v4(xy - zw, 1.0f - (zz + xx), yz + xw, 0.0f);
    rx_v4 row2 = rider_v4(xz + yw, yz - xw, 1.0f - (yy + xx), 0.0f);

    float b = F32(lean, 0x1c) * 3.0f /* 0x92fe48 */;
    b = b >= 4.0f /* 0x9322c0 */ ? 4.0f : (-4.0f /* 0x9776d8 */ < b || b != b ? b : -4.0f);
    float offset = b * 0.25f /* 0x92ff84 */ * 0.5f /* 0x9322cc */;
    float c = F32(lean, 0x14);
    c = c >= 4.0f ? 4.0f : (-4.0f < c || c != c ? c : -4.0f);
    float dt = F32(lean, 4), time = F32(lean, 0x60);
    float f = rx_divs(time > dt ? dt : time, time);
    float smooth = (c * 0.25f - F32(lean, 0x5c)) * f + F32(lean, 0x5c);
    SF32(lean, 0x64, f); SF32(lean, 0x5c, smooth);
    float angle = smooth * 1.5f /* 0x9322a4 */ + offset;
    SF32(lean, 0, angle);
    float fade = 1.0f - fabsf(F32(lean, 0x18)) / 0.45f /* 0x9cd960 */;
    fade = fade >= 1.0f ? 1.0f : (0.0f >= fade ? 0.0f : fade);
    angle = angle * fade;
    SF32(lean, 0, angle);

    /* row 0 rotated about the normalized row 1 by the angle */
    rx_v4 axis = rider_v4_normalize_by(row1, rider_len2(row1));
    float h = angle * 0.5f, sin_h = rider_neg_cos(h + 1.5707964f /* 0x935b00 */), cos_h = 0.0f - rider_neg_cos(h);
    rx_v4 rq = rider_v4(sin_h * axis.v[0], sin_h * axis.v[1], sin_h * axis.v[2], cos_h);
    rx_v4 r0 = rider_quat_rotate(rq, rider_v4(row0.v[0], row0.v[1], row0.v[2], 0.0f));
    rx_addr m = A - 0xb0;
    rx_st4(m, rider_v4(r0.v[0], r0.v[1], r0.v[2], 0.0f));
    rx_st4(m + 0x10, row1);
    rx_st4(m + 0x20, row2);
    rx_st4(m + 0x30, rider_v4(0.0f, 0.0f, 0.0f, 1.0f));
    matrix3_to_quaternion(q, m);

    x = F32(q, 0); y = F32(q, 4); z = F32(q, 8); w = F32(q, 0xc);
    float n2 = (float)((((double)x * x + (double)w * w) + (double)y * y) + (double)z * z);
    if (n2 == 0.0f) {
        SU32(q, 0, 0); SU32(q, 4, 0); SU32(q, 8, 0); SU32(q, 0xc, 0);
    } else {
        float inv = 1.0f / sqrtf(n2);
        SF32(q, 0xc, (float)((double)inv * w));
        SF32(q, 0, (float)((double)inv * x));
        SF32(q, 4, (float)((double)inv * y));
        SF32(q, 8, (float)((double)inv * z));
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00629a60, rx_rider_lean_rotate_bone)

/* One smoothing step of the lean inputs: |v| > 0.01 -> v = prev = (v * 0.9 - prev) * k + prev with k = min(time, 0.3) / time. */
static void lean_smooth(rx_addr lean, uint32_t value, uint32_t prev, uint32_t time, uint32_t factor) {
    float v = F32(lean, value);
    if (!(fabsf(v) > 0.01f /* 0x9322ac */)) return;
    float t = F32(lean, time);
    float k = t > 0.3f /* 0x931da4 */ ? 0.3f : t;
    k = rx_muls(k, rx_divs(1.0f, t));
    float p = F32(lean, prev);
    float r = (v * 0.9f /* 0x9cd970 */ - p) * k + p;
    SF32(lean, factor, k); SF32(lean, prev, r); SF32(lean, value, r);
}

/* 00629910 (EAX = lean controller (ProceduralAnimator +0x58), stack: pose; RET 4): unless the character (+0x08) has +0x230 set:
 * when enabled (+0x0c == 0) the three inputs +0x1c / +0x18 / +0x14 are smoothed (prev +0x20 / +0x34 / +0x48, time +0x24 / +0x38
 * / +0x4c, factor +0x28 / +0x3c / +0x50), then the lean is applied to the bone +0x10 of the pose (00629a60). */
void rx_rider_lean_eval(rx_addr lean, rx_addr pose) {
    ANIM_FRAME8_ENTER(A, 0);
    if (U32(U32(lean, 8), 0x230) == 0) {
        if (U32(lean, 0xc) == 0) {
            lean_smooth(lean, 0x1c, 0x20, 0x24, 0x28);
            lean_smooth(lean, 0x18, 0x34, 0x38, 0x3c);
            lean_smooth(lean, 0x14, 0x48, 0x4c, 0x50);
        }
        rx_set_sp(A - 8);   /* as at 00629a60's entry: the pushed argument and the return address */
        rx_rider_lean_rotate_bone(pose + 0x30u * U32(lean, 0x10), lean);
    }
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x00629910, rx_rider_lean_eval)
