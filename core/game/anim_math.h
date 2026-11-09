/* anim_math.h -- the small math routines the animation code inlines everywhere (the compiler expanded the same source routine at
 * each site, with the same operation order): sign, the [-pi, pi] range reduction with its sine / cosine polynomials, the packed
 * rotation key decode. Shared by all anim_*.c. Constants from .rdata, addresses in the comments. */
#pragma once
#include "rx_game.h"

/* comiss x, 0; jb: -1 when x < 0 or unordered, else 1 (0x931f24 / 0x9cd9e8). */
static inline float anim_sign(float x) { return x >= 0.0f ? 1.0f : -1.0f; }

/* Range reduction used by the inlined sin / cos: a - trunc(a / 2pi) * 2pi, mirrored: pi - that (so the polynomials see [-pi, pi]). */
static inline float anim_trig_reduce(float a) {
    float turns = (float)rx_cvtt(a * 0.15915494f /* 0x9774a0 */);
    return 3.1415927f /* 0x977498 */ - (a - turns * 6.2831855f /* 0x97749c */);
}
/* cos(t) of the reduced t (t2 = t * t, t4 = t2 * t2): ((c6 t2 + c4) t4 + (c2 t2 + c3)) t4 + (c1 t2 + c0). */
static inline float anim_cos_poly(float t2, float t4) {
    float p = t2 * -2.197296e-07f /* 0x935d50 */ + 2.420294e-05f /* 0x935d48 */;
    p = p * t4;
    p = p + (t2 * -0.001385879f /* 0x935d44 */ + 0.04165978f /* 0x935d4c */);
    p = p * t4;
    return p + (t2 * -0.4999942f /* 0x935d40 */ + 0.9999992f /* 0x935d3c */);
}
/* sin(t) / t of the reduced t: ((s6 t2 + s4) t4 + (s2 t2 + s3)) t4 + (s1 t2 + s0) (the caller multiplies by t). */
static inline float anim_sin_poly(float t2, float t4) {
    float p = t2 * -2.036224e-08f /* 0x935d68 */ + 2.699715e-06f /* 0x935d64 */;
    float q = t2 * -0.0001980863f /* 0x935d60 */ + 0.008332403f /* 0x935d5c */;
    float r = t2 * -0.1666655f /* 0x935d58 */ + 0.9999996f /* 0x935d54 */;
    p = p * t4;
    p = p + q;
    p = p * t4;
    return p + r;
}

/* A packed rotation key (8 bytes: u16 angle in units of pi / 4096, then x y z int16 axis / 32767) as the unit quaternion
 * (axis * sin(angle), |cos(angle)|), the axis flipped when cos < 0 (so w >= 0). As inlined in Character_SampleKeyframes /
 * QuatSlerp_Packed 0042ce40. q = x y z w. */
static inline void anim_packed_quat(rx_addr key, float q[4]) {
    float a = (float)U16(key, 0) * 0.0007669904f /* 0x9778fc */;
    float s = anim_sign(a);
    a = rx_muls(a, s);
    float t = anim_trig_reduce(a);
    float t2 = t * t, t4 = t2 * t2;
    float c = rx_subs(-0.0f /* 0x9774a8 */, anim_cos_poly(t2, t4));
    float k = rx_muls(rx_muls(anim_sin_poly(t2, t4) * t, s), 3.051851e-05f /* 0x9c6400: 1 / 32767 */);
    float x = (float)(int16_t)U16(key, 2) * k, y = (float)(int16_t)U16(key, 4) * k, z = (float)(int16_t)U16(key, 6) * k;
    float flip = anim_sign(c);
    q[0] = rx_muls(x, flip); q[1] = rx_muls(y, flip); q[2] = rx_muls(z, flip); q[3] = fabsf(c);
}

/* cos of |x| through the range reduction (the inlined sine of the slerp: cos(x - pi/2) after adding pi / 2 = 0x977508). */
static inline float anim_cos_abs(float x) {
    float t = anim_trig_reduce(fabsf(x));
    float t2 = t * t;
    return anim_cos_poly(t2, t2 * t2);
}

/* The weights of the inlined quaternion slerp (the same sequence in QuatSlerp_Packed 0042ce40, BlendAnimator::vf4, the motion
 * blenders): d = dot(a, b), w = blend factor. theta = acos(|d|) by a rational approximation in double precision (the halves
 * around 0.7071 = 0x9ddfc8), then s0 = sin((1 - w) theta) / sin(theta) and s1 = sign(d) sin(w theta) / sin(theta) (sines as
 * cosines of the angle + pi/2); |d| >= 1 gives s0 = 1, s1 = 0. Result = s0 a + s1 b. */
static inline void anim_slerp_weights_ex(float d, float w, int shortest, float* s0, float* s1) {
    float sgn = anim_sign(d);
    float ad = fabsf(d), ad2 = fabsf(ad);
    float u = ad2;
    if (ad2 >= 0.7071f) {
        float sq = (float)((double)ad * (double)ad);
        u = sqrtf(fabsf((float)(1.0 - (double)sq)));
    }
    float u2 = (float)((double)u * (double)u);
    float u4 = (float)((double)u2 * (double)u2);
    float p = (float)(0.5648093223571777 /* 0x9777b0 */ - (double)u2 * 0.04992850869894028 /* 0x9777a8 */);
    float q = (float)(1.0 /* 0x977638 */ - (double)u2 * 1.4430354833602905 /* 0x9777a0 */);
    float inv = (float)(1.0 / ((double)p * (double)u4 + (double)q));
    float n1 = (float)(0.3993034064769745 /* 0x977798 */ - (double)u2 * 0.0193784236907959 /* 0x977790 */);
    float n2 = (float)(1.0 - (double)u2 * 1.2763687372207642 /* 0x977788 */);
    float num = (float)(((double)n1 * (double)u4 + (double)n2) * (double)u);
    float r = (float)((double)inv * (double)num);                 /* atan(u / ...) ~ asin / acos part */
    if (ad2 >= 0.7071f) r = (float)(1.5707963705062866 /* 0x977510 */ - (double)r);
    if (!(ad >= 0.0f)) r = rx_subs(-0.0f /* 0x9774a8 */, r);
    float theta = (float)(1.5707963705062866 - (double)r);
    float inv_sin = 1.0f / anim_cos_abs(theta + 1.5707964f /* 0x977508 */);
    float tw = theta * w;
    *s0 = ad >= 1.0f ? 1.0f : anim_cos_abs((theta - tw) + 1.5707964f) * inv_sin;
    float c1 = anim_cos_abs(tw + 1.5707964f);
    *s1 = ad >= 1.0f ? 0.0f : (shortest ? rx_muls(c1, sgn) : c1) * inv_sin;
}
/* The usual form: the second quaternion's weight carries sign(d) (shortest arc). */
static inline void anim_slerp_weights(float d, float w, float* s0, float* s1) { anim_slerp_weights_ex(d, w, 1, s0, s1); }
/* The rider limb blends' form (0062b710 / 0062c840): no sign on the second weight. */
static inline void anim_slerp_weights_unsigned(float d, float w, float* s0, float* s1) { anim_slerp_weights_ex(d, w, 0, s0, s1); }

/* acos(d) by the same double-precision rational (the inlined arc cosine of the rider limb reach solvers 0062d970 / 0062fa90):
 * |d| >= 0.7071 (0x9ddfc8) works on sqrt(|1 - d^2|) and the complement; d < 0 (or unordered) mirrors; acos = pi/2 - that. */
static inline float anim_acos_approx(float d) {
    const float ad = fabsf(d);
    float u = ad;
    if (ad >= 0.7071f) u = sqrtf(fabsf((float)(1.0 - (double)(float)((double)d * (double)d))));
    float u2 = (float)((double)u * (double)u);
    float u4 = (float)((double)u2 * (double)u2);
    float p = (float)(0.5648093223571777 - (double)u2 * 0.04992850869894028);
    float q = (float)(1.0 - (double)u2 * 1.4430354833602905);
    float inv = (float)(1.0 / ((double)p * (double)u4 + (double)q));
    float n1 = (float)(0.3993034064769745 - (double)u2 * 0.0193784236907959);
    float n2 = (float)(1.0 - (double)u2 * 1.2763687372207642);
    float num = (float)(((double)n1 * (double)u4 + (double)n2) * (double)u);
    float r = (float)((double)inv * (double)num);
    if (ad >= 0.7071f) r = (float)(1.5707963705062866 - (double)r);
    if (!(d >= 0.0f)) r = rx_subs(-0.0f, r);
    return (float)(1.5707963705062866 - (double)r);
}
/* The inlined sine and cosine of an angle (sign, range reduction, the two polynomials). */
static inline void anim_sincos(float a, float* s_out, float* c_out) {
    float s = anim_sign(a);
    float t = anim_trig_reduce(rx_muls(s, a));
    float t2 = t * t, t4 = t2 * t2;
    *c_out = rx_subs(-0.0f, anim_cos_poly(t2, t4));
    *s_out = rx_muls(anim_sin_poly(t2, t4) * t, s);
}
