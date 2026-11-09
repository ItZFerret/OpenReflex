/* phys_misc.h -- small helpers of the 0x0042xxxx range (phys_misc.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
float   phys_lag_filter_update(rx_addr f, float dt, float target);   /* 0042d6b0 (EAX = f, xmm2 = dt, cdecl target) */
void    phys_lag_filter_set(rx_addr f, float value, float tau);      /* 0042d670 (ECX = f, xmm1 = value, cdecl tau) */
void    phys_matrix6x6_construct(rx_addr m);                         /* 0042f1d0 (EAX) */
void    phys_matrix4x6_construct(rx_addr m);                         /* 0042f230 (EAX) */
void    phys_matrix6x4_construct(rx_addr m);                         /* 0042f2a0 (EAX) */
rx_addr phys_pose_clear(rx_addr p);                                  /* 0042e5f0 (ECX) */
void    phys_simulation_clock_advance(float dt);                     /* 0042ef70 SimulationClock_Advance (cdecl) */
float   phys_remap_clamped(float in_lo, float in_hi, float value, float out_lo, float out_hi);   /* 00424370 (xmm0, xmm1, cdecl x3) */
#ifdef __cplusplus
}
#endif
