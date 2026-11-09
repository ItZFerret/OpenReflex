/* world.h -- MX vs ATV Reflex's physics world layer (core/game/world_*.c): the per-frame physics update, its listeners and
 * solver dispatch, the generic dynamic / kinematic / rider bodies and their collisions, contact events, the node hierarchy, the
 * vehicle / rider shared world code, the wreck checker, camera impacts, float properties and the uniform cell grid.
 * One rx_world_<name> per original function (address in the comment). Layouts and conventions: core/notes/world.md. */
/* Conventions: each prototype's comment gives the original address and how the original passes the C parameters:
 * name=REG (a register), name=sN (the N-th 32-bit stack argument), name=xmmN (lane 0 of an xmm register); RET n = the bytes
 * the original pops; -> EAX / xmm0 / st0 = the result registers. Virtual functions are (this = ECX, stack args...).
 * ebp=EBP / saved=...: the caller's callee-saved registers, which the original only saves (pushes) or passes on: the port
 * writes them where the original's prologue leaves them (dead stack some later code reads uninitialized, notes/world.md).
 * Cross-agent callers: call through these prototypes only (never an RX_FN cast to a guessed signature). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- world_phys.c: the physics world update (0x00769xxx-0x0076dxxx) ---- */
void rx_world_physics_update(rx_addr world, float dt);   /* 00769b80: world=EAX dt=s0; RET 0x4 */
void rx_world_pre_listeners(rx_addr world, float dt);   /* 007699e0: world=EDI dt=s0; RET 0x4 */
void rx_world_post_listeners(rx_addr world, float dt);   /* 00769ab0: world=ESI dt=s0; RET 0x4 */
void rx_world_solver_dispatch(rx_addr world, float dt);   /* 0076a010: world=EAX dt=s0; RET 0x4 */
void rx_world_handler_list_compact_and_run(rx_addr list, rx_addr world);   /* 00768750: list=EAX world=s0; RET 0x4 */
void rx_world_dynobj_collide_all(rx_addr world);   /* 0076bf60: world=s0; RET 0x4 */
void rx_world_dynobj_collide_terrain(rx_addr world);   /* 0076d3c0: world=s0; RET 0x4 */
void rx_world_dynobj_collide_static(rx_addr world);   /* 0076b4a0: world=s0; RET 0x4 */
void rx_world_dynobj_collide_kinematic(rx_addr world);   /* 0076b9b0: world=s0; RET 0x4 */
void rx_world_dynobj_collide_water(rx_addr world);   /* 0076c510: world=ESI; RET 0x0 */
void rx_world_kinematic_collide_water(rx_addr world);   /* 0076ac40: world=s0; RET 0x4 */
void rx_world_kinematic_collide_terrain(rx_addr world);   /* 0076d9a0: world=ESI; RET 0x0 */
void rx_world_kinematic_integrate(rx_addr world, float dt);   /* 0076a360: world=ECX dt=xmm0; RET 0x0 */
void rx_world_kinematic_collide_static(rx_addr world);   /* 0076b070: world=EDI; RET 0x0 */
void rx_world_kinematic_collide_keyed(rx_addr world);   /* 0076cd00: world=EDI; RET 0x0 */
void rx_world_rider_collide_keyed(rx_addr world);   /* 0076c880: world=EDI; RET 0x0 */
void rx_world_kinematic_reset_authored_pose(rx_addr world, rx_addr body); /*0076abb0 ECXworld EDXbody RET0*/
void rx_world_kinematic_reset_lost(rx_addr world);   /* 0076a1e0: world=ESI; RET 0x0 */
uint32_t rx_world_query_shape_contacts(rx_addr types, rx_addr shape, rx_addr world, rx_addr pos, rx_addr results,
                                       rx_addr ids);                       /* 007729f0 */
void rx_world_vehicle_collect_nearby_statics(rx_addr list, rx_addr query, rx_addr world, rx_addr box);   /* 00770e80: list=EDI query=ECX world=s0 box=s1; RET 0x8 -> EAX */

/* ---- world_body.c: body accessors, rigid-body impulses, contact responses, handle-list algorithms (0x00767f80-0x00777fff) */
float rx_world_recip(rx_addr p);   /* 00767f80: p=EAX; RET 0x0 -> xmm0 */
void rx_world_vec3_reciprocal_of(rx_addr obj, rx_addr out);   /* 00767f90: obj=ECX out=s0; RET 0x4 -> EAX */
void rx_world_dynobj_set_pose(rx_addr handle, rx_addr pos, rx_addr rot, rx_addr vel, rx_addr angvel);   /* 00768460: handle=s0 pos=s1 rot=s2 vel=s3 angvel=s4; RET 0x14 */
void rx_world_dynobj_take_contact_results(rx_addr handle, rx_addr out1, rx_addr out2, rx_addr out3);   /* 007684c0: handle=s0 out1=ESI out2=EDI out3=s1; RET 0x8 */
void rx_world_ptr_vector_push_if_room(rx_addr v, uint32_t value);   /* 00768550: v=EAX value=s0; RET 0x4 */
void rx_world_ptr_vector_clear_value(rx_addr v, uint32_t value);   /* 00768590: v=EDI value=s0; RET 0x4 */
void rx_world_ptr_vector_replace_value(rx_addr v, uint32_t old, uint32_t value);   /* 007685f0: v=EDI old=s0 new=s1; RET 0x8 */
rx_addr rx_world_handler_list_next_active(rx_addr list);   /* 00768650: list=ESI; RET 0x0 -> EAX */
rx_addr rx_world_keyed_object_at(rx_addr header, uint32_t index);   /* 00417070: header=ESI index=EDI; RET 0x0 -> EAX */
void rx_world_vector_apply_axis_lock(rx_addr vec, rx_addr body);   /* 00769f40: vec=ECX body=EDX; RET 0x0 */
rx_addr rx_world_body_record_alloc(uint32_t count);   /* 004db640: count=EAX; RET 0x0 -> EAX */
void rx_world_body_record_copy(rx_addr dst, rx_addr src);   /* 00430030: dst=EAX src=ECX; RET 0x0 */
rx_addr rx_world_body_record_fill_end(rx_addr dest, uint32_t n, rx_addr value);   /* 00774ce0: dest=EDI n=ESI value=EDX; RET 0x0 -> EAX */
rx_addr rx_world_body_record_copy_construct(rx_addr first, rx_addr last, rx_addr dest);   /* 004dbf20: first=s0 last=s1 dest=s2 (+2 unused words); RET 0x0 -> EAX */
rx_addr rx_world_body_record_copy_uninit(rx_addr dest, rx_addr first, rx_addr last);   /* 00776d50: dest=EDX first=s0 last=s1; RET 0x8 -> EAX */
rx_addr rx_world_body_record_assign_range(rx_addr src, rx_addr end, rx_addr dest);   /* 00777400: src=ECX end=EDX dest=s0; RET 0x0 -> EAX */
rx_addr rx_world_body_record_copy_backward(rx_addr first, rx_addr last, rx_addr dest_end);   /* 00778020: last=ECX first=EDX dest_end=s0; RET 0x0 -> EAX */
rx_addr rx_world_body_record_move_backward(rx_addr first, rx_addr last, rx_addr dest_end);   /* 00777a60: last=EAX dest_end=EDI first=s0 (+3 words); RET 0x0 -> EAX */
rx_addr rx_world_body_record_iterator_advance(rx_addr iter, rx_addr out, int32_t offset);   /* 00775b10: iter=EAX out=s0 offset=s1; RET 0x8 -> EAX */
void rx_world_body_record_vector_insert(rx_addr vec, rx_addr value, rx_addr container, rx_addr where);   /* 00775800: vec=ECX value=s0 container=s1 where=s2; RET 0xc */
rx_addr rx_world_body_record_vector_insert_at(rx_addr vec, rx_addr out, rx_addr value, rx_addr container, rx_addr where);   /* 00774c30: vec=ESI out=s0 value=s1 container=s2 where=s3; RET 0x10 -> EAX */
int rx_world_kinematic_is_slower(rx_addr world, rx_addr A, rx_addr B);   /* 007686b0: world=s0 A=EDX B=s1; RET 0x8 -> EAX */
void rx_world_rigid_body_contact_impulse(rx_addr vel, rx_addr inv_inertia, rx_addr inv_mass, rx_addr orient, rx_addr centre,
                                          rx_addr angvel, rx_addr restitution, rx_addr friction, rx_addr point,
                                          rx_addr normal);                 /* 007705b0 */
rx_addr rx_world_ptr_remove(rx_addr result, rx_addr value, rx_addr first_c, rx_addr first_p, rx_addr last_c,
                            rx_addr last_p);                               /* 00776920 */
rx_addr rx_world_ptr_remove_copy(rx_addr result, rx_addr first, rx_addr last, rx_addr value, uint32_t unused, rx_addr dest_c,
                                 rx_addr dest_p);                          /* 007774f0 */
void rx_world_body_sort_insertion(rx_addr first, rx_addr last, rx_addr world);   /* 00777db0: first=s0 last=s1 world=s2; RET 0x0 */
void rx_world_body_sort_loop(rx_addr first, rx_addr last, int32_t ideal, rx_addr world);   /* 00776ef0: first=s0 last=s1 ideal=s2 world=s3; RET 0x0 */
void rx_world_body_med3(rx_addr a, rx_addr b, rx_addr c, rx_addr world);   /* 00778190: ESI = a, EDI = b; c=s0 world=s1; plain RET */
void rx_world_body_median(rx_addr first, rx_addr mid, rx_addr last1, rx_addr world);   /* 00777c70: EAX = first; mid=s0 last1=s1 world=s2; plain RET */
void rx_world_body_partition(rx_addr out, rx_addr first, rx_addr last, rx_addr world);   /* 00777580: out=s0 first=s1 last=s2 world=s3; plain RET, EAX = out */
void rx_world_body_push_heap(int32_t hole, rx_addr first, int32_t top, uint32_t value, rx_addr world);   /* 00778660: EAX = hole; first=s0 top=s1 value=s2 world=s3 */
void rx_world_body_adjust_heap(int32_t hole, rx_addr first, int32_t n, uint32_t value, rx_addr world);   /* 00778200: EAX = hole; first=s0 n=s1 value=s2 world=s3 */
void rx_world_body_make_heap(rx_addr first, rx_addr last, rx_addr world);   /* 00777d20: EAX = last, EDI = first; world=s0 */
void rx_world_body_sort_heap(rx_addr first, rx_addr last, rx_addr world);   /* 00777d60: EAX = last, EDI = first; world=s0 */
uint32_t rx_world_body_record_fill_n(rx_addr dest, uint32_t n, rx_addr value);   /* 00777110: dest=s0 n=s1 value=s2; RET 0x0 -> EAX */
void rx_world_kinematic_accelerations(uint32_t k, rx_addr acc, rx_addr qdot, rx_addr world, rx_addr mat, rx_addr vel,
                                      rx_addr ang_acc);                    /* 00769bd0 */
void rx_world_static_contact_response(rx_addr S, rx_addr A, rx_addr contact_mean);   /* 0076efc0: S=ECX A=EDI contact_mean=s0; RET 0x4 */
void rx_world_terrain_contact_response(rx_addr A, rx_addr contact_mean);            /* 0076f160: A=EDI contact_mean=s0; RET 0x4 */
void rx_world_body_record_vector_push_back(rx_addr v, rx_addr rec);   /* 00773d10: v=EAX rec=s0; RET 0x4 */
void rx_world_dynamic_contact_response(rx_addr A, rx_addr B, rx_addr contact_mean);   /* 0076e460: A=EDI B=s0 contact_mean=s1; RET 0x8 */
void rx_world_keyed_obj_contact_response(rx_addr contact_pt, rx_addr A, rx_addr world, rx_addr K);   /* 0076e090: contact_pt=ECX A=EDI world=s0 K=s1; RET 0x8 */
void rx_world_kinematic_deactivate(rx_addr body, rx_addr world);   /* 0076a8b0: body=EDX world=s0; RET 0x4 -> EAX */
void rx_world_keyed_record_moved(uint32_t old_index, rx_addr world, uint32_t new_index);   /* 0076a9a0: old=EAX world=ESI new=s0; RET 0x4 */
void rx_world_keyed_record_unlist(uint32_t index, rx_addr world);   /* 0076aa50: index=ECX world=ESI */
int rx_world_kinematic_activate(rx_addr body, rx_addr world, rx_addr velocity);   /* 00770b00: body=ECX world=s0 velocity=s1; RET 0x8 -> EAX */
void rx_world_kinematic_apply_linear_lock(rx_addr world, rx_addr body, uint32_t k);   /* 00769d40: world=ECX body=ESI k=s0; RET 0x4 */
void rx_world_kinematic_apply_angular_lock(rx_addr world, rx_addr body, uint32_t k);   /* 00769e40: world=ECX body=ESI k=s0; RET 0x4 */
void rx_world_kinematic_terrain_contact_impulse(rx_addr O, rx_addr world, rx_addr contact); /*0076f2f0 thiscall world,contact RET8*/
void rx_world_kinematic_water_contact(rx_addr O, rx_addr world, rx_addr contact_pt);   /* 0076dd50: O=ECX world=s0 contact_pt=s1; RET 0x8 */
void rx_world_kinematic_static_contact_impulse(rx_addr O, rx_addr S, rx_addr world, rx_addr contact);   /* 0076ee60: O=ECX S=EDX world=s0 contact=s1; RET 0x8 */
void rx_world_kinematic_keyed_contact_impulse(rx_addr contact, rx_addr K, rx_addr world, rx_addr O);   /* 0076ea00: contact=ECX K=EDX world=s0 O=s1; RET 0x8 */
void rx_world_dynobj_water_contact(rx_addr world, rx_addr contact, rx_addr A);   /* 0076dec0: world=ECX contact=s0 A=EDI; RET 0x4 */
void rx_world_rider_keyed_contact_impulse(rx_addr contact, rx_addr K, rx_addr world, rx_addr R);   /* 0076e6c0: contact=ECX K=EDX world=s0 R=s1; RET 0x8 */
int rx_world_box_free_of_dynamics(rx_addr world, rx_addr box_min, rx_addr box_max);   /* 00773110: world=EDI box_min=EDX box_max=EAX; RET 0x0 -> EAX */
void rx_world_rigid_body_contact_impulse_pair(rx_addr vel_a, rx_addr inv_mass_a, rx_addr inv_inertia_a, rx_addr orient_a,
                                              rx_addr centre_a, rx_addr angvel_a, rx_addr inv_mass_b, rx_addr inv_inertia_b,
                                              rx_addr orient_b, rx_addr centre_b, rx_addr angvel_b, rx_addr vel_b,
                                              rx_addr restitution, rx_addr friction, rx_addr point, rx_addr normal);   /* 0076f440 */

#ifdef __cplusplus
}
#endif
