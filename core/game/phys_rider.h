/* phys_rider.h -- the rider animation's crash physics (phys_rider.c): the ragdoll's driver at the rider-animation level (A = the
 * rider animation; its ragdoll rig P = A + 0x380, phys_ragdoll.c / phys_rigbody.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_rider_clear_bone_flags(rx_addr bones);                                  /* 0042a240 (stdcall; RET 4) */
uint32_t phys_rider_up_facing(rx_addr A, uint32_t* ecx);                          /* 0042aca0 (stdcall; RET 4); EAX */
uint32_t phys_rider_rest_timer(rx_addr A, float dt);                              /* 00425800 (stdcall; RET 8); returns EDX */
void phys_rider_pose_nodes(rx_addr A, const rx_regs* caller, rx_regs* out);       /* 00426930 (ECX) */
void phys_rider_clamp_speed(rx_addr A, uint32_t* ecx, uint32_t* edx);           /* 00425920 (stdcall; RET 4) */
void phys_rider_crash_sound(rx_addr A, rx_regs* r);                               /* 004252a0 (stdcall; RET 4) */
uint32_t phys_rider_wake_if_clear(rx_addr A, const rx_regs* caller);             /* 0042a310 (ECX); returns EDX */
void phys_rider_swing_twist(rx_addr q, rx_addr out_x, rx_addr out_y, rx_addr out_twist, rx_addr out_swing);   /* 00420160 (stdcall q; ECX EDX ESI EDI) */
void phys_rider_joint_limits(rx_addr P, const rx_regs* caller);                  /* 00420770 (stdcall P; RET 4) */
void phys_rider_contact_impulse(uint32_t i, rx_addr p, rx_addr A, rx_addr n, rx_addr depth);   /* 004263a0 (ECX, EDX; stdcall A, n, depth) */
void phys_rider_ground_contacts(rx_addr A, float dt, const rx_regs* caller);      /* 00425b20 (stdcall A, dt; RET 8) */
void phys_rider_bone_contacts(rx_addr A, float dt);                                /* 00429a40 (stdcall A, dt; RET 8) */
void phys_rider_water(rx_addr A, const rx_regs* caller);                         /* 0042a6c0 (stdcall A; RET 4) */
uint32_t phys_rider_crash_step(rx_addr A, float dt, const rx_regs* caller);       /* 00425640 (stdcall A, dt; RET 8); ECX */
uint32_t phys_rider_crash_update(rx_addr A, float dt, const rx_regs* caller);     /* 00425600 (ESI A; stdcall dt; RET 4); ECX */
#ifdef __cplusplus
}
#endif
