/* bike_air.c -- the motocross simulation's stunt state: trick latches and stick gestures (00440da0) and the in-air control
 * (0043f2e0, slot 12 airborne). This thread's x87 precision: 53 bits (CW 027f).
 *
 * The latch block S+8c60.. (six int32, three at 8c60, three at 8cd0) records which of six cone limits L (S+8c70, 8c90, 8cb0,
 * 8ce0, 8d00, 8d20; 0x20 bytes each: reference q, three limit angles) the bike's orientation q = S+730 has been inside of since
 * take-off, measured against the take-off orientation S+8c10 (or, while S+8c28 is set, against S+8c30 and not latched).
 * The gesture block S+8d48.. follows the rider's sticks on the ground: the previous stick values S+8d54 (y), S+8d74 (x),
 * S+8d70 (y), the strongest flick rates remapped to [0.005, 0.75] (S+8d4c) / [0.25, 1] (S+8d64 x, S+8d68 y), the side of the
 * last x flick S+8d78 (-1 right, +1 left), and two flags: S+8d48 "stick held back" (y < -0.9), S+8d60 "flick up and pull
 * back" (y < -0.4 after an x flick of strength >= 0.25). */
#include "bike_slot12.h"
#include "phys_quat.h"
#include "phys_misc.h"
#include "phys_math.h"
#include "phys_mech.h"
#include "bike_curve.h"
#include "crt.h"
#include "free_air.h"

#define CTL(S)   U32(S, 0x5c30)          /* controls; +70 / +74 the right stick x / y */

/* 0043f2e0's and the gestures' "stick moved away from rest faster than before": the rate (now - before) / dt when
 * |now| > |before| (else no rate) */
static int air_flick(float now, float before, float dt, float* rate) {
    if (!(bike_fabs(now) > bike_fabs(before))) return 0;
    *rate = phys_div(phys_sub(now, before), dt);
    return 1;
}
/* the running maximum m = max(m, r) as the original's comiss (an unordered m keeps r) */
static float air_max(float m, float r) { return m > r ? m : r; }

/* 00440da0 (EDI = S, stack dt; ret 4). Returns what the original leaves in EAX: the last latch written (0 / 1), or the
 * airborne flag S+8d98 when the latch block is skipped, with AH replaced by the flags of the "|y before| == 0" test
 * (lahf) when that test ran. */
uint32_t bike_stunt_latches(rx_addr S, float dt) {
    static const uint32_t latch[6] = { 0x8c60, 0x8c64, 0x8c68, 0x8cd0, 0x8cd4, 0x8cd8 };
    static const uint32_t cone[6]  = { 0x8c70, 0x8c90, 0x8cb0, 0x8ce0, 0x8d00, 0x8d20 };
    uint32_t eax = U32(S, 0x8d98);
    if (U32(S, 0x8d98) != 0 && U32(S, 0x8d44) == 0 && U32(S, 0x8d5c) == 0) {
        RX_FRAME_ENTER(Fr, 0x20);
        const rx_addr q = (Fr + 15) & ~15u;                       /* the orientation, copied to a local */
        rx_st4(q, rx_ld4(S + 0x730));
        for (int i = 0; i < 6; i++) {
            if (U32(S, 0x8c28) != 0) eax = (uint32_t)phys_quat_within_cone_limits(S + cone[i], q, S + 0x8c30);
            else eax = U32(S, latch[i]) != 0 ? 1u : (uint32_t)phys_quat_within_cone_limits(S + cone[i], q, S + 0x8c10);
            SU32(S, latch[i], eax);
        }
        RX_FRAME_LEAVE(Fr, 0x20);
    }
    if (U32(S, 0x8c24) == 0 || U32(S, 0x3c) == 0) {
        if (U32(S, 0x8d98) == 0) {                                /* gestures off: reset */
            SF32(S, 0x8d54, 0.0f); SF32(S, 0x8d4c, 0.0f); SF32(S, 0x8d74, 0.0f);
            SF32(S, 0x8d70, 0.0f); SF32(S, 0x8d64, 0.0f); SF32(S, 0x8d68, 0.0f);
            SU32(S, 0x8d48, 0); SU32(S, 0x8d60, 0);
            SF32(S, 0x8d78, 1.0f);
        }
        return eax;
    }
    if (U32(S, 0x8d98) != 0) return eax;
    if (U32(S, 0x8d60) == 0 && U32(S, 0x8d5c) == 0) {
        /* stick y: the strongest pull rate while it moves away from rest */
        const rx_addr C = CTL(S);
        const float y = F32(C, 0x74), ay = bike_fabs(y);
        int have = 0;
        float rate = 0.0f;
        if (ay > F32(0, 0x009cd970) /* 0.9 */) {
            const float b = bike_fabs(F32(S, 0x8d54));
            const int zf = b == 0.0f || b != b, pf = b != b, cf = b < 0.0f || b != b;
            eax = (eax & 0xffff00ffu) | (uint32_t)(0x02 | zf << 6 | pf << 2 | cf) << 8;
            if (b == 0.0f) have = 1;                               /* rate 0 from rest */
        }
        if (!have) have = air_flick(y, F32(S, 0x8d54), dt, &rate);
        if (have) {
            const float r = phys_remap_clamped(10.0f /* 0092fd88 */, F32(0, 0x009cc37c) /* 40 */, bike_fabs(rate),
                                               0.004999999888241291f /* 00977b48 */, 0.75f /* 00935aa8 */);
            /* 00424370 UCOMISD(lo,hi); LAHF: 10 < 40 leaves AH=03.
             * EAX is otherwise unchanged by the remap helper. */
            eax = (eax & 0xffff00ffu) | 0x0300u;
            SF32(S, 0x8d4c, air_max(F32(S, 0x8d4c), r));
        }
        SF32(S, 0x8d54, bike_x87_copyf(F32(C, 0x74)));
        if (-0.8999999761581421f /* 00977744 */ > F32(C, 0x74)) SU32(S, 0x8d48, 1);
        else { SU32(S, 0x8d48, 0); SF32(S, 0x8d4c, 0.0f); }
    }
    if (U32(S, 0x8d48) != 0 || U32(S, 0x8d44) != 0) return eax;
    const rx_addr C = CTL(S);
    float rate;
    if (air_flick(F32(C, 0x70), F32(S, 0x8d74), dt, &rate)) {     /* stick x flick: its side and strength */
        const float side = rate >= 0.0f ? 1.0f /* 00931f24 */ : F32(0, 0x009cd9e8) /* -1 */;
        SF32(S, 0x8d78, phys_sub(-0.0f /* 009774a8 */, side));
        const float r = phys_remap_clamped(100.0f /* 0093005c */, 250.0f /* 00977394 */, bike_fabs(rate), 0.25f /* 0092ff84 */, 1.0f);
        eax = (eax & 0xffff00ffu) | 0x0300u; /* remap LAHF: 100 < 250 */
        SF32(S, 0x8d64, air_max(F32(S, 0x8d64), r));
    }
    SF32(S, 0x8d74, bike_x87_copyf(F32(C, 0x70)));
    if (air_flick(F32(C, 0x74), F32(S, 0x8d70), dt, &rate)) {     /* stick y flick */
        const float r = phys_remap_clamped(100.0f, 250.0f, bike_fabs(rate), 0.25f, 1.0f);
        eax = (eax & 0xffff00ffu) | 0x0300u; /* remap LAHF: 100 < 250 */
        SF32(S, 0x8d68, air_max(F32(S, 0x8d68), r));
    }
    SF32(S, 0x8d70, bike_x87_copyf(F32(C, 0x74)));
    if (-0.4000000059604645f /* 0097773c */ > F32(C, 0x74) && F32(S, 0x8d64) >= 0.25f /* 0092ff84 */) SU32(S, 0x8d60, 1);
    return eax;
}
static void air_stunt_latches(rx_addr S, float dt) { bike_stunt_latches(S, dt); }
RX_FUNCTION(0x00440da0, air_stunt_latches)

/* ==== 0043f2e0 the in-air control (stack S, dt; ret 8) ======================================================================
 * Only while airborne (slot 12). In order:
 *  1. lean push: every body's velocity (S+100 + 0xa0 i, S+ec bodies) += normalize(up x v_root) * (steer x C+68 * S+5624) * S+120 * dt;
 *  2. the gesture moves started on the ground (00440da0): "pull back" (S+8d40 armed by S+8d48, S+8d50 frames, S+8d44 running)
 *     spins about the take-off axis S+8bf0 by -5 deg * S+8d4c, decaying to 0.3; "flick" (S+8d58 armed by S+8d60, S+8d6c frames,
 *     S+8d5c running) spins about S+8bf0 by -10 deg * S+8d68 plus a twist S+8d64 * S+8d78 about forward / up, decaying to 0.15;
 *  3. the take-off assist fades S+8bd4 / S+8bd8 (1 at take-off) run down while the sticks are held (and stay at 1 under S+8c2c);
 *  4. both sticks mapped onto squares (00440c40) and their rates (S+8f80.., shaped by the curves S+59ec / 5a8c / 5b2c / 5bcc),
 *     the reflex magnitude S+8bcc and its rate S+8bd0; the take-off orientation S+8c10 re-aimed along the velocity;
 *  5. with a stick held (and no gesture, S+3c clear): the angular velocity gets the mixed stick input (three lag filters
 *     S+8b78 roll / S+8b8c pitch / S+8ba0 yaw, weights faded by the assists); without it the bike is pulled back toward the
 *     take-off orientation (slerp 0.008 per step after 0.25 s of air, or the stronger S+8c10 hold once a trick latch is set);
 *  6. the angular velocity S+740 capped at 10 and scaled per axis by S+5680 (times S+56e0 under S+8d9c).
 * Returns the mask lane the original leaves in EDX (the cap test, lane 1); EAX = 3 times it. */
static float air_rate(float now, float before, float dt) {
    float r;
    return air_flick(now, before, dt, &r) ? r : 0.0f;
}
/* a stick rate shaped by its response curve: rate * curve(min(|rate| / 100, 1)) */
static float air_shaped(rx_addr curve, float rate) {
    float x = phys_div(bike_fabs(rate), 100.0f /* 0093005c */);
    if (x > 1.0f) x = 1.0f;
    return phys_mul(bike_param_curve(curve, x), rate);
}
static int air_stick_held(float v) { return bike_fabs(v) > F32(0, 0x009d3bfc) /* 0.05 */; }

uint32_t bike_air_control(rx_addr S, float dt) {
    const float deg = 0.01745329052209854f;                                   /* 00977678 */
    const rx_v4 up = rx_ld4(0x00d616a0), fwd = rx_ld4(0x00d61680);
    rx_st4(S + 0x8e70, rx_ld4(S + 0x780));

    /* 1. lean push */
    {
        const rx_v4 dir = bike_normalize(bike_cross(up, rx_ld4(S + 0x100)));
        const float k = phys_mul(F32(U32(S, 0x5c30), 0x68), F32(S, 0x5624));
        const rx_v4 push = v4_mul(v4_mul(v4_mul(dir, v4_splat(k)), rx_ld4(S + 0x120)), v4_splat(dt));
        for (int32_t i = 0; i < I32(S, 0xec); i++) rx_st4(S + 0x100 + 0xa0u * (uint32_t)i, v4_add(push, rx_ld4(S + 0x100 + 0xa0u * (uint32_t)i)));
    }

    /* the stick mixing weights, faded by the assists a, b (as the original computes them: (to - from) * fade + from) */
    const float a = F32(S, 0x8bd4), b = F32(S, 0x8bd8), z = 0.0f;
    const float wA  = phys_add(phys_mul(phys_sub(z, 1.0f), a), 1.0f);
    const float wB  = phys_sub(phys_mul(phys_sub(z, -1.2000000476837158f /* 00977888 */), a), 1.2000000476837158f /* 00930064 */);
    const float Za  = phys_add(phys_mul(phys_sub(z, z), a), z);
    const float wC  = phys_add(phys_mul(phys_sub(z, 0.75f), a), 0.75f);
    const float wD  = phys_add(phys_mul(phys_sub(-1.5f /* 00977850 */, z), b), z);
    const float wE  = phys_sub(-1.0800000429153442f /* 00977d30 */, phys_mul(b, 0.12000000476837158f /* 00977d2c */));
    const float wF  = phys_add(phys_mul(b, 0.06000000238418579f /* 00977cd0 */), 0.5400000214576721f /* 00977430 */);
    const float wG  = phys_add(phys_mul(phys_sub(1.5f /* 009322a4 */, z), b), z);
    const float wH  = phys_add(phys_mul(b, 1.5f), 0.5f /* 009322cc */);
    const float wI  = phys_add(phys_mul(phys_sub(-0.15000000596046448f /* 00977894 */, z), a), z);
    const float wJ  = phys_add(phys_mul(phys_sub(0.5f, z), a), z);
    const float wK  = phys_add(phys_mul(phys_sub(0.10000000149011612f /* 00931d94 */, z), b), z);
    const float Zb  = phys_add(phys_mul(phys_sub(z, z), b), z);
    rx_v4 q = rx_ld4(S + 0x730), w = rx_ld4(S + 0x740);

    /* 2. gesture moves */
    SU32(S, 0x8d40, U32(S, 0x8d48) != 0 && U32(S, 0x8d60) == 0 ? 1u : 0u);
    SU32(S, 0x8d58, U32(S, 0x8d48) == 0 && U32(S, 0x8d60) != 0 ? 1u : 0u);
    if (U32(S, 0x8d44) != 0 && I32(S, 0x8d50) > 0) SU32(S, 0x8d50, I32(S, 0x8d50) - 1);
    if (U32(S, 0x8d5c) != 0 && I32(S, 0x8d6c) > 0) SU32(S, 0x8d6c, I32(S, 0x8d6c) - 1);
    if (U32(S, 0x8d40) != 0 && I32(S, 0x8d50) > 0) {
        const float s = F32(S, 0x8d4c);
        const rx_v4 axis = bike_rotate_inv(q, rx_ld4(S + 0x8bf0));
        SU32(S, 0x8d44, 1);
        w = v4_add(v4_mul(axis, v4_splat(phys_mul(phys_mul(s, -5.0f /* 0097739c */), deg))), w);
        const float decay = phys_mul(dt, F32(U32(S, 0x5c30), 0x74) > F32(0, 0x009cd970) ? F32(0, 0x009cd960) : 0.10000000149011612f);
        const float v = phys_sub(s, decay);
        SF32(S, 0x8d4c, v > 0.30000001192092896f /* 00931da4 */ ? v : 0.30000001192092896f);
    } else {
        SU32(S, 0x8d48, 0); SU32(S, 0x8d40, 0); SU32(S, 0x8d44, 0);
    }
    if (U32(S, 0x8d58) != 0 && I32(S, 0x8d6c) > 0) {
        const float s = F32(S, 0x8d68), t = F32(S, 0x8d64), side = F32(S, 0x8d78);
        const rx_v4 axis = bike_rotate_inv(q, rx_ld4(S + 0x8bf0));
        rx_v4 dw = v4_mul(axis, v4_splat(phys_mul(phys_mul(s, F32(0, 0x009d78b8) /* -10 */), deg)));
        dw = v4_add(dw, v4_mul(v4_splat(phys_mul(phys_mul(t, side), deg)), fwd));
        dw = v4_add(dw, v4_mul(v4_splat(phys_mul(phys_mul(phys_mul(t, -0.5f /* 009774b0 */), side), deg)), up));
        SU32(S, 0x8d5c, 1);
        w = v4_add(dw, w);
        const float decay = phys_mul(dt, 0.10000000149011612f), lo = 0.15000000596046448f;   /* 009322c8 */
        const float t2 = phys_sub(t, decay), s2 = phys_sub(s, decay);
        SF32(S, 0x8d64, t2 > lo ? t2 : lo);
        SF32(S, 0x8d68, s2 > lo ? s2 : lo);
    } else {
        SU32(S, 0x8d60, 0); SU32(S, 0x8d58, 0); SU32(S, 0x8d5c, 0);
    }

    /* trick hold: a latched pitch trick (S+8c60..) with the stick pushed forward, or a latched roll trick (S+8cd0..) with it
     * pulled back */
    const rx_addr C = U32(S, 0x5c30);
    const int pitch_trick = U32(S, 0x8c60) || U32(S, 0x8c64) || U32(S, 0x8c68);
    const int roll_trick = U32(S, 0x8cd0) || U32(S, 0x8cd4) || U32(S, 0x8cd8);
    /* PORT ADDITION (free_air.h): FREER AIR leaves out the trick hold / the auto-level for a local player's bike; 0 = the original */
    const uint32_t free_air = rx_fa_for(S);
    const int hold = !(free_air & RX_FREE_AIR_NO_HOLD) &&
                     ((pitch_trick && -0.8999999761581421f /* 00977744 */ > F32(C, 0x68)) || (roll_trick && F32(C, 0x68) > F32(0, 0x009cd970)));
    const int any_stick = air_stick_held(F32(C, 0x68)) || air_stick_held(F32(C, 0x6c)) || air_stick_held(F32(C, 0x70)) || air_stick_held(F32(C, 0x74));
    const int active = any_stick && U32(S, 0x3c) == 0 && !hold && U32(S, 0x8d44) == 0;

    /* 3. assist fades */
    {
        const float r1 = phys_mul(dt, air_stick_held(F32(C, 0x68)) || air_stick_held(F32(C, 0x6c)) ? 2.0f /* 0092e330 */ : F32(0, 0x009d789c));
        const float v1 = phys_sub(a, r1);
        SF32(S, 0x8bd4, v1 > 0.0f ? v1 : 0.0f);
        const float r2 = phys_mul(dt, air_stick_held(F32(C, 0x70)) || air_stick_held(F32(C, 0x74)) ? 1.5f : F32(0, 0x009d789c));
        const float v2 = phys_sub(b, r2);
        SF32(S, 0x8bd8, v2 > 0.0f ? v2 : 0.0f);
        if (U32(S, 0x8c28) != 0) {
            if (U32(S, 0x8c2c) != 0) { SF32(S, 0x8bd8, 1.0f); SF32(S, 0x8bd4, 1.0f); }
            if (active) SU32(S, 0x8c2c, 0);
        }
    }

    /* 4. sticks */
    const rx_addr sp = rx_sp(), F = (sp - 0x80) & ~15u;     /* locals passed by address: stick outputs, rotation */
    rx_set_sp(F);
    bike_stick_circle_to_square(bike_x87_copyf(F32(U32(S, 0x5c30), 0x68)), bike_x87_copyf(F32(U32(S, 0x5c30), 0x6c)), F + 0x00, F + 0x04);
    bike_stick_circle_to_square(bike_x87_copyf(F32(U32(S, 0x5c30), 0x70)), bike_x87_copyf(F32(U32(S, 0x5c30), 0x74)), F + 0x08, F + 0x0c);
    const float A1 = F32(F, 0x00), B1 = F32(F, 0x04), A2 = F32(F, 0x08), B2 = F32(F, 0x0c);
    float RA1 = 0.0f, RB1 = 0.0f, RA2 = 0.0f, RB2 = 0.0f;
    if (U32(S, 0x8da4) != 0) {
        RA1 = air_rate(A1, F32(S, 0x8bbc), dt); RB1 = air_rate(B1, F32(S, 0x8bc0), dt);
        RA2 = air_rate(A2, F32(S, 0x8bc4), dt); RB2 = air_rate(B2, F32(S, 0x8bc8), dt);
    }
    SF32(S, 0x8f84, RB1); SF32(S, 0x8f80, RA1); SF32(S, 0x8f88, RA2); SF32(S, 0x8f8c, RB2);
    RA1 = air_shaped(S + 0x59ec, RA1); RB1 = air_shaped(S + 0x5a8c, RB1);
    RA2 = air_shaped(S + 0x5b2c, RA2); RB2 = air_shaped(S + 0x5bcc, RB2);
    SF32(S, 0x8bbc, A1); SF32(S, 0x8bc0, B1); SF32(S, 0x8bc4, A2); SF32(S, 0x8bc8, B2);

    /* the reflex stick's magnitude by its direction (as slot 12) and its rate */
    float m, mr;
    {
        const float rx = F32(U32(S, 0x5c30), 0x70), ry = F32(U32(S, 0x5c30), 0x74);
        const float ang = rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(ry), rx_f80_from_f32(rx), CRT_CW_MAIN), 0);
        const float A = bike_cos_poly2(phys_mul(ang, 4.0f /* 009322c0, x87: exact */));
        const float r = phys_sqrt(phys_add(phys_mul(ry, ry), phys_mul(rx, rx)));              /* SqrtF 0042c230 */
        m = phys_mul(r, phys_sub(1.0f, phys_sub(-0.0f, A)));
        if (0.0f > rx) m = phys_sub(-0.0f, m);
        mr = air_rate(m, F32(S, 0x8bcc), dt);
        SF32(S, 0x8bcc, m); SF32(S, 0x8bd0, mr);
    }

    /* the take-off orientation re-aimed: rows (u x n, n x side, n) with n the flight direction, unless it is near vertical */
    {
        const rx_v4 n = bike_normalize(rx_ld4(S + 0x100));
        const float ny2 = phys_mul(n.v[1], n.v[1]);
        const rx_v4 len = v4_sqrt(v4_splat(phys_add(phys_add(ny2, ny2), ny2)));
        if (bike_all3(v4_cmplt(len, v4_splat(F32(0, 0x009d7894) /* 0.99 */)))) {
            const rx_v4 side = bike_normalize(bike_cross(up, n));
            const rx_v4 u = bike_normalize(bike_cross(n, side));
            float f[3];
            bike_vec3_cross(f, u.v, n.v);                                                        /* 00401630: u x n */
            const float l2 = phys_add(phys_add(phys_mul(f[1], f[1]), phys_mul(f[0], f[0])), phys_mul(f[2], f[2]));
            if (l2 > 0.0f) {
                const float inv = phys_div(1.0f, phys_sqrt(l2));
                f[0] = phys_mul(inv, f[0]); f[1] = phys_mul(inv, f[1]); f[2] = phys_mul(inv, f[2]);
            }
            const float M[3][3] = {{f[0], f[1], f[2]}, {u.v[0], u.v[1], u.v[2]}, {n.v[0], n.v[1], n.v[2]}};
            rx_st4(S + 0x8c10, bike_matrix_to_quat(M));
        }
    }

    /* 5. stick control or the pull back to the take-off orientation */
    if (active) {
        SF32(S, 0x8b74, phys_add(dt, F32(S, 0x8b74)));
        float X0 = phys_add(phys_mul(B2, Zb), phys_mul(A2, wE));                                 /* roll */
        X0 = phys_add(X0, phys_mul(A1, wB)); X0 = phys_add(X0, phys_mul(B1, Za));
        X0 = phys_add(X0, phys_mul(RA2, wD)); X0 = phys_add(X0, phys_mul(RB2, Zb));
        X0 = phys_add(X0, phys_mul(RA1, wI)); X0 = phys_add(X0, phys_mul(RB1, Za));
        float X1 = phys_add(phys_mul(B1, wC), phys_mul(A1, Za));                                 /* pitch, rider stick */
        X1 = phys_add(X1, phys_mul(RA1, Za)); X1 = phys_add(X1, phys_mul(RB1, Za));
        float X2 = phys_add(phys_mul(B2, wH), phys_mul(A2, Zb));                                 /* pitch, reflex stick */
        X2 = phys_add(X2, phys_mul(RA2, Zb)); X2 = phys_add(X2, phys_mul(RB2, wG));
        const float k = F32(0, 0x009cc8a8);                                                      /* 0.8 nose-down */
        if (X1 > 0.0f) X1 = phys_mul(X1, k);
        if (X2 > 0.0f) X2 = phys_mul(X2, k);
        float X3 = phys_add(phys_mul(B2, Zb), phys_mul(A2, wF));                                 /* yaw */
        X3 = phys_add(X3, phys_mul(A1, wA)); X3 = phys_add(X3, phys_mul(B1, Za));
        X3 = phys_add(X3, phys_mul(RA1, wJ)); X3 = phys_add(X3, phys_mul(RB1, Za));
        X3 = phys_add(X3, phys_add(phys_mul(mr, wK), phys_mul(m, Zb)));
        const float Y = bike_fabs(X1) > bike_fabs(X2) ? X1 : X2;
        float roll, pitch, yaw;
        if (free_air & RX_FA_LOOSE) {                                    /* AIR LOOSENESS (free_air.h): quicker lag filters */
            roll = rx_fa_lag_update(S + 0x8b78, dt, bike_x87_copyf(X0));
            pitch = rx_fa_lag_update(S + 0x8b8c, dt, bike_x87_copyf(Y));
            yaw = rx_fa_lag_update(S + 0x8ba0, dt, bike_x87_copyf(X3));
        } else {
            roll = phys_lag_filter_update(S + 0x8b78, dt, bike_x87_copyf(X0));
            pitch = phys_lag_filter_update(S + 0x8b8c, dt, bike_x87_copyf(Y));
            yaw = phys_lag_filter_update(S + 0x8ba0, dt, bike_x87_copyf(X3));
        }
        const rx_v4 axis = bike_rotate_inv(q, rx_ld4(S + 0x8bf0));
        rx_v4 dw = v4_mul(axis, v4_splat(phys_mul(pitch, deg)));
        dw = v4_add(dw, v4_mul(v4_splat(phys_mul(roll, deg)), fwd));
        dw = v4_add(dw, v4_mul(v4_splat(phys_mul(yaw, deg)), up));
        if (free_air & RX_FA_LOOSE) dw = v4_mul(dw, v4_splat(rx_fa_loose_gain()));   /* AIR LOOSENESS: stick authority */
        w = v4_add(dw, w);
    } else {
        const int settle = F32(S, 0x8b74) > 0.25f /* 0092ff84 */ && U32(S, 0x8d5c) == 0 && U32(S, 0x8d44) == 0 &&
                           !(free_air & RX_FREE_AIR_NO_SETTLE);                              /* FREER AIR */
        if ((settle && !hold) || hold) {
            float t = settle && !hold ? 0.00800000037997961f /* 0097736c */ : 0.012000000104308128f /* 00977418 */;
            if (settle && !hold ? (free_air & RX_FA_PART_SETTLE) : (free_air & RX_FA_PART_HOLD))   /* AUTO LEVEL / COUNTER-STEER SNAP below 100 % (free_air.h) */
                t = settle && !hold ? rx_fa_settle_rate(t) : rx_fa_hold_rate(t);
            const rx_v4 rel = bike_qnormalize(bike_quat_mul(bike_quat_conj(q), rx_ld4(S + 0x8c10)));
            q = bike_qnormalize(phys_quat_slerp(rx_ld4(0x00d61640), rel, v4_splat(t)));
            rx_st4(F + 0x10, q);
            phys_mech_rotate(S + 0xd0, F + 0x10);                                                /* 0085c530 */
        }
    }
    rx_set_sp(sp);

    /* 6. cap and per-axis scale */
    const rx_v4 len = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(w, w))));
    const rx_v4 big = v4_cmplt(v4_splat(10.0f /* 0092fd88 */), len);
    if (bike_all3(big)) w = v4_div(v4_mul(w, v4_splat(10.0f)), len);
    rx_v4 s = rx_ld4(S + 0x5680);
    if (free_air & RX_FA_LOOSE) {                                        /* AIR LOOSENESS: the damping eased (free_air.h) */
        rx_v4 t = rx_ld4(S + 0x56e0);
        /* the coast after stick use only (S+8b74, the stick time of this jump): a hands-off jump keeps the original's damping */
        const int coast = !active && F32(S, 0x8b74) > 0.0f;
        for (int i = 0; i < 3; i++) { if (coast) s.v[i] = rx_fa_loose_damping(s.v[i]); t.v[i] = rx_fa_loose_takeoff(t.v[i]); }
        if (U32(S, 0x8d9c) != 0) s = v4_mul(t, s);
    } else if (U32(S, 0x8d9c) != 0) s = v4_mul(rx_ld4(S + 0x56e0), s);
    rx_st4(S + 0x740, v4_mul(v4_perm(s, 1, 2, 0, 3), w));
    return v4_lb(big, 1);
}
static void air_control(rx_addr S, float dt) { bike_air_control(S, dt); }
RX_FUNCTION(0x0043f2e0, air_control)
