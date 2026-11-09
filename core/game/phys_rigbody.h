/* phys_rigbody.h -- the rider rig's rigid bodies (phys_rigbody.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_rigbody_save_velocities(rx_addr P);                                     /* 00422550 (EDI) */
void phys_rigbody_restore_velocities(rx_addr P);                                  /* 00422590 (EDI) */
void phys_rigbody_clear_forces(rx_addr P);                                        /* 00422800 (ESI) */
void phys_rigbody_center_of_mass(rx_addr P, rx_addr out);                         /* 00422300 (ESI, ECX) */
void phys_rigbody_mean_velocity(rx_addr P, rx_addr out);                          /* 00422350 (ESI, ECX) */
void phys_rigbody_axis_inertia(rx_addr P, rx_addr axis, rx_addr out, rx_addr point);   /* 004223a0 (ESI, EDI; out, point) */
void phys_rigbody_apply_impulse(rx_addr P, int32_t body, rx_addr impulse, rx_addr lever);   /* 00422180 (EDX, ESI, EDI; lever) */
void phys_rigbody_add_torque(rx_addr P, int32_t body, rx_addr torque);            /* 00422700 (ECX; body, torque) */
void phys_rigbody_add_angular_velocity(rx_addr v, rx_addr P, int32_t body);       /* 004241f0 (ECX, EDX; body) */
void phys_rigbody_reference_point(rx_addr out, rx_addr P);                        /* 004225d0 (EDX; P) */
void phys_rigbody_damp(rx_addr P, float dt);                                      /* 00423980 (P; xmm0) */
void phys_rigbody_random_spin(rx_addr P, int32_t body, float scale);              /* 004240e0 (ECX, ESI; scale) */
void phys_rigbody_add_world_angular_velocity(rx_addr P, rx_addr v);              /* 00422450 (EDX, ESI) */
void phys_rigbody_integrate(rx_addr P, float dt);                                 /* 00423a50 (ESI, xmm0) */
#ifdef __cplusplus
}
#endif
