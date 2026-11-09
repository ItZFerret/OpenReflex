/* bike_math.h -- the vector idioms of the motocross simulation code (bike_*.c), on top of the physics core's exact 4-lane helpers
 * (phys_vec.h: rx_v4 lanes as the original's SSE registers hold them, x86 NaN rules).
 *
 * The original is LTCG SSE code in which the same few inlined math routines appear over and over (quaternion product, rotation
 * by a quaternion, "normalize or NaN", the horizontal sums). Each helper here reproduces one such inlined sequence exactly
 * (operation order and association as in the disassembly); the comment names the instruction pattern so a site can be matched.
 * Globals the code reads from .data (0x9a6000..) are read from game memory; .rdata constants (0x916000..0x9a58fb) are literals. */
#pragma once
#include "phys_vec.h"
#include "rx_x87.h"

/* ---- constants */
#define BIKE_INF        0x7f800000u   /* 0092dc20 (x4) */
#define BIKE_QNAN       0x7fc00000u   /* 0092dc30 (x4) */
static inline rx_v4 bike_zero(void) { return v4_zero(); }
/* 00d61670: a zero vec4 in .data (read from memory: the original loads it) */
static inline rx_v4 bike_g_zero(void) { return rx_ld4(0x00d61670); }

/* ---- horizontal sums of p = a * a (or a * b): the order differs between sites
 *  hsum_012: (p0 + p1) + p2   shufps t, p, 0x99 ; addss ; shufps t, t, 0x55 ; addss
 *  hsum_021: (p0 + p2) + p1   shufps t, p, 0x66 ; addss ; shufps t, t, 0x55 ; addss */
static inline float bike_hsum_012(rx_v4 p) { return phys_add(phys_add(p.v[0], p.v[1]), p.v[2]); }
static inline float bike_hsum_021(rx_v4 p) { return phys_add(phys_add(p.v[0], p.v[2]), p.v[1]); }
static inline float bike_dot3(rx_v4 a, rx_v4 b) { return bike_hsum_012(v4_mul(a, b)); }
static inline float bike_len2(rx_v4 a) { return bike_hsum_012(v4_mul(a, a)); }

/* ---- "normalize or NaN" (inlined Vector::Normalize): l2 = |v.xyz|^2 (hsum_012), len = sqrt(l2) in all lanes,
 *   r = (v / len) & (len != 0) & (l2 != inf)  |  (l2 == inf ? QNAN : 0)        (cmpneqps / divps / andps / andnps / orps)
 * All four lanes are divided (w / len). A zero vector gives 0, an infinite one QNaN in all lanes. */
static inline rx_v4 bike_normalize_masked(rx_v4 v, float l2) {
    rx_v4 L2 = v4_splat(l2), len = v4_sqrt(L2);
    rx_v4 fin = v4_mask(l2 != rx_bf(BIKE_INF), l2 != rx_bf(BIKE_INF), l2 != rx_bf(BIKE_INF), l2 != rx_bf(BIKE_INF));
    rx_v4 nz = v4_mask(0.0f != len.v[0], 0.0f != len.v[1], 0.0f != len.v[2], 0.0f != len.v[3]);
    rx_v4 r = v4_and(v4_and(v4_div(v, len), nz), fin);
    return v4_or(r, v4_andn(fin, v4_bits(BIKE_QNAN, BIKE_QNAN, BIKE_QNAN, BIKE_QNAN)));
}
static inline rx_v4 bike_normalize(rx_v4 v) { return bike_normalize_masked(v, bike_len2(v)); }
/* quaternion (4-lane) normalize: the same masks, length^2 = (p1 + p3) + (p0 + p2) (shufps 0xee / 0x40 / 0xf0 / 0xaa) */
static inline rx_v4 bike_qnormalize(rx_v4 q) { return bike_normalize_masked(q, phys_hsum4(v4_mul(q, q))); }

/* ---- quaternions (x, y, z, w): the inlined Quaternion product a * b with the sign tables 0093a1f0 / 0093a200 / 0093a210:
 *   A0 = (a.x * b.wzyx) * (1,-1,1,-1)   A1 = (a.y * b.zwxy) * (1,1,-1,-1)   A2 = (b.yxwz * a.z) * (-1,1,1,-1)   A3 = a.w * b
 *   a * b = (A2 + A1) + (A3 + A0) */
static inline rx_v4 bike_quat_mul_observed_parts(rx_v4 a, rx_v4 b, rx_v4* raw_a0, rx_v4* wx_sum) {
    rx_v4 raw = v4_mul(v4_lane(a, 0), v4_perm(b, 3, 2, 1, 0));
    if (raw_a0) *raw_a0 = raw;
    rx_v4 A0 = v4_mul(raw, v4(1.0f, -1.0f, 1.0f, -1.0f));
    rx_v4 A1 = v4_mul(v4_mul(v4_lane(a, 1), v4_perm(b, 2, 3, 0, 1)), v4(1.0f, 1.0f, -1.0f, -1.0f));
    rx_v4 A2 = v4_mul(v4_mul(v4_perm(b, 1, 0, 3, 2), v4_lane(a, 2)), v4(-1.0f, 1.0f, 1.0f, -1.0f));
    rx_v4 A3 = v4_mul(v4_lane(a, 3), b);
    rx_v4 wx = v4_add(A3, A0);
    if (wx_sum) *wx_sum = wx;
    return v4_add(v4_add(A2, A1), wx);
}
static inline rx_v4 bike_quat_mul_observed(rx_v4 a, rx_v4 b, rx_v4* raw_a0) { return bike_quat_mul_observed_parts(a,b,raw_a0,0); }
static inline rx_v4 bike_quat_mul(rx_v4 a, rx_v4 b) { return bike_quat_mul_observed_parts(a,b,0,0); }
/* conjugate as the code computes it: q * (-1,-1,-1,1) (0093a220) */
static inline rx_v4 bike_quat_conj(rx_v4 q) { return v4_mul(v4(-1.0f, -1.0f, -1.0f, 1.0f), q); }
/* world -> body: conj(q) * (v.xyz, 0) * q  (the vector masked with 0092db80 first) */
static inline rx_v4 bike_rotate_inv(rx_v4 q, rx_v4 v) { return bike_quat_mul(bike_quat_conj(q), bike_quat_mul(v4_xyz(v), q)); }

/* body -> world: q * ((v.xyz, 0) * conj(q)) */
static inline rx_v4 bike_rotate(rx_v4 q, rx_v4 v) { return bike_quat_mul(q, bike_quat_mul(v4_xyz(v), bike_quat_conj(q))); }

/* ---- cross product a x b as inlined (shufps 0xc9 / 0xd2): (b.zxy * a.yzx - b.yzx * a.zxy) & xyz */
static inline rx_v4 bike_cross(rx_v4 a, rx_v4 b) {
    rx_v4 a1 = v4_perm(a, 1, 2, 0, 3), a2 = v4_perm(a1, 1, 2, 0, 3);
    rx_v4 b1 = v4_perm(b, 2, 0, 1, 3), b2 = v4_perm(b1, 2, 0, 1, 3);
    return v4_xyz(v4_sub(v4_mul(b1, a1), v4_mul(b2, a2)));
}

/* ---- rigid-body contact helpers (inlined the same way in the tire and contact-response code) */
/* velocity of a body point: rotate(q, w x r_local) + v */
static inline rx_v4 bike_point_velocity(rx_v4 q, rx_v4 w, rx_v4 v, rx_v4 r_local) {
    return v4_add(bike_rotate(q, bike_cross(w, r_local)), v);
}
/* the rotational part of a body's inverse effective mass at world lever arm r along axis a:
 *   a . ((R I^-1 R^T (r x a)) x r)   (I^-1 = the body's inverse inertia, body frame) */
static inline float bike_eff_rot(rx_v4 q, rx_v4 inv_inertia, rx_v4 r, rx_v4 a) {
    rx_v4 t = bike_rotate_inv(q, bike_cross(r, a));
    rx_v4 u = bike_rotate(q, v4_xyz(v4_mul(t, inv_inertia)));
    return bike_dot3(bike_cross(u, r), a);
}

/* ---- "all of x, y, z" test of a cmpps mask (the code adds lanes 1, 2, 0 as integers and compares with -3) */
static inline int bike_all3(rx_v4 mask) { return (int32_t)(v4_lb(mask, 1) + v4_lb(mask, 2) + v4_lb(mask, 0)) == -3; }

/* 00401000 Float_Abs: andps with (0x7fffffff, 0, 0, 0) (lanes 1..3 cleared; only lane 0 is used) */
static inline float bike_fabs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }

/* ---- the inlined arcsine approximation (several sites; 00447f10, 0044c170 ...): x = |s| in [0, 1]; above 0.7071 (global
 * 009ddfc8) via asin(x) = pi/2 - asin(sqrt(1 - s^2)). A rational function of y^2 in double with float roundings between steps. */
static inline float bike_asin_approx(float x, float s) {
    const double one = 1.0;                                                   /* 00977638 */
    const int high = x >= F32(0, 0x009ddfc8);
    float y = x;
    if (high) y = phys_sqrt(bike_fabs((float)(one - (double)(float)((double)s * (double)s))));   /* SqrtF 0042c230 */
    float y2 = (float)((double)y * (double)y);
    float y4 = (float)((double)y2 * (double)y2);
    double p = (double)(float)(0.5648093223571777 - (double)y2 * 0.04992850869894028) * (double)y4;   /* 009777b0 / a8 */
    double q = (double)(float)(one - (double)y2 * 1.4430354833602905);                                  /* 009777a0 */
    float r = (float)(one / (p + q));
    double n1 = (double)(float)(0.3993034064769745 - (double)y2 * 0.0193784236907959) * (double)y4;    /* 00977798 / 90 */
    double n0 = (double)(float)(one - (double)y2 * 1.2763687372207642);                                 /* 00977788 */
    float num = (float)((n1 + n0) * (double)y);
    float a = (float)((double)r * (double)num);
    if (high) a = (float)(1.5707963705062866 - (double)a);                                              /* 00977510 */
    return a;
}

/* 0..1 clamp as compiled (comiss x, 1 / comiss 0, x): x >= 1 -> 1; 0 >= x -> 0; else x (NaN stays) */
static inline float bike_clamp01(float x) { return x >= 1.0f ? 1.0f : !(0.0f >= x) ? x : 0.0f; }

/* ---- the inlined cosine polynomial: r = |a| wrapped to [0, 2 pi) (truncation), u = pi - r,
 *   ((u^2 * c3 + c2) * u^4) + (u^2 * c1 + c0)  with 0093a710..0093a71c   (~ -cos(u) = cos(r)) */
static inline float bike_cos_poly(float a) {
    float x = bike_fabs(a);
    x = phys_sub(x, phys_mul((float)rx_cvtt(phys_mul(x, 0.15915493667125702f /* 009774a0 */)), 6.2831854820251465f /* 0097749c */));
    float u = phys_sub(3.1415927410125732f /* 00977498 */, x), u2 = phys_mul(u, u);
    float hi = phys_add(phys_mul(u2, rx_bf(0xba7dd785u) /* 0093a71c */), rx_bf(0x3d209877u) /* 0093a718 */);
    float lo = phys_add(phys_mul(u2, rx_bf(0xbefd94e5u) /* 0093a714 */), rx_bf(0x3f7fa213u) /* 0093a710 */);
    return phys_add(phys_mul(hi, phys_mul(u2, u2)), lo);
}

/* ---- the second inlined polynomial (00935d3c..00935d50, higher order): r = |a| wrapped to [0, 2 pi), u = pi - r,
 *   ((u^2 k6 + k4) u^4 + (u^2 k2 + k3)) u^4 + (u^2 k1 + k0)      (~ -cos(u) = cos(r)) */
static inline float bike_cos_poly2(float a) {
    float x = bike_fabs(a);
    x = phys_sub(x, phys_mul((float)rx_cvtt(phys_mul(x, 0.15915493667125702f /* 009774a0 */)), 6.2831854820251465f /* 0097749c */));
    const float u = phys_sub(3.1415927410125732f /* 00977498 */, x), u2 = phys_mul(u, u), u4 = phys_mul(u2, u2);
    const float k6 = rx_bf(0xb46beed0u) /* 00935d50 */, k4 = rx_bf(0x37cb076bu) /* 00935d48 */, k3 = rx_bf(0x3d2aa372u) /* 00935d4c */;
    const float k2 = rx_bf(0xbab5a662u) /* 00935d44 */, k1 = rx_bf(0xbeffff3du) /* 00935d40 */, k0 = rx_bf(0x3f7ffff3u) /* 00935d3c */;
    return phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, k6), k4), u4), phys_add(phys_mul(u2, k2), k3)), u4),
                    phys_add(phys_mul(u2, k1), k0));
}

/* ---- 00401630 Vec3_Cross (EAX = a, ECX = c, stack out): out = c x a on three floats, in the original's operand order
 * (the same as rx_world_vec3_cross; kept here so the bike code does not depend on the world files) */
static inline void bike_vec3_cross(float out[3], const float c[3], const float a[3]) {
    const float x = phys_sub(phys_mul(a[2], c[1]), phys_mul(a[1], c[2]));
    const float y = phys_sub(phys_mul(a[0], c[2]), phys_mul(c[0], a[2]));
    const float z = phys_sub(phys_mul(c[0], a[1]), phys_mul(a[0], c[1]));
    out[0] = x; out[1] = y; out[2] = z;
}
/* ---- the inlined rotation matrix -> quaternion (rows m[0..2]; 0043e200, 0043f2e0) */
/* rotation matrix rows -> quaternion (Shepperd: the largest of the four diagonal combinations, compared in the order
 * x, y, z, w; ties go to the last that equals the maximum) */
static inline rx_v4 bike_matrix_to_quat(const float m[3][3]) {
    const float tx = phys_mul(phys_sub(phys_sub(phys_add(m[0][0], 1.0f), m[1][1]), m[2][2]), 0.25f);
    const float ty = phys_mul(phys_sub(phys_add(phys_sub(1.0f, m[0][0]), m[1][1]), m[2][2]), 0.25f);
    const float tw = phys_mul(phys_add(phys_add(phys_add(m[0][0], 1.0f), m[1][1]), m[2][2]), 0.25f);
    const float tz = phys_mul(phys_add(phys_sub(phys_sub(1.0f, m[0][0]), m[1][1]), m[2][2]), 0.25f);
    float mx = tx > ty ? tx : ty;
    if (!(mx > tz)) mx = tz;
    if (!(mx > tw)) mx = tw;
    rx_v4 q;
    if (mx == tw) {
        float r = phys_sqrt(tw), k = phys_div(0.25f, r);
        q = v4(phys_mul(phys_sub(m[1][2], m[2][1]), k), phys_mul(phys_sub(m[2][0], m[0][2]), k), phys_mul(phys_sub(m[0][1], m[1][0]), k), r);
    } else {
        float my = ty > tz ? ty : tz;
        if (!(my > tx)) my = tx;
        if (my == tx) {
            float r = phys_sqrt(tx), k = phys_div(0.25f, r);
            q = v4(r, phys_mul(phys_add(m[1][0], m[0][1]), k), phys_mul(phys_add(m[2][0], m[0][2]), k), phys_mul(phys_sub(m[1][2], m[2][1]), k));
        } else if (ty > tz) {
            float r = phys_sqrt(ty), k = phys_div(0.25f, r);
            q = v4(phys_mul(phys_add(m[1][0], m[0][1]), k), r, phys_mul(phys_add(m[2][1], m[1][2]), k), phys_mul(phys_sub(m[2][0], m[0][2]), k));
        } else {
            float r = phys_sqrt(tz), k = phys_div(0.25f, r);
            q = v4(phys_mul(phys_add(m[2][0], m[0][2]), k), phys_mul(phys_add(m[2][1], m[1][2]), k), r, phys_mul(phys_sub(m[0][1], m[1][0]), k));
        }
    }
    return q;
}

/* ---- x87 helpers. The simulation thread runs with the x87 control word 0x027f (53-bit precision, round to nearest: measured in
 * the oracle at 004390d0; NOT the 0x007f of the D3D thread), so x87 sums of doubles round to 53 bits. */
#define BIKE_X87_BITS 53
/* fld m32 ; fstp m32: a float copied through the x87 stack (a signaling NaN comes out quiet) */
static inline uint32_t bike_x87_copy32(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? (b | 0x400000u) : b; }
static inline float bike_x87_copyf(float x) { return rx_bf(bike_x87_copy32(rx_fb(x))); }
/* fld a ; fmul b ; fadd c ; fstp m32  (BIKE_X87_BITS precision, then rounded to float) */
static inline float bike_x87_mul_add(float a, float b, float c) {
    rx_f80 t = rx_f80_mul(rx_f80_from_f32(a), rx_f80_from_f32(b), BIKE_X87_BITS, 0);
    return rx_f80_to_f32(rx_f80_add(t, rx_f80_from_f32(c), BIKE_X87_BITS, 0), 0);
}
