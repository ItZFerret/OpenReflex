/* bike_curve.c -- the vehicle tuning's curves (native/game/notes/bike.md, _re/notes/indexed-curve-literal-precision.md).
 *
 * Parameter curve C (ParamCurve, 0x5c bytes, e.g. S+4f74 the engine torque curve): +0x00 knot count n (2..10), +0x04 the knots
 * x[0..9], +0x2c the values y[0..9], +0x54 the start slope, +0x58 the end slope. Evaluated as a cubic Hermite-like segment.
 * Indexed table T (e.g. S+4934): 20 floats sampled every 0.5 of x, read by rounding 2x to the neighbouring entries. */
#include "bike_curve.h"
#include <math.h>

/* 00430810 Lerp_Clamped: t <= 0 -> a, t >= 1 -> b, else a + (b - a) * t (computed as (b - a) * t + a). */
float bike_lerp_clamped(float t, float a, float b) {
    if (0.0f >= t) return a;                   /* comiss 0, t; jb (NaN t: CF set -> falls to the next test) */
    if (t >= 1.0f /* 00931f24 */) return b;
    return phys_add(phys_mul(phys_sub(b, a), t), a);
}
RX_FUNCTION(0x00430810, bike_lerp_clamped)

/* The knot search of 00438ee0: the LAST segment i (0 <= i < n-1) with x[i] <= x <= x[i+1]; *t = (x - x[i]) / (x[i+1] - x[i]).
 * -1 when x lies outside every segment (*t then keeps -1.0, 009cd9e8). */
int bike_param_curve_segment(rx_addr curve, float x, float* t) {
    int32_t n = I32(curve, 0x00), seg = -1;
    *t = -1.0f;   /* 009cd9e8 */
    for (int32_t i = 0; i < n - 1; i++) {
        float x0 = F32(curve, 0x04 + 4 * i), x1 = F32(curve, 0x08 + 4 * i);
        if (x >= x0 && x1 >= x) { *t = phys_div(phys_sub(x, x0), phys_sub(x1, x0)); seg = i; }
    }
    return seg;
}

/* 00438ee0 ParamCurve_EvalCubic: outside the knots the end value (below x[0]: y[0], else y[n-1]). Inside segment i (fraction t,
 * width d): neighbours prev = y[i-1], cur = y[i], next = y[i+1], next2 = y[i+2]; the first segment takes prev = y[1] - 2 d s0,
 * the last next2 = y[i] + 2 d s1 (s0 / s1 the end slopes). With half differences B = (next - prev) / 2, A = (next2 - cur) / 2:
 *   C = 3 ((next - cur) - B) - (A - B),  D = ((A - B) - 2 C) * 0.3333333,  y = ((D t + C) t + B) t + cur. */
float bike_param_curve(rx_addr curve, float x) {
    int32_t n = I32(curve, 0x00);
    float t;
    int32_t i = bike_param_curve_segment(curve, x, &t);
    if (i == -1) return F32(curve, 0x04) > x ? F32(curve, 0x2c) : F32(curve, 0x28 + 4 * n);
#define Y(k) F32(curve, 0x2c + 4 * (k))
    float d = phys_sub(F32(curve, 0x08 + 4 * i), F32(curve, 0x04 + 4 * i));
    float prev, cur, next, next2;
    if (i == 0) {
        next2 = Y(2); next = Y(1); cur = Y(0);
        prev = phys_sub(next, phys_mul(phys_mul(F32(curve, 0x54), d), 2.0f /* 0092e330 */));
    } else {
        prev = Y(i - 1); cur = Y(i); next = Y(i + 1);
        next2 = (i == n - 2) ? phys_add(phys_mul(phys_mul(F32(curve, 0x58), d), 2.0f), cur) : Y(i + 2);
    }
#undef Y
    float A = phys_mul(phys_sub(next2, cur), 0.5f /* 009322cc */);
    float B = phys_mul(phys_sub(next, prev), 0.5f);
    float AB = phys_sub(A, B);
    float C = phys_sub(phys_mul(phys_sub(phys_sub(next, cur), B), 3.0f /* 0092fe48 */), AB);
    float D = phys_mul(phys_sub(AB, phys_mul(C, 2.0f)), rx_bf(0x3eaaaa9fu) /* 009774d8: 0.33333299 */);
    return phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(D, t), C), t), B), t), cur);
}
RX_FUNCTION(0x00438ee0, bike_param_curve)

/* x87: st = copysign(0.5, v) (msvcr90 _copysign via [00916230], a double), st += addend at the thread's precision (53 bits, BIKE_X87_BITS), then _ftol2_sse
 * (008fa0d0: fstp m64 ; cvttsd2si): round half away from zero, the sum rounded to 24 bits first. */
static int32_t round_half_away_x87(double v, double addend) {
    rx_f80 s = rx_f80_add(rx_f80_from_f64(copysign(0.5 /* 009774e8 */, v)), rx_f80_from_f64(addend), BIKE_X87_BITS, 0);
    return rx_cvtt(rx_f80_to_f64(s, 0));
}

/* 004390d0 IndexedCurve_Interp: u = 2x; left = round(round(u - 0.49999999999)), right = round(u) (halves away from zero, x87 sums at 53-bit precision),
 * t = u - round(u - 0.49999999999); lerp_clamped(t, T[left], T[right]). */
float bike_indexed_curve(rx_addr table, float x) {
    float u = phys_mul(x, 2.0f /* 0092e330 */);
    double ub = (double)u - 0.49999999999 /* 00977558 */;
    float low = (float)round_half_away_x87(ub, ub);                         /* cvtsi2ss */
    int32_t left = rx_cvtt((float)round_half_away_x87((double)low, (double)low));
    int32_t right = rx_cvtt((float)round_half_away_x87((double)u, (double)u));
    float t = phys_sub(u, low);
    float b = rx_bf(bike_x87_copy32(U32(table, 4 * right)));               /* read first (00439182) */
    float a = rx_bf(bike_x87_copy32(U32(table, 4 * left)));
    return bike_lerp_clamped(t, a, b);
}
RX_FUNCTION(0x004390d0, bike_indexed_curve)
