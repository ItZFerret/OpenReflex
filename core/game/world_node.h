/* world_node.h -- the physics world's "node" share (core/game/world_node*.c): the node hierarchy evaluator (0x007dxxxx) and the
 * other functions of S/rx_world/list_node.tsv. One rx_world_<name> per original function (address in the comment).
 * Layouts and conventions: S/rx_world/notes_node.md (merged into core/notes/world.md). */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- world_node.c: the node hierarchy (0x007db7e0 .. 0x007dd3f1) ---- */
/* The registers 007db7e0 leaves (EAX / ECX / EDX; the exact model is documented at invalidate_at in world_node.c). */
typedef struct rx_node_invalidate_regs { uint32_t eax, ecx, edx; } rx_node_invalidate_regs;
void rx_world_node_evaluate_world(rx_addr rec, uint32_t subtree);   /* 007db890: rec=ECX subtree=s0; RET 0x4 */
void rx_world_node_evaluate_world_saved_regs(rx_addr sp0, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi);   /* 007db890's saved EBP / EBX / ESI / EDI (not an original function) */
void rx_world_node_invalidate(rx_addr rec, uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi);   /* 007db7e0: rec=ECX caller_ebx=EBX caller_esi=ESI caller_edi=EDI; RET 0x0 -> EAX */
rx_node_invalidate_regs rx_world_node_invalidate_regs(rx_addr rec, uint32_t entry_eax, uint32_t entry_edx,
                                                      uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi);   /* 007db7e0: rec=ECX entry_eax=EAX entry_edx=EDX; RET 0x0 -> EAX / ECX / EDX */
void rx_world_node_set_world(rx_addr rec, rx_addr src, uint32_t caller_ebx);   /* 007dc890: rec=EDI src=ESI caller_ebx=EBX; RET 0x0 */
void rx_world_node_set_world_rotation(rx_addr rec, rx_addr quat, uint32_t caller_ebx);   /* 007dcfb0: rec=ECX quat=s0 caller_ebx=EBX; RET 0x4 */
rx_addr rx_world_node_world_to_local_point(rx_addr rec, rx_addr out, rx_addr point);   /* 007dc560: rec=ESI out=EDI point=s0; RET 0x4 -> EAX */
void rx_world_node_set_world_position(rx_addr rec, rx_addr v, uint32_t caller_esi, uint32_t caller_edi);   /* 007dd1b0: rec=s0 v=s1 caller_esi=ESI caller_edi=EDI; RET 0x8 */
void rx_world_node_get_world_position(rx_addr rec, rx_addr out);   /* 007dd3a0: rec=ESI out=EDI; RET 0x0 -> EAX */
void rx_world_node_get_world_matrix(rx_addr rec, rx_addr out);   /* 007dd320: rec=EDI out=ESI; RET 0x0 -> EAX */

/* ---- world_node_audio.c: the FMOD audio event layer */
rx_addr rx_world_aligned_alloc(uint32_t size);   /* = build_alloc.h rx_reflex_aligned_alloc (0078fbe0, not registered here) */
void rx_world_aligned_free(rx_addr p);   /* 0078fc50: p=s0; RET 0x0 */
rx_addr rx_world_vector_last(rx_addr v);   /* 0080cd90: v=EAX; RET 0x0 -> EAX */
void rx_world_vector_push_back(rx_addr v, rx_addr item);   /* 0080cdf0: v=EDI item=s0; RET 0x4 */
rx_addr rx_world_event_pool_take(rx_addr list);   /* 0080c440: list=ESI; RET 0x0 -> EAX */
void rx_world_event_pool_give(rx_addr list, rx_addr ev);   /* 0080c400: list=EAX ev=s0; RET 0x4 */
void rx_world_event_queue(uint32_t cmd, rx_addr ev, uint32_t a, uint32_t b, uint32_t ebp);   /* 0080c280: cmd=s0 ev=s1 a=s2 b=s3 ebp=EBP (saved); RET 0x0 */
void rx_world_event_set_callback(rx_addr ev, uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e, uint32_t g);   /* 0080a990: ev=ESI a=s0 b=s1 c=s2 d=s3 e=s4 g=s5; RET 0x18 */
void rx_world_event_release_instance(rx_addr ev, uint32_t ebp);   /* 0080a810: ev=ESI ebp=EBP; RET 0x0 */
void rx_world_event_stop(rx_addr ev);   /* 0080a770: ev=EDI; RET 0x0 */
void rx_world_event_pool_return(rx_addr ev, rx_addr pool);   /* 0080c970: ev=ESI pool=s0; RET 0x4 */
void rx_world_event_release(rx_addr ev, rx_addr pool);   /* 0080c810: ev=EAX pool=s0; RET 0x4 */
void rx_world_event_set_channel(rx_addr ev, rx_addr ch);   /* 0080bce0: ev=EAX ch=s0; RET 0x4 */
rx_addr rx_world_event_create(rx_addr sys, rx_addr name, uint32_t tag);   /* 0080c760: sys=EAX name=s0 tag=s1; RET 0x8 -> EAX */
void rx_world_event_start(rx_addr ev, uint32_t ebp);   /* 0080a9f0: ev=ESI ebp=EBP; RET 0x0 */
void rx_audio_event_apply_volume(rx_addr ev);   /* 0080b490 AudioEvent_ApplyVolume (EAX = ESI = event; race_sound.c) */
void rx_world_event_apply_volume(rx_addr ev, uint32_t ebp);   /* 0080b460: ev=EDI ebp=EBP; RET 0x0 */
void rx_world_event_play(rx_addr ev, uint32_t a, uint32_t b, uint32_t c);   /* 0080abd0: ev=ESI a=s0 b=s1 c=s2; RET 0xc */
void rx_world_event_set_position(rx_addr pos, rx_addr ev, uint32_t ebp);   /* 0080ae80: pos=EDI ev=s0 ebp=EBP; RET 0x4 */
uint32_t rx_world_event_is_playing(rx_addr ev);   /* 0080b500: ev=ECX; RET 0x0 -> EAX */
void rx_world_event_set_range(rx_addr ev, uint32_t min_bits, uint32_t max_bits, uint32_t ebp);   /* 0080b310: ev=EAX min=s0 max=s1 ebp=EBP; RET 0x8 */
void rx_world_event_set_parameter(rx_addr ev, uint32_t param, float value);   /* 0080afc0: ev=EDI param=s0 value=s1; RET 0x8 */
void rx_world_fmod_event_vf1(rx_addr ev, uint32_t cmd, uint32_t unused, uint32_t ebp);   /* 0080bc80: ev=ECX cmd=s0 unused=s1 ebp=EBP; RET 0x8 */
void rx_world_audio_log(uint32_t enabled, rx_addr fmt);   /* 00805570: enabled=s0 fmt=s1; RET 0x0 */
rx_addr rx_world_map_node_new(uint32_t left, uint32_t parent, uint32_t right, rx_addr value);   /* 0080da40: left=s0 parent=s1 right=s2 value=s3; RET 0x10 -> EAX */
rx_addr rx_world_audio_queue_pop(void);   /* 00803ed0: -; RET 0x0 -> EAX */
uint32_t rx_world_shared_vf_ret0(void);   /* 007a0760: -; RET 0x0 -> EAX */
void rx_world_shared_vf_empty(void);   /* 007e3be0: -; RET 0x0 */
uint32_t rx_world_collision_entity_vf13(rx_addr e);   /* 00819cc0: e=ECX; RET 0x0 -> EAX */
rx_addr rx_world_int64_from_counter(rx_addr out);   /* 008b98c0: out=s0; RET 0x0 -> EAX */
rx_addr rx_world_string_map_lower_bound(rx_addr map, rx_addr key);   /* 008519a0: map=EAX key=s0; RET 0x4 -> EAX */
uint32_t rx_world_object_query_248(rx_addr obj, uint32_t a);   /* 00830130: obj=ECX a=s0; RET 0x4 -> EAX */
rx_addr rx_world_audio_post_command(uint32_t kind, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t a4);   /* 00805860: kind=s0 a1=s1 a2=s2 a3=s3 a4=s4; RET 0x0 -> EAX */
void rx_world_audio_process_queue(void);   /* 00805ff0: -; RET 0x0 */
rx_addr rx_world_audio_nearest_listener(rx_addr pos, rx_addr out);   /* 00805420: pos=s0 out=s1; RET 0x0 -> EAX */


void rx_world_audio_command_args(uint32_t kind, rx_addr rec, rx_addr a, rx_addr b);   /* 0080c2c0: kind=ECX rec=ESI a=EAX b=EDI; RET 0x0 */
uint32_t rx_world_event_parameter_range(uint32_t id, rx_addr out_min, rx_addr out_max);   /* 0080b160: id=ESI out_min=s0 out_max=s1; RET 0x0 -> EAX */
rx_addr rx_world_clock_now(rx_addr out, uint32_t may_step);   /* 00892170: out=s0 may_step=s1; RET 0x0 -> EAX */
rx_addr rx_world_clock_now_locked(rx_addr out);   /* 00892120: out=s0; RET 0x0 -> EAX */

/* ---- world_node_input.c: key maps and the Xenon input manager */
uint32_t rx_world_keymap_latched(rx_addr self, uint32_t mask, uint32_t pad);   /* 00811420 KeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> EAX */
uint32_t rx_world_keymap_down(rx_addr self, uint32_t mask, uint32_t pad);   /* 008115f0 KeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> EAX */
float rx_world_keymap_value(rx_addr self, uint32_t mask, uint32_t pad);                 /* 00811710 KeyMap::vf4: self=ECX mask=s0 pad=s1; RET 0x8 -> st0 */
float rx_debug_keymap_value(rx_addr self, uint32_t mask, uint32_t pad);                 /* 006d78a0 DebugKeyMap::vf4 (thiscall; RET 8 -> st0) */
float rx_world_keymap_value_at(rx_addr self, uint32_t mask, uint32_t pad, rx_addr slot);
uint32_t rx_world_keymap_code(rx_addr self, uint32_t mask, uint32_t pad);   /* 008115e0 KeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> EAX */
uint32_t rx_world_compound_keymap_down(rx_addr self, uint32_t mask, uint32_t pad);   /* 00811ab0 CompoundKeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> EAX */
float rx_world_compound_keymap_value(rx_addr self, uint32_t mask, uint32_t pad);   /* 00811b50 CompoundKeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> st0 */
float rx_world_cumulative_keymap_value(rx_addr self, uint32_t mask, uint32_t pad);   /* 00811db0 CumulativeKeyMap: self=ECX mask=s0 pad=s1; RET 0x8 -> st0 */
uint32_t rx_world_xenon_key_pressed(rx_addr self, uint32_t code, uint32_t which);   /* 00814c00 vf31: self=ECX code=s0 which=s1; RET 0x8 -> EAX */
uint32_t rx_world_xenon_key_held(rx_addr self, uint32_t code, uint32_t which);   /* 00814e60 vf37: self=ECX code=s0 which=s1; RET 0x8 -> EAX */
uint32_t rx_world_xenon_button(rx_addr self, uint32_t kind, uint32_t pad, uint32_t code, uint32_t which, uint32_t unused);   /* 00814cd0 vf33: self=ECX kind=s0 pad=s1 code=s2 which=s3 unused=s4; RET 0x14 -> EAX */
rx_addr rx_world_xenon_control(rx_addr self, uint32_t kind, uint32_t pad, uint32_t code);   /* 00814f30 vf41: self=ECX kind=s0 pad=s1 code=s2; RET 0xc -> EAX */
void rx_world_xenon_set_rumble(rx_addr self, uint32_t kind, uint32_t pad, uint32_t motor, uint32_t high, uint32_t low);   /* 008151e0 vf46: self=ECX kind=s0 pad=s1 motor=s2 high=s3 low=s4; RET 0x14 */
uint32_t rx_world_clip_events_between(rx_addr out, rx_addr clip, float from, float to);   /* 0083d3d0: out=ECX clip=s0 from=xmm2 to=xmm1; RET 0x4 */

/* ---- world_node_water.c: water surface sampling */
uint32_t rx_world_water_patch_height(rx_addr b, rx_addr p, rx_addr g, float scale, float offset);   /* 007e9dc0: b=s0 p=s1 g=s2 scale=s3 offset=s4; RET 0x14 -> EAX */
uint32_t rx_world_water_set_height(rx_addr set, rx_addr pt);   /* 007efef0: set=ESI pt=s0; RET 0x4 -> EAX */
uint32_t rx_world_packed_grid_sample(rx_addr grid, uint32_t level, float u, float v, const uint32_t saved[4]);   /* 007f8e00: grid=ECX level=EAX u=s0 v=s1 saved=the caller's EBX EBP ESI EDI (pushed); RET 0x8 -> EAX */
uint32_t rx_world_packed_grid_sample_regs(rx_addr grid, uint32_t level, float u, float v, const uint32_t saved[4], uint32_t* edx,
                                          uint32_t* xmm0);   /* the same, also the EDX (the second dword's bits, 0 when none) and xmm0 lane 0 (the floor argument's exponent field) the original leaves */

/* 007f8d10: grid=ESI (ECX ignored), u=s0 v=s1 layer=s2; RET12 -> EAX.
 * saved = EBX EBP EDI; ECX output=32-fieldwidth, EDX output=field mask. */
uint32_t rx_world_packed_grid_interleaved(rx_addr grid, float u, float v, uint32_t layer, const uint32_t saved[3], rx_v4* xmm0);


/* ---- world_node_geom.c: line / cylinder intersections */
void rx_world_line_cylinder_x(rx_addr d, rx_addr hit1, rx_addr o, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2);   /* 0081e0b0 */
void rx_world_line_cylinder_y(rx_addr o, rx_addr hit1, rx_addr d, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2);   /* 0081e380 */
void rx_world_line_cylinder_z(rx_addr o, rx_addr hit1, rx_addr d, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2);   /* 0081e630 */
void rx_world_segment_cone(rx_addr p0, rx_addr p1, rx_addr sum, rx_addr count, float h, float r0, float r1);   /* 0081d480: p0=ECX p1=EAX sum=ESI count=EDI h=s0 r0=s1 r1=s2; RET 0xc */
void rx_world_segment_cylinder(rx_addr p0, rx_addr p1, float h, float r, rx_addr out0, rx_addr out1, rx_addr flag0,
                               rx_addr flag1, uint32_t ebp, uint32_t ebx, uint32_t esi);   /* 0081beb0: p0=EDI p1=EAX h=s0 r=s1 out0=s2 out1=s3 flag0=s4 flag1=s5 ebp/ebx/esi=EBP/EBX/ESI (saved); RET 0x18 */
void rx_world_segment_box_faces(rx_addr e, rx_addr p0, rx_addr sum, rx_addr count, rx_addr p1, uint32_t ebp);   /* 0081b6b0: e=ECX p0=ESI sum=EAX count=EDI p1=s0 ebp=EBP (saved); RET 0x4 */
void rx_world_segment_box_faces_ecx(rx_addr e, rx_addr p0, rx_addr sum, rx_addr count, rx_addr p1, uint32_t ebp, uint32_t* ecx);   /* the same, also the ECX the original leaves (face 5's 0 / 1; EDX = p1) */
void rx_world_disc_cylinder(rx_addr n, rx_addr c, rx_addr hits, rx_addr flags, rx_addr obj, float R, float h, float rc,
                            uint32_t ebx, uint32_t ebp);   /* 0081c590: n=ECX c=EAX hits=ESI flags=EDI obj=s0 R=s1 h=s2 rc=s3 ebx=EBX ebp=EBP (saved); RET 0x10 */
void rx_world_line_box_edges(rx_addr ext, rx_addr d, rx_addr o, float r, rx_addr sum, rx_addr wsum);   /* 0081ecb0: ext=EAX d=s0 o=s1 r=s2 sum=s3 wsum=s4; RET 0x14 */

#ifdef __cplusplus
}
#endif
