// world_veh_motocross.cpp -- oracle adapters for core/game/world_veh_motocross.c (group rx_world).
#include "oracle.h"
extern "C" {
#include "../game/world_veh.h"
#include "../game/vehicle_saved_pose.h"
}
using namespace reflex_port;
using rx_oracle::f32;

RX_PORT(0x00462d40, RavenVehicleMotocross_vf62, "rx_world", 0) { rx_world_motocross_vf62(m.r[1]); return m.ret(0); }
RX_PORT(0x00463900, RavenVehicleMotocross_vf29, "rx_world", 0) { rx_world_motocross_vf29(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00464e80, RavenVehicleMotocross_vf35, "rx_world", 0) { rx_world_motocross_vf35(m.r[1], f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0041e080, FUN_0041e080, "rx_world", 0) { rx_world_spline_nearest_angles(m.r[6], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00468300, FUN_00468300, "rx_world", 0) { rx_world_vehicle_wheels(m.arg(0), f32(m.arg(1))); return m.ret(8); }
// 00460070 (EAX = vehicle; plain RET): the divergence reset, placed at the saved pose (core/game/vehicle_saved_pose.c)
RX_PORT(0x00460070, FUN_00460070, "rx_world", 0) { rx_vehicle_restore_saved_pose(m.r[0]); return m.ret(0); }
