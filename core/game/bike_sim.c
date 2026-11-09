/* bike_sim.c -- the simulation framework of the motocross vehicle: the RavenSimulation base slots, the RavenSimulationMotocross
 * overrides, the fixed-step scheduler and step, the input block and the replay record (native/game/notes/bike.md,
 * _re/notes/physics-timing.md, simulation-input.md, replay.md).
 *
 * Simulation object S (RavenSimulationMotocross, vtable 0093c8a4): +04 simulation type (0 local), +1c the 64-byte input block,
 * +b0 scheduling enabled, +b4 steps this update, +b8 max steps, +bc fixed dt, +c0 lead, +d0 mechanism, +ec body count,
 * +f0 / +100 body 0 position / velocity, +730 / +740 orientation / angular velocity, +5c30 control block. */
#include "tuning_reload_log.h"
#include "controller_axis_query.h"
#include "bike_sim.h"
#include "replay_follow.h"
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
#include "fourwheel_rest_frames.h"
#endif
#include "../src/rx_os.h"
#include "phys_mech.h"
#include "phys_math.h"
#include "world_node.h"
#include "phys_mechanism.h"
#include "world.h"
#include "world_misc.h"
#include "rx_vcall.h"
#include "build_xml_stream.h"
#include "build_vehicle_int.h"
#include "build_vehicle.h"
#include "build_xml.h"
#include "crt.h"
#include "loop_jobs.h"
#include <math.h>
#include <string.h>

/* printf (IAT 0x91623c) of a developer message to the console: output only (no game state reads it); the product prints nothing */
static void bike_debug_print(uint32_t msg) {
#ifdef RX_ORACLE
    rx_call_original(U32(0, 0x0091623c), RX_CDECL, 0, 1, &msg);
#else
    (void)msg;
#endif
}

/* ---- calling into code that is not part of this file --------------------------------------------------------------------- */

static rx_regs bike_sim_regs(void) { rx_regs r; memset(&r, 0, sizeof r); return r; }

/* A virtual call as the original makes it: EAX = vtable, ECX = object, EDX = the slot's function (mov eax,[ecx]; mov edx,[eax+off];
 * call edx), stack arguments. Result EAX. The callee is run through rx_call_original_ex with exactly those registers. */
static uint32_t bike_sim_vcall(rx_addr obj, uint32_t off, int nargs, const uint32_t* args) {
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
    if (rx_fourwheel_rest_frames_active()) return rx_fourwheel_rest_frames_vcall(obj, off, nargs, args);
#endif
    rx_addr vt = U32(obj, 0), fn = U32(vt, off);
    if (rx_fn(fn)) return rx_tcall(fn, obj, nargs, args);
    rx_regs in = bike_sim_regs(), out;
    in.r[0] = vt; in.r[1] = obj; in.r[2] = fn;
    rx_call_original_ex(fn, &in, &out, nargs, args);
    return out.r[0];
}

/* ---- tiny slots ---------------------------------------------------------------------------------------------------------- */

/* 00430270 RavenVehicle::vf22: the vehicle type id 0x1082. */
uint32_t bike_vehicle_type_id(void) { return 0x1082; }
RX_FUNCTION(0x00430270, bike_vehicle_type_id)

/* 004392d0 RavenSimulationMotocross::vf3: clears the two wheel contact words S+89e4 and S+8894 (result EAX = 0). */
void bike_clear_wheel_contact_words(rx_addr S) {
    SU32(S, 0x89e4, 0);   /* front wheel record +0x104 */
    SU32(S, 0x8894, 0);   /* rear wheel record +0x104 */
}
RX_FUNCTION(0x004392d0, bike_clear_wheel_contact_words)

/* 004304b0..004304f0 RiderRecord_SetFloat*: one float into the rider record (movss [rec + off], xmm0). */
void bike_rider_record_set_a0(rx_addr rec, float v) { SF32(rec, 0xa0, v); }
RX_FUNCTION(0x004304b0, bike_rider_record_set_a0)
void bike_rider_record_set_a4(rx_addr rec, float v) { SF32(rec, 0xa4, v); }
RX_FUNCTION(0x004304c0, bike_rider_record_set_a4)
void bike_rider_record_set_a8(rx_addr rec, float v) { SF32(rec, 0xa8, v); }
RX_FUNCTION(0x004304d0, bike_rider_record_set_a8)
void bike_rider_record_set_88(rx_addr rec, float v) { SF32(rec, 0x88, v); }
RX_FUNCTION(0x004304e0, bike_rider_record_set_88)
void bike_rider_record_set_ac(rx_addr rec, float v) { SF32(rec, 0xac, v); }
RX_FUNCTION(0x004304f0, bike_rider_record_set_ac)

/* A vec3 copied through the x87 stack (fld m32 ; fstp m32 per component, so a signaling NaN comes out quiet). */
static void bike_sim_copy_vec3_x87(rx_addr dst, rx_addr src) {
    for (uint32_t i = 0; i < 12; i += 4) SU32(dst, i, bike_x87_copy32(U32(src, i)));
}

/* 00430490 / 00430500 / 00430520 / 00430540 RiderRecord_SetVec90 / 60 / 70 / 80: record+off .. +off+8 = the vec3 at src. */
void bike_rider_record_set_vec90(rx_addr rec, rx_addr src) { bike_sim_copy_vec3_x87(rec + 0x90, src); }
RX_FUNCTION(0x00430490, bike_rider_record_set_vec90)
void bike_rider_record_set_vec60(rx_addr rec, rx_addr src) { bike_sim_copy_vec3_x87(rec + 0x60, src); }
RX_FUNCTION(0x00430500, bike_rider_record_set_vec60)
void bike_rider_record_set_vec70(rx_addr rec, rx_addr src) { bike_sim_copy_vec3_x87(rec + 0x70, src); }
RX_FUNCTION(0x00430520, bike_rider_record_set_vec70)
void bike_rider_record_set_vec80(rx_addr rec, rx_addr src) { bike_sim_copy_vec3_x87(rec + 0x80, src); }
RX_FUNCTION(0x00430540, bike_rider_record_set_vec80)

/* 00447730 RavenSimulationMotocross::vf29: 1 when both wheel contact flags (S+8890 rear, S+89e0 front) are set. */
uint32_t bike_both_wheels_in_contact(rx_addr S) {
    return U32(S, 0x8890) != 0 && U32(S, 0x89e0) != 0;
}
RX_FUNCTION(0x00447730, bike_both_wheels_in_contact)

/* 00447750 RavenSimulationMotocross::vf30: slot 29 (+0x74), and all four wheel contact scalars S+8850 / 89a0 / 8854 / 89a4 nonzero. */
uint32_t bike_wheel_contact_scalars_set(rx_addr S) {
    if (bike_sim_vcall(S, 0x74, 0, 0) == 0) return 0;
    return U32(S, 0x8850) != 0 && U32(S, 0x89a0) != 0 && U32(S, 0x8854) != 0 && U32(S, 0x89a4) != 0;
}
RX_FUNCTION(0x00447750, bike_wheel_contact_scalars_set)

/* 00447790 RavenSimulationMotocross::vf31 (thiscall S, stdcall out vec4; RET 4): the normalized sum of the two wheel contact
 * normals (front S+8a10, rear S+88c0): n2 = x*x + y*y + z*z (z*z first, then x*x + y*y, as the original's sqrtps input lane),
 * n = sqrt(n2); out = (n1 + n2) / n in all four lanes, 0 where n == 0, 0x7fc00000 where n2 is infinite (0092dc20 / 0092dc30,
 * phys_normalize3's rule). */
uint32_t bike_wheel_contact_normal(rx_addr S, rx_addr out) {
    v4_st(out, phys_normalize3(v4_add(v4_ld(S + 0x8a10), v4_ld(S + 0x88c0))));
    return out;   /* the original's EAX = the out pointer (004477a4) */
}
RX_FUNCTION(0x00447790, bike_wheel_contact_normal)

/* 0043b5e0 RavenSimulationMotocross::vf5: OR of the body-probe contact flags; the group S+1b5c | 1b54 | 1af8 | 1af0 counts only
 * while S+8c20 == 0. */
uint32_t bike_body_probe_contact_any(rx_addr S) {
    uint32_t gated = U32(S, 0x1b5c) | U32(S, 0x1b54) | U32(S, 0x1af8) | U32(S, 0x1af0);
    uint32_t r = (U32(S, 0x8c20) == 0) ? gated : 0;
    return r | U32(S, 0x1b70) | U32(S, 0x1b64) | U32(S, 0x1b60) | U32(S, 0x1b58) | U32(S, 0x1b0c) | U32(S, 0x1b00) | U32(S, 0x1afc) | U32(S, 0x1af4);
}
RX_FUNCTION(0x0043b5e0, bike_body_probe_contact_any)

/* 004301f0 shared slot (8 classes): copies the vec3 obj+0xc0..0xc8 to out (x87 copies). */
void bike_get_vec_c0(rx_addr obj, rx_addr out) {
    for (uint32_t i = 0; i < 12; i += 4) SU32(out, i, bike_x87_copy32(U32(obj, 0xc0 + i)));
}
RX_FUNCTION(0x004301f0, bike_get_vec_c0)

/* 00430850 Simulation_InitInputBlock: the 64-byte input block (simulation-input.md) all zero except +0x38 (brake hold) = 1. */
void bike_init_input_block(rx_addr block) {
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(block, i, 0);
    SU32(block, 0x38, 1);
}
RX_FUNCTION(0x00430850, bike_init_input_block)

/* 004300a0 ModelNodeRef_SetMatrix: ref -> node record (word 0); the 4x4 matrix at `matrix` (64 bytes, read first) is stored in the
 * owner's matrix table at (node byte +0x0b) * 64 + owner[+0x18]; then Node_Invalidate 007db7e0 marks the hierarchy dirty. The
 * original is a leaf of the Node_Invalidate return registers: EAX = the matrix slot at the call, ECX = the node, EDX = the node's
 * first word (the hierarchy), so its return registers are exactly Node_Invalidate's (see rx_world_node_invalidate_regs). */
void bike_model_node_set_matrix_regs(rx_addr ref, rx_addr matrix, uint32_t out[3]) {
    rx_addr node = U32(ref, 0);
    uint32_t m[16];
    for (int i = 0; i < 16; i++) m[i] = U32(matrix, 4u * (uint32_t)i);
    rx_addr slot = ((uint32_t)U8(node, 0xb) << 6) + U32(U32(node, 0), 0x18);
    for (int i = 0; i < 16; i++) SU32(slot, 4u * (uint32_t)i, m[i]);
    rx_node_invalidate_regs r;
    PHYS_CALL_SP(0, r = rx_world_node_invalidate_regs(node, (uint32_t)slot, U32(node, 0), 0, 0, 0));
    out[0] = r.eax;
    out[1] = r.ecx;
    out[2] = r.edx;
}

/* The void form used by the game code (the world vehicle glue ignores the registers). */
void bike_model_node_set_matrix(rx_addr ref, rx_addr matrix) {
    uint32_t out[3];
    bike_model_node_set_matrix_regs(ref, matrix, out);
}
RX_FUNCTION(0x004300a0, bike_model_node_set_matrix)

/* ---- slots 4 / 7 / 27 of the motocross simulation ------------------------------------------------------------------------- */

/* The twelve body-probe contact flags tested by vf4 (any nonzero). */
static const uint16_t bike_sim_probe_flag_offsets[12] = {0x1af0, 0x1af4, 0x1af8, 0x1afc, 0x1b00, 0x1b0c,
                                                         0x1b54, 0x1b58, 0x1b5c, 0x1b60, 0x1b64, 0x1b70};

/* |v.xyz| as the code computes it: hsum_021 of v * v, splatted, sqrtps (all lanes the same value). */
static float bike_sim_length_021(rx_v4 v) { return phys_sqrt(bike_hsum_021(v4_mul(v, v))); }

/* 0043b4a0 RavenSimulationMotocross::vf4: 1 if any body-probe contact flag is set; else 1 when an external impulse S+2ec0 is
 * present (length > the zero vec4 [00d61670]), the root speed |S+100| is below 4.0 (009322c0) and both wheels are airborne
 * (S+8d98 != 0). */
uint32_t bike_moto_contact_or_impulse(rx_addr S) {
    for (int i = 0; i < 12; i++)
        if (U32(S, bike_sim_probe_flag_offsets[i]) != 0) return 1;
    float impulse = bike_sim_length_021(v4_ld(S + 0x2ec0));
    rx_v4 zero = bike_g_zero();
    int has_impulse = zero.v[0] < impulse && zero.v[1] < impulse && zero.v[2] < impulse;   /* cmpltps: lanes summed == -3 */
    float speed = bike_sim_length_021(v4_ld(S + 0x100));
    int slow = speed < 4.0f /* 009322c0 */;
    return has_impulse && slow && U32(S, 0x8d98) != 0;
}
RX_FUNCTION(0x0043b4a0, bike_moto_contact_or_impulse)

/* 0043c440 RavenSimulationMotocross::vf7 (dt): the scheduler's callback after each step.
 *  - S+8c28 set (pose override restore): moves every body by delta = saved position (S+8c40) - current root position, restores the
 *    root velocity from S+8c50 (bodies 0..3 positions S+f0 / 190 / 230 / 2d0).
 *  - otherwise saves root orientation (S+730 -> S+8c30), position (S+f0 -> S+8c40) and velocity (S+100 -> S+8c50).
 *  Then the timer S+68 (0 .. 1.5): rises at 6/s when S+60 and S+5c are set, else falls (rate 1/s; 0.1/s when S+5c is set and the
 *  control value *(S+5c30) <= 0.2), clamped. */
void bike_moto_save_or_restore_pose(rx_addr S, float dt) {
    if (U32(S, 0x8c28) != 0) {
        rx_v4 delta = v4_sub(v4_ld(S + 0x8c40), v4_ld(S + 0xf0));
        v4_st(S + 0x100, v4_ld(S + 0x8c50));
        v4_st(S + 0xf0, v4_add(v4_ld(S + 0xf0), delta));
        v4_st(S + 0x190, v4_add(delta, v4_ld(S + 0x190)));
        v4_st(S + 0x230, v4_add(v4_ld(S + 0x230), delta));
        v4_st(S + 0x2d0, v4_add(v4_ld(S + 0x2d0), delta));
    } else {
        v4_st(S + 0x8c30, v4_ld(S + 0x730));
        v4_st(S + 0x8c40, v4_ld(S + 0xf0));
        v4_st(S + 0x8c50, v4_ld(S + 0x100));
    }
    float t;
    if (U32(S, 0x60) != 0 && U32(S, 0x5c) != 0) {
        t = phys_add(phys_mul(dt, 6.0f /* 0092fe4c */), F32(S, 0x68));
        SF32(S, 0x68, t);
        if (t >= 1.5f /* 009322a4 */) { SF32(S, 0x68, 1.5f); return; }
        if (0.0f < t || t != t) return;                   /* comiss 0, t ; jb: keeps t (unordered too) */
        SF32(S, 0x68, 0.0f);
        return;
    }
    if (U32(S, 0x5c) != 0) {
        if (!(0.2f /* 0092e388 */ >= F32(U32(S, 0x5c30), 0))) return;      /* jb: control value above 0.2 (or NaN): unchanged */
        t = phys_sub(F32(S, 0x68), phys_mul(dt, 0.1f /* 00931d94 */));
    } else {
        t = phys_sub(F32(S, 0x68), dt);
    }
    SF32(S, 0x68, t);
    if (!(t > 0.0f)) SF32(S, 0x68, 0.0f);
}
RX_FUNCTION(0x0043c440, bike_moto_save_or_restore_pose)

/* 0044c010 RavenSimulationMotocross::vf27: 1 when the bike is resting (S+90) and the global axis [00d615c0] (xyz), rotated by the
 * root orientation S+730 (q * v * conj(q)), has a dot product d with the global vector [00d616a0] (xyz) in -0.015 < d < 0.015
 * (almost perpendicular: the bike lies flat/upright with respect to the reference). */
uint32_t bike_moto_resting_axis_flat(rx_addr S) {
    rx_v4 q = v4_ld(S + 0x730);
    rx_v4 rotated = bike_quat_mul(q, bike_quat_mul(v4_xyz(rx_ld4(0x00d615c0)), bike_quat_conj(q)));
    float d = bike_hsum_012(v4_mul(rotated, rx_ld4(0x00d616a0)));
    if (U32(S, 0x90) == 0) return 0;
    return d < 0.015f /* 009773e8 */ && -0.015f /* 009778e4 */ < d;
}
RX_FUNCTION(0x0044c010, bike_moto_resting_axis_flat)

/* ---- pose override (animation test mode): vf33 begins it, vf10 sets targets, vf34 applies them, 00434ef0 ends it ---------- */

/* |p|^2 as the inlined 4-lane normalize computes it from p = v * v (shufps 0xee / 0x40 / 0xf0 + addps): (p3 + p1) + (p2 + p0). */
static float bike_sim_hsum4(rx_v4 p) { return phys_add(phys_add(p.v[3], p.v[1]), phys_add(p.v[2], p.v[0])); }
/* the vec4 normalize of the pose code: all four lanes divided by sqrt(hsum4), zero / infinite lengths masked (as bike_normalize). */
static rx_v4 bike_sim_normalize4(rx_v4 v) { return bike_normalize_masked(v, bike_sim_hsum4(v4_mul(v, v))); }

/* msvcr90 _CIatan2 (0x009009c0, the CRT port): atan2(st1, st0) of two floats on the x87 stack, the result rounded to a float by
 * fstp dword. */
static float bike_sim_atan2(float y, float x) {
    return rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(y), rx_f80_from_f32(x), CRT_CW_MAIN), 0);
}

/* 00446cd0 RavenSimulationMotocross::vf33: begins a pose override. S+2de0 = 1, steer angle S+2e70 = 0, saved pose S+2e50 =
 * orientation S+730, S+2e60 = position S+f0; targets S+2df0 = [00d61670], S+2e00 / 2e10 = [00d61640]; previous targets S+2e20 /
 * 2e30 / 2e40 = the same. Mode 2 (S+2de4) also saves the wheel contact points S+8b00 / 8b10 (from S+87f0 / 8940), two local
 * frame vectors S+8b20 = normalize(conj(q) * S+910) and S+8b30 = normalize(conj(q * S+8a70) * S+870), and S+8af0 = [00d616a0]. */
void bike_moto_begin_pose_override(rx_addr S) {
    SU32(S, 0x2de0, 1);
    SF32(S, 0x2e70, 0.0f);
    v4_st(S + 0x2e50, v4_ld(S + 0x730));
    v4_st(S + 0x2e60, v4_ld(S + 0xf0));
    v4_st(S + 0x2df0, rx_ld4(0x00d61670));
    v4_st(S + 0x2e00, rx_ld4(0x00d61640));
    v4_st(S + 0x2e10, rx_ld4(0x00d61640));
    v4_st(S + 0x2e20, v4_ld(S + 0x2df0));
    v4_st(S + 0x2e30, v4_ld(S + 0x2e00));
    v4_st(S + 0x2e40, v4_ld(S + 0x2e10));
    if (U32(S, 0x2de4) != 2) return;
    v4_st(S + 0x8b00, v4_ld(S + 0x87f0));
    v4_st(S + 0x8b10, v4_ld(S + 0x8940));
    rx_v4 q = v4_ld(S + 0x730);
    rx_v4 a = bike_quat_mul(bike_quat_conj(q), v4_ld(S + 0x910));
    v4_st(S + 0x8b20, bike_sim_normalize4(a));
    rx_v4 qw = bike_quat_mul(v4_ld(S + 0x730), v4_ld(S + 0x8a70));
    rx_v4 b = bike_quat_mul(v4_mul(qw, v4(-1.0f, -1.0f, -1.0f, 1.0f)), v4_ld(S + 0x870));   /* mulps xmm0, 0093a220 */
    v4_st(S + 0x8b30, bike_sim_normalize4(b));
    v4_st(S + 0x8af0, rx_ld4(0x00d616a0));
}
RX_FUNCTION(0x00446cd0, bike_moto_begin_pose_override)

/* 0044e570 RavenSimulationMotocross::vf10 (stack: three vec4 targets): S+2df0 / 2e00 / 2e10 = targets[0..2]; then the steer angle
 * S+2e70 from the third target: with Q = (-0.2518, 0, 0, 0.9675) (009778dc / 009778e0) and the axis A = xyz of [00d616a0],
 * R = conj(Q) * target2 (xyz, as (x, y, z, x)); angle = atan2(|R|, R.w); the sign is that of (Q * (A * conj(Q))) . R:
 * S+2e70 = angle * 2 when it is positive (zero < dot), else angle * -2. */
void bike_moto_set_pose_override_targets(rx_addr S, rx_addr targets) {
    v4_st(S + 0x2df0, v4_ld(targets));
    v4_st(S + 0x2e00, v4_ld(targets + 0x10));
    v4_st(S + 0x2e10, v4_ld(targets + 0x20));
    rx_v4 Q = v4(-0.2518259882926941f /* 009778dc */, 0.0f, 0.0f, 0.967507004737854f /* 009778e0 */);
    rx_v4 conjQ = v4_mul(Q, v4(-1.0f, -1.0f, -1.0f, 1.0f));
    rx_v4 axis = v4_xyz(rx_ld4(0x00d616a0));
    rx_v4 P1 = bike_quat_mul(axis, conjQ);
    rx_v4 P3 = bike_quat_mul(conjQ, v4_ld(S + 0x2e10));
    rx_v4 R = v4(P3.v[0], P3.v[1], P3.v[2], P3.v[0]);
    float len = phys_sqrt(bike_hsum_021(v4_mul(R, R)));
    float angle = bike_sim_atan2(len, P3.v[3]);
    rx_v4 P2 = bike_quat_mul(Q, P1);
    float d = bike_hsum_012(v4_mul(P2, R));
    rx_v4 zero = bike_g_zero();
    int positive = zero.v[0] < d && zero.v[1] < d && zero.v[2] < d;
    SF32(S, 0x2e70, phys_mul(angle, positive ? 2.0f /* 0092e330 */ : -2.0f /* 009d7b30 */));
}
RX_FUNCTION(0x0044e570, bike_moto_set_pose_override_targets)

/* ---- mechanism helpers used by the pose override (world/phys code, run through the original for now) ------------------------ */

/* Locals the original passes by address live on the game stack: a 16-aligned frame below ESP. */
#define SIM_FRAME_ENTER(F, size) rx_addr F##_sp = rx_sp(); rx_addr F = (F##_sp - (size)) & ~15u; rx_set_sp(F)
#define SIM_FRAME_LEAVE(F) rx_set_sp(F##_sp)

/* 0085c500 Mechanism_Translate (phys): EDX = mechanism, ESI = vec4 delta; adds it to every body position. */
static void bike_sim_mech_translate(rx_addr M, rx_addr vec) {
    PHYS_CALL_SP(0, phys_mech_translate(M, vec));
}
/* 0085c530 Mechanism_Rotate (phys): EDX = mechanism, ESI = quaternion; rotates every body about the first one. */
static void bike_sim_mech_rotate(rx_addr M, rx_addr quat) { phys_mech_rotate(M, quat); }
/* 0085c4b0 Mechanism_SetVelocity (phys): EDX = mechanism, ESI = vec4; position-ish vec4 to every body, the rest zeroed
 * (also called with ESI = [00d61670], all zero: stops the mechanism). */
static void bike_sim_mech_set_velocity(rx_addr M, rx_addr vec) { phys_mech_set_velocity(M, vec); }
/* 0085d300 Mechanism_NodesSlot6 (phys): stdcall (mechanism); node slot 6 of every non-root node. */
static void bike_sim_mech_nodes_slot6(rx_addr M) {
    PHYS_CALL_SP(4, (void)phys_mech_nodes_slot6(M));
}

/* 00434ef0 Simulation_EndPoseOverride (EDI = S): ends a pose override (S+2de0 set). Modes 1, 3, 4 just clear S+2de0; mode 2
 * (S+2de4) first moves the mechanism back: translates it by saved position S+2e60 - current S+f0 (y component zero, from
 * [00d61670] lane 1), rotates it by conj(current S+730) * saved orientation S+2e50, and stops it (velocities zero). Other modes
 * leave the flag. */
void bike_end_pose_override(rx_addr S) {
    if (U32(S, 0x2de0) == 0) return;
    uint32_t mode = U32(S, 0x2de4);
    if (mode - 1u > 3u) return;
    SU32(S, 0x2de0, 0);
    if (mode != 2) return;
    SIM_FRAME_ENTER(F, 0x40);
    rx_addr M = S + 0xd0;
    rx_v4 d = v4_sub(v4_ld(S + 0x2e60), v4_ld(S + 0xf0));
    rx_v4 zero = bike_g_zero();
    v4_st(F + 0x10, v4(d.v[0], zero.v[1], d.v[2], d.v[3]));        /* shufps 0xe5 ; movss */
    bike_sim_mech_translate(M, F + 0x10);
    v4_st(F + 0x10, bike_quat_mul(v4_mul(v4_ld(S + 0x730), v4(-1.0f, -1.0f, -1.0f, 1.0f)), v4_ld(S + 0x2e50)));
    bike_sim_mech_rotate(M, F + 0x10);
    bike_sim_mech_set_velocity(M, 0x00d61670);
    SIM_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00434ef0, bike_end_pose_override)

/* |x| (Float_Abs 00401000: andps with 0x7fffffff, lanes 1..3 of xmm0 cleared; only lane 0 is used by the callers here). */
static float bike_sim_fabs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }

/* The game's polynomial cosine of a shifted angle (inlined in vf34 and in the scheduler's orientation interpolation): with a = |x|,
 * r = a - trunc(a / 2pi) * 2pi, u = (pi - r)^2, u2 = u^2:  ((u c50 + c48) u2 + (u c44 + c4c)) u2 + (u c40 + c3c)
 * = -cos(r) for x = angle, so trig(angle + pi/2) = sin(angle). */
static float bike_sim_trig(float x) {
    float a = bike_sim_fabs(x);
    float n = (float)rx_cvtt(phys_mul(a, 0.15915494f /* 009774a0 */));
    float r = phys_sub(a, phys_mul(n, 6.2831855f /* 0097749c */));
    float pr = phys_sub(3.1415927f /* 00977498 */, r);
    float u = phys_mul(pr, pr), u2 = phys_mul(u, u);
    const float c50 = rx_bf(0xb46beed0u) /* 00935d50 */, c4c = rx_bf(0x3d2aa372u) /* 00935d4c */, c48 = rx_bf(0x37cb076bu) /* 00935d48 */;
    const float c44 = rx_bf(0xbab5a662u) /* 00935d44 */, c40 = rx_bf(0xbeffff3du) /* 00935d40 */, c3c = rx_bf(0x3f7ffff3u) /* 00935d3c */;
    float t1 = phys_add(phys_mul(u, c50), c48);
    float t2 = phys_add(phys_mul(u, c44), c4c);
    float t3 = phys_add(phys_mul(u, c40), c3c);
    t1 = phys_add(phys_mul(t1, u2), t2);
    t1 = phys_mul(t1, u2);
    return phys_add(t1, t3);
}

/* The steering angle's (0, A, 0, B) stored at control block + 0x50 by vf34's modes 3 / 4: x = steer * 0.5, A = trig(x + pi/2)
 * (= sin x), B = -0.0 - trig(x) (= cos x). */
static rx_v4 bike_sim_steer_sincos(float x) {
    float s = bike_sim_trig(phys_add(x, 1.5707964f /* 00977508 */));
    float b = phys_sub(rx_bf(0x80000000u) /* 009774a8: -0.0 */, bike_sim_trig(x));
    return v4(0.0f, s, 0.0f, b);
}

/* 0044e830 RavenSimulationMotocross::vf34 (EDI = S): applies the pose override by mode S+2de4 (jump table 0044eec8), called by the
 * scheduler before each step; the targets S+2df0 (position) / 2e00 (orientation) / 2e10 (steer-related) with the previous ones
 * S+2e20 / 2e30 / 2e40, the saved pose S+2e50 (orientation) / 2e60 (position):
 *  mode 1: translate by q * (target - previous position) * conj(q), rotate by conj(previous orientation) * target orientation;
 *  mode 2: translate so the bike sits at saved position + q * target position * conj(q) (y of the move cleared), rotate to
 *          conj(current orientation) * (saved * target orientation);
 *  modes 3 / 4: set the pose through slot 2 (position, orientation), steering S+2e70 into the control block (+0x10, +0x50 =
 *          sin/cos pair), slot 16, mechanism nodes slot 6.
 * Always ends by copying the targets to the previous ones. */
void bike_moto_apply_pose_override(rx_addr S) {
    uint32_t mode = U32(S, 0x2de4);
    SIM_FRAME_ENTER(F, 0x60);
    rx_addr M = S + 0xd0;
    rx_v4 conj_c = v4(-1.0f, -1.0f, -1.0f, 1.0f);                          /* 0093a220 */
    if (mode == 1) {
        rx_v4 q = v4_ld(S + 0x2e50);
        rx_v4 d = v4_xyz(v4_sub(v4_ld(S + 0x2df0), v4_ld(S + 0x2e20)));
        rx_v4 P = bike_quat_mul(d, v4_mul(conj_c, q));
        rx_v4 T = bike_quat_mul(q, P);
        rx_v4 Rr = bike_quat_mul(v4_mul(v4_ld(S + 0x2e30), conj_c), v4_ld(S + 0x2e00));
        v4_st(F + 0x30, T);
        v4_st(F + 0x20, Rr);
        bike_sim_mech_translate(M, F + 0x30);
        bike_sim_mech_rotate(M, F + 0x20);
    } else if (mode == 2) {
        rx_v4 q = v4_ld(S + 0x2e50);
        rx_v4 v = v4_xyz(v4_ld(S + 0x2df0));
        rx_v4 P = bike_quat_mul(v, v4_mul(q, conj_c));
        rx_v4 R = bike_quat_mul(q, P);
        rx_v4 X = v4_sub(v4_add(R, v4_ld(S + 0x2e60)), v4_ld(S + 0xf0));
        rx_v4 zero = bike_g_zero();
        v4_st(F + 0x20, v4(X.v[0], zero.v[1], X.v[2], X.v[3]));
        bike_sim_mech_translate(M, F + 0x20);
        rx_v4 Qm = bike_quat_mul(v4_ld(S + 0x2e50), v4_ld(S + 0x2e00));
        v4_st(F + 0x20, bike_quat_mul(v4_mul(v4_ld(S + 0x730), conj_c), Qm));
        bike_sim_mech_rotate(M, F + 0x20);
    } else if (mode == 3 || mode == 4) {
        rx_v4 q = v4_ld(S + 0x2e50);
        rx_v4 v = v4_xyz(v4_ld(S + 0x2df0));
        rx_v4 P = bike_quat_mul(v, v4_mul(q, conj_c));
        rx_v4 R = bike_quat_mul(q, P);
        rx_v4 pos = v4_add(R, v4_ld(S + 0x2e60));
        rx_v4 orient = bike_quat_mul(q, v4_ld(S + 0x2e00));
        v4_st(F + 0x30, pos);
        v4_st(F + 0x20, orient);
        uint32_t args[2] = {F + 0x30, F + 0x20};
        rx_addr vt = U32(S, 0), fn = U32(vt, 8);
        if (rx_fn(fn)) rx_tcall(fn, S, 2, args);
        else {
            rx_regs in = bike_sim_regs(); in.r[0] = fn; in.r[1] = S; in.r[2] = F + 0x30;
            rx_call_original_ex(fn, &in, 0, 2, args);
        }                                                                  /* slot 2: set pose (position, orientation), ret 8 */
        rx_addr ctl = U32(S, 0x5c30);
        SU32(ctl, 0x10, bike_x87_copy32(U32(S, 0x2e70)));
        SU32(S, 0xb0, 1);
        ctl = U32(S, 0x5c30);
        float x = phys_mul(F32(ctl, 0x10), 0.5f /* 009322cc */);
        v4_st(ctl + 0x50, bike_sim_steer_sincos(x));
        bike_sim_vcall(S, 0x40, 0, 0);                                     /* slot 16 */
        bike_sim_mech_nodes_slot6(M);
    }
    v4_st(S + 0x2e20, v4_ld(S + 0x2df0));
    v4_st(S + 0x2e30, v4_ld(S + 0x2e00));
    v4_st(S + 0x2e40, v4_ld(S + 0x2e10));
    SIM_FRAME_LEAVE(F);
}
RX_FUNCTION(0x0044e830, bike_moto_apply_pose_override)

/* ---- replay ------------------------------------------------------------------------------------------------------------------ */

/* 00433120 Simulation_GetReplayState (ECX = S, stack out): position S+180, orientation S+7c0, velocity S+100, angular velocity S+740
 * (16 bytes each, at out +0x00 / 0x10 / 0x20 / 0x30), time = the race clock ([00a89be8] + [00a89be4]) at +0x40, the seven input
 * axes S+1c, S+24..S+38 (x87 copies) at +0x48..+0x60, and the flags word +0x44: 0x10000 when S+48 is set, 0x20000 when the reverse
 * input S+20 is above zero. */
void bike_get_replay_state(rx_addr S, rx_addr out) {
    v4_st(out, v4_ld(S + 0x180));
    v4_st(out + 0x10, v4_ld(S + 0x7c0));
    v4_st(out + 0x20, v4_ld(S + 0x100));
    v4_st(out + 0x30, v4_ld(S + 0x740));
    SF32(out, 0x40, phys_add(F32(0, 0x00a89be8), F32(0, 0x00a89be4)));
    SU32(out, 0x48, bike_x87_copy32(U32(S, 0x1c)));
    SU32(out, 0x4c, bike_x87_copy32(U32(S, 0x24)));
    SU32(out, 0x50, bike_x87_copy32(U32(S, 0x28)));
    SU32(out, 0x54, bike_x87_copy32(U32(S, 0x2c)));
    SU32(out, 0x58, bike_x87_copy32(U32(S, 0x30)));
    SU32(out, 0x5c, bike_x87_copy32(U32(S, 0x34)));
    SU32(out, 0x44, 0);
    SU32(out, 0x60, bike_x87_copy32(U32(S, 0x38)));
    if (U32(S, 0x48) != 0) SU32(out, 0x44, 0x10000);
    if (F32(S, 0x20) > 0.0f) SU32(out, 0x44, U32(out, 0x44) | 0x20000);
}
RX_FUNCTION(0x00433120, bike_get_replay_state)

/* The game's float -> half conversion (inlined 12 times in the encoder): sign, then 0x7fff above 0x47ffefff (|x| >= 65520 or NaN),
 * normals rebias by 0x38000000, smaller ones shift the mantissa (count masked to 5 bits like x86's shr by cl); the result is
 * rounded as (m + 0xfff + bit13) >> 13. */
static uint16_t bike_sim_float_to_half(float f) {
    uint32_t bits = rx_fb(f), sign = (bits >> 16) & 0x8000u, mag = bits & 0x7fffffffu, mant;
    if (mag > 0x47ffefffu) return (uint16_t)(sign | 0x7fffu);
    if (mag >= 0x38800000u) {
        mant = mag - 0x38000000u;
    } else {
        uint32_t exp = mag >> 23;
        mant = ((mag & 0x7fffffu) | 0x800000u) >> ((0x71u - exp) & 31u);
    }
    uint32_t h = ((((mant >> 13) & 1u) + mant + 0xfffu) >> 13) & 0x7fffu;
    return (uint16_t)(sign | h);
}
static void bike_sim_store_half4(rx_addr dst, rx_v4 v) {
    for (uint32_t i = 0; i < 4; i++) SU16(dst, 2 * i, bike_sim_float_to_half(v.v[i]));
}

/* 0042ccd0 Vec4Floor (phys): xmm0 -> xmm0, floor of each lane. */
static rx_v4 bike_sim_vec4_floor(rx_v4 x) { return phys_vec4_floor(x); }

/* 00433550 ReplayRecord_Encode (ESI = out record V+0x12c0, stack S): the per-frame replay record of 0x44 bytes (replay.md):
 *   +0x00 half4 (floor(position).xyz, frac.x)    +0x08 half4 (velocity.xyz, frac.y)
 *   +0x10 half4 orientation (S+7c0)              +0x18 half4 (angular velocity.xyz, frac.z)
 *   +0x20 race time ([00a89be8] + [00a89be4])    +0x24..+0x3c the seven input axes (S+1c, S+24..S+38)
 *   +0x40 flags: 0x10000 S+48, 0x20000 reverse S+20 > 0, 0x40000 teleport S+2e98, 0x80000 rider off S+2374
 * where frac = position - floor(position) (the integer part and the residual of the position share the half4s' spare w lanes).
 * The w lanes of position / velocity / angular velocity are first replaced by [00d61660].w (a vec4 global, all zero). */
void bike_replay_record_encode(rx_addr rec, rx_addr S) {
    rx_v4 g = rx_ld4(0x00d61660);
    rx_v4 pos = v4_ld(S + 0x180), vel = v4_ld(S + 0x100), angvel = v4_ld(S + 0x740), quat = v4_ld(S + 0x7c0);
    pos.v[3] = vel.v[3] = angvel.v[3] = g.v[3];
    rx_v4 fl = bike_sim_vec4_floor(v4_ld(S + 0x180));
    fl.v[3] = g.v[3];
    rx_v4 frac = v4_sub(pos, fl);
    bike_sim_store_half4(rec, v4(fl.v[0], fl.v[1], fl.v[2], frac.v[0]));
    bike_sim_store_half4(rec + 8, v4(vel.v[0], vel.v[1], vel.v[2], frac.v[1]));
    bike_sim_store_half4(rec + 0x10, quat);
    bike_sim_store_half4(rec + 0x18, v4(angvel.v[0], angvel.v[1], angvel.v[2], frac.v[2]));
    SF32(rec, 0x20, phys_add(F32(0, 0x00a89be8), F32(0, 0x00a89be4)));
    SU32(rec, 0x24, bike_x87_copy32(U32(S, 0x1c)));
    SU32(rec, 0x28, bike_x87_copy32(U32(S, 0x24)));
    SU32(rec, 0x2c, bike_x87_copy32(U32(S, 0x28)));
    SU32(rec, 0x30, bike_x87_copy32(U32(S, 0x2c)));
    SU32(rec, 0x34, bike_x87_copy32(U32(S, 0x30)));
    SU32(rec, 0x38, bike_x87_copy32(U32(S, 0x34)));
    SU32(rec, 0x40, 0);
    SU32(rec, 0x3c, bike_x87_copy32(U32(S, 0x38)));
    if (U32(S, 0x48) != 0) SU32(rec, 0x40, 0x10000);
    if (F32(S, 0x20) > 0.0f) SU32(rec, 0x40, U32(rec, 0x40) | 0x20000);
    if (U32(S, 0x2e98) != 0) SU32(rec, 0x40, U32(rec, 0x40) | 0x40000);
    if (U32(S, 0x2374) != 0) SU32(rec, 0x40, U32(rec, 0x40) | 0x80000);
}
RX_FUNCTION(0x00433550, bike_replay_record_encode)

/* ---- input ---------------------------------------------------------------------------------------------------------------- */

/* The controller's action objects (loop/input code; run through the original): pad = *(009c52c4 + 4 * controller), the actions are
 * embedded objects in it (vtable at +0). */
#define BIKE_PAD(ctrl) U32(0, 0x009c52c4u + 4u * (ctrl))

/* A call on an action object as 004308a0 makes it: EAX = vtable, ECX = object, EDX = the slot's function, ESI = controller, EDI =
 * the block. Returns the registers the callee leaves (EAX integer results, st0 float results). */
static rx_regs bike_sim_action_call(rx_addr obj, uint32_t off, int nargs, const uint32_t* args, uint32_t ctrl, rx_addr block) {
    rx_addr vt = U32(obj, 0), fn = U32(vt, off);
    rx_regs in = bike_sim_regs(), out;
    in.r[0] = vt; in.r[1] = obj; in.r[2] = fn; in.r[6] = ctrl; in.r[7] = block;
#ifndef RX_ORACLE   /* the product: the core's action function (an integer result; the float values: bike_sim_action_value) */
    if (rx_fn(fn)) { out = in; out.r[0] = rx_tcall(fn, obj, nargs, args); return out; }
#endif
    rx_call_original_ex(fn, &in, &out, nargs, args);
    return out;
}
/* slot 4 (+0x10): the action's value for controller ctrl, as the float it leaves on the x87 stack ((7, ctrl) pushed) */
static float bike_sim_action_value(rx_addr obj, uint32_t ctrl, rx_addr block) {
    uint32_t a[2] = {7, ctrl};
#ifndef RX_ORACLE   /* the product: the core's action function (this, mask, pad) -> float */
    {
        float (*f)(rx_addr, uint32_t, uint32_t) = RX_FN(U32(U32(obj, 0), 0x10), float (*)(rx_addr, uint32_t, uint32_t));
        if (f) { const rx_addr sp = rx_sp(); rx_set_sp(sp - 12u); const float v = f(obj, 7, ctrl); rx_set_sp(sp); return v; }
    }
#endif
    return bike_sim_action_call(obj, 0x10, 2, a, ctrl, block).st0;
}
/* slots +4 / +8 (button state), (7, ctrl) pushed: EAX */
static uint32_t bike_sim_action_state(rx_addr obj, uint32_t off, uint32_t ctrl, rx_addr block) {
    uint32_t a[2] = {7, ctrl};
    return bike_sim_action_call(obj, off, 2, a, ctrl, block).r[0];
}
/* A selector function of the action table (0054xxxx; world_misc.c): its argument controller in EAX (00548f20 / 00548f70) or ESI
 * (00548fc0 / 00549050), the action object in EAX. */
static rx_addr bike_sim_select_action(rx_addr fn, uint32_t ctrl, int in_eax) {
    (void)in_eax;
    rx_addr r;
    switch (fn) {
    case 0x00548f20: PHYS_CALL_SP(0, r = rx_world_pad_steer_map(ctrl)); break;
    case 0x00548f70: PHYS_CALL_SP(0, r = rx_world_pad_lean_map(ctrl)); break;
    case 0x00548fc0: PHYS_CALL_SP(0, r = rx_world_pad_wreck_steer_map(ctrl)); break;
    default:         PHYS_CALL_SP(0, r = rx_world_pad_wreck_lean_map(ctrl)); break;   /* 00549050 */
    }
    return r;
}

/* 004308a0 Simulation_ReadControllerInput (EAX = controller index, EDI = the 64-byte input block): samples the controller's actions
 * into the block (simulation-input.md). Controller -1: nothing. In order: the rider-reflex and steering stick pairs (the action
 * selectors depend on the controller preset), throttle, reverse (0 / 1), stunt modifier (0 / 1), brakes (separate front / rear
 * actions when either reports itself active, else the shared brake; emergency brake: front 0, rear 1; brake hold latch +0x38 forces
 * both to 1), clutch, shift up / down (0 / 1); with the stick throttle/brake flag +0x28 the stick can raise the throttle or set both
 * brakes to its magnitude; throttle or reverse above zero clears the brake hold. */
void bike_read_controller_input(uint32_t ctrl, rx_addr blk) {
    if (ctrl == 0xffffffffu) return;
    bike_sim_vcall(U32(0, 0x00a95844), 8, 0, 0);                                  /* engine context slot 2 (input manager getter) */
    rx_addr rider_x = bike_sim_select_action(0x00548fc0, ctrl, 0);
    rx_addr rider_y = bike_sim_select_action(0x00549050, ctrl, 0);
    float rider_xv = bike_sim_action_value(rider_x, ctrl, blk);
    float rider_yv = bike_sim_action_value(rider_y, ctrl, blk);
    rx_addr steer_x = bike_sim_select_action(0x00548f20, ctrl, 1);
    rx_addr steer_y = bike_sim_select_action(0x00548f70, ctrl, 1);
    float steer_xv = bike_sim_action_value(steer_x, ctrl, blk);
    float steer_yv = bike_sim_action_value(steer_y, ctrl, blk);
    SF32(blk, 0x10, steer_xv);          /* steering_x */
    SF32(blk, 0x14, steer_yv);          /* steering_y */
    SF32(blk, 0x18, rider_xv);          /* rider_reflex_x */
    SF32(blk, 0x1c, rider_yv);          /* rider_reflex_y */
    rx_addr pad = BIKE_PAD(ctrl);
    SF32(blk, 0x00, bike_sim_action_value(pad + 0x348, ctrl, blk));               /* throttle */
    float reverse = bike_sim_action_value(pad + 0x748, ctrl, blk);
    SF32(blk, 0x04, reverse == 0.0f ? 0.0f : 1.0f /* 00931f24 */);                /* reverse: fucompi, only equal -> 0 */
    {   /* 00548660: stunt modifier (steering stick button for preset 0x11) as 1.0 / 0.0 in xmm0 */
        float look_back;
        PHYS_CALL_SP(0, look_back = rx_world_pad_look_back(ctrl));
        SU32(blk, 0x20, look_back > 0.0f /* 0092e12c */ ? 1u : 0u);              /* stunt_modifier */
    }
    uint32_t one = 0;
    {   /* brakes: separate front / rear actions when either one is active (slot 11, +0x2c) */
        uint32_t c[1] = {ctrl};
        one = bike_sim_action_call(pad + 0x1134, 0x2c, 1, c, ctrl, blk).r[0];
        if (one == 0) one = bike_sim_action_call(pad + 0x11d8, 0x2c, 1, c, ctrl, blk).r[0];
        rx_addr rear;
        if (one != 0) {
            SF32(blk, 0x08, bike_sim_action_value(pad + 0x1134, ctrl, blk));      /* front_brake */
            rear = pad + 0x11d8;
        } else {
            SF32(blk, 0x08, bike_sim_action_value(pad + 0x148, ctrl, blk));
            rear = pad + 0x148;
        }
        SF32(blk, 0x0c, bike_sim_action_value(rear, ctrl, blk));                  /* rear_brake */
    }
    if (U32(blk, 0x3c) != 0 && bike_sim_action_state(pad + 0x800, 8, ctrl, blk) != 0) {
        SF32(blk, 0x08, 0.0f);                                                    /* emergency brake: front 0, rear 1 */
        SF32(blk, 0x0c, 1.0f);
    }
    if (U32(blk, 0x38) != 0) { SF32(blk, 0x08, 1.0f); SF32(blk, 0x0c, 1.0f); }   /* brake hold */
    SU32(blk, 0x2c, bike_sim_action_state(pad + 0x548, 8, ctrl, blk));            /* clutch */
    SU32(blk, 0x30, bike_sim_action_state(pad + 0xfec, 4, ctrl, blk) != 0);       /* shift_up */
    uint32_t down = bike_sim_action_state(pad + 0x1090, 4, ctrl, blk) != 0;
    SU32(blk, 0x34, down);                                                        /* shift_down */
    if (U32(blk, 0x28) != 0) {                                                    /* stick throttle / brake */
        float v;
        PHYS_CALL_SP(0, v = rx_controller_stick_drive_axis(ctrl));
        if (v > 0.0f) {
            float t = F32(blk, 0);
            SF32(blk, 0, v > t ? v : t);
        } else if (0.0f > v) {
            float a = bike_sim_fabs(v);
            SF32(blk, 0x08, a);
            SF32(blk, 0x0c, a);
        }
    }
    if (F32(blk, 0) > 0.0f || F32(blk, 4) > 0.0f) SU32(blk, 0x38, 0);              /* brake hold cleared */
}
RX_FUNCTION(0x004308a0, bike_read_controller_input)

/* 00433380 RavenSimulation::vf24 (slot 24, set input): copies the 64-byte block to S+0x1c, then zeroes every one of the first eight
 * floats (S+1c .. S+38) that is NaN or infinite (_finite), and applies the brake-hold / reverse rules: throttle above zero clears
 * the brake-hold latch S+54; while it is set both brakes (S+24, S+28) read 1.0; reverse S+20 becomes 1.0 when above zero, else 0. */
void bike_set_input_block(rx_addr S, rx_addr block) {
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(S, 0x1c + i, U32(block, i));
    for (uint32_t off = 0x1c; off <= 0x38; off += 4) {
        float f = F32(S, off);
        /* NaN -> zero; otherwise _finite(double(f)): a float is finite iff its exponent field is not all ones */
        if (f != f || (rx_fb(f) & 0x7f800000u) == 0x7f800000u) SF32(S, off, 0.0f);
    }
    if (F32(S, 0x1c) > 0.0f) SU32(S, 0x54, 0);
    if (U32(S, 0x54) != 0) { SF32(S, 0x24, 1.0f); SF32(S, 0x28, 1.0f); }
    SF32(S, 0x20, F32(S, 0x20) > 0.0f ? 1.0f : 0.0f);
}
RX_FUNCTION(0x00433380, bike_set_input_block)

/* 0044bff0 RavenSimulationMotocross::vf24 (slot 24): RavenSimulation::vf24, then the steering X S+2c into the control block +0x40. */
void bike_moto_set_input_block(rx_addr S, rx_addr block) {
    bike_set_input_block(S, block);
    SU32(U32(S, 0x5c30), 0x40, bike_x87_copy32(U32(S, 0x2c)));
}
RX_FUNCTION(0x0044bff0, bike_moto_set_input_block)

/* 0044bfb0 RavenSimulationMotocross::vf23 (slot 23, stack: controller): Simulation_ReadControllerInput into S+0x1c, then the steering
 * X S+2c into the control block +0x40 and the shift axes S+4c / S+50 into the control block +0x78 / +0x7c. */
void bike_moto_read_controller(rx_addr S, uint32_t controller) {
    bike_read_controller_input(controller, S + 0x1c);
    SU32(U32(S, 0x5c30), 0x40, bike_x87_copy32(U32(S, 0x2c)));
    SU32(U32(S, 0x5c30), 0x78, U32(S, 0x4c));
    SU32(U32(S, 0x5c30), 0x7c, U32(S, 0x50));
}
RX_FUNCTION(0x0044bfb0, bike_moto_read_controller)

/* ---- rest / activity tests (slots 25, 26, 28) -------------------------------------------------------------------------------- */

/* |v.xyz| > limit for the vec4 at addr (hsum_021, sqrtps, cmpltps limit < len, the three lanes summed == -3: the lanes are equal). */
static int bike_sim_vec_longer_than(rx_addr addr, float limit) {
    int result = limit < bike_sim_length_021(v4_ld(addr));
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
    if (rx_fourwheel_rest_frames_active()) rx_fourwheel_rest_frames_impulse(result);
#endif
    return result;
}

/* "The rider drives": the two pedal-like chains of vf25 / 26 / 28 on the input block (S+1c throttle, S+20 reverse, S+24 / S+28 the
 * brakes, S+48 a flag): throttle above zero with S+48 clear and no brake above zero, or reverse above zero and no brake above zero. */
static int bike_sim_driving_inputs(rx_addr S) {
    int brakes_idle = !(F32(S, 0x24) > 0.0f) && !(F32(S, 0x28) > 0.0f);
    int throttle = F32(S, 0x1c) > 0.0f && U32(S, 0x48) == 0 && brakes_idle;
    int reverse = F32(S, 0x20) > 0.0f && brakes_idle;
    return throttle || reverse;
}
/* S+2ec0 / 2ed0 / 2ee0: the external impulses; "large" when |2ec0| > 3.0 (0092fe48), |2ed0| > 1.0 or |2ee0| > 1.0 */
static int bike_sim_large_impulse(rx_addr S) {
    return bike_sim_vec_longer_than(S + 0x2ec0, 3.0f /* 0092fe48 */) || bike_sim_vec_longer_than(S + 0x2ed0, 1.0f /* 00931f24 */) ||
           bike_sim_vec_longer_than(S + 0x2ee0, 1.0f);
}

/* 004321a0 RavenSimulation::vf28: 1 on any driving activity: one of the axes S+2c .. S+38 is not zero (NaN counts), the driving
 * inputs above, the rider is off the bike (S+2374), or a large external impulse. */
uint32_t bike_sim_activity(rx_addr S) {
    int axes = 0;
    for (uint32_t off = 0x2c; off <= 0x38; off += 4) axes |= !(F32(S, off) == 0.0f);      /* ucomiss: only "equal" continues */
    int impulse = bike_sim_large_impulse(S);
    return axes || bike_sim_driving_inputs(S) || U32(S, 0x2374) != 0 || impulse;
}
RX_FUNCTION(0x004321a0, bike_sim_activity)

/* 00432360 RavenSimulation::vf26 (leave-rest test): slot 30 (+0x78) is called first; 1 when driving inputs, or slot 30 returned 0, or
 * the rider is off the bike, or a large external impulse. */
uint32_t bike_sim_leave_rest_test(rx_addr S) {
    uint32_t wheels = bike_sim_vcall(S, 0x78, 0, 0);
    int impulse = bike_sim_large_impulse(S);
    return bike_sim_driving_inputs(S) || wheels == 0 || U32(S, 0x2374) != 0 || impulse;
}
RX_FUNCTION(0x00432360, bike_sim_leave_rest_test)

/* 004324d0 RavenSimulation::vf25 (enter-rest test). speed = |S+100|; slot 29 (both wheels) and slot 30 are called first. 0 when the
 * driving inputs are active, slot 30 is 0 or the rider is off. Otherwise 1 when the bike is nearly stopped (speed < 1.0, a brake
 * above zero, S+a8 set and S+2e7c clear), else only when slot 29 was true, S+94 < 0.5 and S+2e7c is clear: then slot 31
 * (the wheel contact normal sum, into a local) must have y above 0.9962 (00977770). */
uint32_t bike_sim_enter_rest_test(rx_addr S) {
    float speed = bike_sim_length_021(v4_ld(S + 0x100));
    float d94 = F32(S, 0x94);
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
    if (rx_fourwheel_rest_frames_active()) rx_fourwheel_rest_frames_speed(speed, d94);
#endif
    uint32_t both = bike_sim_vcall(S, 0x74, 0, 0);
    uint32_t wheels = bike_sim_vcall(S, 0x78, 0, 0);
    if (bike_sim_driving_inputs(S) || wheels == 0 || U32(S, 0x2374) != 0) return 0;
    int stopped = 1.0f > speed && (F32(S, 0x24) > 0.0f || F32(S, 0x28) > 0.0f) && U32(S, 0xa8) != 0 && U32(S, 0x2e7c) == 0;
    int settled = both != 0 && 0.5f /* 009322cc */ > d94 && U32(S, 0x2e7c) == 0;
    if (stopped) {
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
        if (rx_fourwheel_rest_frames_active()) rx_fourwheel_rest_frames_stopped();
#endif
        return 1;
    }
    if (!settled) return 0;
#ifdef RX_FOURWHEEL_REST_FRAME_OBSERVERS
    if (rx_fourwheel_rest_frames_active()) {
        uint32_t arg = rx_fourwheel_rest_frames_normal_output();
        rx_addr res = bike_sim_vcall(S, 0x7c, 1, &arg);
        int result = 0.9962f < v4_ld(res).v[1];
        rx_fourwheel_rest_frames_normal_result(result);
        return result;
    }
#endif
    SIM_FRAME_ENTER(F, 0x20);
    uint32_t arg = F;
    rx_addr res = bike_sim_vcall(S, 0x7c, 1, &arg);
    int r = 0.9962f /* 00977770 */ < v4_ld(res).v[1];
    SIM_FRAME_LEAVE(F);
    return r;
}
RX_FUNCTION(0x004324d0, bike_sim_enter_rest_test)

/* ---- pending tuning reload -------------------------------------------------------------------------------------------------- */

/* 0043b640 RavenSimulationMotocross::vf8 (ECX = S): when a tuning XML is pending ([009e3100], set by the tuning editor; guarded by
 * the flag [009cc898] and the critical section 00ad7538): reads it with the XML reader (8252c0), applies it to the parameter block
 * S+2f10 (43a560) and, on success, logs (790540) and rebinds the masses (43b750); a failure clears the flag and the XML's +0xc;
 * a success frees the pending XML object (virtual destructor) and sets the flag again. Without a pending XML the flag is set. */
void bike_moto_apply_pending_tuning(rx_addr S) {
    const rx_addr PENDING = 0x009e3100, FLAG = 0x009cc898, CS = 0x00ad7538;
    if (U32(0, PENDING) != 0) {
        if (U32(0, FLAG) != 0) {
            uint32_t cs = CS;
            rx_os_import(0x0091600c, 1, &cs);              /* EnterCriticalSection */
            SIM_FRAME_ENTER(F, 0x470);
            rx_addr reader = F + 4;
            uint32_t zero = 0, eax;
            {   /* XMLReader_Construct 00824e70 (ESI = the reader, stack: the allocator 0) */
                const rx_addr sp = rx_sp();
                rx_set_sp(sp - 8);
                (void)zero;
                rx_xml_reader_construct(reader);
                rx_set_sp(sp);
            }
            {   /* read the XML: 008252c0 (stdcall reader, the pending stream object; RET 8), build_xml_stream.c */
                const rx_addr sp = rx_sp();
                rx_set_sp(sp - 12);
                eax = rx_xml_reader_load_stream(reader, U32(0, PENDING));
                rx_set_sp(sp);
            }
            if (eax != 0) {
                uint32_t a2[2] = {S + 0x2f10, reader};
                uint32_t ok;
                {   /* 0043a560 (stdcall parameter block, reader; RET 8): the mx parameters applied */
                    const rx_addr sp = rx_sp();
                    rx_set_sp(sp - 12);
                    ok = rx_veh_params_apply_typed(a2[0], a2[1], 0x0093b478u /* "mx" */, 0x0093b47cu /* "mx_bike" */);
                    rx_set_sp(sp);
                }
                if (ok != 0) {
                    rx_tuning_reload_log(0, 0x0043b6c2, 0x0043b6cb);      /* the log object (0042fed0) and its type-10 packet (00790540) */
                    {   /* 0043b750 (ESI = S): the masses of the mechanism */
                        const rx_addr sp = rx_sp();
                        rx_set_sp(sp - 4);
                        rx_sim_moto_setup_masses(S);
                        rx_set_sp(sp);
                    }
                    rx_addr xml = U32(0, PENDING);
                    if (xml != 0) {
                        rx_regs in = bike_sim_regs();
                        rx_addr vt = U32(xml, 0), fn = U32(vt, 0);
                        uint32_t one = 1;
                        in.r[0] = fn; in.r[1] = xml; in.r[2] = vt;
                        rx_call_original_ex(fn, &in, 0, 1, &one);                     /* delete the XML source object */
                    }
                    SU32(0, PENDING, 0);
                    SU32(0, FLAG, 1);
                } else {
                    rx_addr xml = U32(0, PENDING);
                    SU32(0, FLAG, 0);
                    SU32(xml, 0xc, 0);
                }
            }
            cs = CS;
            rx_os_import(0x009160f8, 1, &cs);              /* LeaveCriticalSection */
            {   /* 00824fe0 (ESI = the reader): the reader destructor (build_xml.c rx_xml_reader_destroy, the allocator-less reader) */
                const rx_addr sp = rx_sp();
                rx_set_sp(sp - 4);
                rx_xml_reader_destroy(reader);
                rx_set_sp(sp);
            }
            SIM_FRAME_LEAVE(F);
        }
        if (U32(0, PENDING) != 0) return;
    }
    SU32J(0, FLAG, 1);   /* (every MX simulation stores it: as the last one leaves it, in a parallel simulation batch) */
}
RX_FUNCTION(0x0043b640, bike_moto_apply_pending_tuning)

/* ---- state check ------------------------------------------------------------------------------------------------------------ */

/* 00434d00 FUN_00434d00 (ESI = S; called by every RavenVehicle*::vf29): clears S+10, then for each of the S+ec bodies ORs into it
 * (1 or all ones) a divergence flag: the orientation S+730+i*a0 has a NaN lane (1) or a lane whose bits equal those of +G or -G
 * (G = the vec4 [00adfdd0], -G = 0 - G; all ones); the sum of the body's state vectors (angular velocity +10, +40, torque +50, +70
 * past the orientation; position, velocity, S+130, force accumulators S+140 / 150 / 160 at -640 .. -5d0) has a NaN in x / y / z (1)
 * or a lane (x, y, z) equal to -G or +G as floats (all ones). Returns the accumulated flags in EAX and S+10. */
uint32_t bike_state_divergence_check(rx_addr S) {
    int32_t count = I32(S, 0xec);
    SU32(S, 0x10, 0);
    if (count <= 0) return 0;
    rx_v4 G = v4_ld(0x00adfdd0);
    rx_v4 negG = v4_sub(v4_zero(), G);
    uint32_t flags = 0;
    for (int32_t i = 0; i < count; i++) {
        rx_addr e = S + 0x730 + 0xa0u * (uint32_t)i;                   /* the body's orientation */
        rx_v4 q = v4_ld(e);
        uint32_t nan_q = (q.v[0] == q.v[0] && q.v[1] == q.v[1] && q.v[2] == q.v[2] && q.v[3] == q.v[3]) ? 0u : 1u;
        flags |= nan_q;
        for (int k = 0; k < 4; k++)
            if (v4_lb(q, k) == v4_lb(G, k) || v4_lb(q, k) == v4_lb(negG, k)) flags |= 0xffffffffu;
        rx_v4 sum = v4_add(v4_ld(e + 0x40), v4_ld(e + 0x10));
        static const int16_t more[8] = {0x50, 0x70, -0x640, -0x630, -0x600, -0x5f0, -0x5e0, -0x5d0};
        for (int j = 0; j < 8; j++) sum = v4_add(sum, v4_ld(e + (uint32_t)(int32_t)more[j]));
        int nan_sum = !(sum.v[0] == sum.v[0] && sum.v[1] == sum.v[1] && sum.v[2] == sum.v[2]);
        flags |= nan_sum ? 1u : 0u;
        for (int k = 0; k < 3; k++)
            if (negG.v[k] == sum.v[k] || sum.v[k] == G.v[k]) flags |= 0xffffffffu;
        SU32(S, 0x10, flags);
    }
    return flags;
}
RX_FUNCTION(0x00434d00, bike_state_divergence_check)

/* ---- developer keys ---------------------------------------------------------------------------------------------------------- */

/* The engine's input manager (engine context slot 2) and its key queries: slot 31 (+0x7c) key(code, 0) pressed this frame, slot 37
 * (+0x94) key(code, 0) held, slot 27 (+0x6c) / 33 (+0x84) the five-argument queries. vt_in_edx selects the register pattern of the
 * call site (mov eax,[ecx]; mov edx,[eax+off] vs mov edx,[ecx]; mov eax,[edx+off]). */
static uint32_t bike_sim_im_call(rx_addr im, uint32_t off, int vt_in_edx, int nargs, const uint32_t* args) {
    rx_addr vt = U32(im, 0), fn = U32(vt, off);
    if (rx_fn(fn)) return rx_tcall(fn, im, nargs, args);   /* the core's function (the product) */
    rx_regs in = bike_sim_regs(), out;
    in.r[0] = vt_in_edx ? fn : vt; in.r[1] = im; in.r[2] = vt_in_edx ? vt : fn;
    rx_call_original_ex(fn, &in, &out, nargs, args);
    return out.r[0];
}
static rx_addr bike_sim_input_manager(void) {
    rx_addr ctx = U32(0, 0x00a95844);
    return bike_sim_vcall(ctx, 8, 0, 0);
}
static uint32_t bike_sim_key(rx_addr im, uint32_t off, uint32_t code, int vt_in_edx) {
    uint32_t a[2] = {code, 0};
    return bike_sim_im_call(im, off, vt_in_edx, 2, a);
}

/* 0085c8b0 Mechanism_AddBodyForce (phys): ECX = mechanism, stack (body index, &vec4): adds the vector (given in the body frame,
 * w masked) to the body's force accumulator. */
static void bike_sim_mech_add_body_vector(rx_addr M, uint32_t body, rx_addr vec) {
    phys_mech_add_body_force(M, body, vec);
}

/* 00435320 Simulation_ApplyDebugKeyForces (ECX = S, local simulations only): the push tool. With body index S+2eac, force S+2ea8
 * and torque S+2ea4: keys 0x75 / 0x77 add the force along the body's local x / (z, w) axis (S+110 + body * a0) through 85c8b0, key
 * 0x76 adds it along y directly into the force accumulator S+140, keys 0x78 / 0x79 / 0x7a add the torque along the local axes S+750
 * (x / y / z,w) into the torque accumulator S+780, key 0x74 stops the mechanism (85c4b0 with zero). */
void bike_apply_debug_key_forces(rx_addr S) {
    rx_addr im = bike_sim_input_manager();
    rx_addr M = S + 0xd0;
    SIM_FRAME_ENTER(F, 0x30);
    for (int k = 0; k < 3; k++) {                                              /* keys 0x75, 0x76, 0x77 */
        if (bike_sim_key(im, 0x94, 0x75 + (uint32_t)k, 0) == 0) continue;
        uint32_t idx = U32(S, 0x2eac);
        float f = F32(S, 0x2ea8);
        rx_v4 axis = v4_ld(S + 0x110 + 0xa0u * idx);
        rx_v4 mask = k == 0 ? v4(f, 0.0f, 0.0f, 0.0f) : k == 1 ? v4(0.0f, f, 0.0f, 0.0f) : v4(0.0f, 0.0f, f, f);
        if (k == 1) {
            rx_addr acc = S + 0x140 + 0xa0u * idx;
            v4_st(acc, v4_add(v4_mul(mask, axis), v4_ld(acc)));
        } else {
            v4_st(F + 0x10, v4_mul(mask, axis));
            bike_sim_mech_add_body_vector(M, idx, F + 0x10);
        }
    }
    for (int k = 0; k < 3; k++) {                                              /* keys 0x78, 0x79, 0x7a */
        if (bike_sim_key(im, 0x94, 0x78 + (uint32_t)k, 0) == 0) continue;
        uint32_t idx = U32(S, 0x2eac);
        float t = F32(S, 0x2ea4);
        rx_v4 mask = k == 0 ? v4(t, 0.0f, 0.0f, 0.0f) : k == 1 ? v4(0.0f, t, 0.0f, 0.0f) : v4(0.0f, 0.0f, t, t);
        rx_addr acc = S + 0x780 + 0xa0u * idx;
        v4_st(acc, v4_add(v4_mul(mask, v4_ld(S + 0x750 + 0xa0u * idx)), v4_ld(acc)));
    }
    if (bike_sim_key(im, 0x94, 0x74, 0) != 0) bike_sim_mech_set_velocity(M, 0x00d61670);
    SIM_FRAME_LEAVE(F);
}
RX_FUNCTION(0x00435320, bike_apply_debug_key_forces)

/* 00435030 Simulation_HandleDebugKeys (EDI = S): the developer keys of the input manager.
 *  0x2e  pose-override test mode 4 on / off (prints a message; on: saves S+730 / S+f0 to S+2e50 / S+2e60 and stops the mechanism)
 *  0x2f  gravity toggle ([00d92d48]: the mechanism's gravity vec4 (0, -32.2, 0, 0) or zero + stop)
 *  S+2ea0 push-tool active: the controller query (1, 0, 9, 0, 1) toggles S+b0 (scheduling); key 0x65 toggles S+2ea0
 *  the query (1, 0, 0xb, 0, 1) or [009cc894] set: clears the mechanism's forces (85c9d0) and [009cc894], nothing else; else keys
 *  0x81 / 0x80 scale force and torque (S+2ea8, S+2ea4) by 1.5 / 2/3, 0x83 / 0x82 step the body index S+2eac, 0x7e negates force
 *  and torque, 0x36 toggles S+2eb0. */
void bike_handle_debug_keys(rx_addr S) {
    rx_addr im = bike_sim_input_manager();
    rx_addr M = S + 0xd0;
    if (bike_sim_key(im, 0x7c, 0x2e, 0) != 0) {
        uint32_t msg;
        if (U32(S, 0x2de4) == 4) {
            msg = 0x0093a744;
            bike_debug_print(msg);
            SU32(S, 0x2de0, 0);
            SU32(S, 0x2de4, 0);
        } else {
            msg = 0x0093a764;
            bike_debug_print(msg);
            SU32(S, 0x2de4, 4);
            v4_st(S + 0x2e50, v4_ld(S + 0x730));
            v4_st(S + 0x2e60, v4_ld(S + 0xf0));
            bike_sim_mech_set_velocity(M, 0x00d61670);
        }
    }
    if (bike_sim_key(im, 0x7c, 0x2f, 0) != 0) {
        if (U32(0, 0x00d92d48) != 0) {
            v4_st(S + 0xd0, v4(0.0f, -32.200001f /* 00977688 */, 0.0f, 0.0f));
        } else {
            bike_sim_mech_set_velocity(M, 0x00d61670);
            v4_st(S + 0xd0, v4_zero());
        }
        SU32(0, 0x00d92d48, U32(0, 0x00d92d48) == 0 ? 1u : 0u);
    }
    if (U32(S, 0x2ea0) != 0) {
        uint32_t a[5] = {1, 0, 9, 0, 1};
        if (bike_sim_im_call(im, 0x6c, 1, 5, a) != 0) SU32(S, 0xb0, U32(S, 0xb0) == 0 ? 1u : 0u);
    }
    if (bike_sim_key(im, 0x7c, 0x65, 1) != 0) SU32(S, 0x2ea0, U32(S, 0x2ea0) == 0 ? 1u : 0u);
    uint32_t a5[5] = {1, 0, 0xb, 0, 1};
    uint32_t held = bike_sim_im_call(im, 0x84, 1, 5, a5);
    if (held != 0 || U32(0, 0x009cc894) != 0) {
        PHYS_CALL_SP(0, phys_mech_clear_forces(M));                           /* Mechanism_ClearForces (EDX = M) */
        SU32(0, 0x009cc894, 0);
        return;
    }
    if (bike_sim_key(im, 0x7c, 0x81, 1) != 0) {
        SF32(S, 0x2ea8, phys_mul(F32(S, 0x2ea8), 1.5f /* 009322a4 */));
        SF32(S, 0x2ea4, phys_mul(F32(S, 0x2ea4), 1.5f));
    }
    if (bike_sim_key(im, 0x7c, 0x80, 1) != 0) {
        SF32(S, 0x2ea8, phys_mul(F32(S, 0x2ea8), 0.6666667f /* 00977714 */));
        SF32(S, 0x2ea4, phys_mul(F32(S, 0x2ea4), 0.6666667f));
    }
    if (bike_sim_key(im, 0x7c, 0x83, 1) != 0) {
        SU32(S, 0x2eac, U32(S, 0x2eac) + 1);
        if (U32(S, 0x2eac) == U32(S, 0xec)) SU32(S, 0x2eac, 0);
    }
    if (bike_sim_key(im, 0x7c, 0x82, 1) != 0) {
        SU32(S, 0x2eac, U32(S, 0x2eac) - 1);
        if (I32(S, 0x2eac) < 0) SU32(S, 0x2eac, U32(S, 0xec) - 1);
    }
    if (bike_sim_key(im, 0x7c, 0x7e, 1) != 0) {
        SF32(S, 0x2ea4, phys_sub(-0.0f /* 009774a8 */, F32(S, 0x2ea4)));
        SF32(S, 0x2ea8, phys_sub(-0.0f, F32(S, 0x2ea8)));
    }
    if (bike_sim_key(im, 0x7c, 0x36, 1) != 0) SU32(S, 0x2eb0, U32(S, 0x2eb0) == 0 ? 1u : 0u);
}
RX_FUNCTION(0x00435030, bike_handle_debug_keys)

/* ---- external impulse ----------------------------------------------------------------------------------------------------- */

/* 00768... DynObj_TakeContactResults (007684c0, ESI = out S+2ec0, EDI = out S+2ed0, stack (object, out S+2ee0), ret 8): copies the
 * external object's accumulated impulses to the three vec4s and clears them (zero when it has no body). */
static void bike_sim_take_contact_results(rx_addr S, rx_addr obj) {
    PHYS_CALL_SP(8, rx_world_dynobj_take_contact_results(obj, S + 0x2ec0, S + 0x2ed0, S + 0x2ee0));
}

/* 004355d0 Simulation_ApplyExternalImpulse (stack S): when an external object S+2ebc is attached and S+5c is clear: fetches and clears
 * its accumulated results into S+2ec0 (linear impulse), S+2ed0 (angular impulse) and S+2ee0 (displacement, which translates the
 * whole mechanism). With a nonzero linear impulse (S+c |= 1): the angular impulse is clamped to length 7.5 (10.0 when both wheels
 * touch, slot 29), and every body i gets
 *   offset R = rotate_inv(q, pos_i - pos_0),  v_i += S+2ec0 + rotate(q, w x R),
 *   w_i += rotate_inv(q_i, rotate(q, w)),   with q = the root orientation S+730 and w the angular impulse.
 * Returns how far it got (for the adapter's register state): 0 nothing attached, 1 no linear impulse, 2 applied. */
int bike_apply_external_impulse(rx_addr S) {
    rx_addr obj = U32(S, 0x2ebc);
    if (obj == 0 || U32(S, 0x5c) != 0) return 0;
    bike_sim_take_contact_results(S, obj);
    bike_sim_mech_translate(S + 0xd0, S + 0x2ee0);
    rx_v4 zero = bike_g_zero();
    float len = bike_sim_length_021(v4_ld(S + 0x2ec0));
    int moving = zero.v[0] < len && zero.v[1] < len && zero.v[2] < len;
    if (moving) SU32(S, 0xc, U32(S, 0xc) | 1u);
    if (!moving) return 1;
    rx_v4 q = v4_ld(S + 0x730);
    float wlen = bike_sim_length_021(v4_ld(S + 0x2ed0));
    uint32_t both = bike_sim_vcall(S, 0x74, 0, 0);
    float limit = both == 0 ? 7.5f /* 0097745c */ : 10.0f /* 0092fd88 */;
    if (limit < wlen) {
        rx_v4 w = v4_ld(S + 0x2ed0);
        v4_st(S + 0x2ed0, v4_div(v4_mul(w, v4_splat(limit)), v4_splat(wlen)));
    }
    rx_v4 conj_q = v4_mul(q, v4(-1.0f, -1.0f, -1.0f, 1.0f));
    for (int32_t i = 0; i < I32(S, 0xec); i++) {
        rx_addr body = S + 0x100 + 0xa0u * (uint32_t)i;                     /* velocity of body i; position at -0x10, orientation +0x630 */
        rx_v4 d = v4_xyz(v4_sub(v4_ld(body - 0x10), v4_ld(S + 0xf0)));
        rx_v4 R = bike_quat_mul(conj_q, bike_quat_mul(d, q));
        rx_v4 w = v4_ld(S + 0x2ed0);
        rx_v4 Rp = v4(R.v[2], R.v[0], R.v[1], R.v[3]), Rpp = v4(R.v[1], R.v[2], R.v[0], R.v[3]);
        rx_v4 wp = v4(w.v[1], w.v[2], w.v[0], w.v[3]), wpp = v4(w.v[2], w.v[0], w.v[1], w.v[3]);
        rx_v4 X = v4_xyz(v4_sub(v4_mul(Rp, wp), v4_mul(Rpp, wpp)));          /* w x R */
        rx_v4 Y1 = bike_quat_mul(X, conj_q);
        rx_v4 Y2 = bike_quat_mul(v4_xyz(w), conj_q);
        rx_v4 Wrot = v4_xyz(bike_quat_mul(q, Y2));                           /* rotate(q, w) */
        rx_v4 qi = v4_ld(body + 0x630);
        rx_v4 Wi = bike_quat_mul(v4_mul(qi, v4(-1.0f, -1.0f, -1.0f, 1.0f)), bike_quat_mul(Wrot, qi));
        rx_v4 angvel = v4_add(Wi, v4_ld(body + 0x640));
        rx_v4 V = bike_quat_mul(q, Y1);
        rx_v4 vel = v4_add(V, v4_add(v4_ld(S + 0x2ec0), v4_ld(body)));
        v4_st(body, vel);
        v4_st(body + 0x640, angvel);
    }
    return 2;
}
RX_FUNCTION(0x004355d0, bike_apply_external_impulse)

/* ---- the fixed step ----------------------------------------------------------------------------------------------------------- */

/* The mechanism kernels (phys code, run through the original; conventions from their entries). */
static void bike_sim_mech_stdcall(rx_addr fn, rx_addr M) {
    void (*f)(rx_addr) = RX_FN(fn, void (*)(rx_addr));
    if (f) { PHYS_CALL_SP(4, f(M)); return; }
    uint32_t a = M; rx_call_original(fn, RX_STDCALL, 0, 1, &a);
}
/* 0085c9d0 Mechanism_ClearForces: EDX = mechanism; zeroes the force / impulse accumulators of every body. */
static void bike_sim_mech_clear_forces(rx_addr M) {
    PHYS_CALL_SP(0, phys_mech_clear_forces(M));
}
/* 0085ca40 Mechanism_ComposeForces: EDI = mechanism, stack dt. */
static void bike_sim_mech_compose_forces(rx_addr M, float dt) {
    PHYS_CALL_SP(4, phys_mech_compose_forces(M, dt));
}
/* 0085d270 Mechanism_IntegrateVelocities: EDX = mechanism, stack dt. */
static void bike_sim_mech_integrate_velocities(rx_addr M, float dt) {
    PHYS_CALL_SP(4, rx_mechanism_integrate_velocities(M, dt));
}
/* 0085d100 Mechanism_IntegratePoses: ECX = mechanism, XMM6 = dt. */
static void bike_sim_mech_integrate_poses(rx_addr M, float dt) {
    PHYS_CALL_SP(0, phys_mech_integrate_poses(M, dt));
}
/* the virtual slot called with the step dt pushed through the x87 stack (fld ; fstp [esp]) */
static void bike_sim_vcall_dt(rx_addr S, uint32_t off, float dt) {
    uint32_t a = bike_x87_copy32(rx_fb(dt));
    const rx_addr fn = U32(U32(S, 0), off);
    void (*f)(rx_addr, float) = fn == 0x00679000 ? NULL : RX_FN(fn, void (*)(rx_addr, float));   /* 00679000 takes an integer */
    if (f) { PHYS_CALL_SP(4, f(S, rx_bf(a))); return; }
    bike_sim_vcall(S, off, 1, &a);
}

/* 00431d60 RavenSimulation_IntegrateStep (ESI = S, stack fixed dt): one physics step of the vehicle simulation (the order is
 * bike-connected-path.md): replay follow when S+2e7c; slot 9 (control update); mechanism force reset; external impulse; the push tool
 * (local simulations, S+4 == 0); slot 11 (forces); the mechanism's constraint solve (node slot 2, prepare, compose, node slot 3,
 * solve); velocity integration; slot 12 (unless a pose override other than mode 1 is active); slot 15; pose integration; slot 16;
 * node slot 6; slot 14; slots 13 and 17. Then the speed filter: S+94 += (|v| - S+94) * a with a = min(S+bc, S+98) / S+98
 * (a is kept in S+9c). */
void bike_integrate_step(rx_addr S, float dt) {
    rx_addr M = S + 0xd0;
    if (U32(S, 0x2e7c) != 0) {                                                 /* ReplayFollow_Step 004343e0: EDI = S, stack dt */
        const rx_addr replay_entry_sp=rx_sp();
        const rx_addr replay_caller_sp=((replay_entry_sp-4)&~15u)-0x10;
        const uint32_t replay_dt=bike_x87_copy32(rx_fb(dt));
        SU32(replay_caller_sp,-4,replay_dt);
        SU32(replay_caller_sp,-8,0x00431d81);
        rx_set_sp(replay_caller_sp);
        PHYS_CALL_SP(4, rx_replay_follow_step(S,rx_bf(replay_dt)));
        rx_set_sp(replay_entry_sp);
    }
    bike_sim_vcall_dt(S, 0x24, dt);                                            /* slot 9 */
    bike_sim_mech_clear_forces(M);
    bike_apply_external_impulse(S);
    if (U32(S, 4) == 0) bike_apply_debug_key_forces(S);
    bike_sim_vcall_dt(S, 0x2c, dt);                                            /* slot 11 */
    bike_sim_mech_stdcall(0x0085c6d0, M);                                      /* node slot 2 */
    bike_sim_mech_stdcall(0x0085cb70, M);                                      /* prepare */
    bike_sim_mech_compose_forces(M, rx_bf(bike_x87_copy32(rx_fb(dt))));
    bike_sim_mech_stdcall(0x0085c700, M);                                      /* node slot 3 */
    bike_sim_mech_stdcall(0x0085cf80, M);                                      /* solve */
    bike_sim_mech_integrate_velocities(M, rx_bf(bike_x87_copy32(rx_fb(dt))));
    if (U32(S, 0x2de0) == 0 || U32(S, 0x2de4) == 1) bike_sim_vcall_dt(S, 0x30, dt);   /* slot 12 */
    bike_sim_vcall_dt(S, 0x3c, dt);                                            /* slot 15 */
    bike_sim_mech_integrate_poses(M, dt);
    bike_sim_vcall(S, 0x40, 0, 0);                                             /* slot 16 */
    bike_sim_mech_stdcall(0x0085d300, M);                                      /* node slot 6 */
    bike_sim_vcall(S, 0x38, 0, 0);                                             /* slot 14 */
    bike_sim_vcall_dt(S, 0x34, dt);                                            /* slot 13 */
    bike_sim_vcall_dt(S, 0x44, dt);                                            /* slot 17 */
    float speed = bike_sim_length_021(v4_ld(S + 0x100));
    float h = F32(S, 0xbc), k = F32(S, 0x98), prev = F32(S, 0x94);
    float a = k > h ? h : k;
    a = phys_mul(a, phys_div(1.0f /* 00931f24 */, k));
    SF32(S, 0x9c, a);
    SF32(S, 0x94, phys_add(phys_mul(phys_sub(speed, prev), a), prev));
}
RX_FUNCTION(0x00431d60, bike_integrate_step)

/* ---- the fixed-step scheduler ----------------------------------------------------------------------------------------------- */

/* The double-precision arctangent-like polynomial of the orientation interpolation (a Pade-type approximation of asin; every
 * intermediate rounded to float as the code's cvtsd2ss / cvtss2sd chain does). */
static float bike_sim_asin_approx(float x) {
    const double D0 = rx_bd(0x3fe212eb00000000ull) /* 009777b0 */, D1 = rx_bd(0x3fa9903ac0000000ull) /* 009777a8 */;
    const double D2 = rx_bd(0x3ff716ac60000000ull) /* 009777a0 */, D3 = rx_bd(0x3fd98e2fe0000000ull) /* 00977798 */;
    const double D4 = rx_bd(0x3f93d7f000000000ull) /* 00977790 */, D5 = rx_bd(0x3ff46c01a0000000ull) /* 00977788 */;
    float x2 = (float)((double)x * (double)x);
    float x4 = (float)((double)x2 * (double)x2);
    float t1 = (float)(D0 - (double)x2 * D1);
    double m1 = (double)t1 * (double)x4;
    float t2 = (float)(1.0 /* 00977638 */ - (double)x2 * D2);
    float inv = (float)(1.0 / (m1 + (double)t2));
    float t3 = (float)(D3 - (double)x2 * D4);
    double m2 = (double)t3 * (double)x4;
    float t4 = (float)(1.0 - (double)x2 * D5);
    float p = (float)((m2 + (double)t4) * (double)x);
    return (float)((double)inv * (double)p);
}

/* acos(|dot|) as the interpolation computes it: from asin_approx of |dot| (< 0.7071, 009ddfc8) or of sqrt(1 - dot^2) otherwise. */
static float bike_sim_acos_approx(float ac) {
    int big = !(ac < rx_bf(0x3f350481u) /* 009ddfc8: 0.7071 */);
    float x;
    if (big) {
        float ac2 = (float)((double)ac * (double)ac);
        float d = (float)(1.0 - (double)ac2);
        x = phys_sqrt(bike_sim_fabs(d));                    /* 0042c230 SqrtF = sqrtss */
    } else {
        x = ac;
    }
    float r = bike_sim_asin_approx(x);
    if (big) r = (float)(rx_bd(0x3ff921fb60000000ull) /* 00977510: pi/2 */ - (double)r);
    float signed_r = ac >= 0.0f ? r : phys_sub(-0.0f, r);   /* comiss ac, 0 ; jb */
    return (float)(rx_bd(0x3ff921fb60000000ull) - (double)signed_r);
}

/* The orientation / position interpolation of the scheduler's two tails (430fb0 for the no-step update, 431750 after the steps; the
 * two copies of the code differ only in the order of some commutative operands, `after` selects those). Per body i: the displayed
 * position S+180 = prev * b + cur * a (prev = S+170, cur = S+f0; a = 1 - lead / h, b = 1 - a) and the displayed orientation S+7c0 =
 * normalize(slerp(prev S+7b0, cur S+730)) with the weights sin((1 - a) theta) / sin(theta) and sin(a theta) / sin(theta),
 * theta = acos(|dot|) (the sine from the polynomial; |dot| >= 1: prev only). */
static void bike_sim_interpolate_bodies(rx_addr S, float a, int after) {
    float b = phys_sub(1.0f, a);
    rx_v4 va = v4_splat(a), vb = v4_splat(b);
    int32_t count = I32(S, 0xec);
    for (int32_t i = 0; i < count; i++) {
        uint32_t o = 0xa0u * (uint32_t)i;
        rx_v4 prev = v4_ld(S + 0x170 + o), cur = v4_ld(S + 0xf0 + o);
        v4_st(S + 0x180 + o, after ? v4_add(v4_mul(cur, va), v4_mul(vb, prev)) : v4_add(v4_mul(prev, vb), v4_mul(va, cur)));
        rx_v4 qp = v4_ld(S + 0x7b0 + o), qc = v4_ld(S + 0x730 + o);
        float dot = after ? phys_add(phys_mul(qc.v[1], qp.v[1]), phys_mul(qc.v[0], qp.v[0]))
                          : phys_add(phys_mul(qc.v[0], qp.v[0]), phys_mul(qp.v[1], qc.v[1]));
        dot = phys_add(dot, phys_mul(after ? qc.v[2] : qp.v[2], after ? qp.v[2] : qc.v[2]));
        dot = phys_add(dot, phys_mul(after ? qp.v[3] : qc.v[3], after ? qc.v[3] : qp.v[3]));
        float ac = bike_sim_fabs(dot);
        float theta = bike_sim_acos_approx(ac);
        float sin_theta = bike_sim_trig(phys_add(theta, 1.5707964f /* 00977508 */));
        float inv = phys_div(1.0f, sin_theta);
        float phi = phys_mul(theta, a);
        float wp = !(ac >= 1.0f) ? phys_mul(bike_sim_trig(phys_add(phys_sub(theta, phi), 1.5707964f)), inv) : 1.0f;
        float wc = !(ac >= 1.0f) ? phys_mul(bike_sim_trig(phys_add(phi, 1.5707964f)), inv) : 0.0f;
        rx_v4 q;
        q.v[3] = after ? phys_add(phys_mul(qp.v[3], wp), phys_mul(qc.v[3], wc)) : phys_add(phys_mul(qc.v[3], wc), phys_mul(qp.v[3], wp));
        q.v[0] = phys_add(phys_mul(qc.v[0], wc), phys_mul(qp.v[0], wp));
        q.v[1] = after ? phys_add(phys_mul(qc.v[1], wc), phys_mul(qp.v[1], wp)) : phys_add(phys_mul(qp.v[1], wp), phys_mul(qc.v[1], wc));
        q.v[2] = after ? phys_add(phys_mul(qc.v[2], wc), phys_mul(qp.v[2], wp)) : phys_add(phys_mul(qp.v[2], wp), phys_mul(qc.v[2], wc));
        v4_st(S + 0x7c0 + o, bike_sim_normalize4(q));
    }
}

/* 00431cf0 (ECX = source, stack destination): copies a 0x64-byte replay state record (x87 copies of the scalars). */
static void bike_sim_copy_replay_record(rx_addr dst, rx_addr src) {
    for (uint32_t off = 0; off < 0x40; off += 16) v4_st(dst + off, v4_ld(src + off));
    SU32(dst, 0x40, bike_x87_copy32(U32(src, 0x40)));
    SU32(dst, 0x44, U32(src, 0x44));
    for (uint32_t off = 0x48; off <= 0x60; off += 4) SU32(dst, off, bike_x87_copy32(U32(src, off)));
}

/* prev := cur for every body (S+170 <- S+f0, S+7b0 <- S+730) before a step */
static void bike_sim_save_previous_state(rx_addr S) {
    for (int32_t i = 0; i < I32(S, 0xec); i++) {
        uint32_t o = 0xa0u * (uint32_t)i;
        v4_st(S + 0x170 + o, v4_ld(S + 0xf0 + o));
        v4_st(S + 0x7b0 + o, v4_ld(S + 0x730 + o));
    }
}

/* 00432680 (EDI = S; stack dt; not in plan/rx_bike.tsv): the remote-simulation smoothing step, run by UpdateFixedSteps for a
 * type 1 simulation (S+2e7c = 0). The remote poses arrive in S+2da0 (translation delta) / S+2db0 (linear velocity delta) /
 * S+2dc0 (incremental orientation) / S+2dd0 (angular velocity delta); the weight k = dt / ((S+2c90 - S+2d70) * 0.5 * 3)
 * (clamped to 1) scales them: q = normalize(slerp(identity 0xd61640, S+2dc0, k)); the mechanism's node positions += k*S+2da0,
 * the mechanism is rotated by q, the bodies' linear velocities += k*S+2db0, the root angular velocity S+740 += d3 = k*S+2dd0
 * (as is), and with r = rotate(root orientation S+7c0, d3) (root * d3 * conj(root), 00432819..004328e8) every other body's
 * velocity += r x (com_i - com_0) (com at body+0x80, com_0 at S+180) and its angular velocity += rotate_inv(world orientation
 * body+0x6c0, r) (w-remote4w: the port had r = rotate_inv(root, d3) and added r to S+740; found by the four-wheel remotes,
 * whose wheels sit far from the chassis). Then the 20 replay-state ring at S+2390
 * (stride 0x70: position +0x00, orientation +0x10, velocity +0x20, angular velocity +0x30) is advanced by the same deltas,
 * the four S+2da0.. fields shrink by their deltas, and S+2dc0 = conj(q) * S+2dc0. */
void bike_remote_smooth(rx_addr S, float dt) {
    const float sum = phys_add(phys_sub(F32(S, 0x2d00), F32(S, 0x2d70)), phys_sub(F32(S, 0x2c90), F32(S, 0x2d00)));
    const float k = phys_div(dt, phys_mul(phys_mul(sum, 0.5f /* 009322cc */), 3.0f /* 0092fe48 */));
    const float c = 1.0f /* 00931f24 */ > k ? k : 1.0f;
    const rx_v4 d1 = v4_mul(v4_splat(c), v4_ld(S + 0x2da0));
    const rx_v4 d2 = v4_mul(v4_splat(c), v4_ld(S + 0x2db0));
    const rx_v4 q = bike_qnormalize(phys_quat_slerp(rx_ld4(0x00d61640), v4_ld(S + 0x2dc0), v4_splat(c)));
    const rx_v4 d3 = v4_mul(v4_splat(c), v4_ld(S + 0x2dd0));
    const rx_addr M = S + 0xd0;
    const int32_t n = I32(S, 0xec);

    for (int32_t i = 0; i < n; i++) {                                  /* node positions */
        rx_addr lin = S + 0xf0 + 0xa0u * (uint32_t)i;
        v4_st(lin, v4_add(v4_ld(lin), d1));
    }
    const rx_addr sp = rx_sp(), A = (sp - 4u) & 0xfffffff0u, F = A - 0x7cu;   /* the original's frame (q at [esp+0x50]) */
    rx_set_sp(F);
    v4_st(F + 0x4c, q);
    phys_mech_rotate(M, F + 0x4c);
    for (int32_t i = 0; i < n; i++) {                                  /* body linear velocities */
        rx_addr vel = S + 0x100 + 0xa0u * (uint32_t)i;
        v4_st(vel, v4_add(v4_ld(vel), d2));
    }
    const rx_addr root = S + 0x7c0;
    const rx_v4 r = bike_rotate(v4_ld(root), d3);                     /* root * d3 * conj(root): the delta in the world frame */
    v4_st(S + 0x740, v4_add(v4_ld(S + 0x740), d3));                   /* the root's own angular velocity takes the delta as is */
    const rx_v4 com0 = v4_ld(S + 0x180);
    for (int32_t i = 1; i < n; i++) {                                  /* velocity field of the angular delta */
        rx_addr vel = S + 0x100 + 0xa0u * (uint32_t)i;
        const rx_v4 lever = v4_sub(v4_ld(vel + 0x80), com0);
        v4_st(vel, v4_add(v4_ld(vel), bike_cross(r, lever)));
        const rx_v4 w = v4_ld(vel + 0x6c0);
        v4_st(vel + 0x640, v4_add(v4_ld(vel + 0x640), bike_rotate_inv(w, r)));
    }
    for (uint32_t i = 0; i < 20; i++) {                                /* the ring of replay states */
        rx_addr e = S + 0x2390u + 0x70u * i;
        v4_st(e, v4_add(v4_ld(e), d1));
        v4_st(e + 0x10, bike_quat_mul(v4_ld(e + 0x10), q));
        v4_st(e + 0x20, v4_add(v4_ld(e + 0x20), d2));
        v4_st(e + 0x30, v4_add(v4_ld(e + 0x30), d3));
    }
    rx_set_sp(sp);
    v4_st(S + 0x2da0, v4_sub(v4_ld(S + 0x2da0), d1));
    v4_st(S + 0x2db0, v4_sub(v4_ld(S + 0x2db0), d2));
    v4_st(S + 0x2dc0, bike_quat_mul(bike_quat_conj(q), v4_ld(S + 0x2dc0)));
    v4_st(S + 0x2dd0, v4_sub(v4_ld(S + 0x2dd0), d3));
}
RX_FUNCTION(0x00432680, bike_remote_smooth)

/* 00430ef0 RavenSimulation::UpdateFixedSteps (ECX = S, stack frameDt): the fixed-step scheduler (physics-timing.md). A remote
 * (type 1) simulation with S+237c clear does nothing. deficit = frameDt - lead (S+c0): when it is not positive only the lead
 * shrinks and the bodies' displayed state is interpolated. Otherwise n = ceil(deficit / h) steps (h = S+bc), capped at S+b8 (the lead
 * then restarts at 0, else n h - deficit); then per type: local / type 2 pose override (slot 34), remote replay (00432680); the
 * per-update flags are cleared and slot 3 called; unless the override mode (S+2de4) is 3 / 4 each step saves prev := cur, calls
 * slot 6, IntegrateStep, accumulates the contact flags S+14 / S+18 / S+2e74 (slots 4 / 5 / 28), S+2e78 = slot 27, slot 7. Then the
 * interpolation with the new lead, and for local simulations a ring of the last 20 replay states (S+2390, stride 0x70). */
void bike_update_fixed_steps(rx_addr S, float frame_dt) {
    if (U32(S, 4) == 1 && U32(S, 0x237c) == 0) return;
    float lead = F32(S, 0xc0);
    float deficit = phys_sub(frame_dt, lead);
    if (!(0.0f < deficit || deficit != deficit)) {               /* comiss 0, deficit ; jb: deficit > 0 (or NaN) schedules steps */
        float nl = phys_sub(lead, frame_dt);
        SF32(S, 0xc0, nl);
        float a = phys_sub(1.0f /* 00931f24 */, phys_div(nl, F32(S, 0xbc)));
        SU32(S, 0xb4, 0);
        if (I32(S, 0xec) <= 0) return;
        bike_sim_interpolate_bodies(S, a, 0);
        return;
    }
    float h = F32(S, 0xbc);
    int32_t n = rx_cvtt(ceil((double)phys_div(deficit, h)));      /* CeilF 0042c1d0 + _ftol2_sse: ceil, then truncate */
    int32_t max_steps = I32(S, 0xb8);
    float new_lead;
    SU32(S, 0xb4, (uint32_t)n);
    if (n > max_steps) { new_lead = 0.0f; SU32(S, 0xb4, (uint32_t)max_steps); }
    else new_lead = phys_sub(phys_mul((float)n, F32(S, 0xbc)), deficit);
    SF32(S, 0xc0, new_lead);
    uint32_t type = U32(S, 4);
    if (type == 1) {
        if (U32(S, 0x2e7c) == 0) {                              /* 00432680 (EDI = S, stack dt) */
            uint32_t arg = bike_x87_copy32(rx_fb(frame_dt));
            PHYS_CALL_SP(4, bike_remote_smooth(S, rx_bf(arg)));
        }
    } else if (type == 0 || type == 2) {
        if (U32(S, 0x2de0) != 0 && U32(S, 0x2e7c) == 0) bike_sim_vcall(S, 0x88, 0, 0);          /* slot 34 */
    }
    SU32(S, 0x14, 0); SU32(S, 0x18, 0); SU32(S, 0x2370, 0); SU32(S, 0x2e74, 0); SU32(S, 0x2e78, 0);
    SU32(S, 0x10, 0); SU32(S, 0xc, 0); SU32(S, 0x2e9c, 0);
    bike_sim_vcall(S, 0xc, 0, 0);                                                              /* slot 3 */
    uint32_t mode = U32(S, 0x2de4);
    if (mode == 3 || mode == 4) {
        bike_sim_save_previous_state(S);
    } else if (I32(S, 0xb4) > 0) {
        int32_t k = 0;
        do {
            bike_sim_save_previous_state(S);
            bike_sim_vcall_dt(S, 0x18, F32(S, 0xbc));                                          /* slot 6 */
            bike_integrate_step(S, rx_bf(bike_x87_copy32(U32(S, 0xbc))));
            if (U32(S, 0x14) != 0) SU32(S, 0x14, 1);
            else SU32(S, 0x14, bike_sim_vcall(S, 0x10, 0, 0) != 0);                            /* slot 4 */
            if (U32(S, 0x18) != 0) SU32(S, 0x18, 1);
            else SU32(S, 0x18, bike_sim_vcall(S, 0x14, 0, 0) != 0);                            /* slot 5 */
            if (U32(S, 0x2e74) != 0) SU32(S, 0x2e74, 1);
            else SU32(S, 0x2e74, bike_sim_vcall(S, 0x70, 0, 0) != 0);                          /* slot 28 */
            SU32(S, 0x2e78, bike_sim_vcall(S, 0x6c, 0, 0));                                    /* slot 27 */
            bike_sim_vcall_dt(S, 0x1c, F32(S, 0xbc));                                          /* slot 7 */
            k++;
        } while (k < I32(S, 0xb4));
    }
    float a = phys_sub(1.0f, phys_div(F32(S, 0xc0), F32(S, 0xbc)));
    if (I32(S, 0xec) > 0) bike_sim_interpolate_bodies(S, a, 1);
    if (U32(S, 4) == 1) {
        if (U32(S, 0x2388) == 0) {                                                             /* remote: a ring of replay states */
            SIM_FRAME_ENTER(F, 0x80);
            bike_get_replay_state(S, F);
            for (uint32_t k2 = 19; k2 > 0; k2--) bike_sim_copy_replay_record(S + 0x2390 + 0x70u * k2, S + 0x2390 + 0x70u * (k2 - 1));
            bike_sim_copy_replay_record(S + 0x2390, F);
            SIM_FRAME_LEAVE(F);
            SU32(S, 0x2388, 1);
        }
        SU32(S, 0x2388, U32(S, 0x2388) - 1);
    }
}
RX_FUNCTION(0x00430ef0, bike_update_fixed_steps)

/* 00430eb0 RavenSimulation::vf1 (slot 1, ECX = S, stack frameDt): slot 8 (pending tuning reload), a query of the input manager
 * (slot 31 with (0x26, 0), the result unused), then UpdateFixedSteps when scheduling is enabled (S+b0). */
void bike_sim_update(rx_addr S, float frame_dt) {
    bike_sim_vcall(S, 0x20, 0, 0);                                                             /* slot 8 */
    rx_addr im = bike_sim_input_manager();
    uint32_t a[2] = {0x26, 0};
    bike_sim_im_call(im, 0x7c, 1, 2, a);
    if (U32(S, 0xb0) != 0) bike_update_fixed_steps(S, rx_bf(bike_x87_copy32(rx_fb(frame_dt))));
}
RX_FUNCTION(0x00430eb0, bike_sim_update)

/* 0048d2f0 (ECX = the simulation, stack: a vec4; RET 4; shared by the moto / ATV simulation vtables): the position block +0x70 =
 * the argument's vec4, the matrix row +0x80 = the simulation's +0xf0, +0x5c = 1. Frame: push ebp; and esp, -16. */
void rx_sim_set_position(rx_addr sim, rx_addr v) {
    const rx_addr entry0 = rx_sp();
    rx_set_sp((entry0 - 4u) & ~15u);
    for (uint32_t k = 0; k < 16; k += 4) SU32(sim, 0x70 + k, U32(v, k));
    for (uint32_t k = 0; k < 16; k += 4) SU32(sim, 0x80 + k, U32(sim, 0xf0 + k));
    SU32(sim, 0x5c, 1);
    rx_set_sp(entry0);
}
RX_FUNCTION(0x0048d2f0, rx_sim_set_position)
