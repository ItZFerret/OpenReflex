/* world_veh_damage.h -- the damage behaviour element and its shape handle constructors (world_veh_damage.c). */
#pragma once
#include "rx_game.h"
rx_addr rx_collision_shape_handle_construct(rx_addr h);       /* 00844100 (fastcall ECX) -> EAX = ECX */
rx_addr rx_damage_behavior_construct(rx_addr b);              /* 004798e0 (fastcall ECX) -> EAX = ECX */
void rx_world_effect_record_start(rx_addr rec, rx_addr quat, rx_addr offset, rx_addr position, float strength2);   /* 006971f0: rec=ESI quat=EAX offset=ECX position=s0 strength2=s1; RET 8 */
void rx_world_effect_set_start(rx_addr set, uint32_t id, rx_addr position, rx_addr quat, rx_addr offset, float strength2);   /* 00697b10 (stdcall; RET 0x18) */
void rx_world_vehicle_effects_emit(rx_addr vehicle, uint32_t id, rx_addr position, rx_addr quat, rx_addr offset, float strength);   /* 00691c30 (cdecl) */
void rx_world_damage_hit_points(rx_addr behaviour, uint32_t index, float strength);   /* 0047ba80: behaviour=ECX index=EAX strength=xmm0 */
void rx_world_damage_apply_contact(rx_addr ev, rx_addr behaviour);   /* 0047a970: ev=ECX behaviour=s0; RET 4 */
void rx_world_damage_reset_parts(rx_addr behaviour);   /* 0047a880 (stdcall behaviour; RET 4) */
void rx_world_damage_part_spring(rx_addr e, uint32_t i, float dt);   /* 0047aff0 (stdcall e, part index, dt; RET 0xc) */
void rx_vehicle_foley_end_stop(rx_addr vehicle);              /* 00681750 (ESI = the vehicle) */
rx_addr rx_damage_parse_part_shape(rx_addr out, rx_addr offset_out, rx_addr desc);   /* 00479680 (ECX = out handle, EDX = offset vector, stack description) -> EAX = out */
rx_addr rx_shape_composite_construct(rx_addr s, uint32_t n);          /* 00847c80 (stack shape, n; RET 8) */
rx_addr rx_shape_dynamic_composite_construct(rx_addr s, uint32_t n);  /* 00849080 (EDI = shape, stack n; RET 4) */
rx_addr rx_phys_object_create_from_handle(rx_addr entity, rx_addr world, rx_addr out, rx_addr handle, uint32_t soft);   /* 007697e0 (ECX = entity; world, out, handle, soft; RET 0x10) */
void    rx_physics_entity_attach_body(rx_addr handle, rx_addr entity, rx_addr world);   /* 0041cd90 (ECX = handle; entity, world; RET 8) */
void    rx_vehicle_damage_entity_setup(rx_addr entity, rx_addr handle, uint32_t phys_type, rx_addr name);   /* 0041cb60 (ESI = entity, ECX = handle; type, name; RET 8) */
rx_addr rx_damage_split_part(rx_addr e, uint32_t index);       /* 00479ef0 (ECX = entry; index; RET 4) -> the part entity */
void    rx_damage_split_parts(rx_addr e);                      /* 00479ce0 (ESI = entry) */
void    rx_damage_find_part_nodes(rx_addr e);                  /* 0047ae00 (stdcall entry; RET 4) */
void    rx_damage_bind_vehicle(rx_addr e, rx_addr vehicle);    /* 00479a00 (ECX = entry, EAX = vehicle) */
void    rx_damage_register_vehicle(rx_addr manager, rx_addr vehicle);   /* 0047bb90 (ESI = manager, EAX = vehicle) */
void    rx_damage_move_part_bodies(rx_addr e);                  /* 0047a460 (stdcall entry; RET 4) */
void    rx_damage_move_vehicle_parts(rx_addr manager, rx_addr vehicle);   /* 0047bc20 (EAX = manager; stdcall vehicle; RET 4) */
