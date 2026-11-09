// world_veh_entity.cpp -- oracle adapters for core/game/world_veh_entity.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_veh.h"
}
using namespace reflex_port;

// 004039a0 (thiscall entity, pos, q; RET 8; EAX = pos, ECX = 0 after rep movsd).
RX_PORT(0x004039a0, PhysicsEntity_vf30, "rx_world", OUT_EAX | OUT_ECX) {
    rx_world_physics_entity_write_back(m.r[1], m.arg(0), m.arg(1));
    m.r[1] = 0;
    return m.ret(8, m.arg(0));
}
RX_PORT(0x00403b90, PhysicsEntity_vf12, "rx_world", OUT_EAX) { return m.ret(0, rx_world_physics_entity_get148(m.r[1])); }
RX_PORT(0x00403ba0, rtti_shared_vf_00403ba0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_physics_entity_get14c(m.r[1])); }
RX_PORT(0x0041cce0, PhysicsEntity_vf22, "rx_world", OUT_EAX) { return m.ret(0, rx_world_physics_entity_type_id()); }
RX_PORT(0x00402090, Rainbow_EventHandler_vf6, "rx_world", OUT_EAX) { return m.ret(0, rx_world_event_handler_is_state1(m.r[1])); }
// 00418f30 (EAX = handle; EAX = body).
RX_PORT(0x00418f30, PhysicsHandle_Body, "rx_world", OUT_EAX) { return m.ret(0, rx_world_physics_handle_body(m.r[0])); }
// 0041cea0 (ESI = entity, EDI = out).
RX_PORT(0x0041cea0, FUN_0041cea0, "rx_world", 0) { rx_world_physics_entity_velocity(m.r[6], m.r[7]); return m.ret(0); }
// 0041cf30 (thiscall entity, stdcall out; RET 4).
RX_PORT(0x0041cf30, FUN_0041cf30, "rx_world", 0) { rx_world_physics_entity_speed(m.r[1], m.arg(0)); return m.ret(4); }
// 00402dc0 (EDX = list; EAX = the entry or 0).
RX_PORT(0x00402dc0, FUN_00402dc0, "rx_world", OUT_EAX) { return m.ret(0, rx_world_find_by_controller(m.r[2])); }
// 004020c0 (EAX = chain, EDI = name, stdcall a, b; RET 8; EAX = 0).
RX_PORT(0x004020c0, FUN_004020c0, "rx_world", OUT_EAX) {
    rx_world_event_dispatch(m.r[0], m.r[7], m.arg(0), m.arg(1));
    return m.ret(8, 0);
}

// Actual reset callers use memory publication; no register result is consumed.
RX_PORT(0x00430560, VehicleDamagePhysicsEntity_vf30, "rx_KinematicReset", 0) {
    rx_world_damage_physics_entity_write_back(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 00430670 VehicleDamagePhysicsEntity::vf31 (ECX = entity, stack position, quaternion; RET 8)
RX_PORT(0x00430670, VehicleDamagePhysicsEntity_vf31, "rx_world", 0) { rx_world_damage_physics_entity_get_pose(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// 0041cc60 PhysicsEntity::vf19 (ECX = entity, stack arg; RET 4)
RX_PORT(0x0041cc60, PhysicsEntity_vf19, "rx_world", 0) { rx_world_physics_entity_vf19(m.r[1], m.arg(0)); return m.ret(4); }
// 0041cbe0 PhysicsEntity::vf2 (ECX = entity)
RX_PORT(0x0041cbe0, PhysicsEntity_vf2, "rx_world", 0) { rx_world_physics_entity_disable(m.r[1]); return m.ret(0); }
// PhysicsEntity's destruction and the keyed body removal (w-partdtor): 00417c80 (ESI = dst, EDI = src -> EAX), 00417830 (stdcall vec,
// index, src; RET 0xc), 00416fe0 (ESI = vec; stdcall index; RET 4), 00402360 (stdcall world, EDI = index; RET 4), 00416f90 (ECX = handle),
// 0041c7b0 (ECX = entity), 00403bb0 PhysicsEntity::sdtor (thiscall; flags; RET 4 -> EAX = this).
RX_PORT(0x00417c80, KeyedRecord_Assign, "rx_world", OUT_EAX) { return m.ret(0, rx_world_keyed_record_assign(m.r[6], m.r[7])); }
RX_PORT(0x00417830, KeyedRecords_MoveSlot, "rx_world", 0) { rx_world_keyed_records_move_slot(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00416fe0, KeyedRecords_Remove, "rx_world", 0) { rx_world_keyed_records_remove(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00402360, KeyedBody_Remove, "rx_world", 0) { rx_world_keyed_body_remove(m.arg(0), m.r[7]); return m.ret(4); }
RX_PORT(0x00416f90, PhysicsObjectHandle_vf1, "rx_world", 0) { rx_physics_object_handle_reset(m.r[1]); return m.ret(0); }
RX_PORT(0x0041c7b0, PhysicsEntity_Destroy, "rx_world", 0) { rx_physics_entity_destroy(m.r[1]); return m.ret(0); }
RX_PORT(0x00403bb0, PhysicsEntity_Sdtor, "rx_world", OUT_EAX) { return m.ret(4, rx_physics_entity_sdtor(m.r[1], m.arg(0))); }
