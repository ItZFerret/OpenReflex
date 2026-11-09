/* world_veh_motocross.c -- the motocross vehicle's large per-frame vfuncs (RavenVehicleMotocross vf62 / vf29 / vf35) and the
 * wheel block update 00468300 / 0041e080 of the physics world's vehicle share.
 * Vehicle (RavenVehicleMotocross, ECX / ESI / EBX): +0xc0 position, +0xe0 rotation (quaternion), +0x178 the dynamic-object
 * handle, +0x17c the body handle (+4 world, +8 index), +0x188 the session, +0x1564 / +0x1568 / +0x1574 the model nodes of
 * the fork / swing arm / ..., +0x16d4 the simulation, +0x17c0 / +0x17c4 child indices of the visual composite.
 * Quaternion products and rotations: veh_quat_mul / veh_rotate (world_veh_util.h, the inlined Xenon sign-table form).
 * core/notes/world.md (veh). */
#include "world_veh.h"
#include "vehicle_saved_pose.h"
#include "world_veh_util.h"
#include "world.h"
#include "world_extern.h"
#include "phys_crt.h"
#include "phys_math.h"
#include "phys_coll_shape.h"
#include "bike_sim.h"
#include "bike_vehicle.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_rider_util.h"
#include "world_misc.h"
#include "world_wreck.h"

/* the double-precision rotation of a vector s by the unit quaternion (x, y, z, w) as the vehicle code inlines it
 * (q (s, 0) conj(q) with every partial product in double and the four quaternion terms rounded to float):
 *   A = ((w c - s0 x) - s1 y) - s2 z, B = ((s0 w + x c) + s2 y) - s1 z, C = ((s1 w + y c) + s0 z) - s2 x,
 *   D = ((s2 w + z c) + s1 x) - s0 y (c = 0.0, 0x977548); with n = -q.xyz:
 *   out = (((w B + A nx) + nz C) - ny D, ((w C + ny A) + D nx) - nz B, ((w D + nz A) + ny B) - C nx) */
static void veh_rotate_d(float x, float y, float z, float w, float s0f, float s1f, float s2f, float out[3]) {
    const double s0 = s0f, s1 = s1f, s2 = s2f, c = 0.0, X = x, Y = y, Z = z, W = w;
    const float nx = phys_sub(-0.0f, x), ny = phys_sub(-0.0f, y), nz = phys_sub(-0.0f, z);
    const float A = (float)(((W * c - s0 * X) - s1 * Y) - s2 * Z);
    const float B = (float)(((s0 * W + X * c) + s2 * Y) - s1 * Z);
    const float C = (float)(((s1 * W + Y * c) + s0 * Z) - s2 * X);
    const float D = (float)(((s2 * W + Z * c) + s1 * X) - s0 * Y);
    out[0] = (float)(((W * B + (double)A * nx) + (double)nz * C) - (double)ny * D);
    out[1] = (float)(((W * C + (double)ny * A) + (double)D * nx) - (double)nz * B);
    out[2] = (float)(((W * D + (double)nz * A) + (double)ny * B) - (double)C * nx);
}

#define XYZ_MASK  0x0092db80u   /* (ffffffff, ffffffff, ffffffff, 0) */
#define W_ONE     0x0092db60u   /* (0, 0, 0, 1.0) */
#define W_MASK    0x0092db90u   /* (0, 0, 0, ffffffff) */

/* a global axis normalised as the inlined code does it: length from (p0 + p1) + p2, 0 where the length is 0, the NaN pattern
 * where |v|^2 is infinite; w forced to 1 */
static rx_v4 axis_unit(rx_addr g) {
    const rx_v4 d = v4_ld(g), p = v4_mul(d, d);
    const float l2 = phys_add(phys_add(p.v[0], p.v[1]), p.v[2]);
    const rx_v4 L2 = v4_splat(l2), len = v4_sqrt(L2);
    const rx_v4 finite = v4_cmpneq(L2, v4_ld(WORLD_INF4));
    rx_v4 r = v4_and(v4_and(v4_div(d, len), v4_cmpneq(v4_zero(), len)), finite);
    r = v4_or(r, v4_andn(finite, v4_ld(WORLD_NAN4)));
    return v4_or(v4_and(r, v4_ld(XYZ_MASK)), v4_ld(W_ONE));
}
/* the rotation quaternion about a unit axis (w = 1) by Vec4Cos_RangeReduced 0042c390 (ECX = c_out, stdcall s_out, xmm0 =
 * the half angle splat): (s.xyz | c.w) * axis */
static rx_v4 axis_rotation(rx_v4 axis, float half, rx_addr c_out, rx_addr s_out) {
    WORLD_CALL_SP(4, phys_vec4_sincos(v4_splat(half), s_out, c_out));
    return v4_mul(v4_or(v4_and(v4_ld(s_out), v4_ld(XYZ_MASK)), v4_and(v4_ld(c_out), v4_ld(W_MASK))), axis);
}
/* the composite's child placement: 00848800 CompositeSetChildPosition (ESI = composite, ECX = world position, EDX = body
 * position; stdcall child, body rotation) and 00848910 CompositeSetChildRotation (ESI = composite, ECX = world rotation, EDX =
 * child; stdcall body rotation) */
static void child_position(rx_addr comp, rx_addr pos, rx_addr ppos, uint32_t child, rx_addr prot) {
    WORLD_CALL_SP(8, phys_composite_set_child_position(comp, pos, ppos, child, prot));
}
static void child_rotation(rx_addr comp, rx_addr rot, uint32_t child, rx_addr prot) {
    WORLD_CALL_SP(4, phys_composite_set_child_rotation(comp, rot, child, prot));
}
/* composite vf8 (+0x20): the child index of a name (< 0 when absent) */
static int32_t child_index(rx_addr comp, rx_addr name) {
    return (int32_t)WORLD_VCALL(comp, 0x20, name);
}
/* child k's vf2 (+8) (rotation, position, a, b): the child's own update (bounds-checked vector +0x2c .. +0x30) */
static void child_update(rx_addr comp, uint32_t k, rx_addr rot, rx_addr pos, rx_addr a, rx_addr b) {
    world_bounds_check(k, (U32(comp, 0x30) - U32(comp, 0x2c)) >> 2);
    const rx_addr child = U32(U32(comp, 0x2c), 4 * k);
    WORLD_VCALL(child, 8, rot, pos, a, b);
}

/* 00462d40 RavenVehicleMotocross::vf62 (thiscall vehicle; frame push ebp; and esp, -16; sub 0x184, cookie; 3 pushes): the
 * visual pose of the bike: the dynamic object posed from the body (vf37 / vf38 velocities, DynObj_SetPose); the wheel spin
 * quaternion about the global axis 0xd61680 (pi / 4) and the steering one about 0xd615c0 (0.325); through the fork node
 * (+0x1564) the composite children "0x93bc94" (offset (0, 1, [0x9cd9dc])) and "0x93bc88" (offset (0, -0.5, 0.75), turned by
 * the steering), through the swing arm (+0x1568) child +0x17c0 and through +0x1574 child +0x17c4 (both spun, with their own
 * updates); the body's grid refresh (0042b000); then the 8 corners of the bike box (+-1.5, -2.25 / 2.25, -3.5 / 3.5) in world
 * space give the bounds for Vehicle_CollectNearbyStatics. */
void rx_world_motocross_vf62(rx_addr veh) {
    world_frame f = world_frame_enter(0x184, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0x18c, U32(0, 0x009b45e4) ^ (S + 0xc));
    WORLD_VCALL(veh, 0x94, S + 0xb0);
    WORLD_VCALL(veh, 0x98, S + 0xa0);
    v4_st(S + 0x70, v4(F32(veh, 0xc0), F32(veh, 0xc4), F32(veh, 0xc8), F32(veh, 0xc8)));
    v4_st(S + 0x20, v4(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec)));
    WORLD_CALL_SP(0x14, rx_world_dynobj_set_pose(veh + 0x178, S + 0x70, S + 0x20, S + 0xb0, S + 0xa0));
    v4_st(S + 0x80, v4_zero());
    v4_st(S + 0x90, axis_unit(0x00d61680));
    const rx_v4 spin = axis_rotation(v4_ld(S + 0x90), 0.785398185f /* 0x9776e0 */, S + 0x40, S + 0x50);
    v4_st(S + 0x40, spin);
    v4_st(S + 0x30, v4_ld(S + 0x70));
    v4_st(S + 0x10, v4_ld(S + 0x20));
    const rx_addr comp = U32(world_handle_object(U32(veh, 0x17c), WCTX(.ebx = veh)), 0x2c);
    WORLD_CALL_SP(4, rx_world_node_world_pose(U32(veh, 0x1564), S + 0x30, S + 0x10));
    int32_t k = child_index(comp, 0x0093bc94);
    if (k >= 0) {
        const rx_v4 q = v4_ld(S + 0x10);
        const rx_v4 off = v4_and(v4(0.0f, 1.0f, F32(0, 0x009cd9dc), F32(0, 0x009cd9dc)), v4_ld(XYZ_MASK));
        v4_st(S + 0x90, v4_add(veh_rotate(q, off), v4_ld(S + 0x30)));
        v4_st(S + 0x50, q);
        child_position(comp, S + 0x90, S + 0x70, (uint32_t)k, S + 0x20);
        child_rotation(comp, S + 0x50, (uint32_t)k, S + 0x20);
    }
    k = child_index(comp, 0x0093bc88);
    if (k >= 0) {
        const rx_v4 q = v4_ld(S + 0x10);
        const rx_v4 off = v4_and(v4(0.0f, -0.5f /* 0x9774b0 */, 0.75f /* 0x935aa8 */, 0.75f), v4_ld(XYZ_MASK));
        v4_st(S + 0x60, v4_add(veh_rotate(q, off), v4_ld(S + 0x30)));
        v4_st(S + 0x50, axis_unit(0x00d615c0));
        const rx_v4 steer = axis_rotation(v4_ld(S + 0x50), 0.324999988f /* 0x977dcc */, S + 0x80, S + 0x90);
        v4_st(S + 0x80, veh_quat_mul(v4_ld(S + 0x10), steer));
        child_position(comp, S + 0x60, S + 0x70, (uint32_t)k, S + 0x20);
        child_rotation(comp, S + 0x80, (uint32_t)k, S + 0x20);
    }
    WORLD_CALL_SP(4, rx_world_node_world_pose(U32(veh, 0x1568), S + 0x30, S + 0x10));
    uint32_t c = U32(veh, 0x17c0);
    child_position(comp, S + 0x30, S + 0x70, c, S + 0x20);
    v4_st(S + 0x10, veh_quat_mul(v4_ld(S + 0x10), v4_ld(S + 0x40)));
    child_rotation(comp, S + 0x10, c, S + 0x20);
    child_update(comp, c, S + 0x10, S + 0x30, S + 0x140, S + 0x170);
    WORLD_CALL_SP(4, rx_world_node_world_pose(U32(veh, 0x1574), S + 0x30, S + 0x10));
    c = U32(veh, 0x17c4);
    v4_st(S + 0x10, veh_quat_mul(v4_ld(S + 0x10), v4_ld(S + 0x40)));
    child_position(comp, S + 0x30, S + 0x70, c, S + 0x20);
    child_rotation(comp, S + 0x10, c, S + 0x20);
    child_update(comp, c, S + 0x10, S + 0x30, S + 0x150, S + 0x160);
    {
        const rx_addr body = U32(veh, 0x17c);
        world_keyed_refresh(U32(body, 8), U32(body, 4) + 0x1954, WCTX(.ebx = veh));
    }
    /* the box corners (x = +-1.5, y = 2.25 / -2.25, z = 3.5 / -3.5), w masked */
    static const float cx[8] = {1.5f, 1.5f, 1.5f, -1.5f, 1.5f, -1.5f, -1.5f, -1.5f};
    static const float cy[8] = {2.25f, 2.25f, -2.25f, 2.25f, -2.25f, 2.25f, -2.25f, -2.25f};
    static const float cz[8] = {3.5f, -3.5f, 3.5f, 3.5f, -3.5f, -3.5f, 3.5f, -3.5f};
    const rx_v4 R = v4_ld(S + 0x20), pos = v4_ld(S + 0x70);
    rx_v4 hi = v4_splat(-3.40282347e+38f /* 0x977550 */), lo = v4_splat(3.40282347e+38f /* 0x977494 */);
    for (int i = 0; i < 8; i++) {
        v4_st(S + 0xc0 + 0x10 * (uint32_t)i, v4(cx[i], cy[i], cz[i], cz[i]));
        const rx_v4 v = v4_and(v4_ld(S + 0xc0 + 0x10 * (uint32_t)i), v4_ld(XYZ_MASK));
        const rx_v4 p = v4_add(veh_rotate(R, v), pos);
        hi = v4_max(hi, p);
        lo = v4_min(lo, p);
    }
    v4_st(S + 0x40, hi);
    v4_st(S + 0x60, lo);
    WORLD_CALL_SP(8, rx_world_vehicle_collect_nearby_statics(U32(veh, 0x16d4) + 0x1300, S + 0x40,
                                                             U32(U32(veh, 0x188), 0x20134), S + 0x60));
    phys_security_check_cookie(U32(S, 0x18c) ^ (S + 0xc));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00462d40, rx_world_motocross_vf62)

/* the frame-counter phase test of the vehicle code: [0xad3ed4] % 3 is 0 or 2 */
static int frame_phase_02(void) {
    const int32_t n = I32(0, 0x00ad3ed4), m = n - (n / 3) * 3;
    return m == 0 || m == 2;
}
/* a node's world matrix row 3 (position) after Node_EvaluateWorld (copied through the frame like the original) */
static void node_world_position(rx_addr node, rx_addr local, rx_addr out) {
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, 0));
    const rx_addr m = U32(U32(node, 0), 0x1c) + ((uint32_t)U8(node, 0xb) << 6);
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(local, i, U32(m, i));
    SU32(out, 0, U32(local, 0x30)); SU32(out, 4, U32(local, 0x34)); SU32(out, 8, U32(local, 0x38));
}
/* a rotation matrix (rows 0..2 at out, out + 0x10, out + 0x20; w lanes 0) from a quaternion, the scalar code's form */
static void quat_matrix_rows(rx_addr out, float x, float y, float z, float w) {
    const veh_rot r = veh_quat_rows(x, y, z, w);
    for (int i = 0; i < 3; i++) {
        SF32(out, 0x10 * i + 0, r.m[i][0]); SF32(out, 0x10 * i + 4, r.m[i][1]); SF32(out, 0x10 * i + 8, r.m[i][2]);
        SF32(out, 0x10 * i + 0xc, 0.0f);
    }
}

/* 00463900 RavenVehicleMotocross::vf29 (thiscall vehicle; dt; RET 4; frame push ebp; and esp, -16; sub 0xb4, cookie;
 * 3 pushes): the vehicle's per-frame state from its simulation (+0x16d4): the replay record (00434d00 / 00460070, the replay
 * state 00433120 when live, ReplayRecord_Encode 00433550); position = sim +0x180 - rotate(sim rotation +0x7c0, the centre of
 * mass +0x3770), rotation (vf36) and velocity (vf37) into +0xc0 / +0xe0 / +0x160; the live simulation's step 0045d910; the
 * model matrix +0xf0 (rotation and the position offset by the rotated static (0, -2.167, 0) in double precision), the forward /
 * up rows +0x130 / +0x140, the speed +0x150 (below 1.001 the velocity becomes the forward row); every other frame (phase 0 / 2)
 * vf60; the rider anchor nodes (+0x1a4 .. +0x1b0) into +0x1d0 / +0x1c0 / +0x1f0 / +0x1e0; the rider frame matrix +0x240 (the
 * simulation's pose override, or node +0x1560, or for the ATV the rider's root matrix 006243d0); vf35 every other frame with a
 * rider; the wheel blocks (00468300); the landing rumble from the suspension speeds (+0x89b0 / +0x8860) / 2200, the camera hook
 * and 0046a380. */
void rx_world_motocross_vf29(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0xb4, 3, 1);
    const rx_addr S = f.esp;
    SU32(S, 0xbc, U32(0, 0x009b45e4) ^ (S + 0xc));
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));
    rx_addr sim = U32(veh, 0x16d4);
    uint32_t diverged;
    WORLD_CALL_SP(0, diverged = bike_state_divergence_check(sim));
    if (diverged != 0) {
        v4_st(S + 0x80, v4_ld(U32(veh, 0x16d4) + 0x170));
        WORLD_CALL_SP(0, rx_vehicle_restore_saved_pose(veh));   /* 00460070 (EAX = veh); EBX / ESI are left alone */
    }
    sim = U32(veh, 0x16d4);
    if (U32(sim, 4) == 0) WORLD_CALL_SP(4, bike_get_replay_state(sim, veh + 0x1220));   /* Simulation_GetReplayState */
    WORLD_CALL_SP(4, bike_replay_record_encode(veh + 0x12c0, U32(veh, 0x16d4)));        /* ReplayRecord_Encode */
    sim = U32(veh, 0x16d4);
    const rx_v4 com = veh_rotate(v4_ld(sim + 0x7c0), v4_and(v4_ld(sim + 0x3770), v4_ld(XYZ_MASK)));
    v4_st(S + 0x60, v4_sub(v4_ld(sim + 0x180), com));
    WORLD_VCALL(veh, 0x90, S + 0xa0);
    WORLD_VCALL(veh, 0x94, S + 0x90);
    v4_st(veh + 0xc0, v4_ld(S + 0x60));
    v4_st(veh + 0xe0, v4_ld(S + 0xa0));
    v4_st(veh + 0x160, v4_ld(S + 0x90));
    if (U32(U32(veh, 0x16d4), 4) == 0) WORLD_CALL_SP(4, bike_vehicle_debug_view(veh, dtp));
    if ((U32(0, 0x00dcf300) & 1u) == 0) {
        SU32(0, 0x00dcf300, U32(0, 0x00dcf300) | 1u);
        SF32(0, 0x00dcf2f0, 0.0f); SF32(0, 0x00dcf2f4, -2.16700006f /* 0x977ee8 */); SF32(0, 0x00dcf2f8, 0.0f);
    }
    {   /* the model matrix: the offset (s0, s1, s2) rotated by the vehicle rotation, in double precision */
        const float x = F32(veh, 0xe0), y = F32(veh, 0xe4), z = F32(veh, 0xe8), w = F32(veh, 0xec);
        float o[3];
        veh_rotate_d(x, y, z, w, F32(0, 0x00dcf2f0), F32(0, 0x00dcf2f4), F32(0, 0x00dcf2f8), o);
        const float px = phys_add(F32(veh, 0xc0), o[0]), py = phys_add(F32(veh, 0xc4), o[1]), pz = phys_add(F32(veh, 0xc8), o[2]);
        quat_matrix_rows(veh + 0xf0, x, y, z, w);
        SF32(veh, 0x120, px); SF32(veh, 0x124, py); SF32(veh, 0x128, pz); SF32(veh, 0x12c, 1.0f);
        const veh_rot r = veh_quat_rows(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec));
        SF32(veh, 0x130, r.m[2][0]); SF32(veh, 0x134, r.m[2][1]); SF32(veh, 0x138, r.m[2][2]);
        SF32(veh, 0x140, r.m[1][0]); SF32(veh, 0x144, r.m[1][1]); SF32(veh, 0x148, r.m[1][2]);
    }
    {
        const float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        float speed;
        WORLD_CALL_SP(4, speed = phys_sqrtf(phys_add(phys_add(phys_mul(vx, vx), phys_mul(vy, vy)), phys_mul(vz, vz))));
        SF32(veh, 0x150, speed);
        if (1.00100005f /* 0x977ecc */ > speed)
            for (uint32_t i = 0; i < 0xc; i += 4) SU32(veh, 0x160 + i, world_x87_pass(U32(veh, 0x130 + i)));
    }
    if (frame_phase_02()) world_vcall_f(veh, 0xf0, dtp);   /* vf60 */
    node_world_position(U32(veh, 0x1a4), S + 0x20, veh + 0x1d0);
    node_world_position(U32(veh, 0x1a8), S + 0x20, veh + 0x1c0);
    node_world_position(U32(veh, 0x1ac), S + 0x20, veh + 0x1f0);
    node_world_position(U32(veh, 0x1b0), S + 0x20, veh + 0x1e0);
    if (U32(veh, 0x280) == 0) {
        sim = U32(veh, 0x16d4);
        if (U32(sim, 0x2de0) != 0) {
            const rx_v4 q0 = v4_ld(sim + 0x2e00), v0 = v4_and(v4_ld(sim + 0x2df0), v4_ld(XYZ_MASK));
            const rx_v4 cj = v4(-1.0f, -1.0f, -1.0f, 1.0f);
            const rx_v4 u = veh_quat_mul(v4_mul(q0, cj), veh_quat_mul(v0, q0));
            const rx_v4 off = v4_and(v4_sub(v4_zero(), u), v4_ld(XYZ_MASK));
            const rx_v4 R = v4_ld(veh + 0xe0);
            const rx_v4 p = v4_add(veh_quat_mul(R, veh_quat_mul(off, v4_mul(R, cj))), v4_ld(veh + 0xc0));
            v4_st(S + 0x60, p);
            const rx_v4 r2 = veh_quat_mul(R, v4_mul(q0, cj));
            v4_st(S + 0x70, r2);
            quat_matrix_rows(veh + 0x240, r2.v[0], r2.v[1], r2.v[2], r2.v[3]);
            SF32(veh, 0x270, p.v[0]); SF32(veh, 0x274, p.v[1]); SF32(veh, 0x278, p.v[2]); SF32(veh, 0x27c, 1.0f);
        } else {
            WORLD_CALL_SP(0, rx_world_node_get_world_matrix(U32(veh, 0x1560), veh + 0x240));
        }
    } else if (U32(veh, 0x1a0) != 0) {
        WORLD_CALL_SP(0, rx_world_rider_root_matrix(U32(veh, 0x1a0), veh + 0x240));
    }
    if (frame_phase_02() && U32(veh, 0x1a0) != 0) world_vcall_f(veh, 0x8c, dtp);   /* vf35 */
    WORLD_CALL_SP(8, rx_world_vehicle_wheels(veh, dtp));
    {
        sim = U32(veh, 0x16d4);
        v4_st(S + 0x60, v4_ld(sim + 0x89b0));
        v4_st(S + 0x70, v4_ld(sim + 0x8860));
        const float ax = F32(S, 0x60), ay = F32(S, 0x64), az = F32(S, 0x68);
        const float bx = F32(S, 0x70), by = F32(S, 0x74), bz = F32(S, 0x78);
        float n1, n2;
        WORLD_CALL_SP(4, n1 = phys_sqrtf(phys_add(phys_add(phys_mul(ay, ay), phys_mul(ax, ax)), phys_mul(az, az))));
        WORLD_CALL_SP(4, n2 = phys_sqrtf(phys_add(phys_add(phys_mul(by, by), phys_mul(bx, bx)), phys_mul(bz, bz))));
        float m = n1 > n2 ? n1 : n2;
        if (0.0f > m) m = 0.0f;
        m = phys_div(m, 2200.0f /* 0x977ef0 */);
        SF32(S, 0xc, m);
        WORLD_CALL_SP(0, rx_world_landing_rumble(veh, m));
        WORLD_CALL_SP(4, rx_world_vehicle_camera_update(veh, rx_bf(world_x87_pass(rx_fb(m)))));
        WORLD_CALL_SP(4, rx_world_raven_sim_440da0(veh, dtp));
    }
    phys_security_check_cookie(U32(S, 0xbc) ^ (S + 0xc));
    world_frame_leave(&f);
}
RX_FUNCTION(0x00463900, rx_world_motocross_vf29)

/* the rider record setters of the simulation code (0x004304xx): (ECX = record, EAX = vector) / (EAX = record, xmm0 = value) */
static void rec_vec(rx_addr fn, rx_addr rec, rx_addr v) {
    void (*f)(rx_addr, rx_addr) = RX_FN(fn, void (*)(rx_addr, rx_addr));
    if (f) WORLD_CALL_SP(0, f(rec, v)); else WORLD_CTX0(fn, &WCTX(.ecx = rec, .eax = v), 0);
}
static void rec_float(rx_addr fn, rx_addr rec, float x) {
    void (*f)(rx_addr, float) = RX_FN(fn, void (*)(rx_addr, float));
    if (f) WORLD_CALL_SP(0, f(rec, x)); else WORLD_CTX0(fn, &WCTX(.eax = rec, .xmm0 = v4(x, 0.0f, 0.0f, 0.0f)), 0);
}
static uint32_t vcall(rx_addr self, uint32_t slot) { return WORLD_VCALL0(self, slot); }

/* one gesture state's per-frame feed (rider +0x5b760 / +0x5b850): +0x04 = 1 (the second) or 0 (the first), the vehicle
 * position (SetVec60), the airborne flag history (+0x90 = old +0x94, +0x94 = vf43), the forward / up rows (SetVec70 / 80),
 * the throttle (SetFloatAC), then the state machine (0062a490) */
static void gesture_feed(rx_addr veh, rx_addr g, float dtp) {
    rec_vec(0x00430500, g, veh + 0xc0);                                   /* RiderRecord_SetVec60 */
    const uint32_t air = vcall(veh, 0xac);
    const uint32_t old = U32(g, 0x94);
    SU32(g, 0x94, air);
    SU32(g, 0x90, old);
    rec_vec(0x00430520, g, veh + 0x130);                                  /* RiderRecord_SetVec70 */
    rec_vec(0x00430540, g, veh + 0x140);                                  /* RiderRecord_SetVec80 */
    rec_float(0x004304f0, g, F32(U32(veh, 0x16d4), 0x8d90));              /* RiderRecord_SetFloatAC */
    WORLD_CALL_SP(4, rx_world_gesture_update(g, dtp));
}

/* 00464e80 RavenVehicleMotocross::vf35 (thiscall vehicle; dt; RET 4; frame push ebp; and esp, -16; sub 0x54; 3 pushes): the
 * rider animation's (+0x1a0) inputs from the vehicle: the body-space velocity (+0x5b9f0 = conj(q) v q of the simulation's
 * rotation +0x730 and velocity +0x160), its frame matrix (vf16 with +0x240), the vehicle axes (+0x5bad0 / +0x5bae0 /
 * +0x5baf0: the unit axes rotated by the vehicle rotation in double precision), the velocity in the model frame +0x5bb10,
 * the angular velocity (vf38) rotated into +0x5bb20, the velocity +0x5bb00, the speed +0x5bb30 (0 while scripted), the
 * steer / lean / throttle inputs (+0x5bb34, +0x5b97c, +0x5b528 .. +0x5b534), the moving state +0x5bb54 (0 standing, 1 slow,
 * 2 fast; from the rider frame's movement since the last frame 0xdb4720, the airborne height and the speed), the wreck
 * state's ramp (006367b0) and its trigger (vf42 changes), both gesture states (level vf42, hold sim +0x28), the look-at record
 * (+0x5b590: velocity, speed, lean, airborne, steer, the model matrix pointer +0x5b5dc, throttle; 00624ef0), the lean blend
 * (+0x5b6e4: asin of the side axis, the throttle), both gesture feeds, the wreck state advance (00636870), the misc flags
 * (+0x5bab8, +0x5babc / +0x5bac0, +0x5baa8, +0x5bb50, +0x5b548, +0x5bb4c), and finally the rider animation's vf19. */
void rx_world_motocross_vf35(rx_addr veh, float dt) {
    world_frame f = world_frame_enter(0x54, 3, 1);
    const rx_addr S = f.esp;
    const float dtp = rx_bf(world_x87_pass(rx_fb(dt)));
    rx_addr sim = U32(veh, 0x16d4);
    {
        const rx_v4 q = v4_ld(sim + 0x730), v = v4_and(v4_ld(sim + 0x160), v4_ld(XYZ_MASK));
        const rx_v4 t = veh_quat_mul(v, q);
        v4_st(U32(veh, 0x1a0) + 0x5b9f0, veh_quat_mul(v4_mul(v4(-1.0f, -1.0f, -1.0f, 1.0f), q), t));
    }
    WORLD_VCALL(U32(veh, 0x1a0), 0x40, veh + 0x240);
    {
        rx_addr ra = U32(veh, 0x1a0);
        SF32(ra, 0x5bad0, 1.0f); SF32(ra, 0x5bad4, 0.0f); SF32(ra, 0x5bad8, 0.0f);
        SF32(ra, 0x5bae0, 0.0f); SF32(ra, 0x5bae4, 1.0f); SF32(ra, 0x5bae8, 0.0f);
        SF32(ra, 0x5baf0, 0.0f); SF32(ra, 0x5baf4, 0.0f); SF32(ra, 0x5baf8, 1.0f);
        for (uint32_t a = 0x5bad0; a <= 0x5baf0; a += 0x10) {
            ra = U32(veh, 0x1a0);
            float o[3];
            veh_rotate_d(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec), F32(ra, a), F32(ra, a + 4), F32(ra, a + 8), o);
            SF32(ra, a, o[0]); SF32(ra, a + 4, o[1]); SF32(ra, a + 8, o[2]);
        }
    }
    {   /* the velocity in the model frame (rows of +0xf0) */
        const rx_addr m = veh + 0xf0;
        const float vx = F32(veh, 0x160), vy = F32(veh, 0x164), vz = F32(veh, 0x168);
        const rx_addr ra = U32(veh, 0x1a0);
        for (uint32_t r = 0; r < 3; r++) {
            const rx_addr row = m + 0x10 * r;
            const float d = phys_add(phys_add(phys_add(phys_mul(vy, F32(row, 4)), phys_mul(vx, F32(row, 0))),
                                              phys_mul(vz, F32(row, 8))), F32(row, 0xc));
            SF32(ra, 0x5bb10 + 4 * r, d);
        }
    }
    WORLD_VCALL(veh, 0x98, S + 0x30);
    {
        float o[3];
        veh_rotate_d(F32(veh, 0xe0), F32(veh, 0xe4), F32(veh, 0xe8), F32(veh, 0xec), F32(S, 0x30), F32(S, 0x34), F32(S, 0x38), o);
        SF32(S, 0x30, o[0]); SF32(S, 0x34, o[1]); SF32(S, 0x38, o[2]);
        const rx_addr ra = U32(veh, 0x1a0);
        SF32(ra, 0x5bb20, o[0]); SF32(ra, 0x5bb24, o[1]); SF32(ra, 0x5bb28, o[2]);
    }
    {
        rx_addr ra = U32(veh, 0x1a0);
        for (uint32_t i = 0; i < 0xc; i += 4) SU32(ra, 0x5bb00 + i, world_x87_pass(U32(veh, 0x160 + i)));
        sim = U32(veh, 0x16d4);
        SF32(ra, 0x5bb30, U32(sim, 0x90) != 0 ? 0.0f : F32(veh, 0x150));
        const int fresh = (U32(0, 0x00db4730) & 1u) == 0;
        SU32(ra, 0x5bb34, world_x87_pass(U32(sim, 0x1c)));
        SU32(ra, 0x5b97c, world_x87_pass(U32(sim, 0x8d90)));
        SU32(ra, 0x5b528, world_x87_pass(U32(sim, 0x2c)));
        SU32(ra, 0x5b52c, world_x87_pass(U32(sim, 0x30)));
        SU32(ra, 0x5b530, world_x87_pass(U32(sim, 0x34)));
        SU32(ra, 0x5b534, world_x87_pass(U32(sim, 0x38)));
        SU32(ra, 0x5b53c, 0);
        const float px = F32(veh, 0x270), py = F32(veh, 0x274), pz = F32(veh, 0x278);
        if (fresh) {
            SU32(0, 0x00db4730, U32(0, 0x00db4730) | 1u);
            SF32(0, 0x00db4720, 0.0f); SF32(0, 0x00db4724, 0.0f); SF32(0, 0x00db4728, 0.0f);
        }
        ra = U32(veh, 0x1a0);
        const float dx = phys_sub(px, F32(0, 0x00db4720)), dy = phys_sub(py, F32(0, 0x00db4724)), dz = phys_sub(pz, F32(0, 0x00db4728));
        const float d2 = phys_add(phys_add(phys_mul(dy, dy), phys_mul(dx, dx)), phys_mul(dz, dz));
        uint32_t moving = 0;
        if (0.0f > F32(ra, 0x5bb18)) {
            sim = U32(veh, 0x16d4);
            if ((F32(sim, 0x1c) == 0.0f || !(F32(sim, 0x20) == 0.0f)) && U32(sim, 0x90) == 0 && d2 > F32(0, 0x009cc8a4))
                moving = 30.0f /* 0x931d98 */ > F32(veh, 0x150) ? 1 : 2;
        }
        SU32(ra, 0x5bb54, moving);
        SF32(0, 0x00db4720, px); SF32(0, 0x00db4724, py); SF32(0, 0x00db4728, pz);
    }
    {
        rx_addr ra = U32(veh, 0x1a0);
        WORLD_CALL_SP(0, rx_world_wreck_state_ramp_to(ra + 0x5b940, F32(U32(veh, 0x16d4), 0x1c)));
        const uint32_t trig = vcall(veh, 0xa8);
        const rx_addr ws = U32(veh, 0x1a0) + 0x5b940;
        if (trig != U32(ws, 0x10)) {
            if (U32(ws, 0) != 2) SU32(ws, 4, U32(ws, 0));
            SU32(ws, 0x14, 1);
            SU32(ws, 0, 2);
            SU32(ws, 0x10, trig);
        }
        const uint32_t lvl = vcall(veh, 0xa8);
        WORLD_CALL_SP(0, rx_world_gesture_set_level(U32(veh, 0x1a0) + 0x5b760, (int32_t)lvl));
        const uint32_t hold = !(F32(U32(veh, 0x16d4), 0x28) == 0.0f);
        WORLD_CALL_SP(0, rx_world_gesture_set_hold(U32(veh, 0x1a0) + 0x5b850, hold));
        SU32(U32(veh, 0x1a0), 0x2b8, U32(U32(veh, 0x190), 8));
        ra = U32(veh, 0x1a0);
        rec_vec(0x00430490, ra + 0x5b590, veh + 0x160);                              /* RiderRecord_SetVec90 */
        rec_float(0x004304b0, U32(veh, 0x1a0) + 0x5b590, F32(veh, 0x150));           /* SetFloatA0 */
        ra = U32(veh, 0x1a0);
        rec_float(0x004304d0, ra + 0x5b590, F32(ra, 0x5b530));                       /* SetFloatA8 */
        SU32(U32(veh, 0x1a0), 0x5b63c, vcall(veh, 0xac));
        ra = U32(veh, 0x1a0);
        rec_float(0x004304c0, ra + 0x5b590, F32(ra, 0x5b528));                       /* SetFloatA4 */
        SU32(U32(veh, 0x1a0), 0x5b5dc, veh + 0xf0);
        rec_float(0x004304e0, U32(veh, 0x1a0) + 0x5b590, F32(U32(veh, 0x16d4), 0x8d90));   /* SetFloat88 */
        WORLD_CALL_SP(4, rx_world_lookat_update(U32(veh, 0x1a0) + 0x5b590, dtp));
        const rx_addr lb = U32(veh, 0x1a0) + 0x5b6e4;
        if (U32(lb, 0xc) != 0) SU32(lb, 0x14, U32(U32(veh, 0x16d4), 0x740));
        ra = U32(veh, 0x1a0);
        WORLD_CALL_SP(0, rx_world_blend_set_18(ra + 0x5b6e4, rider_asin(F32(ra, 0x5bad4))));
        WORLD_CALL_SP(0, rx_world_blend_set_1c(U32(veh, 0x1a0) + 0x5b6e4, F32(U32(veh, 0x16d4), 0x8d90)));
        SU32(U32(veh, 0x1a0), 0x5b764, 0);
        gesture_feed(veh, U32(veh, 0x1a0) + 0x5b760, dtp);
        SU32(U32(veh, 0x1a0), 0x5b854, 1);
        gesture_feed(veh, U32(veh, 0x1a0) + 0x5b850, dtp);
        WORLD_CALL_SP(4, rx_world_wreck_state_advance(U32(veh, 0x1a0) + 0x5b940, dtp));
        SU32(U32(veh, 0x1a0), 0x5bab8, U32(veh, 0x194));
        ra = U32(veh, 0x1a0);
        SU32(ra, 0x5bac0, U32(ra, 0x5babc));
        SU32(U32(veh, 0x1a0), 0x5babc, vcall(veh, 0xac));
        SU32(U32(veh, 0x1a0), 0x5baa8, U32(U32(veh, 0x234), 0x2eb8));
        SU32(U32(veh, 0x1a0), 0x5bb50, vcall(veh, 0xe8));
        const rx_addr grp = U32(U32(veh, 0x190), 0x94);
        SU32(U32(veh, 0x1a0), 0x5b548, grp != 0 && U32(grp, 0x21c) == 0);
        SU32(U32(veh, 0x1a0), 0x5bb4c, vcall(veh, 0xb0));
        ra = U32(veh, 0x1a0);
        world_vcall_f(ra, 0x4c, dtp);             /* RiderAnimation::vf19 */
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x00464e80, rx_world_motocross_vf35)
