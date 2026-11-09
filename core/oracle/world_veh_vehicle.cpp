// world_veh_vehicle.cpp -- oracle adapters for core/game/world_veh_vehicle.c (group rx_world).
#include "phys_oracle.h"
extern "C" {
#include "../game/world_veh.h"
}
using namespace reflex_port;
using namespace phys_oracle;

// 004603f0 (EAX = node, EDI = position out, stdcall quaternion out; RET 4).
RX_PORT(0x004603f0, FUN_004603f0, "rx_world", 0) { rx_world_node_world_pose(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
// 00461680 (EDX = point, EDI = owner, stdcall object; RET 4; xmm0 = (r, 0, 0, 0)).
RX_PORT(0x00461680, FUN_00461680, "rx_world", OUT_XMM0) {
    const float r = rx_world_terrain_grip(m.r[2], m.r[7], m.arg(0));
    rx_v4 v = {{r, 0.0f, 0.0f, 0.0f}};
    set_xmm(m, 0, v);
    return m.ret(4);
}
// 00461760 (EDX = point, EDI = owner; xmm0).
RX_PORT(0x00461760, FUN_00461760, "rx_world", OUT_XMM0) { set_xmm(m, 0, rx_world_terrain_layer1_factor(m.r[2], m.r[7])); return m.ret(0); }
// 00461220 (thiscall ctx, stdcall vehicle, wake block, dt; RET 0xc).
RX_PORT(0x00461220, FUN_00461220, "rx_world", 0) {
    rx_world_water_wake(m.r[1], m.arg(0), m.arg(1), argf(m, 2));
    return m.ret(0xc);
}
// 004617b0 (ESI = vehicle, xmm0 lane 0 = impact).
RX_PORT(0x004617b0, FUN_004617b0, "rx_world", 0) { rx_world_landing_rumble(m.r[6], xmm(m, 0).v[0]); return m.ret(0); }
// 004618e0 (ESI = vehicle, stdcall dt; RET 4).
RX_PORT(0x004618e0, FUN_004618e0, "rx_world", 0) { rx_world_vehicle_camera_update(m.r[6], argf(m, 0)); return m.ret(4); }
