/* world_veh_extern.h -- the vehicle world code's calls into functions it does not own (terrain tracks / deformation, nodes,
 * rider records, CRT ...) and that are not in the core yet: each runs the original through rx_call_original(_ex) (oracle build)
 * until its owner ports it. One wrapper per callee, named after what it does. */
#pragma once
#include "world_node.h"
#include "world_util.h"
#include "phys_terrain.h"
#include "phys_misc.h"
#include "phys_crt.h"
#include "phys_math.h"
#include "phys_mech.h"
#include "bike_curve.h"
#include "bike_sim.h"

/* ---- terrain tracks (the visual tire-track strips, three global instances 0xa95774 / 0xa95778 / 0xa9577c; collision group) */
/* 007c3510 TerrainTracks_AddPoint (EAX = tracks, stdcall handle, point, point2, f0, f1, f2, flag; RET 0x1c) */
static inline void veh_tracks_add_point(rx_addr tracks, uint32_t handle, rx_addr p0, rx_addr p1, uint32_t f0, uint32_t f1,
                                        uint32_t f2, uint32_t flag, world_ctx c) {
    WORLD_CALL_SP(0x1c, (void)phys_tracks_add_point(tracks, (int32_t)handle, p0, p1, rx_bf(f0), rx_bf(f1), rx_bf(f2), flag, c.ecx));
}
/* 007c4560 TerrainTracks_CutStrip (ESI = tracks, EAX = handle): ends the handle's current strip */
static inline void veh_tracks_cut_strip(rx_addr tracks, uint32_t handle, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, phys_tracks_cut_strip(tracks, (int32_t)handle));
}
/* 007c2ab0 DeformablePatch_AddStamp (ESI = patch = terrain surface +0x230, EAX = p_a, stdcall p_b, z_front, z_back,
 * hw_front, hw_back; RET 0x14) (terrain-deformation.md 2) */
static inline void veh_deform_add_stamp(rx_addr patch, rx_addr pa, rx_addr pb, uint32_t zf, uint32_t zb, uint32_t hwf,
                                        uint32_t hwb, world_ctx c) {
    uint32_t edx;
    (void)c;
    WORLD_CALL_SP(0x14, (void)phys_patch_add_stamp(patch, pa, pb, rx_bf(zf), rx_bf(zb), rx_bf(hwf), rx_bf(hwb), &edx));
}

/* ---- small math (rx_phys / rx_bike own them; core: phys_lag_filter_update / _set (phys_misc.c), bike_lerp_clamped,
 * phys_sqrtf: called through the original until the world build links those files) */
/* 0042d6b0 (EAX = lag filter {value, tau, alpha}, xmm2 = dt, stdcall target; RET 4): value += (target - value) * min(tau, dt) /
 * tau; xmm0 = the new value */
static inline float veh_lag_filter_update(rx_addr filter, float target, float dt, world_ctx c) {
    float r;
    (void)c;
    WORLD_CALL_SP(4, r = phys_lag_filter_update(filter, dt, rx_bf(world_x87_pass(rx_fb(target)))));
    return r;
}
/* 0042d670 (ECX = lag filter, xmm1 = value, stdcall tau; RET 4): value / tau set unless FLT_MAX */
static inline void veh_lag_filter_set(rx_addr filter, float value, uint32_t tau, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_lag_filter_set(filter, value, rx_bf(tau)));
}
/* 00430810 Lerp_Clamped (xmm2 = t, cdecl a, b; xmm0): a for t <= 0, b for t >= 1, else (b - a) * t + a */
static inline float veh_lerp_clamped(uint32_t a, uint32_t b, rx_v4 t, world_ctx c) {
    float r;
    (void)c;
    WORLD_CALL_SP(8, r = bike_lerp_clamped(t.v[0], rx_bf(a), rx_bf(b)));
    return r;
}
/* 0042c230 SqrtF (cdecl x; xmm0 = sqrtss) */
static inline float veh_sqrtf(float x, world_ctx c) {
    float r;
    (void)c;
    WORLD_CALL_SP(4, r = phys_sqrtf(x));
    return r;
}

/* ---- nodes (world node share / bike) */
/* 007dcfb0 Node_SetWorldRotation (thiscall node, stdcall quaternion; RET 4) */
static inline void veh_node_set_world_rotation(rx_addr node, rx_addr q, world_ctx c) {
    WORLD_CALL_SP(4, rx_world_node_set_world_rotation(node, q, c.ebx));
}
/* 007dd1b0 Node_SetWorldPosition (stdcall node, position; RET 8) */
static inline void veh_node_set_world_position(rx_addr node, rx_addr p, world_ctx c) {
    WORLD_CALL_SP(8, rx_world_node_set_world_position(node, p, c.esi, c.edi));
}
/* 004300a0 ModelNodeRef_SetMatrix (ECX = node ref, stdcall matrix; RET 4) (core: bike_model_node_set_matrix) */
static inline void veh_model_node_set_matrix(rx_addr ref, rx_addr matrix, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, bike_model_node_set_matrix(ref, matrix));
}
/* 0042c390 Vec4Cos_RangeReduced (xmm0 = angles, ECX = cos out, cdecl sin out) (core: phys_vec4_sincos) */
static inline void veh_vec4_sincos(rx_v4 angles, rx_addr cos_out, rx_addr sin_out, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, phys_vec4_sincos(angles, sin_out, cos_out));
}
/* 007db890 Node_EvaluateWorld (thiscall node, stdcall subtree; RET 4) (core: rx_world_node_evaluate_world, world_node.c) */
static inline void veh_node_evaluate_world(rx_addr node, uint32_t subtree, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(node, subtree));
}
/* 007db7e0 Node_Invalidate (ECX = node) */
static inline void veh_node_invalidate(rx_addr node, world_ctx c) { WORLD_CALL_SP(0, rx_world_node_invalidate(node, c.ebx, c.esi, c.edi)); }
/* 00853180 Noise1D (cdecl t; xmm0); it keeps xmm7 (its LTCG callers rely on that) */
static inline float veh_noise1d(float t, world_ctx c) {
    float r;
    (void)c;
    WORLD_CALL_SP(4, r = phys_noise_1d(rx_bf(world_x87_pass(rx_fb(t)))));
    return r;
}
