/* phys_quat.h -- packed animation quaternions (phys_quat.c). */
#pragma once
#include "phys_vec.h"
#include "phys_crt.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_quat_slerp_packed(rx_addr a, rx_addr b, rx_addr out, float t);   /* 0042ce40 (EAX = a, ESI = b, EDI = out, cdecl t) */
/* one packed quaternion (8 bytes) -> (x, y, z, w), w >= 0 (inlined by the samplers 00840440 / 00840b70) */
rx_v4 phys_quat_unpack(rx_addr packed);
/* acos(z) as the exe's inlined double polynomial (also used by 007c2ab0) */
float phys_acos_poly(float z);
void phys_quat_slerp_weights(float dot, float t, float* wa, float* wb);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
/* 0042db20 (ECX = base*, EDX = up*, stack: probe, target*, q*, r*; RET 0x10) */
void phys_probe_frame_setup(rx_addr base, rx_addr up, rx_addr probe, rx_addr target, rx_addr q, rx_addr r);
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
int32_t phys_quat_within_cone_limits(rx_addr L, rx_addr qb, rx_addr qa);   /* 0042de70 (ESI = limits, ECX = q_b*; q_a*) */
void phys_pose_blend(rx_addr pa, rx_addr out, rx_addr pb, int32_t count, float t);   /* 00841c90 (ECX = a, EDX = out; b, count, t) */
#ifdef __cplusplus
}
#endif
