/* phys_math.c -- the packed SSE math kernels of MXReflex.exe (0x0042c2xx..0x0042cdxx): sin, atan, atan2, quaternion slerp and
 * floor, per lane. The original computes all four lanes with masks (andps / andnps / orps selections of precomputed
 * candidates); every lane is independent, so each kernel is written here as its scalar lane function applied to the four lanes.
 * Constants are the .rdata vectors named in the comments (all four lanes equal unless said). */
#include "phys_math.h"

/* 0092daf4..0092db1c: the odd Taylor coefficients of sin, -1/3! .. -1/23! */
static const uint32_t k_sin_coeff[11] = {0xbe2aaaab, 0x3c088889, 0xb9500d01, 0x3638ef1d, 0xb2d7322b, 0x2f309231,
                                         0xab573f9f, 0x274a963c, 0xa317a4da, 0x1eb8dc78, 0x9a3b0da1};
#define K_INV_2PI   0x3e22f983u   /* 0092dc00 */
#define K_2PI       0x40c90fdbu   /* 0092dbf0 */
#define K_HALF_PI   0x3fc90fdbu   /* 0093a6c0 */
#define K_PI        0x40490fdbu

/* 0042c240 lane: range reduction y = x - 2pi * round(x / 2pi) (the quotient unrounded when |x / 2pi| >= 2^23 or NaN:
 * 0092dc50 abs mask, 0092dc70 2^23 as an integer compare), then y + c0 y^3 + c1 y^5 + ... + c10 y^23 accumulated in that
 * order (each term coefficient * power, the last one power * coefficient). */
static float sin_lane(float x) {
    float q = rx_bf(K_INV_2PI) * x;
    int fits = (int32_t)0x4b000000 > (int32_t)(rx_fb(q) & 0x7fffffffu);
    float turns = fits ? (float)rx_cvt(q) : q;
    float y = x - turns * rx_bf(K_2PI);
    float y2 = y * y, p = y2 * y;                       /* y^2, y^3 */
    float acc = rx_bf(k_sin_coeff[0]) * p + y;
    for (int k = 1; k <= 9; k++) {                      /* y^5 .. y^21 */
        p = p * y2;
        acc = rx_bf(k_sin_coeff[k]) * p + acc;
    }
    p = p * y2;                                         /* y^23 */
    return p * rx_bf(k_sin_coeff[10]) + acc;
}

rx_v4 phys_vec4_sin(rx_v4 x) {
    for (int i = 0; i < 4; i++) x.v[i] = sin_lane(x.v[i]);
    return x;
}
RX_FUNCTION(0x0042c240, phys_vec4_sin)

/* 0042c650 lane: a = |x| (max(0 - x, x)), inverted when a > 1; reduced by pi/6 when a > tan(pi/12) (0x3e8930a3):
 * t = (a (sqrt3 - 1) + a - 1) / (sqrt3 + a); then t + t * P(t^2) / Q(t^2) (t itself when |t| <= 2^-12), negated for the
 * inverted case, plus the offset (pi/6 or pi/3 when reduced, pi/2 when inverted and not reduced), the sign of x
 * (x <= 0 negates), and +-pi/2 for |x| >= 2^126. */
/* what the original leaves in its stack frame (per lane; see phys_math_dead_stack) */
struct atan_spill { float tiny_mask, t, offset_reduced, offset_inverted; };

static float atan_lane(float x, struct atan_spill* spill) {
    const float minus_one = -1.0f;                      /* 0092dbc0 */
    float a = rx_maxss(0.0f - x, x);
    int inverted = 1.0f < a;                            /* 0092dba0 */
    float inv = 1.0f / a;
    a = inverted ? inv : a;
    int reduced = rx_bf(0x3e8930a3) < a;                /* tan(pi / 12) */
    float t = (rx_bf(0x3f3b67af) * a + a + minus_one) / (rx_bf(0x3fddb3d7) + a);   /* sqrt3 - 1, sqrt3 */
    t = reduced ? t : a;
    float offset = reduced ? (inverted ? rx_bf(0x3f860a92) : rx_bf(0x3f060a92))    /* pi/3, pi/6 */
                           : (inverted ? rx_bf(K_HALF_PI) : 0.0f);
    int tiny = rx_maxss(0.0f - t, t) <= rx_bf(0x39800000);                         /* 2^-12 */
    float t2 = t * t;
    float p = (((rx_bf(0xbf566bd7) * t2 + rx_bf(0xc107e9fb)) * t2 + rx_bf(0xc1a40bfe)) * t2 + rx_bf(0xc15b0533)) * t2;
    float q = (((rx_bf(0x4170624f) + t2) * t2 + rx_bf(0x426e5052)) * t2 + rx_bf(0x42ac5090)) * t2 + rx_bf(0x422443e6);
    float r = p / q * t + t;
    float v = tiny ? t : r;
    if (spill) {
        spill->tiny_mask = rx_bf(tiny ? ~0u : 0u); spill->t = t;
        spill->offset_reduced = reduced ? offset : 0.0f; spill->offset_inverted = reduced ? 0.0f : offset;
    }
    v = inverted ? phys_mul(v, minus_one) : v;
    v = v + offset;
    v = x <= 0.0f ? phys_mul(v, minus_one) : v;
    const float huge = rx_bf(0x7e800000);               /* 2^126 */
    if (x <= phys_mul(huge, minus_one)) return phys_mul(minus_one, rx_bf(K_HALF_PI));
    if (huge <= x) return rx_bf(K_HALF_PI);
    return v;
}

rx_v4 phys_vec4_atan(rx_v4 x) {
    for (int i = 0; i < 4; i++) x.v[i] = atan_lane(x.v[i], NULL);
    return x;
}
RX_FUNCTION(0x0042c650, phys_vec4_atan)

/* 0042c880 lane: atan2(y, x) = atan(y / x) + (pi with the sign of y when x is negative), except the special cases
 * (selected by masks in the original; the regular case is marked by an all-ones mask):
 *   y infinite: x infinite -> +-pi/4 (x >= +0) or +-3pi/4, else +-pi/2;   x infinite (y finite): +-0 (x >= +0) or +-pi;
 *   y == 0: +-0 or +-pi;   x == 0: +-pi/2.   "+-" = the sign of y, "x >= +0" = the sign bit of x clear. */
struct atan2_spill { float special, regular_mask, x_pos_mask, x_neg_offset; struct atan_spill atan; };

static float atan2_lane(float y, float x, struct atan2_spill* spill) {
    const uint32_t sign_y = rx_fb(y) & 0x80000000u;
    const int x_pos = (rx_fb(x) & 0x80000000u) == 0;
    const int x_inf = rx_bf(rx_fb(x) & 0x7fffffffu) == rx_bf(0x7f800000u);
    const int y_inf = rx_bf(rx_fb(y) & 0x7fffffffu) == rx_bf(0x7f800000u);
    const float pi_y = rx_bf(K_PI | sign_y), half_pi_y = rx_bf(K_HALF_PI | sign_y);
    const float on_axis = x_pos ? rx_bf(sign_y) : pi_y;             /* y == 0 or x infinite */
    const float x_neg_offset = x_pos ? 0.0f : pi_y;
    float div = y / x, special;                                     /* special: the result unless "regular" */
    const uint32_t regular = 0xffffffffu;
    if (x_inf) special = y_inf ? (x_pos ? rx_bf(0x3f490fdbu | sign_y) : rx_bf(0x4016cbe4u | sign_y)) : on_axis;   /* pi/4, 3pi/4 */
    else if (y_inf) special = half_pi_y;
    else if (0.0f == y) special = on_axis;
    else if (0.0f == x) special = half_pi_y;
    else special = rx_bf(regular);
    if (spill) {
        spill->special = special; spill->regular_mask = rx_bf(rx_fb(special) == regular ? ~0u : 0u);
        spill->x_pos_mask = rx_bf(x_pos ? ~0u : 0u); spill->x_neg_offset = x_neg_offset;
    }
    float a = atan_lane(div, spill ? &spill->atan : NULL) + x_neg_offset;
    return rx_fb(special) == regular ? a : special;
}

rx_v4 phys_vec4_atan2(rx_v4 y, rx_v4 x) {
    for (int i = 0; i < 4; i++) y.v[i] = atan2_lane(y.v[i], x.v[i], NULL);
    return y;
}
RX_FUNCTION(0x0042c880, phys_vec4_atan2)

/* 0042ca00 quaternion slerp: slerp(q0, q1, t) along the shortest arc (q1 weighted with the sign of the dot), linear weights
 * (1 - t, t) when |dot| >= 0.99999 (0093a6e0). The weights vector is ((-t.y) + 1, t.x + 0, 0, 0) (shufps 0xb1 of the t
 * register, mask 0093a700, sign 0093a6f0, plus 0092db50): the original reads lane 1 of the t register for 1 - t, so callers
 * pass t splatted. Each weight k = sin(angle * w) / sin(angle) with angle = atan2(sqrt(1 - |dot|^2), |dot|). */
rx_v4 phys_quat_slerp(rx_v4 q0, rx_v4 q1, rx_v4 t) {
    const float dot = phys_dot4(q0, q1);
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    const float cosine = phys_mul(sign, dot);
    const int curved = cosine < rx_bf(0x3f7fff58);
    const float sine = phys_sqrt(1.0f - cosine * cosine);
    const float angle = atan2_lane(sine, cosine, NULL);
    const float w0 = rx_bf(rx_fb(t.v[1]) ^ 0x80000000u) + 1.0f, w1 = t.v[0] + 0.0f;
    const float k0 = curved ? phys_div(sin_lane(angle * w0), sine) : w0;
    const float k1 = curved ? phys_div(sin_lane(angle * w1), sine) : w1;
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = phys_add(phys_mul(phys_mul(k1, sign), q1.v[i]), phys_mul(k0, q0.v[i]));
    return r;
}
RX_FUNCTION(0x0042ca00, phys_quat_slerp)

/* 0042ccd0 lane (|x| < 2^23): round with the 2^23 trick (2^23 with the sign of x: 0092..f0 | x & -0.0f), minus 1 where that
 * rounded up (r - x > +-0, or unordered). */
static float floor_lane(float x) {
    const float signed_zero = rx_bf(rx_fb(x) & 0x80000000u);            /* 009774a8 = -0.0f as a mask */
    const float magic = rx_bf(0x4b000000u | rx_fb(signed_zero));        /* 009774f0 = 2^23 */
    float r = phys_sub(phys_add(x, magic), magic);
    float up = phys_sub(r, x);
    return phys_sub(r, !(up <= signed_zero) ? 1.0f : 0.0f);                     /* 00931f24 = 1.0f */
}

rx_v4 phys_vec4_floor(rx_v4 x) {
    for (int i = 0; i < 4; i++) x.v[i] = floor_lane(x.v[i]);
    return x;
}
RX_FUNCTION(0x0042ccd0, phys_vec4_floor)

/* 0042c390 sin and cos per lane (Vec4Cos_RangeReduced; cdecl sin_out*, ECX = cos_out*, xmm0 = x): the range reduction of
 * 0042c240, then both Taylor series to y^23 / y^22. The powers are built from each other as the original does (the rounding of
 * each power depends on it): y2 = y y, y3 = y2 y, y4 = y2 y2, y5 = y3 y2, y6 = y3 y3, y7 = y4 y3, y8 = y4 y4, y9 = y5 y4,
 * y10 = y5 y5, y11 = y6 y5, y12 = y6 y6, y13 = y7 y6, y14 = y7 y7, y15 = y8 y7, y16 = y8 y8, y17 = y9 y8, y18 = y9 y9,
 * y19 = y10 y9, y20 = y10 y10, y21 = y11 y10, y22 = y11 y11, y23 = y12 y11; terms added from the lowest power up.
 * cos coefficients 0092db24.. (-1/2!, 1/4!, ..., -1/22!), sin coefficients 0092daf4.. (as 0042c240). Stores sin, then cos. */
static const uint32_t k_cos_coeff[11] = {0xbf000000, 0x3d2aaaab, 0xbab60b61, 0x37d00d01, 0xb493f27e, 0x310f76c8,
                                         0xad49cba5, 0x29573f9f, 0xa53413c3, 0x20f2a15d, 0x9c8671cb};
static void sincos_lane(float x, float* sin_out, float* cos_out) {
    float q = rx_bf(K_INV_2PI) * x;
    int fits = (int32_t)0x4b000000 > (int32_t)(rx_fb(q) & 0x7fffffffu);
    float turns = fits ? (float)rx_cvt(q) : q;
    float y[24];                                        /* y[k] = y^k */
    y[1] = x - turns * rx_bf(K_2PI);
    y[2] = y[1] * y[1];
    for (int k = 3; k <= 23; k++) y[k] = y[(k + 1) / 2] * y[k / 2];
    float c = rx_bf(k_cos_coeff[0]) * y[2] + 1.0f;
    for (int k = 1; k < 11; k++) c = c + rx_bf(k_cos_coeff[k]) * y[2 * k + 2];
    float s = rx_bf(k_sin_coeff[0]) * y[3] + y[1];
    for (int k = 1; k < 11; k++) s = s + rx_bf(k_sin_coeff[k]) * y[2 * k + 3];
    *sin_out = s; *cos_out = c;
}

void phys_vec4_sincos(rx_v4 x, rx_addr sin_out, rx_addr cos_out) {
    rx_v4 s, c;
    for (int i = 0; i < 4; i++) sincos_lane(x.v[i], &s.v[i], &c.v[i]);
    v4_st(sin_out, s);
    v4_st(cos_out, c);
}
RX_FUNCTION(0x0042c390, phys_vec4_sincos)

/* 0042caf0 Vec4_SplatLane (cdecl lane index 0..3, xmm0): every lane = lane `index` of x (through a stack copy, stack cookie). */
rx_v4 phys_vec4_splat_lane(rx_v4 x, int32_t index) { return v4_splat(x.v[index & 3]); }
RX_FUNCTION(0x0042caf0, phys_vec4_splat_lane)

#if defined(RX_ORACLE) || defined(RX_FOURWHEEL_ACTIVE_STACK)
/* Oracle only: the dead stack QuatSlerp_Simd leaves (its frame, its nested Vec4Atan2 / Vec4Atan frames, their return addresses
 * and saved EBPs). A function of another subsystem copies some of these uninitialized words into the heap (notes/dead-stack.md;
 * the reader is not identified yet): until it is ported with a defined value, the oracle reproduces the words so the whole-memory
 * hash matches. esp = ESP at entry (the return address slot), ebp = EBP at entry. The product does not need this. */
void phys_quat_slerp_dead_stack(rx_addr esp, uint32_t ebp, rx_v4 q0, rx_v4 q1, rx_v4 t) {
    const float dot = phys_dot4(q0, q1);
    const float sign = dot < 0.0f ? -1.0f : 1.0f;
    const float cosine = phys_mul(sign, dot);
    const int curved = cosine < rx_bf(0x3f7fff58);
    const float sine = phys_sqrt(1.0f - cosine * cosine);
    struct atan2_spill sp;
    atan2_lane(sine, cosine, &sp);
    const float w0 = rx_bf(rx_fb(t.v[1]) ^ 0x80000000u) + 1.0f, w1 = t.v[0] + 0.0f;
    /* slerp: push ebp; and esp, -16; sub esp, 0x60 */
    const rx_addr f = ((esp - 4) & ~15u) - 0x60, f2 = f - 0x80, f3 = f2 - 0x70;
    SU32(esp, -4, ebp);
    v4_st(f + 0x00, v4(w0, w1, 0.0f + 0.0f, 0.0f + 0.0f));
    v4_st(f + 0x10, v4_splat(sine));
    v4_st(f + 0x20, v4_splat(rx_bf(curved ? ~0u : 0u)));
    v4_st(f + 0x30, v4_splat(sign));
    v4_st(f + 0x40, q1);
    v4_st(f + 0x50, q0);
    SU32(f, -4, 0x0042caacu);                           /* the return address of the last call (Vec4Sin) */
    /* Vec4Atan2 (entry esp f - 4): saved ebp, frame 0x70 */
    SU32(f, -8, esp - 4);
    v4_st(f2 + 0x00, v4_splat(sine));
    v4_st(f2 + 0x10, v4_splat(rx_bf(K_HALF_PI | (rx_fb(sine) & 0x80000000u))));
    v4_st(f2 + 0x20, v4_splat(sp.special));
    v4_st(f2 + 0x30, v4_splat(sp.regular_mask));
    v4_st(f2 + 0x40, v4_splat(cosine));
    v4_st(f2 + 0x50, v4_splat(sp.x_pos_mask));
    v4_st(f2 + 0x60, v4_splat(sp.x_neg_offset));
    SU32(f2, -4, 0x0042c9cbu);
    /* Vec4Atan (entry esp f2 - 4): saved ebp, frame 0x60 */
    SU32(f2, -8, f - 8);
    v4_st(f3 + 0x00, v4_splat(rx_bf(K_HALF_PI)));
    v4_st(f3 + 0x10, v4_splat(sp.atan.tiny_mask));
    v4_st(f3 + 0x20, v4_splat(sp.atan.t));
    v4_st(f3 + 0x30, v4_splat(sp.atan.offset_reduced));
    v4_st(f3 + 0x40, v4_splat(sp.atan.offset_inverted));
    v4_st(f3 + 0x50, v4_zero());
}
#endif
