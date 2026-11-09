/* phys_misc.c -- small helpers of the 0x0042xxxx range the physics frame runs: a first-order lag filter, its setter, the
 * RavenPhysics block-matrix constructors, a cleared pose, and the simulation clock. */
#include "phys_misc.h"
#include "race_driver.h"
#include "world_wreck.h"

/* 0042d6b0 (EAX = filter F, xmm2 = dt, cdecl target; RET 4): first-order lag toward target with time constant F+4:
 * k = min(dt, tau) * (1 / tau) (F+8), value F+0 += (target - value) * k. Returns the new value (xmm0). */
float phys_lag_filter_update(rx_addr f, float dt, float target) {
    const float tau = F32(f, 4);
    const float step = tau > dt ? dt : tau;
    /* Original DIVSS, MULSS, SUBSS, MULSS, ADDSS operand order. Plain C
     * produces the host ISA's invalid/NaN payloads instead of x86's. */
    const float k = phys_mul(step, phys_div(F32(0x931f24, 0), tau));
    const float value = F32(f, 0);
    const float next = phys_add(phys_mul(phys_sub(target, value), k), value);
    SF32(f, 8, k);
    SF32(f, 0, next);
    return next;
}
RX_FUNCTION(0x0042d6b0, phys_lag_filter_update)

/* 0042d670 (ECX = filter F, xmm1 = value, cdecl tau; RET 4): sets the value and / or the time constant, each unless it is
 * FLT_MAX (00977494, "keep"); a new time constant also sets k (F+8) to 1. */
void phys_lag_filter_set(rx_addr f, float value, float tau) {
    const float keep = rx_bf(0x7f7fffffu);
    if (value != keep) SF32(f, 0, value);
    if (tau != keep) { SF32(f, 4, tau); SF32(f, 8, 1.0f); }
}
RX_FUNCTION(0x0042d670, phys_lag_filter_set)

/* RavenPhysics matrix objects: +0 vtable, +4 rows, +8 columns; block matrices hold sub-matrix objects (each with vtable,
 * rows, columns) at +0x10 and further (native/game/notes/mechanism.md). */
#define VT_MATRIX3X3 0x0093ca04u
#define VT_MATRIX3X4 0x0093ca28u
#define VT_MATRIX4X3 0x0093ca4cu
#define VT_MATRIX6X6 0x0093ca94u
#define VT_MATRIX4X6 0x0093cab8u
#define VT_MATRIX6X4 0x0093cadcu

/* 0042f1d0 (EAX = this): Matrix6x6 = four Matrix3x3 blocks at +0x10, +0x50, +0x90, +0xd0 (stores in the original's order). */
void phys_matrix6x6_construct(rx_addr m) {
    SU32(m, 0, VT_MATRIX6X6); SU32(m, 4, 6); SU32(m, 8, 6);
    for (uint32_t b = 0x10; b <= 0xd0; b += 0x40) { SU32(m, b + 4, 3); SU32(m, b + 8, 3); SU32(m, b, VT_MATRIX3X3); }
}
RX_FUNCTION(0x0042f1d0, phys_matrix6x6_construct)

/* 0042f230 (EAX = this): Matrix4x6 = two Matrix4x3 blocks at +0x10, +0x60. */
void phys_matrix4x6_construct(rx_addr m) {
    SU32(m, 8, 6); SU32(m, 0, VT_MATRIX4X6); SU32(m, 4, 4);
    SU32(m, 0x14, 4); SU32(m, 0x18, 3); SU32(m, 0x10, VT_MATRIX4X3);
    SU32(m, 0x64, 4); SU32(m, 0x68, 3); SU32(m, 0x60, VT_MATRIX4X3);
}
RX_FUNCTION(0x0042f230, phys_matrix4x6_construct)

/* 0042f2a0 (EAX = this): Matrix6x4 = two Matrix3x4 blocks at +0x10, +0x50. */
void phys_matrix6x4_construct(rx_addr m) {
    SU32(m, 4, 6); SU32(m, 0, VT_MATRIX6X4); SU32(m, 8, 4);
    SU32(m, 0x14, 3); SU32(m, 0x18, 4); SU32(m, 0x10, VT_MATRIX3X4);
    SU32(m, 0x54, 3); SU32(m, 0x58, 4); SU32(m, 0x50, VT_MATRIX3X4);
}
RX_FUNCTION(0x0042f2a0, phys_matrix6x4_construct)

/* 0042e5f0 (ECX = this): +0 = the runtime zero vec4 (0x00d61670), +0x10 and +0x20 = the runtime vec4 at 0x00d61640.
 * Returns this (EAX). */
rx_addr phys_pose_clear(rx_addr p) {
    v4_st(p + 0x00, v4_ld(0x00d61670));
    v4_st(p + 0x10, v4_ld(0x00d61640));
    v4_st(p + 0x20, v4_ld(0x00d61640));
    return p;
}
RX_FUNCTION(0x0042e5f0, phys_pose_clear)

/* The engine's time source (another subsystem): a static object at 0x00da6ca0, constructed once (FUN_006de1f0 + atexit of
 * its destructor thunk 0x00913e00, guard bit 0 of 0x00dcf304), queried by FUN_006def20 (EAX = object; xmm0 = seconds). */
static float engine_time_now(void) {
    race_static_dcf304();                                       /* 006de1f0 behind its guard 0x00dcf304 + the atexit */
    float t;
    PHYS_CALL_SP(0, t = rx_world_timer_seconds(0x00da6ca0));    /* 006def20 */
    return t;
}

/* 0042ef70 SimulationClock_Advance (cdecl dt): keeps a simulation clock in step with the engine's time source:
 * base 0x00a89be4, elapsed 0x00a89be8, correction 0x00a89bec, "reset pending" 0x00a89bf0. When the engine context's clock
 * (context 0x00a95844 vfunc 2, then its vfunc 31 (0x2c, 0)) reports a restart, the clock restarts at the current time.
 * If the predicted time (correction + elapsed + base) drifts from the source by more than the limit at 0x009cf5a8 (0.03 s,
 * writable data), it is either rebased (no reset pending) or the drift goes into the correction. Then elapsed += dt. */
void phys_simulation_clock_advance(float dt) {
    const rx_addr context = U32(0, 0x00a95844);
    const rx_addr clock = rx_call_original(U32(U32(context, 0), 8), RX_THISCALL, context, 0, NULL);
    const uint32_t restart_args[2] = {0x2c, 0};
    float predicted;
    if (rx_call_original(U32(U32(clock, 0), 0x7c), RX_THISCALL, clock, 2, restart_args) != 0) {
        const float now = engine_time_now();
        SF32(0, 0x00a89be4, now);
        SF32(0, 0x00a89be8, 0.0f);
        SF32(0, 0x00a89bec, 0.0f);
        SU32(0, 0x00a89bf0, 0);
        predicted = 0.0f + F32(0, 0x00a89be8) + F32(0, 0x00a89be4);
    } else {
        predicted = F32(0, 0x00a89bec) + F32(0, 0x00a89be8) + F32(0, 0x00a89be4);
    }
    const float now = engine_time_now();
    const float drift = rx_bf(rx_fb(predicted - now) & 0x7fffffffu);   /* Float_Abs 00401000 */
    float elapsed = F32(0, 0x00a89be8);
    if (drift > F32(0, 0x009cf5a8)) {
        if (U32(0, 0x00a89bf0) == 0) {
            SF32(0, 0x00a89be4, now);
            SF32(0, 0x00a89bec, 0.0f);
            elapsed = 0.0f;
        } else {
            SF32(0, 0x00a89bec, F32(0, 0x00a89bec) + drift);
            elapsed = F32(0, 0x00a89be8);
        }
    }
    SF32(0, 0x00a89be8, elapsed + dt);
    SU32(0, 0x00a89bf0, 0);
}
RX_FUNCTION(0x0042ef70, phys_simulation_clock_advance)

/* 00424370 (xmm0 = in_lo, xmm1 = in_hi, cdecl value, out_lo, out_hi): maps value from [in_lo, in_hi] linearly onto
 * [out_lo, out_hi] (in double precision, the fraction rounded to float first) and clamps it to the output range; out_lo when
 * the input range is empty (in_lo == in_hi). */
float phys_remap_clamped(float in_lo, float in_hi, float value, float out_lo, float out_hi) {
    if (in_lo == in_hi) return out_lo;
    const float t = (float)(((double)value - (double)in_lo) / ((double)in_hi - (double)in_lo));
    const float lo = out_hi > out_lo ? out_lo : out_hi;
    const float hi = out_lo > out_hi ? out_lo : out_hi;
    float r = (float)(((double)out_hi - (double)out_lo) * (double)t + (double)out_lo);
    if (!(r > lo)) r = lo;
    if (!(hi > r)) r = hi;
    return r;
}
RX_FUNCTION(0x00424370, phys_remap_clamped)
