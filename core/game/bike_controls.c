/* bike_controls.c -- the motocross control update, slot 9 (_re/notes/bike-controls-recovery.md, bike-controls-roadmap.md,
 * simulation-input.md).
 *
 * Slot 9 (0044c170) turns the raw input block S+1c (throttle +1c, reverse +20, front / rear brake +24 / +28, steering x / y
 * +2c / +30, rider reflex x / y +34 / +38, raw flag +40, clutch +48) into the processed control block C = *(S+5c30):
 *   C+00 throttle      C+04 reverse drive     C+08 / C+0c rear / front brake torque   C+10 steering angle
 *   C+20 drag vector   C+30 rider lean offsets C+40 lean          C+50 steering quaternion
 *   C+60 engine scale  C+64 pitch target      C+68..74 raw steering x, y, reflex x, y
 *   C+78 / C+7c shift-up / -down pulses (manual gearbox S+2eb0)   C+80 clutch   C+84 engine mode
 * using the tuning curves of the parameter block S+2f10.. and seven scalar filters {value, time, factor} at S+5c34 (front
 * brake), 5c48 (rear brake), 5c5c (steering), 5c70 (pitch), 5c84 (steering correction), 5c98 (reflex y), 5cac (engine).
 * Wheel records: rear W = S+8790 (contact S+8890, W+80 = S+8810), front W = S+88e0 (contact S+89e0, W+80 = S+8960);
 * both wheels airborne S+8d98. This thread's x87 precision: 53 bits (CW 027f). */
#include "bike_controls.h"
#include "phys_misc.h"
#include "crt.h"
#include "bike_curve.h"
#include "phys_terrain.h"
#include "world_veh.h"

#define CTL(S)        U32(S, 0x5c30)
#define G_AXIS_X      0x00d615c0
#define G_AXIS_UP     0x00d616a0
#define G_AXIS_FWD    0x00d61680
#define G_NUMERATOR   0x00d61650
#define G_ZERO        0x00d61670
#define G_ONES        0x00d61690
#define G_ASIN_SPLIT  0x00d61660
#define G_MINUS_ONE   0x009cd9e8      /* .data float -1.0 */

/* 0042d6b0 (phys): first-order filter F = {value, time, factor} towards input: EAX = &F, xmm2 = dt, stack input; ret 4.
 * Returns the new value (its xmm0). */
static float ctl_filter(rx_addr F, float dt, float input) {
    return phys_lag_filter_update(F, dt, bike_x87_copyf(input));        /* fld m32 ; fstp m32 */
}
/* 0042d670 (phys): sets a filter: value (xmm1) unless FLT_MAX, time (stack) unless FLT_MAX (then factor = 1); ECX = &F; ret 4. */
static void ctl_filter_set(rx_addr F, float value, float time) {
    phys_lag_filter_set(F, value, bike_x87_copyf(time));
}
/* 00401020 Float_Fmod (CRT _CIfmod through x87): cdecl (a, b), result st(0), stored with fstp m32. */
static float ctl_fmod(float a, float b) {
    return rx_f80_to_f32(crt_ci_fmod(rx_f80_from_f32(a), rx_f80_from_f32(b), CRT_CW_MAIN), 0);   /* fld a ; fld b ; _CIfmod */
}
/* A direct call at the original call site: the callee enters with its `n` stack arguments (and the return-address slot the
 * replaced call left) below the caller's stack pointer. Returns the caller's SP to restore after the call. */
static rx_addr ctl_callsite(uint32_t n, const uint32_t* a) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 4u * n - 4u;
    SU32(sp, 0, 0);
    for (uint32_t i = 0; i < n; i++) SU32(sp + 4 + 4u * i, 0, a[i]);
    rx_set_sp(sp);
    return sp0;
}
/* 00402dc0 (world): the world of the current scene from the registry 009e30d4 (function-local static, initialized here). */
static rx_addr ctl_current_world(void) {
    if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    const rx_addr sp0 = ctl_callsite(0, NULL);
    const rx_addr w = rx_world_find_by_controller(0x009e30d4);
    rx_set_sp(sp0);
    return w;
}
/* 007c6cc0 TerrainSegmentQuery (world): ESI = world, ECX = &start, EAX = &end (xyz read), stack (&hit point, &hit normal);
 * ret 8; EAX = hit. Called at the original's stack position (frame of 0044c170: entry ESP E, ebp = E - 4, aligned down to 16
 * = A; locals at A - 0xb0, start at A - 0x80, end at A - 0x40, outputs at A - 0x60 / A - 0x10): the terrain code reads stack
 * it has not written (its result does not depend on it, but errno does), so its frame must lie where the original's does. */
static uint32_t ctl_terrain_segment(rx_addr entry_sp, rx_addr world, rx_v4 start, rx_v4 end) {
    const rx_addr sp = rx_sp(), A = (entry_sp - 4) & ~15u;
    rx_st4(A - 0x80, start); rx_st4(A - 0x40, end);
    rx_set_sp(A - 0xb0);
    const uint32_t args[2] = {A - 0x60, A - 0x10};
    uint32_t regs[2] = {A - 0x80, 0};
    const rx_addr call_sp = ctl_callsite(2, args);
    const uint32_t hit = phys_terrain_segment_query(world, A - 0x80, A - 0x40, args[0], args[1], regs);
    (void)call_sp;
    rx_set_sp(sp);
    return hit;
}

/* step towards target by at most max_step: (min(|target - f|, max_step) * sign + f) on the x87 stack, stored as float */
static float ctl_step_towards(float f, float target, float max_step) {
    float d = phys_sub(target, f);
    float sg = d >= 0.0f ? 1.0f : F32(0, G_MINUS_ONE);
    float a = bike_fabs(d);
    float step = max_step > a ? a : max_step;
    return bike_x87_mul_add(step, sg, f);
}

/* The steering quaternion C+50 about y from the half steering angle h = C+10 / 2: sine and cosine by polynomials of the
 * wrapped angles (u from |h|, w from |h + pi/2|): C+50 = (0, B(w), 0, -A(u)). */
static void ctl_steering_quaternion(rx_addr S) {
    const rx_addr C = CTL(S);
    const float h = phys_mul(F32(C, 0x10), 0.5f /* 009322cc */);
    float a = bike_fabs(h);
    a = phys_sub(a, phys_mul((float)rx_cvtt(phys_mul(a, 0.15915493667125702f)), 6.2831854820251465f));
    const float u = phys_sub(3.1415927410125732f, a), u2 = phys_mul(u, u), u4 = phys_mul(u2, u2);
    float b = bike_fabs(phys_add(h, 1.5707963705062866f));
    b = phys_sub(b, phys_mul((float)rx_cvtt(phys_mul(b, 0.15915493667125702f)), 6.2831854820251465f));
    const float w = phys_sub(3.1415927410125732f, b), w2 = phys_mul(w, w), w4 = phys_mul(w2, w2);
    const float k6 = rx_bf(0xb46beed0u) /* 00935d50 */, k4 = rx_bf(0x37cb076bu) /* 00935d48 */, k3 = rx_bf(0x3d2aa372u) /* 00935d4c */;
    const float k2 = rx_bf(0xbab5a662u) /* 00935d44 */, k1 = rx_bf(0xbeffff3du) /* 00935d40 */, k0 = rx_bf(0x3f7ffff3u) /* 00935d3c */;
    const float A = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, k6), k4), u4), phys_add(phys_mul(u2, k2), k3)), u4),
                             phys_add(phys_mul(u2, k1), k0));
    const float B = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(w2, k6), k4), w4), phys_add(phys_mul(w2, k2), k3)), w4),
                             phys_add(phys_mul(w2, k1), k0));
    rx_st4(C + 0x50, v4(0.0f, B, 0.0f, phys_sub(-0.0f, A)));
}

/* 0043e9f0 BikeControls_SmoothGroundDirection (stack S, dt; ret 8): the reference "up" S+8b50: on the ground and steering
 * (|C+68| >= 0.02) the wheel contact normals (both: their normalized sum scaled by 00d615d0), else world up; world up as well
 * when the steering direction disagrees with the side of the normal. Each lane is smoothed by the filters *(S+8b40/44/48). */
void bike_smooth_ground_direction(rx_addr S, float dt) {
    const rx_v4 up = rx_ld4(G_AXIS_UP);
    rx_v4 d = up;
    if (U32(S, 0x8d98) == 0 && !(0.019999999552965164f > bike_fabs(F32(CTL(S), 0x68)))) {
        if (U32(S, 0x8890) != 0) {
            if (U32(S, 0x89e0) != 0)
                d = bike_normalize(v4_mul(v4_add(rx_ld4(S + 0x8a10), rx_ld4(S + 0x88c0)), rx_ld4(0x00d615d0)));
            else d = rx_ld4(S + 0x88c0);          /* rear contact normal */
        } else if (U32(S, 0x89e0) != 0) d = rx_ld4(S + 0x8a10);   /* front contact normal */
    }
    {   /* keep it only on the side the rider steers to */
        const rx_v4 fwd = bike_rotate(rx_ld4(S + 0x730), rx_ld4(G_AXIS_FWD));
        const int right = bike_all3(v4_cmplt(rx_ld4(G_ZERO), v4_splat(bike_dot3(fwd, bike_cross(up, d)))));
        const int not_left = !(F32(CTL(S), 0x68) > 0.0f);
        if (right != not_left) d = rx_ld4(G_AXIS_UP);
    }
    rx_v4 out = d;
    for (int i = 0; i < 3; i++) {             /* inline 0042d6b0 filters (factor = min(dt, T) * (1 / T)) */
        rx_addr F = U32(S, 0x8b40 + 4u * (uint32_t)i);
        float T = F32(F, 4), x = T > dt ? dt : T;
        float k = phys_mul(x, phys_div(1.0f, T));
        float old = F32(F, 0);
        float val = phys_add(phys_mul(phys_sub(d.v[i], old), k), old);
        if (i == 0) { SF32(F, 0, val); SF32(F, 8, k); } else { SF32(F, 8, k); SF32(F, 0, val); }
        out.v[i] = val;
    }
    rx_st4(S + 0x8b50, out);
}
RX_FUNCTION(0x0043e9f0, bike_smooth_ground_direction)

/* 0044e0d0 BikeControls_UpdateRiderOff (EDI = S, stack dt; ret 4): slot 9 while the rider is off the bike: no inputs, the
 * drag vector and friction coefficients from the speed curves alone, the steering quaternion and lean target. */
void bike_controls_rider_off(rx_addr S, float dt) {
    const rx_v4 zero = v4_splat(0.0f);
    SF32(CTL(S), 0x68, 0.0f); SF32(CTL(S), 0x6c, 0.0f); SF32(CTL(S), 0x70, 0.0f); SF32(CTL(S), 0x74, 0.0f);
    const rx_v4 v = rx_ld4(S + 0x100), q = rx_ld4(S + 0x730);
    const float fwd = bike_rotate_inv(q, v).v[2];
    rx_addr C = CTL(S);
    rx_st4(C + 0x20, v4_sub(zero, bike_normalize(v)));
    {
        float k = bike_param_curve(S + (U32(S, 0x8d98) != 0 ? 0x34ac : 0x340c), fwd);
        rx_st4(C + 0x20, v4_mul(v4_splat(k), rx_ld4(C + 0x20)));
    }
    rx_st4(CTL(S) + 0x30, zero);
    SF32(CTL(S), 0x00, 0.0f);
    SF32(CTL(S), 0x04, 0.0f);
    SU32(CTL(S), 0x80, 0);
    SF32(CTL(S), 0x60, ctl_filter(S + 0x5cac, dt, bike_x87_copyf(F32(S, 0x51d4))));
    SF32(CTL(S), 0x08, 0.0f);
    SF32(CTL(S), 0x0c, 0.0f);
    {
        float f_a = bike_param_curve(S + 0x452c, fwd), r_b = bike_param_curve(S + 0x4a24, fwd);
        float r_a = bike_param_curve(S + 0x4b64, fwd), f_b = bike_param_curve(S + 0x43ec, fwd);
        rx_st4(S + 0x8990, v4_splat(f_b));
        rx_st4(S + 0x8980, v4_splat(f_a));
        rx_st4(S + 0x8840, v4_splat(r_b));
        rx_st4(S + 0x8830, v4_splat(r_a));
    }
    bike_smooth_ground_direction(S, dt);
    ctl_steering_quaternion(S);
    {   /* lean target: (0, -1, 0, 0) (the .data -1.0 at 009cd9e8) + S+5540 */
        rx_v4 off = v4_movss(v4_shuf(v4_splat(F32(0, G_MINUS_ONE)), zero, 0xee), zero);
        rx_st4(S + 0x8d80, v4_add(off, rx_ld4(S + 0x5540)));
        rx_st4(S + 0x6060, rx_ld4(S + 0x8d80));
    }
}
RX_FUNCTION(0x0044e0d0, bike_controls_rider_off)

/* 0044c170 RavenSimulationMotocross::vf9 (ECX = S, stack dt; ret 4). */
void bike_controls_update(rx_addr S, float dt) {
    if (U32(S, 0x2374) != 0) { bike_controls_rider_off(S, dt); return; }
    const rx_addr entry_sp = rx_sp();                                      /* (the original's frame position, see below) */
    const rx_v4 G = rx_ld4(G_ZERO), N = rx_ld4(G_NUMERATOR), zero = v4_splat(0.0f);
    const float m1 = F32(0, G_MINUS_ONE);

    if (U32(S, 0x2eb0) != 0) {            /* manual gearbox: consume the shift pulses */
        rx_addr C = CTL(S);
        if (U32(C, 0x78) != 0) { int32_t g = I32(S, 0x8ac0) + 1; if (!(g < 4)) g = 4; SU32(S, 0x8ac0, g); SU32(C, 0x78, 0); }
        C = CTL(S);
        if (U32(C, 0x7c) != 0) { int32_t g = I32(S, 0x8ac0) - 1; if (g <= 0) g = 0; SU32(S, 0x8ac0, g); SU32(C, 0x7c, 0); }
    }
    rx_addr C = CTL(S);
    SF32(C, 0x68, bike_x87_copyf(F32(S, 0x2c)));   /* raw steering x / y, reflex x / y */
    SF32(C, 0x6c, bike_x87_copyf(F32(S, 0x30)));
    SF32(C, 0x70, bike_x87_copyf(F32(S, 0x34)));
    SF32(C, 0x74, bike_x87_copyf(F32(S, 0x38)));

    /* drag direction: against the velocity, scaled by a curve of the forward speed */
    const rx_v4 v = rx_ld4(S + 0x100), q = rx_ld4(S + 0x730);
    const float fwd = bike_rotate_inv(q, v).v[2];                       /* forward speed (root frame z) */
    const float speed = phys_sqrt(bike_hsum_021(v4_mul(v, v)));
    rx_st4(C + 0x20, v4_sub(zero, bike_normalize(v)));
    {
        float k = bike_param_curve(S + (U32(S, 0x8d98) != 0 ? 0x34ac : 0x340c), fwd);
        rx_st4(C + 0x20, v4_mul(v4_splat(k), rx_ld4(C + 0x20)));
    }

    /* rider lean offsets from the reflex inputs (dead zone 0.02) */
    float reflex_x = 0.0f, reflex_y = 0.0f;
    if (bike_fabs(F32(S, 0x34)) > 0.019999999552965164f /* 00977408 */) reflex_x = F32(S, 0x34);
    if (bike_fabs(F32(S, 0x38)) > 0.019999999552965164f) reflex_y = F32(S, 0x38);
    {
        float a = phys_mul(reflex_y, F32(S, 0x5594)), b = phys_mul(F32(S, 0x55dc), reflex_x);
        rx_st4(C + 0x30, v4_movss(v4_shuf(zero, v4_splat(a), 0xee), v4_splat(b)));
    }
    const float reflex_y_f = ctl_filter(S + 0x5c98, dt, reflex_y);

    /* throttle: none while braking or reversing; cut by the launch timer S+68 */
    C = CTL(S);
    if (F32(S, 0x28) > 0.0f || F32(S, 0x20) > 0.0f) SF32(C, 0x00, 0.0f);
    else SF32(C, 0x00, bike_x87_copyf(F32(S, 0x1c)));
    float cut = 0.0f;
    if (F32(S, 0x68) > 0.0f) {
        float t = (float)(((double)F32(S, 0x68) - 0.0010000000474974513) / 1.4989999999525025);     /* 00977b30 / 28 */
        float r = (float)((0.9800000190734863 - 0.0) * (double)t + 0.0);                            /* 00977b58 */
        if (r > 0.0f) { float k = F32(0, 0x009cd9bc); cut = k > r ? r : k; }
    }
    C = CTL(S);
    SF32(C, 0x00, phys_sub(F32(C, 0x00), phys_mul(F32(C, 0x00), cut)));
    SF32(CTL(S), 0x04, 0.0f);
    if (F32(S, 0x20) > 0.0f && 2.0f > fwd && !(F32(S, 0x28) > 0.009999999776482582f /* 009322ac */)) {
        SF32(CTL(S), 0x00, 0.0f);
        SF32(CTL(S), 0x04, bike_x87_copyf(F32(S, 0x20)));               /* reverse */
    }

    /* engine coupling mode C+84 and its timer S+8ab8 */
    C = CTL(S);
    SU32(C, 0x80, U32(S, 0x48));                                         /* clutch */
    int stopped = 0.5f > bike_fabs(fwd) && 0.0f > F32(C, 0x00) && 0.0f > F32(C, 0x04);
    if (U32(C, 0x80) == 0 && !stopped) {
        if (!(0.0f > F32(S, 0x8ab8)) && U32(S, 0x8890) != 0) {
            SU32(C, 0x84, 1); SF32(S, 0x8ab8, phys_sub(F32(S, 0x8ab8), dt));
        } else {
            SU32(C, 0x84, 0); SF32(S, 0x8ab8, F32(0, 0x009cd9dc));
        }
    } else {
        float x = phys_div(phys_sub(speed, 5.0f /* 00935890 */), 20.0f /* 00935aac */);
        SU32(C, 0x84, 2);
        if (x > 1.0f) x = 1.0f;
        SF32(S, 0x8ab8, phys_mul(F32(S, 0x52ac), x));
    }

    /* engine scale (filtered into C+60): reduced by steering, rear slip and rear sliding */
    float engine;
    if (U32(S, 0x8d98) != 0) engine = F32(S, 0x51d4);
    else {
        C = CTL(S);
        const float c74 = F32(C, 0x74), base = F32(S, 0x521c);
        float floor;
        if (0.0f > c74) { engine = phys_sub(base, phys_mul(phys_sub(F32(S, 0x51d4), base), c74)); floor = 0.5f; }
        else { engine = base; floor = 0.10000000149011612f; /* 00931d94 */ }
        float slip = 0.0f;
        if (U32(S, 0x8854) == 0 && U32(S, 0x8890) != 0) {
            float r = bike_fabs(F32(S, 0x8810));                         /* rear rolling speed */
            if (r > 1.0f) slip = bike_clamp01(phys_div(phys_sub(r, 1.0f), 25.0f /* 0092e134 */));
        }
        float st = bike_fabs(F32(C, 0x68));
        float s = slip > st ? slip : st;
        float f = phys_sub(1.0f, s);
        if (floor > f) f = floor;
        engine = phys_mul(f, engine);
        if (U32(S, 0x8890) != 0) {
            float g = phys_div(bike_fabs(F32(S, 0x8810 + 8)), 40.0f /* 009cc37c */);
            if (g > 1.0f) g = 1.0f;
            float h = phys_sub(1.0f, g);
            h = h >= 1.0f ? 1.0f : !(0.10000000149011612f >= h) ? h : 0.10000000149011612f;
            engine = phys_mul(h, engine);
        }
    }
    SF32(CTL(S), 0x60, ctl_filter(S + 0x5cac, dt, engine));
    SF32(CTL(S), 0x08, 0.0f);
    SF32(CTL(S), 0x0c, 0.0f);

    /* brakes: filtered on the way up, immediate on release */
    float brake_f = F32(S, 0x24), brake_r = F32(S, 0x28);
    if (U32(S, 0x40) == 0 && U32(S, 0x8d98) != 0) brake_f = 0.0f;
    float bf = ctl_filter(S + 0x5c34, dt, brake_f);
    float br = ctl_filter(S + 0x5c48, dt, brake_r);
    if (bf > brake_f) { ctl_filter_set(S + 0x5c34, brake_f, F32(S, 0x5cc4)); bf = brake_f; }
    if (br > brake_r) { ctl_filter_set(S + 0x5c48, brake_r, F32(S, 0x5cc4)); br = brake_r; }
    {   /* brake torques against the wheel's spin relative to the chassis (S+8a30 front, S+8a34 rear) */
        float sp = F32(S, 0x8a30);
        if (sp > 0.0f) SF32(CTL(S), 0x0c, phys_sub(-0.0f, phys_mul(bike_param_curve(S + 0x3b84, sp), phys_mul(F32(S, 0x3b3c), bf))));
        else SF32(CTL(S), 0x0c, phys_mul(bike_param_curve(S + 0x3b84, phys_sub(-0.0f, sp)), phys_mul(F32(S, 0x3b3c), bf)));
        C = CTL(S);
        SF32(C, 0x0c, phys_mul(phys_add(F32(S, 0x38), 1.0f), F32(C, 0x0c)));
        C = CTL(S);
        SF32(C, 0x0c, phys_mul(phys_sub(1.0f, bike_fabs(F32(S, 0x2c))), F32(C, 0x0c)));
        sp = F32(S, 0x8a34);
        if (sp > 0.0f) SF32(CTL(S), 0x08, phys_sub(-0.0f, phys_mul(bike_param_curve(S + 0x3c6c, sp), phys_mul(F32(S, 0x3c24), br))));
        else SF32(CTL(S), 0x08, phys_mul(bike_param_curve(S + 0x3c6c, phys_sub(-0.0f, sp)), phys_mul(F32(S, 0x3c24), br)));
    }

    /* steering angle: squared input, rate-limited (100 / s), filtered, scaled by a speed curve; reflex x adds to it */
    const float steer_k = bike_param_curve(S + 0x354c, fwd);
    float steer;
    {
        float x = F32(S, 0x2c);
        float sg = x >= 0.0f ? 1.0f : m1;
        float f = F32(S, 0x5c5c);
        float target = ctl_step_towards(f, phys_mul(phys_mul(x, sg), x), phys_mul(dt, 100.0f /* 0093005c */));
        steer = ctl_filter(S + 0x5c5c, dt, target);
        C = CTL(S);
        SF32(C, 0x10, phys_mul(steer, steer_k));
        float ry = F32(C, 0x70);
        if (phys_mul(ry, steer) > -0.05000000074505806f /* 009776ec */)
            SF32(C, 0x10, phys_add(phys_mul(phys_mul(steer_k, 0.20000000298023224f /* 0092e388 */), ry), F32(C, 0x10)));
        else
            SF32(C, 0x10, phys_sub(F32(C, 0x10), phys_mul(bike_fabs(ry), phys_mul(F32(C, 0x10), 0.20000000298023224f))));
    }

    /* steering correction target: heading of the horizontal velocity (forward), wobbles (backward, front wheel) */
    const rx_v4 q2 = rx_ld4(S + 0x730), vl = bike_rotate_inv(q2, rx_ld4(S + 0x100));
    rx_v4 zsrc = vl;
    int straight_up = 0;                                                  /* (its [esp+20]) */
    float corr = 0.0f;
    if (bike_all3(v4_cmplt(G, v4_lane(vl, 2)))) {
        const rx_v4 vh = v4_mul(v4_movss(v4_shuf(zero, v4_splat(1.0f), 0xee), v4_splat(1.0f)), vl);   /* (x, 0, z, w) */
        const float speed2 = phys_sqrt(bike_hsum_021(v4_mul(v, v)));
        const rx_v4 n = bike_normalize(vh);
        const rx_v4 c = v4_lane(v4_max(G, v4_splat(bike_dot3(n, rx_ld4(G_AXIS_FWD)))), 0);
        /* packed arccosine of c (the SSE twin of bike_asin_approx), signed by the turn direction */
        const rx_v4 one = rx_ld4(G_ONES);
        const rx_v4 s1 = v4_sub(one, v4_mul(c, c));
        const rx_v4 a1 = v4_max(v4_sub(zero, s1), s1), ac = v4_max(v4_sub(zero, c), c);
        const rx_v4 sq = v4_sqrt(a1);
        const rx_v4 hi = v4_cmple(v4_splat(0.7071059942245483f /* 00977780 */), ac);
        const rx_v4 y = v4_or(v4_and(sq, hi), v4_andn(hi, ac));
        const rx_v4 y2 = v4_mul(y, y), y4 = v4_mul(y2, y2);
        rx_v4 den = v4_mul(v4_add(v4_mul(y2, v4_splat(-0.04992850869894028f /* 00935bc0 */)), v4_splat(0.5648093223571777f /* 00935bb0 */)), y4);
        den = v4_add(den, v4_add(v4_mul(y2, v4_splat(-1.4430354833602905f /* 00935ba0 */)), one));
        const rx_v4 R = v4_div(v4_splat(1.0f /* 0092dba0 */), den);
        rx_v4 num = v4_mul(v4_add(v4_mul(y2, v4_splat(-0.0193784236907959f /* 00935b70 */)), v4_splat(0.3993034064769745f /* 00935b80 */)), y4);
        num = v4_mul(v4_add(num, v4_add(v4_mul(y2, v4_splat(-1.2763687372207642f /* 00935b90 */)), one)), y);
        const rx_v4 A = v4_mul(R, num);
        const rx_v4 halfpi = v4_splat(1.5707963705062866f /* 00935b00 */);
        const rx_v4 as = v4_or(v4_and(v4_sub(halfpi, A), hi), v4_andn(hi, A));
        const rx_v4 pos = v4_cmple(rx_ld4(G_ASIN_SPLIT), c);
        const rx_v4 asg = v4_or(v4_and(as, pos), v4_andn(pos, v4_sub(zero, as)));
        rx_v4 heading = v4_sub(halfpi, asg);
        if (bike_all3(v4_cmplt(v4_lane(bike_cross(rx_ld4(G_AXIS_FWD), n), 1), G))) heading = v4_sub(zero, heading);
        float h = phys_mul(bike_lerp_clamped(bike_clamp01(phys_div(phys_sub(speed2, 5.0f), 15.0f /* 009322d0 */)), 0.0f, 0.75f /* 00935aa8 */),
                           heading.v[0]);
        float rate = U32(S, 0x8d98) != 0 ? 20.0f : 5.0f;
        rate = phys_mul(phys_sub(1.0f, bike_fabs(F32(CTL(S), 0x68))), rate);
        float sg = h >= 0.0f ? 1.0f : m1;
        float a = bike_fabs(h), lim = phys_mul(rate, 0.01745329052209854f /* 00977678 */);
        corr = phys_mul(sg, lim > a ? a : lim);
        SF32(S, 0x8db0, 0.0f);
        if (U32(S, 0x8db8) != 0) {
            rx_v4 u = v4_lane(bike_rotate(q2, rx_ld4(G_AXIS_X)), 1);
            if (bike_all3(v4_cmplt(v4_max(v4_sub(zero, u), u), v4_splat(0.5f)))) straight_up = 1;
        }
        zsrc = vh;
    } else if (U32(S, 0x89e0) == 0 && U32(S, 0x8890) == 0) {
        SF32(S, 0x8db0, corr);
    } else {   /* rolling backwards: a wobble whose amplitude grows with the speed (15..45) */
        const rx_v4 z = v4_lane(vl, 2);
        rx_v4 amp = v4_div(v4_sub(v4_max(v4_sub(zero, z), z), v4_splat(15.0f)), v4_splat(30.0f /* 00931d98 */));
        amp = v4_max(G, v4_min(N, amp));
        const float phase = F32(S, 0x8db0);
        const float wob = phys_mul(bike_cos_poly(phys_add(phase, 1.5707963705062866f /* 00977508 */)), amp.v[0]);
        SF32(S, 0x8db0, ctl_fmod(phys_add(phys_mul(dt, 20.0f), phase), 6.2831854820251465f /* 0097749c */));
        const rx_v4 ax = v4_lane(vl, 0);
        const rx_v4 d = v4_sub(v4_sub(zero, z), v4_max(v4_sub(zero, ax), ax));
        if (bike_all3(v4_cmplt(v4_splat(30.0f), d))) SU32(S, 0x8db8, 1);
        else if (U32(S, 0x8db8) != 0 && bike_all3(v4_cmplt(d, v4_splat(15.0f)))) {
            rx_v4 u = v4_lane(bike_rotate(q2, rx_ld4(G_AXIS_X)), 1);
            if (bike_all3(v4_cmplt(v4_max(v4_sub(zero, u), u), v4_splat(0.5f)))) straight_up = 1;
        }
        corr = wob;
    }
    if (U32(S, 0x89e0) != 0 && F32(S, 0x89f4) > 0.25f /* 0092ff84 */) {
        /* front wheel loaded: a wobble of the steering (S+8db4 its phase), fading in below 10 and out above 10 .. 60 */
        const float load = F32(S, 0x89f4);
        const rx_v4 z = v4_lane(zsrc, 2), ten = v4_splat(10.0f /* 0092fd88 */);
        int fast;
        rx_v4 k;
        if (bike_all3(v4_cmplt(z, ten))) { k = v4_div(z, ten); fast = 0; }
        else { k = v4_sub(N, v4_div(v4_sub(z, zero), v4_splat(50.0f /* 009d78dc */))); fast = 1; }
        const float kk = v4_max(G, v4_min(N, k)).v[0];
        const float w = bike_cos_poly(phys_add(F32(S, 0x8db4), 1.5707963705062866f));
        corr = phys_mul(w, phys_mul(phys_mul(phys_sub(load, 0.25f), 0.22499999403953552f /* 00977b54 */), kk));
        float freq = 10.0f;
        if (fast) freq = bike_lerp_clamped(phys_sub(1.0f, kk), 10.0f, 20.0f);
        if (kk > 0.0f) SF32(S, 0x8db4, ctl_fmod(phys_add(phys_mul(freq, dt), F32(S, 0x8db4)), 6.2831854820251465f));
        else SF32(S, 0x8db4, 0.0f);
    } else SF32(S, 0x8db4, 0.0f);
    {   /* the correction, rate-limited (50 / s) and filtered, adds to the steering angle, clamped to +-30 degrees */
        float f = F32(S, 0x5c84);
        float target = ctl_step_towards(f, corr, phys_mul(dt, 50.0f));
        C = CTL(S);
        SF32(C, 0x10, phys_add(ctl_filter(S + 0x5c84, dt, target), F32(C, 0x10)));
        C = CTL(S);
        float s = F32(C, 0x10);
        s = s >= 0.5235987305641174f /* 00977a10 */ ? 0.5235987305641174f : !(-0.5235987305641174f >= s) ? s : -0.5235987305641174f;
        SF32(C, 0x10, s);
    }

    /* brakes eased while steering */
    const float steer_abs = bike_fabs(F32(CTL(S), 0x68));
    SF32(S, 0x24, bike_lerp_clamped(steer_abs, bike_x87_copyf(F32(S, 0x24)), phys_mul(F32(S, 0x24), 0.0f)));
    SF32(S, 0x28, bike_lerp_clamped(steer_abs, bike_x87_copyf(F32(S, 0x28)), bike_x87_copyf(F32(S, 0x28))));

    /* lean C+40: front lateral sliding, reflex x, reflex y */
    {
        float lean_in = 0.0f;
        if (U32(S, 0x89a4) == 0 && U32(S, 0x89e0) != 0) {
            float wx = F32(S, 0x8960);
            float a = bike_fabs(wx);
            if (a > 1.0f) {
                float sg = wx >= 0.0f ? 1.0f : m1;
                float y = phys_div(phys_sub(a, 1.0f), 10.0f);
                y = y >= 2.0f ? 2.0f : !(0.0f >= y) ? y : 0.0f;
                lean_in = phys_sub(-0.0f, phys_mul(y, sg));
            }
        }
        C = CTL(S);
        SF32(C, 0x40, phys_add(phys_mul(phys_sub(1.0f, bike_fabs(lean_in)), F32(C, 0x40)), lean_in));
        C = CTL(S);
        const float lean = F32(C, 0x40), rx = F32(C, 0x70);
        int push = bike_fabs(lean) > 0.05000000074505806f /* 009d3bfc */ ? phys_mul(rx, lean) > 0.0f
                                                                        : phys_mul(rx, steer) > -0.05000000074505806f;
        if (push) SF32(C, 0x40, phys_add(phys_mul(rx, 0.550000011920929f /* 009776f0 */), F32(C, 0x40)));
        C = CTL(S);
        float ry = F32(C, 0x74);
        if (0.0f > ry) ry = 0.0f;
        const float l = F32(C, 0x40);
        SF32(C, 0x40, bike_lerp_clamped(phys_mul(steer_abs, ry), bike_x87_copyf(l), phys_mul(l, 1.5f /* 009322a4 */)));
    }

    /* climbing fast (v.y > 15): look ahead along the horizontal velocity for terrain; none -> jump assist */
    {
        const rx_v4 vv = rx_ld4(S + 0x100);
        if (bike_all3(v4_cmplt(v4_splat(15.0f), v4_lane(vv, 1)))) {
            float vx = vv.v[0], vy = vv.v[1], vz = vv.v[2];
            float l2 = phys_add(phys_add(phys_mul(vx, vx), phys_mul(vy, vy)), phys_mul(vz, vz));
            float nx = vx, nz = vz;
            if (l2 > 0.0f) {
                float inv = phys_div(1.0f, phys_sqrt(l2));                 /* SqrtF 0042c230 */
                nx = phys_mul(vx, inv); nz = phys_mul(inv, vz);
            }
            const rx_v4 p = rx_ld4(S + 0x230);                            /* rider body position */
            float lift = (float)((double)(float)(((double)vy - 15.0) / 25.0) * 6.0 + 1.0);   /* 00977b20 / 18 / 68 */
            lift = lift > 1.0f ? (7.0f /* 00930004 */ > lift ? lift : 7.0f) : 1.0f;
            rx_v4 start = p, end = v4(phys_add(p.v[0], phys_mul(nx, 25.0f)), 0.0f, phys_add(p.v[2], phys_mul(nz, 25.0f)), 0.0f);
            start.v[1] = phys_add(lift, p.v[1]);
            end.v[1] = phys_add(lift, phys_add(p.v[1], phys_mul(0.0f, 25.0f)));
            const rx_addr world = ctl_current_world();
            if (ctl_terrain_segment(entry_sp, world, start, end) == 0) {
                SU32(S, 0x8c24, 1);
                C = CTL(S);
                SF32(C, 0x40, phys_add(phys_mul(F32(C, 0x70), 1.5f), F32(C, 0x40)));
                C = CTL(S);
                if (bike_fabs(F32(C, 0x70)) > 0.6000000238418579f /* 00931da8 */ && bike_fabs(F32(C, 0x68)) > 0.6000000238418579f)
                    SU32(S, 0x8c20, 1);
            } else { SU32(S, 0x8c24, 0); SU32(S, 0x8c20, 0); }
        } else { SU32(S, 0x8c24, 0); SU32(S, 0x8c20, 0); }
    }

    /* pitch target C+64: in the air the bike's roll angle, on the ground a curve of the speed times the lean */
    if (U32(S, 0x8d98) != 0) {
        float t = bike_dot3(bike_rotate(rx_ld4(S + 0x730), rx_ld4(G_AXIS_X)), rx_ld4(G_AXIS_UP));
        float a = bike_asin_approx(bike_fabs(t), t);
        SF32(CTL(S), 0x64, ctl_filter(S + 0x5c70, dt, t >= 0.0f ? a : phys_sub(-0.0f, a)));
    } else if (U32(S, 0x8db8) == 0) {
        float k = phys_mul(bike_param_curve(S + 0x367c, fwd), F32(CTL(S), 0x40));
        SF32(CTL(S), 0x64, ctl_filter(S + 0x5c70, dt, phys_sub(-0.0f, k)));
    } else if (straight_up) SU32(S, 0x8db8, 0);

    /* friction coefficients of the wheel records (front W+b0 / W+a0, rear W+b0 / W+a0) over speed and reflex inputs */
    {
        float f_b = bike_param_curve(S + 0x43ec, fwd), f_a = bike_param_curve(S + 0x452c, fwd);
        float r_b = bike_param_curve(S + 0x4a24, fwd), r_a = bike_param_curve(S + 0x4b64, fwd);
        float x = 0.0f > F32(CTL(S), 0x64) ? F32(S, 0x34) : phys_sub(-0.0f, F32(S, 0x34));
        f_a = phys_mul(bike_param_curve(S + 0x470c, x), f_a);
        r_a = phys_mul(bike_param_curve(S + 0x4d44, x), r_a);
        f_a = phys_mul(bike_param_curve(S + 0x484c, reflex_y_f), f_a);
        f_b = phys_mul(bike_param_curve(S + 0x47ac, reflex_y_f), f_b);
        r_a = phys_mul(bike_param_curve(S + 0x4e84, reflex_y_f), r_a);
        r_b = phys_mul(bike_param_curve(S + 0x4de4, reflex_y_f), r_b);
        if (U32(S, 0x8c20) != 0) { r_a = phys_mul(r_a, 0.4000000059604645f /* 00935860 */); f_a = phys_mul(f_a, F32(0, 0x009cc8a8)); }
        rx_st4(S + 0x8990, v4_splat(f_b));
        rx_st4(S + 0x8840, v4_splat(r_b));
        rx_st4(S + 0x8980, v4_splat(f_a));
        rx_st4(S + 0x8830, v4_splat(r_a));
    }
    if (U32(S, 0x2de0) != 0) SF32(CTL(S), 0x10, bike_x87_copyf(F32(S, 0x2e70)));   /* pose override steering */
    bike_smooth_ground_direction(S, dt);

    ctl_steering_quaternion(S);
    {   /* rider lean target S+8d80 (copied to the rider node S+6060) */
        float a = phys_mul(F32(S, 0x38), F32(S, 0x5594)), b = phys_mul(F32(S, 0x55dc), F32(S, 0x34));
        rx_v4 off = v4_movss(v4_shuf(zero, v4_splat(a), 0xee), v4_splat(b));
        rx_st4(S + 0x8d80, v4_add(off, rx_ld4(S + 0x5540)));
        rx_st4(S + 0x6060, rx_ld4(S + 0x8d80));
    }
}
RX_FUNCTION(0x0044c170, bike_controls_update)
