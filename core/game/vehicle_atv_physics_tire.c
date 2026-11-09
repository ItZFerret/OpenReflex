/*0049a340: dedicated four-wheel ATV contact forces, EDI=S, stack(dt,W), RET8.
 * Original ordered SSE kernels and four-wheel-specific slide/brake/differential
 * paths; no motocross simulation dispatch or fallback. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "phys_misc.h"
#include "phys_mech.h"
#include "bike_curve.h"
#include "weather_grip.h"

/* Optional developer observer; null in the product runtime. No game-memory writes. */
typedef void (*AtvTireObserver)(uint32_t,const float*,const float*);
static AtvTireObserver tire_observer;
void rx_atv_tire_set_observer(AtvTireObserver observer){tire_observer=observer;}
#define ATV_TIRE_OBSERVE(stage,a,b) do{if(tire_observer){rx_v4 va=(a),vb=(b);tire_observer(stage,va.v,vb.v);}}while(0)

#define W_BODY        0x000   /* u32: front2/3, rear4/5 */
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
#define W_SLIP        0x110   /* wheel-specific indexed curve input */
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

/* Original0049a340, invoked once per ATV wheel by S11. */
void rx_atv_tire_contact_force(rx_addr S, float dt, rx_addr W) {
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
    float lean = phys_div(phys_sub(pi, angle), pi);
    const float lean_inv = phys_sub(1.0f, lean);
    const float lean_blend = phys_sub(1.0f, phys_mul(lean_inv, lean_inv));

    /* Friction curves of this wheel over its slip input (indexed tables) */
    const uint32_t body = U32(W, W_BODY);
    const int front = body == 2 || body == 3;
    const int rear = body == 4 || body == 5;
    const uint32_t paired = body == 2 ? 0x9390 : body == 3 ? 0x9240 : body == 4 ? 0x9630 : 0x94e0;
    if(U32(S,paired)) lean=phys_mul(lean,.5f);
    const float slip = F32(W,W_SLIP);
    float mu_a_curve = bike_indexed_curve(S + (front ? 0x43f4 : 0x4894), slip);
    float mu_b_curve = bike_indexed_curve(S + (front ? 0x42b4 : 0x4754), slip);
    if (g_rx_weather_grip_on) {   /* WEATHER GRIP (port, weather_grip.h): rain / snow, StaticMuSideSlip / StaticMuRoll */
        mu_a_curve = phys_mul(mu_a_curve, g_rx_weather_grip); mu_b_curve = phys_mul(mu_b_curve, g_rx_weather_grip);
    }
    const float roll_scale = bike_indexed_curve(S + (front ? 0x44e4 : 0x4984), slip);
    const float lat_scale  = bike_indexed_curve(S + (front ? 0x43a4 : 0x4844), slip);
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
    const rx_v4 roll_limit_base = v4_mul(load,v4_mul(rx_ld4(W + W_MU_A),v4_splat(mu_a_curve)));
    const rx_v4 lat_limit_base = v4_mul(load,v4_mul(rx_ld4(W + W_MU_B),v4_splat(mu_b_curve)));

    ATV_TIRE_OBSERVE(1,roll_limit_base,lat_limit_base);

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

    ATV_TIRE_OBSERVE(2,roll_impulse,demand);
    const rx_v4 zero_vec=zero;
    rx_v4 drive_force=zero;
    const float lateral=F32(W,W_VEL+8);
    if(rear && lateral<0){
        rx_v4 local_velocity=bike_rotate_inv(q,root_v);
        float fade=phys_sub(1,bike_clamp01(phys_div(local_velocity.v[2],40)));
        float steer=phys_mul(phys_add(F32(S,0x34),F32(S,0x2c)),.5f);
        steer=steer>=1?1:steer<=-1?-1:steer;
        float slide=bike_clamp01(phys_div(phys_sub(phys_sub(-0.0f,lateral),1),40));
        float drive=phys_mul(phys_mul(slide,phys_sub(-0.0f,steer)),fade);
        drive_force=v4_splat(phys_mul(bike_indexed_curve(S+0x4ac4,F32(W,W_SLIP)),drive));
        SU32(S,0x99b0,bike_fabs(drive)>.6000000238418579f);
        float ny=normal.v[1],tilt=bike_asin_approx(bike_fabs(ny),ny);
        if(ny<0)tilt=phys_sub(-0.0f,tilt);
        float base=phys_sub(1,phys_div(phys_sub(rx_bf(0x3fc90fdb),tilt),rx_bf(0x3fc90fdb)));
        float k=(float)((double)base*(double)base);
        if(k>=1)k=1;else if(k<=.33333f)k=.33333f;
        float reflex=F32(S,0x59e4),limit=6;
        int forward=bike_all3(v4_cmple(G,v4_splat(local_velocity.v[0])));
        int backward=bike_all3(v4_cmplt(v4_splat(local_velocity.v[0]),G));
        if(forward&&reflex<0){
            float st=F32(S,0x2c);if(st<0)st=0;
            float t=phys_sub(-0.0f,phys_add(reflex,st));if(t>=1)t=1;
            limit=bike_lerp_clamped(t,6,phys_mul(k,1.5f));
        }else if(backward&&reflex>0){
            float st=F32(S,0x2c);if(st>0)st=0;
            float t=phys_add(reflex,st);if(t>=1)t=1;
            limit=bike_lerp_clamped(t,6,phys_mul(k,1.5f));
        }else if(forward&&reflex>0)limit=bike_lerp_clamped(reflex,6,12);
        else if(backward&&reflex<0)limit=bike_lerp_clamped(phys_sub(-0.0f,reflex),6,12);
        rx_v4 L=v4_splat(limit),clamped=v4_max(v4_sub(zero_vec,L),v4_min(roll_rel,L));
        demand=v4_movss(v4_shuf(v4_div(lat_impulse,v4_add(roll_limit_base,N)),G,0),clamped);
    }

    ATV_TIRE_OBSERVE(3,drive_force,demand);

    /* Limits of the two impulses */
    rx_v4 lat_limit = lat_limit_base, roll_limit = roll_limit_base, roll_offset = zero;
    if (bike_all3(v4_cmplt(N, demand_len))) {          /* demand beyond the unit circle: sliding */
        float mu;
        /*0049aa45/aa53 cache wheel LINEAR velocity;0049bbb8..bbf8
         * dot it with rolling axis for the speed friction curve. The impulse
         * above correctly uses wheel contact-point velocity instead. */
        const float roll_speed_abs=bike_fabs(bike_dot3(roll_axis,rx_ld4(B+0x100)));
        if(front)mu=phys_mul(bike_param_curve(S+0x4584,roll_speed_abs),bike_indexed_curve(S+0x4534,F32(W,W_SLIP)));
        else{
            float throttle_blend=bike_lerp_clamped(F32(S,0x5880),1,1);
            float factor=F32(S,0x596c);if(0>factor)factor=0;
            float curve=bike_param_curve(S+0x4a24,roll_speed_abs),table=bike_indexed_curve(S+0x49d4,F32(W,W_SLIP));
            mu=phys_mul(curve,phys_mul(table,phys_sub(1,phys_mul(throttle_blend,factor))));
        }
        const rx_v4 dir = bike_normalize(demand);
        const rx_v4 scales = v4_movss(v4_shuf(v4_splat(lat_scale), v4_splat(roll_scale), 0xee), v4_splat(roll_scale));
        lat_limit = v4_mul(v4_splat(lat_scale), lat_limit_base);
        const rx_v4 r = v4_mul(v4_lane(v4_mul(scales, dir), 0), roll_limit_base);
        roll_limit = v4_max(v4_sub(zero_vec, r), r);
        roll_offset = v4_mul(v4_lane(dir, 0), v4_splat(mu));
        if(F32(S,0x28)>0){
            float a=bike_fabs(F32(S,0x5968)),b=bike_fabs(F32(S,0x5960));if(!(b>a))b=a;
            rx_v4 weight=v4_splat(b),curve=v4_splat(bike_indexed_curve(S+0x49d4,F32(W,W_SLIP)));
            roll_limit=v4_add(v4_mul(v4_max(curve,roll_limit),v4_sub(N,weight)),v4_mul(weight,roll_limit));
        }
    }
    if (bike_all3(v4_cmplt(lat_limit, lat_impulse))) {
        lat_impulse = v4_min(lat_impulse, lat_limit); SU32(W, W_GRIP_LAT, 0);
    } else if (bike_all3(v4_cmplt(lat_impulse, v4_sub(zero_vec, lat_limit)))) {
        lat_impulse = v4_max(lat_impulse, v4_sub(zero_vec, lat_limit)); SU32(W, W_GRIP_LAT, 0);
    }
    rx_v4 lat_force = v4_mul(lat_axis, lat_impulse);
    if(bike_all3(v4_cmplt(roll_limit,roll_impulse))){
        rx_v4 bound=v4_add(roll_limit,roll_offset);
        if(bike_all3(v4_cmplt(bound,roll_impulse)))roll_impulse=bound;
        SU32(W,W_GRIP_ROLL,0);
    }else if(bike_all3(v4_cmplt(roll_impulse,v4_sub(zero_vec,roll_limit)))){
        rx_v4 bound=v4_add(v4_sub(zero_vec,roll_limit),roll_offset);
        if(bike_all3(v4_cmplt(roll_impulse,bound)))roll_impulse=bound;
        SU32(W,W_GRIP_ROLL,0);
    }
    ATV_TIRE_OBSERVE(4,roll_impulse,lat_force);
    roll_impulse=v4_add(roll_impulse,drive_force);
    rx_v4 roll_force = v4_mul(roll_axis, roll_impulse);
    ATV_TIRE_OBSERVE(5,roll_force,lat_force);

    /* Apply: wheel force at the wheel's lever arm (the root's while the rider is off), the root's normal response */
    const rx_v4 total = v4_add(v4_add(roll_force, normal_force), lat_force);
    rx_st4(W + W_FORCE, total);
    const rx_v4 wheel_force = v4_add(v4_add(roll_force, wheel_normal_force), lat_force);
    tire_add_force_at_point(S,wheel_force,wheel_r_local,body);
    if(U32(0x00d92d54,0)==1 && rear && (!U32(S,0x94e0)||!U32(S,0x9630))){
        tire_add_force_at_point(S,v4_sub(zero_vec,lat_force),root_r_local,0);
        tire_add_force_at_point(S,lat_force,G,0);
    }
    tire_add_force_at_point(S,root_normal_force,root_r_local,0);
    tire_filter(W + W_LOAD, dt, phys_sqrt(bike_hsum_021(normal_force_sq)));

}
RX_FUNCTION(0x0049a340,rx_atv_tire_contact_force)
