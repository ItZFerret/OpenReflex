/* world_veh.h -- the vehicle share of MX vs ATV Reflex's physics world layer (core/game/world_veh*.c): the small math helpers of
 * 0x0040xxxx / 0x0041xxxx, PhysicsEntity / PhysicsHandle_Body, the RavenVehicleMotocross virtuals, the wheel deformation and
 * track marks, the vehicle damage behaviour manager. One rx_world_<name> per original function (address in the comment).
 * Layouts and conventions: core/notes/world.md (veh). */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- world_veh_math.c ---- */
float rx_world_float_abs(float x);                                         /* 00401000: x=xmm0; RET 0x0 -> xmm0 */
float rx_world_float_abs_thunk(float x);                                   /* 00401040: x=xmm0; RET 0x0 -> xmm0 */
float rx_world_float_fmod(float a, float b);   /* 00401020: a=s0 b=s1; RET 0x0 -> st0 */
void  rx_world_vec3_cross(rx_addr out, rx_addr c, rx_addr a);   /* 00401630: out=s0 c=ECX a=EAX; RET 0x0 */
void  rx_world_vec3_sub(rx_addr out, rx_addr a, rx_addr b);   /* the 00401700 Vec3_Sub arithmetic (registered by rx_phys as phys_vec3_sub; share-local copy) */
void  rx_world_vec4_splat(rx_addr out, float s);   /* 0041c6c0: out=s0 s=s1; RET 0x8 -> EAX */
rx_v4 rx_world_vec4_cmple_cr6(rx_v4 a, rx_v4 b, rx_addr flags);            /* 0041c640: a=xmm0 b=xmm1 flags=s0 (cdecl); RET 0x0 -> xmm0 */
void  rx_world_matrix3_mul_vec3(rx_addr out, rx_addr v, rx_addr M);   /* 0041f5c0: out=ECX v=EDX M=s0; RET 0x0 -> EAX */
void  rx_world_matrix3_transpose(rx_addr dst, rx_addr src);   /* 0041f650: dst=s0 src=ECX; RET 0x0 -> EAX */
void  rx_world_matrix3_inverse(rx_addr dst, rx_addr src);   /* 0041f6e0: dst=ECX src=s0; RET 0x0 */
void  rx_world_matrix4_transpose(rx_addr dst, rx_addr src);   /* 00401760: dst=EDX src=EAX; RET 0x0 */
uint32_t rx_world_cursor_advance4(rx_addr cursor);   /* 0041ad80: cursor=ECX; RET 0x0 -> EAX */
rx_v4 rx_world_matrix3_to_quaternion(rx_addr M);   /* 00401210: M=s0; RET 0x0 -> EAX/xmm0 */
void  rx_world_matrix3_to_quaternion_to(rx_addr out, rx_addr M);   /* 00401740: out=ESI M=s0; RET 0x0 -> EAX/xmm0 */
void  rx_world_matrix3_to_quaternion_to_fr(rx_addr out, rx_addr M, uint32_t ebp);   /* the same, also the caller's EBP its push ebp saves */
void  rx_world_matrix3_mul(rx_addr out, rx_addr A, rx_addr B);   /* 0041f430: out=EDX A=ESI B=ECX; RET 0x0 */
void  rx_world_block_inverse(rx_addr A, rx_addr B, rx_addr C, rx_addr D, rx_addr out11, rx_addr out12, rx_addr out21,
                             rx_addr out22);                               /* 0041f800 */
void  rx_world_matrix3_skew(rx_addr out, rx_addr v);   /* 0041f390: out=s0 v=ECX; RET 0x4 -> EAX */
void  rx_world_quat_to_matrix3(rx_addr out, rx_addr q);   /* 0041f160: out=ECX q=s0; RET 0x4 -> EAX */
void  rx_world_bone_point_world(rx_addr out, rx_addr skel, uint32_t bone);   /* 0041feb0: out=EDI skel=ESI bone=EDX; RET 0x0 */
void  rx_world_skeleton_orient(rx_addr skel);   /* 0041f030: skel=ESI; RET 0x0 */
void  rx_world_skeleton_set_velocity(rx_addr skel, rx_addr v, rx_addr arg);   /* 0041fe60: skel=EDX v=EDI arg=s0; RET 0x4 */

/* ---- world_veh_entity.c ---- */
void     rx_world_damage_physics_entity_write_back(rx_addr entity, rx_addr pos, rx_addr q); /*00430560 thiscall RET8*/
void     rx_world_damage_physics_entity_get_pose(rx_addr entity, rx_addr pos, rx_addr q);   /* 00430670 thiscall RET8 */
void     rx_world_physics_entity_vf19(rx_addr entity, uint32_t arg);   /* 0041cc60 thiscall RET4 */
void     rx_world_physics_entity_disable(rx_addr entity);   /* 0041cbe0 thiscall */
uint32_t rx_physics_entity_get_pose(rx_addr e, rx_addr pos, rx_addr quat);   /* 00403b20 (build_scene_props.c): ECX = entity; RET 8 */
void     rx_world_physics_entity_write_back(rx_addr entity, rx_addr pos, rx_addr q);   /* 004039a0: entity=ECX pos=s0 q=s1; RET 0x8 -> EAX */
void     rx_physics_entity_place(rx_addr entity, rx_addr pos, rx_addr q);      /* 004037c0 PhysicsEntity::vf28 (thiscall; RET 8) */
void     rx_physics_entity_teleport(rx_addr entity, rx_addr pos, rx_addr q);   /* 00403940 PhysicsEntity::vf29 (thiscall; RET 8) */
uint32_t rx_world_physics_entity_get148(rx_addr entity);   /* 00403b90: entity=ECX; RET 0x0 -> EAX */
uint32_t rx_world_physics_entity_get14c(rx_addr entity);   /* 00403ba0: entity=ECX; RET 0x0 -> EAX */
uint32_t rx_world_physics_entity_type_id(void);   /* 0041cce0: -; RET 0x0 -> EAX */
uint32_t rx_world_event_handler_is_state1(rx_addr self);   /* 00402090: self=ECX; RET 0x0 -> EAX */
rx_addr  rx_world_physics_handle_body(rx_addr handle);   /* 00418f30: handle=EAX; RET 0x0 -> EAX */
/* PhysicsEntity's destruction and the keyed body removal (world_veh_entity.c, w-partdtor) */
rx_addr  rx_world_keyed_record_assign(rx_addr dst, rx_addr src);                    /* 00417c80: dst=ESI src=EDI -> EAX */
void     rx_world_keyed_records_move_slot(rx_addr vec, uint32_t index, rx_addr src);  /* 00417830: stdcall vec, index, src; RET 0xc */
void     rx_world_keyed_records_remove(rx_addr vec, uint32_t index);                /* 00416fe0: vec=ESI index=s0; RET 0x4 */
void     rx_world_keyed_body_remove(rx_addr world, uint32_t index);                 /* 00402360: world=s0 index=EDI; RET 0x4 */
void     rx_physics_object_handle_reset(rx_addr h);                                 /* 00416f90 IntrusiveHandle<ObjectMonitor>::vf1 (ECX) */
void     rx_physics_entity_destroy(rx_addr e);                                      /* 0041c7b0 (ECX = entity) */
rx_addr  rx_physics_entity_sdtor(rx_addr e, uint32_t flags);                        /* 00403bb0 PhysicsEntity::sdtor (thiscall; RET 4) -> this */
void     rx_world_physics_entity_velocity(rx_addr entity, rx_addr out);   /* 0041cea0: entity=ESI out=EDI; RET 0x0 */
void     rx_world_physics_entity_speed(rx_addr entity, rx_addr out);   /* 0041cf30: entity=ECX out=s0; RET 0x4 */
rx_addr  rx_world_find_by_controller(rx_addr list);   /* 00402dc0: list=EDX; RET 0x0 -> EAX */
void     rx_world_event_dispatch(rx_addr chain, rx_addr name, uint32_t a, uint32_t b);   /* 004020c0: chain=EAX name=EDI a=s0 b=s1; RET 0x8 -> EAX */

/* ---- world_veh_raven.c ---- */
void     rx_world_raven_get_7c0(rx_addr veh, rx_addr out);   /* 00466920: veh=ECX out=s0; RET 0x4 -> EAX */
rx_addr  rx_world_vehicle_block_140(rx_addr self);   /* 00430280: this=ECX; RET 0x0 -> EAX */
rx_addr  rx_world_vehicle_block_matrix(rx_addr self);   /* 00430340: this=ECX; RET 0x0 -> EAX */
void     rx_world_raven_set_sim_type(rx_addr veh, uint32_t value);   /* 00462950: veh=ECX value=s0; RET 0x4 */
void     rx_world_vehicle_set_block_flag(rx_addr veh, uint32_t value);   /* 004302a0: veh=ECX value=s0; RET 0x4 */
void     rx_world_raven_get_7a0(rx_addr veh, rx_addr out);   /* 004669d0: veh=ECX out=s0; RET 0x4 -> EAX = out */
uint32_t rx_world_raven_vf61(rx_addr veh);   /* 004666e0: veh=ECX; RET 0x0 -> EAX */
uint32_t rx_world_raven_frame(rx_addr sim, uint32_t edx_in, uint32_t *edx_out);   /* 0044aec0: sim=ECX edx_in=EDX; RET 0x0 -> EAX, EDX through edx_out */
void     rx_world_vehicle_o94_copy_nodes(rx_addr self);   /* 004302c0: this=ECX (vehicle + 0x94); RET 0x0 */
rx_addr  rx_world_shared_vf_sub94(rx_addr self);   /* 00430290: this=ECX; RET 0x0 -> EAX */
void     rx_world_raven_get_100(rx_addr veh, rx_addr out);   /* 00466940: veh=ECX out=s0; RET 0x4 -> EAX */
void     rx_world_raven_get_740(rx_addr veh, rx_addr out);   /* 00466960: veh=ECX out=s0; RET 0x4 -> EAX */
void     rx_world_raven_get_160(rx_addr veh, rx_addr out);   /* 004669b0: veh=ECX out=s0; RET 0x4 -> EAX */
void     rx_world_raven_get_8b60_plus_740(rx_addr veh, rx_addr out);   /* 00466980: veh=ECX out=s0; RET 0x4 -> EAX */
uint32_t rx_world_raven_get_gear(rx_addr veh);   /* 00466a20: veh=ECX; RET 0x0 -> EAX */
uint32_t rx_world_raven_get_8d98(rx_addr veh);   /* 00466ae0: veh=ECX; RET 0x0 -> EAX */
uint32_t rx_world_raven_get_8da0(rx_addr veh);   /* 00466af0: veh=ECX; RET 0x0 -> EAX */
float    rx_world_raven_wheel_value(rx_addr veh, uint32_t which, rx_addr arg_slot);   /* 00466b70: veh=ECX which=ECX+0x16d4 arg_slot=ESP+4; RET 0x4 -> EAX/st0 */
uint32_t rx_world_raven_state_code(rx_addr veh);   /* 00466a30: veh=ECX; RET 0x0 -> EAX */
uint32_t rx_world_raven_has_id(rx_addr veh, uint32_t id);   /* 00467440: veh=ECX id=s0; RET 0x4 -> EAX */
uint32_t rx_world_raven_atv_kind(rx_addr veh);   /* 00467470: veh=ECX; RET 0x0 -> EAX */
uint32_t rx_world_raven_id_kind(rx_addr veh, uint32_t id);   /* 00467490: veh=ECX id=s0; RET 0x4 -> EAX */
void     rx_world_raven_sim_440da0(rx_addr veh, float x);   /* 0046a380: veh=EAX x=s0; RET 0x4 */
rx_addr  rx_world_damage_manager_instance(void);   /* 0047bb40: -; RET 0x0 -> EAX */
void     rx_world_damage_manager_update(rx_addr mgr, rx_addr world);   /* 0047bd10: mgr=ECX world=s0; RET 0x4 */
void     rx_world_raven_wheels_mid(rx_addr veh, rx_addr out);   /* 00466720: veh=ECX out=s0; RET 0x4 */
rx_f80   rx_world_raven_grip_factor(rx_addr veh, uint32_t wheel);   /* 00466bd0: veh=ECX wheel=s0; RET 0x4 -> st0 */
void     rx_world_raven_update_nodes(rx_addr veh);   /* 004647c0: veh=ECX; RET 0x4 */
void     rx_world_rider_pose(rx_addr veh);   /* 004674e0: veh=s0; RET 0x4 */
void     rx_world_key_track_block_sample(rx_addr block);   /* 00476340: block=s0; RET 0x4 */
void     rx_world_raven_frame_update(rx_addr veh, float dt);   /* 004636f0: veh=ECX dt=s0; RET 0x4 */

/* ---- world_veh_track.c ---- */
void rx_world_wheel_deform_and_track_marks(rx_addr P, rx_addr veh, rx_addr surf, float dt);   /* 004607e0: P=ECX veh=s0 surf=s1 dt=s2; RET 0xc */
void rx_world_slip_track_mark(rx_addr Q, rx_addr veh);   /* 00468120: Q=EDI veh=s0; RET 0x4 */

/* ---- world_veh_vehicle.c ---- */
void  rx_world_node_world_pose(rx_addr node, rx_addr pos_out, rx_addr q_out);   /* 004603f0: node=EAX pos_out=EDI q_out=s0; RET 0x4 */
float rx_world_terrain_grip(rx_addr point, rx_addr owner, rx_addr obj);   /* 00461680: point=EDX owner=EDI obj=s0; RET 0x4 -> xmm0 */
rx_v4 rx_world_terrain_layer1_factor(rx_addr point, rx_addr owner);   /* 00461760: point=EDX owner=EDI; RET 0x0 -> xmm0 */
void  rx_world_water_wake(rx_addr ctx, rx_addr veh, rx_addr W, float dt);   /* 00461220: ctx=ECX veh=s0 W=s1 dt=s2; RET 0xc */
void  rx_world_landing_rumble(rx_addr veh, float impact);   /* 004617b0: veh=ESI impact=xmm0; RET 0x0 */
void  rx_world_vehicle_camera_update(rx_addr veh, float dt);   /* 004618e0: veh=ESI dt=s0; RET 0x4 */

void  rx_world_spline_nearest_angles(rx_addr sp, rx_addr a1, rx_addr a2);   /* 0041e080: sp=ESI a1=s0 a2=s1; RET 0x8 */

void  rx_world_vehicle_wheels(rx_addr veh, float dt);   /* 00468300: veh=s0 dt=s1; RET 0x8 */

/* ---- world_veh_motocross.c */
void  rx_world_motocross_vf62(rx_addr veh);   /* 00462d40 RavenVehicleMotocross::vf62: veh=ECX; RET 0x0 */
void  rx_world_motocross_vf29(rx_addr veh, float dt);   /* 00463900 RavenVehicleMotocross::vf29: veh=ECX dt=s0; RET 0x4 */
void  rx_world_motocross_vf35(rx_addr veh, float dt);   /* 00464e80 RavenVehicleMotocross::vf35: veh=ECX dt=s0; RET 0x4 */

#ifdef __cplusplus
}
#endif
