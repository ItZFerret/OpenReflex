/* world_misc.h -- core/game/world_misc.c: small leaves of the physics world list (one rx_world_<name> per original). */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "crt.h"
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

uint32_t rx_world_entity_vf13(rx_addr self);   /* 0051bd60: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_lap_pacing_vf5(rx_addr self);   /* 00617e20: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_rider_animation_vf12(rx_addr self);   /* 00617f30: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_rider_animation_position(rx_addr self, rx_addr out);   /* 00617d70 RiderAnimation::vf17: self=ECX; RET 4 -> EAX */
void rx_world_shared_vf_ret8(rx_addr self, uint32_t a, uint32_t b);   /* 005a4150: self=ECX a=s0 b=s1; RET 0x8 */
void rx_world_shared_vf_ret16(rx_addr self, uint32_t a, uint32_t b, uint32_t c, uint32_t d);   /* 005de600: self=ECX a=s0 b=s1 c=s2 d=s3; RET 0x10 */
uint32_t rx_world_series_event_on_advance_post(rx_addr self, uint32_t a, uint32_t b);   /* 00592b20: self=ECX a=s0 b=s1; RET 0x8 -> EAX */
uint32_t rx_world_base_scenario_vf8(rx_addr self);   /* 0055eb10: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_free_ride_vf8(rx_addr self);   /* 0055ebd0: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_activity_vf21(rx_addr self);   /* 0055e960: self=ECX; RET 0x0 -> EAX */
uint32_t rx_world_race_stub_vf8(rx_addr self);   /* 0056a020: self=ECX; RET 0x0 -> EAX */
rx_addr rx_world_anim_entity_matrix(rx_addr self);   /* 00617d20: self=ECX; RET 0x0 -> EAX */
void rx_world_anim_entity_set_matrix_regs(rx_addr self, rx_addr m, uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi);
void rx_world_anim_entity_set_matrix(rx_addr self, rx_addr m);   /* 00617dc0 AnimationEntity::vf16: self=ECX m=s0; RET 0x4 (the _regs variant also takes the caller's EBX / ESI / EDI) */
uint32_t rx_world_rider_pose_is_current(rx_addr ra);   /* 00617f00: ra=ECX; RET 0x0 -> EAX */
uint32_t rx_world_rider_pose_is_current_5a0(rx_addr ra);   /* 00545340: ra=ECX; RET 0x0 -> EAX */
void rx_world_blend_set_18(rx_addr b, float v);   /* 006298f0: b=EAX v=xmm0; RET 0x0 */
void rx_world_blend_set_1c(rx_addr b, float v);   /* 00629900: b=EAX v=xmm0; RET 0x0 */
uint32_t rx_world_record_vector_size(rx_addr v);   /* 004db570: v=ECX; RET 0x0 -> EAX */
void rx_world_record_construct(rx_addr dst, rx_addr src);   /* 004dbfa0: dst=EAX src=ECX; RET 0x0 */
rx_addr rx_world_record_copy_range(rx_addr first, rx_addr last, rx_addr dst);   /* 004dbed0: first=ECX last=EDX dst=s0; RET 0x0 -> EAX */
rx_addr rx_world_record_uninitialized_copy(rx_addr first, rx_addr last, rx_addr dst);   /* 004dbe80: first=s0 last=EAX dst=EDI; RET 0x0 -> EAX */
uint32_t rx_world_ptr_vector_push_unique(rx_addr v, rx_addr value);   /* 00544cc0: v=ESI value=s0; RET 0x4 -> EAX */
/* 00544cc0 with the original's frames: e = its return slot, r = the caller's EBX EBP ESI EDI (world_misc.c) */
uint32_t rx_world_ptr_vector_push_unique_fr(rx_addr e, crt_regs4 r, rx_addr v, rx_addr value);
float rx_world_pad_look_back(uint32_t pad);   /* 00548660: pad=ESI; RET 0x0 -> xmm0 */
uint32_t rx_world_pad_rumble(uint32_t pad, uint32_t motor, uint32_t speed_bits);   /* 00548b00: pad=s0 motor=s1 speed_bits=s2; RET 0x0 */
uint32_t rx_world_pad_reset_combo(uint32_t pad);   /* 00548b70: pad=ESI; RET 0x0 -> EAX */
rx_addr rx_world_pad_steer_map(uint32_t pad);   /* 00548f20: pad=EAX; RET 0x0 -> EAX */
rx_addr rx_world_pad_lean_map(uint32_t pad);   /* 00548f70: pad=EAX; RET 0x0 -> EAX */
rx_addr rx_world_pad_wreck_steer_map(uint32_t pad);   /* 00548fc0: pad=ESI; RET 0x0 -> EAX */
rx_addr rx_world_pad_wreck_lean_map(uint32_t pad);   /* 00549050: pad=ESI; RET 0x0 -> EAX */
void rx_world_rider_wreck_input(rx_addr r, rx_addr out);   /* 0054ee80: r=EAX out=s0; RET 0x4 */
void rx_world_raise_event(rx_addr sender, rx_addr name, rx_addr dispatcher, uint32_t arg);   /* 00554da0: sender=ECX name=EDX dispatcher=ESI arg=s0; RET 0x4 */
void rx_world_wreck_checker_add_severity(rx_addr chk);   /* 0054e330: chk=EDX; RET 0x0 */
void rx_world_rider_clear_wreck(rx_addr r);   /* 0054e890: r=EAX; RET 0x0 */
void rx_world_rider_start_wreck(rx_addr r, rx_addr anim, uint32_t cls, uint32_t type);   /* 0054e8f0: r=EDI anim=EAX cls=ECX type=s0; RET 0x4 */
void rx_world_rider_fall(rx_addr r, uint32_t caller_esi);   /* 0054ed10: r=EDI caller_esi=ESI; RET 0x0 */
int32_t rx_world_group_rider_index(rx_addr g, rx_addr rider);   /* 0055a050: g=ECX rider=ESI; RET 0x0 -> EAX */
void rx_world_activity_audio_vf13(rx_addr self, uint32_t player, rx_addr rider);   /* 00581c30: self=ECX player=s0 rider=s1; RET 0x8 */
void rx_world_activity_audio_vf14(rx_addr self, uint32_t player, rx_addr rider, uint32_t ebp);   /* 00581cb0: self=ECX player=s0 rider=s1 ebp=EBP; RET 0x8 */
rx_addr rx_world_tree_rotate_left(rx_addr x, rx_addr tree);   /* 00586660: x=ECX tree=s0; RET 0x4 -> EAX */
rx_addr rx_world_tree_rotate_right(rx_addr x, rx_addr tree);   /* 005866b0: x=ECX tree=s0; RET 0x4 -> EAX */
void rx_world_tree_iterator_decrement(rx_addr it);   /* 00586c10: it=ESI; RET 0x0 */
uint32_t rx_world_registry_find(rx_addr registry, rx_addr name);   /* 00593220: registry=s0 name=s1; RET 0x8 -> EAX */
uint32_t rx_world_elf_hash(rx_addr s);   /* 005c3380: s=EDX; RET 0x0 -> EAX */
void rx_world_swap_dwords(rx_addr a, rx_addr b);   /* 00613420: a=s0 b=s1; RET 0x0 */
void rx_world_ui_container_vf14(rx_addr self);   /* 005cf8f0: self=ECX; RET 0x0 */
void rx_world_ui_set_color(rx_addr elem, rx_addr color);   /* 005d57e0: elem=ECX color=EAX; RET 0x0 */
void rx_world_handler_list_call(rx_addr s, uint32_t a, uint32_t b);   /* 00592d80: s=EAX a=s0 b=s1; RET 0x8 */
rx_addr rx_world_record_vector_erase(rx_addr v, rx_addr out, uint32_t first_cont, rx_addr first, uint32_t last_cont, rx_addr last);   /* 004db5b0: v=ESI out=s0 first_cont=s1 first=s2 last_cont=s3 last=s4; RET 0x14 -> EAX */
uint32_t rx_world_string_table_find(rx_addr table, rx_addr key, rx_addr out);   /* 007fa030: table=ESI key=EAX out=s0; RET 0x4 -> EAX */
rx_addr rx_world_tree_insert_at(rx_addr where, rx_addr tree, rx_addr out, uint32_t add_left, rx_addr value);   /* 005860e0: where=EAX tree=EDI out=s0 add_left=s1 value=s2; RET 0xc -> EAX */
void rx_world_quick_camera_set_mode(rx_addr cam, uint32_t mode);   /* 00646e80: mode=EAX cam=ESI; RET 0x0 */
rx_addr rx_world_player_active_camera_entity(rx_addr p);   /* 0054c020: p=EDI; RET 0x0 -> EAX */

#ifdef __cplusplus
}
#endif
