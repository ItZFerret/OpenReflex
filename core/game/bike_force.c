/* bike_force.c -- the force stage (slot 11) of the motocross simulation: gear choice, engine and transmission, axle torques,
 * suspension spring / damper, wheel drag (native/game/notes/bike.md, _re/notes/engine-audio.md, bike-connected-path.md).
 *
 * Simulation object S (RavenSimulationMotocross), the fields this stage uses (offsets in comments at each use):
 *   +0d0 mechanism M            +0f0 / +100 root position / velocity   +140 root force accumulator   +730 root orientation (quat)
 *   +740 root angular velocity  +780 root torque accumulator           +5c30 control block C (see below)
 *   bodies (stride 0xa0): 1 rider, 2 front tire (+230 pos, +240 vel, +870 orientation), 3 rear tire (+2d0, +2e0, +910)
 *   +8790 / +88e0 rear / front wheel contact records, +8890 / +89e0 their contact flags, +8d98 both wheels airborne
 *   +2374 rider off the bike
 *   powertrain: +8abc engine speed (rad/s), +8ad8 load, +8ac0 gear, +8ac4 five gear ratios, +8aec shift timer,
 *   +8adc / +8ae0 coupling angle / rate, +5014 maximum rpm, +505c idle rpm, +4f74 engine torque curve, +5134 engine drag curve.
 * Control block C = *(S+5c30): +00 throttle, +04 brake, +08 rear axle torque, +0c front axle torque, +20 drag vec4,
 *   +60 torque reaction factor, +84 drive mode (0 coupled, 1 blended, 2 free).
 * Vehicle bodies are touched only through the mechanism functions (0085c770 force at a point, 0085c8b0 local force): the force at a point
 * and local force are direct calls. */
#include "bike_force.h"
#include "crt.h"
#include "bike_tire.h"
#include "bike_curve.h"
#include "phys_mech.h"
#include <string.h>

/* ---- globals and constants of the original (.data globals are read from memory, .rdata constants are literals) ---------- */
#define G_ZERO        0x00d61670u   /* zero vec4 (also the "point" of the force calls and the sign reference of the drag tests) */
#define G_NUMERATOR   0x00d61650u   /* vec4 numerator of the inverse inertia (S+940 = N / S+930) */
#define G_AXIS_Y      0x00d616a0u   /* the bike's up axis in the root frame (vec4) */
#define G_BRAKE_SPEED 0x009d789cu   /* float: speed above which the brake force starts to fade (2.5) */
#define NEG_ZERO      (-0.0f)       /* 009774a8 */
#define RPM_PER_RAD   rx_bf(0x4118cccdu)   /* 009774d0: 9.55 rpm per rad/s */

/* ---- calls out of this file ----------------------------------------------------------------------------------------------- */
static rx_regs force_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }

/* 0085c770 Mechanism_AddForceAtPoint (phys): EDX = mechanism S+d0, ECX = &force vec4, ESI = &point vec4 (body-local), stack body
 * index; ret 4. Adds the force to the body's force accumulator and the lever torque to its torque accumulator. */
static void force_add_force_at_point(rx_addr S, rx_addr force, rx_addr point, uint32_t body) {
    const rx_addr sp0 = rx_sp(), sp = sp0 - 8u;
    SU32(sp, 0, 0); SU32(sp + 4, 0, body);
    rx_set_sp(sp);
    phys_mech_add_force_at_point(force, S + 0xd0, point, body);
    rx_set_sp(sp0);
}

/* 0085c8b0 (phys): ECX = mechanism S+d0, stack body index, &force vec4 (local to the body); ret 8. Rotates the force into the world
 * by the body's orientation and adds it to the body's force accumulator. */
static void force_add_local_force(rx_addr S, uint32_t body, rx_addr force) {
    phys_mech_add_body_force(S + 0xd0, body, force);
}

/* 00447f10 BikeTire_ContactForce (bike lead): EDI = S, stack dt, W (wheel contact record); ret 8. Its EAX / ECX / EDX on return can be
 * its entry values (they are live to vf11's callers), so the caller's register state is carried through: `regs` holds ECX / EDX /
 * EAX as the original has them at the call and the callee's leftovers on return. */
static void force_tire_contact_force(rx_addr S, float dt, rx_addr W, rx_regs* regs) {
    const int active = U32(W, 0x100) != 0;
    bike_tire_contact_force(S, dt, W);
    regs->r[0] = active ? W + 0xd0 : W + 0xe0;          /* left over: EAX = &W+d0 / &W+e0, ECX = the security cookie, */
    regs->r[1] = U32(0, 0x009b45e4);                    /* EDX = the mechanism when active (else untouched) */
    if (active) regs->r[2] = S + 0xd0;
}

/* 009009d2 MSVCR90 _CIpow (import thunk; st(1) = x, st(0) = y; result st(0)), called as the engine step does: fld x (m32) ; fld 0.25
 * (m64, 00977868) ; call. The result is stored as a float (fstp m32). */
static float force_pow_quarter(float x) {
    return rx_f80_to_f32(crt_ci_pow(rx_f80_from_f32(x), rx_f80_from_f64(0.25), CRT_CW_MAIN), 0);
}

/* a / b on the x87 stack (fld a ; fdiv b) at the thread's 53-bit precision, stored as a float (fstp m32): two roundings. */
static float force_x87_div_f32(float a, float b) {
    return rx_f80_to_f32(rx_f80_div(rx_f80_from_f32(a), rx_f80_from_f32(b), BIKE_X87_BITS, 0), 0);
}

/* ---- small shared pieces -------------------------------------------------------------------------------------------------- */

/* 0 - x as the code computes it (subss from -0.0: the sign of a zero result differs from -x only in NaN payloads). */
static inline float force_neg(float x) { return phys_sub(NEG_ZERO, x); }

/* (x, 0, 0, 0) + v, all four lanes (the movss-built vec4 added to an accumulator). */
static inline rx_v4 force_add_x(float x, rx_v4 v) { return v4_add(v4(x, 0.0f, 0.0f, 0.0f), v); }

/* ==== 00446a40 BikeEngine_CouplingSpring ================================================================================== */
/* Torsional coupling of the drivetrain (stack S, factor, dt; ret 12). A damped spring with state angle S+8adc / rate S+8ae0,
 * driven by the root-local Y component of S+160 (the net force the root receives): stiffness k = 1000 at factor <= 0 down to 500 at
 * factor >= 1 (linear between),  accel = -angle * k - localY;  rate = (accel * dt + rate) * 0.97;  angle += rate * dt.
 * The original also rotates the up axis G_AXIS_Y by S+730 and dots it with S+d0..: a result nothing reads (dropped here).
 * Returns the root-local Y (the original leaves it splatted in xmm2). */
float bike_engine_coupling_spring(rx_addr S, float factor, float dt) {
    const rx_v4 q = v4_ld(S + 0x730);
    const rx_v4 local = bike_rotate_inv(q, v4_ld(S + 0x160));
    const float local_y = local.v[1];
    float k;
    if (0.0f >= factor) k = F32(0, 0x009cf5b0);                  /* 1000 (.data) */
    else if (factor >= 1.0f /* 00931f24 */) k = 500.0f /* 009778ec */;
    else k = phys_sub(F32(0, 0x009cf5b0), phys_mul(factor, 500.0f));
    const float angle = F32(S, 0x8adc), rate = F32(S, 0x8ae0);
    float accel = phys_add(force_neg(phys_mul(angle, k)), force_neg(local_y));
    float new_rate = phys_mul(phys_add(phys_mul(accel, dt), rate), rx_bf(0x3f7851ecu) /* 009778e8: 0.97 */);
    SF32(S, 0x8ae0, new_rate);
    SF32(S, 0x8adc, phys_add(phys_mul(new_rate, dt), angle));
    return local_y;
}
RX_FUNCTION(0x00446a40, bike_engine_coupling_spring)

/* ==== 00447cd0 BikeWheels_ApplyAxleTorques ================================================================================ */
/* Stack S; ret 4. The controls' axle torques: C+8 spins the rear wheel (S+960 += (T,0,0,0)) with the opposite reaction on the root
 * (S+780 -= (T,0,0,0)); C+0c spins the front wheel (S+8c0), its reaction -(T,0,0,0) is rotated by the front wheel's orientation
 * S+870 into the world and by the root's inverse S+730 into the root frame, and added to S+780. */
void bike_wheels_apply_axle_torques(rx_addr S) {
    const rx_addr C = U32(S, 0x5c30);
    const rx_v4 rear = v4(F32(C, 8), 0.0f, 0.0f, 0.0f);
    v4_st(S + 0x960, v4_add(v4_ld(S + 0x960), rear));                            /* rear wheel torque accumulator */
    v4_st(S + 0x780, v4_add(v4_ld(S + 0x780), v4_sub(v4_zero(), rear)));         /* root torque accumulator */
    const rx_v4 front = v4(F32(U32(S, 0x5c30), 0xc), 0.0f, 0.0f, 0.0f);          /* the control pointer is read again */
    v4_st(S + 0x8c0, v4_add(v4_ld(S + 0x8c0), front));                           /* front wheel torque accumulator */
    const rx_v4 world = bike_rotate(v4_ld(S + 0x870), v4_xyz(v4_sub(v4_zero(), front)));
    const rx_v4 reaction = bike_rotate_inv(v4_ld(S + 0x730), world);
    v4_st(S + 0x780, v4_add(v4_ld(S + 0x780), reaction));
}
RX_FUNCTION(0x00447cd0, bike_wheels_apply_axle_torques)

/* ==== gear choice ========================================================================================================= */
/* The shared part of 00445e80 / 004460f0: the root-local forward speed over the wheel radius S+48ec (at least 0.5), then each of the
 * five ratios scored by the engine torque curve at the engine speed it would give (capped at 0.1047 * max rpm S+5014, floored at 0),
 * minus the drag term that cancels in the original's (f * ratio - drag) + drag. Returns the best ratio index (the first one on a tie;
 * a later ratio must score strictly more), -1 when the gate S+2eb0 is set (no choice made). */
static int32_t gear_best_ratio(rx_addr S) {
    const rx_v4 local = bike_rotate_inv(v4_ld(S + 0x730), v4_ld(S + 0x100));
    float rate = phys_div(local.v[2], F32(S, 0x48ec));
    if (0.5f /* 009322cc */ > rate) rate = 0.5f;
    if (U32(S, 0x2eb0) != 0) return -1;
    const float max_rot = phys_mul(F32(S, 0x5014), rx_bf(0x3dd67344u) /* 00977870 */);
    const float square = phys_mul(max_rot, max_rot);
    float best_score = 0.0f;
    int32_t best = 0;
    for (int32_t i = 0; i < 5; i++) {
        const float ratio = F32(S, 0x8ac4 + 4 * i);
        float rot = phys_mul(ratio, rate);
        if (rot >= max_rot) rot = max_rot;
        if (0.0f >= rot) rot = 0.0f;
        const float drag = phys_sub(NEG_ZERO, phys_div(phys_mul(phys_mul(F32(S, 0x50ec), rot), rot), square));
        const float force = bike_param_curve(S + 0x4f74, rot);
        const float score = phys_add(phys_sub(phys_mul(force, ratio), drag), drag);
        if (score > best_score || i == 0) { best_score = score; best = i; }
    }
    return best;
}

/* 00445e80 BikeGear_AutoSelect (EDI = S; ret 0): called while the throttle is on. Shifts up to the best ratio once the shift timer
 * S+8aec passes the up threshold S+8ae4, down by ONE gear (at most to 0) once it passes the down threshold S+8ae8; the timer
 * restarts at 0 on a shift. */
int32_t bike_gear_auto_select(rx_addr S) {
    const int32_t best = gear_best_ratio(S);
    if (best < 0) return best;
    const int32_t gear = I32(S, 0x8ac0);
    const float timer = F32(S, 0x8aec);
    if (best > gear) {
        if (timer > F32(S, 0x8ae4)) { SU32(S, 0x8ac0, best); SF32(S, 0x8aec, 0.0f); }
    } else if (best < gear) {
        if (timer > F32(S, 0x8ae8)) { SU32(S, 0x8ac0, gear - 1 > 0 ? gear - 1 : 0); SF32(S, 0x8aec, 0.0f); }
    }
    return best;
}
RX_FUNCTION(0x00445e80, bike_gear_auto_select)

/* 004460f0 BikeGear_SelectDownshift (stack S; ret 4): called while the throttle is off. Only shifts DOWN, straight to the best ratio,
 * once the timer S+8aec passes the down threshold S+8ae8. */
int32_t bike_gear_select_downshift(rx_addr S) {
    const int32_t best = gear_best_ratio(S);
    if (best < 0) return best;
    if (best < I32(S, 0x8ac0) && F32(S, 0x8aec) > F32(S, 0x8ae8)) {
        SU32(S, 0x8ac0, best);
        SF32(S, 0x8aec, 0.0f);
    }
    return best;
}
RX_FUNCTION(0x004460f0, bike_gear_select_downshift)

/* ==== 00446340 BikeEngine_DriveForces ===================================================================================== */
/* Engine / transmission step (EDI = S, stack dt; ret 4). With omega = engine speed S+8abc in rad/s, limited to [idle, max] =
 * [S+505c, S+5014] / 9.55:
 *   load S+8ad8 = (throttle - clamp01((omega - idle) / (max - idle)) + 1) * 0.5     (read by the engine audio)
 *   blend = clamp01((S+52ac - S+8ab8) / S+52ac) ^ 0.25     how far the clutch is in
 *   omega = lerp(S+50a4 / 9.55, omega, blend), clamped to [0, max]
 *   drive = engine torque curve (omega), dragc = engine drag curve (omega), ratio = S+8ac4[gear]
 *   drag = -(dragc * (S+5264 * ratio) * S+50ec), sign flipped when the rear wheel spins backwards (S+920 < 0)
 * then by the drive mode C+84: 0 = coupled (omega := ratio * wheel spin), 1 = blended with the free engine, 2 = free (omega += dt *
 * force / inertia); wheel torque / root reaction go to S+960 / S+780, the inertia S+930 = (S+3aac [+ S+3a64], 0, 0, 0) + S+3a10 and
 * its inverse S+940 = N / S+930 are updated, the coupling spring 00446a40 runs with the clutch factor, omega is clamped to
 * [idle, max], and with the brake on (C+4 > 0) a braking force along the local -Z (fading out above 2.5 + 10 speed) goes to the root
 * through 0085c8b0. */
void bike_engine_drive_forces(rx_addr S, float dt) {
    const rx_addr C = U32(S, 0x5c30);
    const rx_v4 g = bike_g_zero(), numerator = v4_ld(G_NUMERATOR);
    const float omega_max = phys_div(F32(S, 0x5014), RPM_PER_RAD);
    const float omega_min = phys_div(F32(S, 0x505c), RPM_PER_RAD);
    const float wheel_spin = F32(S, 0x920);          /* rear wheel angular speed (body 3 angular velocity x) */
    float omega = F32(S, 0x8abc);
    if (omega >= omega_max) omega = omega_max;
    if (0.0f >= omega) omega = 0.0f;
    SF32(S, 0x8abc, omega);

    float fraction = phys_div(phys_sub(omega, omega_min), phys_sub(omega_max, omega_min));
    if (fraction >= 1.0f /* 00931f24 */) fraction = 1.0f;
    else if (0.0f >= fraction) fraction = 0.0f;
    const float throttle = F32(C, 0);
    SF32(S, 0x8ad8, phys_mul(phys_add(phys_sub(throttle, fraction), 1.0f), 0.5f /* 009322cc */));

    float blend = phys_div(phys_sub(F32(S, 0x52ac), F32(S, 0x8ab8)), F32(S, 0x52ac));
    if (blend >= 1.0f) blend = 1.0f;
    else if (0.0f >= blend) blend = 0.0f;
    blend = force_pow_quarter(blend);                                  /* stored as a float */

    float speed = bike_lerp_clamped(blend, phys_div(F32(S, 0x50a4), RPM_PER_RAD), omega);
    if (speed >= omega_max) speed = omega_max;
    if (0.0f >= speed) speed = 0.0f;
    SF32(S, 0x8abc, speed);

    const float ratio = F32(S, 0x8ac4 + 4 * I32(S, 0x8ac0));
    const float drive = bike_param_curve(S + 0x4f74, speed);         /* engine torque */
    const float drag_curve = bike_param_curve(S + 0x5134, speed);     /* engine drag */
    const float k_ratio = F32(S, 0x5264), k_drag = F32(S, 0x50ec);
    float drag = force_neg(phys_mul(drag_curve, phys_mul(phys_mul(k_ratio, ratio), k_drag)));
    if (bike_all3(v4_cmplt(v4_splat(wheel_spin), g))) drag = force_neg(drag);

    float wheel_torque = 0.0f, root_torque = 0.0f, clutch = 1.0f;       /* L24, L20, L28 of the original's frame */
    const float reaction = F32(C, 0x60);
    switch (U32(C, 0x84)) {
    case 0: {   /* coupled: the engine turns with the rear wheel */
        const float force = phys_add(phys_mul(phys_sub(phys_mul(drive, ratio), drag), throttle), drag);
        root_torque = force_neg(phys_mul(reaction, force));
        wheel_torque = force_neg(force);
        v4_st(S + 0x930, force_add_x(phys_add(F32(S, 0x3aac), F32(S, 0x3a64)), v4_ld(S + 0x3a10)));
        v4_st(S + 0x940, v4_div(numerator, v4_ld(S + 0x930)));
        SF32(S, 0x8abc, phys_mul(ratio, wheel_spin));
        clutch = phys_div(force > 0.0f ? 0.0f : force, drag);
        break;
    }
    case 1: {   /* blended between the free engine and the coupled one by the clutch blend */
        const float free_drag = force_neg(phys_mul(drag_curve, k_drag));
        const float free_force = phys_add(phys_mul(phys_sub(phys_mul(phys_mul(ratio, 2.0f /* 0092e330 */), drive), free_drag), throttle), free_drag);
        const float free_root = force_neg(phys_mul(free_force, reaction));
        const float free_wheel = force_neg(free_force);
        const float free_speed = phys_mul(phys_add(phys_div(phys_mul(free_force, dt), F32(S, 0x3a64)), speed), k_ratio);
        const float matched = phys_mul(ratio, wheel_spin);
        const float force = phys_add(phys_mul(phys_sub(phys_mul(drive, ratio), drag), throttle), drag);
        root_torque = force_neg(phys_mul(force, reaction));
        wheel_torque = force_neg(force);
        /* the clutch factor divides on the x87 stack (53-bit), then each torque / speed is a clamped lerp towards the free value */
        clutch = bike_lerp_clamped(blend, 1.0f, force_x87_div_f32(force > 0.0f ? 0.0f : force, drag));
        wheel_torque = bike_lerp_clamped(blend, free_wheel, wheel_torque);
        root_torque = bike_lerp_clamped(blend, free_root, root_torque);
        float s = bike_lerp_clamped(blend, free_speed, matched);
        if (s >= omega_max) s = omega_max;
        if (0.0f >= s) s = 0.0f;
        SF32(S, 0x8abc, s);
        v4_st(S + 0x930, force_add_x(F32(S, 0x3aac), v4_ld(S + 0x3a10)));
        v4_st(S + 0x940, v4_div(numerator, v4_ld(S + 0x930)));
        break;
    }
    case 2: {   /* free engine: omega integrates the net engine force */
        const float free_drag = force_neg(phys_mul(drag_curve, k_drag));
        const float force = phys_mul(phys_add(phys_mul(phys_sub(drive, free_drag), throttle), free_drag), dt);
        v4_st(S + 0x930, force_add_x(F32(S, 0x3aac), v4_ld(S + 0x3a10)));
        v4_st(S + 0x940, v4_div(numerator, v4_ld(S + 0x930)));
        SF32(S, 0x8abc, phys_add(phys_div(force, F32(S, 0x3a64)), F32(S, 0x8abc)));
        break;
    }
    default: break;
    }

    bike_engine_coupling_spring(S, clutch, dt);

    float s = F32(S, 0x8abc);
    if (s >= omega_max) s = omega_max;
    if (omega_min >= s) s = omega_min;
    SF32(S, 0x8abc, s);
    v4_st(S + 0x960, force_add_x(force_neg(wheel_torque), v4_ld(S + 0x960)));     /* rear wheel torque accumulator */
    v4_st(S + 0x780, force_add_x(root_torque, v4_ld(S + 0x780)));                  /* root torque accumulator (reaction) */

    if (F32(U32(S, 0x5c30), 4) > 0.0f) {      /* brake: a force along the bike's local Z, weaker the faster it goes */
        const rx_v4 v = v4_ld(S + 0x100);
        const float speed_ws = phys_sqrt(bike_hsum_021(v4_mul(v, v)));
        float fade = phys_div(phys_sub(speed_ws, F32(0, G_BRAKE_SPEED)), 10.0f /* 0092fd88 */);
        if (fade >= 1.0f) fade = 1.0f;
        else if (0.0f >= fade) fade = 0.0f;
        const float z = phys_mul(phys_sub(1.0f, fade), -150.0f /* 009778f0 */);
        RX_FRAME_ENTER(F, 0x20);
        const rx_addr force = (F + 15) & ~15u;
        v4_st(force, v4(0.0f, 0.0f, z, z));
        force_add_local_force(S, 0, force);
        RX_FRAME_LEAVE(F, 0x20);
    }
}
RX_FUNCTION(0x00446340, bike_engine_drive_forces)

/* ==== 0043c590 BikeSuspension_SpringDamperForces ========================================================================== */
/* The two suspension struts (EDI = S; ret 0). The front strut joins the root to the front tire (body 2), the rear one to the rear
 * tire (body 3). Per strut, with q the root orientation:
 *   dir = q * axis * q^-1 (the strut axis in the world);  local = q^-1 * (tire position - root position) * q  (the tire in the root frame)
 *   dist = |local - anchor| (the rear anchor's Z and W are the constant -2.6)
 *   elastic = (dist - rest) * spring * dir;  flag low / high when dist < lower / upper < dist
 *   speed = (tire velocity - (root velocity + offset x root angular velocity)) . dir
 *   viscous = (-speed * damping) * dir, damping = the "compress" value when speed < 0 (all of G's lanes), else the "extend" one
 *   F = viscous + elastic acts on the tire (at G = the origin), -F on the root at the local lever `local`.
 * The front strut also leaves its viscous / elastic parts in the root frame in S+8f20 / S+8f30. */
typedef struct {
    uint32_t body, pos, vel, axis, anchor, rest, lower, upper, spring, damp_extend, damp_compress, flag_low, flag_high;
} susp_strut;
static const susp_strut SUSP_FRONT = { 2, 0x230, 0x240, 0x6e80, 0x6e70, 0x3e5c, 0x3f7c, 0x3fc4, 0x8aac, 0x8ab0, 0x8ab4, 0x89e8, 0x89ec };
static const susp_strut SUSP_REAR  = { 3, 0x2d0, 0x2e0, 0x77e0, 0x77d0, 0x400c, 0x4174, 0x426c, 0x8aa0, 0x8aa4, 0x8aa8, 0x8898, 0x889c };

/* one strut; the vectors the force calls read by address live in the game frame F (16-aligned): +00 reaction, +10 force, +20 lever */
static void suspension_strut(rx_addr S, const susp_strut* st, rx_addr F, rx_v4 q, rx_v4 root_pos, rx_v4 root_vel, rx_v4 ang_world,
                             int rear, rx_v4* viscous_out, rx_v4* elastic_out) {
    const rx_v4 dir = bike_rotate(q, v4_ld(S + st->axis));
    const rx_v4 offset = v4_sub(v4_ld(S + st->pos), root_pos);
    const rx_v4 local = bike_rotate_inv(q, offset);
    rx_v4 anchor = v4_ld(S + st->anchor);
    if (rear) anchor = v4(anchor.v[0], anchor.v[1], rx_bf(0xc0266666u) /* 009778a4: -2.6 */, rx_bf(0xc0266666u));   /* shufps 0xa4 */
    const rx_v4 delta = v4_sub(local, anchor);
    const float dist = phys_sqrt(bike_hsum_021(v4_mul(delta, delta)));
    const float stretch = phys_mul(phys_sub(dist, F32(S, st->rest)), F32(S, st->spring));
    const rx_v4 elastic = v4_mul(v4_splat(stretch), dir);
    if (dist < F32(S, st->lower)) SU32(S, st->flag_low, 1);
    else if (F32(S, st->upper) < dist) SU32(S, st->flag_high, 1);

    /* offset x angular velocity (shuffled products), as lanes 0..2, plus the root velocity */
    const rx_v4 cross = v4_sub(v4_mul(v4_perm(offset, 2, 0, 1, 3), v4_perm(ang_world, 1, 2, 0, 3)),
                               v4_mul(v4_perm(offset, 1, 2, 0, 3), v4_perm(ang_world, 2, 0, 1, 3)));
    const rx_v4 rel = v4_sub(v4_ld(S + st->vel), v4_add(v4_xyz(cross), root_vel));
    const float speed = phys_hsum3(v4_mul(rel, dir));
    const rx_v4 sp = v4_splat(speed);
    const float damping = F32(S, bike_all3(v4_cmplt(sp, bike_g_zero())) ? st->damp_compress : st->damp_extend);
    const rx_v4 viscous = v4_mul(v4_mul(v4_sub(v4_zero(), sp), v4_splat(damping)), dir);
    const rx_v4 force = v4_add(viscous, elastic);
    v4_st(F + 0x00, v4_sub(v4_zero(), force));
    v4_st(F + 0x10, force);
    v4_st(F + 0x20, local);
    force_add_force_at_point(S, F + 0x10, G_ZERO, st->body);     /* F on the tire */
    force_add_force_at_point(S, F + 0x00, F + 0x20, 0);          /* -F on the root at `local` */
    *viscous_out = viscous; *elastic_out = elastic;
}

void bike_suspension_spring_damper_forces(rx_addr S) {
    const rx_v4 root_vel = v4_ld(S + 0x100), root_pos = v4_ld(S + 0xf0);
    const rx_v4 q = v4_ld(S + 0x730);
    const rx_v4 ang_world = bike_rotate(q, v4_ld(S + 0x740));
    SU32(S, 0x89e8, 0); SU32(S, 0x8898, 0); SU32(S, 0x89ec, 0); SU32(S, 0x889c, 0);   /* travel limit flags */
    RX_FRAME_ENTER(Fr, 0x40);
    const rx_addr F = (Fr + 15) & ~15u;
    rx_v4 viscous, elastic;
    suspension_strut(S, &SUSP_FRONT, F, q, root_pos, root_vel, ang_world, 0, &viscous, &elastic);
    v4_st(S + 0x8f20, bike_rotate_inv(q, viscous));              /* front strut forces in the root frame (read by the audio / FX) */
    v4_st(S + 0x8f30, bike_rotate_inv(q, elastic));
    suspension_strut(S, &SUSP_REAR, F, q, root_pos, root_vel, ang_world, 1, &viscous, &elastic);
    RX_FRAME_LEAVE(Fr, 0x40);
}
RX_FUNCTION(0x0043c590, bike_suspension_spring_damper_forces)

/* ==== 0043efa0 BikeWheel_DragForces ======================================================================================= */
/* EDI = S; ret 0 (called with the rider on). Per wheel in contact (flags S+8890 rear / S+89e0 front) the drag along the wheel body's
 * velocity v: -(c1 * v/|v| + c2 * v) * k, with c1, c2 the indexed curves of the wheel's slip (rear S+4934 / S+4984, front S+42fc /
 * S+434c at S+88a4 / S+89f4) and k = clamp(load / 173.88, 0, 2) (load S+8870 / S+89c0). Root force = -v_root * S+2f00 + C+20 + both
 * drags; then the root and the two tire bodies receive forces (rider off: all zero): root the sum, rear / front tire (S+8a40 /
 * S+8a50) + G, through 0085c770 at the origin. */
static rx_v4 drag_wheel_force(rx_addr table_a, rx_addr table_b, float slip, rx_addr vel, float load) {
    const float c1 = bike_indexed_curve(table_a, slip);
    const float c2 = bike_indexed_curve(table_b, slip);
    const rx_v4 v = v4_ld(vel);
    const rx_v4 n = bike_normalize(v);
    const rx_v4 combined = v4_add(v4_mul(n, v4_sub(v4_zero(), v4_splat(c1))), v4_mul(v4_sub(v4_zero(), v4_splat(c2)), v));
    float k = phys_div(load, rx_bf(0x432de148u) /* 009778f4: 173.88 */);
    if (k >= 2.0f /* 0092e330 */) k = 2.0f;
    else if (0.0f >= k) k = 0.0f;
    return v4_mul(v4_splat(k), combined);
}

void bike_wheel_drag_forces(rx_addr S) {
    const rx_v4 g = bike_g_zero();
    rx_v4 rear_drag = g, front_drag = g;
    if (U32(S, 0x8890) != 0) rear_drag = drag_wheel_force(S + 0x4934, S + 0x4984, F32(S, 0x88a4), S + 0x2e0, F32(S, 0x8870));
    if (U32(S, 0x89e0) != 0) front_drag = drag_wheel_force(S + 0x42fc, S + 0x434c, F32(S, 0x89f4), S + 0x240, F32(S, 0x89c0));
    const rx_addr C = U32(S, 0x5c30);
    const int rider_off = U32(S, 0x2374) != 0;
    rx_v4 root_force = v4_mul(v4_sub(v4_zero(), v4_ld(S + 0x100)), v4_splat(F32(S, 0x2f00)));
    root_force = v4_add(root_force, v4_ld(C + 0x20));
    root_force = v4_add(root_force, rear_drag);
    root_force = v4_add(root_force, front_drag);
    rx_v4 rear_force = v4_add(v4_ld(S + 0x8a40), g), front_force = v4_add(v4_ld(S + 0x8a50), g);
    if (rider_off) {
        const rx_v4 zero = v4_splat(0.0f /* 0092e12c */);
        root_force = v4_mul(zero, root_force); rear_force = v4_mul(zero, rear_force); front_force = v4_mul(zero, front_force);
    }
    RX_FRAME_ENTER(Fr, 0x60);
    const rx_addr F = (Fr + 15) & ~15u;
    v4_st(F + 0x30, root_force); v4_st(F + 0x10, rear_force); v4_st(F + 0x20, front_force);
    force_add_force_at_point(S, F + 0x30, G_ZERO, 0);
    force_add_force_at_point(S, F + 0x10, G_ZERO, 3);
    force_add_force_at_point(S, F + 0x20, G_ZERO, 2);
    RX_FRAME_LEAVE(Fr, 0x60);
}
RX_FUNCTION(0x0043efa0, bike_wheel_drag_forces)

/* ==== 0043e100 RavenSimulationMotocross::vf11 ============================================================================= */
/* Slot 11, the force stage of a physics step (ECX = S, stack dt; ret 4). With the rider on: the shift timer S+8aec += dt, the gear
 * choice (up / auto while the throttle C+0 > 0, down-only otherwise; only when a wheel touches the ground, S+8d98 == 0), the engine,
 * the axle torques. Always the suspension; with the rider on the wheel drag. Then four vectors S+8f00 / 8f10 / 8ee0 / 8ef0 are
 * cleared to the zero vec4 G, the tire forces of the rear (S+8790) and front (S+88e0) wheel run, and S+8e60 keeps a copy of the root
 * torque accumulator S+780. */
void bike_force_stage(rx_addr S, float dt) {
    if (U32(S, 0x2374) == 0) {
        const float throttle = F32(U32(S, 0x5c30), 0);
        SF32(S, 0x8aec, phys_add(F32(S, 0x8aec), dt));
        const int airborne = U32(S, 0x8d98) != 0;
        if (throttle > 0.0f) { if (!airborne) bike_gear_auto_select(S); }
        else if (!airborne) bike_gear_select_downshift(S);
        bike_engine_drive_forces(S, dt);
        bike_wheels_apply_axle_torques(S);
    }
    bike_suspension_spring_damper_forces(S);
    if (U32(S, 0x2374) == 0) bike_wheel_drag_forces(S);
    v4_st(S + 0x8f00, bike_g_zero());
    v4_st(S + 0x8f10, bike_g_zero());
    v4_st(S + 0x8ee0, bike_g_zero());
    v4_st(S + 0x8ef0, bike_g_zero());
    rx_regs regs = force_regs();                      /* the original calls with ECX = &W, EDX = &W (front) / the mechanism (rear) */
    regs.r[1] = S + 0x8790; regs.r[2] = S + 0xd0;
    force_tire_contact_force(S, dt, S + 0x8790, &regs);   /* rear */
    regs.r[2] = S + 0x88e0;
    force_tire_contact_force(S, dt, S + 0x88e0, &regs);   /* front (ECX, EAX: the rear call's leftovers) */
    v4_st(S + 0x8e60, v4_ld(S + 0x780));
}
RX_FUNCTION(0x0043e100, bike_force_stage)
