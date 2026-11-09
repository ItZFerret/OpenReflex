/* loop_rider.c -- the procedural rider limb helpers the limb animators of rx_anim call (anim_rider_b.c, anim_ik_anim.c): a bone's
 * direction to the limb target in the bone's frame (00632b00), the reach contact probe (00632dc0), the contact reach (00633010),
 * the release (00635e10) and 00636990.
 *
 * Limb state L (rx_anim's ProceduralAnimator +0x5c / +0x60, anim.md "Rider limb state"): +0x20 / +0x24 / +0x28 the three bones
 * (hand or foot, elbow or knee, shoulder or hip), +0x30 the target, +0xbc the rider animation (+0x1b4 the rider, its +0x144 the
 * character the pose belongs to), +0xc0 the reach extension. A bone's model transform comes from SkeletonPose_BoneToModelScalar
 * (0082e610, loop_anim.c) into a stack pose record {identity rotation, zero translation (0x00a95d50), flags 0}. */
#include "loop_rider.h"
#include "loop_anim.h"
#include "loop_cam.h"
#include "loop_util.h"
#include "loop_extern.h"
#include "anim_math.h"

/* the bone's model transform into the stack record R (the original's call: EAX = pose, ECX = bone, EDI = R, stdcall character) */
static void rider_bone_model(rx_addr character, uint32_t bone, rx_addr pose, rx_addr R, rx_addr ret) {
    const uint32_t a[1] = {character};
    const rx_addr s = loop_push(1, a, ret);
    rx_skeleton_bone_to_model_scalar(character, bone, pose, R);
    loop_pop(s);
}
/* a stack pose record: the identity rotation (0x9cb550), the zero translation (0x00a95d50), both flags clear */
static void rider_record_init(rx_addr R) {
    for (uint32_t o = 0; o < 16; o += 4) SU32(R, o, U32(0, 0x009cb550 + o));
    SU32(R, 0x10, U32(0, 0x00a95d50));
    SU32(R, 0x14, U32(0, 0x00a95d54));
    SU32(R, 0x18, U32(0, 0x00a95d58));
    SU8(R, 0x20, 0);
    SU8(R, 0x21, 0);
}

/* 00632b00 (LTCG: ESI = out vec3; stdcall limb state, pose, bone; ret 0xc): the direction from the bone to the limb's target
 * (+0x30) in the bone's frame: the target minus the bone's model position, times the transpose of the bone's rotation matrix
 * (from its model quaternion), normalized (unless zero). Frame: push ebp; and esp, -16; sub esp, 0x98; push ebx / edi: the pose
 * record at the aligned base - 0x70, the matrix entries kept at - 0x3c.. - 0x18. */
void rx_rider_bone_target_dir(rx_addr out, rx_addr st, rx_addr pose, uint32_t bone) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, R = A - 0x70u;
    rx_set_sp(A - 0xa0u);
    const rx_addr character = U32(U32(U32(st, 0xbc), 0x1b4), 0x144);
    SU8(A, -0x50, 0); SU8(A, -0x4f, 0);
    for (uint32_t o = 0; o < 16; o += 4) SU32(R, o, U32(0, 0x009cb550 + o));
    SU32(A, -0x60, U32(0, 0x00a95d50));
    SU32(A, -0x5c, U32(0, 0x00a95d54));
    SU32(A, -0x58, U32(0, 0x00a95d58));
    rider_bone_model(character, bone, pose, R, 0x00632b75);
    const float x = F32(R, 0), y = F32(R, 4), z = F32(R, 8), w = F32(R, 0xc);
    const float x2 = x * 2.0f /* 0x92e330 */;
    const float wx = w * x2, xx = x * x2;
    SF32(A, -0x90, wx);
    const float y2 = y * 2.0f;
    const float wy = w * y2;
    SF32(A, -0x88, wy);
    const float z2 = z * 2.0f;
    SF32(A, -0x84, xx);
    const float xy = y * x2;
    const float wz = w * z2;
    const float zz = z * z2;
    const float xz = z * x2;
    const float yy = y * y2;
    SF32(A, -0x8c, wz);
    const float m00 = 1.0f /* 0x931f24 */ - (zz + yy);
    const float m01 = xy + wz;
    SF32(A, -0x3c, m01);
    const float m10 = xy - wz;
    const float yz = z * y2;
    const float m02 = xz - F32(A, -0x88);
    SF32(A, -0x38, m02);
    const float m20 = xz + F32(A, -0x88);
    const float m11 = 1.0f - (zz + F32(A, -0x84));
    SF32(A, -0x2c, m11);
    const float m12 = yz + F32(A, -0x90);
    SF32(A, -0x28, m12);
    const float m21 = yz - F32(A, -0x90);
    const float m22 = 1.0f - (yy + F32(A, -0x84));
    SF32(A, -0x20, m20); SF32(A, -0x1c, m21); SF32(A, -0x18, m22);
    const float dy = F32(st, 0x34) - F32(A, -0x5c);
    const float dz = F32(st, 0x38) - F32(A, -0x58);
    const float dx = F32(st, 0x30) - F32(A, -0x60);
    const float X = ((dz * F32(A, -0x38)) + (dy * F32(A, -0x3c))) + (dx * m00);
    const float Z = ((dz * F32(A, -0x18)) + (dy * F32(A, -0x1c))) + (F32(A, -0x20) * dx);
    const float Y = ((dz * F32(A, -0x28)) + (dy * F32(A, -0x2c))) + (m10 * dx);
    SF32(A, -0x78, Z);
    SF32(out, 8, Z);
    const float l = ((Z * Z) + (Y * Y)) + (X * X);
    SF32(A, -0x80, X); SF32(A, -0x7c, Y);
    SF32(out, 0, X); SF32(out, 4, Y);
    SF32(A, -0x90, l);
    if (l > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f / rx_sqrts(l);
        SF32(out, 0, F32(A, -0x80) * inv);
        SF32(out, 4, F32(A, -0x7c) * inv);
        SF32(out, 8, F32(A, -0x78) * inv);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00632b00, rx_rider_bone_target_dir)

/* 007c6cc0 TerrainSegmentQuery (ESI = terrain, ECX = start, EAX = end; stdcall hit, normal) -> EAX (rx_phys) */
static uint32_t rider_segment_query(rx_addr world, rx_addr start, rx_addr end, rx_addr hit, rx_addr normal, uint32_t edx, rx_addr ret) {
    const uint32_t a[2] = {hit, normal};
    uint32_t regs[2] = {start, edx};
    const rx_addr s = loop_push(2, a, ret);
    const uint32_t r = phys_terrain_segment_query(world, start, end, hit, normal, regs);
    loop_pop(s);
    return r;
}

/* 00632dc0 (LTCG: EAX = limb state; stdcall pose, hit out, normal out; ret 0xc) -> EAX: the reach contact probe: from the middle
 * bone's (+0x24) model position along the direction to the end bone (+0x20) for the bones' distance plus the reach extension
 * +0xc0, a terrain segment query (007c6cc0 in the world of the registry 009e30d4) into hit / normal; EAX = its result. Frame:
 * push ebp; and esp, -16; sub esp, 0xa4; push ebx / esi / edi: the two pose records at the aligned base - 0x70 and - 0x30. */
uint32_t rx_rider_reach_probe(rx_addr st, rx_addr pose, rx_addr hit, rx_addr normal) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, R1 = A - 0x70u, R2 = A - 0x30u;
    rx_set_sp(A - 0xb0u);
    const rx_addr character = U32(U32(U32(st, 0xbc), 0x1b4), 0x144);
    rider_record_init(R1);
    rider_record_init(R2);
    rider_bone_model(character, U32(st, 0x24), pose, R1, 0x00632e69);
    rider_bone_model(character, U32(st, 0x20), pose, R2, 0x00632e7c);
    const float p1x = F32(R1, 0x10), p1y = F32(R1, 0x14), p1z = F32(R1, 0x18);
    const float dx = F32(R2, 0x10) - p1x;
    SF32(A, -0x40, p1x);
    const float dy = F32(R2, 0x14) - p1y;
    SF32(A, -0x3c, p1y);
    const float dz = F32(R2, 0x18) - p1z;
    SF32(A, -0x38, p1z);
    SF32(A, -0x78, dz); SF32(A, -0x98, dz);
    SF32(A, -0x7c, dy); SF32(A, -0x9c, dy);
    const float l = ((dz * dz) + (dy * dy)) + (dx * dx);
    SF32(A, -0xa4, l);
    SF32(A, -0x80, dx); SF32(A, -0xa0, dx);
    const float len = rx_sqrts(l) + F32(st, 0xc0);
    SF32(A, -0x84, len);
    float ux, uy, uz;
    if (l > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
        ux = F32(A, -0x80) * inv; uy = F32(A, -0x7c) * inv; uz = F32(A, -0x78) * inv;
    } else {
        ux = F32(A, -0xa0); uy = F32(A, -0x9c); uz = F32(A, -0x98);
    }
    const float ex = (ux * len) + F32(R1, 0x10);
    const float ey = (uy * len) + F32(R1, 0x14);
    const float ez = (uz * len) + F32(R1, 0x18);
    SF32(A, -0xa0, ex); SF32(A, -0x9c, ey); SF32(A, -0x98, ez);
    if (!(U8(0, 0x009e30e0) & 1)) {
        SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1);
        SU32(0, 0x009e30dc, 0);
    }
    const rx_addr world = cam_registry_entry(0x009e30d4, 0x00632fef);
    const uint32_t r = rider_segment_query(world, A - 0x40u, A - 0xa0u, hit, normal, normal, 0x00633006);
    rx_set_sp(sp0);
    return r;
}
RX_FUNCTION(0x00632dc0, rx_rider_reach_probe)

/* ---- 00636990: the rider's twist stage ---- */

/* The inlined sine / cosine of 00636990 (anim_math.h's range reduction and polynomials; the negative sign is the .data -1.0 at
 * 0x009cd9e8): sign = a >= 0 ? 1 : -1, t = pi - (|a| - trunc(|a| / 2pi) * 2pi), cos = -0 - cospoly(t), sin = sinpoly(t) * t * sign. */
static void stage_sincos(float a, float* s, float* c) {
    const float sg = a >= 0.0f ? 1.0f /* 0x931f24 */ : F32(0, 0x009cd9e8);
    const float t = anim_trig_reduce(rx_muls(sg, a));
    const float t2 = t * t, t4 = t2 * t2;
    *c = rx_subs(-0.0f /* 0x9774a8 */, anim_cos_poly(t2, t4));
    *s = rx_muls(anim_sin_poly(t2, t4) * t, sg);
}
/* the 4x4 rotation matrix about x (rows 0x10 apart: 1 0 0 0 / 0 c s 0 / 0 -s c 0 / 0 0 0 1) or z (c s 0 0 / -s c 0 0 / 0 0 1 0 /
 * 0 0 0 1) as the original stores it on the stack, then Matrix3_ToQuaternionToEsi (00401740, ESI = out; cdecl matrix) */
static void stage_axis_quat(rx_addr M, int about_z, float s, float c, rx_addr q, rx_addr ret, uint32_t ebp) {
    const float e[16] = {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, c, s, 0.0f, 0.0f, -0.0f - s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    const float ez[16] = {c, s, 0.0f, 0.0f, -0.0f - s, c, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
    for (uint32_t i = 0; i < 16; i++) SF32(M, 4 * i, about_z ? ez[i] : e[i]);
    if (about_z) SF32(M, 0x10, rx_subs(-0.0f, s));
    else SF32(M, 0x24, rx_subs(-0.0f, s));
    const uint32_t a[1] = {M};
    const rx_addr sp = loop_push(1, a, ret);
    rx_world_matrix3_to_quaternion_to_fr(q, M, ebp);   /* its frame below ours: the animation worker's dead stack (below) */
    loop_pop(sp);
}
/* the bone rotation b (pose record +0x00 x y z w) times q, in double precision with the original's term order, stored at R
 * (x y z w) and copied to the record: w = bw qw - bx qx - by qy - bz qz, x = (bw qx + bx qw) + by qz - bz qy,
 * y = (by qw + bw qy) + bz qx - bx qz, z = (bz qw + bw qz) + bx qy - by qx */
static void stage_rotate_bone(rx_addr rec, float qx, float qy, float qz, float qw, rx_addr R) {
    const double bx = F32(rec, 0), by = F32(rec, 4), bz = F32(rec, 8), bw = F32(rec, 0xc);
    const double x = qx, y = qy, z = qz, w = qw;
    SF32(R, 0xc, (float)(((bw * w - bx * x) - by * y) - bz * z));
    SF32(R, 0x0, (float)(((bw * x + bx * w) + by * z) - bz * y));
    SF32(R, 0x4, (float)(((by * w + bw * y) + bz * x) - bx * z));
    SF32(R, 0x8, (float)(((bz * w + bw * z) + bx * y) - by * x));
    for (uint32_t o = 0; o < 16; o += 4) SU32(rec, o, U32(R, o));
}

/* 00636990 (EDI = pose; stack stage, value (the stage's +0x18, through the x87); RET 8): for a positive value the rider's twist:
 * the bone +0x08 turns about x by value * (.data 0x009d78d8, -15 at run time) degrees (* 0x977678 = pi / 180); the bone +0x0c by
 * the product of the turn about z by value * (.data 0x009d78dc, 50) degrees and the turn about x by value * 10 degrees (q3 x q2
 * in the original's term order, single precision results). Pose records 0x30 bytes per bone. Frame: push ebp; and esp, -16;
 * sub esp, 0xa8; push ebx / esi: the matrices at +0x30 / +0x70, the quaternions at +0x10 / +0x20, the products at +0x20. */
void rx_rider_stage_twist_fr(rx_addr stage, float value, rx_addr pose, uint32_t ebx, uint32_t ebp, uint32_t esi) {
    /* the frame is written as the original's (push ebp; push ebx / esi before the test; the three Matrix3_ToQuaternion frames
     * below it): the animation worker's stack keeps it as dead stack that 006b0530 reads back as the rider record's never-written
     * w lane (rx_loop's Engine::Update Verify residue before: Concord call 1697 [152414cc], SX 1375, Terracross 1201) */
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, S = A - 0xb0u, fp = sp0 - 4u;
    SU32(sp0, -4, ebp); SU32(A, -0xac, ebx); SU32(A, -0xb0, esi);
    if (!(value > 0.0f)) return;
    rx_set_sp(S);
    const float k = 0.017453291f /* 0x977678 */;
    float s, c;
    /* bone +0x08: about x */
    stage_sincos(value * F32(0, 0x009d78d8) * k, &s, &c);
    stage_axis_quat(S + 0x30, 0, s, c, S + 0x10, 0x00636b3e, fp);
    stage_rotate_bone(pose + U32(stage, 8) * 0x30u, F32(S, 0x10), F32(S, 0x14), F32(S, 0x18), F32(S, 0x1c), S + 0x20);
    /* bone +0x0c: about z (q2), then about x (q3) */
    stage_sincos(value * F32(0, 0x009d78dc) * k, &s, &c);
    stage_axis_quat(S + 0x70, 1, s, c, S + 0x20, 0x00636e53, fp);
    stage_sincos(value * 10.0f /* 0x92fd88 */ * k, &s, &c);
    stage_axis_quat(S + 0x30, 0, s, c, S + 0x10, 0x00636fdd, fp);
    {
        const double ax = F32(S, 0x20), ay = F32(S, 0x24), az = F32(S, 0x28), aw = F32(S, 0x2c);   /* q2 */
        const double bx = F32(S, 0x10), by = F32(S, 0x14), bz = F32(S, 0x18), bw = F32(S, 0x1c);   /* q3 */
        const float pw = (float)(((aw * bw - ax * bx) - ay * by) - az * bz);
        const float px = (float)(((bw * ax + aw * bx) + az * by) - ay * bz);
        const float py = (float)(((aw * by + ay * bw) + bz * ax) - az * bx);
        const float pz = (float)(((aw * bz + az * bw) + ay * bx) - by * ax);
        const rx_addr rec = pose + U32(stage, 0xc) * 0x30u;
        const double rx = F32(rec, 0), ry = F32(rec, 4), rz = F32(rec, 8), rw = F32(rec, 0xc);
        const double w = pw, x = px, y = py, z = pz;
        SF32(S, 0x2c, (float)(((w * rw - x * rx) - y * ry) - z * rz));
        SF32(S, 0x20, (float)(((w * rx + x * rw) + z * ry) - y * rz));
        SF32(S, 0x24, (float)(((w * ry + y * rw) + x * rz) - z * rx));
        SF32(S, 0x28, (float)(((w * rz + z * rw) + y * rx) - x * ry));
        for (uint32_t o = 0; o < 16; o += 4) SU32(rec, o, U32(S, 0x20 + o));
    }
    rx_set_sp(sp0);
}
void rx_rider_stage_twist(rx_addr stage, float value, rx_addr pose) { rx_rider_stage_twist_fr(stage, value, pose, 0, 0, 0); }
RX_FUNCTION(0x00636990, rx_rider_stage_twist)

/* ---- shared pieces of the contact solvers (00635e10 / 00633010) ---- */

/* acos(d) by the double-precision rational the limb solvers inline (anim_math.h anim_acos_approx, with the .data threshold
 * 0x009ddfc8 = 0.7071 read from memory): |d| at or above it works on sqrt(|1 - d^2|) and takes the complement, a negative (or
 * unordered) d mirrors, acos = pi/2 - that. */
static float limb_acos(float d) {
    const float ad = fabsf(d), lim = F32(0, 0x009ddfc8);
    float u = ad;
    if (ad >= lim) u = rx_sqrts(fabsf((float)(1.0 /* 0x977638 */ - (double)(float)((double)d * (double)d))));
    const float u2 = (float)((double)u * (double)u);
    const float u4 = (float)((double)u2 * (double)u2);
    const float p = (float)(0.5648093223571777 /* 0x9777b0 */ - (double)u2 * 0.04992850869894028 /* 0x9777a8 */);
    const float q = (float)(1.0 - (double)u2 * 1.4430354833602905 /* 0x9777a0 */);
    const float inv = (float)(1.0 / ((double)p * (double)u4 + (double)q));
    const float n1 = (float)(0.3993034064769745 /* 0x977798 */ - (double)u2 * 0.0193784236907959 /* 0x977790 */);
    const float n2 = (float)(1.0 - (double)u2 * 1.2763687372207642 /* 0x977788 */);
    const float num = (float)(((double)n1 * (double)u4 + (double)n2) * (double)u);
    float r = (float)((double)inv * (double)num);
    if (ad >= lim) r = (float)(1.5707963705062866 /* 0x977510 */ - (double)r);
    if (!(d >= 0.0f)) r = rx_subs(-0.0f, r);
    return (float)(1.5707963705062866 - (double)r);
}
/* the rotation matrix of a unit quaternion (x y z w), as the solvers expand it: m[row][col] */
static void limb_quat_matrix(float x, float y, float z, float w, float m[3][3]) {
    const float x2 = x * 2.0f /* 0x92e330 */, wx = w * x2, xx = x * x2;
    const float y2 = y * 2.0f, wy = w * y2;
    const float z2 = z * 2.0f, xy = y * x2, wz = w * z2, zz = z * z2, yy = y * y2, xz = z * x2, yz = z * y2;
    m[0][0] = 1.0f - (zz + yy); m[0][1] = xy + wz;           m[0][2] = xz - wy;
    m[1][0] = xy - wz;          m[1][1] = 1.0f - (zz + xx);  m[1][2] = yz + wx;
    m[2][0] = xz + wy;          m[2][1] = yz - wx;           m[2][2] = 1.0f - (yy + xx);
}
/* the rotation about the axis b (not normalized) by the angle whose sine / cosine are s / c (t = 1 - c), stored as the 4x4
 * stack matrix M (rows 0x10 apart) and turned into the quaternion q (Matrix3_ToQuaternionToEsi 00401740, ESI = q) */
static void limb_axis_quat(rx_addr M, float bx, float by, float bz, float s, float c, rx_addr q, rx_addr ret) {
    const float sz = s * bz, t = 1.0f /* 0x931f24 */ - c, sy = s * by, sx = s * bx;
    const float tx = t * bx, ty = t * by, tz = t * bz;
    const float e[16] = {tx * bx + c, tx * by + sz, tx * bz - sy, 0.0f,
                         ty * bx - sz, ty * by + c, ty * bz + sx, 0.0f,
                         tz * bx + sy, tz * by - sx, tz * bz + c, 0.0f,
                         0.0f, 0.0f, 0.0f, 1.0f};
    for (uint32_t i = 0; i < 16; i++) SF32(M, 4 * i, e[i]);
    const uint32_t a[1] = {M};
    const rx_addr sp = loop_push(1, a, ret);
    rx_world_matrix3_to_quaternion_to(q, M);
    loop_pop(sp);
}
/* the pose bone record turned by the quaternion q (at q: x y z w): R = q x bone in double precision with the original's term
 * order (w = ((qw bw - qx bx) - qy by) - qz bz, x = ((qw bx + qx bw) + qz by) - qy bz, y = ((qw by + qy bw) + qx bz) - qz bx,
 * z = ((qw bz + qz bw) + qy bx) - qx by), stored, renormalized in double precision (zero length: zero) and marked valid (+0x20) */
static void limb_turn_bone(rx_addr rec, rx_addr q) {
    const double qx = F32(q, 0), qy = F32(q, 4), qz = F32(q, 8), qw = F32(q, 0xc);
    const double bx = F32(rec, 0), by = F32(rec, 4), bz = F32(rec, 8), bw = F32(rec, 0xc);
    const float w = (float)(((qw * bw - qx * bx) - qy * by) - qz * bz);
    const float x = (float)(((qw * bx + qx * bw) + qz * by) - qy * bz);
    const float y = (float)(((qw * by + qy * bw) + qx * bz) - qz * bx);
    const float z = (float)(((qw * bz + qz * bw) + qy * bx) - qx * by);
    SF32(rec, 0, x); SF32(rec, 4, y); SF32(rec, 8, z); SF32(rec, 0xc, w);
    const float len2 = (float)((((double)x * x + (double)w * w) + (double)y * y) + (double)z * z);
    if (len2 == 0.0f) {
        for (uint32_t o = 0; o < 16; o += 4) SU32(rec, o, 0);
    } else {
        const float inv = 1.0f / rx_sqrts(len2);
        SF32(rec, 0xc, (float)((double)inv * (double)F32(rec, 0xc)));
        SF32(rec, 0, (float)((double)inv * (double)F32(rec, 0)));
        SF32(rec, 4, (float)((double)F32(rec, 4) * (double)inv));
        SF32(rec, 8, (float)((double)inv * (double)F32(rec, 8)));
    }
    SU8(rec, 0x20, 1);
}

/* 00635e10 RiderLimb_Release (stdcall limb state, pose; RET 8): while the contact timer +0xd0 runs out, the end bone (+0x20)
 * turns back from the contact point (+0xe0): the angle between the bone's z axis (its model rotation, 0082e610) and the
 * direction to the negated contact point, times the release fraction ((+0xcc - +0xd0) (0x00d94ab8 - 0x009cc8a8) / +0xcc +
 * 0x009cc8a8), about the cross product of the two (taken into the bone's frame). Frame: push ebp; and esp, -16; sub esp, 0x74;
 * push ebx / esi / edi: the pose record (then the matrix) at +0x40, the quaternion at +0x20. */
void rx_rider_limb_release(rx_addr st, rx_addr pose) {
    const rx_addr sp0 = rx_sp(), S = ((sp0 - 4u) & ~0xfu) - 0x74u - 12u, R = S + 0x40;
    rx_set_sp(S);
    const float lo = F32(0, 0x009cc8a8), cc = F32(st, 0xcc);
    const float f = rx_adds(rx_divs(rx_muls(rx_subs(cc, F32(st, 0xd0)), rx_subs(F32(0, 0x00d94ab8), lo)), cc), lo);
    rider_record_init(R);
    rider_bone_model(U32(U32(U32(st, 0xbc), 0x1b4), 0x144), U32(st, 0x20), pose, R, 0x00635ebc);
    float m[3][3];
    limb_quat_matrix(F32(R, 0), F32(R, 4), F32(R, 8), F32(R, 0xc), m);
    const float vx = rx_subs(-0.0f, F32(st, 0xe0)), vy = rx_subs(-0.0f, F32(st, 0xe4)), vz = rx_subs(-0.0f, F32(st, 0xe8));
    const float dot = (vy * m[2][1] + vx * m[2][0]) + vz * m[2][2];
    const float a = rx_muls(limb_acos(dot), f);
    /* the axis v x (row 2), normalized when not zero, then into the bone frame (the matrix rows) */
    float ax = vz * m[2][1] - vy * m[2][2];
    float ay = vx * m[2][2] - vz * m[2][0];
    float az = vy * m[2][0] - vx * m[2][1];
    const float l2 = (ay * ay + ax * ax) + az * az;
    if (l2 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l2);
        ax = ax * inv; ay = ay * inv; az = az * inv;
    }
    const float bx = (m[0][1] * ay + m[0][0] * ax) + m[0][2] * az;
    const float by = (m[1][0] * ax + m[1][1] * ay) + m[1][2] * az;
    const float bz = (ax * m[2][0] + ay * m[2][1]) + az * m[2][2];
    float s, c;
    stage_sincos(a, &s, &c);
    limb_axis_quat(S + 0x40, bx, by, bz, s, c, S + 0x20, 0x00636538);
    limb_turn_bone(pose + U32(st, 0x20) * 0x30u, S + 0x20);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00635e10, rx_rider_limb_release)

/* ---- 00633010 RiderLimb_ReachContact: the two-bone reach to a contact point ---- */

static float limb_len2(float x, float y, float z) { return (y * y + x * x) + z * z; }

/* 00633010 RiderLimb_ReachContact (stdcall limb state, pose, contact point, contact normal; RET 0x10): the limb (root +0x28,
 * middle +0x24, end +0x20; their model transforms from 0082e610) reaches the contact point: the end bone's reach E = middle +
 * the middle -> end direction * (its length + the extension +0xc0); the triangle root (P3) - middle (K) - contact with the side
 * lengths |middle - root| and |E - middle| puts the middle at K, offset from the root -> contact line toward the .data down
 * vector (0x009cb4c0, negated) taken into the middle bone's frame (no triangle: nothing changes). Then
 *   the root turns by the angle between (K - P3) and its current middle direction about their cross product (in its frame: Q);
 *   the middle turns so that the reach point, carried by Q, points from K to the contact (W);
 *   the end bone turns its z axis, carried by Q and W, toward the negated contact normal by the release fraction
 *   ((+0xc8 - +0xd0) 0.75 / +0xc8 + 0.25, 1 while +0xd0 < 0).
 * Each turn is the axis-angle matrix (axis in the bone's frame) -> quaternion (00401740) -> bone (x) q in double precision,
 * renormalized, rotation flag set. Inlined: the arc cosine (limb_acos), sine / cosine (stage_sincos); x87 (53-bit, the rider
 * animation worker's precision): the root -> middle length squared, computed in double here (same rounding). Frame: push ebp;
 * and esp, -16; sub esp, 0x1c4; push ebx / esi / edi: pose records at +0xe0 (middle), +0x160 (end), +0x120 (root), matrices at
 * +0x80, the quaternion at +0x20. */
void rx_rider_limb_reach_contact(rx_addr st, rx_addr pose, rx_addr contact, rx_addr normal) {
    const rx_addr sp0 = rx_sp(), S = ((sp0 - 4u) & ~0xfu) - 0x1d0u;
    const rx_addr R1 = S + 0xe0, R2 = S + 0x160, R3 = S + 0x120, M = S + 0x80, Q4 = S + 0x20;
    rx_set_sp(S);
    const rx_addr character = U32(U32(U32(st, 0xbc), 0x1b4), 0x144);
    rider_record_init(R1); rider_record_init(R2); rider_record_init(R3);
    rider_bone_model(character, U32(st, 0x24), pose, R1, 0x006330ff);
    rider_bone_model(character, U32(st, 0x20), pose, R2, 0x00633112);
    rider_bone_model(character, U32(st, 0x28), pose, R3, 0x00633125);
    const float p1x = F32(R1, 0x10), p1y = F32(R1, 0x14), p1z = F32(R1, 0x18);   /* middle */
    const float p3x = F32(R3, 0x10), p3y = F32(R3, 0x14), p3z = F32(R3, 0x18);   /* root */
    /* the reach point E along middle -> end */
    const float d12x = F32(R2, 0x10) - p1x, d12y = F32(R2, 0x14) - p1y, d12z = F32(R2, 0x18) - p1z;
    const float d31x = p1x - p3x, d31y = p1y - p3y, d31z = p1z - p3z;
    const float l12 = (d12x * d12x + d12y * d12y) + d12z * d12z;
    const float len = rx_adds(rx_sqrts(l12), F32(st, 0xc0));
    float ux = d12x, uy = d12y, uz = d12z;
    if (l12 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l12);
        ux = inv * d12x; uy = d12y * inv; uz = d12z * inv;
    }
    const float ex = ux * len + p1x, ey = uy * len + p1y, ez = uz * len + p1z;
    const float l31 = (float)(((double)d31x * d31x + (double)d31y * d31y) + (double)d31z * d31z);   /* x87, 53 bits */
    const float L31 = rx_sqrts(l31);
    /* the root -> contact direction (no length: the middle -> end vector stays there, as the original's stack slot does) */
    const float dax = F32(contact, 0) - p3x, day = F32(contact, 4) - p3y, daz = F32(contact, 8) - p3z;
    const float la = (dax * dax + day * day) + daz * daz;
    float nax = d12x, nay = d12y, naz = d12z;
    if (la > 0.0f) {
        const float inv = 1.0f / rx_sqrts(la);
        nax = dax * inv; nay = day * inv; naz = daz * inv;
    }
    /* the triangle: x along the root -> contact line, h off it */
    const float La = rx_sqrts(la);
    float x = rx_divs((((La * La) - (len * len)) + (L31 * L31)) * 0.5f /* 0x9322cc */, La), h;
    if (x > L31 || (La - len) > x) {
        h = 0.0f;
        x = L31;
    } else {
        const float hp = (((L31 - (La + len)) * ((len - La) - L31)) * ((len - La) + L31)) * ((La + len) + L31);
        if (0.0f >= hp) { rx_set_sp(sp0); return; }
        h = rx_divs(rx_sqrts(hp) * 0.5f, La);
    }
    float m[3][3];
    limb_quat_matrix(F32(R1, 0), F32(R1, 4), F32(R1, 8), F32(R1, 0xc), m);           /* the middle bone's frame */
    float px, py, pz;
    {   /* the side the middle goes to: the negated down vector in the middle's frame, perpendicular to the contact line */
        const float vx = rx_subs(-0.0f, F32(0, 0x009cb4c0)), vy = rx_subs(-0.0f, F32(0, 0x009cb4c4)), vz = rx_subs(-0.0f, F32(0, 0x009cb4c8));
        const float c0 = (vx * m[0][0] + vy * m[1][0]) + vz * m[2][0];
        const float c1 = (vy * m[1][1] + m[0][1] * vx) + vz * m[2][1];
        const float c2 = (vy * m[1][2] + m[0][2] * vx) + vz * m[2][2];
        const float dd = (c0 * nax + nay * c1) + naz * c2;
        px = c0 - nax * dd; py = c1 - nay * dd; pz = c2 - naz * dd;
        const float lp = (py * py + px * px) + pz * pz;
        if (lp > 0.0f) {
            const float inv = 1.0f / rx_sqrts(lp);
            px = px * inv; py = py * inv; pz = pz * inv;
        }
    }
    /* the middle's new place K, and its direction from the root */
    const float kx = (nax * x + p3x) + px * h, ky = (nay * x + p3y) + py * h, kz = (naz * x + p3z) + pz * h;
    float Dx = nax * x + px * h, Dy = nay * x + py * h, Dz = naz * x + pz * h;
    {
        const float l = (Dy * Dy + Dx * Dx) + Dz * Dz;
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            Dx = Dx * inv; Dy = Dy * inv; Dz = Dz * inv;
        }
    }
    float n31x = d31x, n31y = d31y, n31z = d31z;
    if (l31 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l31);
        n31y = d31y * inv; n31z = d31z * inv; n31x = inv * d31x;
    }
    /* the root's turn: from its middle direction to D */
    float s, c, Q[3][3];
    {
        const float theta = limb_acos((Dx * n31x + Dy * n31y) + Dz * n31z);
        float a0 = Dz * n31y - Dy * n31z, a1 = n31z * Dx - Dz * n31x, a2 = Dy * n31x - n31y * Dx;
        const float l = (a1 * a1 + a0 * a0) + a2 * a2;
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            a0 = a0 * inv; a1 = a1 * inv; a2 = a2 * inv;
        }
        float r[3][3];
        limb_quat_matrix(F32(R3, 0), F32(R3, 4), F32(R3, 8), F32(R3, 0xc), r);      /* the root bone's frame */
        stage_sincos(theta, &s, &c);
        {   /* Q: the turn about the world axis (it carries the reach point and the end bone's axis below) */
            const float sa2 = s * a2, sa1 = s * a1, t = 1.0f - c, sa0 = s * a0;
            const float t0 = t * a0, t1 = t * a1, t2 = t * a2;
            Q[0][0] = t0 * a0 + c; Q[0][1] = t0 * a1 + sa2; Q[0][2] = t0 * a2 - sa1;
            Q[1][0] = t1 * a0 - sa2; Q[1][1] = t1 * a1 + c; Q[1][2] = t1 * a2 + sa0;
            Q[2][0] = t2 * a0 + sa1; Q[2][1] = t2 * a1 - sa0; Q[2][2] = t2 * a2 + c;
        }
        const float b0 = (r[0][1] * a1 + r[0][0] * a0) + r[0][2] * a2;
        const float b1 = (r[1][0] * a0 + r[1][1] * a1) + r[1][2] * a2;
        const float b2 = (r[2][0] * a0 + r[2][1] * a1) + r[2][2] * a2;
        limb_axis_quat(M, b0, b1, b2, s, c, Q4, 0x00634254);
        limb_turn_bone(pose + U32(st, 0x28) * 0x30u, Q4);
    }
    /* the middle's turn: the reach point carried by Q, seen from K, toward the contact */
    float W[3][3];
    {
        const float e0 = ex - p3x, e1 = ey - p3y, e2 = ez - p3z;
        const float f0 = (e0 * Q[0][0] + e1 * Q[1][0]) + e2 * Q[2][0];
        const float f1 = (e0 * Q[0][1] + e1 * Q[1][1]) + e2 * Q[2][1];
        const float f2 = (e0 * Q[0][2] + e1 * Q[1][2]) + e2 * Q[2][2];
        float gx = (f0 + p3x) - kx, gy = (f1 + p3y) - ky, gz = (f2 + p3z) - kz;
        float hx = F32(contact, 0) - kx, hy = F32(contact, 4) - ky, hz = F32(contact, 8) - kz;
        float l = limb_len2(gx, gy, gz);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            gx = gx * inv; gy = inv * gy; gz = inv * gz;
        }
        l = limb_len2(hx, hy, hz);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            hx = hx * inv; hy = inv * hy; hz = inv * hz;
        }
        const float theta = limb_acos((hy * gy + hx * gx) + hz * gz);
        float a0 = hz * gy - hy * gz, a1 = hx * gz - gx * hz, a2 = gx * hy - hx * gy;
        const float la2 = (a1 * a1 + a0 * a0) + a2 * a2;
        if (la2 > 0.0f) {
            const float inv = 1.0f / rx_sqrts(la2);
            a0 = a0 * inv; a1 = a1 * inv; a2 = a2 * inv;
        }
        stage_sincos(theta, &s, &c);
        {
            const float sa2 = s * a2, sa1 = s * a1, t = 1.0f - c, sa0 = s * a0;
            const float t0 = t * a0, t1 = t * a1, t2 = t * a2;
            W[0][0] = t0 * a0 + c; W[0][1] = t0 * a1 + sa2; W[0][2] = t0 * a2 - sa1;
            W[1][0] = t1 * a0 - sa2; W[1][1] = t1 * a1 + c; W[1][2] = t1 * a2 + sa0;
            W[2][0] = t2 * a0 + sa1; W[2][1] = t2 * a1 - sa0; W[2][2] = t2 * a2 + c;
        }
        const float b0 = (m[0][1] * a1 + m[0][0] * a0) + m[0][2] * a2;
        const float b1 = (m[1][0] * a0 + m[1][1] * a1) + m[1][2] * a2;
        const float b2 = (m[2][0] * a0 + m[2][1] * a1) + m[2][2] * a2;
        limb_axis_quat(M, b0, b1, b2, s, c, Q4, 0x00635015);
        limb_turn_bone(pose + U32(st, 0x24) * 0x30u, Q4);
    }
    /* the end bone's turn: its z axis (carried by Q and W) toward the negated contact normal, by the release fraction */
    {
        const float cc8 = F32(st, 0xc8), d0 = F32(st, 0xd0);
        float frac = rx_adds(rx_divs(rx_subs(cc8, d0) * 0.75f /* 0x935aa8 */, cc8), 0.25f /* 0x92ff84 */);
        if (0.0f > d0) frac = 1.0f;
        float e[3][3];
        limb_quat_matrix(F32(R2, 0), F32(R2, 4), F32(R2, 8), F32(R2, 0xc), e);
        const float r0 = (Q[0][0] * e[2][0] + Q[1][0] * e[2][1]) + Q[2][0] * e[2][2];
        const float r1 = (Q[0][1] * e[2][0] + Q[1][1] * e[2][1]) + Q[2][1] * e[2][2];
        const float r2 = (Q[0][2] * e[2][0] + Q[1][2] * e[2][1]) + Q[2][2] * e[2][2];
        const float s0 = (W[1][0] * r1 + W[0][0] * r0) + W[2][0] * r2;
        const float s1 = (W[1][1] * r1 + W[0][1] * r0) + W[2][1] * r2;
        const float s2 = (W[1][2] * r1 + W[0][2] * r0) + W[2][2] * r2;
        const float vx = rx_subs(-0.0f, F32(normal, 0)), vy = rx_subs(-0.0f, F32(normal, 4)), vz = rx_subs(-0.0f, F32(normal, 8));
        const float a = rx_muls(limb_acos((vy * s1 + vx * s0) + vz * s2), frac);
        float k0 = vz * s1 - vy * s2, k1 = vx * s2 - vz * s0, k2 = vy * s0 - vx * s1;
        const float lk = (k1 * k1 + k0 * k0) + k2 * k2;
        if (lk > 0.0f) {
            const float inv = 1.0f / rx_sqrts(lk);
            k0 = k0 * inv; k1 = k1 * inv; k2 = k2 * inv;
        }
        const float c0 = (e[0][1] * k1 + e[0][0] * k0) + e[0][2] * k2;
        const float c1 = (e[1][0] * k0 + e[1][1] * k1) + e[1][2] * k2;
        const float c2 = (e[2][0] * k0 + e[2][1] * k1) + e[2][2] * k2;
        stage_sincos(a, &s, &c);
        limb_axis_quat(M, c0, c1, c2, s, c, Q4, 0x00635b96);
        limb_turn_bone(pose + U32(st, 0x20) * 0x30u, Q4);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00633010, rx_rider_limb_reach_contact)
