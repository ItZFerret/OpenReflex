/* loop_cam_shake.c -- the chase camera's shake (MXThirdPersonCamera +0x6a0..+0x748): an impact shake 006519b0 (a decaying
 * oscillation of the camera about its own axes) and the noise shake 00652800 (Noise1D-driven, +0x700). Both turn the camera's
 * basis into a quaternion, compose the shake rotation (+0x730, a quaternion the chase camera multiplies its view by) and rotate
 * the shake offset into world space (+0x740, added to the camera position). MXThirdPersonCamera::vf19 (loop_cam_tpc.c) reads
 * both.
 *
 * Fields: +0x6a0 shaking, +0x6a4 duration, +0x6a8 elapsed (the envelope w = 1 - (elapsed / duration)^2), +0x6ac / +0x6b0 the two
 * angle amplitudes, +0x6b4 the shake roll, +0x6c0 / +0x6d0 / +0x6e0 / +0x6f0 the offset amplitudes, axis weights, phase rates and
 * phases (16-byte vectors), +0x700 noise mode, +0x704 noise clock, +0x708 noise rate, +0x710 / +0x720 noise amplitudes.
 * The packed SSE code is transliterated lane by lane (the products / sums of each lane in the original's order). */
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_cam_shake.h"
#include "loop_util.h"
#include "anim_math.h"
#include "phys_mech.h"
#include "world_veh.h"

typedef struct { float v[4]; } sv4;
static sv4 sv4_ld(rx_addr a) { sv4 r; for (int i = 0; i < 4; i++) r.v[i] = F32(a, 4u * (uint32_t)i); return r; }
static void sv4_st(rx_addr a, sv4 x) { for (int i = 0; i < 4; i++) SF32(a, 4u * (uint32_t)i, x.v[i]); }
static sv4 sv4_mul(sv4 a, sv4 b) { sv4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] * b.v[i]; return r; }
static sv4 sv4_add(sv4 a, sv4 b) { sv4 r; for (int i = 0; i < 4; i++) r.v[i] = a.v[i] + b.v[i]; return r; }
static sv4 sv4_splat(float x) { sv4 r = {{x, x, x, x}}; return r; }
/* shufps a, b, imm */
static sv4 sv4_shuf(sv4 a, sv4 b, unsigned imm) {
    sv4 r = {{a.v[imm & 3], a.v[(imm >> 2) & 3], b.v[(imm >> 4) & 3], b.v[(imm >> 6) & 3]}};
    return r;
}
static sv4 sv4_c(rx_addr a) { return sv4_ld(a); }   /* the .rdata sign vectors 0x93a1f0 .. 0x93a220 */

/* 00853180 Noise1D (cdecl t -> xmm0) (rx_phys) at the original's stack depth */
static float shake_noise(float t, rx_addr ret) {
    const uint32_t a[1] = {rx_fb(t)};
    const rx_addr s = loop_push(1, a, ret); const float r = phys_noise_1d(t); loop_pop(s); return r;
}
/* 00401630 Vec3_Cross (EAX = a, ECX = c; cdecl out): out = c x a (rx_world) */
static void shake_cross(rx_addr out, rx_addr c, rx_addr a, rx_addr ret) {
    const uint32_t v[1] = {out};
    const rx_addr s = loop_push(1, v, ret); rx_world_vec3_cross(out, c, a); loop_pop(s);
}

/* The basis (right R = (rx, R +4, R +8), up U, forward F) as a quaternion into Q (x, y, z, w), the largest-diagonal method:
 * q0..qw = the four (+-rx +- U.y +- F.z + 1) / 4, kept at s0 / s1 / sw / s2; the largest picks the branch (sqrt of it, the other
 * three components (a -+ b) * (0.25 / sqrt)). `fy_first`: the original adds (F.y + U.z) instead of (U.z + F.y) in two branches. */
static void shake_basis_quat(float rx, rx_addr R, rx_addr U, rx_addr F, rx_addr Q, rx_addr s0, rx_addr s1, rx_addr sw,
                             rx_addr s2, int fy_first) {
    const float uy = F32(U, 4), fz = F32(F, 8);
    const float a = rx + 1.0f /* 0x931f24 */, b = 1.0f - rx;
    const float q0 = ((a - uy) - fz) * 0.25f /* 0x92ff84 */;
    const float q1 = ((uy + b) - fz) * 0.25f;
    const float qw = ((uy + a) + fz) * 0.25f;
    const float q2 = ((b - uy) + fz) * 0.25f;
    SF32(sw, 0, qw); SF32(s0, 0, q0); SF32(s1, 0, q1); SF32(s2, 0, q2);
    float m = (q0 > q1) ? q0 : q1;
    if (!(m > q2)) m = q2;
    if (!(m > qw)) m = qw;
    const float ux = F32(U, 0), uz = F32(U, 8), fx = F32(F, 0), fy = F32(F, 4);
    const float ry = F32(R, 4), rz = F32(R, 8);
    const float uzfy = fy_first ? (fy + uz) : (uz + fy);
    if (m == qw) {
        const float s = rx_sqrts(F32(sw, 0));
        const float k = 0.25f / s;
        SF32(Q, 0xc, s);
        SF32(Q, 0, (uz - fy) * k);
        SF32(Q, 4, (fx - rz) * k);
        SF32(Q, 8, (ry - ux) * k);
        return;
    }
    float m2 = (q1 > q2) ? q1 : q2;
    if (!(m2 > q0)) m2 = q0;
    if (m2 == q0) {
        const float s = rx_sqrts(F32(s0, 0));
        const float k = 0.25f / s;
        SF32(Q, 0, s);
        SF32(Q, 0xc, (uz - fy) * k);
        SF32(Q, 4, (ry + ux) * k);
        SF32(Q, 8, (rz + fx) * k);
    } else if (q1 > q2) {
        const float s = rx_sqrts(F32(s1, 0));
        const float k = 0.25f / s;
        SF32(Q, 4, s);
        SF32(Q, 0xc, (fx - rz) * k);
        SF32(Q, 0, (ry + ux) * k);
        SF32(Q, 8, uzfy * k);
    } else {
        const float s = rx_sqrts(F32(s2, 0));
        const float k = 0.25f / s;
        SF32(Q, 8, s);
        SF32(Q, 0xc, (ry - ux) * k);
        SF32(Q, 0, (rz + fx) * k);
        SF32(Q, 4, uzfy * k);
    }
}

/* The shake rotation from three half angles' sines / cosines (a, b, c): (X, Y, Z, W). */
static void shake_compose(float sa, float ca, float sb, float cb, float sc, float cc, float* X, float* Y, float* Z, float* W) {
    const float cbc = cc * cb, sbc = sc * sb;
    *W = (sbc * sa) + (cbc * ca);
    *X = (cbc * sa) + (sbc * ca);
    *Y = ((sb * ca) * cc) - ((cb * sa) * sc);
    *Z = ((cb * ca) * sc) - ((sb * sa) * cc);
}

/* The product of the shake rotation S = (X, Y, Z, W) and the camera's quaternion Q in double precision, into out (x, y, z, w). */
static void shake_qmul(float X, float Y, float Z, float W, rx_addr Q, rx_addr out) {
    const double qx = F32(Q, 0), qy = F32(Q, 4), qz = F32(Q, 8), qw = F32(Q, 0xc);
    const double x = X, y = Y, z = Z, w = W;
    SF32(out, 0xc, (float)((((w * qw) - (x * qx)) - (y * qy)) - (z * qz)));
    SF32(out, 0, (float)((((w * qx) + (qw * x)) + (z * qy)) - (y * qz)));
    SF32(out, 4, (float)((((w * qy) + (y * qw)) + (qz * x)) - (z * qx)));
    SF32(out, 8, (float)((((w * qz) + (z * qw)) + (y * qx)) - (qy * x)));
}

/* The offset L (lane 3 masked to 0 by 0x92db80) rotated by the quaternion R (the packed sandwich with the sign vectors
 * 0x93a1f0 / 0x93a200 / 0x93a210 / 0x93a220), into cam +0x740 (all four lanes). */
static void shake_rotate(sv4 R, sv4 L, rx_addr out) {
    L.v[3] = rx_bf(rx_fb(L.v[3]) & 0u);
    const sv4 C1f0 = sv4_c(0x0093a1f0), C200 = sv4_c(0x0093a200), C210 = sv4_c(0x0093a210), C220 = sv4_c(0x0093a220);
    const sv4 conj = sv4_mul(R, C220);
    sv4 s = sv4_shuf(conj, conj, 0x1b);
    sv4 p3 = sv4_mul(sv4_shuf(L, L, 0x00), s);
    s = sv4_shuf(s, s, 0xb1);
    sv4 p4 = sv4_mul(sv4_shuf(L, L, 0x55), s);
    s = sv4_shuf(s, s, 0x1b);
    sv4 p2 = sv4_mul(s, sv4_shuf(L, L, 0xaa));
    p2 = sv4_mul(p2, C210);
    p4 = sv4_mul(p4, C200);
    p2 = sv4_add(p2, p4);
    p3 = sv4_mul(p3, C1f0);
    sv4 p0 = sv4_mul(sv4_shuf(L, L, 0xff), conj);
    p0 = sv4_add(p0, p3);
    const sv4 T = sv4_add(p2, p0);
    sv4 t = sv4_shuf(T, T, 0x1b);
    sv4 r3 = sv4_mul(sv4_shuf(R, R, 0x00), t);
    r3 = sv4_mul(r3, C1f0);
    t = sv4_shuf(t, t, 0xb1);
    const sv4 t5 = sv4_shuf(t, t, 0x1b);
    sv4 r4 = sv4_mul(sv4_shuf(R, R, 0x55), t);
    sv4 r5 = sv4_mul(t5, sv4_shuf(R, R, 0xaa));
    r5 = sv4_mul(r5, C210);
    sv4 r0 = sv4_mul(sv4_shuf(R, R, 0xff), T);
    r4 = sv4_mul(r4, C200);
    r0 = sv4_add(r0, r3);
    r5 = sv4_add(r5, r4);
    r5 = sv4_add(r5, r0);
    sv4_st(out, r5);
}

/* the end of both shakes: done when elapsed == duration (the shake rotation / offset reset), elapsed += dt (at most the duration) */
static void shake_finish(rx_addr cam, float dt, int reset_phase) {
    if (F32(cam, 0x6a8) == F32(cam, 0x6a4)) {
        SU8(cam, 0x6a0, 0);
        if (reset_phase) for (uint32_t o = 0; o < 16; o += 4) SU32(cam, 0x6f0 + o, U32(0, 0x00d61670 + o));
        SF32(cam, 0x740, 0.0f); SF32(cam, 0x744, 0.0f); SF32(cam, 0x748, 0.0f);
        SF32(cam, 0x730, 0.0f); SF32(cam, 0x734, 0.0f); SF32(cam, 0x738, 0.0f);
        SF32(cam, 0x73c, 1.0f /* 0x931f24 */);
    }
    const float v = dt + F32(cam, 0x6a8), d = F32(cam, 0x6a4);
    SF32(cam, 0x6a8, (v > d) ? d : v);
}

/* 00652800 (LTCG: ESI = camera; stack dt; ret 4): the noise shake. The camera's up (+0x30 of the Camera +0x90) and forward give
 * the basis quaternion; the noise clock +0x704 runs at +0x708 * w; three Noise1D samples (clock, + 5, + 10) times the amplitudes
 * +0x710 * w are the half angles of the shake rotation (stored +0x730); three more (clock + 15, + 20, + 25) times +0x720 * w are
 * the offset, rotated by the product into +0x740. Frame: push ebp; and esp, -16; sub esp, 0x80: E = the aligned base - 0x80. */
void rx_cam_noise_shake(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x80u;
    rx_set_sp(E);
    const float r = F32(cam, 0x6a8) / F32(cam, 0x6a4);
    SF32(E, 0xc, 1.0f - (r * r));
    rx_cam_get_position(cam + 0x90u, E + 0x30u);
    rx_cam_get_forward(cam + 0x90u, E + 0x70u);
    rx_cam_get_right(cam + 0x90u, E + 0x30u);          /* 007923a0: the Camera's +0x30 (its up row) */
    shake_cross(E + 0x50u, E + 0x30u, E + 0x70u, 0x00652868);
    float rx = F32(E, 0x50);
    {
        const float ry = F32(E, 0x54), rz = F32(E, 0x58);
        const float l = ((ry * ry) + (rx * rx)) + (rz * rz);
        SF32(E, 0x2c, l);
        if (l > 0.0f /* 0x92e12c */) {
            const float inv = 1.0f / rx_sqrts(l);
            rx = inv * F32(E, 0x50);
            const float y = inv * F32(E, 0x54), z = inv * F32(E, 0x58);
            SF32(E, 0x54, y);
            SF32(E, 0x58, z);
        }
    }
    shake_basis_quat(rx, E + 0x50u, E + 0x30u, E + 0x70u, E + 0x10u, E + 0x4cu, E + 0x6cu, E + 0x2cu, E + 0x8u, 1);
    const float w = F32(E, 0xc);
    const float clock = ((F32(cam, 0x708) * w) * dt) + F32(cam, 0x704);
    SF32(E, 0x8, clock);
    SF32(cam, 0x704, clock);
    rx_set_sp(E - 4u);
    const float n0 = shake_noise(cam_x87_f32(E + 0x8u), 0x00652b7f);
    SF32(E, 0x8, n0);
    const float n1 = shake_noise(F32(cam, 0x704) + 5.0f /* 0x935890 */, 0x00652b9f);
    const float n2 = shake_noise(F32(cam, 0x704) + 10.0f /* 0x92fd88 */, 0x00652bc0);
    rx_set_sp(E);
    sv4_st(E + 0x70u, sv4_splat(w));
    sv4 N = {{n0, n1, n2, n2}};
    N = sv4_mul(N, sv4_mul(sv4_ld(cam + 0x710u), sv4_splat(w)));
    sv4_st(E + 0x30u, N);
    const float h2 = N.v[2] * 0.5f /* 0x9322cc */;
    SF32(E, 0x6c, h2);
    float s0, c0, s1, c1, s2, c2;
    anim_sincos(N.v[0] * 0.5f, &s0, &c0);
    anim_sincos(N.v[1] * 0.5f, &s1, &c1);
    anim_sincos(F32(E, 0x6c), &s2, &c2);
    SF32(E, 0x2c, c0);
    float X, Y, Z, W;
    shake_compose(s0, c0, s1, c1, s2, c2, &X, &Y, &Z, &W);
    SF32(E, 0x30, X); SF32(E, 0x34, Y);
    SF32(cam, 0x730, X); SF32(cam, 0x734, Y);
    SF32(E, 0x3c, W); SF32(E, 0x38, Z);
    SF32(cam, 0x738, Z); SF32(cam, 0x73c, W);
    shake_qmul(X, Y, Z, W, E + 0x10u, E + 0x30u);
    rx_set_sp(E - 4u);
    const float m0 = shake_noise(F32(cam, 0x704) + 15.0f /* 0x9322d0 */, 0x0065312d);
    SF32(E, 0x8, m0);
    const float m1 = shake_noise(F32(cam, 0x704) + 20.0f /* 0x935aac */, 0x0065314d);
    const float m2 = shake_noise(F32(cam, 0x704) + 25.0f /* 0x92e134 */, 0x0065316e);
    const sv4 amp = sv4_mul(sv4_ld(cam + 0x720u), sv4_ld(E + 0x70u));
    sv4 L = {{F32(E, 0x8), m1, m2, m2}};
    L = sv4_mul(L, amp);
    shake_rotate(sv4_ld(E + 0x30u), L, cam + 0x740u);
    rx_set_sp(E);
    shake_finish(cam, dt, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00652800, rx_cam_noise_shake)

/* 006519b0 (thiscall camera, dt; ret 4): the camera shake. In noise mode (+0x700) the noise shake; else the impact shake: the
 * basis quaternion from the camera's forward / up; the phases +0x6f0 advance by +0x6e0 * dt; the half angles of the shake
 * rotation from the phases' sines (amplitudes +0x6b0 / +0x6ac times w and the axis weights +0x6d0, swapped and signed by the
 * roll +0x6b4); the offset (+0x6c0 * w times the sines) turned by the roll, rotated by the product into +0x740. Frame: push ebp;
 * and esp, -16; sub esp, 0x6c; push esi: E = the aligned base - 0x70. */
void rx_cam_shake(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x70u;
    rx_set_sp(E);
    if (U8(cam, 0x6a0) == 0) { rx_set_sp(sp0); return; }
    if (U8(cam, 0x700) != 0) {
        const uint32_t a[1] = {cam_x87_bits(rx_fb(dt))};
        const rx_addr s = loop_push(1, a, 0x006519de);
        rx_cam_noise_shake(cam, rx_bf(a[0]));
        loop_pop(s);
        rx_set_sp(sp0);
        return;
    }
    const float r = F32(cam, 0x6a8) / F32(cam, 0x6a4);
    SF32(E, 0x64, 1.0f - (r * r));
    SU32(E, 0x10, U32(cam, 0xd0)); SU32(E, 0x14, U32(cam, 0xd4)); SU32(E, 0x18, U32(cam, 0xd8));
    SU32(E, 0x50, U32(cam, 0xc0)); SU32(E, 0x54, U32(cam, 0xc4)); SU32(E, 0x58, U32(cam, 0xc8));
    shake_cross(E + 0x40u, E + 0x50u, E + 0x10u, 0x00651a74);
    float rx = F32(E, 0x40);
    {
        const float ry = F32(E, 0x44), rz = F32(E, 0x48);
        const float l = ((ry * ry) + (rx * rx)) + (rz * rz);
        SF32(E, 0x3c, l);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            rx = inv * F32(E, 0x40);
            const float y = inv * F32(E, 0x44), z = inv * F32(E, 0x48);
            SF32(E, 0x44, y);
            SF32(E, 0x48, z);
        }
    }
    shake_basis_quat(rx, E + 0x40u, E + 0x50u, E + 0x10u, E + 0x20u, E + 0x38u, E + 0x8u, E + 0x3cu, E + 0xcu, 0);
    /* the phases */
    {
        const sv4 ph = sv4_add(sv4_mul(sv4_splat(dt), sv4_ld(cam + 0x6e0u)), sv4_ld(cam + 0x6f0u));
        sv4_st(cam + 0x6f0u, ph);
        sv4_st(E + 0x10u, ph);
    }
    const float b0 = fabsf(F32(E, 0x10) + 1.5707964f /* 0x977508 */);
    SF32(E, 0x68, b0);
    const float sin0 = cam_trig_poly(cam_trig_reduce(b0));
    SF32(E, 0x50, sin0);
    const float b1 = fabsf(F32(E, 0x14) + 1.5707964f);
    SF32(E, 0x6c, b1);
    const float sin1 = cam_trig_poly(cam_trig_reduce(b1));
    const float w = F32(E, 0x64);
    sv4_st(E + 0x10u, sv4_ld(cam + 0x6d0u));
    const float a0 = ((F32(cam, 0x6b0) * w) * F32(E, 0x50)) * F32(E, 0x10);
    const float a1 = ((F32(cam, 0x6ac) * w) * sin1) * F32(E, 0x14);
    const float roll = F32(cam, 0x6b4);
    float p, q;
    if (roll > 0.0f) { p = a1; q = a0; }
    else if (0.0f > roll) { p = a1; q = -0.0f /* 0x9774a8 */ - a0; }
    else { p = a0; q = a1; }
    const float hq = q * 0.5f /* 0x9322cc */;
    SF32(E, 0xc, hq);
    const float h3 = 0.0f * 0.5f;
    SF32(E, 0x38, h3);
    float s1, c1, s2, c2, s3, c3;
    anim_sincos(p * 0.5f, &s1, &c1);
    SF32(E, 0x3c, c1);
    anim_sincos(F32(E, 0xc), &s2, &c2);
    anim_sincos(F32(E, 0x38), &s3, &c3);
    SF32(E, 0x8, c3);
    float X, Y, Z, W;
    shake_compose(s1, c1, s2, c2, s3, c3, &X, &Y, &Z, &W);
    SF32(E, 0x10, X); SF32(E, 0x14, Y);
    SF32(cam, 0x730, X); SF32(cam, 0x734, Y);
    SF32(E, 0x1c, W); SF32(E, 0x18, Z);
    SF32(cam, 0x738, Z); SF32(cam, 0x73c, W);
    shake_qmul(X, Y, Z, W, E + 0x20u, E + 0x40u);
    /* the offset */
    const float sa = cam_trig_poly(cam_trig_reduce(F32(E, 0x68)));
    SF32(E, 0x54, sa);
    const float sb = cam_trig_poly(cam_trig_reduce(F32(E, 0x6c)));
    SF32(E, 0x50, sb);
    SF32(E, 0x58, 0.0f);
    sv4 L = sv4_mul(sv4_mul(sv4_splat(w), sv4_ld(cam + 0x6c0u)), sv4_ld(E + 0x50u));
    sv4_st(E + 0x10u, L);
    float rs, rc;
    anim_sincos(F32(cam, 0x6b4), &rs, &rc);
    {
        const float l0 = F32(E, 0x10), l1 = F32(E, 0x14);
        SF32(E, 0x14, (l1 * rc) + (l0 * rs));
        SF32(E, 0x10, (l0 * rc) - (l1 * rs));
    }
    shake_rotate(sv4_ld(E + 0x40u), sv4_ld(E + 0x10u), cam + 0x740u);
    shake_finish(cam, dt, 1);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x006519b0, rx_cam_shake)
