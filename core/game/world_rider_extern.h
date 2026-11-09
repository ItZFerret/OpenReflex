/* world_rider_extern.h -- the rider share's calls into functions it does not own (rider animation = rx_anim, bike = rx_bike,
 * terrain / collision = rx_phys, race / game = later) and that are not in the core yet: each runs the original through
 * rx_call_original(_ex) (oracle build) until its owner ports it. One wrapper per callee, named after what it does.
 * This file is the list of what the rider share still borrows. */
#pragma once
#include "world_wreck.h"
#include "world_node.h"
#include "world_misc.h"
#include "world_util.h"
#include "phys_terrain.h"
#include "phys_anim_sample.h"
#include "phys_quat.h"
#include "phys_rider.h"
#include "bike_sim.h"
#include "anim_rider_stunt.h"
#include "anim_rider_play.h"
#include "build_rider_misc.h"

/* ---- the terrain-track strips (TerrainTracks, rx_phys): sets 0xa95774 (tag 0x1000), 0xa95778 (0x2000), 0xa9577c (0x4000) */
/* 007c34b0 TerrainTracks_AllocSlotDefault: a free slot of the 0xa9577c set (EAX; -1 when none) */
static inline uint32_t world_tracks_alloc_slot_default(world_ctx c) { (void)c; int32_t r; WORLD_CALL_SP(0, r = phys_tracks_alloc_slot()); return (uint32_t)r; }
/* 007c4560 TerrainTracks_CutStrip (ESI = set, EAX = slot): ends the slot's current strip */
static inline void world_tracks_cut_strip(rx_addr set, uint32_t slot, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, phys_tracks_cut_strip(set, (int32_t)slot));
}
/* 007c3510 TerrainTracks_AddPoint (EAX = set; stdcall slot, pos, prev or first, width, ext, intensity, flag; RET 0x1c) */
static inline void world_tracks_add_point(rx_addr set, uint32_t slot, rx_addr pos, rx_addr prev, uint32_t width, uint32_t ext,
                                          uint32_t intensity, uint32_t flag, world_ctx c) {
    WORLD_CALL_SP(0x1c, (void)phys_tracks_add_point(set, (int32_t)slot, pos, prev, rx_bf(width), rx_bf(ext), rx_bf(intensity), flag, c.ecx));
}
/* 007c2ab0 DeformablePatch_AddStamp (ESI = patch = surface +0x230, EAX = p_a; stack p_b, z front, z back, half width front,
 * half width back; RET 0x14) */
static inline void world_deform_add_stamp(rx_addr patch, rx_addr p_a, rx_addr p_b, uint32_t z_front, uint32_t z_back,
                                          uint32_t hw_front, uint32_t hw_back, world_ctx c) {
    uint32_t edx;
    (void)c;
    WORLD_CALL_SP(0x14, (void)phys_patch_add_stamp(patch, p_a, p_b, rx_bf(z_front), rx_bf(z_back), rx_bf(hw_front), rx_bf(hw_back), &edx));
}

/* ---- float properties (0x0063b0e0 .. 0x0063b8f0, the wreck share): listener vectors {float* target, int enabled} ---- */
/* 0063b8f0 FloatListeners_Get (stdcall listeners; RET 4): the first enabled target's value (xmm0), 0 when none */
static inline float world_float_get(rx_addr listeners, world_ctx c) {
    (void)c;
    float r;
    WORLD_CALL_SP(4, r = rx_world_float_listeners_get(listeners));
    return r;
}
/* 0063b830 FloatListeners_Set (stdcall listeners, float* value; RET 8): *value to every enabled target (fld / fstp) */
static inline void world_float_set(rx_addr listeners, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(8, rx_world_float_listeners_set(listeners, value));
}

/* ---- pose kernels (rx_phys: "large SSE + double kernels" 0x00840xxx) ---- */
/* 00840b70 (stdcall keys a, keys b, float t a, float t b, float weight, pose out; RET 0x18): keyframe pick and blend into a
 * pose */
static inline void world_pose_blend_keys(rx_addr a, rx_addr b, uint32_t ta, uint32_t tb, uint32_t w, rx_addr out, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0x18, (void)phys_pose_sample_blend(a, b, rx_bf(ta), rx_bf(tb), rx_bf(w), out));
}
/* 00841c90 (ECX = pose a, EDX = pose out; cdecl pose b, bone count, float weight): pose blend */
static inline void world_pose_blend(rx_addr a, rx_addr out, rx_addr b, uint32_t count, uint32_t w, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0xc, phys_pose_blend(a, out, b, (int32_t)count, rx_bf(w)));
}
/* 0063b0e0 RangedFloat_Get (EAX = ranged float): its listeners' value (+0x14), xmm0 */
static inline float world_ranged_float_get(rx_addr rf, world_ctx c) {
    (void)c;
    float r;
    WORLD_CALL_SP(0, r = rx_world_ranged_float_get(rf));
    return r;
}
/* 0063b480 ClampedFloat_Write (EAX = clamped float {min, max, float* target}, ECX = float* value): *target = clamp(*value) */
static inline void world_clamped_float_write(rx_addr cf, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, rx_world_clamped_float_write(cf, value));
}
/* 0063b160 RangedFloat_StepAny (EAX = ranged float, ECX = float* target): steps it and broadcasts its position (EAX = 1 when
 * it has listeners) */
static inline void world_ranged_float_step_any(rx_addr rf, rx_addr target, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, (void)rx_world_ranged_float_step_any(rf, target));
}
/* 0063b0f0 RangedFloat_Step (ESI = ranged float, EAX = float* target): steps it and broadcasts its position */
static inline void world_ranged_float_step(rx_addr rf, rx_addr target, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, rx_world_ranged_float_step(rf, target));
}
/* 0063b6c0 ShortProperty_Set (EAX = property {+4 byte* dirty, +8 short* value}, EDX = short* source) */
static inline void world_short_property_set(rx_addr prop, rx_addr src, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, rx_world_short_property_set(prop, src));
}
/* 0063b7d0 (EAX = listener vector; stdcall float* value; RET 4): *value to every listener (0063c190) */
static inline void world_listeners_broadcast(rx_addr listeners, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, rx_rider_listeners_broadcast_vec(listeners, value));
}
/* 0063b500 ClampedFloat_Diff (ECX = clamped float {min, max, float* target}; stdcall float* value; RET 4): clamp((target ?
 * *target : 0) - *value) in xmm0 */
static inline float world_clamped_float_diff(rx_addr cf, rx_addr value, world_ctx c) {
    (void)c;
    float r;
    WORLD_CALL_SP(4, r = rx_world_clamped_float_diff(cf, value));
    return r;
}
/* 0063b540 ClampedFloat_Set (ESI = clamped float {min, max, listeners +8}, EAX = float* value): listeners = clamp(*value) */
static inline void world_clamped_float_set(rx_addr cf, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, rx_world_clamped_float_set(cf, value));
}
/* 0063b5f0 ClampedFloat_Sub (ESI = clamped float; stdcall float* value; RET 4): listeners = clamp(get - *value) */
static inline void world_clamped_float_sub(rx_addr cf, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, rx_rider_clamped_float_sub(cf, value));   /* 0063b5f0 (trickplayback's port) */
}
/* 00830130 (ECX = character, stdcall animation entry; RET 4): the entry's clip (EAX) */
static inline rx_addr world_character_find_clip(rx_addr character, rx_addr entry, world_ctx c) {
    (void)c;
    rx_addr r;
    WORLD_CALL_SP(4, r = rx_world_object_query_248(character, entry));
    return r;
}
/* 0083d3d0 (ECX = 8 events of 12 bytes out {time, ?, code}, xmm2 = t from, xmm1 = t to; stdcall clip; RET 4): the clip's
 * events between the two times */
static inline void world_clip_events(rx_addr events, rx_addr clip, float t1, float t2, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, (void)rx_world_clip_events_between(events, clip, t1, t2));
}
/* 00434ef0 Simulation_EndPoseOverride (EDI = simulation) */
static inline void world_simulation_end_pose_override(rx_addr sim, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, bike_end_pose_override(sim));
}
/* 0063b4b0 ClampedFloat_Add (EAX = clamped float {min, max, float* target}; stdcall float* value; RET 4): *target =
 * clamp(*value + *target) */
static inline void world_clamped_float_add(rx_addr cf, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, rx_rider_clamped_float_add(cf, value));   /* 0063b4b0 (trickstart's port) */
}
/* 0063b650 WrappedFloat_Add (ESI = wrapped float; stdcall float* value; RET 4): listeners += *value, wrapped into the range */
static inline void world_wrapped_float_add(rx_addr wf, rx_addr value, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, rx_world_wrapped_float_add(wf, value));
}
/* 00621f40 (ESI = ra): the crash animation state step (rider animation, anim_rider_play.c) */
static inline void world_rider_crash_step(rx_addr ra, world_ctx c) { (void)c; WORLD_CALL_SP(0, rx_rider_crash_step(ra)); }
/* 00620a60 (stdcall ra, dt; RET 8): the crash animation update (rider animation, anim_rider_play.c) */
static inline void world_rider_crash_update(rx_addr ra, uint32_t dt, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(0, rx_rider_crash_update(ra, rx_bf(dt)));
}
/* 0042aca0 (stdcall rider body +0x5d0; RET 4): EAX != 0 when the body is upright (quaternion test; rx_bike) */
static inline uint32_t world_rider_body_upright(rx_addr body, world_ctx c) { uint32_t ecx = c.ecx, r; WORLD_CALL_SP(4, r = phys_rider_up_facing(body, &ecx)); return r; }
/* 006b6d80 (EAX = animation entity): keyframe pick and blend of the entity's clip at its time +0xdb4 (the wreck share) */
static inline void world_anim_entity_blend(rx_addr entity, world_ctx c) { (void)c; WORLD_CALL_SP(0, rx_world_keyframe_blend_select(entity)); }
/* 0063b360 FloatListeners_EnableAll / 0063b3f0 FloatListeners_DisableAll (stdcall listeners; RET 4): every listener's enabled
 * word (+4) set to 1 / 0 */
static inline void world_float_listeners_enable_all(rx_addr listeners, world_ctx c) {
    (void)c;
    rx_rider_vec8_mark_all(listeners);
}
static inline void world_float_listeners_disable_all(rx_addr listeners, world_ctx c) {
    (void)c;
    rx_rider_vec8_clear_all(listeners);
}
/* 007fa030 (ESI = map, EAX = key*; stdcall out*; RET 4): the map's value for the key (0 when absent) into *out */
static inline void world_map_find(rx_addr map, rx_addr key, rx_addr out, world_ctx c) {
    (void)c;
    WORLD_CALL_SP(4, (void)rx_world_string_table_find(map, key, out));
}
