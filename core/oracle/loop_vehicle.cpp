// loop_vehicle.cpp -- oracle adapters for core/game/loop_vehicle.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/loop_vehicle.h"
}
using namespace reflex_port;
using rx_oracle::f32;

// ECX = v; stack a0, a1, a2; ret 0xc (callers read nothing: the original's EAX is the last callee's leftover)
RX_PORT(0x004302f0, RavenVehicle_vf6_o94, "rx_loop", OUT_FPU) { rx_vehicle_root_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
// ECX = v; EAX = v + 0xf0
RX_PORT(0x00430240, RavenVehicle_vf14, "rx_loop", OUT_EAX) { m.r[0] = rx_vehicle_matrix_ptr(m.r[1]); return m.ret(0); }
// ECX = v; stack out; ret 4
RX_PORT(0x00430220, RavenVehicle_vf15, "rx_loop", OUT_FPU) { rx_vehicle_get_matrix(m.r[1], m.arg(0)); return m.ret(4); }
// ECX = sim; result st(0)
RX_PORT(0x004392e0, RavenSimulationMotocross_vf19, "rx_loop", OUT_FPU) {
    m.fpu.push_back(reflex_f80::from_f32(rx_oracle::bits(rx_sim_vf19(m.r[1]))));
    return m.ret(0);
}
RX_PORT(0x0043be80, RavenSimulationMotocross_vf35, "rx_loop", OUT_FPU) { rx_sim_apply_tuning(m.r[1]); return m.ret(0); }
// EDI = v, ESI = cur; stack def; ret 4
RX_PORT(0x0045f270, RavenVehicle_SetTuning, "rx_loop", OUT_FPU) { rx_vehicle_set_tuning(m.r[7], m.r[6], m.arg(0)); return m.ret(4); }
// ECX = v, EAX = src
RX_PORT(0x0045f530, RavenVehicle_SetControlInputs, "rx_loop", OUT_FPU) { rx_vehicle_set_control_inputs(m.r[1], m.r[0]); return m.ret(0); }
// EDI = v
RX_PORT(0x0045f590, RavenVehicle_ReleaseControls, "rx_loop", OUT_FPU) { rx_vehicle_release_controls(m.r[7]); return m.ret(0); }
// ECX = v; stack on; ret 4
RX_PORT(0x004a3870, RavenVehicle_SetBodyFlags, "rx_loop", OUT_FPU) { rx_vehicle_set_body_flags(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0042a1c0, Vehicle_ClearHandles, "rx_loop", OUT_FPU) { rx_vehicle_clear_handles(m.arg(0)); return m.ret(4); }
// ECX = src, EDX = dst pointers; stack f; ret 4
RX_PORT(0x0042d8c0, Vehicle_SetLimits, "rx_loop", OUT_FPU) { rx_vehicle_set_limits(m.r[1], m.r[2], f32(m.arg(0))); return m.ret(4); }
// ECX = rays; stack min, max; ret 8
RX_PORT(0x00845050, DynamicRays_Bounds, "rx_loop", OUT_FPU) { rx_rays_bounds(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
// ECX = self
RX_PORT(0x00844170, Registry_Remove, "rx_loop", OUT_FPU) { rx_registry_remove(m.r[1]); return m.ret(0); }
// EAX = newsize, EDI = vector
RX_PORT(0x008499d0, Vector_Resize, "rx_loop", OUT_FPU) { rx_vector_resize(m.r[7], m.r[0]); return m.ret(0); }
// cdecl (world, rays, n, hits, mask); plain ret
RX_PORT(0x0085d360, PhysicsWorld_RayCastBodies, "rx_loop", OUT_FPU) {
    rx_physics_world_raycast_bodies(m.arg(0), m.arg(1), int32_t(m.arg(2)), m.arg(3), m.arg(4));
    return m.ret(0);
}
// LTCG: ECX = rays, EDX = rays frame translation, EDI = contact set; stack box, q_box, t_box, q_rays, invert; ret 0x14
RX_PORT(0x0086a1e0, Collision_RaysVsBox, "rx_loop", OUT_FPU) {
    rx_collision_rays_vs_box(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4) & 0xff, m.r[2], m.r[7]);
    return m.ret(0x14);
}
