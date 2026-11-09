// world_veh_damage.cpp -- oracle adapters for core/game/world_veh_damage.c (group rx_world). fastcall ECX -> EAX = ECX.
#include "oracle.h"
extern "C" {
#include "../game/world_veh_damage.h"
#include "../game/build_model.h"
}
using namespace reflex_port;

RX_PORT(0x00844100, CollisionShapeHandleConstruct, "rx_world", OUT_EAX) { m.r[0] = rx_collision_shape_handle_construct(m.r[1]); return m.ret(0); }
RX_PORT(0x004798e0, DamageBehaviorConstruct, "rx_world", OUT_EAX) { m.r[0] = rx_damage_behavior_construct(m.r[1]); return m.ret(0); }

// world_veh_damage_hit.c
// 006971f0 (ESI = effect record, EAX = quaternion*, ECX = offset vector*; stack position*, strength^2; RET 8)
RX_PORT(0x006971f0, EffectRecordStart, "rx_world", 0) { rx_world_effect_record_start(m.r[6], m.r[0], m.r[1], m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
// 00697b10 (stdcall set, id, position*, quaternion*, offset*, strength^2; RET 0x18)
RX_PORT(0x00697b10, EffectSetStart, "rx_world", 0) { rx_world_effect_set_start(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), rx_oracle::f32(m.arg(5))); return m.ret(0x18); }
// 00691c30 (cdecl vehicle, id, position*, quaternion*, offset*, strength)
RX_PORT(0x00691c30, VehicleEffectsEmit, "rx_world", 0) { rx_world_vehicle_effects_emit(m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), rx_oracle::f32(m.arg(5))); return m.ret(0); }
// 0047ba80 (ECX = behaviour, EAX = part index, xmm0 = strength; plain RET; EAX = 0)
RX_PORT(0x0047ba80, DamageHitPoints, "rx_world", OUT_EAX) { rx_world_damage_hit_points(m.r[1], m.r[0], rx_oracle::f32(uint32_t(m.xmm[0].lo))); m.r[0] = 0; return m.ret(0); }
// 0047a970 (ECX = contact event; stack behaviour; RET 4)
RX_PORT(0x0047a970, DamageApplyContact, "rx_world", 0) { rx_world_damage_apply_contact(m.r[1], m.arg(0)); return m.ret(4); }
// 0047a880 (stdcall behaviour; RET 4)
RX_PORT(0x0047a880, DamageResetParts, "rx_world", 0) { rx_world_damage_reset_parts(m.arg(0)); return m.ret(4); }

// world_veh_damage_spring.c
// 0047aff0 (stdcall entry, part index, dt; RET 0xc)
RX_PORT(0x0047aff0, DamagePartSpring, "rx_world", 0) { rx_world_damage_part_spring(m.arg(0), m.arg(1), rx_oracle::f32(m.arg(2))); return m.ret(0xc); }
// 00681750 (ESI = the vehicle; plain RET)
RX_PORT(0x00681750, VehicleFoleyEndStop, "rx_world", 0) { rx_vehicle_foley_end_stop(m.r[6]); return m.ret(0); }

// world_veh_damage_register.c
// 00479680 (ECX = out handle, EDX = offset vector, stack description; the caller pops it) -> EAX = out
RX_PORT(0x00479680, DamageParsePartShape, "rx_world", OUT_EAX) { m.r[0] = rx_damage_parse_part_shape(m.r[1], m.r[2], m.arg(0)); return m.ret(0); }
// 00847c80 (stack shape, n; RET 8) -> EAX = shape
RX_PORT(0x00847c80, ShapeCompositeConstruct, "rx_world", OUT_EAX) { m.r[0] = rx_shape_composite_construct(m.arg(0), m.arg(1)); return m.ret(8); }
// 00849080 (EDI = shape, stack n; RET 4) -> EAX = shape
RX_PORT(0x00849080, ShapeDynamicCompositeConstruct, "rx_world", OUT_EAX) { m.r[0] = rx_shape_dynamic_composite_construct(m.r[7], m.arg(0)); return m.ret(4); }

// build_model.c: the node set of a subtree (007dda20 and helpers)
// 007dda20 (ESI = source set, stack destination, name; RET 8)
RX_PORT(0x007dda20, NodeSetExtractSubtree, "rx_world", 0) { rx_nodeset_extract_subtree(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
// 007ddfa0 (ECX = source set, stack destination, map; RET 8)
RX_PORT(0x007ddfa0, NodeSetExtract, "rx_world", 0) { rx_nodeset_extract(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 007dd890 (ESI = set)
RX_PORT(0x007dd890, NodeSetRelease, "rx_world", 0) { rx_nodeset_reset(m.r[6]); return m.ret(0); }
// 007dea70 (ECX = set, EDI = first record)
RX_PORT(0x007dea70, NodeSetWalkFrom, "rx_world", 0) { rx_nodeset_walk_from(m.r[1], m.r[7]); return m.ret(0); }
// 007697e0 (ECX = entity; stack world, out, handle, soft; RET 0x10) -> EAX = out
RX_PORT(0x007697e0, PhysObjectCreateFromHandle, "rx_world", OUT_EAX) { m.r[0] = rx_phys_object_create_from_handle(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3) & 0xff); return m.ret(0x10); }
// 0041cd90 (ECX = handle; stack entity, world; RET 8)
RX_PORT(0x0041cd90, PhysEntityAttachBody, "rx_world", 0) { rx_physics_entity_attach_body(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0041cb60 (ESI = entity, ECX = handle; stack type, name; RET 8)
RX_PORT(0x0041cb60, VehicleDamageEntitySetup, "rx_world", 0) { rx_vehicle_damage_entity_setup(m.r[6], m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 00479ef0 (ECX = entry; stack index; RET 4) -> EAX = the part entity
RX_PORT(0x00479ef0, DamageSplitPart, "rx_world", OUT_EAX) { m.r[0] = rx_damage_split_part(m.r[1], m.arg(0)); return m.ret(4); }
// 00479ce0 (ESI = entry)
RX_PORT(0x00479ce0, DamageSplitParts, "rx_world", 0) { rx_damage_split_parts(m.r[6]); return m.ret(0); }
// 0047ae00 (stdcall entry; RET 4)
RX_PORT(0x0047ae00, DamageFindPartNodes, "rx_world", 0) { rx_damage_find_part_nodes(m.arg(0)); return m.ret(4); }
// 00479a00 (ECX = entry, EAX = vehicle)
RX_PORT(0x00479a00, DamageBindVehicle, "rx_world", 0) { rx_damage_bind_vehicle(m.r[1], m.r[0]); return m.ret(0); }
// 0047bb90 (ESI = manager, EAX = vehicle)
RX_PORT(0x0047bb90, DamageRegisterVehicle, "rx_world", 0) { rx_damage_register_vehicle(m.r[6], m.r[0]); return m.ret(0); }
// 0047a460 (stdcall entry; RET 4) / 0047bc20 (EAX = manager; stdcall vehicle; RET 4): the part bodies follow the vehicle into another world (w-partdtor)
RX_PORT(0x0047a460, DamageMovePartBodies, "rx_world", 0) { rx_damage_move_part_bodies(m.arg(0)); return m.ret(4); }
RX_PORT(0x0047bc20, DamageMoveVehicleParts, "rx_world", 0) { rx_damage_move_vehicle_parts(m.r[0], m.arg(0)); return m.ret(4); }
