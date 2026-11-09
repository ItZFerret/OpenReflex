// world_veh_transfer.cpp -- oracle adapters for core/game/world_veh_transfer.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_veh_transfer.h"
}
using namespace reflex_port;

RX_PORT(0x00769900, PhysicsWorldRemoveListener, "rx_world", 0) { rx_world_physics_remove_listener(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00769110, PhysicsWorldRemoveSimulation, "rx_world", 0) { rx_world_physics_remove_simulation(m.r[1], m.arg(0)); return m.ret(4); }
// 007c3400 (ECX = the tracks, EAX = the slot)
RX_PORT(0x007c3400, TracksFreeSlot, "rx_world", 0) { rx_tracks_free_slot(m.r[1], (int32_t)m.r[0]); return m.ret(0); }
// 0042b4a0 (ESI = dst, EDI = src) -> EAX = dst
RX_PORT(0x0042b4a0, DynRecordAssign, "rx_world", OUT_EAX) { m.r[0] = rx_dyn_record_assign(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x0042b300, DynRecordsMoveSlot, "rx_world", 0) { rx_dyn_records_move_slot(m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// 008825f0 (ECX = index, EDX = the grid)
RX_PORT(0x008825f0, GridSlotClear, "rx_world", 0) { rx_grid_slot_clear_fr(m.r[1], m.r[2], m.r[5], m.r[3], m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x0042af70, DynRecordsRemove, "rx_world", 0) { rx_dyn_records_remove(m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x0042a0d0, RagdollBodiesMove, "rx_world", 0) { rx_ragdoll_bodies_move(m.r[0], m.arg(0)); return m.ret(0); }
RX_PORT(0x0061d160, RiderWorldMove, "rx_world", 0) { rx_rider_world_move(m.r[0], m.arg(0)); return m.ret(4); }
RX_PORT(0x00464a00, VehicleWorldMove, "rx_world", 0) { rx_vehicle_world_move(m.r[1], m.arg(0)); return m.ret(4); }
