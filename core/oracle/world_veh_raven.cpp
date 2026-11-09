// world_veh_raven.cpp -- oracle adapters for core/game/world_veh_raven.c (group rx_world): the small vehicle virtuals.
#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/world_veh.h"
}
using namespace reflex_port;

// The vector getters (thiscall vehicle, stdcall out; RET 4; EAX = out).
RX_PORT(0x00430290, rtti_shared_vf_00430290, "rx_world", OUT_EAX) { return m.ret(0, rx_world_shared_vf_sub94(m.r[1])); }
RX_PORT(0x00430280, RavenVehicle_vf1_o94, "rx_world", OUT_EAX) { return m.ret(0, rx_world_vehicle_block_140(m.r[1])); }
RX_PORT(0x00430340, RavenVehicle_vf11_o94, "rx_world", OUT_EAX) { return m.ret(0, rx_world_vehicle_block_matrix(m.r[1])); }
RX_PORT(0x00462950, RavenVehicleMotocross_vf55, "rx_world", OUT_EAX) {
    const uint32_t sim = uint32_t(m.mem.read(m.r[1] + 0x16d4, 4));
    rx_world_raven_set_sim_type(m.r[1], m.arg(0));
    return m.ret(4, sim);
}
RX_PORT(0x004302a0, RavenVehicle_vf59, "rx_world", OUT_EAX) {
    const uint32_t blk = uint32_t(m.mem.read(m.r[1] + 0x1328, 4));
    rx_world_vehicle_set_block_flag(m.r[1], m.arg(0));
    return m.ret(4, blk);
}
RX_PORT(0x004669d0, RavenVehicleMotocross_vf41, "rx_world", OUT_EAX) { rx_world_raven_get_7a0(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
RX_PORT(0x004666e0, RavenVehicleMotocross_vf61, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_vf61(m.r[1])); }
// 0044aec0 (ECX = the simulation, EDX = its caller's EDX; RET 0; EAX and EDX are live: vf61 tail-jumps to it)
RX_PORT(0x0044aec0, RavenSimulationMotocross_frame, "rx_world", OUT_EAX | OUT_EDX) {
    uint32_t edx = 0;
    const uint32_t eax = rx_world_raven_frame(m.r[1], m.r[2], &edx);
    m.r[2] = edx;
    return m.ret(0, eax);
}
RX_PORT(0x004302c0, RavenVehicle_vf14_o94, "rx_world", 0) { rx_world_vehicle_o94_copy_nodes(m.r[1]); return m.ret(0); }
RX_PORT(0x00466920, RavenVehicleMotocross_vf36, "rx_world", OUT_EAX) { rx_world_raven_get_7c0(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
RX_PORT(0x00466940, RavenVehicleMotocross_vf37, "rx_world", OUT_EAX) { rx_world_raven_get_100(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
RX_PORT(0x00466960, RavenVehicleMotocross_vf38, "rx_world", OUT_EAX) { rx_world_raven_get_740(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
RX_PORT(0x004669b0, RavenVehicleMotocross_vf40, "rx_world", OUT_EAX) { rx_world_raven_get_160(m.r[1], m.arg(0)); return m.ret(4, m.arg(0)); }
// 00466980 (EAX = the simulation, EDX = out after).
RX_PORT(0x00466980, RavenVehicleMotocross_vf39, "rx_world", OUT_EAX | OUT_EDX) {
    rx_world_raven_get_8b60_plus_740(m.r[1], m.arg(0));
    m.r[2] = m.arg(0);
    return m.ret(4, m.mem.read(m.r[1] + 0x16d4, 4));
}
RX_PORT(0x00466a20, RavenVehicleMotocross_GetGear, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_get_gear(m.r[1])); }
RX_PORT(0x00466ae0, RavenVehicleMotocross_vf43, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_get_8d98(m.r[1])); }
RX_PORT(0x00466af0, RavenVehicleMotocross_vf44, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_get_8da0(m.r[1])); }
// 00466b70 (thiscall, stdcall which; RET 4; st(0) = the value; EAX = 0 for which 0, else the simulation).
RX_PORT(0x00466b70, RavenVehicleMotocross_vf56, "rx_world", OUT_EAX | OUT_FPU) {
    const uint32_t which = m.arg(0), sim = uint32_t(m.mem.read(m.r[1] + 0x16d4, 4));
    const float v = rx_world_raven_wheel_value(m.r[1], which, m.r[4] + 4);
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(v)));
    return m.ret(4, which == 0 ? 0 : sim);
}
RX_PORT(0x00466a30, RavenVehicleMotocross_vf58, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_state_code(m.r[1])); }
// 00467440 (thiscall, stdcall id; RET 4; EDX = id).
RX_PORT(0x00467440, RavenVehicleMotocross_vf53, "rx_world", OUT_EAX | OUT_EDX) {
    m.r[2] = m.arg(0);
    return m.ret(4, rx_world_raven_has_id(m.r[1], m.arg(0)));
}
RX_PORT(0x00467470, RavenVehicleATV_vf12, "rx_world", OUT_EAX) { return m.ret(0, rx_world_raven_atv_kind(m.r[1])); }
RX_PORT(0x00467490, RavenVehicleMotocross_vf54, "rx_world", OUT_EAX) { return m.ret(4, rx_world_raven_id_kind(m.r[1], m.arg(0))); }
// 0046a380 (EAX = vehicle, stdcall float; RET 4).
RX_PORT(0x0046a380, FUN_0046a380, "rx_world", 0) { rx_world_raven_sim_440da0(m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x0047bb40, FUN_0047bb40, "rx_world", OUT_EAX) { return m.ret(0, rx_world_damage_manager_instance()); }
// 0047bd10 (thiscall manager, stdcall world; RET 4).
RX_PORT(0x0047bd10, VehicleDamageBehaviorManager_vf0, "rx_world", 0) { rx_world_damage_manager_update(m.r[1], m.arg(0)); return m.ret(4); }
// 00466720 (thiscall, stdcall out; RET 4).
RX_PORT(0x00466720, RavenVehicleMotocross_vf30, "rx_world", 0) { rx_world_raven_wheels_mid(m.r[1], m.arg(0)); return m.ret(4); }
// 00466bd0 (thiscall, stdcall wheel; RET 4; st(0) = the _CIpow result).
RX_PORT(0x00466bd0, RavenVehicleMotocross_vf57, "rx_world", OUT_FPU) {
    const rx_f80 r = rx_world_raven_grip_factor(m.r[1], m.arg(0));
    reflex_f80::F80 v; v.kind = r.kind; v.sign = r.sign; v.mant = r.mant; v.exp = r.exp;
    m.fpu.push_back(v);
    return m.ret(4);
}
// 004647c0 vf60 (thiscall vehicle, stdcall unused; RET 4).
RX_PORT(0x004647c0, RavenVehicleMotocross_vf60, "rx_world", 0) { rx_world_raven_update_nodes(m.r[1]); return m.ret(4); }
// 004674e0 (stdcall vehicle; RET 4).
RX_PORT(0x004674e0, FUN_004674e0, "rx_world", 0) { rx_world_rider_pose(m.arg(0)); return m.ret(4); }
// 00476340 (stdcall block; RET 4). EDX is data dependent (the last integer scratch of the packed-quaternion decode): undeclared; x87 untouched.
RX_PORT(0x00476340, KeyTrackBlock_Sample, "rx_world", 0) {
    rx_world_key_track_block_sample(m.arg(0));
    return m.ret(4);
}
// 004636f0 vf0_o90 (thiscall vehicle, stdcall dt; RET 4).
RX_PORT(0x004636f0, RavenVehicleMotocross_vf0_o90, "rx_world", 0) {
    rx_world_raven_frame_update(m.r[1], rx_oracle::f32(m.arg(0)));
    return m.ret(4);
}
