/* world_rider.h -- the vehicle / rider shared world code of the physics world layer (core/game/world_rider*.c, original range
 * 0x004d0000..0x0062ffff): key tracks, the rider / body terrain-track contacts, the rider look-at controller, the rider
 * animation and animation-entity virtuals that the physics frame reaches, the player / activity / scenario helpers.
 * One rx_world_<name> per original function (address in the comment). Layouts and conventions: core/notes/world.md (rider). */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- world_rider_4d.c: key tracks, terrain-track contacts, 16-byte / 0x50-byte vectors (0x004dxxxx .. 0x0054xxxx helpers) */
void     rx_world_vec4_from_scalars(rx_addr out, float x, float y, float z, float w);   /* 005452d0: out=s0 x=xmm3 y=xmm2 z=xmm1 w=xmm0; RET 0x4 -> EAX */
uint32_t rx_world_keytrack_find_segment(rx_addr keys, uint32_t count, float t, rx_addr out_prev, rx_addr out_next,
                                        rx_addr out_frac);                                                /* 004dbce0 */
void     rx_world_tracks_update_contact_slots(rx_addr last, rx_addr active, rx_addr owner, int32_t count, rx_addr touching,
                                              rx_addr pos, rx_addr vel, rx_addr slots, rx_addr intensity);       /* 004db710 */
void     rx_world_tracks_add_contact_points(rx_addr surface, rx_addr pos, rx_addr prev, rx_addr slots, float scale,
                                            rx_addr touching, rx_addr group, rx_addr weights, rx_addr radii,
                                            float dt);                                                    /* 004db8e0 */

/* ---- world_rider_55.c: players, activities, scenarios (0x0054xxxx .. 0x005dxxxx) ---- */
rx_addr  rx_world_players_find(rx_addr players, uint32_t index, uint32_t kind);   /* 00552b90: players=EDX index=s0 kind=s1; RET 0x8 -> EAX */

/* ---- world_rider_62.c: the rider look-at controller (0x0062xxxx) ---- */
uint32_t rx_world_lookat_find_target(rx_addr look);   /* 00625390: look=EAX; RET 0x0 -> EAX */
void     rx_world_lookat_set_target(rx_addr look, rx_addr rider);   /* 006258a0: look=ESI rider=EAX; RET 0x0 */
uint32_t rx_world_lookat_update_search(rx_addr look);   /* 00625560: look=EAX; RET 0x0 -> EAX */
void     rx_world_body_forward(rx_addr body, rx_addr out);   /* 00508480: body=ECX out=EAX; RET 0x0 */
void     rx_world_body_up(rx_addr body, rx_addr out);   /* 00508460: body=ECX out=EAX; RET 0x0 */
void     rx_world_lookat_aim_ahead(rx_addr look, rx_addr out, rx_addr dir, float h);   /* 006255a0: look=ESI out=EDI dir=EAX h=s0; RET 0x4 */
void     rx_world_lookat_update(rx_addr look, float dt);   /* 00624ef0: look=EAX dt=s0; RET 0x4 */
void     rx_world_shape_set_radius(rx_addr shape, float r);   /* 00617e60: shape=s0 r=s1; RET 0x8 */
void     rx_world_rider_check_body_hits(rx_addr ra);   /* 00624560: ra=ESI; RET 0x0 */

/* ---- world_rider_61.c: the rider animation update (RiderAnimation, 0x0061xxxx .. 0x0062xxxx) ---- */
void     rx_world_rider_blend_overlay(rx_addr ra, float dt);   /* 0061ca60: ra=ESI dt=xmm1; RET 0x0 */
void     rx_world_rider_update_lean(rx_addr ra, float dt);   /* 0061d350: ra=ESI dt=s0; RET 0x4 */
void     rx_world_rider_update_body_weight(rx_addr ra, float dt);   /* 0061faa0: ra=EAX dt=s0; RET 0x14 */
void     rx_world_rider_update_pose_weights(rx_addr ra, float dt);   /* 006230c0: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_update_steer_weight(rx_addr ra, float dt);   /* 0061ecd0: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_update_limb_ranges(rx_addr ra);   /* 0061e770: ra=EAX; RET 0x0 */
float    rx_world_rider_lean_target(rx_addr steer, rx_addr lean);   /* 006206d0: steer=ECX lean=EAX; RET 0x0 -> xmm0 */
void     rx_world_rider_update_steer(rx_addr ra, float steer, float b, float lean, float roll);   /* 00620110: ra=EAX steer=s0 b=s1 lean=s2 roll=s3; RET 0x10 */
void     rx_world_rider_update_crouch(rx_addr ra);   /* 0061feb0: ra=EAX; RET 0x10 */
void     rx_world_rider_chains_fade_in(rx_addr ra, uint32_t sel, uint32_t force, float t);   /* 006237a0: ra=ECX sel=EAX force=ESI t=s0; RET 0x4 */
void     rx_world_rider_chains_fade_out(rx_addr ra, uint32_t sel, float t, uint32_t force);   /* 006235e0: ra=ECX sel=EAX t=s0 force=s1; RET 0x8 */
void     rx_world_rider_update_gesture(rx_addr ra, float dt);   /* 0061eac0: ra=EDI dt=s0; RET 0x4 */
void     rx_world_rider_start_idle_gesture(rx_addr ra);   /* 0061ea90: ra=ESI; RET 0x0 */
void     rx_world_rider_update_arm_chains(rx_addr ra);   /* 00620750: ra=EDI; RET 0x0 */
void     rx_world_rider_chain_event(rx_addr ev, rx_addr ra);   /* 00621030: ev=EAX ra=ECX; RET 0x0 */
void     rx_world_rider_play_clip_events(rx_addr ra, rx_addr clip, float t1, float t2);   /* 00620fc0: ra=s0 clip=s1 t1=s2 t2=s3; RET 0x10 */
void     rx_world_rider_end_trick(rx_addr ra);   /* 00621320: ra=ECX; RET 0x0 */
void     rx_world_rider_end_pose_override(rx_addr ra);   /* 00621490: ra=s0; RET 0x4 */
void     rx_world_rider_update_trick(rx_addr ra, float dt);   /* 0061dd60: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_update_blends(rx_addr ra, float dt);   /* 0061d8b0: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_update_idle(rx_addr ra, float dt);   /* 0061e120: ra=ESI dt=s0; RET 0x4 */
void     rx_world_rider_update_scripted(rx_addr ra, float dt);   /* 0061dfd0: ra=ECX dt=s0; RET 0x4 */
void     rx_world_rider_end_stunt(rx_addr ra);   /* 00621400: ra=EAX; RET 0x0 */
void     rx_world_rider_update_scripted_lean(rx_addr ra, float dt);   /* 0061db10: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_ragdoll_blend(rx_addr ra, uint32_t freeze);   /* 00621850: ra=EAX freeze=s0; RET 0x4 */
void     rx_world_rider_update_ragdoll(rx_addr ra, float dt);   /* 006217a0: ra=ESI dt=s0; RET 0x4 */
void     rx_world_rider_end_crash(rx_addr ra);   /* 00622e30: ra=s0; RET 0x4 */
void     rx_world_rider_start_scripted(rx_addr ra, uint32_t index);   /* 00621b90: ra=EDI index=s0; RET 0x4 */
void     rx_world_rider_start_scripted_mode(rx_addr ra, uint32_t index);   /* 00621b20: ra=EAX index=ESI; RET 0x0 */
void     rx_world_rider_update_scripted_start(rx_addr ra);   /* 00621ab0: ra=EAX; RET 0x0 */
rx_addr  rx_world_rider_chain_group(rx_addr ra, uint32_t k);   /* 00623960: ra=EAX k=ECX; RET 0x0 -> EAX */
void     rx_world_rider_end_wreck(rx_addr ra, uint32_t cls);   /* 00623310: ra=s0 cls=s1; RET 0x8 */
void     rx_world_gesture_set_level(rx_addr s, int32_t level);   /* 0062a370: s=EAX level=EDI; RET 0x0 */
void     rx_world_gesture_set_hold(rx_addr s, uint32_t hold);   /* 0062a420: s=EDX hold=EDI; RET 0x0 */
uint32_t rx_world_gesture_allowed(rx_addr s);   /* 0062a9a0: s=EAX; RET 0x0 -> EAX */
void     rx_world_gesture_wobble(rx_addr s);   /* 0062aa30: s=ECX; RET 0x0 */
void     rx_world_gesture_update(rx_addr s, float dt);   /* 0062a490: s=ESI dt=s0; RET 0x4 */
void     rx_world_rider_root_matrix(rx_addr ra, rx_addr out);   /* 006243d0: ra=ECX out=EAX; RET 0x0 */
void     rx_world_rider_arm_side(rx_addr ra);   /* 0061e950: ra=EDI; RET 0x0 */
void     rx_world_rider_update_locomotion(rx_addr ra, float dt);   /* 0061e3d0: ra=EAX dt=s0; RET 0x4 */
void     rx_world_rider_animation_vf19(rx_addr ra, float dt);   /* 0061c210: ra=ECX dt=s0; RET 0x4 */
void     rx_world_rider_update_state(rx_addr ra, float dt);   /* 0061f160: ra=s0 dt=s1; RET 0x8 */
void     rx_world_rider_slow_pose(rx_addr ra);   /* 00621100: ra=ESI; RET 0x0 */
void     rx_world_rider_crouch_pose(rx_addr ra);   /* 006211d0: ra=EAX; RET 0x0 */

#ifdef __cplusplus
}
#endif
