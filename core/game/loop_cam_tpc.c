/* loop_cam_tpc.c -- MXThirdPersonCamera::vf19 0064c110 (vtable 0095a8a4), the chase camera of the race: the per-frame update that
 * places the gameplay camera Unity draws (game-camera.md). Its pieces (orbit pose, water / ground clearance, water-ahead probe,
 * the smoothing springs) are in loop_cam_third.c / loop_cam_util.c.
 *
 * Camera `cam` (an MXPlayerCamera, loop_cam_player.c): +0x344 the controller C (C +4 local, +8 viewport, +0x10 kind, +0x94 the
 * activity, +0x1ac the followed vehicle V, +0x1f0 the player index), +0x350 / +0x35c two angle springs, +0x380 the last target
 * (+0x390 kept), +0x391 follow the velocity, +0x3b4 the yaw spring {pos, target +0x3b8, speed}, +0x3f8 the height offset spring,
 * +0x404 the pitch spring, +0x410 the steer-look spring (+0x41c its hold clock), +0x420 / +0x42c the turn-lean springs, +0x438
 * the side-slip spring (+0x43c its target), +0x444 the backwards flag spring, +0x450 the water spring, +0x45c the roll spring,
 * +0x468 an angle spring in degrees, +0x474 the look-ahead spring, +0x490 the vector spring (ground lift), +0x4c0 the fov
 * transition spring, +0x3c0..+0x3f0 the cut-in transition (+0x3f4 active), +0x500.. the tuning (offsets, rates, curves).
 * V: vf17 (0x44) position, +0x130 forward, vf37 (0x94) the velocity, vf43 (0xac) "on a rail", +0x190 the input object, +0x194
 * the class, +0x170 the subclass, +0x280 "attached" (the follow point is an attachment node, 006468a0).
 *
 * Frame: push ebp; and esp, -16; sub esp, 0x174 (the security cookie at +0x170); push ebx / esi / edi: E = the aligned base -
 * 0x180; the locals below are at E + their offset, as the original's (several are reused: see the comments at each use). */
#include "loop_cam.h"
#include "loop_cam_entity.h"
#include "loop_cam_player.h"
#include "loop_cam_tpc.h"
#include "loop_cam_shake.h"
#include "loop_cam_misc.h"
#include "loop_util.h"
#include "anim_math.h"
#include "crt.h"
#include "world_veh.h"
#include "world_misc.h"
#include "world_wreck.h"
#include "bike_curve.h"
#include "phys_misc.h"
#include "race_activity.h"
#include "race_driver.h"

/* ---- the callees other subsystems own (their core functions, at the original's stack depth) */
static float tpc_atan2f(float y, float x, rx_addr ret) {      /* 0041c4b0 (cdecl y, x; st(0)): _CIatan2 rounded to float (fx) */
    (void)ret;
    return rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y), rx_f80_from_f32(x), CRT_CW_MAIN), 0);
}
static rx_addr tpc_active_camera_entity(rx_addr player, rx_addr ret) {     /* 0054c020 (EDI = player) -> EAX (rx_world) */
    const rx_addr s = loop_push(0, 0, ret); const rx_addr r = rx_world_player_active_camera_entity(player); loop_pop(s); return r;
}
static rx_addr tpc_player_driver(rx_addr player, rx_addr act, rx_addr ret) {   /* 00559fd0 (EAX = player, EDI = activity) (rx_race) */
    const rx_addr s = loop_push(0, 0, ret); const rx_addr r = rx_activity_player_driver(player, act); loop_pop(s); return r;
}
static uint32_t tpc_mode_index(rx_addr cam, rx_addr ret) {     /* 006467f0 Camera_ModeIndex (EAX = camera) (rx_world) */
    const rx_addr s = loop_push(0, 0, ret); const uint32_t r = rx_world_camera_mode_index(cam); loop_pop(s); return r;
}
static float tpc_param_curve(rx_addr curve, float x, rx_addr ret) {   /* 00438ee0 ParamCurve_EvalCubic (ESI = curve, xmm0 = x) (rx_bike) */
    const rx_addr s = loop_push(0, 0, ret); const float r = bike_param_curve(curve, x); loop_pop(s); return r;
}
static float tpc_lerp_clamped(float t, float a, float b, rx_addr ret) {   /* 00430810 Lerp_Clamped (xmm2 = t; cdecl a, b) (rx_bike) */
    const uint32_t v[2] = {cam_x87_bits(rx_fb(a)), cam_x87_bits(rx_fb(b))};
    const rx_addr s = loop_push(2, v, ret); const float r = bike_lerp_clamped(t, rx_bf(v[0]), rx_bf(v[1])); loop_pop(s); return r;
}
static void tpc_lag_set(rx_addr filt, float value, float tau, rx_addr ret) {   /* 0042d670 (ECX = filter, xmm1 = value; stdcall tau) (rx_phys) */
    const uint32_t v[1] = {cam_x87_bits(rx_fb(tau))};
    const rx_addr s = loop_push(1, v, ret); phys_lag_filter_set(filt, value, rx_bf(v[0])); loop_pop(s);
}
static float tpc_lag_update(rx_addr filt, float dt, float target, rx_addr ret) {   /* 0042d6b0 (EAX = filter, xmm2 = dt; stdcall target) (rx_phys) */
    const uint32_t v[1] = {cam_x87_bits(rx_fb(target))};
    const rx_addr s = loop_push(1, v, ret); const float r = phys_lag_filter_update(filt, dt, rx_bf(v[0])); loop_pop(s); return r;
}
static float tpc_remap(float in_lo, float in_hi, float value, float out_lo, float out_hi, rx_addr ret) {   /* 00424370 (xmm0, xmm1; cdecl value, out lo, out hi) (rx_phys) */
    const uint32_t v[3] = {cam_x87_bits(rx_fb(value)), rx_fb(out_lo), cam_x87_bits(rx_fb(out_hi))};
    const rx_addr s = loop_push(3, v, ret); const float r = phys_remap_clamped(in_lo, in_hi, rx_bf(v[0]), rx_bf(v[1]), rx_bf(v[2])); loop_pop(s); return r;
}
static void tpc_cross(rx_addr out, rx_addr c, rx_addr a, rx_addr ret) {     /* 00401630 Vec3_Cross (EAX = a, ECX = c; cdecl out) (rx_world) */
    const uint32_t v[1] = {out};
    const rx_addr s = loop_push(1, v, ret); rx_world_vec3_cross(out, c, a); loop_pop(s);
}
static void tpc_mat_to_quat(rx_addr out, rx_addr m, rx_addr ret) {      /* 00401740 Matrix3_ToQuaternionToEsi (ESI = out; cdecl m) (rx_world) */
    const uint32_t v[1] = {m};
    const rx_addr s = loop_push(1, v, ret); rx_world_matrix3_to_quaternion_to(out, m); loop_pop(s);
}

/* ---- this subsystem's callees, at the original's stack depth ---- */
static void tpc_angle_spring(rx_addr s, float dt, float accel, float max, float k, rx_addr ret) {   /* 0063c7c0 (ESI = spring) */
    const uint32_t v[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(rx_fb(accel)), cam_x87_bits(rx_fb(max)), cam_x87_bits(rx_fb(k))};
    const rx_addr sp = loop_push(4, v, ret);
    rx_cam_angle_spring(s, rx_bf(v[0]), rx_bf(v[1]), rx_bf(v[2]), rx_bf(v[3]));
    loop_pop(sp);
}
static void tpc_spring_call(rx_addr s, float dt, float accel, float max, float k, rx_addr ret) {    /* 0063c8c0 (ESI = spring) */
    const uint32_t v[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(rx_fb(accel)), cam_x87_bits(rx_fb(max)), cam_x87_bits(rx_fb(k))};
    const rx_addr sp = loop_push(4, v, ret);
    rx_cam_spring(s, rx_bf(v[0]), rx_bf(v[1]), rx_bf(v[2]), rx_bf(v[3]));
    loop_pop(sp);
}
static void tpc_vec3_spring(rx_addr s, float dt, float accel, float max, float k, rx_addr ret) {    /* 0063c9b0 (ESI = spring) */
    const uint32_t v[4] = {cam_x87_bits(rx_fb(dt)), cam_x87_bits(rx_fb(accel)), cam_x87_bits(rx_fb(max)), cam_x87_bits(rx_fb(k))};
    const rx_addr sp = loop_push(4, v, ret);
    rx_cam_vec3_spring(s, rx_bf(v[0]), rx_bf(v[1]), rx_bf(v[2]), rx_bf(v[3]));
    loop_pop(sp);
}
static void tpc_orbit_pose(rx_addr cam, rx_addr dir, uint32_t side, rx_addr target, rx_addr pos, rx_addr look, rx_addr ret) {   /* 006502e0 */
    const uint32_t v[4] = {side, target, pos, look};
    const rx_addr sp = loop_push(4, v, ret);
    rx_cam_orbit_pose(cam, dir, side, target, pos, look);
    loop_pop(sp);
}
static void tpc_set_pose(rx_addr cam, rx_addr m, rx_addr ret) {            /* 0063dee0 (stdcall entity, matrix) */
    const uint32_t v[2] = {cam, m};
    const rx_addr sp = loop_push(2, v, ret);
    rx_cam_entity_set_pose(cam, m);
    loop_pop(sp);
}
/* 00508b60 (EAX = spring -> xmm0 its value) / 00508b70 (EAX = spring, xmm0: its target +4 = value): trivial leaves (rx_race) */
static float tpc_value(rx_addr s) { return F32(s, 0); }
static void tpc_set_target(rx_addr s, float v) { SF32(s, 4, v); }

/* The inlined spring {pos, target, vel} (as rx_cam_spring): step = the original's acceleration * dt product, speed limit
 * min(|target - pos| * k, 999999 = 0x977aa8), snapping to the target when the step passes it. The original keeps the target, the
 * sign and the new speed in stack locals (`s_tgt`, `s_sign`, `s_v`). */
static void tpc_spring(rx_addr cam, uint32_t pos, uint32_t tgt, uint32_t vel, float step, float k, float dt,
                       rx_addr s_tgt, rx_addr s_sign, rx_addr s_v) {
    const float target = F32(cam, tgt);
    SF32(s_tgt, 0, target);
    const float d = target - F32(cam, pos);
    const float sign = (0.0f > d) ? F32(0, 0x009cd9e8) : 1.0f /* 0x931f24 */;
    const float v = (step * sign) + F32(cam, vel);
    SF32(s_sign, 0, sign);
    SF32(s_v, 0, v);
    SF32(cam, vel, v);
    float lim = fabsf(d) * k;
    if (lim > 999999.0f /* 0x977aa8 */) lim = 999999.0f;
    if (F32(s_v, 0) * F32(s_sign, 0) > lim) SF32(cam, vel, lim * F32(s_sign, 0));
    const float p = (F32(cam, vel) * dt) + F32(cam, pos);
    SF32(cam, pos, p);
    if (0.0f > (F32(s_tgt, 0) - p) * F32(s_sign, 0)) {
        SF32(cam, pos, F32(s_tgt, 0));
        SF32(cam, vel, 0.0f);
    }
}

/* the vehicle's "on a rail" (vf43, slot 0xac) */
static uint32_t tpc_on_rail(rx_addr cam) {
    const rx_addr v = U32(U32(cam, 0x344), 0x1ac);
    return loop_tc0(loop_vslot(v, 0xac), v);
}
static uint32_t tpc_attached(rx_addr cam) { return U32(U32(U32(cam, 0x344), 0x1ac), 0x280); }

/* normalize the vec3 at `src` into `dst` (each component times 1 / sqrt(l) when l = ((x x + y y) + z z) > 0; l kept at `sl`) */
static void tpc_normalize(rx_addr dst, rx_addr src, rx_addr sl) {
    const float x = F32(src, 0), y = F32(src, 4), z = F32(src, 8);
    const float l = ((x * x) + (y * y)) + (z * z);
    SF32(sl, 0, l);
    if (l > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
        SF32(dst, 0, F32(src, 0) * inv);
        SF32(dst, 4, F32(src, 4) * inv);
        SF32(dst, 8, F32(src, 8) * inv);
    }
}

/* right = the world up (.data 0x9cb4b0) crossed with the view direction D (at E + 0x50): stored at +0x60 and +0x30, its squared
 * length at +0x20, normalized into +0x30 */
static void tpc_side_axis(rx_addr E) {
    const float dz = F32(E, 0x58), dy = F32(E, 0x54), dx = F32(E, 0x50);
    const float uy = F32(0, 0x009cb4b4), uz = F32(0, 0x009cb4b8), ux = F32(0, 0x009cb4b0);
    const float rx = (dz * uy) - (dy * uz);
    const float rz = (dy * ux) - (uy * dx);
    const float ry = (uz * dx) - (dz * ux);
    SF32(E, 0x64, ry); SF32(E, 0x34, ry);
    SF32(E, 0x60, rx); SF32(E, 0x30, rx);
    const float l = ((ry * ry) + (rx * rx)) + (rz * rz);
    SF32(E, 0x68, rz); SF32(E, 0x38, rz);
    SF32(E, 0x20, l);
    if (l > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l);
        SF32(E, 0x30, F32(E, 0x60) * inv);
        SF32(E, 0x34, F32(E, 0x64) * inv);
        SF32(E, 0x38, F32(E, 0x68) * inv);
    }
}

/* the yaw's sine / cosine against the velocity: |sin(yaw) v.x + cos(yaw) v.z| / 84 (at most 1) (v = E + 0x30) */
static float tpc_yaw_slip(rx_addr E, float yaw) {
    const float d = (cam_sin(yaw) * F32(E, 0x30)) + (cam_cos(yaw) * F32(E, 0x38));
    const float a = fabsf(d);
    return (84.0f /* 0x977dfc */ > a) ? a / 84.0f : 84.0f / 84.0f;
}

/* 00650210 (LTCG: EAX = camera): the water-ahead probe reset: unless steep water was seen (+0x5ec) the water spring's target
 * +0x454 is zeroed; the probe clock +0x5cc, the frame count +0x5e0, the sums +0x5e4 / +0x5e8 and the flag +0x5ec cleared. */
void rx_cam_water_reset(rx_addr cam) {
    if (U8(cam, 0x5ec) == 0) SF32(cam, 0x454, 0.0f);
    SF32(cam, 0x5cc, 0.0f);
    SU32(cam, 0x5e0, 0);
    SF32(cam, 0x5e4, 0.0f);
    SF32(cam, 0x5e8, 0.0f);
    SU8(cam, 0x5ec, 0);
}
RX_FUNCTION(0x00650210, rx_cam_water_reset)

/* 0064c110 part 5 (0064d297..0064dc1d): the orbit pose (position +0xa0, look point +0x60), the view direction N (+0x80) and the
 * horizontal direction to the target G; the roll target (+0x45c) from the angle between them beyond the tuning's dead zone
 * (+0x564 / +0x568 degrees by the side, at most +0x56c / +0x570, scaled by -1.3 / 2); the roll, ground lift and fov springs; a
 * cut (+0x340) snaps every spring. */
static void tpc_view_roll(rx_addr cam, float dt, rx_addr E) {
    tpc_orbit_pose(cam, E + 0x50u, 0, E + 0xc0u, E + 0xa0u, E + 0x60u, 0x0064d2c1);
    const float dx = F32(E, 0x60) - F32(E, 0xa0), dy = F32(E, 0x64) - F32(E, 0xa4), dz = F32(E, 0x68) - F32(E, 0xa8);
    SF32(E, 0x94, dy);
    const float l = ((dy * dy) + (dx * dx)) + (dz * dz);
    SF32(E, 0x1c, dx); SF32(E, 0x90, dx); SF32(E, 0x2c, dz); SF32(E, 0x98, dz); SF32(E, 0x20, l);
    float nz;
    if (l > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l);
        SF32(E, 0x80, F32(E, 0x90) * inv);
        SF32(E, 0x84, F32(E, 0x94) * inv);
        nz = F32(E, 0x98) * inv;
        SF32(E, 0x88, nz);
    } else nz = F32(E, 0x88);
    const float h = F32(E, 0xc4) - F32(E, 0xa4);
    const float l2 = ((dx * dx) + (h * h)) + (dz * dz);
    SF32(E, 0x60, dx); SF32(E, 0x64, h); SF32(E, 0x68, dz); SF32(E, 0x20, l2);
    float gx, gy, gz;
    if (l2 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l2);
        gx = F32(E, 0x60) * inv; gy = F32(E, 0x64) * inv; gz = F32(E, 0x68) * inv;
        nz = F32(E, 0x88);
        SF32(E, 0x90, gx); SF32(E, 0x94, gy); SF32(E, 0x98, gz);
    } else {
        gz = F32(E, 0x98); gy = F32(E, 0x94); gx = F32(E, 0x90);
    }
    const float l3 = ((gx * gx) + (gy * gy)) + (gz * gz);
    SF32(E, 0x20, l3);
    if (l3 > 0.0f) {
        const float inv = 1.0f / rx_sqrts(l3);
        gz = F32(E, 0x98) * inv;
        nz = F32(E, 0x88);
        gx = inv * F32(E, 0x90);
        gy = F32(E, 0x94) * inv;
    }
    const float nx = F32(E, 0x80), ny = F32(E, 0x84);
    const float cx = (gz * ny) - (gy * nz);
    const float cy = (nz * gx) - (gz * nx);
    const float cz = (gy * nx) - (ny * gx);
    SF32(E, 0x60, cx); SF32(E, 0x64, cy); SF32(E, 0x68, cz);
    const float ny0 = ny * 0.0f;
    SF32(E, 0x20, ny0);
    const float det = ((((nz * 0.0f) - (nx * 0.0f)) * cy) + ((ny0 - nz) * cx)) + ((nx - ny0) * cz);
    const float dot = ((gy * ny) + (gx * nx)) + (gz * nz);
    SF32(E, 0x18, dot);
    const int left = 0.0f > det;
    const float a1 = F32(cam, left ? 0x568 : 0x564) * 0.017453291f /* 0x977678 */;
    SF32(E, 0x1c, a1);
    if (cam_cos(a1) > dot) {
        SF32(E, 0x20, fabsf(dot));
        float x = anim_acos_approx(dot) - F32(E, 0x1c);
        const float m = F32(cam, left ? 0x570 : 0x56c) * 0.017453291f;
        if (!(m > x)) x = m;
        x = x * (left ? 2.0f /* 0x92e330 */ : -1.3f /* 0x9778b4 */);
        tpc_set_target(cam + 0x45cu, x);
    }
    tpc_angle_spring(cam + 0x45cu, dt, 5.2359872f /* 0x977df8 */, 999999.0f, 4.0f /* 0x9322c0 */, 0x0064dada);
    tpc_vec3_spring(cam + 0x490u, dt, F32(cam, 0x480), F32(cam, 0x484), F32(cam, 0x488), 0x0064db0c);
    tpc_angle_spring(cam + 0x4c0u, dt, 1.0f, 9999.0f /* 0x977df4 */, 1.0f, 0x0064db36);
    if (U8(cam, 0x340) != 0) {
        rx_cam_water_reset(cam);
        static const uint16_t snap1[] = {0x3b4, 0x350, 0x35c, 0x3f8};
        for (unsigned i = 0; i < 4; i++) rx_cam_spring_snap(cam + snap1[i]);
        tpc_set_target(cam + 0x404u, 0.0f); rx_cam_spring_snap(cam + 0x404u);
        tpc_set_target(cam + 0x410u, 0.0f); rx_cam_spring_snap(cam + 0x410u);
        static const uint16_t snap2[] = {0x420, 0x42c, 0x438, 0x444};
        for (unsigned i = 0; i < 4; i++) rx_cam_spring_snap(cam + snap2[i]);
        tpc_set_target(cam + 0x450u, 0.0f); rx_cam_spring_snap(cam + 0x450u);
        rx_cam_spring_snap(cam + 0x468u);
        rx_cam_spring_snap(cam + 0x474u);
        tpc_set_target(cam + 0x4c0u, F32(0, 0x009cd9e8)); rx_cam_spring_snap(cam + 0x4c0u);
        SU32(cam, 0x3f4, 0);
    }
}

/* 0064c110 part 6 (0064dc1d..0064f224): the final pose (see the function comment). */
static void tpc_frame(rx_addr cam, float dt, rx_addr E) {
    tpc_orbit_pose(cam, E + 0x50u, 0, E + 0xc0u, E + 0x80u, E + 0x90u, 0x0064dc42);
    /* the speed: x87 (v.y * v.y + v.x^2) + v.z^2 at the thread's precision, stored as a float */
    {
        const rx_f80 vy = rx_f80_from_f32(F32(E, 0x34));
        rx_f80 q = rx_f80_mul(vy, vy, CAM_X87_BITS, 0);
        q = rx_f80_add(q, rx_f80_from_f32(F32(E, 0xb8)), CAM_X87_BITS, 0);
        q = rx_f80_add(q, rx_f80_from_f32(F32(E, 0x7c)), CAM_X87_BITS, 0);
        SF32(E, 0x2c, rx_f80_to_f32(q, 0));
    }
    const float speed = rx_sqrts(F32(E, 0x2c));
    SF32(E, 0x20, speed);
    float ratio = (speed - F32(0, 0x00d94ac8)) / F32(0, 0x009d7b34);
    if (ratio >= 0.5f /* 0x9322cc */) ratio = 0.5f;
    else if (!(0.0f < ratio)) { if (!(ratio != ratio)) ratio = 0.0f; }
    const float r2 = ratio * ratio;
    SF32(E, 0x4c, r2);
    if (U8(cam, 0x340) != 0) tpc_lag_set(cam + 0x5b0u, r2, 0.5f /* 0x9322cc */, 0x0064dcc3);
    else SF32(E, 0x4c, tpc_lag_update(cam + 0x5b0u, dt, F32(E, 0x4c), 0x0064dcdc));
    /* the fov of the tuning curve (class, mode, camera index) at the speed */
    {
        const rx_addr v = U32(U32(cam, 0x344), 0x1ac);
        uint32_t cls = U32(v, 0x194);
        if (cls == 3) cls = (U32(v, 0x170) != 8 ? 5u : 0u) + 3u;
        const uint32_t mode = U32(U32(0, 0x00a946b8), 0x15b24);
        const uint32_t m2 = (mode == 2) ? 1u : 0u;
        const uint32_t idx = tpc_mode_index(cam, 0x0064dd30);
        const rx_addr curve = (m2 + idx * 2u) * 160u + cls * 0x1da80u + 0x00afa97cu;
        SF32(E, 0x18, tpc_param_curve(curve, F32(E, 0x20), 0x0064dd51) * 0.017453291f /* 0x977678 */);
        const int32_t vi = I32(U32(cam, 0x344), 8);
        if (vi != -1) {
            const rx_addr ctx = U32(0, 0x00a95844);
            const rx_addr scene = U32(ctx, 0x1c + 4u * U32(ctx, 4));
            if (vi < I32(scene, 0x19d0)) {
                const rx_addr o = U32(U32(scene, 0x19b8 + 4u * (uint32_t)vi), 0x64);
                if (o != 0) rx_cam_set_float_254(o, 0.1f /* 0x931d94 */);
            }
        }
    }
    /* the position pulled toward the look point by the speed factor */
    {
        const float lx = F32(E, 0x90), ly = F32(E, 0x94), lz = F32(E, 0x98);
        SF32(cam, 0x370, lx); SF32(cam, 0x374, ly); SF32(cam, 0x378, lz);
        const float dx = lx - F32(E, 0x80), dy = ly - F32(E, 0x84), dz = lz - F32(E, 0x88);
        SF32(E, 0x50, dx); SF32(E, 0x54, dy); SF32(E, 0x58, dz);
        const float l = ((dx * dx) + (dy * dy)) + (dz * dz);
        SF32(E, 0x78, l);
        float dist = rx_sqrts(l);
        SF32(E, 0x1c, dist);
        SF32(E, 0x20, l);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            SF32(E, 0x50, F32(E, 0x50) * inv);
            SF32(E, 0x54, F32(E, 0x54) * inv);
            SF32(E, 0x58, F32(E, 0x58) * inv);
            dist = F32(E, 0x1c);
        }
        const float k = F32(E, 0x4c);
        const float py = (((F32(E, 0x54) * k) * dist) * 0.33333334f /* 0x92e214 */) + F32(E, 0x84);
        const float pz = (((F32(E, 0x58) * k) * dist) * 0.33333334f) + F32(E, 0x88);
        const float px = F32(E, 0x80) + ((dist * (k * F32(E, 0x50))) * 0.33333334f);
        SF32(E, 0x80, px); SF32(E, 0x84, py); SF32(E, 0x88, pz);
    }
    {
        const float l = F32(E, 0x2c);
        SF32(E, 0x20, l);
        if (l > 0.0f) SF32(E, 0xa4, (1.0f / rx_sqrts(l)) * F32(E, 0x34));
        SF32(E, 0x7c, rx_sqrts(((0.0f * 0.0f) + F32(E, 0xb8)) + F32(E, 0x7c)));
    }
    float side0 = 0.0f;   /* xmm1 after the pitch block */
    /* the pitch: the slope spring +0x404 tilts the view direction about the side axis */
    if (U8(cam, 0x594) != 0 && tpc_attached(cam) == 0) {
        const rx_addr ps = cam + 0x404u;
        float x = 0.0f;
        if (F32(E, 0x7c) > F32(cam, 0x5a0) && F32(cam, 0x598) > F32(E, 0x34))
            x = tpc_remap(F32(cam, 0x59c), -0.707f /* 0x977df0 */, F32(E, 0xa4), 0.0f, F32(cam, 0x5a4), 0x0064dffe);
        tpc_set_target(ps, x);
        SF32(E, 0x20, rx_cam_spring_target(ps));
        const float cur = tpc_value(ps);
        const float accel = (cur > F32(E, 0x20)) ? F32(cam, 0x5ac) : F32(cam, 0x5a8);
        tpc_spring_call(ps, dt, accel, 9999999.0f /* 0x977d64 */, 10.0f /* 0x92fd88 */, 0x0064e066);
        tpc_side_axis(E);
        const float a = tpc_value(ps) * -0.017453291f /* 0x977dec */;
        float sn, cs;
        anim_sincos(a, &sn, &cs);
        SF32(E, 0x1c, cs);
        SF32(E, 0x20, sn);
        tpc_cross(E + 0xa0u, E + 0x30u, E + 0x50u, 0x0064e287);
        tpc_cross(E + 0x60u, E + 0xa0u, E + 0x30u, 0x0064e29f);
        const float rnx = F32(E, 0x30), rny = F32(E, 0x34), rnz = F32(E, 0x38);
        const float d = ((rnx * F32(E, 0x50)) + (rny * F32(E, 0x54))) + (rnz * F32(E, 0x58));
        const float t3 = d * rnx;
        SF32(E, 0x34, rny * d);
        SF32(E, 0x38, rnz * d);
        const float s = F32(E, 0x20), c = F32(E, 0x1c);
        const float c1x = F32(E, 0xa0) * s, c1y = F32(E, 0xa4) * s, c1z = F32(E, 0xa8) * s;
        const float nx = ((F32(E, 0x60) * c) + c1x) + t3;
        const float ny = ((F32(E, 0x64) * c) + c1y) + F32(E, 0x34);
        const float nz = ((F32(E, 0x68) * c) + c1z) + F32(E, 0x38);
        SF32(E, 0x60, nx); SF32(E, 0x64, ny); SF32(E, 0x50, nx); SF32(E, 0x54, ny);
        SF32(E, 0x68, nz); SF32(E, 0x58, nz);
        tpc_normalize(E + 0x50u, E + 0x60u, E + 0x20u);
    }
    /* the steer look: the stick (005487d0) beyond the dead zone +0x57c, scaled by the speed above +0x58c, held +0x590 s */
    if (U8(cam, 0x578) != 0 && tpc_attached(cam) == 0) {
        const rx_addr v = U32(U32(cam, 0x344), 0x1ac);
        const rx_addr in = U32(v, 0x190);
        const uint32_t player = U32(U32(cam, 0x344), 0x1f0);
        SF32(E, 0xb8, side0);
        if (U32(in, 4) != 0 && U32(in, 0x10) == 0) {
            const uint32_t a[1] = {E + 0xb8u};
            const rx_addr s = loop_push(1, a, 0x0064e453);
            rx_player_stick7(player, E + 0xb8u);
            loop_pop(s);
        }
        const float dz = F32(cam, 0x57c), mx = F32(cam, 0x580);
        SF32(E, 0x20, dz); SF32(E, 0x1c, mx);
        float x;
        if (dz != 1.0f /* 0x931f24 */) {
            const float r = (float)(((double)fabsf(F32(E, 0xb8)) - (double)F32(E, 0x20)) / (1.0 - (double)F32(E, 0x20)));
            const float lo = (mx > 0.0f) ? 0.0f : mx;
            const float hi = (0.0f > mx) ? 0.0f : mx;
            float v2 = (float)((((double)mx - 0.0) * (double)r) + 0.0);
            if (!(v2 > lo)) v2 = lo;
            x = (hi > v2) ? v2 : hi;
        } else x = 0.0f;
        x = x * ((F32(E, 0xb8) >= 0.0f) ? 1.0f : F32(0, 0x009cd9e8));
        SF32(E, 0x20, x);
        float sf = 0.0f;
        const float base = F32(cam, 0x58c);
        if ((double)base != 100.0 /* 0x977b80 */) {
            const float q = (float)(((double)F32(E, 0x7c) - (double)base) / (100.0 - (double)base));
            const float q2 = (float)(((double)q * (1.0 /* 0x977638 */ - 0.0)) + 0.0);
            if (q2 > 0.0f) sf = (1.0f > q2) ? q2 : 1.0f;
        }
        SF32(E, 0x4c, sf);
        const rx_addr ls = cam + 0x410u;
        if (tpc_on_rail(cam) == 0 && fabsf(F32(E, 0xb8)) > F32(cam, 0x57c)) {
            const float hold = F32(cam, 0x41c) + dt;
            SF32(cam, 0x41c, hold);
            if (hold > F32(cam, 0x590)) {
                tpc_set_target(ls, F32(E, 0x4c) * F32(E, 0x20));
                SU32(E, 0x7c, U32(cam, 0x584));
                SF32(E, 0x20, rx_cam_spring_target(ls));
                SF32(E, 0x20, F32(E, 0x20) - tpc_value(ls));
                if (0.0f > F32(E, 0x20) * rx_cam_spring_speed(ls)) SF32(E, 0x7c, 2.0f /* 0x92e330 */);
                tpc_spring_call(ls, dt, F32(E, 0x7c), 999999.0f, 10.0f /* 0x92fd88 */, 0x0064e6e3);
            }
        } else {
            SF32(cam, 0x41c, 0.0f);
            tpc_set_target(ls, 0.0f);
            tpc_spring_call(ls, dt, F32(cam, 0x588), 999999.0f, 10.0f, 0x0064e6e3);
        }
        tpc_side_axis(E);
        const float side = tpc_value(ls);
        const float x2 = (F32(E, 0x30) * side) + F32(E, 0x50);
        const float y2 = (F32(E, 0x34) * side) + F32(E, 0x54);
        const float z2 = (F32(E, 0x38) * side) + F32(E, 0x58);
        SF32(E, 0x60, x2); SF32(E, 0x50, x2);
        SF32(E, 0x64, y2); SF32(E, 0x68, z2); SF32(E, 0x54, y2); SF32(E, 0x58, z2);
        tpc_normalize(E + 0x50u, E + 0x60u, E + 0x20u);
    }
    /* the cut-in transition (+0x3f4): position / look point / fov blended from the stored ones */
    if (U32(cam, 0x3f4) != 0) {
        if (F32(cam, 0x3cc) >= 0.5f /* 0x9322cc */) {
            const float v = F32(cam, 0x3c0) - dt;
            SF32(cam, 0x3c0, v);
            if (0.2f /* 0x92e388 */ > v) SF32(cam, 0x3c0, 0.2f);
        } else {
            const float v = dt + F32(cam, 0x3c0);
            SF32(cam, 0x3c0, v);
            if (v > 1.0f) SF32(cam, 0x3c0, 1.0f);
        }
        const float w = (dt * F32(cam, 0x3c0)) + F32(cam, 0x3c8);
        SF32(cam, 0x3c8, w);
        const float m = F32(cam, 0x3c4);
        SF32(E, 0x7c, m);
        if (w > m) SF32(cam, 0x3c8, m);
        const float c8 = F32(cam, 0x3c8);
        SF32(E, 0x1c, c8);
        const float r = c8 / m;
        SF32(E, 0x4c, r);
        SF32(cam, 0x3cc, r);
        SF32(E, 0x80, tpc_lerp_clamped(r, F32(cam, 0x3d0), F32(E, 0x80), 0x0064e986));
        SF32(E, 0x84, tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3d4), F32(E, 0x84), 0x0064e9ae));
        SF32(E, 0x88, tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3d8), F32(E, 0x88), 0x0064e9d6));
        SF32(E, 0x90, tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3e0), F32(E, 0x90), 0x0064e9fe));
        SF32(E, 0x94, tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3e4), F32(E, 0x94), 0x0064ea26));
        const float lz = tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3e8), F32(E, 0x98), 0x0064ea4e);
        const float dz = lz - F32(E, 0x88), dx = F32(E, 0x90) - F32(E, 0x80), dy = F32(E, 0x94) - F32(E, 0x84);
        const float l = ((dx * dx) + (dy * dy)) + (dz * dz);
        SF32(E, 0x60, dx); SF32(E, 0x50, dx);
        SF32(E, 0x64, dy); SF32(E, 0x68, dz); SF32(E, 0x54, dy); SF32(E, 0x58, dz);
        SF32(E, 0x20, l);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            SF32(E, 0x50, F32(E, 0x60) * inv);
            SF32(E, 0x54, F32(E, 0x64) * inv);
            SF32(E, 0x58, F32(E, 0x68) * inv);
        }
        SF32(E, 0x18, tpc_lerp_clamped(F32(E, 0x4c), F32(cam, 0x3f0), F32(E, 0x18), 0x0064eb3f));
        if (F32(E, 0x1c) == F32(E, 0x7c)) {
            SU32(cam, 0x3f4, 0);
            tpc_set_target(cam + 0x4c0u, F32(0, 0x009cd9e8));
        }
    } else {
        const rx_addr fs = cam + 0x4c0u;
        const float t = rx_cam_spring_target(fs);
        const float fov = F32(E, 0x18);
        if (0.0f > t) {
            tpc_set_target(fs, fov);
            rx_cam_spring_snap(fs);
        } else {
            tpc_set_target(fs, fov);
            SF32(E, 0x18, tpc_value(fs));
        }
    }
    {
        const rx_addr s = loop_push(0, 0, 0x0064ebc6);
        rx_cam_set_fov(cam + 0x90u, F32(E, 0x18));
        loop_pop(s);
    }
    /* the shake offset; the basis from D and the world up */
    SF32(E, 0x80, F32(cam, 0x740) + F32(E, 0x80));
    SF32(E, 0x84, F32(cam, 0x744) + F32(E, 0x84));
    SF32(E, 0x88, F32(cam, 0x748) + F32(E, 0x88));
    SF32(E, 0xa0, 0.0f); SF32(E, 0xa8, 0.0f); SF32(E, 0x30, 0.0f); SF32(E, 0x38, 0.0f);
    SU32(E, 0x90, U32(E, 0x50)); SU32(E, 0x94, U32(E, 0x54));
    SF32(E, 0xa4, 1.0f); SF32(E, 0x34, 1.0f);
    SU32(E, 0x98, U32(E, 0x58));
    tpc_cross(E + 0x60u, E + 0xa0u, E + 0x50u, 0x0064ec8e);
    tpc_cross(E + 0x30u, E + 0x50u, E + 0x60u, 0x0064eca3);
    {
        const float x = F32(E, 0x30), y = F32(E, 0x34), z = F32(E, 0x38);
        const float l = ((y * y) + (x * x)) + (z * z);
        SF32(E, 0x20, l);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            SF32(E, 0x30, inv * F32(E, 0x30));
            const float y1 = inv * F32(E, 0x34), z1 = inv * F32(E, 0x38);
            SF32(E, 0x34, y1); SF32(E, 0x38, z1);
        }
    }
    {
        const float x = F32(E, 0x50), y = F32(E, 0x54), z = F32(E, 0x58);
        const float l = ((y * y) + (x * x)) + (z * z);
        SF32(E, 0x20, l);
        if (l > 0.0f) {
            const float inv = 1.0f / rx_sqrts(l);
            SF32(E, 0x90, inv * F32(E, 0x50));
            SF32(E, 0x94, F32(E, 0x54) * inv);
            SF32(E, 0x98, F32(E, 0x58) * inv);
        }
    }
    tpc_cross(E + 0x60u, E + 0x30u, E + 0x90u, 0x0064edd1);
    {
        const rx_addr M = E + 0xd0u;
        SU32(M, 0x00, U32(E, 0x60)); SU32(M, 0x04, U32(E, 0x64)); SU32(M, 0x08, U32(E, 0x68)); SF32(M, 0x0c, 0.0f);
        SU32(M, 0x10, U32(E, 0x30)); SU32(M, 0x14, U32(E, 0x34)); SU32(M, 0x18, U32(E, 0x38)); SF32(M, 0x1c, 0.0f);
        SU32(M, 0x20, U32(E, 0x90)); SU32(M, 0x24, U32(E, 0x94)); SU32(M, 0x28, U32(E, 0x98)); SF32(M, 0x2c, 0.0f);
        SU32(M, 0x30, U32(E, 0x80)); SU32(M, 0x34, U32(E, 0x84)); SU32(M, 0x38, U32(E, 0x88)); SF32(M, 0x3c, 1.0f);
        tpc_mat_to_quat(E + 0x30u, M, 0x0064eeda);
    }
    /* the quaternion times the shake rotation +0x730 (double precision products), back to a matrix */
    {
        const double qx = F32(E, 0x30), qy = F32(E, 0x34), qz = F32(E, 0x38), qw = F32(E, 0x3c);
        const double sx = F32(cam, 0x730), sy = F32(cam, 0x734), sz = F32(cam, 0x738), sw = F32(cam, 0x73c);
        const float aw = (float)((((qw * sw) - (qx * sx)) - (qy * sy)) - (qz * sz));
        SF32(E, 0x6c, aw);
        const float ax = (float)((((qx * sw) + (qw * sx)) + (qy * sz)) - (qz * sy));
        SF32(E, 0x60, ax);
        const float ay = (float)((((qw * sy) + (qy * sw)) + (qz * sx)) - (qx * sz));
        const float az = (float)((((qw * sz) + (qz * sw)) + (qx * sy)) - (qy * sx));
        SF32(E, 0x64, ay);
        const float x2 = ax * 2.0f /* 0x92e330 */, y2 = ay * 2.0f, z2 = az * 2.0f;
        const float xx = x2 * ax;
        SF32(E, 0x7c, xx);
        const float yy = y2 * ay;
        const float wz = z2 * aw, zz = z2 * az;
        const float wx = x2 * aw;
        SF32(E, 0x1c, wx);
        const float wy = y2 * aw, yz = y2 * az;
        SF32(E, 0x20, wz);
        const float xy = x2 * ay, xz = x2 * az;
        const rx_addr M = E + 0xd0u;
        SF32(M, 0x00, 1.0f - (zz + yy));
        SF32(M, 0x04, xy + wz);
        SF32(M, 0x10, xy - wz);
        SF32(M, 0x08, xz - wy);
        SF32(M, 0x14, 1.0f - (zz + xx));
        SF32(M, 0x0c, 0.0f);
        SF32(M, 0x20, xz + wy);
        SF32(M, 0x28, 1.0f - (yy + xx));
        SU32(M, 0x30, U32(E, 0x80));
        SU32(M, 0x34, U32(E, 0x84));
        SF32(M, 0x18, yz + wx);
        SF32(M, 0x1c, 0.0f);
        SF32(M, 0x24, yz - wx);
        SF32(M, 0x2c, 0.0f);
        SU32(M, 0x38, U32(E, 0x88));
        SF32(M, 0x3c, 1.0f);
        tpc_set_pose(cam, M, 0x0064f1aa);
    }
    loop_tc1(loop_vslot(cam, 0x3c), cam, E + 0x120u);
    {
        const float roll = tpc_value(cam + 0x45cu);
        const uint32_t a[2] = {E + 0x120u, rx_fb(roll)};
        const rx_addr s = loop_push(2, a, 0x0064f1d9);
        const float scratch = rx_cam_rotate_matrix(E + 0x120u, roll);
        SF32(rx_sp(), 8, scratch);
        loop_pop(s);
    }
    tpc_set_pose(cam, E + 0x120u, 0x0064f1ea);
    SU32(cam, 0x380, U32(E, 0xc0));
    SU32(cam, 0x384, U32(E, 0xc4));
    SU32(cam, 0x388, U32(E, 0xc8));
    SU8(cam, 0x390, 1);
}

/* 0064c110 MXThirdPersonCamera::vf19 (dt; ret 4): the chase camera. While the pose is valid (+0x2f4):
 *  1. the player camera's update (MXPlayerCamera::vf19); a shake (+0x6a0) runs 006519b0;
 *  2. the vehicle's position L (+0x110), forward F (+0x50), velocity v (+0x30): the yaws atan2(F.x, F.z) (+0x78) and
 *     atan2(v.x, v.z) (+0x18), the horizontal speed^2 (+0x4c); "going backwards" (+0x2b): not on a rail, not attached, the
 *     velocity direction against the forward beyond 150 deg (cos poly of 30 deg) and the speed above 1.2;
 *  3. the target T (+0xc0): the follow point (attached) or L + the tuning offsets +0x500 / +0x504 + +0x510 / +0x508; kept at +0x380
 *     unless held (+0x390);
 *  4. the springs: +0x350 / +0x35c (angle), the yaw +0x3b4 chasing the forward's (or, when riding a rail / following the velocity
 *     / in the race's state 2, the velocity's) yaw + +0x354, the height +0x3f8 (target the drop of T.y), the turn leans +0x420 /
 *     +0x42c from the yaw error (wrapped, at most 80 deg), the side slip +0x438 (the velocity across the yaw, squared), the
 *     backwards flag +0x444, the water ahead (0064f240, +0x450) for the active local camera, +0x468, the look-ahead +0x474;
 *  5. the orbit pose (006502e0) gives the position P (+0xa0) and the look point (+0x60): the view direction against the
 *     direction to T gives the roll +0x45c (acos, the tuning's limits); the vector spring +0x490 and the fov spring +0x4c0 run;
 *     a cut (+0x340) snaps every spring (00650210 and the snaps);
 *  6. the orbit pose again (+0x80 position, +0x90 look point); the speed (x87 |v|) picks the fov from the tuning curve of the
 *     vehicle class / mode (ParamCurve_EvalCubic, +0x18); the position is pulled toward the look point by the speed factor
 *     (lag filter +0x5b0); the view direction D (+0x50) is pitched by the spring +0x404 (the slope), turned by the steer-look
 *     spring +0x410 (the stick, 005487d0), the cut-in transition (+0x3f4) blends position / look / fov;
 *  7. the basis from D and the world up, as a quaternion times +0x730 (the shake), into the pose (0063dee0), then rolled by
 *     +0x45c (0063cf00) and set again; T kept (+0x380, +0x390 = 1). */
void rx_third_person_camera_vf19(rx_addr cam, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x180u;
    rx_set_sp(E);
    SU32(E, 0x17c, U32(0, 0x009b45e4) ^ (E + 0xc));       /* the security cookie (xor the ESP after sub esp, 0x174) */
    if (U32(cam, 0x2f4) == 0) { rx_set_sp(sp0); return; }
    const uint32_t dtb = cam_x87_bits(rx_fb(dt));
    { const uint32_t a[1] = {dtb};
      const rx_addr s = loop_push(1, a, 0x0064c148); rx_player_camera_vf19(cam, rx_bf(dtb)); loop_pop(s); }
    if (U8(cam, 0x6a0) != 0) {
        const uint32_t a[1] = {dtb};
        const rx_addr s = loop_push(1, a, 0x0064c15f);
        rx_cam_shake(cam, rx_bf(dtb));
        loop_pop(s);
    }
    /* 2. the vehicle */
    { const rx_addr v = U32(U32(cam, 0x344), 0x1ac); loop_tc1(loop_vslot(v, 0x44), v, E + 0x110u); }
    {
        const rx_addr v = U32(U32(cam, 0x344), 0x1ac);
        const uint32_t fy = U32(v, 0x134);
        cam_x87_copy(E + 0x50u, v + 0x130u);
        cam_x87_copy(E + 0x58u, v + 0x138u);
        SU32(E, 0x54, fy);
        const rx_addr v2 = U32(U32(cam, 0x344), 0x1ac);
        loop_tc1(loop_vslot(v2, 0x94), v2, E + 0x160u);
    }
    for (uint32_t o = 0; o < 0x10; o += 4) SU32(E, 0x30 + o, U32(E, 0x160 + o));
    {
        rx_set_sp(E - 8u);
        SF32(E - 8u, 4, cam_x87_f32(E + 0x58u)); SF32(E - 8u, 0, cam_x87_f32(E + 0x50u));
        SF32(E, 0x78, tpc_atan2f(F32(E, 0x50), F32(E, 0x58), 0x0064c1e9));
        SF32(E - 8u, 4, cam_x87_f32(E + 0x38u)); SF32(E - 8u, 0, cam_x87_f32(E + 0x30u));
        SF32(E, 0x18, tpc_atan2f(F32(E, 0x30), F32(E, 0x38), 0x0064c204));
        rx_set_sp(E);
    }
    {
        const float vz2 = F32(E, 0x38) * F32(E, 0x38), vx2 = F32(E, 0x30) * F32(E, 0x30);
        SF32(E, 0x7c, vz2);
        const float l = vz2 + vx2;
        SF32(E, 0xb8, vx2);
        SF32(E, 0x4c, l);
        SF32(E, 0x2c, l);
        if (l != 0.0f /* 0x92e12c; NaN too */) {
            const float inv = 1.0f /* 0x931f24 */ / rx_sqrts(l);
            SF32(E, 0x20, inv * F32(E, 0x30));
            SF32(E, 0x24, inv * F32(E, 0x38));
        }
    }
    uint8_t backwards = 0;
    if (tpc_on_rail(cam) == 0 && tpc_attached(cam) == 0) {
        const float c = cam_trig(0.52359873f /* 0x977a10 */);
        const float dot = (F32(E, 0x20) * F32(E, 0x50)) + (F32(E, 0x24) * F32(E, 0x58));
        if (c > dot && F32(E, 0x4c) > 1.44f /* 0x977e04 */) backwards = 1;
    }
    SU8(E, 0x2b, backwards);
    /* 3. the target */
    float tx, ty, tz;
    if (tpc_attached(cam) != 0) {
        const rx_addr s = loop_push(0, 0, 0x0064c3a4);
        const rx_addr p = rx_cam_follow_point(cam, E + 0xa0u);
        loop_pop(s);
        tx = F32(p, 0); ty = F32(p, 4); tz = F32(p, 8);
    } else {
        tx = F32(E, 0x110) + F32(cam, 0x500);
        ty = F32(cam, 0x510) + (F32(cam, 0x504) + F32(E, 0x114));
        tz = F32(cam, 0x508) + F32(E, 0x118);
    }
    SF32(E, 0xc4, ty); SF32(E, 0xc8, tz); SF32(E, 0xc0, tx);
    if (U8(cam, 0x390) == 0) { SF32(cam, 0x380, tx); SF32(cam, 0x384, ty); SF32(cam, 0x388, tz); }
    /* 4. the springs */
    tpc_angle_spring(cam + 0x350u, dt, 8.7266455f /* 0x977e00 */, 999999.0f /* 0x977aa8 */, 5.0f /* 0x935890 */, 0x0064c461);
    tpc_angle_spring(cam + 0x35cu, dt, 8.7266455f, 999999.0f, 5.0f, 0x0064c493);
    if (tpc_on_rail(cam) == 0 && tpc_attached(cam) == 0) SU32(cam, 0x3b0, U32(E, 0x78));
    else if (U8(cam, 0x391) != 0) SU32(cam, 0x3b0, U32(E, 0x18));
    else if ((tpc_on_rail(cam) != 0 || tpc_attached(cam) == 0) &&
             U32(U32(U32(cam, 0x344), 0x94), 0x21c) == 2) SU32(cam, 0x3b0, U32(E, 0x18));
    SF32(cam, 0x3b8, F32(cam, 0x3b0) + F32(cam, 0x354));
    tpc_angle_spring(cam + 0x3b4u, dt, F32(cam, 0x4e0), 999999.0f, F32(cam, 0x4e4), 0x0064c56d);
    SF32(cam, 0x3f8, ((F32(cam, 0x384) - F32(E, 0xc4)) * F32(cam, 0x534)) + F32(cam, 0x3f8));
    SU32(E, 0x2c, U32(cam, 0x530));
    tpc_spring(cam, 0x3f8, 0x3fc, 0x400, F32(cam, 0x52c) * dt, F32(E, 0x2c), dt, E + 0x1cu, E + 0x18u, E + 0x78u);
    if (tpc_on_rail(cam) == 0 && tpc_attached(cam) == 0) {
        float d;
        {
            const uint32_t a[1] = {rx_fb(F32(cam, 0x3b4) - F32(cam, 0x3b8))};
            const rx_addr s = loop_push(1, a, 0x0064c6d5);
            d = rx_cam_wrap_angle(rx_bf(a[0]));
            loop_pop(s);
        }
        const float sign = (0.0f > d) ? F32(0, 0x009cd9e8) : 1.0f;
        SF32(E, 0x18, sign);
        float a = fabsf(d);
        if (!(1.3962632f /* 0x977aac */ > a)) a = 1.3962632f;
        const float f = a / 1.3962632f;
        SF32(cam, 0x424, (F32(cam, 0x514) * f) * F32(E, 0x18));
        SF32(cam, 0x430, (F32(cam, 0x520) * f) * F32(E, 0x18));
    } else {
        SF32(cam, 0x424, 0.0f);
        SF32(cam, 0x430, 0.0f);
    }
    SU32(E, 0x1c, U32(cam, 0x51c));
    tpc_spring(cam, 0x420, 0x424, 0x428, F32(cam, 0x518) * dt, F32(E, 0x1c), dt, E + 0x78u, E + 0x18u, E + 0x2cu);
    SU32(E, 0x1c, U32(cam, 0x528));
    tpc_spring(cam, 0x42c, 0x430, 0x434, F32(cam, 0x524) * dt, F32(E, 0x1c), dt, E + 0x78u, E + 0x18u, E + 0x2cu);
    if (tpc_on_rail(cam) == 0) {
        if (tpc_attached(cam) == 0) {
            const float q = tpc_yaw_slip(E, F32(cam, 0x3b8));
            SF32(cam, 0x43c, ((q * q) * 2.0f /* 0x92e330 */) * F32(cam, 0x538));
        }
        tpc_spring_call(cam + 0x438u, dt, F32(cam, 0x53c), 999999.0f, F32(cam, 0x540), 0x0064cf61);
    } else {
        const float q = tpc_yaw_slip(E, F32(cam, 0x3b8));
        SF32(cam, 0x43c, ((q * q) * 2.0f) * F32(cam, 0x544));
        tpc_spring_call(cam + 0x438u, dt, F32(cam, 0x548), 999999.0f, F32(cam, 0x54c), 0x0064cf61);
    }
    /* the backwards flag spring {444, 448, 44c}: acceleration 1.5, speed at most min(|d|, the horizontal speed) */
    {
        SF32(cam, 0x448, U8(E, 0x2b) != 0 ? 1.0f : 0.0f);
        const float speed = rx_sqrts(F32(E, 0x4c));
        SF32(E, 0x20, speed);
        const float target = F32(cam, 0x448);
        SF32(E, 0x2c, target);
        const float d = target - F32(cam, 0x444);
        const float sign = (0.0f > d) ? F32(0, 0x009cd9e8) : 1.0f;
        const float v = ((dt * 1.5f /* 0x9322a4 */) * sign) + F32(cam, 0x44c);
        SF32(E, 0x18, sign);
        SF32(E, 0x1c, v);
        SF32(cam, 0x44c, v);
        float lim = fabsf(d);
        if (lim > F32(E, 0x20)) lim = F32(E, 0x20);
        if (v * sign > lim) SF32(cam, 0x44c, lim * sign);
        const float p = (dt * F32(cam, 0x44c)) + F32(cam, 0x444);
        SF32(cam, 0x444, p);
        if (0.0f > (F32(E, 0x2c) - p) * sign) { SF32(cam, 0x444, F32(E, 0x2c)); SF32(cam, 0x44c, 0.0f); }
    }
    const uint8_t active = tpc_active_camera_entity(U32(cam, 0x344), 0x0064d075) == cam;
    SU8(E, 0x2b, active);
    {
        const rx_addr C = U32(cam, 0x344);
        if (active && U32(C, 4) != 0 && U32(C, 0x10) == 0 && tpc_attached(cam) == 0) {
            const uint32_t a[2] = {E + 0xc0u, E + 0x50u};
            const rx_addr s = loop_push(2, a, 0x0064d0b7);
            rx_cam_water_ahead(cam, E + 0xc0u, E + 0x50u);
            loop_pop(s);
            tpc_angle_spring(cam + 0x450u, dt, F32(cam, 0x55c), 999999.0f, F32(cam, 0x560), 0x0064d0e9);
        }
    }
    tpc_angle_spring(cam + 0x468u, dt, F32(cam, 0x4d4) * 0.017453291f /* 0x977678 */, F32(cam, 0x4dc), F32(cam, 0x4d8), 0x0064d127);
    tpc_spring(cam, 0x474, 0x478, 0x47c, dt * 90.0f /* 0x935850 */, 4.0f /* 0x9322c0 */, dt, E + 0x1cu, E + 0x18u, E + 0x20u);
    {
        const rx_addr C = U32(cam, 0x344);
        if (U8(E, 0x2b) != 0 && U32(C, 4) != 0 && U32(C, 0x10) == 0) {
            const uint32_t a[1] = {E + 0xc0u};
            const rx_addr s = loop_push(1, a, 0x0064d234);
            rx_cam_orbit_probe(cam, E + 0x50u, E + 0xc0u);
            loop_pop(s);
        }
    }
    {
        const rx_addr act = U32(U32(cam, 0x344), 0x94);
        if (act != 0) {
            if (U32(act, 0x23c) == 0xa) {
                if (U32(cam, 0x5f0) == 0) {
                    const rx_addr r = tpc_player_driver(U32(cam, 0x344), act, 0x0064d25e);
                    SU32(cam, 0x5f0, r);
                    if (r == 0) SF32(cam, 0x5f4, 0.0f);
                    else SF32(cam, 0x5f4, 0.17453291f /* 0x977a08 */);
                } else SF32(cam, 0x5f4, 0.17453291f);
            } else if (U32(cam, 0x5f0) != 0) SU32(cam, 0x5f0, 0);
        }
    }
    SF32(cam, 0x460, 0.0f);
    tpc_view_roll(cam, dt, E);
    tpc_frame(cam, dt, E);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0064c110, rx_third_person_camera_vf19)

/* 00649ed0 MXThirdPersonCamera::vf30 (ECX = camera, stack controller index, dt; RET 8): the user-camera tweak Player_UpdateCamera
 * runs each frame while P+0x988 == 4 (on the selected camera and on P+0x984):
 *  - the dolly key map (00548760): forward +0x368 -= 25 dt, at least -2.0; back +0x368 += 25 dt, at most 15.0 (and returns);
 *  - the H control (005486c0) turns the free-look yaw +0x354 by -25 deg/s * dt, wrapped to (-pi, pi];
 *  - the followed vehicle's forward (+0x130) is kept in +0x3a0..+0x3a8 (copied while the vehicle is not airborne, vf43 0, or when
 *    the stored vector was zero: in the air it keeps the takeoff direction);
 *    its cross with the world up (0, 1, 0) is normalised, and the look-point offset +0x500..+0x508 becomes that cross times
 *    sign(yaw) * min(4, 15 |cos(yaw + pi/2)|);
 *  - the V control (00548710) moves the +0x35c angle spring's target (+0x360) by 25 deg/s * dt, clamped to +-20 deg.
 * Frame: push ebp; and esp, -16; sub esp, 0x34; push ebx / esi / edi: E = the aligned base - 0x40. */
void rx_third_person_camera_vf30(rx_addr cam, uint32_t controller, float dt) {
    const rx_addr sp0 = rx_sp(), A = (sp0 - 4u) & ~0xfu, E = A - 0x40u;
    rx_set_sp(E);
    float v;
    { const rx_addr s = loop_push(0, 0, 0x00649ee8u); v = rx_camera_dolly(controller); loop_pop(s); }
    if (v == 1.0f /* 0x931f24 */) {
        float d = F32(cam, 0x368) - dt * 25.0f /* 0x92e134 */;
        SF32(cam, 0x368, d);
        const float dmin = F32(0, 0x009d7b30u);           /* -2.0 (.data) */
        if (d < dmin) d = dmin;
        SF32(cam, 0x368, d);
    }
    if (v == -1.0f /* 0x9cd9e8 */) {
        float d = dt * 25.0f + F32(cam, 0x368);
        SF32(cam, 0x368, d);
        float m = 15.0f /* 0x9322d0 */;
        if (!(d > 15.0f)) m = d;
        SF32(cam, 0x368, m);
        rx_set_sp(sp0);
        return;
    }
    if (v == 1.0f) { rx_set_sp(sp0); return; }
    SF32(E, 0x18, F32(cam, 0x354));                        /* the yaw before the turn */
    { const rx_addr s = loop_push(0, 0, 0x00649f97u); v = rx_camera_hcontrol(controller); loop_pop(s); }
    float yaw = (v * -0.43633226f /* 0x97797c */) * dt + F32(E, 0x18);
    {   /* wrap to (-pi, pi]: r -= trunc(r / 2pi) * 2pi, then the two pi branches (comiss >= 0 / < 0) */
        const float r0 = yaw - ((float)rx_cvtt((double)(yaw * 0.15915494f /* 0x9774a0 */)) * 6.2831855f /* 0x97749c */);
        yaw = r0;
        if (yaw - 3.1415927f /* 0x977498 */ >= 0.0f) yaw = yaw - 6.2831855f;
        if (!(yaw + 3.1415927f >= 0.0f)) yaw = yaw + 6.2831855f;
    }
    SF32(cam, 0x354, yaw);
    const rx_addr veh = U32(U32(cam, 0x344u), 0x1acu);
    SF32(E, 0x18, yaw);
    SU32(E, 0x20, U32(veh, 0x130u));
    SU32(E, 0x24, U32(veh, 0x134u));
    SU32(E, 0x28, U32(veh, 0x138u));
    const uint32_t airborne = loop_tc0r(loop_vslot(veh, 0xacu), veh, 0x0064a046u);   /* vehicle vf43 00466ae0 (airborne) */
    if (airborne == 0 || (U32(cam, 0x3a0u) == U32(0, 0x00a95d50u) && U32(cam, 0x3a4u) == U32(0, 0x00a95d54u) &&
                          U32(cam, 0x3a8u) == U32(0, 0x00a95d58u))) {
        SU32(cam, 0x3a0u, U32(E, 0x20u));                  /* the vehicle's forward */
        SU32(cam, 0x3a4u, U32(E, 0x24u));
        SU32(cam, 0x3a8u, U32(E, 0x28u));
    }
    const float a0 = F32(cam, 0x3a0u), a4 = F32(cam, 0x3a4u), a8 = F32(cam, 0x3a8u);
    const float b0 = F32(0, 0x009cb4b0u), b4 = F32(0, 0x009cb4b4u), b8 = F32(0, 0x009cb4b8u);
    float c30 = a4 * b8 - a8 * b4;
    float c2c = a8 * b0 - a0 * b8;
    float c28 = a0 * b4 - a4 * b0;
    const float len2 = (c2c * c2c + c30 * c30) + c28 * c28;
    if (len2 > 0.0f /* 0x92e12c */) {
        const float inv = 1.0f / rx_sqrts(len2);            /* SqrtF 0042c230 */
        c30 = inv * c30; c2c = inv * c2c; c28 = inv * c28;
    }
    {   /* |cos(yaw + pi/2)| through the 6-term even polynomial at 0x935d3c..0x935d50, clamped by 4 * min(1, 15 |c| / 4) */
        float x = rx_bf(rx_fb(F32(E, 0x18) + 1.5707964f /* 0x977508 */) & 0x7fffffffu);   /* Float_Abs 00401000 */
        x = x - ((float)rx_cvtt((double)(x * 0.15915494f)) * 6.2831855f);
        x = 3.1415927f - x;
        const float x2 = x * x;
        const float x4 = x2 * x2;
        const float p = (((-2.197296e-07f /* 0x935d50 */ * x2 + 2.420294e-05f /* 0x935d48 */) * x4 +
                          (-0.001385879f /* 0x935d44 */ * x2 + 0.04165978f /* 0x935d4c */)) * x4 +
                         (-0.4999942f /* 0x935d40 */ * x2 + 0.9999992f /* 0x935d3c */));
        const float pa = rx_bf(rx_fb(p) & 0x7fffffffu);      /* Float_Abs */
        float mag;
        if (!(0.0f >= pa)) {                               /* comiss 0, pa; jb */
            if (pa >= 1.0f) mag = 4.0f /* 0x9322c0 */;
            else {
                const float t = (15.0f - 0.0f) * pa + 0.0f;
                if (!(t >= 4.0f)) mag = (!(0.0f >= t)) ? t : 0.0f;
                else mag = 4.0f;
            }
        } else mag = 0.0f;
        const float sign = (F32(E, 0x18) >= 0.0f) ? 1.0f : -1.0f;
        const float k = sign * mag;
        SF32(cam, 0x500u, c30 * k);
        SF32(cam, 0x504u, c2c * k);
        SF32(cam, 0x508u, c28 * k);
    }
    { const rx_addr s = loop_push(0, 0, 0x0064a2c9u); v = rx_camera_vcontrol(controller); loop_pop(s); }
    const float inc = (v * 0.87266451f /* 0x977978 */) * dt;
    const rx_addr spring = cam + 0x35cu;
    float target = inc + rx_cam_spring_target(spring);     /* 0063c7b0 */
    if (-0.3490658f /* 0x977974 */ > target) target = -0.3490658f;
    else if (target > 0.3490658f /* 0x977970 */) target = 0.3490658f;
    rx_float_set4(spring, target);                         /* 00508b70: spring + 4 = target */
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00649ed0, rx_third_person_camera_vf30)
