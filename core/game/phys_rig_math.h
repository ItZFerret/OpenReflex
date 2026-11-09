/* phys_rig_math.h -- the rider rig's 3x3 matrix helpers (0x0041xxxx, world math outside the physics core), called as the
 * original calls them (registers, stack arguments below the caller's current stack pointer). Matrices: three vec4 rows. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void rig_m3_from_quat(rx_addr out, rx_addr q);               /* 0041f160 (ECX = out, stack q; RET 4) */
void rig_m3_skew(rx_addr out, rx_addr v);                    /* 0041f390 (ECX = v, stack out; RET 4) */
void rig_m3_mul(rx_addr out, rx_addr A, rx_addr B);          /* 0041f430 (EDX = out, ESI = A, ECX = B): out = A B */
void rig_m3_mul_vec(rx_addr out, rx_addr v, rx_addr M);      /* 0041f5c0 (ECX = out, EDX = v, cdecl M): out = M v */
void rig_m3_transpose(rx_addr dst, rx_addr src);             /* 0041f650 (ECX = src, cdecl dst) */
void rig_m3_inverse(rx_addr dst, rx_addr src);               /* 0041f6e0 (ECX = dst, cdecl src) */
void rig_block_inverse(rx_addr A, rx_addr B, rx_addr C, rx_addr D, rx_addr o11, rx_addr o12, rx_addr o21, rx_addr o22);   /* 0041f800 (cdecl) */
#ifdef __cplusplus
}
#endif
