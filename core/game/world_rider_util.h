/* world_rider_util.h -- helpers shared by the rider share's files (core/game/world_rider*.c). */
#pragma once
#include "world_util.h"
#include "world_veh.h"
#include "phys_crt.h"

/* asin(s) as the rider code inlines it: below |s| < [0x9ddfc8] (0.7071, writable data) the rational approximation
 * x (1 - 1.2763687 x2 + (0.3993034 - 0.019378424 x2) x4) / (1 - 1.4430355 x2 + (0.5648093 - 0.04992851 x2) x4) of x = |s|,
 * above it pi/2 - that of x = sqrt(|1 - s^2|); negated for s < 0 (-0.0 - r). Every partial result is rounded to float as
 * the original's cvtsd2ss / cvtpd2ps (doubles 0x977788 .. 0x9777b0, 1.0 0x977638, pi/2 0x977510). |x| and the square root go
 * through Float_Abs_Thunk 00401040 and SqrtF 0042c230 as in the original. */
static inline float rider_asin(float s) {
    float ax;
    WORLD_CALL_SP(0, ax = rx_world_float_abs_thunk(s));
    const int wide = ax >= F32(0, 0x009ddfc8);
    float x = ax;
    if (wide) {
        const float s2 = (float)((double)s * (double)s);
        float a;
        WORLD_CALL_SP(0, a = rx_world_float_abs_thunk((float)(1.0 - (double)s2)));
        WORLD_CALL_SP(4, x = phys_sqrtf(a));
    }
    const double one = 1.0 /* 0x977638 */;
    const float x2 = (float)((double)x * (double)x);
    const float x4 = (float)((double)x2 * (double)x2);
    const float den_hi = (float)(0x1.212ebp-1 /* 0x9777b0 */ - (double)x2 * 0x1.9903acp-5 /* 0x9777a8 */);
    const float den_lo = (float)(one - (double)x2 * 0x1.716ac6p+0 /* 0x9777a0 */);
    const float inv_den = (float)(one / ((double)den_hi * (double)x4 + (double)den_lo));
    const float num_hi = (float)(0x1.98e2fep-2 /* 0x977798 */ - (double)x2 * 0x1.3d7fp-6 /* 0x977790 */);
    const float num_lo = (float)(one - (double)x2 * 0x1.46c01ap+0 /* 0x977788 */);
    const float num = (float)(((double)num_hi * (double)x4 + (double)num_lo) * (double)x);
    float r = (float)((double)inv_den * (double)num);
    if (wide) r = (float)(0x1.921fb6p+0 /* 0x977510 */ - (double)r);
    if (!(s >= 0.0f)) r = phys_sub(rx_bf(0x80000000u) /* 0x9774a8 */, r);
    return r;
}
/* clamp as the original's comiss ladders: x >= hi -> hi; else lo >= x -> lo (a NaN stays) */
static inline float rider_clamp(float x, float lo, float hi) {
    if (x >= hi) return hi;
    if (lo >= x) return lo;
    return x;
}
/* push ebp; mov ebp, esp; and esp, -align; sub esp, size; `pushes` pushes (world_frame_enter for any alignment) */
static inline world_frame rider_frame_enter(uint32_t size, int pushes, uint32_t align) {
    world_frame f; f.entry = rx_sp(); f.ebp = f.entry - 4;
    f.esp = (f.ebp & ~(align - 1u)) - size - 4u * (uint32_t)pushes;
    rx_set_sp(f.esp); return f;
}
/* The first-order lag the rider code inlines (as 0042d6b0 computes it): a filter {value +0, tau +4, k +8}:
 * k = min(dt, tau) * (1 / tau), value += (target - value) * k; returns the new value. */
static inline float rider_lag(rx_addr filter, float target, float dt) {
    const float tau = F32(filter, 4);
    const float step = tau > dt ? dt : tau;
    const float k = step * (1.0f / tau);
    const float value = F32(filter, 0);
    const float next = (target - value) * k + value;
    SF32(filter, 8, k);
    SF32(filter, 0, next);
    return next;
}
