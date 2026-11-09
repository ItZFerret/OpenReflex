/* anim_ik.c -- InverseKinematicsAnimator (RTTI vtable 0095efd8) and its two chain solvers.
 *
 * The animator evaluates its child (+0x5c) into a pool pose, copies that into the output pose, then bends the bones toward
 * goal positions in model space: two CCD chains (records of 0x80 bytes at [+0x64] + 0x00 / + 0x80, 006b0530) and two two-bone
 * limbs ([+0x64] + 0x100 / + 0x180, 006b0eb0); with the global switch byte 0x00d9d3e0 set the chains are solved by 006b2fb0
 * instead. Bone positions / model rotations come from SkeletonPose_BoneToModel (0082efb0, engine).
 *
 * IK goal record (0x80 bytes, 16-aligned; the chain and the limb solvers share the first 0x50 bytes and +0x70):
 *   +0x08 engaged (0: the goal blends from target + offset back to the animated effector; else from the effector to the target)
 *   +0x0c iterations (CCD passes)            +0x10 -> target position (vec3, read through the x87 every evaluation)
 *   +0x20 target (vec4, xyz copied)          +0x30 goal offset (vec4; xyz relative to the target)
 *   +0x40 blend time (+= dt)                 +0x50 tolerance (vec4: stop when every xyz lane of the worst step error is below)
 *   +0x60 joint count                        +0x64 effector bone index
 *   +0x68 -> joint bone indices (int[])      +0x6c -> joint weights (float[])   +0x70 blend duration
 *
 * Dead stack: the solvers keep their temporaries in a frame laid out like the original's (ANIM_FRAME_ENTER) and store into it
 * what the original stores there, because the original reads stale stack in two places that land in live memory:
 *  - 0082efb0 copies its output transform's +0x1c (never written by anyone: the vec4 after the translation) into the w lane of
 *    the bone position it returns;
 *  - 006b0530 reads its goal as a vec4 whose w lane ([A - 0x144]) it never writes before: that is the w of the last quaternion
 *    the previous 006b0530 call at the same stack depth computed (chain 1 sees chain 0's).
 * Those w lanes reach the goal offset's w (+0x3c) and nothing else: no code reads +0x3c, the IK results never depend on them.
 * The port reproduces them exactly (same frame addresses, same callee frames), so it matches the original bit for bit. */
#include "anim_ik.h"
#include "../src/rx_os.h"
#include "anim_extern.h"
#include "phys_vec.h"
#include "../src/rx_x87.h"



/* 0082efb0 SkeletonPose_BoneToModel (engine; ECX = bone, EDX = pose, EDI = position out (vec4); stack: skeleton, transform out;
 * RET 8): the bone's rotation / translation composed up the parent chain (skeleton +0x23c parent indices) and the skeleton's
 * world node; writes the model-space rotation into the transform (+0x00) and the position (vec4) to EDI; returns EDI.
 * ebx / ebp / esi: the values the original has at the call (they land in the callee's frame). */
static rx_addr bone_to_model(rx_addr skel, rx_addr pose, uint32_t bone, rx_addr pos_out, rx_addr xform, rx_addr ret,
                             uint32_t ebx, uint32_t ebp, uint32_t esi) {
    (void)ebx; (void)ebp; (void)esi;
    const uint32_t a[2] = {skel, xform};
    const rx_addr sp0 = ik_enter(2, a, ret);
    const rx_addr r = rx_skeleton_bone_to_model(skel, xform, bone, pose, pos_out);
    rx_set_sp(sp0);
    return r;
}

/* 005089d0 (ECX = transform; leaf, transliterated): identity rotation (0x9cb550), translation = the 0xa95d50 vector, the two
 * valid bytes +0x20 / +0x21 cleared; +0x1c is not written. */
static void bone_transform_init(rx_addr t) {
    SU32(t, 0x00, U32(0, 0x009cb550)); SU32(t, 0x04, U32(0, 0x009cb554));
    SU32(t, 0x08, U32(0, 0x009cb558)); SU32(t, 0x0c, U32(0, 0x009cb55c));
    SU32(t, 0x10, U32(0, 0x00a95d50)); SU32(t, 0x14, U32(0, 0x00a95d54)); SU32(t, 0x18, U32(0, 0x00a95d58));
    SU8(t, 0x20, 0); SU8(t, 0x21, 0);
}

/* The exe's packed quaternion product q * t (q splat by lane, t shuffled, sign vectors 0x93a1f0 / 0x93a200 / 0x93a210), as
 * inlined in 0082efb0 and here: ((t.yxwz * q.z) * (-1 1 1 -1) + (t.zwxy * q.y) * (1 1 -1 -1)) + (q.w * t + (q.x * t.wzyx) * (1 -1 1 -1)). */
static rx_v4 quat_mul_packed(rx_v4 q, rx_v4 t) {
    rx_v4 t_wzyx = v4_shuf(t, t, 0x1b);
    rx_v4 px = v4_mul(v4_lane(q, 0), t_wzyx);
    rx_v4 t_zwxy = v4_shuf(t_wzyx, t_wzyx, 0xb1);
    rx_v4 py = v4_mul(v4_lane(q, 1), t_zwxy);
    rx_v4 t_yxwz = v4_shuf(t_zwxy, t_zwxy, 0x1b);
    rx_v4 pz = v4_mul(t_yxwz, v4_lane(q, 2));
    pz = v4_mul(pz, v4(-1.0f, 1.0f, 1.0f, -1.0f));   /* 0x93a210 */
    py = v4_mul(py, v4(1.0f, 1.0f, -1.0f, -1.0f));   /* 0x93a200 */
    px = v4_mul(px, v4(1.0f, -1.0f, 1.0f, -1.0f));   /* 0x93a1f0 */
    pz = v4_add(pz, py);
    rx_v4 pw = v4_add(v4_mul(v4_lane(q, 3), t), px);
    return v4_add(pz, pw);
}

/* The inlined packed cos(t) polynomial of the reduced angle (all lanes alike: scalars here), x86-exact:
 * ((c6 t2 + c5) t4 + (c4 t2 + c3)) t4 + (c2 t2 + c1)  (0x935b10 .. 0x935b60). */
static float cos_poly_packed(float r) {
    float t2 = phys_mul(r, r), t4 = phys_mul(t2, t2);
    float p = phys_add(phys_mul(t2, K_C6), K_C5);    /* 0x935b60, 0x935b50 */
    p = phys_mul(p, t4);
    p = phys_add(p, phys_add(phys_mul(t2, K_C4), K_C3)); /* 0x935b40, 0x935b30 */
    p = phys_mul(p, t4);
    return phys_add(p, phys_add(phys_mul(t2, K_C2), K_C1)); /* 0x935b20, 0x935b10 */
}
/* The packed range reduction: |a| (max(0 - a, a)) folded into [-pi, pi] as pi - (|a| - trunc(|a| / 2pi) * 2pi). Stores the
 * truncated turn count (as floats, all lanes) into the scratch vec4 as the original does. */
static float trig_reduce_packed(float a, rx_addr scratch) {
    float m = phys_max(phys_sub(0.0f, a), a);
    v4_st(scratch, v4_splat(phys_mul(m, K_INV_2PI)));                    /* 0x935ad0: 1 / 2pi */
    float turns = (float)rx_cvtt(phys_mul(m, K_INV_2PI));
    v4_st(scratch, v4_splat(turns));
    return phys_sub(K_PI, phys_sub(m, phys_mul(turns, K_2PI)));  /* 0x935af0, 0x935ae0 */
}

/* 006b0530 (ECX = skeleton, ESI = chain record, EDX = pose; stack: dt; RET 4): CCD inverse kinematics of one joint chain.
 * Copies the target, blends the goal between the effector and the target (+0x40 / +0x70; see the record above), then up to
 * +0x0c passes: for every joint from the last to the first, the rotation that turns (effector - joint) toward (goal - joint),
 * its angle scaled by the joint weight (* (1 + 0.2 * (1 - pass / passes))) / joint count, applied in model space to the joint's
 * local rotation. Stops early when the largest step error of a pass falls below the tolerance in every lane. */
void rx_ik_chain_ccd(rx_addr skel, rx_addr chain, rx_addr pose, float dt) {
    ANIM_FRAME_ENTER(A, 0x190);                    /* sub esp, 0x188; push ebx; push edi */
    const rx_addr ebp = A_sp0 - 4;
    const rx_addr xf = A - 0xf0;                   /* transform out of 0082efb0 (effector) */
    const rx_addr jxf = A - 0x130;                 /* transform out of 0082efb0 (joint): its model rotation */
    const rx_addr scratch = A - 0x90, quat = A - 0x150;
    ik_frame_cookie(A - 4, A - 0x188);
    v4_st(xf, v4_ld(0x009cb550));
    SU32(xf, 0x10, U32(0, 0x00a95d50)); SU32(xf, 0x14, U32(0, 0x00a95d54));
    rx_addr src = U32(chain, 0x10);
    SU32(chain, 0x20, ik_x87_bits(U32(src, 0))); SU32(chain, 0x24, ik_x87_bits(U32(src, 4))); SU32(chain, 0x28, ik_x87_bits(U32(src, 8)));
    SU32(A - 0x158, 0, skel);
    SU32(A - 0x174, 0, pose);
    SU32(xf, 0x18, U32(0, 0x00a95d58));
    SU8(xf, 0x20, 0); SU8(xf, 0x21, 0);
    rx_v4 eff = v4_ld(bone_to_model(skel, pose, U32(chain, 0x64), A - 0x60, xf, 0x006b05d4, 0, ebp, chain));
    v4_st(A - 0x70, eff);

    /* goal blend: from `from` toward `to` by t = time / duration (1 once the time passed the duration) */
    const uint32_t engaged = U32(chain, 0x08);
    rx_v4 to, from;
    if (engaged) { to = v4_ld(chain + 0x20); from = eff; }
    else {
        SF32(quat, 0x0, phys_add(F32(chain, 0x30), F32(chain, 0x20)));
        SF32(quat, 0x4, phys_add(F32(chain, 0x34), F32(chain, 0x24)));
        SF32(quat, 0x8, phys_add(F32(chain, 0x38), F32(chain, 0x28)));
        from = v4_ld(quat);                         /* w: the stale [A - 0x144] (see the file comment) */
        to = eff;
    }
    const float dur = F32(chain, 0x70), time = F32(chain, 0x40);
    float t;
    if (dur < time || dur != dur || time != time) t = 1.0f;          /* comiss; jb */
    else t = phys_div(time, dur > K_EPS ? dur : K_EPS);          /* 0x9775a4 */
    const float now = phys_add(time, dt);
    rx_v4 goal = v4_add(v4_mul(v4_sub(to, from), v4_splat(t)), from);
    v4_st(A - 0x40, goal);
    SF32(chain, 0x40, now);
    if (engaged || now > dur) {
        v4_st(chain + 0x30, goal);
        SF32(chain, 0x30, phys_sub(F32(chain, 0x30), F32(chain, 0x20)));
        SF32(chain, 0x34, phys_sub(F32(chain, 0x34), F32(chain, 0x24)));
        SF32(chain, 0x38, phys_sub(F32(chain, 0x38), F32(chain, 0x28)));
    }

    const int32_t joints = I32(chain, 0x60), passes = I32(chain, 0x0c);
    SU32(A - 0x134, 0, (uint32_t)joints);
    int more = 1;
    int32_t pass = 0;
    SU32(A - 0x154, 0, 0);
    if (passes > 0) do {
        if (!more) break;
        rx_v4 worst = v4_splat(-1.0f);              /* 0x9cd9e8 */
        v4_st(A - 0x80, worst);
        if (pass != 0) {
            eff = v4_ld(bone_to_model(skel, U32(A - 0x174, 0), U32(chain, 0x64), A - 0x20, xf, 0x006b070e, (uint32_t)pass, ebp, chain));
            v4_st(A - 0x70, eff);
        }
        const float boost = (1.0f - (float)pass / (float)passes) * 0.2f /* 0x92e388 */ + 1.0f;
        SF32(A - 0x138, 0, boost);
        int32_t j = joints - 1;
        SU32(A - 0x178, 0, (uint32_t)j);
        if (j >= 0) {
            v4_st(A - 0xc0, v4_splat(0.0f));        /* 0x92e12c */
            v4_st(A - 0xb0, v4_splat((float)joints));
            v4_st(A - 0xa0, v4_splat(0.5f));        /* 0x9322cc */
            v4_st(A - 0x100, v4_zero());
            do {
                bone_transform_init(jxf);
                const uint32_t bone = U32(U32(chain, 0x68), 4u * (uint32_t)j);
                rx_v4 at = v4_ld(bone_to_model(skel, U32(A - 0x174, 0), bone, A - 0x30, jxf, 0x006b07cf, bone, ebp, chain));
                eff = v4_ld(A - 0x70);
                goal = v4_ld(A - 0x40);
                /* the rotation axis (effector - joint) x (goal - joint), its length and the step's squared error */
                rx_v4 a = v4_sub(eff, at), b = v4_sub(goal, at);
                rx_v4 a_yzx = v4_shuf(a, a, 0xc9), b_zxy = v4_shuf(b, b, 0xd2);
                rx_v4 axis = v4_sub(v4_mul(b_zxy, a_yzx), v4_mul(v4_shuf(b_zxy, b_zxy, 0xd2), v4_shuf(a_yzx, a_yzx, 0xc9)));
                axis = v4_and(axis, v4_bits(~0u, ~0u, ~0u, 0));                       /* 0x92db80 */
                const float aa = phys_hsum3(v4_mul(a, a));
                rx_v4 xx = v4_mul(axis, axis);
                const float sin_ab = phys_sqrt(phys_add(phys_add(xx.v[0], xx.v[2]), xx.v[1]));
                const float bb = phys_hsum3(v4_mul(b, b));
                const float err = phys_div(phys_mul(sin_ab, sin_ab), aa);
                const float aabb = phys_mul(bb, aa);
                v4_st(scratch, v4_splat(rx_bf(0.0f < aabb ? ~0u : 0u)));
                float step = 0.0f;
                if (0.0f < aabb) {
                    const float w = phys_mul(F32(U32(chain, 0x6c), 4u * (uint32_t)j), F32(A - 0x138, 0));
                    step = phys_mul(phys_mul(rx_rsqrt(aabb), sin_ab), w);
                }
                v4_st(scratch, v4_splat(rx_bf(0.0f == step ? ~0u : 0u)));
                worst = v4_max(v4_ld(A - 0x80), v4_splat(err));
                v4_st(A - 0x80, worst);
                float len = sin_ab;
                if (0.0f == step) { len = 1.0f; axis = v4_ld(0x00d615c0); }          /* default axis (zero) */
                const float angle = phys_div(step, F32(A - 0xb0, 0));
                rx_v4 u = v4_div(axis, v4_splat(len));
                const float uu = phys_hsum3(v4_mul(u, u));
                const float ulen = phys_sqrt(uu);
                v4_st(A - 0x50, v4_splat(ulen));
                const uint32_t finite = uu != rx_bf(0x7f800000u) ? ~0u : 0u;          /* cmpneqps 0x92dc20 */
                const float half = phys_mul(angle, F32(A - 0xa0, 0));
                /* sin(half) = -cos(pi/2 + half) and cos(half) of the reduced angles */
                const float r_sin = trig_reduce_packed(phys_add(half, K_PI_2), scratch);   /* 0x935b00 */
                const float r_cos = trig_reduce_packed(half, scratch);
                {
                    float t2 = phys_mul(r_cos, r_cos);
                    v4_st(scratch, v4_splat(phys_mul(t2, t2)));
                }
                const float c = phys_sub(0.0f, cos_poly_packed(r_cos));
                v4_st(A - 0x60, v4_splat(c));
                const float s = cos_poly_packed(r_sin);
                /* the unit axis (0 when its length is 0, NaN 0x92dc30 when infinite) */
                rx_v4 nu = v4_div(u, v4_splat(ulen));
                const uint32_t nonzero = 0.0f != ulen || ulen != ulen ? ~0u : 0u;
                for (int i = 0; i < 4; i++)
                    nu.v[i] = rx_bf(((nonzero & v4_lb(nu, i)) & finite) | (~finite & 0x7fc00000u));
                rx_v4 q = v4_mul(v4_splat(s), nu);
                q.v[3] = c;
                v4_st(scratch, q);
                /* rotation in model space, then back into the joint's parent space */
                rx_v4 tq = v4_ld(jxf);
                rx_v4 r = quat_mul_packed(q, tq);
                v4_st(A - 0x170, r);
                const rx_addr rec = pose + 0x30u * bone;
                const float bw = F32(rec, 0xc), bx = F32(rec, 0), by = F32(rec, 4), bz = F32(rec, 8);
                const float t0 = tq.v[0], t1 = tq.v[1], t2 = tq.v[2], t3 = tq.v[3];
                const float dw = ((bw * t3 + bx * t0) + by * t1) + bz * t2;
                const float dx = ((bw * t0 - bx * t3) - bz * t1) + by * t2;
                float dy = bw * t1 - by * t3;
                dy = dy - bx * t2;
                SF32(A - 0x17c, 0, dy);
                dy = dy + bz * t0;
                const float dz = ((bw * t2 - bz * t3) - by * t0) + bx * t1;
                const double nx = rx_subs(-0.0f, dx), ny = rx_subs(-0.0f, dy), nz = rx_subs(-0.0f, dz);
                const double rx_ = r.v[0], ry = r.v[1], rz = r.v[2], rw = r.v[3], dwd = dw;
                SF32(quat, 0xc, (float)(((rw * dwd - rx_ * nx) - ry * ny) - rz * nz));
                SF32(quat, 0x0, (float)(((rw * nx + rx_ * dwd) + rz * ny) - ry * nz));
                j--;
                SU32(A - 0x178, 0, (uint32_t)j);
                SF32(quat, 0x4, (float)(((ry * dwd + rw * ny) + nz * rx_) - rz * nx));
                SF32(quat, 0x8, (float)(((rz * dwd + rw * nz) + ry * nx) - ny * rx_));
                SU32(rec, 0x0, U32(quat, 0x0)); SU32(rec, 0x4, U32(quat, 0x4));
                SU32(rec, 0x8, U32(quat, 0x8)); SU32(rec, 0xc, U32(quat, 0xc));
                SU8(rec, 0x20, 1);
            } while (j >= 0);
        }
        rx_v4 lt = v4_cmplt(v4_ld(chain + 0x50), v4_ld(A - 0x80));
        v4_st(scratch, lt);
        more = v4_lb(lt, 0) && v4_lb(lt, 1) && v4_lb(lt, 2);
        pass++;
        SU32(A - 0x154, 0, (uint32_t)pass);
    } while (pass < passes);
    SU32(A - 0x18c, 0, 0x006b0ea7);                 /* __security_check_cookie's return address (over the saved ebx) */
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b0530, rx_ik_chain_ccd)

/* ---- two-bone limb (006b0eb0) ---- */

/* x86 single-precision operations (NaN rule of SSE): the limb solve keeps them everywhere, since degenerate limbs (zero lengths,
 * goal on the root) make NaNs that reach the pose. */
static inline float xadd(float a, float b) { return rx_adds(a, b); }
static inline float xsub(float a, float b) { return rx_subs(a, b); }
static inline float xmul(float a, float b) { return rx_muls(a, b); }
static inline float xdiv(float a, float b) { return rx_divs(a, b); }
/* the same for SSE2 doubles (default NaN 0xfff8000000000000, else the first NaN operand quieted) */
static inline double d_nan_rule(double r, double a, double b) {
    if (r == r) return r;
    if (a != a) return rx_bd(rx_db(a) | 0x0008000000000000ull);
    if (b != b) return rx_bd(rx_db(b) | 0x0008000000000000ull);
    return rx_bd(0xfff8000000000000ull);
}
static inline double dadd(double a, double b) { return d_nan_rule(a + b, a, b); }
static inline double dsub(double a, double b) { return d_nan_rule(a - b, a, b); }
static inline double dmul(double a, double b) { volatile double vb = b; return d_nan_rule(a * vb, a, b); }
static inline double ddiv(double a, double b) { return d_nan_rule(a / b, a, b); }

/* SqrtF 0042c230 (cdecl float, sqrtss) called at stack pointer esp: its argument, return address and saved ebp land on the
 * dead stack as in the original. */
static float sqrtf_call(rx_addr esp, uint32_t arg_bits, rx_addr ret, rx_addr ebp) {
    SU32(esp - 4, 0, arg_bits); SU32(esp - 8, 0, ret); SU32(esp - 12, 0, ebp);
    return rx_sqrts(rx_bf(arg_bits));
}
/* Float_Abs_Thunk 00401040 (xmm0; push ecx; call Float_Abs 00401000) at stack pointer esp: |x| (sign bit cleared). */
static float fabs_call(rx_addr esp, float x, rx_addr ret, uint32_t ecx, rx_addr ebp) {
    SU32(esp - 4, 0, ret); SU32(esp - 8, 0, ecx); SU32(esp - 12, 0, 0x00401046); SU32(esp - 16, 0, ebp);
    return rx_bf(rx_fb(x) & 0x7fffffffu);
}
/* x87 (fld / fmul st0 / faddp / fstp m32): (a * a + b * b) + c * c at the animation threads' precision control, 53 bits
 * (control word 0x027f: the products are exact, the sums round to double, the store to float). */
static float x87_sumsq3(float a, float b, float c) {
    rx_f80 x = rx_f80_from_f32(a), y = rx_f80_from_f32(b), z = rx_f80_from_f32(c);
    rx_f80 s = rx_f80_add(rx_f80_mul(x, x, 53, 0), rx_f80_mul(y, y, 53, 0), 53, 0);
    return rx_f80_to_f32(rx_f80_add(s, rx_f80_mul(z, z, 53, 0), 53, 0), 0);
}

/* 0082e610 (engine; ECX = bone, EAX = pose, EDI = transform out; stack: skeleton; RET 4): the bone's model-space rotation and
 * position (composed up the parent chain) into the transform (+0x00 rotation, +0x10 position). */
static void bone_model_xform(rx_addr skel, rx_addr pose, uint32_t bone, rx_addr out, rx_addr ret,
                             uint32_t ebx, uint32_t ebp) {
    (void)ebx; (void)ebp;
    const rx_addr sp0 = ik_enter(1, &skel, ret);
    rx_skeleton_bone_to_model_scalar(skel, bone, pose, out);
    rx_set_sp(sp0);
}
/* Matrix3_ToQuaternionToEsi 00401740 (math; ESI = quaternion out; cdecl: the 4x4 matrix, rows 0x10 apart). */
static void matrix_to_quat(rx_addr m, rx_addr q_out, rx_addr ret, uint32_t ecx, uint32_t ebx, uint32_t ebp, uint32_t edi) {
    (void)ecx; (void)ebx; (void)edi;
    const rx_addr sp0 = ik_enter(1, &m, ret);
    rx_world_matrix3_to_quaternion_to_fr(q_out, m, ebp);
    rx_set_sp(sp0);
}

/* The inlined asin of 006b0eb0 (x = a cosine; |x| < 0.7071 (0x9ddfc8): rational polynomial of |x|, else pi/2 - that of
 * sqrt(|1 - x * x|); sign of x), in double precision with float roundings where the original converts. The Float_Abs_Thunk and
 * SqrtF calls leave their frames at stack pointer esp. Returns acos(x) = pi/2 - asin(x). */
static float limb_acos(float x, rx_addr esp, rx_addr ret_abs1, rx_addr ret_abs2, rx_addr ret_sqrt, uint32_t ecx, rx_addr ebp,
                       rx_addr slot_ax, rx_addr slot_x) {
    const float ax = fabs_call(esp, x, ret_abs1, ecx, ebp);
    SF32(slot_ax, 0, ax); SF32(slot_x, 0, x);
    float y;
    const int small = ax < K_SQRT_HALF;                                     /* comiss; jb (unordered: small) */
    const int big = !(ax < K_SQRT_HALF) && ax == ax;
    if (big) {
        float v = (float)dmul((double)x, (double)x);
        v = (float)dsub(1.0, (double)v);
        v = fabs_call(esp, v, ret_abs2, ecx, ebp);
        y = sqrtf_call(esp, rx_fb(v), ret_sqrt, ebp);
    } else y = ax;
    (void)small;
    const double yd = y;
    const float y2 = (float)dmul(yd, yd);
    const float y4 = (float)dmul((double)y2, (double)y2);
    const float a = (float)dsub(0.5648093223571777, dmul((double)y2, 0.04992850869894028));      /* 0x9777b0, 0x9777a8 */
    const double ay4 = dmul((double)a, (double)y4);
    const float b = (float)dsub(1.0, dmul((double)y2, 1.4430354833602905));                       /* 0x9777a0 */
    const float r = (float)ddiv(1.0, dadd(ay4, (double)b));
    const float c = (float)dsub(0.3993034064769745, dmul((double)y2, 0.0193784236907959));       /* 0x977798, 0x977790 */
    const float d = (float)dsub(1.0, dmul((double)y2, 1.2763687372207642));                       /* 0x977788 */
    const float num = (float)dmul(dadd(dmul((double)c, (double)y4), (double)d), yd);
    float s = (float)dmul((double)r, (double)num);
    if (big) s = (float)dsub(1.5707963705062866, (double)s);                                      /* 0x977510 */
    if (!(x >= 0.0f)) s = xsub(-0.0f, s);                                                        /* comiss x, 0; jb */
    return (float)dsub(1.5707963705062866, (double)s);
}

/* The inlined sin / cos of an angle: sign s (1 / -1 (0x9cd9e8) when negative or NaN), the [-pi, pi] reduction of s * angle,
 * cos = -cos_poly, sin = sin_poly * reduced * s (anim_math.h polynomials; x86-exact here). The original keeps the sign and the
 * reduced angle in frame slots: their final values are written there. */
static float limb_cos_poly(float t2, float t4) {
    float p = xadd(xmul(t2, K_C6), K_C5);
    p = xmul(p, t4);
    p = xadd(p, xadd(xmul(t2, K_C4), K_C3));
    p = xmul(p, t4);
    return xadd(p, xadd(xmul(t2, K_C2), K_C1));
}
static float limb_sin_poly(float t2, float t4) {
    float p = xadd(xmul(t2, K_S6), K_S5);
    float q = xadd(xmul(t2, K_S4), K_S3);
    float r = xadd(xmul(t2, K_S2), K_S1);
    p = xmul(p, t4);
    p = xadd(p, q);
    p = xmul(p, t4);
    return xadd(p, r);
}
static void limb_sincos(float angle, float* s_out, float* c_out, rx_addr slot_sign, rx_addr slot_red) {
    const float sgn = angle >= 0.0f ? 1.0f : -1.0f;
    SF32(slot_sign, 0, sgn);
    float a = xmul(sgn, angle);
    const float turns = (float)rx_cvtt(xmul(a, K_INV_2PI));                /* 0x9774a0 */
    const float red = xsub(K_PI, xsub(a, xmul(turns, K_2PI)));     /* 0x977498, 0x97749c */
    SF32(slot_red, 0, red);
    const float t2 = xmul(red, red), t4 = xmul(t2, t2);
    *c_out = xsub(-0.0f, limb_cos_poly(t2, t4));
    *s_out = xmul(xmul(limb_sin_poly(t2, t4), red), sgn);
}

/* The axis-angle rotation matrix (4x4, rows 0x10 apart, translation row zero, [3][3] = 1) of unit axis w, sin s, cos c, as the
 * original computes it: m[i][j] = ((1 - c) * w_i) * w_j + c / +- s * w_k. */
static void axis_angle_matrix(rx_addr m, float s, float c, float w0, float w1, float w2) {
    const float omc = xsub(1.0f, c);
    float k = xmul(omc, w0);
    SF32(m, 0x00, xadd(xmul(k, w0), c)); SF32(m, 0x04, xadd(xmul(k, w1), xmul(s, w2))); SF32(m, 0x08, xsub(xmul(k, w2), xmul(s, w1)));
    SF32(m, 0x0c, 0.0f);
    k = xmul(omc, w1);
    SF32(m, 0x10, xsub(xmul(k, w0), xmul(s, w2))); SF32(m, 0x14, xadd(xmul(k, w1), c)); SF32(m, 0x18, xadd(xmul(k, w2), xmul(s, w0)));
    SF32(m, 0x1c, 0.0f);
    k = xmul(omc, w2);
    SF32(m, 0x20, xadd(xmul(k, w0), xmul(s, w1))); SF32(m, 0x24, xsub(xmul(k, w1), xmul(s, w0))); SF32(m, 0x28, xadd(xmul(k, w2), c));
    SF32(m, 0x2c, 0.0f);
    SF32(m, 0x30, 0.0f); SF32(m, 0x34, 0.0f); SF32(m, 0x38, 0.0f); SF32(m, 0x3c, 1.0f);
}

/* A bone record's rotation renormalized in double precision (zero when its squared length is exactly 0); the squared length
 * goes to the frame slot, the SqrtF call leaves its frame at esp. `mid`: the operand order of the mid joint's copy. */
static void normalize_bone_quat(rx_addr b, rx_addr slot, rx_addr esp, rx_addr ret, rx_addr ebp, int mid) {
    const double x = F32(b, 0), w = F32(b, 0xc), y = F32(b, 4), z = F32(b, 8);
    const float len2 = (float)dadd(dadd(dadd(dmul(x, x), dmul(w, w)), dmul(y, y)), dmul(z, z));
    SF32(slot, 0, len2);
    if (len2 == 0.0f) { SU32(b, 0, 0); SU32(b, 4, 0); SU32(b, 8, 0); SU32(b, 0xc, 0); return; }
    const double inv = xdiv(1.0f, sqrtf_call(esp, ik_x87_bits(rx_fb(len2)), ret, ebp));
    SF32(b, 0xc, (float)(mid ? dmul(inv, (double)F32(b, 0xc)) : dmul((double)F32(b, 0xc), inv)));
    SF32(b, 0x0, (float)dmul(inv, (double)F32(b, 0x0)));
    SF32(b, 0x4, (float)dmul(inv, (double)F32(b, 0x4)));
    SF32(b, 0x8, (float)(mid ? dmul(inv, (double)F32(b, 0x8)) : dmul((double)F32(b, 0x8), inv)));
}

/* 006b0eb0 (EAX = skeleton; stack: limb record, pose, dt; RET 0xc): two-bone IK of a limb (joints [+0x68][0] root, [1] mid,
 * effector +0x64; the goal record of the file comment, +0x00 = the effector's extension beyond the end bone). The goal blends
 * like the chains' (from the extended end point to the target or back); then, law of cosines: the mid joint's target K is at
 * `proj` along root -> goal and `h` along the bend direction (the mid joint's rotation applied to -(0x9cb4c0), made
 * perpendicular to the goal direction); the root turns (root -> mid) onto (root -> K), the mid joint turns (mid -> end) onto
 * (K -> goal), both as axis-angle matrices taken into the joint's frame and converted to quaternions (00401740) multiplied
 * onto the local rotations, which are renormalized and marked valid. Disengaged with the blend over: only the offset is kept.
 * An unreachable triangle (negative Heron term) leaves the pose. */
void rx_ik_two_bone_solve(rx_addr skel, rx_addr limb, rx_addr pose, float dt) {
    ANIM_FRAME_ENTER(A, 0x1a0);                    /* sub esp, 0x194; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, E = A - 0x1a0;  /* E: ESP at the calls */
#define SLOT(o) (A - (o))
#define ST(o, v) SF32(A - (o), 0, (v))
#define LD(o) F32(A - (o), 0)
    const rx_addr Tm = SLOT(0x80), Te = SLOT(0x130), Tr = SLOT(0xb0);   /* mid, effector, root model transforms */
    const rx_addr joints = U32(limb, 0x68);
    const uint32_t engaged = U32(limb, 0x08);
    rx_addr tx[3] = {Tm, Te, Tr};
    for (int i = 0; i < 3; i++) {
        v4_st(tx[i], v4_ld(0x009cb550));
        SU32(tx[i], 0x10, U32(0, 0x00a95d50)); SU32(tx[i], 0x14, U32(0, 0x00a95d54)); SU32(tx[i], 0x18, U32(0, 0x00a95d58));
        SU8(tx[i], 0x20, 0); SU8(tx[i], 0x21, 0);
    }
    rx_set_sp(E);
    bone_model_xform(skel, pose, U32(joints, 4), Tm, 0x006b0f93, limb, ebp);
    bone_model_xform(skel, pose, U32(limb, 0x64), Te, 0x006b0fa3, limb, ebp);
    bone_model_xform(skel, pose, U32(joints, 0), Tr, 0x006b0fb8, limb, ebp);

    /* the two bones: d1 = mid - root, d2 = end - mid */
    const float mx = F32(Tm, 0x10), my = F32(Tm, 0x14), mz = F32(Tm, 0x18);
    const float d2x = xsub(F32(Te, 0x10), mx), d2y = xsub(F32(Te, 0x14), my), d2z = xsub(F32(Te, 0x18), mz);
    const float d1x = xsub(mx, F32(Tr, 0x10)), d1y = xsub(my, F32(Tr, 0x14)), d1z = xsub(mz, F32(Tr, 0x18));
    ST(0x160, d1x); ST(0x140, d1x);
    ST(0xd0, d2x); ST(0x170, d2x); ST(0xcc, d2y); ST(0x16c, d2y);
    const float l2sq = xadd(xadd(xmul(d2x, d2x), xmul(d2y, d2y)), xmul(d2z, d2z));
    ST(0x17c, l2sq);
    ST(0xc8, d2z); ST(0x168, d2z); ST(0x15c, d1y); ST(0x158, d1z); ST(0x13c, d1y); ST(0x138, d1z);
    const float l2 = xadd(sqrtf_call(E, ik_x87_bits(rx_fb(l2sq)), 0x006b1093, ebp), F32(limb, 0x00));   /* + extension */
    ST(0x178, l2);
    ST(0x18c, l2sq);
    float n2x = d2x, n2y = d2y, n2z = d2z;
    if (l2sq > 0.0f) {
        const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(l2sq)), 0x006b10c2, ebp));
        n2x = xmul(inv, d2x); n2z = xmul(inv, d2z); n2y = xmul(inv, d2y);
    }
    /* the (extended) end point P and the goal blend G */
    const float px = xadd(xmul(n2x, l2), mx), py = xadd(xmul(n2y, l2), my), pz = xadd(xmul(n2z, l2), mz);
    ST(0xc0, px); ST(0xbc, py); ST(0xb8, pz); ST(0xe8, pz);
    const rx_addr tgt = U32(limb, 0x10);
    float tox, toy, toz, frx, fry, frz;
    if (engaged) { tox = F32(tgt, 0); toy = F32(tgt, 4); toz = F32(tgt, 8); frx = px; fry = py; frz = pz; }
    else {
        frx = xadd(F32(tgt, 0), F32(limb, 0x30)); fry = xadd(F32(tgt, 4), F32(limb, 0x34));
        tox = px; toy = py; toz = pz;
        frz = xadd(F32(tgt, 8), F32(limb, 0x38));
        ST(0xe8, frz);
    }
    const float dur = F32(limb, 0x70), time = F32(limb, 0x40);
    float t;
    if (dur < time || dur != dur || time != time) t = 1.0f;
    else t = xdiv(time, dur > K_EPS ? dur : K_EPS);
    const float now = xadd(time, dt);
    const float gx = xadd(xmul(xsub(tox, frx), t), frx), gy = xadd(xmul(xsub(toy, fry), t), fry), gz = xadd(xmul(xsub(toz, frz), t), frz);
    ST(0xd0, gx); ST(0xcc, gy); ST(0xc8, gz);
    SF32(limb, 0x40, now);
    if (!engaged && now > dur) {                   /* blended out: keep the offset, leave the pose */
        SF32(limb, 0x30, xsub(gx, F32(tgt, 0))); SF32(limb, 0x34, xsub(gy, F32(tgt, 4))); SF32(limb, 0x38, xsub(gz, F32(tgt, 8)));
        ANIM_FRAME_LEAVE(A);
        return;
    }
    if (engaged) { SF32(limb, 0x30, xsub(gx, F32(tgt, 0))); SF32(limb, 0x34, xsub(gy, F32(tgt, 4))); SF32(limb, 0x38, xsub(gz, F32(tgt, 8))); }

    /* the triangle root - mid - goal */
    const float l1sq = x87_sumsq3(LD(0x160), LD(0x15c), LD(0x158));
    ST(0x190, l1sq);
    const float l1 = sqrtf_call(E, ik_x87_bits(rx_fb(l1sq)), 0x006b12ac, ebp);
    const float rx0 = F32(Tr, 0x10), ry0 = F32(Tr, 0x14), rz0 = F32(Tr, 0x18);
    const float vy = xsub(gy, ry0), vz = xsub(gz, rz0);
    ST(0x17c, l1);
    const float vx = xsub(gx, rx0);
    ST(0x150, vx); ST(0x14c, vy);
    const float lgsq = xadd(xadd(xmul(vx, vx), xmul(vy, vy)), xmul(vz, vz));
    ST(0x148, vz); ST(0x180, lgsq); ST(0x18c, lgsq);
    if (lgsq > 0.0f) {                             /* the goal direction (else d2's slots stay) */
        const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lgsq)), 0x006b1339, ebp));
        ST(0x170, xmul(vx, inv)); ST(0x16c, xmul(inv, vy)); ST(0x168, xmul(inv, vz));
    }
    const float lg = sqrtf_call(E, ik_x87_bits(rx_fb(lgsq)), 0x006b1383, ebp);
    float proj = xdiv(xmul(xadd(xsub(xmul(lg, lg), xmul(l2, l2)), xmul(l1, l1)), 0.5f), lg);
    ST(0x180, lg); ST(0x17c, proj);
    float h;
    if (proj > l1 || xsub(lg, l2) > proj) { h = 0.0f; ST(0x17c, l1); proj = l1; }
    else {
        const float a = xadd(lg, l2), b = xsub(l2, lg);
        const float heron = xmul(xmul(xmul(xsub(l1, a), xsub(b, l1)), xadd(b, l1)), xadd(a, l1));
        ST(0x18c, heron);
        if (0.0f > heron) { ANIM_FRAME_LEAVE(A); return; }
        h = xdiv(xmul(sqrtf_call(E, ik_x87_bits(rx_fb(heron)), 0x006b1424, ebp), 0.5f), LD(0x180));
    }
    ST(0x178, h);

    /* the bend direction: the mid joint's rotation matrix applied to -(0x9cb4c0), minus its part along the goal direction */
    {
        const float qx = F32(Tm, 0), qy = F32(Tm, 4), qz = F32(Tm, 8), qw = F32(Tm, 0xc);
        const float y2 = xmul(qy, 2.0f), yy = xmul(y2, qy), z2 = xmul(qz, 2.0f), zz = xmul(z2, qz);   /* 0x92e330 */
        ST(0x174, z2);
        const float wz = xmul(z2, qw);
        ST(0x180, wz);
        const float x2 = xmul(qx, 2.0f), wx = xmul(x2, qw);
        ST(0x188, wx);
        const float wy = xmul(y2, qw);
        ST(0xd4, y2);
        const float yz = xmul(y2, qz), xx = xmul(qx, x2);
        ST(0x18c, yz);
        const float xy = xmul(x2, qy);
        ST(0x44, x2);
        const float xz = xmul(x2, qz);
        const float m00 = xsub(1.0f, xadd(zz, yy));
        const float m10 = xsub(xy, wz), m01 = xadd(xy, wz);
        ST(0x120, m10);
        const float m11 = xsub(1.0f, xadd(zz, xx));
        ST(0x11c, m11); ST(0x130, m00);
        const float m02 = xsub(xz, wy), m20 = xadd(xz, wy);
        ST(0x128, m02);
        const float m12 = xadd(yz, wx), m21 = xsub(yz, wx);
        const float m22 = xsub(1.0f, xadd(yy, xx));
        ST(0x118, m12); ST(0x10c, m21);
        const float nvx = xsub(-0.0f, F32(0, 0x009cb4c0)), nvy = xsub(-0.0f, F32(0, 0x009cb4c4)), nvz = xsub(-0.0f, F32(0, 0x009cb4c8));
        const float bx = xadd(xadd(xmul(nvy, m10), xmul(nvx, m00)), xmul(nvz, m20));
        const float by = xadd(xadd(xmul(nvx, m01), xmul(nvy, m11)), xmul(nvz, m21));
        const float bz = xadd(xadd(xmul(nvx, m02), xmul(nvy, m12)), xmul(nvz, m22));
        const float dirx = LD(0x170), diry = LD(0x16c), dirz = LD(0x168);
        const float dot = xadd(xadd(xmul(by, diry), xmul(bx, dirx)), xmul(bz, dirz));
        const float ppz = xsub(bz, xmul(dot, dirz)), ppx = xsub(bx, xmul(dirx, dot)), ppy = xsub(by, xmul(dot, diry));
        const float lpsq = xadd(xadd(xmul(ppy, ppy), xmul(ppx, ppx)), xmul(ppz, ppz));
        ST(0x150, ppx); ST(0x14c, ppy); ST(0x148, ppz); ST(0x188, lpsq);
        float npx = ppx, npy = ppy, npz = ppz;
        if (lpsq > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lpsq)), 0x006b16ae, ebp));
            npx = xmul(ppx, inv); npy = xmul(inv, ppy); npz = xmul(inv, ppz);
        }
        /* the mid joint's target K = root + proj * dir + h * bend, and K - root */
        const float pdx = xmul(dirx, proj), hy = xmul(npy, h), hx = xmul(npx, h), hz = xmul(npz, h);
        ST(0x188, pdx);
        const float kx = xadd(xadd(pdx, rx0), hx);
        const float pdz = xmul(proj, dirz), pdy = xmul(proj, diry);
        ST(0x18c, pdz);
        const float kry = xadd(pdy, hy);
        const float ky = xadd(xadd(pdy, ry0), hy);
        ST(0xf0, kx);
        const float krx = xadd(LD(0x188), hx);
        const float kz = xadd(xadd(pdz, rz0), hz);
        const float krz = xadd(LD(0x18c), hz);
        const float lksq = xadd(xadd(xmul(kry, kry), xmul(krx, krx)), xmul(krz, krz));
        ST(0xec, ky); ST(0xe8, kz); ST(0x170, krx); ST(0x16c, kry); ST(0x168, krz); ST(0x188, lksq);
        if (lksq > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lksq)), 0x006b17d0, ebp));
            ST(0x170, xmul(krx, inv)); ST(0x16c, xmul(inv, kry)); ST(0x168, xmul(inv, krz));
        }
    }

    /* the root: turn (mid - root) onto (K - root) */
    const float l1sq_ = LD(0x190);
    ST(0x188, l1sq_);
    if (l1sq_ > 0.0f) {
        const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(l1sq_)), 0x006b1832, ebp));
        ST(0x140, xmul(inv, LD(0x160))); ST(0x13c, xmul(inv, LD(0x15c))); ST(0x138, xmul(inv, LD(0x158)));
    }
    {
        const float nkx = LD(0x170), nky = LD(0x16c), nkz = LD(0x168);
        const float nax = LD(0x140), nay = LD(0x13c), naz = LD(0x138);
        const float cosa = xadd(xadd(xmul(nkx, nax), xmul(nky, nay)), xmul(nkz, naz));
        ST(0x188, cosa);
        const float angle = limb_acos(cosa, E, 0x006b18b2, 0x006b18fc, 0x006b1907, engaged, ebp, SLOT(0x18c), SLOT(0x188));
        ST(0x178, angle);
        /* axis = (mid - root) x (K - root) */
        const float cx = xsub(xmul(nkz, nay), xmul(nky, naz));
        const float cz = xsub(xmul(nax, nky), xmul(nkx, nay));
        const float cy = xsub(xmul(nkx, naz), xmul(nax, nkz));
        const float lcsq = xadd(xadd(xmul(cy, cy), xmul(cx, cx)), xmul(cz, cz));
        ST(0x150, cx); ST(0x14c, cy); ST(0x148, cz); ST(0x188, lcsq);
        float ux = cx, uy = cy, uz = cz;
        if (lcsq > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lcsq)), 0x006b1ae9, ebp));
            ux = xmul(cx, inv); uy = xmul(cy, inv); uz = xmul(cz, inv);
        }
        /* the root's rotation matrix (rows at A - 0x130 / - 0x120 / - 0x110) */
        const float qx = F32(Tr, 0), qy = F32(Tr, 4), qz = F32(Tr, 8), qw = F32(Tr, 0xc);
        const float y2 = xmul(qy, 2.0f), x2 = xmul(qx, 2.0f), xx = xmul(qx, x2), z2 = xmul(qz, 2.0f);
        ST(0x17c, xx);
        const float yy = xmul(y2, qy);
        ST(0x180, yy);
        const float zz = xmul(z2, qz);
        ST(0x190, zz);
        const float wz = xmul(z2, qw), wx = xmul(x2, qw);
        ST(0x188, wz); ST(0x184, wx);
        const float yz = xmul(y2, qz);
        ST(0x18c, yz);
        const float wy = xmul(y2, qw), xy = xmul(x2, qy), xz = xmul(x2, qz);
        const float r00 = xsub(1.0f, xadd(zz, yy));
        ST(0x130, r00);
        const float r01 = xadd(xy, wz);
        ST(0x12c, r01);
        const float r02 = xsub(xz, wy);
        ST(0x128, r02);
        const float r11 = xsub(1.0f, xadd(zz, xx));
        const float r10 = xsub(xy, wz);
        ST(0x11c, r11); ST(0x120, r10);
        const float r20 = xadd(xz, wy), r21 = xsub(yz, wx), r12 = xadd(yz, wx);
        ST(0x110, r20);
        const float r22 = xsub(1.0f, xadd(yy, xx));
        ST(0x10c, r21); ST(0x118, r12); ST(0x108, r22);
        float s, c;
        limb_sincos(angle, &s, &c, SLOT(0x190), SLOT(0x184));
        ST(0x180, c);
        /* the axis-angle matrix (model space) at A - 0x40 .. - 0x18 */
        const float suz = xmul(s, uz);
        ST(0x184, suz);
        const float suy = xmul(s, uy);
        ST(0x190, suy);
        const float omc = xsub(1.0f, c);
        float k = xmul(omc, ux);
        ST(0x40, xadd(xmul(k, ux), c)); ST(0x3c, xadd(xmul(k, uy), suz)); ST(0x38, xsub(xmul(k, uz), suy));
        k = xmul(omc, uy);
        ST(0x30, xsub(xmul(k, ux), suz)); ST(0x2c, xadd(xmul(k, uy), c));
        const float sux = xmul(s, ux);
        const float m12 = xadd(xmul(k, uz), sux);
        k = xmul(omc, uz);
        ST(0x20, xadd(xmul(k, ux), suy)); ST(0x1c, xsub(xmul(k, uy), sux));
        /* the axis in the root's frame: w = (u . rows) */
        const float w0 = xadd(xadd(xmul(r00, ux), xmul(uy, r01)), xmul(uz, r02));
        ST(0x18, xadd(xmul(k, uz), c));
        ST(0x28, m12);
        const float w2 = xadd(xadd(xmul(uy, r21), xmul(ux, r20)), xmul(uz, r22));
        const float w1 = xadd(xadd(xmul(uy, r11), xmul(ux, r10)), xmul(uz, r12));
        ST(0x148, w2);
        /* the rotation by `angle` about w (the root's frame) as a matrix, its quaternion onto the root's local rotation */
        float s2, c2;
        limb_sincos(angle, &s2, &c2, SLOT(0x190), SLOT(0x184));
        ST(0x190, xmul(s2, w1));
        axis_angle_matrix(SLOT(0x130), s2, c2, w0, w1, w2);
        SU32(E - 4, 0, SLOT(0x130));
        matrix_to_quat(SLOT(0x130), SLOT(0x160), 0x006b215a, SLOT(0x130), limb, ebp, Tr);
        const uint32_t root = U32(U32(limb, 0x68), 0);
        const rx_addr b = pose + 0x30u * root;
        const double bw = F32(b, 0xc), bx = F32(b, 0), by = F32(b, 4), bz = F32(b, 8);
        const double qx2 = LD(0x160), qy2 = LD(0x15c), qz2 = LD(0x158), qw2 = LD(0x154);
        ST(0x144, (float)dsub(dsub(dsub(dmul(qw2, bw), dmul(qx2, bx)), dmul(qy2, by)), dmul(qz2, bz)));
        ST(0x150, (float)dsub(dadd(dadd(dmul(qx2, bw), dmul(qw2, bx)), dmul(qz2, by)), dmul(qy2, bz)));
        ST(0x148, (float)dsub(dadd(dadd(dmul(qw2, bz), dmul(qz2, bw)), dmul(qy2, bx)), dmul(qx2, by)));
        ST(0x14c, (float)dsub(dadd(dadd(dmul(qw2, by), dmul(qy2, bw)), dmul(qx2, bz)), dmul(qz2, bx)));
        SU32(b, 0x0, U32(SLOT(0x150), 0)); SU32(b, 0x4, U32(SLOT(0x14c), 0));
        SU32(b, 0x8, U32(SLOT(0x148), 0)); SU32(b, 0xc, U32(SLOT(0x144), 0));
        normalize_bone_quat(b, SLOT(0x184), E, 0x006b233c, ebp, 0);
    }

    /* the mid joint: turn (end' - K) onto (goal - K), end' = the end point moved with the root */
    {
        const uint32_t root = U32(U32(limb, 0x68), 0);
        const float rx_ = F32(Tr, 0x10), ry_ = F32(Tr, 0x14), rz_ = F32(Tr, 0x18);
        const float ez = xsub(LD(0xb8), rz_), ey = xsub(LD(0xbc), ry_), ex = xsub(LD(0xc0), rx_);
        const float v0 = xadd(xadd(xmul(ey, LD(0x30)), xmul(ex, LD(0x40))), xmul(ez, LD(0x20)));
        const float v2 = xadd(xadd(xadd(xmul(ey, LD(0x28)), xmul(ex, LD(0x38))), xmul(ez, LD(0x18))), rz_);
        const float v1 = xadd(xadd(xadd(xmul(ey, LD(0x2c)), xmul(ex, LD(0x3c))), xmul(ez, LD(0x1c))), ry_);
        const float kx = LD(0xf0), ky = LD(0xec), kz = LD(0xe8);
        const float e2z = xsub(v2, kz), g2x = xsub(LD(0xd0), kx), e2x = xsub(xadd(v0, rx_), kx);
        const float g2y = xsub(LD(0xcc), ky), e2y = xsub(v1, ky), g2z = xsub(LD(0xc8), kz);
        const float le2 = xadd(xadd(xmul(e2y, e2y), xmul(e2x, e2x)), xmul(e2z, e2z));
        SU8(pose + 0x30u * root, 0x20, 1);
        ST(0x170, e2x); ST(0x16c, e2y); ST(0x168, e2z); ST(0x140, g2x); ST(0x13c, g2y); ST(0x138, g2z); ST(0x184, le2);
        float nex = e2x, ney = e2y, nez = e2z;
        if (le2 > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(le2)), 0x006b254c, ebp));
            nex = xmul(e2x, inv); ney = xmul(inv, e2y); nez = xmul(inv, e2z);
            ST(0x170, nex); ST(0x16c, ney); ST(0x168, nez);
        }
        const float lg2 = xadd(xadd(xmul(g2y, g2y), xmul(g2x, g2x)), xmul(g2z, g2z));
        ST(0x184, lg2);
        float ngx = g2x, ngy = g2y, ngz = g2z;
        if (lg2 > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lg2)), 0x006b25d4, ebp));
            ngx = xmul(g2x, inv); ngy = xmul(inv, g2y); ngz = xmul(inv, g2z);
            ST(0x140, ngx); ST(0x13c, ngy); ST(0x138, ngz);
        }
        const float cosb = xadd(xadd(xmul(ngy, ney), xmul(ngx, nex)), xmul(ngz, nez));
        ST(0x184, cosb);
        const float angle = limb_acos(cosb, E, 0x006b264b, 0x006b2691, 0x006b269c, 0x30u * root, ebp, SLOT(0x188), SLOT(0x184));
        ST(0x188, angle);
        /* axis = (end' - K) x (goal - K) */
        const float cx = xsub(xmul(ngz, ney), xmul(ngy, nez));
        const float cz = xsub(xmul(nex, ngy), xmul(ngx, ney));
        const float cy = xsub(xmul(ngx, nez), xmul(nex, ngz));
        const float lcsq = xadd(xadd(xmul(cy, cy), xmul(cx, cx)), xmul(cz, cz));
        ST(0xf0, cx); ST(0xc0, cx); ST(0xec, cy); ST(0xe8, cz); ST(0xb8, cz); ST(0x184, lcsq);
        float uy = cy;
        if (lcsq > 0.0f) {
            const float inv = xdiv(1.0f, sqrtf_call(E, ik_x87_bits(rx_fb(lcsq)), 0x006b2891, ebp));
            ST(0xc0, xmul(cx, inv));
            ST(0xb8, xmul(cz, inv));
            uy = xmul(cy, inv);
        }
        const float ux = LD(0xc0), uz = LD(0xb8);
        /* the mid joint's rotation matrix again (from the 2x terms kept in the frame) */
        const float x2 = LD(0x44), qw = F32(Tm, 0xc), y2 = LD(0xd4), z2 = LD(0x174);
        const float wx = xmul(x2, qw);
        ST(0x178, wx); ST(0x174, z2);
        const float zz = xmul(z2, F32(Tm, 8)), xx = xmul(x2, F32(Tm, 0));
        const float wy = xmul(y2, qw);
        ST(0x17c, wy);
        const float wz = xmul(LD(0x174), qw);
        ST(0x18c, xx); ST(0x190, wz);
        const float xy = xmul(x2, F32(Tm, 4)), xz = xmul(x2, F32(Tm, 8)), yy = xmul(y2, F32(Tm, 4)), yz = xmul(y2, F32(Tm, 8));
        ST(0x180, xz);
        const float m02 = xsub(LD(0x180), LD(0x17c));
        ST(0x174, yz);
        const float m00 = xsub(1.0f, xadd(zz, yy));
        const float m10 = xsub(xy, LD(0x190)), m01 = xadd(xy, LD(0x190));
        ST(0x120, m10);
        const float m11 = xsub(1.0f, xadd(zz, LD(0x18c)));
        ST(0x11c, m11);
        const float m21 = xsub(LD(0x174), LD(0x178)), m12 = xadd(LD(0x174), LD(0x178));
        ST(0x10c, m21);
        const float m22 = xsub(1.0f, xadd(yy, LD(0x18c)));
        ST(0x108, m22);
        const float w0 = xadd(xadd(xmul(m00, ux), xmul(uy, m01)), xmul(uz, m02));
        ST(0x12c, m01); ST(0x118, m12);
        const float m20 = xadd(LD(0x180), LD(0x17c));
        ST(0x110, m20);
        const float w2 = xadd(xadd(xmul(uy, m21), xmul(ux, m20)), xmul(uz, m22));
        const float w1 = xadd(xadd(xmul(uy, m11), xmul(ux, m10)), xmul(uz, m12));
        float s3, c3;
        limb_sincos(angle, &s3, &c3, SLOT(0x18c), SLOT(0x174));
        ST(0x190, c3);
        ST(0xd4, xmul(s3, w0)); ST(0x18c, xmul(s3, w1)); ST(0x174, xmul(s3, w2));
        axis_angle_matrix(SLOT(0x130), s3, c3, w0, w1, w2);
        SU32(E - 4, 0, SLOT(0x130));
        matrix_to_quat(SLOT(0x130), SLOT(0x160), 0x006b2d18, 0x30u * root, limb, ebp, pose);
        const rx_addr b = pose + 0x30u * U32(U32(limb, 0x68), 4);
        const double bw = F32(b, 0xc), bx = F32(b, 0), by = F32(b, 4), bz = F32(b, 8);
        const double qx2 = LD(0x160), qy2 = LD(0x15c), qz2 = LD(0x158), qw2 = LD(0x154);
        ST(0xe4, (float)dsub(dsub(dsub(dmul(bw, qw2), dmul(qx2, bx)), dmul(by, qy2)), dmul(bz, qz2)));
        ST(0xf0, (float)dsub(dadd(dadd(dmul(bx, qw2), dmul(qx2, bw)), dmul(by, qz2)), dmul(bz, qy2)));
        ST(0xe8, (float)dsub(dadd(dadd(dmul(bz, qw2), dmul(bw, qz2)), dmul(bx, qy2)), dmul(qx2, by)));
        ST(0xec, (float)dsub(dadd(dadd(dmul(by, qw2), dmul(bw, qy2)), dmul(qx2, bz)), dmul(bx, qz2)));
        SU32(b, 0x0, U32(SLOT(0xf0), 0)); SU32(b, 0x4, U32(SLOT(0xec), 0));
        SU32(b, 0x8, U32(SLOT(0xe8), 0)); SU32(b, 0xc, U32(SLOT(0xe4), 0));
        normalize_bone_quat(b, SLOT(0x174), E, 0x006b2f0c, ebp, 1);
        SU8(pose + 0x30u * U32(U32(limb, 0x68), 4), 0x20, 1);
    }
    ANIM_FRAME_LEAVE(A);
#undef SLOT
#undef ST
#undef LD
}
RX_FUNCTION(0x006b0eb0, rx_ik_two_bone_solve)

/* ---- InverseKinematicsAnimator ---- */

/* 006b2fb0 (not in the rx_anim list; EAX = skeleton, ECX = chain; stack: chain record, pose, dt, side; RET 0x10): the
 * alternative chain solver used when the switch byte 0x00d9d3e0 is set. */
static void ik_chain_alt_solve(rx_addr skel, rx_addr chain, rx_addr pose, uint32_t dt_bits, uint32_t side, rx_addr ret,
                               uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi) {
    rx_regs r = anim_regs();
    r.r[ANIM_EAX] = skel; r.r[ANIM_ECX] = chain;
    r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp; r.r[ANIM_ESI] = esi; r.r[ANIM_EDI] = edi;
    uint32_t a[4] = {chain, pose, dt_bits, side};
    ik_call_ex(0x006b2fb0, &r, 0, 4, a, ret);
}

/* 0083a7c0 (not in the rx_anim list; EDI = the animator's +0x28 object; stack: pose, bone count, root delta; RET 0xc). */
static void ik_post_pose(rx_addr obj, rx_addr pose, uint32_t bones, rx_addr delta, rx_addr ret,
                         uint32_t ebx, uint32_t ebp, uint32_t esi) {
    rx_regs r = anim_regs();
    r.r[ANIM_EDI] = obj; r.r[ANIM_ECX] = pose; r.r[ANIM_EDX] = bones;
    r.r[ANIM_EBX] = ebx; r.r[ANIM_EBP] = ebp; r.r[ANIM_ESI] = esi;
    uint32_t a[3] = {pose, bones, delta};
    ik_call_ex(0x0083a7c0, &r, 0, 3, a, ret);
}


/* 006b5300 InverseKinematicsAnimator::vf4 (thiscall (character, root delta out, pose); RET 0xc). Child +0x5c into a pool pose,
 * copied into the output pose (translations through the x87), then the two chains (+0x64 + 0x00 / 0x80: 006b0530, or 006b2fb0
 * when the byte 0x00d9d3e0 is set) and the two limbs (+0x64 + 0x100 / 0x180: 006b0eb0) with skeleton +0x54 and dt +0x6c. When
 * the object +0x2c has state (flags & 7) == 1 and +0x28 is set, 0083a7c0 post-processes the pose and the root delta and the
 * state becomes 2. The child's root delta goes out (always written). */
void rx_ik_animator_eval(rx_addr self, rx_addr ch, rx_addr delta_out, rx_addr pose) {
    ANIM_FRAME_ENTER(A, 0x30);                     /* sub esp, 0x24; push ebx; push esi; push edi */
    const rx_addr ebp = A_sp0 - 4, delta = A - 0x10;
    anim_pose_reset(ch, pose);
    SU32(A - 0x34, 0, 0x006b531b);
    const rx_addr pool = anim_pose_acquire(ch);
    SU32(A - 0x34, 0, 0x006b5322);
    SU32(A - 0x18, 0, pool);
    ik_eval_child(U32(self, 0x5c), ch, delta, pool, 0x006b5339);
    const uint32_t bones = U32(ch, 0x234);
    SU32(A - 0x14, 0, bones);
    for (uint32_t i = 0; i < bones; i++) {
        const rx_addr s = pool + 0x30u * i, d = pose + 0x30u * i;
        SU8(d, 0x20, U8(s, 0x20));
        SU32(d, 0x00, U32(s, 0x00)); SU32(d, 0x04, U32(s, 0x04)); SU32(d, 0x08, U32(s, 0x08)); SU32(d, 0x0c, U32(s, 0x0c));
        SU8(d, 0x21, U8(s, 0x21));
        SU32(d, 0x10, ik_x87_bits(U32(s, 0x10))); SU32(d, 0x14, ik_x87_bits(U32(s, 0x14))); SU32(d, 0x18, ik_x87_bits(U32(s, 0x18)));
    }
    rx_os_import(0x0091607c, 0, 0);    /* GetCurrentThreadId (result unused) */
    SU32(A - 0x34, 0, 0x006b53a0);
    const rx_addr skel = U32(self, 0x54), goals = U32(self, 0x64);
    const uint32_t dt = ik_x87_bits(U32(self, 0x6c));
    if (U8(0, 0x00d9d3e0)) {
        ik_chain_alt_solve(skel, goals, pose, dt, 0, 0x006b53c2, self, ebp, ch, pool);
        ik_chain_alt_solve(skel, goals + 0x80, pose, dt, 1, 0x006b53de, self, ebp, ch, pool);
    } else {
        for (uint32_t k = 0; k < 0x100; k += 0x80) {
            SU32(A - 0x1c, 0, k);
            SU32(A - 0x34, 0, dt); SU32(A - 0x38, 0, 0x006b5401);
            rx_set_sp(A - 0x38);
            rx_ik_chain_ccd(skel, U32(self, 0x64) + k, pose, rx_bf(dt));
            rx_set_sp(A - 0x30);
        }
        SU32(A - 0x1c, 0, 0x100);
    }
    for (int k = 0; k < 2; k++) {                  /* the limbs: push dt, pose, limb; return address */
        SU32(A - 0x34, 0, ik_x87_bits(U32(self, 0x6c))); SU32(A - 0x38, 0, pose);
        SU32(A - 0x3c, 0, U32(self, 0x64) + 0x100u + 0x80u * (uint32_t)k); SU32(A - 0x40, 0, k ? 0x006b5450 : 0x006b5433);
        rx_set_sp(A - 0x40);
        rx_ik_two_bone_solve(skel, U32(self, 0x64) + 0x100u + 0x80u * (uint32_t)k, pose, rx_bf(U32(A - 0x34, 0)));
        rx_set_sp(A - 0x30);
    }
    const rx_addr state = U32(self, 0x2c);
    if (state && (U32(state, 0) & 7) == 1 && U32(self, 0x28)) {
        ik_post_pose(U32(self, 0x28), pose, bones, delta, 0x006b547d, self, ebp, ch);
        const rx_addr st = U32(self, 0x2c);
        SU32(st, 0, (U32(st, 0) & ~5u) | 2u);
    }
    SU32(delta_out, 0, U32(delta, 0)); SU32(delta_out, 4, U32(delta, 4)); SU32(delta_out, 8, U32(delta, 8));
    /* the pool pose back: used flag [+0x22c][slot] = 0 under the pool's critical section */
    anim_enter_cs(ch + 0x208);
    SU32(A - 0x38, 0, 0x006b54be);
    const int32_t records = (int32_t)(pool - U32(ch, 0x228)) / 0x30;
    const uint32_t slot = (uint32_t)records / U32(ch, 0x234);
    SU32(U32(ch, 0x22c), 4u * slot, 0);
    anim_leave_cs(ch + 0x208);
    SU32(A - 0x38, 0, 0x006b54f1);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x006b5300, rx_ik_animator_eval)

/* 006b57a0 InverseKinematicsAnimator::vf3 (thiscall (rate); RET 4): passes the rate (through the x87) to the child +0x5c's vf3. */
void rx_ik_animator_set_rate(rx_addr self, float rate) {
    const rx_addr child = U32(self, 0x5c);
    if (!child) return;
    anim_set_rate_child(child, ik_x87_bits(rx_fb(rate)), 0x006b57b8);
}
RX_FUNCTION(0x006b57a0, rx_ik_animator_set_rate)

/* 006b04d0 (ECX = IK animator; plain ret; used by the rider when a crash / reset re-enables the IK): when the IK is on (+0x70),
 * every goal record of +0x64 (count +0x60, 0x80 bytes each) not engaged yet becomes engaged (+0x08 = 1) with a 0.25 s blend
 * (+0x70 = 0.25 (0x92ff84), time +0x40 = 0.25 - time when below 0.25, else 0). Returns the goals visited (the EDX it leaves). */
uint32_t rx_ik_animator_engage_goals(rx_addr self) {
    if (U32(self, 0x70) == 0) return 0;
    uint32_t i = 0;
    for (; i < U32(self, 0x60); i++) {
        const rx_addr g = U32(self, 0x64) + 0x80u * i;
        if (U32(g, 0x08) != 1) {
            const float t = F32(g, 0x40);
            SF32(g, 0x70, 0.25f);
            SF32(g, 0x40, t >= 0.25f ? 0.0f : rx_subs(0.25f, t));
        }
        SU32(g, 0x08, 1);
    }
    return i;
}
RX_FUNCTION(0x006b04d0, rx_ik_animator_engage_goals)
