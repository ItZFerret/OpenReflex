/* phys_ragdoll.h -- the rider ragdoll (phys_ragdoll.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t phys_ragdoll_joint_jacobians(rx_addr P, uint32_t edx_in);     /* 00421fa0 (stdcall P; RET 4); returns EDX */
uint32_t phys_ragdoll_external_forces(rx_addr P, float h);             /* 00422850 (EDI, xmm0); returns EDX */
uint32_t phys_ragdoll_node_inertia(rx_addr P, uint32_t edx_in);       /* 00422960 (stdcall P; RET 4); returns EDX */
void phys_ragdoll_solve_forces(rx_addr P);                             /* 004232b0 (ESI) */
void phys_ragdoll_step(rx_addr P, float dt);                           /* 004238a0 (ECX, xmm0) */
void phys_ragdoll_fix_positions(rx_addr P, uint32_t* ecx, uint32_t* edx);     /* 00423b80 (stdcall P; RET 4) */
void phys_ragdoll_fix_velocities(rx_addr P, uint32_t* ecx, uint32_t* edx);    /* 00423df0 (stdcall P; RET 4) */
#ifdef __cplusplus
}
#endif
