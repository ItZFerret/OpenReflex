/* phys_mech_kernel.h -- the matrix-vector / matrix-matrix kernels of the RavenPhysics mechanism solver (phys_mech_kernel.c).
 * Matrix objects as in phys_mech_matrix.h (+4 rows, +8 columns, rows of vec4 from +0x10); block matrices hold sub-matrices at
 * fixed offsets (3 x 3 blocks of a 6 x 6 at +0x10 / +0x50 / +0x90 / +0xd0, rows at +0x20 / +0x60 / +0xa0 / +0xe0; the two halves
 * of a 4 x 6 at +0x10 and +0x60; ...). Signatures list the original's register inputs in this order: ECX, EDX, then the other
 * registers, then the stack arguments. Every kernel returns nothing the callers read unless stated. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* matrix-vector products: out = A x (vec4 rows from +0x10; 6-vectors have their halves at +0x10 / +0x20) */
void phys_mat3_mulvec(rx_addr out, rx_addr x, rx_addr A);         /* 00854490 (ECX, EDX, arg) 3 x 3 (also 00854520) */
void phys_mat3x3_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 00854520 */
void phys_mat3x4_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 008545b0 */
void phys_mat4x3_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 00853ca0 */
void phys_mat4_mulvec(rx_addr out, rx_addr x, rx_addr A);         /* 00853bd0 */
void phys_mat3x6_mulvec(rx_addr x, rx_addr out, rx_addr A);       /* 00853d80 (ECX = x, EDX = out) */
void phys_mat4x6_mulvec(rx_addr x, rx_addr out, rx_addr A);       /* 00853a30 (ECX = x, EDX = out) */
void phys_mat5x6_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 008542c0 */
void phys_mat6x3_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 00853fa0 */
void phys_mat6x4_mulvec(rx_addr out, rx_addr x, rx_addr A);       /* 00853e80 */
void phys_mat6_mulvec(rx_addr out, rx_addr x, rx_addr A);         /* 008540a0 */
/* Y -= A x */
void phys_mat3x6_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A);     /* 00857650 */
void phys_mat4x6_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A);     /* 00857750 */
void phys_mat6x3_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A);     /* 008578f0 */
void phys_mat6x4_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A);     /* 00857a00 */
/* matrix-matrix products with a transposed operand */
void phys_mat3_mul_abt(rx_addr out, rx_addr B, rx_addr A);        /* 00854650 out = B * A^T (3 x 3) */
void phys_mat4x3_mul_mat3t(rx_addr out, rx_addr B, rx_addr A);    /* 00854ac0 */
void phys_mat3x4_mul_mat4t(rx_addr out, rx_addr B, rx_addr A);    /* 00854cb0 */
void phys_mat3x4_mul_mat3x4t(rx_addr out, rx_addr B, rx_addr A);  /* 00855590 */
void phys_mat4x3_mul_mat4x3t(rx_addr out, rx_addr B, rx_addr A);  /* 00855240 */
void phys_mat6_mul_abt(rx_addr A, rx_addr out, rx_addr B);        /* 00854f00 (ECX = A, ESI = out, arg B): out = A * B^T (6 x 6) */
void phys_mat3x6_mul_abt(rx_addr out, rx_addr B, rx_addr C);      /* 008547d0 */
void phys_mat4x6_mul_abt(rx_addr out, rx_addr B, rx_addr C);      /* 00854910 */
void phys_mat3x3_mul_abt_sum(rx_addr out, rx_addr L, rx_addr B);   /* 008550f0 (ESI = out, EDI = L, arg B) */
void phys_mat4x4_mul_abt_sum(rx_addr out, rx_addr B, rx_addr C);  /* 00855190 (ESI = out, EDI = B, arg C) */
void phys_mat3x4_mul_blocks(rx_addr B, rx_addr D, rx_addr L);     /* 00854a90 (EDI = B, ESI = D, arg L) */
void phys_mat3_mul_blocks(rx_addr B, rx_addr D, rx_addr L);       /* 00854a60 (EDI = B, ESI = D, arg L) */
void phys_mat6_mul_3x4t_blocks(rx_addr out, rx_addr L, rx_addr B);/* 00855090 (ESI = out, args L, B) */
void phys_mat6_mul_3t_blocks(rx_addr out, rx_addr L, rx_addr B);  /* 00855030 (ESI = out, args L, B) */
/* coupling products of the preparation (C := C-transform, B := its transpose) */
void phys_coupling3(rx_addr A, rx_addr B, rx_addr C);             /* 00857220 (ECX = A, EDX = B, arg C) */
void phys_coupling4(rx_addr B, rx_addr A, rx_addr C);             /* 00856b40 (ECX = B, EDX = A, arg C) */
void phys_coupling6x3(rx_addr A, rx_addr B, rx_addr C);           /* 00855810 */
void phys_coupling6x4(rx_addr A, rx_addr B, rx_addr C);           /* 00855ee0 */
/* inverses */
void phys_mat3_invert(rx_addr out, rx_addr in);                   /* 00859250 (ECX = out, arg = in) */
void phys_mat4_invert(rx_addr out, rx_addr in);                   /* 00859340 (ESI = out, EDI = in) */
void phys_mat5_invert(rx_addr out, rx_addr in);                   /* 00859520 (args out, in; modifies in) */
void phys_mat6_invert(rx_addr in, rx_addr out);                   /* 008597e0 (ECX = in, arg = out) */
/* constructors of block matrices (EAX / ESI = object) */
void phys_matrix3x6_construct(rx_addr m);                         /* 00853550 (EAX) */
void phys_matrix5x6_construct(rx_addr m);                         /* 008535b0 (ESI) */
void phys_matrix6x5_construct(rx_addr m);                         /* 00853640 (ESI) */
void phys_matrix6x3_construct(rx_addr m);                         /* 00853710 (EAX) */
/* rotation matrices, cross-product matrices, copy */
void phys_matrix3x3_from_quaternion(rx_addr out, rx_addr q);      /* 00857e50 (ECX = out, arg = q) */
void phys_matrix3x3_from_quaternion2(rx_addr out, rx_addr q);     /* 00857d00 */
void phys_matrix3x3_skew(rx_addr w, rx_addr out);                 /* 00857ff0 (ECX = w vec4, arg = out) */
void phys_matrix3x3_skew_t(rx_addr w, rx_addr out);               /* 00858090 */
rx_addr phys_matrix5x6_copy_from(rx_addr src, rx_addr dst);          /* 00858d90 (ECX = src, arg = dst) */
#ifdef __cplusplus
}
#endif
