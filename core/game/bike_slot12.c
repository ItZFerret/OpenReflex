/* bike_slot12.c -- slot 12 of the motocross simulation, after velocity integration (native/game/notes/bike.md).
 *
 * 00443610 vf12: rider off -> only cap the root spin (0043f270); airborne -> the in-air control 0043f2e0; on the ground -> the
 * grounded correction 00441300, then it resets the in-air state (S+8b70.., S+8c60.., S+8cd0.., S+8d44..), maps the two
 * sticks onto squares (00440c40: steering into S+8bbc / 8bc0, reflex into S+8bc4 / 8bc8), the reflex stick's magnitude by
 * its direction S+8bcc, and the take-off reference axis S+8bf0. Last it copies the wheel anchor points S+8dc0 / 8dd0 to
 * S+8de0 / 8df0. This thread's x87 precision: 53 bits (CW 027f). */
#include "bike_slot12.h"
#include "bike_curve.h"
#include "phys_math.h"
#include "phys_mech.h"
#include "phys_misc.h"
#include "crt.h"

#define CTL(S)        U32(S, 0x5c30)
#define G_AXIS_X      0x00d615c0
#define G_AXIS_UP     0x00d616a0
#define G_ZERO        0x00d61670

/* MSVCR90 _CIatan2 (thunk 009009c0, the CRT port): x87 st(0) = x, st(1) = y -> st(0) = atan2(y, x); stored with fstp m32. */
static float s12_atan2(float y, float x) {
    return rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y), rx_f80_from_f32(x), CRT_CW_MAIN), 0);
}

/* 0043f270 Simulation_ClampRootAngularVelocity (stack S; ret 4): root angular velocity capped at length 10 (h021 length,
 * all lanes rescaled). Returns the mask lanes the original leaves in ECX (lane 1) / EDX (lane 2 + lane 1 + lane 0). */
void bike_clamp_root_spin(rx_addr S, uint32_t* lane1, uint32_t* sum) {
    const rx_v4 w = rx_ld4(S + 0x740);
    const rx_v4 len = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(w, w))));
    const rx_v4 big = v4_cmplt(v4_splat(10.0f /* 0092fd88 */), len);
    if (lane1) *lane1 = v4_lb(big, 1);
    if (sum) *sum = v4_lb(big, 2) + v4_lb(big, 1) + v4_lb(big, 0);
    if (bike_all3(big)) rx_st4(S + 0x740, v4_div(v4_mul(w, v4_splat(10.0f)), len));
}
static void s12_clamp_root_spin(rx_addr S) { bike_clamp_root_spin(S, NULL, NULL); }
RX_FUNCTION(0x0043f270, s12_clamp_root_spin)

/* 00440c40 Stick_CircleToSquare (stack x, y; EDI = &out_a, ESI = &out_b; ret 8): the round stick range onto a square. With
 * r = |(x, y)| and the angle a = atan2(y, x) in quarter sectors (pi/4 = 0.7853982):
 *   [-pi/4, pi/4): a_out = r, b_out = a / (pi/4) * r            [pi/4, 3pi/4): b_out = r, a_out = (pi/2 - a) r / (pi/4)
 *   [-3pi/4, -pi/4): b_out = -r, a_out = (a + pi/2) r / (pi/4)  else: a_out = -r, b_out = (+-pi - a) r / (pi/4)
 * Returns r (the original leaves its bits in ECX). */
float bike_stick_circle_to_square(float x, float y, rx_addr out_a, rx_addr out_b) {
    const float r = phys_sqrt(phys_add(phys_mul(x, x), phys_mul(y, y)));          /* SqrtF 0042c230 */
    const float a = s12_atan2(bike_x87_copyf(y), bike_x87_copyf(x));
    const float q = 0.7853981852531433f;                                          /* 009776e0 */
    if (a >= -0.7853981852531433f /* 009778a0 (fcomip) */ && q > a) {
        SF32(out_a, 0, r); SF32(out_b, 0, phys_mul(phys_div(a, q), r));
    } else if (a >= q && 2.356194496154785f /* 0097789c */ > a) {
        SF32(out_b, 0, r); SF32(out_a, 0, phys_div(phys_mul(phys_sub(1.5707963705062866f /* 00977508 */, a), r), q));
    } else if (!(a >= 2.356194496154785f) && !(-2.356194496154785f /* 00977898 */ > a)) {
        SF32(out_b, 0, phys_sub(-0.0f, r)); SF32(out_a, 0, phys_div(phys_mul(phys_add(a, 1.5707963705062866f), r), q));
    } else {
        SF32(out_a, 0, phys_sub(-0.0f, r));
        if (a > 0.0f) SF32(out_b, 0, phys_div(phys_mul(phys_sub(3.1415927410125732f /* 00977498 */, a), r), q));
        else SF32(out_b, 0, phys_div(phys_mul(phys_sub(-3.1415927410125732f /* 00977530 */, a), r), q));
    }
    return r;
}
static void s12_stick(float x, float y, rx_addr a, rx_addr b) { bike_stick_circle_to_square(x, y, a, b); }
RX_FUNCTION(0x00440c40, s12_stick)

/* ==== 00441300 the grounded correction (ESI = S, stack dt; ret 4) ==========================================================
 * On the ground, after the velocity integration. The wheel anchors (rear S+8970 -> S+8dc0, front S+8820 -> S+8dd0, world) give
 * the bike's axis dn (rear minus front, normalized).
 *  1. upright and the axis nearly level (|dn . up| <= S9AC): the lean c = S+8b50 . (body x axis) as an angle
 *     asin(c); either (S+8db8) it only goes to the controls' lean C+64 (and its filter S+5c70), or the bodies are turned about
 *     the axis through the front anchor by C+64 - asin(c) (S+ec bodies: positions and orientations) and the angular velocity
 *     loses its part along the axis (S+8b60 gets it);
 *     then the yaw / pitch damping by the contact state: both wheels down (S+8854, S+89a4) -> the sideways velocity removed;
 *     else (spin S+920.x not below -N) the angular velocity's part along S+88c0 / S+8a10 / up is kept by a factor f from the
 *     speed, the alignment s18 = v . dn and the steering;
 *     not upright or tilted: only C+64 = asin(c).
 *  2. the wheelie / stoppie limits: with only the front wheel down (S+8890, not S+89e0) the pitch rate S+740.x is scaled down
 *     by the nose angle (timer S+8da8, flag S+2eb8); with only the rear wheel down (S+89e0, not S+8890) by the wheelie angle
 *     (timer S+8dac, flag S+2eb4). */
#define K_9AC  F32(0, 0x009cd9ac)
#define K_958  F32(0, 0x009cd958)
#define K_970  F32(0, 0x009cd970)
#define K_9BC  F32(0, 0x009cd9bc)
#define K_894  F32(0, 0x009d7894)

static float g_signed_asin(float c) {                 /* asin(c) with the sign of c (comiss c, 0: negative -> -0 - a) */
    const float a = bike_asin_approx(bike_fabs(c), c);
    return c >= 0.0f ? a : phys_sub(-0.0f, a);
}
/* the bike's angle against the axis: pi/2 - (float)(pi/2 - (double)a), as the code rounds it */
static float g_angle_from(float a) { return phys_sub(1.5707963705062866f /* 00977508 */, (float)(1.5707963705062866 /* 00977510 */ - (double)a)); }
/* angular velocity damping about axis a (world): w_world - (w_world . a) a (1 - f), back in the body frame */
static rx_v4 g_damp_about(rx_v4 q, rx_v4 w, rx_v4 a, float f) {
    const rx_v4 ww = bike_rotate(q, w);
    const rx_v4 u = v4_xyz(v4_sub(ww, v4_mul(v4_mul(v4_splat(bike_dot3(ww, a)), a), v4_splat(phys_sub(1.0f, f)))));
    return bike_rotate_inv(q, u);
}

void bike_ground_correction(rx_addr S, float dt) {
    const rx_v4 G = rx_ld4(0x00d61670), up = rx_ld4(0x00d616a0), X = rx_ld4(0x00d615c0);
    const rx_addr C = U32(S, 0x5c30);
    const rx_v4 spin = rx_ld4(S + 0x2ec0);
    const int spinning = bike_all3(v4_cmplt(G, v4_sqrt(v4_splat(bike_hsum_021(v4_mul(spin, spin))))));
    rx_v4 q = rx_ld4(S + 0x730);
    const rx_v4 fwdX = bike_rotate(q, X);                                       /* the body x axis in the world */
    const rx_v4 upW = bike_rotate(q, up);
    const rx_v4 rear = v4_add(bike_rotate(q, rx_ld4(S + 0x8970)), rx_ld4(S + 0x230));
    const rx_v4 front = v4_add(bike_rotate(q, rx_ld4(S + 0x8820)), rx_ld4(S + 0x2d0));
    rx_st4(S + 0x8dc0, rear);
    rx_st4(S + 0x8dd0, front);
    const rx_v4 dn = bike_normalize(v4_sub(rear, front));
    const rx_v4 dn_body = bike_rotate_inv(q, dn);
    const rx_v4 w0 = rx_ld4(S + 0x740);
    const float align = bike_dot3(bike_normalize(rx_ld4(S + 0x100)), dn);       /* s18 */
    const float level = bike_dot3(dn, up);
    const rx_addr sp = rx_sp(), F = (sp - 0x40) & ~15u;                         /* locals passed by address */
    rx_set_sp(F);
    if (bike_all3(v4_cmple(G, v4_lane(upW, 1))) && bike_all3(v4_cmple(v4_splat(level), v4_splat(K_9AC))) &&
        bike_all3(v4_cmple(v4_sub(v4_splat(0.0f), v4_splat(K_9AC)), v4_splat(level)))) {
        /* ---- 1. lean */
        const float c = bike_dot3(rx_ld4(S + 0x8b50), fwdX);
        const float ang = g_signed_asin(c);
        if (U32(S, 0x8db8) != 0) {
            SF32(U32(S, 0x5c30), 0x64, ang);
            phys_lag_filter_set(S + 0x5c70, ang, rx_bf(0x7f7fffffu) /* 00977494 FLT_MAX: keep tau */);
        } else {
            /* turn all bodies about the bike axis through the front anchor by (C+64 - ang) */
            const rx_v4 axis = v4_or(v4_xyz(bike_normalize(dn_body)), rx_ld4(0x0092db60) /* (0, 0, 0, 1) */);
            phys_vec4_sincos(v4_splat(phys_mul(phys_sub(F32(C, 0x64), ang), 0.5f /* 009322cc */)), F + 0x10, F);
            const rx_v4 R = v4_mul(v4_or(v4_xyz(rx_ld4(F + 0x10)), v4_and(rx_ld4(0x0092db90), rx_ld4(F))), axis);
            for (int32_t i = 0; i < I32(S, 0xec); i++) {
                const rx_addr B = S + 0xf0 + 0xa0u * (uint32_t)i;
                const rx_v4 Q = rx_ld4(B + 0x640);
                const rx_v4 dl = v4_xyz(bike_rotate_inv(Q, v4_xyz(v4_sub(rx_ld4(B), front))));
                const rx_v4 dr = v4_xyz(bike_rotate(R, dl));
                rx_st4(B + 0x640, bike_quat_mul(Q, R));
                rx_st4(B, v4_add(bike_rotate(Q, dr), front));
            }
            const rx_v4 proj = v4_mul(v4_splat(bike_dot3(dn_body, w0)), dn_body);
            rx_st4(S + 0x740, v4_sub(w0, proj));
            rx_st4(S + 0x8b60, proj);
        }
        /* ---- yaw / pitch damping by the contact state */
        q = rx_ld4(S + 0x730);
        const rx_v4 fx = bike_rotate(q, X);
        const rx_v4 v = rx_ld4(S + 0x100);
        const float speed = phys_div(v4_sqrt(v4_splat(bike_hsum_021(v4_mul(v, v)))).v[0], 20.0f /* 00935aac */);
        const float k = speed > 1.0f ? 1.0f : speed;
        const uint32_t front_down = U32(S, 0x8854);
        if (front_down != 0 && U32(S, 0x89a4) != 0) {
            /* both wheels down: no sideways velocity */
            rx_st4(F, v4_sub(v, v4_mul(v4_splat(bike_dot3(fx, v)), fx)));
            phys_mech_set_node_linear_velocity(S + 0xd0, F, 0);
        } else if (!bike_all3(v4_cmplt(v4_lane(rx_ld4(S + 0x920), 0), v4_sub(v4_splat(0.0f), rx_ld4(0x00d61650))))) {
            const rx_v4 w = v4_xyz(rx_ld4(S + 0x740));
            rx_v4 wn;
            if (U32(S, 0x89a4) == 0 && U32(S, 0x8890) != 0 && front_down != 0) {
                const float hi = U32(S, 0x89e0) != 0 ? K_894 : K_9BC;
                const float f = align > 0.0f ? phys_add(phys_mul(phys_sub(1.0f, align), K_958), phys_mul(hi, align)) : K_958;
                wn = g_damp_about(q, w, rx_ld4(S + 0x88c0), f);
            } else if (U32(S, 0x89a4) == 0 && U32(S, 0x8890) != 0) {
                const float k0 = bike_lerp_clamped(k, 1.0f, K_970);
                float f = k0;
                if (align > 0.0f) f = phys_add(phys_mul(bike_lerp_clamped(k, 1.0f, K_894), align), phys_mul(phys_sub(1.0f, align), k0));
                wn = g_damp_about(q, w, rx_ld4(S + 0x8a10), f);
            } else {
                float hi, lo;
                if (U32(S, 0x89a4) != 0 && U32(S, 0x8890) != 0) {
                    hi = bike_lerp_clamped(k, 1.0f, K_894);
                    lo = bike_lerp_clamped(k, 1.0f, K_958);
                } else {                                          /* no front contact (S+8890 clear) */
                    const float steer = bike_fabs(F32(C, 0x68));
                    hi = bike_lerp_clamped(steer, bike_lerp_clamped(k, K_958, K_970), K_9AC);
                    lo = bike_lerp_clamped(steer, bike_lerp_clamped(k, K_958, K_958), K_9AC);
                }
                if (U32(S, 0x2eb4) != 0 && !spinning) { lo = 0.5f; hi = 0.5f; }   /* 009322cc */
                const float f = align > 0.0f ? phys_add(phys_mul(phys_sub(1.0f, align), lo), phys_mul(hi, align)) : lo;
                wn = g_damp_about(q, w, up, f);
            }
            rx_st4(F, wn);
            phys_mech_set_node_angular_velocity(S + 0xd0, F, 0);
        }
    } else {
        const float ang = g_signed_asin(bike_dot3(rx_ld4(S + 0x8b50), fwdX));
        SF32(U32(S, 0x5c30), 0x64, ang);
        phys_lag_filter_set(S + 0x5c70, ang, rx_bf(0x7f7fffffu));
    }
    rx_set_sp(sp);

    /* ---- 2. wheelie / stoppie limits */
    const rx_v4 v = rx_ld4(S + 0x100);
    const float speed2 = phys_div(v4_sqrt(v4_splat(bike_hsum_021(v4_mul(v, v)))).v[0], 60.0f /* 00931da0 */);
    const float k2 = speed2 > 1.0f ? 1.0f : speed2;
    const float t_nose = bike_clamp01(phys_mul(phys_sub(F32(S, 0x8da8), 1.0f), 0.5f));
    const float L1 = bike_lerp_clamped(k2, 0.6000000238418579f /* 00931da8 */, K_970);
    const float back = phys_sub(-0.0f, F32(S, 0x38));
    const float t_back = 0.0f > back ? 0.0f : back;
    const float L2 = bike_lerp_clamped(t_nose, K_958, 1.0f);
    const float L3 = bike_lerp_clamped(t_nose, K_9AC, 1.0f);
    const float L5 = bike_lerp_clamped(t_nose, bike_lerp_clamped(t_back, L1, 1.0f), 1.0f);
    const float r = phys_div(phys_sub(F32(S, 0x52ac), F32(S, 0x8ab8)), F32(S, 0x52ac));
    const float L6 = bike_lerp_clamped(bike_clamp01(r), 1.0f, L5);
    SU32(S, 0x2eb8, 0);
    if (U32(S, 0x8890) != 0 && U32(S, 0x89e0) == 0) {           /* front wheel only: nose down */
        const float b = g_angle_from(g_signed_asin(bike_dot3(dn, up)));
        const rx_v4 w = rx_ld4(S + 0x740);
        const int pitching = bike_all3(v4_cmplt(v4_lane(w, 0), G));
        float e = phys_div(bike_fabs(phys_sub(b, 0.43633225560188293f /* 00977a00 */)), 0.3490658104419708f /* 00977970 */);
        if (e > 1.0f) e = 1.0f;
        float f = 1.0f;
        if (pitching) {
            f = bike_lerp_clamped(e, L2, L6);
            if (b >= 0.7853980660438538f /* 009779f8 */) {
                float x = phys_div(phys_sub(0.8726645112037659f /* 00977978 */, b), 0.0872664526104927f /* 00977dd4 */);
                if (0.0f > x) x = 0.0f;
                f = bike_lerp_clamped(x, L3, f);
            }
        }
        float wx = phys_mul(f, w.v[0]);
        if (pitching) wx = wx > -1.5f ? wx : -1.5f;             /* maxps with 00977850 splat */
        rx_st4(S + 0x740, v4(wx, w.v[1], w.v[2], w.v[3]));
        SF32(S, 0x8da8, phys_add(F32(S, 0x8da8), dt));
        if (F32(S, 0x8da8) > 0.25f) SU32(S, 0x2eb8, 1);
    } else {
        SF32(S, 0x8da8, 0.0f);
    }
    const float t_wheelie = phys_sub(1.0f, bike_clamp01(phys_div(phys_sub(F32(S, 0x8dac), 1.0f), 3.0f /* 0092fe48 */)));
    const float steer = bike_fabs(F32(U32(S, 0x5c30), 0x68));
    const float M2 = bike_lerp_clamped(steer, bike_lerp_clamped(t_wheelie, 1.0f, F32(0, 0x009cc8a8)), 1.0f);
    const float M4 = bike_lerp_clamped(steer, bike_lerp_clamped(t_wheelie, 1.0f, 0.75f /* 00935aa8 */), 1.0f);
    SU32(S, 0x2eb4, 0);
    if (spinning) SF32(S, 0x8dac, 0.0f);
    if (U32(S, 0x89e0) != 0 && U32(S, 0x8890) == 0) {           /* rear wheel only: wheelie */
        const float b = phys_sub(-0.0f, g_angle_from(g_signed_asin(bike_dot3(dn, up))));
        const rx_v4 w = rx_ld4(S + 0x740);
        const int pitching = bike_all3(v4_cmplt(G, v4_lane(w, 0)));
        float e = phys_div(bike_fabs(phys_sub(b, 0.5235987305641174f /* 00977a10 */)), 0.43633225560188293f /* 00977a00 */);
        if (e > 1.0f) e = 1.0f;
        float f = 1.0f;
        if (pitching) {
            f = bike_lerp_clamped(e, M2, 1.0f);
            if (b >= 0.6544983983039856f /* 00977dd0 */) {
                float x = phys_div(phys_sub(0.6981316208839417f /* 00977a0c */, b), 0.04363322630524635f /* 009779f4 */);
                if (0.0f > x) x = 0.0f;
                f = bike_lerp_clamped(x, M4, f);
            }
        }
        rx_st4(S + 0x740, v4(phys_mul(f, w.v[0]), w.v[1], w.v[2], w.v[3]));
        SF32(S, 0x8dac, phys_add(F32(S, 0x8dac), dt));
        if (F32(S, 0x8dac) > 0.25f) SU32(S, 0x2eb4, 1);
    } else {
        SF32(S, 0x8dac, 0.0f);
    }
}
static void s12_ground_correction(rx_addr S, float dt) { bike_ground_correction(S, dt); }
RX_FUNCTION(0x00441300, s12_ground_correction)

/* 00443610 RavenSimulationMotocross::vf12 (ECX = S, stack dt; ret 4). */
void bike_slot12(rx_addr S, float dt) {
    rx_st4(S + 0x8b60, rx_ld4(G_ZERO));
    if (U32(S, 0x2374) != 0) { s12_clamp_root_spin(S); goto anchors; }
    if (U32(S, 0x8d98) != 0) {             /* airborne */
        SF32(S, 0x8b70, phys_add(F32(S, 0x8b70), dt));                   /* air time */
        bike_air_control(S, dt);
        goto anchors;
    }
    s12_ground_correction(S, dt);
    SF32(S, 0x8b70, 0.0f); SF32(S, 0x8b74, 0.0f); SF32(S, 0x8b78, 0.0f);
    SF32(S, 0x8b8c, 0.0f); SF32(S, 0x8ba0, 0.0f); SF32(S, 0x8bdc, 0.0f);
    {
        rx_addr C = CTL(S);
        float sx = bike_x87_copyf(F32(C, 0x68)), sy = bike_x87_copyf(F32(C, 0x6c));
        bike_stick_circle_to_square(sx, sy, S + 0x8bbc, S + 0x8bc0);
        C = CTL(S);
        float rx = bike_x87_copyf(F32(C, 0x70)), ry = bike_x87_copyf(F32(C, 0x74));
        bike_stick_circle_to_square(rx, ry, S + 0x8bc4, S + 0x8bc8);
    }
    {   /* the reflex stick: magnitude times a cosine-polynomial of 4x its direction, sign of x */
        const rx_addr C = CTL(S);
        const float rx = F32(C, 0x70), ry = F32(C, 0x74);
        const float dir = phys_mul(s12_atan2(ry, rx), 4.0f /* 009322c0 */);
        float a = bike_fabs(dir);
        a = phys_sub(a, phys_mul((float)rx_cvtt(phys_mul(a, 0.15915493667125702f)), 6.2831854820251465f));
        const float u = phys_sub(3.1415927410125732f, a), u2 = phys_mul(u, u), u4 = phys_mul(u2, u2);
        const float r = phys_sqrt(phys_add(phys_mul(ry, ry), phys_mul(rx, rx)));
        const float k6 = rx_bf(0xb46beed0u) /* 00935d50 */, k4 = rx_bf(0x37cb076bu) /* 00935d48 */, k3 = rx_bf(0x3d2aa372u) /* 00935d4c */;
        const float k2 = rx_bf(0xbab5a662u) /* 00935d44 */, k1 = rx_bf(0xbeffff3du) /* 00935d40 */, k0 = rx_bf(0x3f7ffff3u) /* 00935d3c */;
        const float A = phys_add(phys_mul(phys_add(phys_mul(phys_add(phys_mul(u2, k6), k4), u4), phys_add(phys_mul(u2, k2), k3)), u4),
                                 phys_add(phys_mul(u2, k1), k0));
        float m = phys_mul(r, phys_sub(1.0f, phys_sub(-0.0f, A)));
        SF32(S, 0x8bcc, m);
        if (0.0f > rx) SF32(S, 0x8bcc, phys_sub(-0.0f, m));
    }
    SF32(S, 0x8bd4, 1.0f); SF32(S, 0x8bd8, 1.0f);
    rx_st4(S + 0x8c00, rx_ld4(S + 0x730));                               /* take-off orientation */
    SU32(S, 0x8c60, 0); SU32(S, 0x8c64, 0); SU32(S, 0x8c68, 0);
    SU32(S, 0x8cd0, 0); SU32(S, 0x8cd4, 0); SU32(S, 0x8cd8, 0);
    rx_st4(S + 0x8c10, rx_ld4(S + 0x8c00));
    {   /* take-off reference axis: horizontal side axis of the velocity, or the bike's x axis when climbing steeply */
        const rx_v4 n = bike_normalize(rx_ld4(S + 0x100));
        const float ny2 = phys_mul(n.v[1], n.v[1]);
        const rx_v4 len = v4_sqrt(v4_splat(phys_add(phys_add(ny2, ny2), ny2)));
        if (bike_all3(v4_cmplt(len, v4_splat(F32(0, 0x009cd9ac))))) rx_st4(S + 0x8bf0, bike_normalize(bike_cross(rx_ld4(G_AXIS_UP), n)));
        else rx_st4(S + 0x8bf0, bike_rotate(rx_ld4(S + 0x8c00), rx_ld4(G_AXIS_X)));
    }
    SU32(S, 0x8d44, 0); SU32(S, 0x8d50, 600); SU32(S, 0x8d5c, 0); SU32(S, 0x8d6c, 600);
anchors:
    /* previous wheel anchor points (x87 float copies) */
    for (int i = 0; i < 3; i++) SF32(S, 0x8de0 + 4 * i, bike_x87_copyf(F32(S, 0x8dc0 + 4 * i)));
    for (int i = 0; i < 3; i++) SF32(S, 0x8df0 + 4 * i, bike_x87_copyf(F32(S, 0x8dd0 + 4 * i)));
}
RX_FUNCTION(0x00443610, bike_slot12)
