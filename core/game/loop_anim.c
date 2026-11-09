/* loop_anim.c -- the skeleton animation core the rider animation jobs and the scene's animated entities run every frame:
 * skeleton poses (reset, the pose pool, a bone's model-space transform up its parent chain), keyframe tracks (locate, rotation and
 * translation sampling) and the animated skeleton's update (0082ff20). (native/game/notes/engine.md "What is hot", anim.md)
 *
 * Skeleton: +0x208 the pose pool's lock, +0x224 the animator, +0x228 the pose pool (+0x22c slot-used flags, +0x230 slots),
 * +0x234 bone count, +0x23c parent index per bone (-1 = root), +0x240 the node binding (+0x00 the root node, +0x08 one 16-byte
 * node reference per bone: +0x00 the hierarchy, +0x0b the node's slot in the hierarchy's 0x40-byte world records at +0x18).
 * A pose is one 0x30-byte record per bone: +0x00 rotation quaternion (x y z w), +0x10 translation, +0x20 / +0x21 changed flags.
 * Keyframe tracks: rotation keys are int16 quaternions (x 1/32767), translation keys 16-byte vectors. */
#include "loop_anim.h"
#include "loop_jobs.h"
#include "loop_util.h"
#include "loop_platform.h"
#include "loop_extern.h"
#include "phys_vec.h"
#include "anim_extern.h"
#include "rider_cap.h"

/* `push ebp; mov ebp, esp`: the caller's frame pointer saved below the return address (a later reader copies such stale words: the
 * limb aiming 006259c0's +0x7c, core/notes/anim.md), EBP = that slot; restored at the return (anim_extern.h's register file). */
#define LA_ENTER(sp0) const uint32_t la_ebp = anim_frame_push_ebp(sp0)
#define LA_LEAVE() anim_reg_set(ANIM_EBP, la_ebp)

/* ---- quaternions as the exe's packed code multiplies them ----
 * a * b = (A2 + A1) + (A3 + A0): A0 = a.x * b.wzyx * (1, -1, 1, -1), A1 = a.y * b.zwxy * (1, 1, -1, -1),
 * A2 = b.yxwz * a.z * (-1, 1, 1, -1), A3 = a.w * b (sign vectors 0093a1f0 / 0093a200 / 0093a210). */
static rx_v4 quat_mul(rx_v4 a, rx_v4 b) {
    const rx_v4 a0 = v4_mul(v4_mul(v4_lane(a, 0), v4_perm(b, 3, 2, 1, 0)), v4(1.0f, -1.0f, 1.0f, -1.0f));
    const rx_v4 a1 = v4_mul(v4_mul(v4_lane(a, 1), v4_perm(b, 2, 3, 0, 1)), v4(1.0f, 1.0f, -1.0f, -1.0f));
    const rx_v4 a2 = v4_mul(v4_mul(v4_perm(b, 1, 0, 3, 2), v4_lane(a, 2)), v4(-1.0f, 1.0f, 1.0f, -1.0f));
    const rx_v4 a3 = v4_mul(v4_lane(a, 3), b);
    return v4_add(v4_add(a2, a1), v4_add(a3, a0));
}
/* v rotated by the unit quaternion q: q * (v.xyz, 0) * conj(q), conj = q * (-1, -1, -1, 1) (0093a220); all four lanes */
static rx_v4 quat_rotate(rx_v4 q, rx_v4 v) {
    return quat_mul(q, quat_mul(v4_xyz(v), v4_mul(q, v4(-1.0f, -1.0f, -1.0f, 1.0f))));
}
/* q / |q| (|q|^2 = (p3 + p1) + (p2 + p0)); 0 where |q| = 0, NaN (0092dc30) where |q|^2 is infinite */
static rx_v4 quat_normalize(rx_v4 q) {
    const rx_v4 p = v4_mul(q, q);
    const rx_v4 l2 = v4_splat(phys_add(phys_add(p.v[3], p.v[1]), phys_add(p.v[2], p.v[0])));
    const rx_v4 len = v4_sqrt(l2);
    const rx_v4 finite = v4_mask(l2.v[0] != INFINITY, l2.v[1] != INFINITY, l2.v[2] != INFINITY, l2.v[3] != INFINITY);
    const rx_v4 nonzero = v4_mask(len.v[0] != 0.0f, len.v[1] != 0.0f, len.v[2] != 0.0f, len.v[3] != 0.0f);
    const rx_v4 r = v4_and(v4_and(v4_div(q, len), nonzero), finite);
    return v4_or(r, v4_andn(finite, v4_splat(rx_bf(0x7fc00000u))));
}

/* ---- poses ---- */

/* 0082e250 SkeletonPose_Reset (EAX = skeleton, EDX = pose): every bone the identity rotation (009cb550) and the zero
 * translation (00a95d50), changed flags clear. */
void rx_skeleton_pose_reset(rx_addr skel, rx_addr pose) {
    const rx_addr end = pose + 0x30u * U32(skel, 0x234);
    for (rx_addr b = pose; b != end; b += 0x30) {
        SU8(b, 0x20, 0);
        SU8(b, 0x21, 0);
        rx_wr64(b + 0x00, rx_rd64(0x009cb550));
        rx_wr64(b + 0x08, rx_rd64(0x009cb558));
        SU32(b, 0x10, U32(0, 0x00a95d50));
        SU32(b, 0x14, U32(0, 0x00a95d54));
        SU32(b, 0x18, U32(0, 0x00a95d58));
    }
}
RX_FUNCTION(0x0082e250, rx_skeleton_pose_reset)

/* 0082e2d0 SkeletonPosePool_Acquire (EDI = skeleton) -> a reset pose from the skeleton's pool (the first free slot, under the
 * pool's lock); the pool's first pose when every slot is taken. */
rx_addr rx_skeleton_pose_acquire(rx_addr skel) {
    loop_enter_cs(skel + 0x208);
    const rx_addr pool = U32(skel, 0x228);
    for (uint32_t i = 0; i < U32(skel, 0x230); i++) {
        if (U32(U32(skel, 0x22c), 4 * i) != 0) continue;
        SU32(U32(skel, 0x22c), 4 * i, 1);
        loop_leave_cs(skel + 0x208);
        const rx_addr pose = U32(skel, 0x228) + 0x30u * (U32(skel, 0x234) * i);
        { const rx_addr sp = loop_push(0, 0, 0x0082e346); rx_skeleton_pose_reset(skel, pose); loop_pop(sp); }
        return pose;
    }
    loop_leave_cs(skel + 0x208);
    return pool;
}
RX_FUNCTION(0x0082e2d0, rx_skeleton_pose_acquire)

/* 0082efb0 SkeletonPose_BoneToModel (skeleton, out transform; ECX = bone, EDX = pose, EDI = out vector (16-aligned); ret 8)
 * -> EDI: the bone's transform in model space: its pose transform carried up the parent chain (translation rotated by each
 * parent's rotation plus the parent's translation, rotation = parent * rotation), then through the root node's local rotation and
 * translation (the hierarchy record's +0x30..+0x38). out = (rotation normalized, translation), *vec = the translation. */
rx_addr rx_skeleton_bone_to_model(rx_addr skel, rx_addr out, uint32_t bone, rx_addr pose, rx_addr vec) {
    /* the original's frame: push ebp; and esp, -16; sub esp, 0x88; push ebx; push esi; the root quaternion local at A - 0x80 */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, F = A - 0x80;
    LA_ENTER(sp0);
    rx_set_sp(A - 0x90);
    const rx_addr parents = U32(skel, 0x23c), b = pose + 0x30u * bone;
    rx_wr64(out + 0, rx_rd64(b + 0));
    rx_wr64(out + 8, rx_rd64(b + 8));
    SU32(out, 0x10, U32(b, 0x10));
    SU32(out, 0x14, U32(b, 0x14));
    SU32(out, 0x18, U32(b, 0x18));
    rx_st4(vec, rx_ld4(out + 0x10));          /* the translation; its w lane is whatever out + 0x1c holds */
    rx_v4 rot = rx_ld4(out);
    for (int32_t p = I32(parents, 4 * bone); p >= 0; p = I32(parents, 4u * (uint32_t)p)) {
        const rx_addr pb = pose + 0x30u * (uint32_t)p;
        const rx_v4 q = rx_ld4(pb);
        rx_st4(vec, v4_add(quat_rotate(q, rx_ld4(vec)), rx_ld4(pb + 0x10)));
        rot = quat_mul(q, rot);
    }
    const rx_addr nodes = U32(skel, 0x240), root = U32(nodes, 0);
    const rx_addr rec = U32(U32(root, 0), 0x18) + 0x40u * U8(root, 0xb);
    __typeof__(&rx_node_local_rotation) local_rotation = LOOP_CORE(0x007dd380, rx_node_local_rotation);
    if (local_rotation) { const rx_addr sp = loop_push(0, 0, 0x0082f193); local_rotation(root, F); loop_pop(sp); }
    else { rx_regs in = loop_regs(); in.r[LOOP_EAX] = root; in.r[LOOP_ESI] = F; loop_ex(0x007dd380, in, 0, 0); }
    const rx_v4 qr = rx_ld4(F);
    const rx_v4 t = v4_add(quat_rotate(qr, rx_ld4(vec)), v4(F32(rec, 0x30), F32(rec, 0x34), F32(rec, 0x38), F32(rec, 0x38)));
    rot = quat_mul(qr, rot);
    rx_st4(vec, t);
    rx_st4(out + 0x10, t);
    rx_st4(out, quat_normalize(rot));
    LA_LEAVE(); rx_set_sp(sp0);
    return vec;
}
RX_FUNCTION(0x0082efb0, rx_skeleton_bone_to_model)

/* ---- the scalar variant (0082e610): the same chain in double-precision scalar code ----
 * Every product and sum in double from float operands, each component rounded to float when stored, in the original's order. */
typedef struct { float x, y, z, w; } quatf;
static inline double D(float f) { return (double)f; }
static quatf quatf_ld(rx_addr a) { quatf q = {F32(a, 0), F32(a, 4), F32(a, 8), F32(a, 0xc)}; return q; }
/* v rotated by q, as q * (v, 0) * (-x, -y, -z, w) (the negations in float, 0x9774a8 - x) */
static void rotate_scalar(quatf q, float v[3], float out[3]) {
    const float vx = v[0], vy = v[1], vz = v[2];
    const float nx = -0.0f - q.x, ny = -0.0f - q.y, nz = -0.0f - q.z;
    const float tw = (float)(((D(q.w) * 0.0 - D(q.x) * D(vx)) - D(q.y) * D(vy)) - D(q.z) * D(vz));
    const float tx = (float)(((D(q.w) * D(vx) + D(q.x) * 0.0) + D(q.y) * D(vz)) - D(q.z) * D(vy));
    const float ty = (float)(((D(q.w) * D(vy) + D(q.y) * 0.0) + D(q.z) * D(vx)) - D(q.x) * D(vz));
    const float tz = (float)(((D(q.w) * D(vz) + D(q.z) * 0.0) + D(q.x) * D(vy)) - D(q.y) * D(vx));
    out[0] = (float)(((D(q.w) * D(tx) + D(tw) * D(nx)) + D(nz) * D(ty)) - D(ny) * D(tz));
    out[1] = (float)(((D(q.w) * D(ty) + D(ny) * D(tw)) + D(tz) * D(nx)) - D(nz) * D(tx));
    out[2] = (float)(((D(q.w) * D(tz) + D(nz) * D(tw)) + D(ny) * D(tx)) - D(ty) * D(nx));
}
/* q * r */
static quatf mul_scalar(quatf q, quatf r) {
    quatf o;
    o.w = (float)(((D(q.w) * D(r.w) - D(r.x) * D(q.x)) - D(q.y) * D(r.y)) - D(q.z) * D(r.z));
    o.x = (float)(((D(q.w) * D(r.x) + D(q.x) * D(r.w)) + D(q.y) * D(r.z)) - D(q.z) * D(r.y));
    o.y = (float)(((D(q.w) * D(r.y) + D(q.y) * D(r.w)) + D(q.z) * D(r.x)) - D(r.z) * D(q.x));
    o.z = (float)(((D(q.w) * D(r.z) + D(q.z) * D(r.w)) + D(r.y) * D(q.x)) - D(q.y) * D(r.x));
    return o;
}

/* 0082e610 SkeletonPose_BoneToModelScalar (stack skeleton; ECX = bone, EAX = pose, EDI = out: rotation, translation; ret 4):
 * as 0082efb0 in scalar double-precision code: the bone's pose transform carried up the parent chain, then through the root node's
 * rotation (Matrix3_ToQuaternion of its world record) and translation; the rotation normalized (zero when its length is 0). */
void rx_skeleton_bone_to_model_scalar(rx_addr skel, uint32_t bone, rx_addr pose, rx_addr out) {
    /* the original's frame: push ebp; and esp, -16; sub esp, 0x58; push ebx; push esi; the root quaternion local at A - 0x40 */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, Q = A - 0x40;
    LA_ENTER(sp0);
    rx_set_sp(A - 0x60);
    const rx_addr b = pose + 0x30u * bone, parents = U32(skel, 0x23c);
    rx_wr64(out + 0, rx_rd64(b + 0));
    rx_wr64(out + 8, rx_rd64(b + 8));
    SU32(out, 0x10, U32(b, 0x10)); SU32(out, 0x14, U32(b, 0x14)); SU32(out, 0x18, U32(b, 0x18));
    for (int32_t p = I32(parents, 4u * bone); p >= 0; ) {
        const rx_addr pb = pose + 0x30u * (uint32_t)p;
        const quatf q = quatf_ld(pb);
        float v[3] = {F32(out, 0x10), F32(out, 0x14), F32(out, 0x18)}, r[3];
        rotate_scalar(q, v, r);
        p = I32(parents, 4u * (uint32_t)p);
        SF32(out, 0x10, F32(pb, 0x10) + r[0]);
        SF32(out, 0x14, F32(pb, 0x14) + r[1]);
        SF32(out, 0x18, F32(pb, 0x18) + r[2]);
        const quatf o = mul_scalar(q, quatf_ld(out));
        SF32(out, 0, o.x); SF32(out, 4, o.y); SF32(out, 8, o.z); SF32(out, 0xc, o.w);
    }
    const rx_addr root = U32(U32(skel, 0x240), 0);
    const rx_addr rec = U32(U32(root, 0), 0x18) + 0x40u * U8(root, 0xb);
    __typeof__(&rx_world_matrix3_to_quaternion_to) to_quat = LOOP_CORE(0x00401740, rx_world_matrix3_to_quaternion_to);
    if (to_quat) { const uint32_t a[1] = {rec}; const rx_addr sp = loop_push(1, a, 0x0082ea93); to_quat(Q, rec); loop_pop(sp); }
    else { rx_regs in = loop_regs(); const uint32_t a[1] = {rec}; in.r[LOOP_ESI] = Q; loop_ex(0x00401740, in, 1, a); }
    const quatf q = quatf_ld(Q);
    float v[3] = {F32(out, 0x10), F32(out, 0x14), F32(out, 0x18)}, r[3];
    rotate_scalar(q, v, r);
    SF32(out, 0x10, r[0] + F32(rec, 0x30));
    SF32(out, 0x14, F32(rec, 0x34) + r[1]);
    SF32(out, 0x18, F32(rec, 0x38) + r[2]);
    const quatf o = mul_scalar(q, quatf_ld(out));
    SF32(out, 0, o.x); SF32(out, 4, o.y); SF32(out, 8, o.z); SF32(out, 0xc, o.w);
    const float n = (float)(((D(o.x) * D(o.x) + D(o.w) * D(o.w)) + D(o.y) * D(o.y)) + D(o.z) * D(o.z));
    if (n == 0.0f) {
        rx_wr64(out, 0); rx_wr64(out + 8, 0);
    } else {
        const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(n);   /* SqrtF 0042c230 */
        SF32(out, 0xc, (float)(D(inv) * D(o.w)));
        SF32(out, 0x0, (float)(D(o.x) * D(inv)));
        SF32(out, 0x4, (float)(D(o.y) * D(inv)));
        SF32(out, 0x8, (float)(D(o.z) * D(inv)));
    }
    LA_LEAVE(); rx_set_sp(sp0);
}
RX_FUNCTION(0x0082e610, rx_skeleton_bone_to_model_scalar)

/* ---- keyframe tracks ---- */

/* 0082b930 AnimKeys_Locate (ECX = key set, xmm0 = time, stack out (or 0); ret 4): the time wrapped into the set's length
 * (+0x10), the key pair around it: frame = time x frames (+0x08), key0 = data (+0x3c) + base (+0x2c) + frame x stride x 8
 * (stride +0x24 rounded up to even), key1 the next (the same at the last frame, +0x20). Kept in the set (+0x14 time, +0x18
 * fraction, +0x34 / +0x38 the keys) and, when given, in out {time, fraction, key0, key1}. */
void rx_animkeys_locate(rx_addr ks, rx_addr out, float t) {
    const float len = F32(ks, 0x10);
    if (t > len) do { t = t - len; } while (t > len);
    const uint32_t stride = (U32(ks, 0x24) + 1u) & ~1u;
    const float f = (float)I32(ks, 8) * t;
    const int32_t frame = rx_cvtt(f);
    const rx_addr key0 = U32(ks, 0x2c) + 8u * (stride * (uint32_t)frame) + U32(ks, 0x3c);
    int32_t next = frame + 1;
    if (next >= I32(ks, 0x20)) next = frame;
    const float frac = f - (float)frame;
    const rx_addr key1 = U32(ks, 0x2c) + 8u * (stride * (uint32_t)next) + U32(ks, 0x3c);
    if (out != 0) {
        SF32(out, 0, t); SF32(out, 4, frac); SU32(out, 8, key0); SU32(out, 0xc, key1);
    }
    /* the cache in the key set: the riders' parallel passes (loop_jobs.h) share their motions' key sets and nothing reads it during
     * them, so it ends as the one-after-another passes leave it */
    const uint32_t c[10] = {rx_fb(t), rx_fb(frac), 0, 0, 0, 0, 0, 0, key0, key1};
    if (rx_jobs_in_job()) { rx_job_store_last(ks + 0x14, c, 2); rx_job_store_last(ks + 0x34, c + 8, 2); return; }
    SU32(ks, 0x34, key0);
    SF32(ks, 0x14, t);
    SF32(ks, 0x18, frac);
    SU32(ks, 0x38, key1);
}
RX_FUNCTION(0x0082b930, rx_animkeys_locate)

/* the slerp of two rotation keys (phys_math's QuatSlerp_Simd 0042ca00: xmm0 = q0, xmm1 = q1, xmm2 = t) */
static rx_v4 slerp(rx_v4 q0, rx_v4 q1, rx_v4 t, rx_addr ret, uint32_t ebp) {
    const rx_addr sp = loop_push(0, 0, ret);
    const rx_v4 r = phys_quat_slerp(q0, q1, t);
#ifdef RX_ORACLE
    phys_quat_slerp_dead_stack(rx_sp(), ebp, q0, q1, t);   /* the frames the original slerp leaves (phys_math.c) */
#else
    (void)ebp;
#endif
    loop_pop(sp);
    return r;
}
static rx_v4 int16_quat(rx_addr k) {
    const float s = 3.0518509e-05f;   /* 0x92e054: 1 / 32767 */
    return v4((float)(int16_t)U16(k, 0) * s, (float)(int16_t)U16(k, 2) * s, (float)(int16_t)U16(k, 4) * s, (float)(int16_t)U16(k, 6) * s);
}

/* 0082b9e0 AnimKeys_SampleRotation (ECX = located key set (+0x04 fraction, +0x08 / +0x0c key0 / key1), EDX = track, ESI = out):
 * the track's two int16 quaternion keys slerped by the fraction into out. */
void rx_animkeys_sample_rotation(rx_addr ks, uint32_t track, rx_addr out) {
    /* the original's frame: push ebp; and esp, -16; sub esp, 0x1c; push edi; then the call */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~15u;
    LA_ENTER(sp0);
    rx_set_sp(A - 0x1c - 4);
    const rx_v4 q0 = int16_quat(U32(ks, 8) + 8u * track), q1 = int16_quat(U32(ks, 0xc) + 8u * track);
    rx_st4(A - 0x10, q1);   /* the original's frame keeps key1's floats (the last staging of the keys) */
    rx_st4(out, slerp(q0, q1, v4_splat(F32(ks, 4)), 0x0082bab0, sp0 - 4));
    LA_LEAVE(); rx_set_sp(sp0);
}
RX_FUNCTION(0x0082b9e0, rx_animkeys_sample_rotation)

/* 0082bac0 AnimKeys_SampleTranslation (stack key set; ECX = track, EDX = out, EDI = located time {time, fraction}; ret 4):
 * a static track (not the set's animated one, +0x04) reads its key (+0x3c data, row +0x1c + track); the animated track lerps
 * the keys around time x rate (+0x0c): a (1 - f) + b f. */
void rx_animkeys_sample_translation(rx_addr ks, uint32_t track, rx_addr out, rx_addr at) {
    if (track != U32(ks, 4)) {
        const rx_addr k = U32(ks, 0x3c) + 16u * (U32(ks, 0x1c) + track);
        SU32(out, 0, U32(k, 0)); SU32(out, 4, U32(k, 4)); SU32(out, 8, U32(k, 8));
        return;
    }
    const int32_t i = rx_cvtt((float)I32(ks, 0xc) * F32(at, 0));
    const float f = F32(at, 4);
    int32_t j = i + 1;
    if (j > I32(ks, 0x1c)) j = i;
    const rx_addr a = U32(ks, 0x3c) + 16u * (uint32_t)i, b = U32(ks, 0x3c) + 16u * (uint32_t)j;
    const float g = 1.0f /* 0x931f24 */ - f;
    SF32(out, 0, F32(a, 0) * g + F32(b, 0) * f);
    SF32(out, 4, F32(a, 4) * g + F32(b, 4) * f);
    SF32(out, 8, F32(a, 8) * g + F32(b, 8) * f);
}
RX_FUNCTION(0x0082bac0, rx_animkeys_sample_translation)

/* 00840040 (ECX = pose, ESI = key set, xmm0 = time, stack the bone map (a byte per track); ret 4): a motion sampled into the
 * pose: the key pair located at the time; the rotation tracks two per 16-byte key record (int16 quaternions x 1/32767, 0092e000)
 * slerped by the fraction into their bones' rotations (changed flag +0x20); the static translations (+0x28 tracks from row +0x1c)
 * copied (flag +0x21); the animated translation track (+0x04) lerped between the keys around time x rate (+0x0c). */
static rx_v4 int16x4(rx_addr k) {
    return v4((float)(int16_t)U16(k, 0), (float)(int16_t)U16(k, 2), (float)(int16_t)U16(k, 4), (float)(int16_t)U16(k, 6));
}
void rx_motion_sample(rx_addr map, rx_addr pose, rx_addr ks, float t) {
    /* the original's frame: push ebp; and esp, -16; sub esp, 0xf8; push ebx; push edi; the located keys at A - 0xb0 */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4) & ~15u, L = A - 0xb0;
    LA_ENTER(sp0);
    rx_set_sp(A - 0x100);
    { const uint32_t a[1] = {L}; const rx_addr sp = loop_push(1, a, 0x00840063); rx_animkeys_locate(ks, L, t); loop_pop(sp); }
    const rx_v4 scale = v4_splat(3.0518509e-05f), frac = v4_splat(F32(L, 4));
    for (int32_t k = 0; k < I32(ks, 0x24); k += 2) {
        const rx_addr r0 = U32(L, 8) + 16u * (uint32_t)(k / 2), r1 = U32(L, 0xc) + 16u * (uint32_t)(k / 2);
        const rx_v4 b = slerp(v4_mul(int16x4(r0 + 8), scale), v4_mul(int16x4(r1 + 8), scale), frac, 0x00840208, sp0 - 4);
        const rx_v4 a = slerp(v4_mul(int16x4(r0), scale), v4_mul(int16x4(r1), scale), frac, 0x0084024f, sp0 - 4);
        const rx_addr bone = pose + 0x30u * U8(map, (uint32_t)k);
        rx_st4(bone, a);
        SU8(bone, 0x20, 1);
        if (k + 1 < I32(ks, 0x24)) {
            const rx_addr next = pose + 0x30u * U8(map, (uint32_t)k + 1);
            rx_st4(next, b);
            SU8(next, 0x20, 1);
        }
    }
    for (int32_t c = 0; c < I32(ks, 0x28); c++) {
        const rx_addr key = U32(ks, 0x3c) + 16u * (U32(ks, 0x1c) + (uint32_t)c);
        const rx_addr bone = pose + 0x30u * U8(map, (uint32_t)c);
        SU32(bone, 0x10, U32(key, 0)); SU32(bone, 0x14, U32(key, 4)); SU32(bone, 0x18, U32(key, 8));
        SU8(bone, 0x21, 1);
    }
    const int32_t i = rx_cvtt((float)I32(ks, 0xc) * F32(L, 0));
    int32_t j = i + 1;
    if (j > I32(ks, 0x1c)) j = i;
    const float f = F32(L, 4), g = 1.0f /* 0x931f24 */ - f;
    const rx_addr a = U32(ks, 0x3c) + 16u * (uint32_t)i, b = U32(ks, 0x3c) + 16u * (uint32_t)j;
    const rx_addr out = pose + 0x30u * U8(map, U32(ks, 4)) + 0x10;
    SF32(out, 0, F32(a, 0) * g + F32(b, 0) * f);
    SF32(out, 4, F32(a, 4) * g + F32(b, 4) * f);
    SF32(out, 8, F32(a, 8) * g + F32(b, 8) * f);
    LA_LEAVE(); rx_set_sp(sp0);
}
RX_FUNCTION(0x00840040, rx_motion_sample)

/* ---- the animated skeleton ---- */

/* the bone's node: its local rotation from the pose, its translation into the node's world record, the subtree invalidated */
static void bone_to_node(rx_addr skel, uint32_t i, rx_addr bone, rx_addr ret_rot, rx_addr ret_inv) {
    const rx_addr node = U32(U32(skel, 0x240), 8) + 0x10u * i;
    __typeof__(&rx_node_set_local_rotation) set_rotation = LOOP_CORE(0x007dcdf0, rx_node_set_local_rotation);
    if (set_rotation) { const uint32_t a[1] = {bone}; const rx_addr sp = loop_push(1, a, ret_rot); set_rotation(node, bone); loop_pop(sp); }
    else { rx_regs in = loop_regs(); const uint32_t a[1] = {bone}; in.r[LOOP_ESI] = node; loop_ex(0x007dcdf0, in, 1, a); }
    const rx_addr rec = U32(U32(node, 0), 0x18) + 0x40u * U8(node, 0xb);
    SU32(rec, 0x30, U32(bone, 0x10));
    SU32(rec, 0x34, U32(bone, 0x14));
    SU32(rec, 0x38, U32(bone, 0x18));
    loop_tc0r(0x007db7e0, node, ret_inv);   /* Node_Invalidate (rx_world) */
}

/* 0082ff20 (ECX = the animation context, EDI = skeleton): the skeleton's frame: the animator (+0x224) advances by dt (slot 3) and
 * evaluates into the pose (slot 4: skeleton, context, pose). Incremental skeletons (+0x1f4): only bones whose pose changed (+0x20 /
 * +0x21) go to their nodes. Else the bound pose copies (+0x1f8, bones +0x1fc, count +0x200) take their bones' transforms and every
 * bone not masked (skeleton + 4 i) goes to its node; then +0x204 (evaluated) is set. */
void rx_skeleton_animate(rx_addr ctx, rx_addr skel) {
    /* the original's frame: push ebp; and esp, -16; sub esp, 0x28; push ebx; push esi */
    const rx_addr sp0 = rx_sp();
    LA_ENTER(sp0);
    rx_set_sp(((sp0 - 4) & ~15u) - 0x30);
    const rx_addr pose = U32(skel, 0x228), animator = U32(skel, 0x224);
    loop_tc_f(loop_vslot(animator, 0xc), animator, F32(loop_timing(), 0x18));
    loop_tc3(loop_vslot(U32(skel, 0x224), 0x10), U32(skel, 0x224), skel, ctx, pose);
    SU8(skel, 0x204, 0);
    if (U8(skel, 0x1f4) != 0) {
        for (uint32_t i = 0; i < U32(skel, 0x234); i++) {
            const rx_addr bone = U32(skel, 0x228) + 0x30 * i;
            if (U8(bone, 0x21) != 0 || U8(bone, 0x20) != 0) bone_to_node(skel, i, bone, 0x0082ffb9, 0x0082fff6);
        }
        LA_LEAVE(); rx_set_sp(sp0);
        return;
    }
    for (int32_t j = 0; j < I32(skel, 0x200); j++) {
        const rx_addr src = U32(skel, 0x228) + 0x30u * U32(U32(skel, 0x1fc), 4u * (uint32_t)j);
        const rx_addr dst = U32(skel, 0x1f8) + 0x30u * (uint32_t)j;
        SU32(dst, 0x10, U32(src, 0x10));
        SU32(dst, 0x14, U32(src, 0x14));
        SU32(dst, 0x18, U32(src, 0x18));
        rx_wr64(U32(skel, 0x1f8) + 0x30u * (uint32_t)j, rx_rd64(src));
        rx_wr64(U32(skel, 0x1f8) + 0x30u * (uint32_t)j + 8, rx_rd64(src + 8));
    }
    for (uint32_t i = 0; i < U32(skel, 0x234); i++) {
        if (U32(skel, 4 * i) == 0) bone_to_node(skel, i, U32(skel, 0x228) + 0x30 * i, 0x008300c3, 0x00830100);
    }
    SU8(skel, 0x204, 1);
    LA_LEAVE(); rx_set_sp(sp0);
}
RX_FUNCTION(0x0082ff20, rx_skeleton_animate)

/* ---- the rider animation workers ---- */

/* 0061cdb0 RiderAnimation_WorkerThread (worker index): one wake (go 00af6c08 + 4 i): the worker's rider animation job (00a9408c
 * + 4 i), when it is running (+0x5bb5c): its skeleton (+0x144) animated (0082ff20); the head node's world matrix (hierarchy +0x2b4,
 * node +0xaeec) evaluated; the node wobble (00623c80) with the matrix's rows (row 0 and row 1: x, y and the fourth lane; row 2
 * negated), the fade (+0x5bb30 x 0.0340911, at most 1, x ModelFadeStart 009c4b4c) and rate (+0x5bb30 x 0.0681821 x 4), unless
 * the rider is off (+0x5b578, +0x5b1fc); the nod timer (+0x5b514 += +0x5b538, the nod 00623f30 once it reaches +0x5b518); the
 * pending job count (00a94080) decremented under its lock. A quit event (00af6b50 + 4 i) ends the thread (signals 00af6b80 + 4 i).
 * Returns the head node of the job it ran (0: none) and *quit when the thread ended (the oracle's thread glue uses both). */
rx_addr rx_rider_anim_worker_run(uint32_t i, uint32_t* quit) {
    *quit = 0;
    /* the original's frame: push ebp; and esp, -16; sub esp, 0x94; push ebx; push esi; push edi (S = A - 0xa0) */
    const rx_addr sp0 = rx_sp(), S = ((sp0 - 4) & ~15u) - 0xa0;
    LA_ENTER(sp0);
    rx_set_sp(S);
    loop_wait_event(U32(rx_rc_tab(0x00af6c08, 4, i), 0));
    const rx_addr job = U32(rx_rc_tab(0x00a9408c, 4, i), 0);
    SU32(S, 0x14, job);
    if (loop_wait_event_timeout(U32(rx_rc_tab(0x00af6b50, 4, i), 0), 0) != 0x102) {   /* quit */
        if (!loop_set_event(U32(rx_rc_tab(0x00af6b80, 4, i), 0))) loop_get_last_error();
        LA_LEAVE(); rx_set_sp(sp0);
        *quit = 1;
        return 0;
    }
    rx_addr node = 0;
    if (U32(job, 0x5bb5c) != 0) {
        { const rx_addr sp = loop_push(0, 0, 0x0061ce2e); rx_skeleton_animate(S + 0x90, U32(job, 0x144)); loop_pop(sp); }
        float fade = F32(job, 0x5bb30) * 0.034091063f /* 0x977abc */;
        if (fade > 1.0f /* 0x931f24 */) fade = 1.0f;
        fade = fade * F32(0, 0x009c4b4c);                       /* ModelFadeStart */
        const rx_addr hier = U32(job, 0x2b4);
        SU32(hier, 0x4c, 1);
        node = U32(U32(hier, 0x44), 0x18) + 16u * U32(job, 0xaeec);
        SF32(S, 0x18, fade);
        { const uint32_t a[1] = {0}; const rx_addr sp = loop_push(1, a, 0x0061cea0); rx_world_node_evaluate_world(node, 0); loop_pop(sp); }
        const rx_addr M = U32(U32(node, 0), 0x1c) + 0x40u * U8(node, 0xb), C = S + 0x50;
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(C, k, U32(M, k));
        const float minus1 = F32(0, 0x009cd9e8);
        SF32(S, 0x40, F32(C, 0x20) * minus1);
        SF32(S, 0x44, F32(C, 0x24) * minus1);
        SU32(S, 0x20, U32(C, 0x10)); SU32(S, 0x24, U32(C, 0x14)); SU32(S, 0x28, U32(C, 0x1c));
        SU32(S, 0x30, U32(C, 0x00)); SU32(S, 0x34, U32(C, 0x04)); SU32(S, 0x38, U32(C, 0x0c));
        SU32(hier, 0x4c, 0);
        const float rate = F32(job, 0x5bb30) * 0.068182126f /* 0x977ab8 */ * 4.0f /* 0x9322c0 */;
        SF32(S, 0x48, F32(C, 0x28) * minus1);
        SF32(S, 0x1c, rate);
        if (U8(job, 0x5b578) == 0 && U32(job, 0x5b1fc) == 0) {
            __typeof__(&rx_rider_anim_noise_wobble) wobble = LOOP_CORE(0x00623c80, rx_rider_anim_noise_wobble);
            if (wobble) {
                const uint32_t a[5] = {U32(job, 0x5b538), rx_fb(rate), rx_fb(fade), S + 0x40, S + 0x20};
                const rx_addr sp = loop_push(5, a, 0x0061cfd6);
                wobble(job, S + 0x30, F32(job, 0x5b538), rate, fade, S + 0x40, S + 0x20);
                loop_pop(sp);
            }
            else {
                rx_regs in = loop_regs();
                const uint32_t a[5] = {U32(job, 0x5b538), rx_fb(rate), rx_fb(fade), S + 0x40, S + 0x20};
                in.r[LOOP_ECX] = job; in.r[LOOP_EDX] = S + 0x30;
                loop_ex(0x00623c80, in, 5, a);
            }
        }
        const float t = F32(job, 0x5b514) + F32(job, 0x5b538);
        SF32(job, 0x5b514, t);
        if (t >= F32(job, 0x5b518)) {
            const uint32_t a[1] = {job}; const rx_addr sp = loop_push(1, a, 0x0061d009); rx_rider_anim_nod(job); loop_pop(sp);
        }
        loop_enter_cs(0x00af6c98);
        SU32(0, 0x00a94080, U32(0, 0x00a94080) - 1);
        loop_leave_cs(0x00af6c98);
    }
    LA_LEAVE(); rx_set_sp(sp0);
    return node;
}
RX_FUNCTION(0x0061cdb0, rx_rider_anim_worker_run)
