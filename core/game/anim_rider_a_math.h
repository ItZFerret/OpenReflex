/* anim_rider_a_math.h -- the inlined SSE math of the rider's procedural pose (00623xxx .. 00629xxx), written once and shared by
 * anim_rider_a.c and anim_rider_a_ik.c. Each helper reproduces the original's instruction sequence (operation order, association,
 * masks), lane by lane where the original computes packed: every vector constant these sequences use is a splat (0x935ad0 ...),
 * so a lane of a packed computation on a splat input is the scalar computation written here. */
#pragma once
#include "anim_extern.h"

/* ---- range reduction and the sin / cos polynomials (0x935d3c.. scalar, 0x935b10.. packed: the same values) ----
 * wrap_pi(a): pi - (a - trunc(a / 2pi) * 2pi) for a >= 0 (cvttss2si; an out of range a gives the "integer indefinite").
 * cos_poly(u2, u4) ~ cos(u), sin_poly(u2, u4) ~ sin(u) / u, with u2 = u * u, u4 = u2 * u2. */
static inline float rider_wrap_pi(float a) {
    float k = (float)rx_cvtt(a * 0.15915494f /* 0x9774a0 */);
    return 3.1415927f /* 0x977498 */ - (a - k * 6.2831855f /* 0x97749c */);
}
static inline float rider_cos_poly(float u2, float u4) {
    return ((u2 * -2.197296e-07f + 2.420294e-05f) * u4 + (u2 * -0.001385879f + 0.04165978f)) * u4 + (u2 * -0.4999942f + 0.9999992f);
}
static inline float rider_sin_poly(float u2, float u4) {
    return ((u2 * -2.036224e-08f + 2.699715e-06f) * u4 + (u2 * -0.0001980863f + 0.008332403f)) * u4 + (u2 * -0.1666655f + 0.9999996f);
}
/* The packed half-angle pair of the axis-angle quaternions: cos_poly(wrap_pi(|x|)) with |x| = maxps(0 - x, x). For x = h + pi/2 it
 * is sin(h) (the xyz lanes), 0 - it for x = h is cos(h) (the w lane). */
static inline float rider_neg_cos(float x) {
    float a = rx_maxss(0.0f - x, x);
    float u = rider_wrap_pi(a), u2 = u * u, u4 = u2 * u2;
    return rider_cos_poly(u2, u4);
}

/* ---- vectors ---- */
static inline rx_v4 rider_v4(float x, float y, float z, float w) { rx_v4 r = {{x, y, z, w}}; return r; }
/* The normalization idiom (sqrtps of the splat length, divps, `cmpneqps sqrt, 0` and `cmpneqps len2, inf` masks, 0x7fc00000 where
 * len2 is infinite): lanes v / |v| (all four lanes of v divided), 0 where the length is 0. */
static inline rx_v4 rider_v4_normalize_by(rx_v4 v, float len2) {
    float s = sqrtf(len2);
    rx_v4 r;
    for (int i = 0; i < 4; i++) {
        float d = s != 0.0f ? rx_divs(v.v[i], s) : 0.0f;
        r.v[i] = len2 != INFINITY ? d : rx_bf(0x7fc00000u);
    }
    return r;
}
/* The dot product of the xyz lanes as the shufps 0x99 / 0x55 idiom adds the packed products: (x + y) + z. */
static inline float rider_dot3(rx_v4 a, rx_v4 b) { return (a.v[0] * b.v[0] + a.v[1] * b.v[1]) + a.v[2] * b.v[2]; }
static inline float rider_len2(rx_v4 v) { return rider_dot3(v, v); }
static inline rx_v4 rider_normalize(rx_v4 v) { return rider_v4_normalize_by(v, rider_len2(v)); }
/* a x b as the shufps 0xc9 / 0xd2 idiom forms it (lane i = a[i+1] * b[i+2] - a[i+2] * b[i+1]), lane 3 masked to 0 (0x92db80). */
static inline rx_v4 rider_cross(rx_v4 a, rx_v4 b) {
    return rider_v4(a.v[1] * b.v[2] - a.v[2] * b.v[1], a.v[2] * b.v[0] - a.v[0] * b.v[2], a.v[0] * b.v[1] - a.v[1] * b.v[0], 0.0f);
}
/* v times three rows: ((v.x * r0 + v.y * r1) + v.z * r2) lane by lane. */
static inline rx_v4 rider_vec_mat3(rx_v4 v, rx_v4 r0, rx_v4 r1, rx_v4 r2) {
    rx_v4 o;
    for (int i = 0; i < 4; i++) o.v[i] = (v.v[0] * r0.v[i] + v.v[1] * r1.v[i]) + v.v[2] * r2.v[i];
    return o;
}
/* Row-vector 4x4 product a * b: row i = (a[i].w * b3 + a[i].y * b1) + (a[i].x * b0 + a[i].z * b2). */
static inline void rider_mat_mul(const rx_v4 a[4], const rx_v4 b[4], rx_v4 o[4]) {
    rx_v4 t[4];
    for (int r = 0; r < 4; r++)
        for (int i = 0; i < 4; i++)
            t[r].v[i] = (a[r].v[3] * b[3].v[i] + a[r].v[1] * b[1].v[i]) + (a[r].v[0] * b[0].v[i] + a[r].v[2] * b[2].v[i]);
    for (int r = 0; r < 4; r++) o[r] = t[r];
}
/* The rotation rows of a unit quaternion (x y z w), lane 3 = 0 (the original's inlined Quaternion -> matrix). */
static inline void rider_quat_rows(rx_addr q, rx_v4 r[3]) {
    float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    float x2 = x * 2.0f /* 0x92e330 */, y2 = y * 2.0f, z2 = z * 2.0f;
    float xx = x2 * x, yy = y2 * y, zz = z2 * z, xw = x2 * w, yw = y2 * w, zw = z2 * w, xy = x2 * y, xz = x2 * z, yz = y2 * z;
    r[0] = rider_v4(1.0f - (zz + yy), xy + zw, xz - yw, 0.0f);
    r[1] = rider_v4(xy - zw, 1.0f - (zz + xx), yz + xw, 0.0f);
    r[2] = rider_v4(xz + yw, yz - xw, 1.0f - (yy + xx), 0.0f);
}

/* ---- the inlined packed acos (on a splat): asin of |c| or of sqrt(1 - c * c) by the rational approximation 0x935b70..0x935bc0,
 * folded with pi / 2 and the sign; the 1.0 / 0.0 vectors are the globals 0xd61690 / 0xd61660 (splats set at startup). */
static inline float rider_acos(float c) {
    const float one = F32(0, 0xd61690), zero = F32(0, 0xd61660);
    float om = one - c * c;
    float a = rx_maxss(0.0f - c, c);
    int big = 0.707106f /* 0x977780 */ <= a;
    float t = big ? sqrtf(rx_maxss(0.0f - om, om)) : a;
    float t2 = t * t, t4 = t2 * t2;
    float den = (t2 * -0.04992851f + 0.5648093f) * t4 + (t2 * -1.4430355f + one);
    float num = ((t2 * -0.019378424f + 0.3993034f) * t4 + (t2 * -1.2763687f + one)) * t;
    float r = (1.0f / den) * num;
    float as = big ? 1.5707964f - r : r;
    float sas = zero <= c ? as : 0.0f - as;
    return 1.5707964f - sas;
}

/* sin and cos of x as the packed code forms them: sin = cos_poly of (x + pi / 2), cos = 0 - cos_poly of x. */
static inline void rider_sincos(float x, float* s, float* c) {
    *s = rider_neg_cos(x + 1.5707964f /* 0x935b00 */);
    *c = 0.0f - rider_neg_cos(x);
}
/* The rotation rows about the unit axis a by the angle of (s, c), omc = 1 - c (shufps 0xc9 / 0xd2 cross terms, the 0x935950 -1
 * vector, rows assembled lane by lane): off = (a.yzx * omc) * a.zxy, D = (a * omc) * a + c, P = a * s + off, M = (a * s - off) * -1;
 * rows (D0 P2 M1 0), (M2 D1 P0 0), (P1 M0 D2 0). */
static inline void rider_rot_rows(rx_v4 a, float s, float c, float omc, rx_v4 r[3]) {
    float D[3], P[3], M[3];
    static const int c9[3] = {1, 2, 0}, d2[3] = {2, 0, 1};
    for (int i = 0; i < 3; i++) {
        float off = (a.v[c9[i]] * omc) * a.v[d2[i]];
        float sa = a.v[i] * s;
        D[i] = (a.v[i] * omc) * a.v[i] + c;
        P[i] = sa + off;
        M[i] = rx_muls(sa - off, -1.0f);
    }
    r[0] = rider_v4(D[0], P[2], M[1], 0.0f);
    r[1] = rider_v4(M[2], D[1], P[0], 0.0f);
    r[2] = rider_v4(P[1], M[0], D[2], 0.0f);
}

/* ---- quaternions ---- */
/* The packed quaternion product a * b (x y z w) of the shufps 0x1b / 0xb1 idiom with the sign vectors 0x93a1f0 (1 -1 1 -1),
 * 0x93a200 (1 1 -1 -1), 0x93a210 (-1 1 1 -1): lane i = ((a2 * b_1032) * s210 + s200 * (a1 * b_2301)) + (a3 * b + s1f0 * (a0 * b_3210)). */
static inline rx_v4 rider_quat_mul(rx_v4 a, rx_v4 b) {
    static const float s1f0[4] = {1.0f, -1.0f, 1.0f, -1.0f}, s200[4] = {1.0f, 1.0f, -1.0f, -1.0f}, s210[4] = {-1.0f, 1.0f, 1.0f, -1.0f};
    static const int p3210[4] = {3, 2, 1, 0}, p2301[4] = {2, 3, 0, 1}, p1032[4] = {1, 0, 3, 2};
    rx_v4 r;
    for (int i = 0; i < 4; i++) {
        float t2 = rx_muls(a.v[2] * b.v[p1032[i]], s210[i]);
        float t1 = rx_muls(s200[i], a.v[1] * b.v[p2301[i]]);
        float t0 = rx_muls(s1f0[i], a.v[0] * b.v[p3210[i]]);
        r.v[i] = (t2 + t1) + (a.v[3] * b.v[i] + t0);
    }
    return r;
}
/* The conjugate as the original forms it: q * (-1 -1 -1 1) (0x93a220). */
static inline rx_v4 rider_quat_conj(rx_v4 q) {
    return rider_v4(rx_muls(q.v[0], -1.0f), rx_muls(q.v[1], -1.0f), rx_muls(q.v[2], -1.0f), rx_muls(q.v[3], 1.0f));
}
/* v rotated by q: q * (v * conj(q)). */
static inline rx_v4 rider_quat_rotate(rx_v4 q, rx_v4 v) { return rider_quat_mul(q, rider_quat_mul(v, rider_quat_conj(q))); }
