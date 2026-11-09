/* phys_quat.c -- packed animation quaternions (0x0042ce40): 8 bytes = u16 angle (pi / 4096 units: 0..2pi for the half angle's
 * double) + 3 x int16 axis components (1 / 32767 units). Scalar SSE with float sin / cos polynomials and a double precision
 * acos polynomial. */
#include "phys_quat.h"
#include "crt.h"

#define K_PI       0x40490fdbu   /* 00977498 */
#define K_2PI      0x40c90fdbu   /* 0097749c */
#define K_INV_2PI  0x3e22f983u   /* 009774a0 */
#define K_HALF_PI  0x3fc90fdbu   /* 00977508 */

/* cos (00935d3c..00935d50) and sin (00935d54..00935d68) polynomial coefficients on [-pi, pi] */
static const uint32_t k_cos[6] = {0x3f7ffff3, 0xbeffff3d, 0xbab5a662, 0x37cb076b, 0x3d2aa372, 0xb46beed0};
static const uint32_t k_sin[6] = {0x3f7ffff9, 0xbe2aaa5c, 0x3c0884a2, 0xb94fb563, 0x36352cc0, 0xb2aee90a};
#define C(i) rx_bf(k_cos[i])
#define S(i) rx_bf(k_sin[i])

/* pi - (z mod 2pi): the reduction every evaluation uses (CVTTSS2SI of z / 2pi; z >= 0 except in the decode's sign case) */
static float pi_minus_wrapped(float z) {
    const float turns = (float)rx_cvtt(z * rx_bf(K_INV_2PI));
    return rx_bf(K_PI) - (z - turns * rx_bf(K_2PI));
}
/* the cos polynomial in y^2: ((y2 c5 + c3) y4 + (y2 c2 + c4)) y4 + (y2 c1 + c0) -- cos(y) for y in [-pi, pi] */
static float cos_poly(float y2) {
    const float y4 = y2 * y2;
    return ((y2 * C(5) + C(3)) * y4 + (y2 * C(2) + C(4))) * y4 + (y2 * C(1) + C(0));
}
/* the sin polynomial in y^2 without the final factor y: ((y2 s5 + s4) y4 + (y2 s3 + s2)) y4 + (y2 s1 + s0) */
static float sin_poly(float y2) {
    const float y4 = y2 * y2;
    return ((y2 * S(5) + S(4)) * y4 + (y2 * S(3) + S(2))) * y4 + (y2 * S(1) + S(0));
}
static float float_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }   /* Float_Abs 00401000 (andps 0x7fffffff) */
static float sign_of(float x) { return x >= 0.0f ? 1.0f : F32(0, 0x009cd9e8); }   /* -1.0f is writable data */

/* acos(z) as the original evaluates it: c = |z|; for c >= cos 45 (0x009ddfc8, writable data) through
 * asin(sqrt(1 - z^2)), else asin(c), with the rational approximation
 * x (1 - 1.2763687 x2 + (0.3993034 - 0.019378424 x2) x4) / (1 - 1.4430355 x2 + (0.5648093 - 0.04992851 x2) x4) of asin
 * (doubles 00977788..009777b0, 1.0 00977638, pi/2 00977510), each partial result rounded to float as the original's
 * cvtsd2ss / cvtpd2ps; a negative (or NaN) z gives pi - . */
static float acos_poly(float z) {
    const double one = 1.0, half_pi = rx_bd(0x3ff921fb60000000ull);
    const float c = float_abs(z);
    const int wide = c >= F32(0, 0x009ddfc8);
    float x = c;
    if (wide) {
        const float z2 = (float)((double)z * (double)z);
        x = phys_sqrtf(float_abs((float)(1.0 - (double)z2)));
    }
    const float x2 = (float)((double)x * (double)x);
    const float x4 = (float)((double)x2 * (double)x2);
    const float den_hi = (float)(rx_bd(0x3fe212eb00000000ull) - (double)x2 * rx_bd(0x3fa9903ac0000000ull));
    const float den_lo = (float)(one - (double)x2 * rx_bd(0x3ff716ac60000000ull));
    const float inv_den = (float)(one / ((double)den_hi * (double)x4 + (double)den_lo));
    const float num_hi = (float)(rx_bd(0x3fd98e2fe0000000ull) - (double)x2 * rx_bd(0x3f93d7f000000000ull));
    const float num_lo = (float)(one - (double)x2 * rx_bd(0x3ff46c01a0000000ull));
    const float num = (float)(((double)num_hi * (double)x4 + (double)num_lo) * (double)x);
    float asin_x = (float)((double)inv_den * (double)num);
    if (wide) asin_x = (float)(half_pi - (double)asin_x);
    if (!(z >= 0.0f)) asin_x = rx_bf(0x80000000u) - asin_x;
    return (float)(half_pi - (double)asin_x);
}

/* One packed quaternion -> (x, y, z, w) with w >= 0: the half angle h = angle * pi/4096 (009778fc) gives
 * w = cos h = -cos_poly(pi - h), sin h = sin_poly * y; axis * sin h / 32767 (0x009c6400, writable); the sign of w moves to
 * the axis (w = |w|). */
static rx_v4 unpack(rx_addr p) {
    const float angle = (float)(int32_t)U16(p, 0) * rx_bf(0x3a490fdbu);
    const float s = sign_of(angle);
    const float y = pi_minus_wrapped(s * angle);
    const float y2 = y * y;
    const float w = rx_bf(0x80000000u) - cos_poly(y2);                    /* -0.0f - P (009774a8) */
    const float scale = sin_poly(y2) * y * s * F32(0, 0x009c6400);
    const float sw = sign_of(w);
    rx_v4 q;
    q.v[0] = (float)(int16_t)U16(p, 2) * scale;
    q.v[1] = (float)(int16_t)U16(p, 4) * scale;
    q.v[2] = (float)(int16_t)U16(p, 6) * scale;
    q.v[3] = float_abs(w);
    q.v[0] = q.v[0] * sw;
    q.v[1] = sw * q.v[1];
    q.v[2] = sw * q.v[2];
    return q;
}

/* the slerp weights of two unit quaternions with dot product `dot` at t: theta = acos(|dot|) from a double polynomial (|dot| >=
 * cos 45 (0x009ddfc8, writable): through asin of sqrt(1 - dot^2)); w_a = sin((1 - t) theta) / sin theta and w_b = sign(dot)
 * sin(t theta) / sin theta, each sine evaluated as cos_poly(pi - wrap(|x + pi/2|)); |dot| >= 1 gives (1, 0). (Also inlined by
 * the blend sampler 00840b70.) */
void phys_quat_slerp_weights(float dot, float t, float* wa, float* wb) {
    const float dot_sign = dot >= 0.0f ? 1.0f : F32(0, 0x009cd9e8);   /* comiss with 0.0 (0092e12c) */
    const float c = float_abs(float_abs(dot));                        /* Float_Abs, Float_Abs_Thunk */
    const float theta = acos_poly(c);
    /* 1 / sin theta, sin x = cos_poly((pi - wrap(|x + pi/2|))^2) */
    float y = pi_minus_wrapped(float_abs(theta + rx_bf(K_HALF_PI)));
    const float inv_sin = 1.0f / cos_poly(y * y);
    const float t_theta = theta * t;
    if (c >= 1.0f) {
        *wa = 1.0f;
        *wb = 0.0f;
    } else {
        y = pi_minus_wrapped(float_abs((theta - t_theta) + rx_bf(K_HALF_PI)));
        *wa = cos_poly(y * y) * inv_sin;
        y = pi_minus_wrapped(float_abs(t_theta + rx_bf(K_HALF_PI)));
        *wb = cos_poly(y * y) * dot_sign * inv_sin;
    }
}

rx_v4 phys_quat_unpack(rx_addr packed) { return unpack(packed); }
float phys_acos_poly(float z) { return acos_poly(z); }

/* 0042ce40 QuatSlerp_Packed (EAX = a, ESI = b, EDI = out float[4], cdecl t): slerp(a, b, t) along the shortest arc.
 * theta = acos(|dot|) from a double polynomial (|dot| >= cos 45 (0x009ddfc8, writable): through asin of sqrt(1 - dot^2));
 * the weights sin((1 - t) theta) / sin theta and sign(dot) sin(t theta) / sin theta, each sine evaluated as
 * cos_poly(pi - wrap(|x + pi/2|)); |dot| >= 1 gives the weights (1, 0). Stores w, x, y, z. */
void phys_quat_slerp_packed(rx_addr a, rx_addr b, rx_addr out, float t) {
    const rx_v4 qa = unpack(a);
    const rx_v4 qb = unpack(b);
    const float dot = ((qb.v[0] * qa.v[0] + qb.v[1] * qa.v[1]) + qb.v[2] * qa.v[2]) + qb.v[3] * qa.v[3];
    float w_a, w_b;
    phys_quat_slerp_weights(dot, t, &w_a, &w_b);
    SF32(out, 0xc, w_b * qb.v[3] + w_a * qa.v[3]);
    SF32(out, 0x0, w_b * qb.v[0] + w_a * qa.v[0]);
    SF32(out, 0x4, w_b * qb.v[1] + w_a * qa.v[1]);
    SF32(out, 0x8, qb.v[2] * w_b + w_a * qa.v[2]);
}
RX_FUNCTION(0x0042ce40, phys_quat_slerp_packed)

/* 0042db20 (ECX = base point*, EDX = up vector*, stack: probe P, target point*, rotation q*, rotation r*; RET 0x10): sets up
 * a probe frame (callers: BikeWheel_QueryTerrainContact, the simulations' wheel / body probes):
 *   P+0x40 = *up, P+0x30 = q * v * conj(q) with v = the runtime vec4 at 0x00d615c0 (w masked: its xyz rotated by q),
 *   P+0x50 = *target, P+0x70 = (target - base) scaled to the length vec4 P+0x10 (|d|^2 summed as (x + z) + y),
 *   P+0x60 = base + P+0x70, P+0x130 = up, P+0x120 = normalize(P+0x30 x up), P+0x140 = up x P+0x120,
 *   P+0x90 = conj(r) * (P+0x70.xyz * r).
 * Every result goes through memory as the original's (later steps read the stored vectors back). */
void phys_probe_frame_setup(rx_addr base, rx_addr up, rx_addr probe, rx_addr target, rx_addr q, rx_addr r) {
    v4_st(probe + 0x40, v4_ld(up));
    const rx_v4 v = v4_xyz(v4_ld(0x00d615c0)), rot = v4_ld(q);
    v4_st(probe + 0x30, phys_quat_mul(rot, phys_quat_mul(v, phys_quat_conj(rot))));
    v4_st(probe + 0x50, v4_ld(target));
    v4_st(probe + 0x70, v4_sub(v4_ld(probe + 0x50), v4_ld(base)));
    const rx_v4 d = v4_ld(probe + 0x70), p = v4_mul(d, d);
    const float len = phys_sqrt((p.v[0] + p.v[2]) + p.v[1]);
    v4_st(probe + 0x70, v4_mul(v4_div(v4_ld(probe + 0x10), v4_splat(len)), v4_ld(probe + 0x70)));
    v4_st(probe + 0x60, v4_add(v4_ld(probe + 0x70), v4_ld(base)));
    v4_st(probe + 0x130, v4_ld(probe + 0x40));
    v4_st(probe + 0x120, phys_cross_ba(v4_ld(probe + 0x130), v4_ld(probe + 0x30)));
    v4_st(probe + 0x120, phys_normalize3(v4_ld(probe + 0x120)));
    v4_st(probe + 0x140, phys_cross_ba(v4_ld(probe + 0x120), v4_ld(probe + 0x130)));
    const rx_v4 rr = v4_ld(r);
    const rx_v4 dr = phys_quat_mul(v4_xyz(v4_ld(probe + 0x70)), rr);
    v4_st(probe + 0x90, phys_quat_mul_ex(phys_quat_conj(rr), dr, 1));
}
RX_FUNCTION(0x0042db20, phys_probe_frame_setup)

/* atan2(y, x) through MSVCR90's _CIatan2 (crt_ci_atan2: st1 = y, st0 = x, main-thread control word), stored as a float */
static float atan2_f(float y, float x) {
    return rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y), rx_f80_from_f32(x), CRT_CW_MAIN), 0);
}
/* the limit for an azimuth a in (-pi, pi]: linear between limit[i] and limit[j] over each quarter turn (i / j swapped
 * between the quarters), as the original's four-way case split */
static float quarter_limit(float a, float limit_i, float limit_j) {
    const float half_pi = rx_bf(K_HALF_PI);
    float lo = limit_i, hi = limit_j;
    if (a < 0.0f || a != a || !(half_pi >= a)) {               /* not in [0, pi/2] */
        if (a > half_pi) { lo = limit_j; hi = limit_i; a = a - half_pi; }
        else if (a >= rx_bf(0xbfc90fdbu) && 0.0f >= a) { lo = limit_j; hi = limit_i; a = a + half_pi; }   /* 00977520 */
        else a = a + rx_bf(K_PI);
    }
    return (hi - lo) * a / half_pi + lo;
}

/* 0042de70 (ESI = limits L, ECX = q_b*, stack q_a*; RET 4): 1 when the relative rotation p = conj(q_a q_ref) q_b
 * (q_ref = L+0) keeps two reference axes inside elliptic cones: the axis v1 (runtime vec4 0x00d61680) rotated by p must be
 * within acos(z) < limit(azimuth atan2(x, y)) between L+0x10 / L+0x14, and the axis v2 (0x00d616a0) within
 * acos(y) < limit(atan2(z, x)) between L+0x10 / L+0x18. EAX = the result. */
int32_t phys_quat_within_cone_limits(rx_addr L, rx_addr qb, rx_addr qa) {
    const rx_v4 r = phys_quat_conj(phys_quat_mul(v4_ld(qa), v4_ld(L)));
    const rx_v4 p = phys_quat_mul(r, v4_ld(qb));
    const rx_v4 cp = phys_quat_conj(p);
    const rx_v4 w1 = phys_quat_mul(p, phys_quat_mul(v4_xyz(v4_ld(0x00d61680)), cp));
    const float lim1 = quarter_limit(atan2_f(w1.v[0], w1.v[1]), F32(L, 0x10), F32(L, 0x14));
    const int in1 = lim1 > acos_poly(w1.v[2]);
    const rx_v4 w2 = phys_quat_mul(p, phys_quat_mul(v4_xyz(v4_ld(0x00d616a0)), cp));
    const float lim2 = quarter_limit(atan2_f(w2.v[2], w2.v[0]), F32(L, 0x18), F32(L, 0x10));
    const int in2 = lim2 > acos_poly(w2.v[1]);
    return in1 && in2 ? 1 : 0;
}
RX_FUNCTION(0x0042de70, phys_quat_within_cone_limits)

/* 00841c90 (ECX = pose a, EDX = pose out; cdecl pose b, bone count, t): blends two animation poses bone by bone. A bone
 * (0x30 bytes): +0 position valid, +4 rotation valid, +0x10 position, +0x20 rotation (x, y, z, w). Where either pose has the
 * channel: position = pb - (pb - pa) t; rotation = slerp(qb, qa, t) by the polynomial sines of 0042ce40 (qa weighted with
 * the sign of the dot; the weights (1, 0) for |dot| >= 1), renormalized in double precision (zero when its length is 0);
 * the output's channel flag is set. */
void phys_pose_blend(rx_addr pa, rx_addr out, rx_addr pb, int32_t count, float t) {
    for (int32_t i = 0; i < count; i++) {
        const rx_addr a = pa + 0x30u * (uint32_t)i, b = pb + 0x30u * (uint32_t)i, o = out + 0x30u * (uint32_t)i;
        if (U32(b, 0) != 0 || U32(a, 0) != 0) {
            for (int k = 0; k < 3; k++) {
                const float v = F32(b, 0x10 + 4 * k);
                SF32(o, 0x10 + 4 * k, v - (v - F32(a, 0x10 + 4 * k)) * t);
            }
            SU32(o, 0, 1);
        }
        if (U32(b, 4) == 0 && U32(a, 4) == 0) continue;
        const float bx = F32(b, 0x20), by = F32(b, 0x24), bz = F32(b, 0x28), bw = F32(b, 0x2c);
        const float ax = F32(a, 0x20), ay = F32(a, 0x24), az = F32(a, 0x28), aw = F32(a, 0x2c);
        const float dot = ((ax * bx + by * ay) + az * bz) + aw * bw;
        const float sign = dot >= 0.0f ? 1.0f : F32(0, 0x009cd9e8);
        const float c = float_abs(float_abs(dot));
        const float theta = acos_poly(c);
        float y = pi_minus_wrapped(float_abs(theta + rx_bf(K_HALF_PI)));
        const float inv_sin = 1.0f / cos_poly(y * y);
        const float t_theta = theta * t;
        float w_b, w_a;
        if (c >= 1.0f) {
            w_b = 1.0f;
            w_a = 0.0f;
        } else {
            y = pi_minus_wrapped(float_abs((theta - t_theta) + rx_bf(K_HALF_PI)));
            w_b = cos_poly(y * y) * inv_sin;
            y = pi_minus_wrapped(float_abs(t_theta + rx_bf(K_HALF_PI)));
            w_a = cos_poly(y * y) * sign * inv_sin;
        }
        float qw = aw * w_a + bw * w_b, qx = ax * w_a + bx * w_b;
        float qy = by * w_b + ay * w_a, qz = az * w_a + bz * w_b;
        const float n2 = (float)((((double)qx * (double)qx + (double)qw * (double)qw) + (double)qy * (double)qy)
                                 + (double)qz * (double)qz);
        if (n2 == 0.0f) {
            qx = qy = qz = qw = 0.0f;
        } else {
            const float inv = 1.0f / phys_sqrtf(n2);
            qw = (float)((double)qw * (double)inv);
            qx = (float)((double)qx * (double)inv);
            qy = (float)((double)qy * (double)inv);
            qz = (float)((double)qz * (double)inv);
        }
        SF32(o, 0x20, qx); SF32(o, 0x24, qy); SF32(o, 0x28, qz); SF32(o, 0x2c, qw);
        SU32(o, 4, 1);
    }
}
RX_FUNCTION(0x00841c90, phys_pose_blend)
