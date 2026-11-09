/* world_wreck.h -- the world code of 0x0063xxxx-0x0075xxxx (core/game/world_wreck*.c): float properties with listeners, the
 * uniform cell grid of the terrain-side collision items, the wreck checker, the third-person camera's impact reactions, the
 * collision audio, the clip recorder and small virtuals. Layouts: core/notes/world.md. */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- world_wreck.c: properties and the cell grid */
void rx_world_float_listeners_set(rx_addr v, rx_addr value);   /* 0063b830: v=s0 value=s1; RET 0x8 -> EAX */
float rx_world_float_listeners_get(rx_addr v);   /* 0063b8f0: v=s0; RET 0x4 -> EAX/xmm0 */
float rx_world_ranged_float_get(rx_addr obj);   /* 0063b0e0: obj=EAX; RET 0x0 -> EAX/xmm0 */
void rx_world_ranged_float_step(rx_addr obj, rx_addr input);   /* 0063b0f0: obj=ESI input=EAX; RET 0x0 -> EAX */
int rx_world_ranged_float_step_any(rx_addr obj, rx_addr input);   /* 0063b160: obj=EAX input=ECX; RET 0x0 -> EAX */
void rx_world_clamped_float_write(rx_addr obj, rx_addr input);   /* 0063b480: obj=EAX input=ECX; RET 0x0 -> EAX */
float rx_world_clamped_float_diff(rx_addr obj, rx_addr b);   /* 0063b500: obj=ECX b=s0; RET 0x4 -> EAX/xmm0 */
void rx_world_clamped_float_set(rx_addr obj, rx_addr input);   /* 0063b540: obj=ESI input=EAX; RET 0x0 -> EAX */
void rx_world_clamped_float_add(rx_addr obj, rx_addr delta);   /* 0063b590: obj=ESI delta=s0; RET 0x4 -> EAX */
void rx_world_wrapped_float_add(rx_addr obj, rx_addr delta);   /* 0063b650: obj=ESI delta=s0; RET 0x4 -> EAX */
void rx_world_short_property_set(rx_addr obj, rx_addr input);   /* 0063b6c0: obj=EAX input=EDX; RET 0x0 -> EAX */
int rx_world_grid_cell_contains(rx_addr grid, rx_addr point, uint32_t cell);   /* 00668910: grid=ECX point=s0 cell=s1; RET 0x8 -> EAX */
uint32_t rx_world_grid_cell_of(rx_addr grid, rx_addr point);   /* 00668820: grid=ECX point=EDX; RET 0x0 -> EAX */
void rx_world_grid_neighbours(rx_addr grid, uint32_t cell);   /* 00668710: grid=ECX cell=EDX; RET 0x0 -> EAX */
int rx_world_grid_test_neighbours(rx_addr grid, rx_addr point);   /* 006689c0: grid=EDI point=s0; RET 0x4 -> EAX */
int32_t rx_world_grid_locate(rx_addr grid, rx_addr point);   /* 00668690: grid=EAX point=s0; RET 0x4 -> EAX */
void rx_world_grid_collect_hits(rx_addr grid, rx_addr list);   /* 00668650: grid=EAX list=EDX; RET 0x0 */
void rx_world_grid_report_overlaps(rx_addr shape, rx_addr set2, rx_addr grid, rx_addr pos, rx_addr rot, rx_addr out2, rx_addr set1,
                                   rx_addr out1);                         /* 00667e40 */

/* ---- world_wreck_checker.c: the wreck checker */
uint32_t rx_world_wreck_checker_type_class(uint32_t index);   /* 00637ee0: index=ECX; RET 0x0 -> EAX */
float rx_world_wreck_checker_class_anim(uint32_t index, rx_addr out);   /* 00637ff0: index=ECX out=EAX; RET 0x0 -> xmm0 */
float rx_world_wreck_checker_excess(float a, float b, float c);   /* 00638150: a=s0 b=s1 c=s2; RET 0xc -> xmm0 */
void rx_world_wreck_checker_update(rx_addr checker, float dt);   /* 006373e0: checker=s0 dt=s1; RET 0x8 */
void rx_world_wreck_checker_vf0(rx_addr checker, float dt);   /* 00637320: checker=ECX dt=s0; RET 0x4 */
void rx_world_wreck_state_ramp_to(rx_addr state, float target);   /* 006367b0: state=ECX target=xmm2; RET 0x0 */
void rx_world_wreck_state_advance(rx_addr state, float dt);   /* 00636870: state=ESI dt=s0; RET 0x4 */
void rx_world_wreck_state_react(rx_addr state, rx_addr rider);   /* 00632aa0: state=ECX rider=ESI; RET 0x0 */

/* ---- world_wreck_camera.c: the third-person camera's impact reactions */
uint32_t rx_world_third_person_camera_vf22(rx_addr cam);   /* 00649bc0: cam=ECX; RET 0x0 -> EAX */
uint32_t rx_world_camera_mode_index(rx_addr cam);   /* 006467f0: cam=EAX; RET 0x0 -> EAX */
int rx_world_camera_impact_shake(rx_addr cam, rx_addr v);   /* 006510a0: cam=EDI v=s0; RET 0x4 -> EAX */
int rx_world_camera_impact_kick(rx_addr cam, rx_addr v);   /* 00651320: cam=EDI v=s0; RET 0x4 -> EAX */
int rx_world_camera_wreck_kick(rx_addr cam, rx_addr v);   /* 006514e0: cam=EDI v=s0; RET 0x4 -> AL */
void rx_world_camera_start_kick(rx_addr cam, rx_addr kick, float strength, float duration, rx_addr kick2);   /* 00651940: cam=s0 kick=s1 strength=s2 duration=s3 kick2=EDX; RET 0x10 -> EAX */
void rx_world_camera_start_shake(rx_addr cam, rx_addr scale, float duration, rx_addr shake1, rx_addr shake2);   /* 006516a0: cam=s0 scale=s1 duration=s2 shake1=ECX shake2=EDX; RET 0xc -> EAX */
void rx_world_third_person_camera_vf34(rx_addr cam, rx_addr v, float duration);   /* 00653300: cam=ECX v=s0 duration=s1; RET 0x8 */
void rx_world_third_person_camera_vf33(rx_addr cam, uint32_t unused, rx_addr shake, rx_addr impact, uint32_t no_kick);   /* 00651000: cam=ECX unused=s0 shake=s1 impact=s2 no_kick=s3; RET 0x10 */

/* ---- world_wreck_misc.c: small virtuals, crowd audio, the clip recorder, assorted */
void rx_world_shared_vf_ret4(rx_addr self, uint32_t arg);   /* 00679000: self=ECX arg=s0; RET 0x4 */
uint32_t rx_world_shared_vf_type10(rx_addr self);   /* 00703d50: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_forest_system_vf22(rx_addr self);   /* 0065cab0: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_key_map_vf11(rx_addr map, uint32_t key);   /* 006d7870: map=ECX key=s0; RET 0x4 -> EAX */
rx_addr rx_world_active_sound_table(rx_addr track);   /* 0075c580: track=EDX; RET 0x0 -> EAX */
int rx_world_crowd_bank_in_use(void);   /* 0067ec50: -; RET 0x0 -> EAX */
int rx_world_ambience_bank_in_use(void);   /* 0067eca0: -; RET 0x0 -> EAX */
rx_addr rx_world_crowd_slot(uint32_t slot);   /* 00679420: slot=EAX; RET 0x0 -> EAX */
float rx_world_crowd_slot_level(uint32_t slot, rx_addr table);   /* 0067abc0: slot=EAX table=EDX; RET 0x0 -> xmm0 */
float rx_world_crowd_level(uint32_t slot);   /* 00679540: slot=ESI; RET 0x0 -> xmm0 */
void rx_world_crowd_excite(float delta);   /* 00674440: delta=s0; RET 0x0 */
void rx_world_crowd_react(uint32_t kind);   /* 006745d0: kind=EAX; RET 0x0 */
void rx_world_crowd_event(int32_t kind);   /* 00674540: kind=ECX; RET 0x0 */
void rx_world_ik_restart_fades(rx_addr obj);   /* 006b0480: obj=ESI; RET 0x0 */
void rx_world_get_three_vectors(rx_addr obj, rx_addr out);   /* 006b7560: obj=ECX out=s0; RET 0x4 -> EAX */
float rx_world_timer_seconds(rx_addr obj);   /* 006def20: obj=EAX; RET 0x0 -> xmm0 */
rx_addr rx_world_string_map_find(rx_addr map, rx_addr out, rx_addr key);   /* 00702ad0: map=ESI out=s0 key=s1; RET 0x8 -> EAX */
void rx_world_keyframe_blend_select(rx_addr b);   /* 006b6d80: b=EAX; RET 0x0 */
void rx_world_clip_recorder_fill_entry(rx_addr bike, rx_addr entry, rx_addr rider);   /* 006aa460: bike=EDI entry=s0 rider=s1; RET 0x8 */
void rx_world_clip_recorder_fill_entries(rx_addr entries);   /* 006abbc0: entries=s0; RET 0x0 */
void rx_world_clip_recorder_candidate(uint32_t type, rx_addr rider, float start, float end);   /* 006aacf0: type=ECX rider=EDX start=s0 end=s1; RET 0x0 */
void rx_world_clip_recorder_candidate_of_type(uint32_t type, rx_addr rider);   /* 006aaca0: type=ECX rider=EDX; RET 0x0 */
void rx_world_clip_recorder_log_wreck(rx_addr rider, uint32_t value);   /* 006ab8a0: rider=EAX value=s0; RET 0x0 */
void rx_world_clip_recorder_log_stunt(rx_addr rider, uint32_t value);   /* 006ab930: rider=EAX value=s0; RET 0x0 */
void rx_world_clip_recorder_log_recover(rx_addr rider);   /* 006ab9b0: rider=EAX; RET 0x0 */

/* ---- world_wreck_audio.c: the collision audio */
int rx_world_collision_key_less(rx_addr a, rx_addr b);   /* 0067ac80: a=ESI b=EDI; RET 0x0 -> EAX */
void rx_world_collision_map_iter_next(rx_addr it);   /* 0068fc80: it=ESI; RET 0x0 */
void rx_world_collision_map_insert(rx_addr map, rx_addr out, rx_addr key);   /* 0068e130: map=s0 out=s1 key=s2; RET 0xc -> EAX */
void rx_world_rider_sound_slot_stop(rx_addr block, uint32_t slot, uint32_t ebp);   /* 0068c360: block=s0 slot=s1 ebp=EBP; RET 0x8 */
void rx_world_rider_sound_stop(rx_addr rider, uint32_t slot, uint32_t ebp);   /* 00681840: rider=ESI slot=s0 ebp=EBP; RET 0x0 */
rx_addr rx_world_rider_sound_source(rx_addr block);   /* 0068bfb0: block=EDI; RET 0x0 -> EAX */
void rx_world_rider_sound_start(rx_addr rider, uint32_t slot);   /* 006817c0: rider=ESI slot=s0; RET 0x0 */
rx_addr rx_world_audio_table_name(rx_addr table, uint32_t id);   /* 0067d310: table=ECX id=EAX; RET 0x0 -> EAX */
rx_addr rx_world_audio_collision_sound_name(rx_addr mgr, uint32_t id);   /* 0067b890: mgr=ESI id=EAX; RET 0x0 -> EAX */
int rx_world_audio_collision_event_is_ours(rx_addr rec);   /* 0067c930: rec=EAX; RET 0x0 -> EAX */
void rx_world_audio_collision_add(rx_addr mgr, rx_addr event);   /* 0067bb10: mgr=EAX event=s0; RET 0x4 */
void rx_world_audio_collision_manager_vf0(rx_addr mgr, rx_addr events);   /* 0067bac0: mgr=ECX events=s0; RET 0x4 */
void rx_world_audio_collision_update(rx_addr rec, rx_addr mgr);   /* 0067c2c0: rec=ESI mgr=EDI; RET 0x0 */
void rx_world_audio_collision_play(rx_addr mgr, rx_addr rec);   /* 0067bd30: mgr=s0 rec=s1; RET 0x8 */
void rx_world_audio_collision_update_roll(rx_addr mgr, rx_addr rec);   /* 0067c350: mgr=ECX rec=s0; RET 0x4 */

#ifdef __cplusplus
}
#endif
