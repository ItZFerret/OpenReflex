/* bike_tire.c -- the tire / contact force of one wheel: 00447f10 BikeTire_ContactForce (_re/notes/bike-tire-contact.md).
 *
 * Called by slot 11 (0043e100) for the rear wheel record W = S+8790 (body 3), then the front W = S+88e0 (body 2). The contact
 * geometry was found by slot 13 of the previous step; this function turns it into forces:
 *   1. the contact-point velocities of the root body (0) and of the wheel body;
 *   2. a normal impulse that cancels the approach speed along the contact normal (wheel body, and the root when W+108 is set);
 *   3. friction impulses along the lateral and the rolling axis, each scaled by the effective mass at the contact, limited by
 *      the normal force times the friction coefficients and curves (load, slip, speed, lean, brakes), plus a rear drive force;
 *   4. the forces applied to the wheel (or the root while the rider is off) and the root, a filtered load, diagnostics.
 *
 * Simulation S (native/game/notes/bike.md): body i (stride 0xa0): position +f0, linear velocity +100, mass +110,
 * orientation +730, angular velocity +740, inverse inertia +760. Input block S+1c: rear brake +28, steering x +2c,
 * rider reflex x / y +34 / +38. Rider off the bike S+2374. Control block *(S+5c30): throttle +0, +74.
 *
 * Wheel contact record W (0x150 bytes): */
#include "bike_tire.h"
#include "phys_misc.h"
#include "phys_mech.h"
#include "bike_curve.h"
#include "weather_grip.h"

#define W_BODY        0x000   /* u32: 2 front, 3 rear */
#define W_POINT       0x060   /* contact point (world) */
#define W_VEL         0x080   /* out: contact-point velocity in the contact frame (rolling, normal, lateral, lateral) */
#define W_MU_A        0x0a0   /* friction coefficients (vec4) of the rolling / lateral limits */
#define W_MU_B        0x0b0
#define W_GRIP_LAT    0x0c0   /* u32 out: 1 = lateral impulse within its limit */
#define W_GRIP_ROLL   0x0c4   /* u32 out: 1 = rolling impulse within its limit */
#define W_FORCE       0x0d0   /* out: total contact force (world) */
#define W_LOAD        0x0e0   /* filtered normal load: value +e0, time constant +e4, factor +e8 */
#define W_ACTIVE      0x100   /* u32: wheel touches */
#define W_ROOT_RESP   0x108   /* u32: the normal response also acts on the root body */
#define W_SLIP        0x110   /* curve input (the front record's is S+89f0, the rear's S+88a0) */
#define W_LATERAL     0x120   /* contact frame: lateral axis */
#define W_NORMAL      0x130   /*                 contact normal */
#define W_ROLL        0x140   /*                 rolling axis */

#define BODY(S, i)    ((S) + 0xa0u * (uint32_t)(i))
#define G_AXIS_X      0x00d615c0   /* globals (.data): world x axis, world up, the unit "numerator" (1/m = N / m), zero */
#define G_AXIS_UP     0x00d616a0
#define G_NUMERATOR   0x00d61650
#define G_ZERO        0x00d61670


/* Impulse that cancels the speed `speed` along an axis: (-N * speed) * scale / (eff_rot + 1/m)  (all lanes) */
static rx_v4 tire_impulse(rx_v4 neg_n, float speed, rx_v4 scale, float eff_rot, rx_v4 inv_mass) {
    return v4_div(v4_mul(v4_mul(v4_splat(speed), neg_n), scale), v4_add(v4_splat(eff_rot), inv_mass));
}

/* 0085c770 Mechanism_AddForceAtPoint (phys): EDX = mechanism S+d0, ECX = &force, ESI = &point (body-local), stack body; ret 4. */
static void tire_add_force_at_point(rx_addr S, rx_v4 force, rx_v4 point, uint32_t body) {
    rx_addr sp = rx_sp(), F = (sp - 0x20) & ~15u;
    rx_st4(F, force); rx_st4(F + 0x10, point);
    rx_set_sp(F - 8u);
    SU32(F - 8u, 0, 0); SU32(F - 4u, 0, body);
    phys_mech_add_force_at_point(F, S + 0xd0, F + 0x10, body);
    rx_set_sp(sp);
}

/* 0042d6b0 (phys): first-order filter F = {value, time, factor}: EAX = &F, xmm2 = dt, stack input; ret 4. */
static void tire_filter(rx_addr F, float dt, float input) {
    phys_lag_filter_update(F, dt, bike_x87_copyf(input));                 /* fld m32 ; fstp m32 */
}

/* 00447f10 BikeTire_ContactForce (EDI = S, stack dt, W; ret 8). */
void bike_tire_contact_force(rx_addr S, float dt, rx_addr W) {
    const rx_v4 N = rx_ld4(G_NUMERATOR), G = rx_ld4(G_ZERO);

    if (U32(W, W_ACTIVE) == 0) {          /* airborne wheel: no velocity, the load decays towards 0 */
        SU32(W, W_GRIP_LAT, 1);
        rx_st4(W + W_VEL, G);
        float T = F32(W, W_LOAD + 4), x = T > dt ? dt : T;
        float k = phys_div(x, T);
        SF32(W, W_LOAD + 8, k);
        float old = F32(W, W_LOAD);
        SF32(W, W_LOAD, phys_add(phys_mul(phys_sub(0.0f, old), k), old));
        return;
    }

    /* Lean of the root body: the angle of its x axis from the horizontal plane; lean = (pi - angle) / pi, blend = 1-(1-lean)^2 */
    const rx_v4 q = rx_ld4(S + 0x730);
    const float side_up = bike_dot3(bike_rotate(q, rx_ld4(G_AXIS_X)), rx_ld4(G_AXIS_UP));
    float angle = bike_asin_approx(bike_fabs(side_up), side_up);
    angle = bike_fabs(side_up >= 0.0f ? angle : phys_sub(-0.0f, angle));
    const float pi = 3.1415927410125732f;                                    /* 00977498 */
    const float lean = phys_div(phys_sub(pi, angle), pi);
    const float lean_inv = phys_sub(1.0f, lean);
    const float lean_blend = phys_sub(1.0f, phys_mul(lean_inv, lean_inv));

    /* Friction curves of this wheel over its slip input (indexed tables) */
    const uint32_t body = U32(W, W_BODY);
    const int front = body == 2;
    const float slip = F32(S, front ? 0x89f0 : 0x88a0);
    float mu_a_curve = bike_indexed_curve(S + (front ? 0x44dc : 0x4b14), slip);
    float mu_b_curve = bike_indexed_curve(S + (front ? 0x439c : 0x49d4), slip);
    if (g_rx_weather_grip_on) {   /* WEATHER GRIP (port, weather_grip.h): rain / snow, StaticMuSideSlip / StaticMuRoll */
        mu_a_curve = phys_mul(mu_a_curve, g_rx_weather_grip); mu_b_curve = phys_mul(mu_b_curve, g_rx_weather_grip);
    }
    const float roll_scale = bike_indexed_curve(S + (front ? 0x45cc : 0x4c04), slip);
    const float lat_scale  = bike_indexed_curve(S + (front ? 0x448c : 0x4ac4), slip);
    const rx_v4 inv_dt = v4_splat(phys_div(1.0f, dt));
    SU32(W, W_GRIP_LAT, 1);
    SU32(W, W_GRIP_ROLL, 1);

    /* Root body (0) at the contact point */
    const rx_v4 root_v = rx_ld4(S + 0x100), root_inv_inertia = rx_ld4(S + 0x760);
    const rx_v4 root_inv_mass = v4_div(N, rx_ld4(S + 0x110));
    const rx_v4 point = rx_ld4(W + W_POINT);
    const rx_v4 root_r = v4_sub(point, rx_ld4(S + 0xf0));
    const rx_v4 root_r_local = bike_rotate_inv(q, root_r);
    const rx_v4 root_vc = bike_point_velocity(q, rx_ld4(S + 0x740), root_v, root_r_local);

    /* Wheel body at the contact point */
    const rx_addr B = BODY(S, body);
    const rx_v4 qb = rx_ld4(B + 0x730), wheel_inv_inertia = rx_ld4(B + 0x760);
    const rx_v4 wheel_inv_mass = v4_div(N, rx_ld4(B + 0x110));
    const rx_v4 wheel_r = v4_sub(point, rx_ld4(B + 0xf0));
    const rx_v4 wheel_r_local = bike_rotate_inv(qb, wheel_r);
    const rx_v4 wheel_vc = bike_point_velocity(qb, rx_ld4(B + 0x740), rx_ld4(B + 0x100), wheel_r_local);

    const rx_v4 roll_axis = rx_ld4(W + W_ROLL), normal = rx_ld4(W + W_NORMAL), lat_axis = rx_ld4(W + W_LATERAL);
    {   /* the contact-frame velocity (rolling, normal, lateral, lateral) */
        float vr = bike_dot3(wheel_vc, roll_axis), vn = bike_dot3(wheel_vc, normal), vl = bike_dot3(wheel_vc, lat_axis);
        rx_st4(W + W_VEL, v4(vr, vn, vl, vl));
    }

    /* Normal impulses: cancel the approach speed (only when approaching) */
    const rx_v4 zero = v4_splat(0.0f);                                       /* 0092e12c */
    const rx_v4 neg_n = v4_sub(zero, v4_add(zero, N));
    rx_v4 root_normal_force = G, wheel_normal_force = G;
    if (U32(W, W_ROOT_RESP) != 0) {
        float approach = bike_dot3(normal, root_vc);
        if (bike_all3(v4_cmplt(v4_splat(approach), G))) {
            float e = bike_eff_rot(q, root_inv_inertia, root_r, normal);
            root_normal_force = v4_mul(tire_impulse(neg_n, approach, inv_dt, e, root_inv_mass), normal);
        }
    }
    {
        float approach = bike_dot3(normal, wheel_vc);
        if (bike_all3(v4_cmplt(v4_splat(approach), G))) {
            float e = bike_eff_rot(qb, wheel_inv_inertia, wheel_r, normal);
            wheel_normal_force = v4_mul(tire_impulse(neg_n, approach, inv_dt, e, wheel_inv_mass), normal);
        }
    }
    const rx_v4 normal_force = v4_add(wheel_normal_force, root_normal_force);
    const rx_v4 normal_force_sq = v4_mul(normal_force, normal_force);
    const rx_v4 load = v4_sqrt(v4_splat(bike_hsum_021(normal_force_sq)));     /* |normal force| */
    const rx_v4 roll_limit_base = v4_mul(load, v4_mul(rx_ld4(W + W_MU_A), v4_splat(mu_a_curve)));
    const rx_v4 lat_limit_base = v4_mul(load, v4_mul(rx_ld4(W + W_MU_B), v4_splat(mu_b_curve)));

    /* Friction impulses: lateral on the wheel body; rolling blended between the wheel body and the root by the lean */
    const float lat_speed = bike_dot3(lat_axis, wheel_vc);
    rx_v4 lat_impulse = tire_impulse(neg_n, lat_speed, inv_dt, bike_eff_rot(qb, wheel_inv_inertia, wheel_r, lat_axis), wheel_inv_mass);
    const float root_roll_speed = bike_dot3(roll_axis, root_vc);
    const float root_roll_eff = bike_eff_rot(q, root_inv_inertia, root_r, roll_axis);
    const float roll_speed = bike_dot3(roll_axis, wheel_vc);
    const float wheel_roll_eff = bike_eff_rot(qb, wheel_inv_inertia, wheel_r, roll_axis);
    const rx_v4 roll_scale_dt = v4_mul(inv_dt, v4_splat(lean));
    const rx_v4 wheel_roll = tire_impulse(neg_n, roll_speed, roll_scale_dt, wheel_roll_eff, wheel_inv_mass);
    const rx_v4 root_roll = tire_impulse(neg_n, root_roll_speed, roll_scale_dt, root_roll_eff, root_inv_mass);
    rx_v4 roll_impulse = v4_add(v4_mul(v4_sub(root_roll, wheel_roll), v4_splat(lean_blend)), wheel_roll);

    /* The demanded friction relative to the roll limit: P = (rolling, lateral, 0, 0) */
    const rx_v4 demand_den = v4_add(roll_limit_base, N);
    const rx_v4 lat_rel = v4_div(lat_impulse, demand_den), roll_rel = v4_div(roll_impulse, demand_den);
    rx_v4 demand = v4_movss(v4_shuf(lat_rel, G, 0x00), roll_rel);
    const rx_v4 demand_len = v4_sqrt(v4_splat(bike_hsum_021(v4_mul(demand, demand))));

    /* Rear drive / slide assist along the rolling axis (uses the REAR record's lateral speed S+8810+8, written by the rear call) */
    const float rear_lat_speed = F32(S, 0x8810 + 8);
    const rx_v4 root_v_local = bike_rotate_inv(q, root_v);
    const float speed_fade = phys_sub(1.0f, bike_clamp01(phys_div(root_v_local.v[2], 60.0f /* 00931da0 */)));
    float steer_in = phys_mul(phys_add(phys_add(F32(S, 0x2c), F32(S, 0x34)), F32(S, 0x38)), 0.5f);   /* steering x + reflex x + y */
    steer_in = steer_in >= 1.0f ? 1.0f : -1.0f >= steer_in ? -1.0f : steer_in;
    const float slide = bike_clamp01(phys_div(phys_sub(phys_sub(-0.0f, rear_lat_speed), 1.0f), 40.0f /* 009cc37c */));
    const float drive = phys_mul(phys_mul(slide, phys_sub(-0.0f, steer_in)), speed_fade);
    const rx_v4 drive_force = v4_splat(phys_mul(bike_indexed_curve(S + 0x4f24, F32(W, W_SLIP)), drive));
    SU32(S, 0x8da0, bike_fabs(drive) > 0.6000000238418579f /* 00931da8 */ ? 1 : 0);

    const rx_v4 zero_vec = zero;   /* (the code's xorps zero kept in a stack slot) */
    if (body == 3 && 0.0f > rear_lat_speed) {
        /* rear wheel sliding: the rolling demand is limited by a value from the normal's tilt, the reflex inputs and the speed */
        const rx_v4 v_local = bike_rotate_inv(q, root_v);
        const rx_v4 lat_rel2 = v4_div(lat_impulse, v4_add(roll_limit_base, N));
        const float reflex_y = 0.0f > F32(S, 0x38) ? 0.0f : F32(S, 0x38);
        const float reflex_x = F32(S, 0x34);
        const float ny = normal.v[1];
        float tilt = bike_asin_approx(bike_fabs(ny), ny);
        tilt = ny >= 0.0f ? tilt : phys_sub(-0.0f, tilt);
        float base = phys_sub(1.0f, phys_div((float)(1.5707963705062866 - (double)tilt), 1.5707963705062866f /* 00977508 */));
        float p = (float)((double)base * (double)base);             /* PowF(base, 2.0) 0042c1f0: exact square */
        float k = p >= 1.0f ? 1.0f : 1.0f < p ? p : (p != p ? p : 1.0f);
        float limit_a = 5.0f;                                        /* 00935890 */
        const int forward = bike_all3(v4_cmple(G, v4_splat(v_local.v[0])));
        const int backward = bike_all3(v4_cmplt(v4_splat(v_local.v[0]), G));
        if (forward && 0.0f > reflex_x) {
            float b = phys_mul(phys_sub(1.0f, reflex_y), F32(S, 0x2c));
            if (0.0f > b) b = 0.0f;
            limit_a = bike_lerp_clamped(phys_sub(-0.0f, phys_add(reflex_x, b)), 5.0f, phys_mul(k, 3.0f));
        } else if (backward && reflex_x > 0.0f) {
            float b = phys_mul(phys_sub(1.0f, reflex_y), F32(S, 0x2c));
            if (b > 0.0f) b = 0.0f;
            limit_a = bike_lerp_clamped(phys_add(reflex_x, b), 5.0f, phys_mul(k, 3.0f));
        } else if (forward && reflex_x > 0.0f) {
            limit_a = bike_lerp_clamped(reflex_x, 5.0f, 8.0f /* 009322b8 */);
        } else if (backward && 0.0f > reflex_x) {
            limit_a = bike_lerp_clamped(phys_sub(-0.0f, reflex_x), 5.0f, 8.0f);
        }
        const float limit = bike_lerp_clamped(reflex_y, limit_a, phys_mul(k, 3.0f /* 0092fe48 */));
        const rx_v4 L = v4_splat(limit);
        const rx_v4 clamped = v4_max(v4_sub(zero_vec, L), v4_min(roll_rel, L));
        demand = v4_movss(v4_shuf(lat_rel2, G, 0x00), clamped);
    }

    /* Limits of the two impulses */
    rx_v4 lat_limit = lat_limit_base, roll_limit = roll_limit_base, roll_offset = zero;
    if (bike_all3(v4_cmplt(N, demand_len))) {          /* demand beyond the unit circle: sliding */
        float mu;
        const float roll_speed_abs = bike_fabs(roll_speed);
        if (front) {
            mu = phys_mul(bike_param_curve(S + 0x466c, roll_speed_abs), bike_indexed_curve(S + 0x461c, F32(S, 0x89f0)));
        } else {
            rx_addr C = U32(S, 0x5c30);
            float throttle_blend = bike_lerp_clamped(F32(C, 0x00), 1.0f, 0.5f /* 009322cc */);
            float c74 = 0.0f > F32(C, 0x74) ? 0.0f : F32(C, 0x74);
            float curve = bike_param_curve(S + 0x4ca4, roll_speed_abs);
            float table = bike_indexed_curve(S + 0x4c54, F32(S, 0x88a0));
            mu = phys_mul(curve, phys_mul(table, phys_sub(1.0f, phys_mul(throttle_blend, c74))));
        }
        const rx_v4 dir = bike_normalize(demand);
        const rx_v4 scales = v4_movss(v4_shuf(v4_splat(lat_scale), v4_splat(roll_scale), 0xee), v4_splat(roll_scale));
        lat_limit = v4_mul(v4_splat(lat_scale), lat_limit_base);
        const rx_v4 r = v4_mul(v4_lane(v4_mul(scales, dir), 0), roll_limit_base);
        roll_limit = v4_max(v4_sub(zero_vec, r), r);
        roll_offset = v4_mul(v4_lane(dir, 0), v4_splat(mu));
        if (body == 3) {   /* rear brake: towards the full roll limit */
            const rx_v4 rb = v4_splat(F32(S, 0x28));
            roll_limit = v4_add(v4_mul(v4_sub(N, rb), roll_limit), v4_mul(rb, roll_limit_base));
        }
    }
    if (bike_all3(v4_cmplt(lat_limit, lat_impulse))) {
        lat_impulse = v4_min(lat_impulse, lat_limit); SU32(W, W_GRIP_LAT, 0);
    } else if (bike_all3(v4_cmplt(lat_impulse, v4_sub(zero_vec, lat_limit)))) {
        lat_impulse = v4_max(lat_impulse, v4_sub(zero_vec, lat_limit)); SU32(W, W_GRIP_LAT, 0);
    }
    rx_v4 lat_force = v4_mul(lat_axis, lat_impulse);
    if (bike_all3(v4_cmplt(roll_limit, roll_impulse))) {
        roll_impulse = v4_min(roll_impulse, v4_add(roll_limit, roll_offset)); SU32(W, W_GRIP_ROLL, 0);
    } else if (bike_all3(v4_cmplt(roll_impulse, v4_sub(zero_vec, roll_limit)))) {
        roll_impulse = v4_max(roll_impulse, v4_add(v4_sub(zero_vec, roll_limit), roll_offset)); SU32(W, W_GRIP_ROLL, 0);
    }
    if (body == 3) roll_impulse = v4_add(roll_impulse, drive_force);
    rx_v4 roll_force = v4_mul(roll_axis, roll_impulse);
    if (U32(S, 0x2374) != 0) {   /* rider off: weak friction, no grip */
        roll_force = v4_mul(v4_splat(0.2f /* 0092e388 */), roll_force);
        lat_force = v4_mul(v4_splat(0.3f /* 00931da4 */), lat_force);
        SU32(W, W_GRIP_ROLL, 0); SU32(W, W_GRIP_LAT, 0);
    }

    /* Apply: wheel force at the wheel's lever arm (the root's while the rider is off), the root's normal response */
    const rx_v4 total = v4_add(v4_add(roll_force, normal_force), lat_force);
    rx_st4(W + W_FORCE, total);
    const rx_v4 wheel_force = v4_add(v4_add(roll_force, wheel_normal_force), lat_force);
    if (U32(S, 0x2374) != 0) tire_add_force_at_point(S, wheel_force, root_r_local, 0);
    else tire_add_force_at_point(S, wheel_force, wheel_r_local, body);
    tire_add_force_at_point(S, root_normal_force, root_r_local, 0);
    tire_filter(W + W_LOAD, dt, phys_sqrt(bike_hsum_021(normal_force_sq)));

    /* Diagnostics (rear S+8e80.., front S+8e90..): impulse bounds, forces, torque about the root in the root frame */
    const rx_v4 roll_bound = v4_add(v4_max(v4_sub(zero_vec, roll_offset), roll_offset), roll_limit);
    const rx_v4 d_roll = v4_movss(v4_shuf(roll_impulse, roll_bound, 0x00), v4_sub(zero_vec, roll_bound));
    const rx_v4 d_lat = v4_movss(v4_shuf(lat_impulse, lat_limit, 0x00), v4_sub(zero_vec, lat_limit));
    const rx_v4 d_force = v4_movss(v4_shuf(root_normal_force, normal_force, 0x00), wheel_force);
    const int rear = body == 3;
    rx_st4(S + (rear ? 0x8e80 : 0x8e90), d_roll);
    rx_st4(S + (rear ? 0x8ea0 : 0x8eb0), d_lat);
    rx_st4(S + (rear ? 0x8ec0 : 0x8ed0), d_force);
    rx_st4(S + (rear ? 0x8f00 : 0x8ee0), rx_ld4(W + W_FORCE));
    rx_st4(S + (rear ? 0x8f10 : 0x8ef0), bike_rotate_inv(q, bike_cross(root_r, rx_ld4(W + W_FORCE))));
}
RX_FUNCTION(0x00447f10, bike_tire_contact_force)
