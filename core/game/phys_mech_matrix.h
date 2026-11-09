/* phys_mech_matrix.h -- RavenPhysics matrix / vector objects of the mechanism solver (phys_mech_matrix.c).
 *
 * Object layout (every class): +0 vtable, +4 row count, +8 column count, rows of vec4 from +0x10 (stride 0x10, all four lanes
 * stored, W included). Block matrices (4x6, 6x4, 5x6 / 6x6 / 6x5) hold sub-matrix objects (each with its own vtable) at the
 * offsets named in each function and forward to their virtual methods. Virtual slots (byte offset = 4 * slot):
 *   1 clear   2 transpose into arg   3 copy from arg   4 arg0 = this - arg1   5 arg0 = arg1 + this   6 negate in place
 * Every function here is a thiscall (this first) returning EAX as the original leaves it. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Matrix3x3 */
uint32_t phys_matrix3x3_clear(rx_addr self);                              /* 00857c30 vf1 */
uint32_t phys_matrix3x3_transpose(rx_addr self, rx_addr out);             /* 00857c90 vf2 */
uint32_t phys_matrix3x3_copy(rx_addr self, rx_addr src);                  /* 00857bf0 vf3 (also Matrix3x4) */
uint32_t phys_matrix3x3_sub(rx_addr self, rx_addr out, rx_addr b);        /* 00858160 vf4 */
uint32_t phys_matrix3x3_add(rx_addr self, rx_addr out, rx_addr b);        /* 008581a0 vf5 */
uint32_t phys_matrix3x3_negate(rx_addr self);                             /* 008581e0 vf6 */
/* Matrix3x4 / Matrix4x3 / Matrix4x4 */
uint32_t phys_matrix3x4_transpose(rx_addr self, rx_addr out);             /* 00858220 vf2 */
uint32_t phys_matrix4x3_transpose(rx_addr self, rx_addr out);             /* 00858440 vf2 */
uint32_t phys_matrix4x3_sub(rx_addr self, rx_addr out, rx_addr b);        /* 00858660 vf4 (also Matrix4x4) */
uint32_t phys_matrix4x3_add(rx_addr self, rx_addr out, rx_addr b);        /* 008583c0 vf5 (also Matrix4x4) */
uint32_t phys_matrix4x4_clear(rx_addr self);                              /* 00858620 vf1 */
/* block matrices */
uint32_t phys_matrix4x6_transpose(rx_addr self, rx_addr out);             /* 00858a80 vf2 */
uint32_t phys_matrix6x4_transpose(rx_addr self, rx_addr out);             /* 00858bd0 vf2 */
uint32_t phys_matrix4x6_copy_from(rx_addr src, rx_addr dst);              /* 008588d0 (ECX = source, arg = destination) */
uint32_t phys_matrix4x6_copy(rx_addr self, rx_addr src);                  /* 008588c0 vf3 */
uint32_t phys_matrix5x6_clear(rx_addr self);                              /* 00858e80 vf1 (also 6x6 / 6x5) */
uint32_t phys_matrix5x6_transpose(rx_addr self, rx_addr out);             /* 00858f80 vf2 */
uint32_t phys_matrix5x6_sub(rx_addr self, rx_addr out, rx_addr b);        /* 00858c70 vf4 */
uint32_t phys_matrix5x6_copy(rx_addr self, rx_addr src);                  /* 00858d80 vf3 */
/* Vec6 / Vec3 */
uint32_t phys_vec6_clear(rx_addr self);                                   /* 0042f2e0 vf0 */
uint32_t phys_vec6_copy(rx_addr self, rx_addr src);                       /* 0042f310 vf1 */
uint32_t phys_vec3_copy(rx_addr self, rx_addr src);                       /* 0042f3b0 vf1 (also Vec4) */

/* A virtual call through the vtable of obj (slot index): the core function when the slot's address is registered, else the
 * original. Thiscall, 0..2 stack arguments, result = EAX. */
uint32_t phys_vcall0(rx_addr obj, uint32_t slot);
uint32_t phys_vcall1(rx_addr obj, uint32_t slot, uint32_t a);
uint32_t phys_vcall2(rx_addr obj, uint32_t slot, uint32_t a, uint32_t b);
#ifdef __cplusplus
}
#endif
