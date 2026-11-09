/* loop_vehicle.h -- vehicle glue: RavenVehicle / RavenSimulationMotocross virtual slots, the vehicle's tuning and control-input
 * setters, the engine's ray-cast / dynamic-ray helpers (loop_vehicle.c). Notes: core/notes/loop.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_vehicle_root_bounds(rx_addr v, rx_addr a0, rx_addr a1, rx_addr a2);   /* 004302f0 RavenVehicle::vf6_o94 (ECX = v; stack a0, a1, a2; ret 0xc) */
rx_addr  rx_vehicle_matrix_ptr(rx_addr v);                                         /* 00430240 RavenVehicle::vf14 (ECX = v; result EAX = v + 0xf0) */
void     rx_vehicle_get_matrix(rx_addr v, rx_addr out);                            /* 00430220 RavenVehicle::vf15 (ECX = v; stack out; ret 4): 64 bytes from v + 0xf0 */
float    rx_sim_vf19(rx_addr sim);                                                 /* 004392e0 RavenSimulationMotocross::vf19 (ECX = sim; result st(0)) */
void     rx_sim_apply_tuning(rx_addr sim);                                         /* 0043be80 RavenSimulationMotocross::vf35 (ECX = sim) */
void     rx_vehicle_set_tuning(rx_addr v, rx_addr cur, rx_addr def);               /* 0045f270 (EDI = v, ESI = cur; stack def; ret 4) */
void     rx_vehicle_set_control_inputs(rx_addr v, rx_addr src);                    /* 0045f530 (ECX = v, EAX = src) */
void     rx_vehicle_release_controls(rx_addr v);                                   /* 0045f590 (EDI = v) */
void     rx_vehicle_set_body_flags(rx_addr v, uint32_t off);                       /* 004a3870 (ECX = v; stack off; ret 4) */
void     rx_vehicle_clear_handles(rx_addr obj);                                    /* 0042a1c0 (stack obj; ret 4) */
void     rx_vehicle_set_limits(rx_addr src, rx_addr dst, float f);                 /* 0042d8c0 (ECX = src, EDX = dst; stack f; ret 4) */
void     rx_rays_bounds(rx_addr rays, rx_addr min_out, rx_addr max_out);           /* 00845050 CollisionShapeDynamicRays::vf1 (ECX = rays; stack min, max; ret 8) */
void     rx_registry_remove(rx_addr self);                                         /* 00844170 (ECX = self) */
void     rx_vector_resize(rx_addr vec, uint32_t newsize);                          /* 008499d0 (EAX = newsize, EDI = vector) */
void     rx_physics_world_raycast_bodies(rx_addr world, rx_addr rays, int32_t n, rx_addr hits, uint32_t mask); /* 0085d360 (cdecl; ret) */
void     rx_collision_rays_vs_box(rx_addr rays, rx_addr box, rx_addr q_box, rx_addr t_box, rx_addr q_rays, uint32_t invert,
                                  rx_addr t_rays, rx_addr set);                       /* 0086a1e0 (ECX = rays, EDX = t_rays, EDI = set; stack box, q_box, t_box, q_rays, invert; ret 0x14) */
#ifdef __cplusplus
}
#endif
