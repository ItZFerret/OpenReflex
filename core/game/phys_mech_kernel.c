/* phys_mech_kernel.c -- the matrix kernels of the RavenPhysics mechanism solver: matrix-vector products, products with a
 * transposed operand, the coupling products of the preparation, inverses, and the small rotation / cross-product matrices.
 * They use the exe's SSE evaluation orders: a row dot product of three elements is (p0 + p1) + p2 (phys_dot3), of four elements
 * (p1 + p3) + (p0 + p2) (phys_dot4); products are written matrix element first unless the original has the vector first.
 * Packed W lanes are stored as the original packs them (documented per function). */
#include "phys_mech.h"
#include "phys_mech_kernel.h"
#include "phys_mech_matrix.h"
#include "phys_vec.h"
#include "phys_misc.h"
#ifdef RX_CONTACT_FRAME_OBSERVERS
#include "contact_pose_frames.h"
#endif

#define VT_MATRIX3X3 0x0093ca04u
#define G_ZERO 0x00d61670u                 /* runtime vec4 (0, 0, 0, 0) the kernels clear with */
#define NUMERATOR 0x00d61650u              /* runtime vec4 the inverses divide: 1 / det */

/* ---- helpers ---- */
/* row i (0-based) of a matrix object whose rows start at +0x10 */
static rx_addr row_of(rx_addr m, uint32_t i) { return m + 0x10u + 0x10u * i; }
/* a 3-element dot of a stored row with a vector: row * x */
static float d3(rx_addr row, rx_v4 x) { return phys_dot3(v4_ld(row), x); }
static float d4(rx_addr row, rx_v4 x) { return phys_dot4(v4_ld(row), x); }
/* the same with the vector first: x * row */
static float d3x(rx_v4 x, rx_addr row) { return phys_dot3(x, v4_ld(row)); }
static float d4x(rx_v4 x, rx_addr row) { return phys_dot4(x, v4_ld(row)); }
/* a row of a 6-wide matrix against both halves of a 6-vector: top block row . x0 + bottom block row . x1 */
static float d6(rx_addr top, rx_addr bot, rx_v4 x0, rx_v4 x1) { return phys_add(d3(top, x0), d3(bot, x1)); }
/* three results packed with the 4th lane = 3rd / 1st (the original's shuffles) */
static rx_v4 pack3(float a, float b, float c) { return v4(a, b, c, c); }
static rx_v4 pack3w0(float a, float b, float c) { return v4(a, b, c, a); }

/* ---- matrix-vector products ---- */

/* 00854490 (ECX = out vec4, EDX = x vec4, arg = A 3 x 3): out = (A0 . x, A1 . x, A2 . x, A2 . x); x is read before out is written
 * (they may be the same vector). */
void phys_mat3_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x);
    float d0 = d3(row_of(A, 0), xv), d1 = d3(row_of(A, 1), xv), d2 = d3(row_of(A, 2), xv);
    v4_st(out, pack3(d0, d1, d2));
}
RX_FUNCTION(0x00854490, phys_mat3_mulvec)

/* 00854520 (ECX = out vec4, EDX = x vec4 (+0x10 here: vector objects), arg = A 3 x 3): the same product on vector objects, whose
 * data starts at +0x10. */
void phys_mat3x3_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float d0 = d3(row_of(A, 0), xv), d1 = d3(row_of(A, 1), xv), d2 = d3(row_of(A, 2), xv);
    v4_st(out + 0x10, pack3(d0, d1, d2));
}
RX_FUNCTION(0x00854520, phys_mat3x3_mulvec)

/* 008545b0 3 x 4 (vector objects): out = (A0 . x, A1 . x, A2 . x, A0 . x) with 4-element dots. */
void phys_mat3x4_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float d0 = d4(row_of(A, 0), xv), d1 = d4(row_of(A, 1), xv), d2 = d4(row_of(A, 2), xv);
    v4_st(out + 0x10, pack3w0(d0, d1, d2));
}
RX_FUNCTION(0x008545b0, phys_mat3x4_mulvec)

/* 00853ca0 4 x 3: out = (A0 . x, A1 . x, A2 . x, A3 . x), 3-element dots. */
void phys_mat4x3_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float d0 = d3(row_of(A, 0), xv), d1 = d3(row_of(A, 1), xv), d2 = d3(row_of(A, 2), xv), d3_ = d3(row_of(A, 3), xv);
    v4_st(out + 0x10, v4(d0, d1, d2, d3_));
}
RX_FUNCTION(0x00853ca0, phys_mat4x3_mulvec)

/* 00853bd0 4 x 4: out = (A0 . x, ..., A3 . x), 4-element dots. */
void phys_mat4_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float d0 = d4(row_of(A, 0), xv), d1 = d4(row_of(A, 1), xv), d2 = d4(row_of(A, 2), xv), d3_ = d4(row_of(A, 3), xv);
    v4_st(out + 0x10, v4(d0, d1, d2, d3_));
}
RX_FUNCTION(0x00853bd0, phys_mat4_mulvec)

/* 00853d80 3 x 6 (ECX = x: Vec6, EDX = out): out_i = A_top_i . x0 + A_bottom_i . x1 (top rows +0x20.., bottom rows +0x60..). */
void phys_mat3x6_mulvec(rx_addr x, rx_addr out, rx_addr A) {
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float d[3];
    for (uint32_t i = 0; i < 3; i++) d[i] = d6(A + 0x20 + 0x10 * i, A + 0x60 + 0x10 * i, x0, x1);
    v4_st(out + 0x10, pack3(d[0], d[1], d[2]));
}
RX_FUNCTION(0x00853d80, phys_mat3x6_mulvec)

/* 00853a30 4 x 6 (ECX = x: Vec6, EDX = out): top rows +0x20.., bottom rows +0x70..; out = (d0, d1, d2, d3). */
void phys_mat4x6_mulvec(rx_addr x, rx_addr out, rx_addr A) {
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float d[4];
    for (uint32_t i = 0; i < 4; i++) d[i] = d6(A + 0x20 + 0x10 * i, A + 0x70 + 0x10 * i, x0, x1);
    v4_st(out + 0x10, v4(d[0], d[1], d[2], d[3]));
}
RX_FUNCTION(0x00853a30, phys_mat4x6_mulvec)

/* 008542c0 5 x 6 (ECX = out: Vec6, EDX = x: Vec6): rows 0..2 from the 3 x 3 blocks +0x20 / +0x60, rows 3..4 from the first two rows
 * of +0xa0 / +0xe0; out+0x10 = (d0, d1, d2, d2), out+0x20 = (d3, d4, g, g) with g lane 2 of the runtime vec4 00d61670 (read first). */
void phys_mat5x6_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    float g = F32(0, G_ZERO + 8);
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float d[5];
    for (uint32_t i = 0; i < 3; i++) d[i] = d6(A + 0x20 + 0x10 * i, A + 0x60 + 0x10 * i, x0, x1);
    for (uint32_t i = 0; i < 2; i++) d[3 + i] = d6(A + 0xa0 + 0x10 * i, A + 0xe0 + 0x10 * i, x0, x1);
    v4_st(out + 0x10, pack3(d[0], d[1], d[2]));
    v4_st(out + 0x20, v4(d[3], d[4], g, g));
}
RX_FUNCTION(0x008542c0, phys_mat5x6_mulvec)

/* 00853fa0 6 x 3: A = two 3 x 3 blocks (rows +0x20.., +0x60..); out+0x10 = (t0, t1, t2, t2), out+0x20 = (b0, b1, b2, b2). */
void phys_mat6x3_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float t[3], b[3];
    for (uint32_t i = 0; i < 3; i++) { t[i] = d3(A + 0x20 + 0x10 * i, xv); b[i] = d3(A + 0x60 + 0x10 * i, xv); }
    v4_st(out + 0x10, pack3(t[0], t[1], t[2]));
    v4_st(out + 0x20, pack3(b[0], b[1], b[2]));
}
RX_FUNCTION(0x00853fa0, phys_mat6x3_mulvec)

/* 00853e80 6 x 4: A = two 3 x 4 blocks (rows +0x20.., +0x60..), 4-element dots; out+0x10 = (t0, t1, t2, t0), out+0x20 =
 * (b0, b1, b2, b0). */
void phys_mat6x4_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float t[3], b[3];
    for (uint32_t i = 0; i < 3; i++) { t[i] = d4(A + 0x20 + 0x10 * i, xv); b[i] = d4(A + 0x60 + 0x10 * i, xv); }
    v4_st(out + 0x10, pack3w0(t[0], t[1], t[2]));
    v4_st(out + 0x20, pack3w0(b[0], b[1], b[2]));
}
RX_FUNCTION(0x00853e80, phys_mat6x4_mulvec)

/* 008540a0 6 x 6 (blocks +0x20 / +0x60 over +0xa0 / +0xe0): out+0x10 = (d0, d1, d2, d2) with d_i = A00_i . x0 + A01_i . x1;
 * out+0x20 likewise from A10 / A11. */
void phys_mat6_mulvec(rx_addr out, rx_addr x, rx_addr A) {
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float t[3], b[3];
    for (uint32_t i = 0; i < 3; i++) {
        t[i] = d6(A + 0x20 + 0x10 * i, A + 0x60 + 0x10 * i, x0, x1);
        b[i] = d6(A + 0xa0 + 0x10 * i, A + 0xe0 + 0x10 * i, x0, x1);
    }
    v4_st(out + 0x10, pack3(t[0], t[1], t[2]));
    v4_st(out + 0x20, pack3(b[0], b[1], b[2]));
}
RX_FUNCTION(0x008540a0, phys_mat6_mulvec)

/* ---- Y -= A x ---- */

/* 00857650 3 x 6: Y (+0x10) -= (d0, d1, d2, d2), d_i = A_top_i . x0 + A_bottom_i . x1 (rows +0x20.. / +0x60..). */
void phys_mat3x6_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A) {
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float d[3];
    for (uint32_t i = 0; i < 3; i++) d[i] = d6(A + 0x20 + 0x10 * i, A + 0x60 + 0x10 * i, x0, x1);
    v4_st(Y + 0x10, v4_sub(v4_ld(Y + 0x10), pack3(d[0], d[1], d[2])));
}
RX_FUNCTION(0x00857650, phys_mat3x6_mulvec_sub)

/* 00857750 4 x 6 (rows +0x20.. / +0x70..): Y (+0x10) -= (d0, d1, d2, d3). */
void phys_mat4x6_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A) {
    rx_v4 x0 = v4_ld(x + 0x10), x1 = v4_ld(x + 0x20);
    float d[4];
    for (uint32_t i = 0; i < 4; i++) d[i] = d6(A + 0x20 + 0x10 * i, A + 0x70 + 0x10 * i, x0, x1);
    v4_st(Y + 0x10, v4_sub(v4_ld(Y + 0x10), v4(d[0], d[1], d[2], d[3])));
}
RX_FUNCTION(0x00857750, phys_mat4x6_mulvec_sub)

/* 008578f0 6 x 3: Y+0x10 -= (t0, t1, t2, t2), Y+0x20 -= (b0, b1, b2, b2). */
void phys_mat6x3_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float t[3], b[3];
    for (uint32_t i = 0; i < 3; i++) { t[i] = d3(A + 0x20 + 0x10 * i, xv); b[i] = d3(A + 0x60 + 0x10 * i, xv); }
    v4_st(Y + 0x10, v4_sub(v4_ld(Y + 0x10), pack3(t[0], t[1], t[2])));
    v4_st(Y + 0x20, v4_sub(v4_ld(Y + 0x20), pack3(b[0], b[1], b[2])));
}
RX_FUNCTION(0x008578f0, phys_mat6x3_mulvec_sub)

/* 00857a00 6 x 4: Y+0x10 -= (t0, t1, t2, t0), Y+0x20 -= (b0, b1, b2, b0), 4-element dots. */
void phys_mat6x4_mulvec_sub(rx_addr Y, rx_addr x, rx_addr A) {
    rx_v4 xv = v4_ld(x + 0x10);
    float t[3], b[3];
    for (uint32_t i = 0; i < 3; i++) { t[i] = d4(A + 0x20 + 0x10 * i, xv); b[i] = d4(A + 0x60 + 0x10 * i, xv); }
    v4_st(Y + 0x10, v4_sub(v4_ld(Y + 0x10), pack3w0(t[0], t[1], t[2])));
    v4_st(Y + 0x20, v4_sub(v4_ld(Y + 0x20), pack3w0(b[0], b[1], b[2])));
}
RX_FUNCTION(0x00857a00, phys_mat6x4_mulvec_sub)

/* ---- products with a transposed operand: out_j = (A_0 . B_j, A_1 . B_j, ...) ---- */

/* 00854650 Mat3_MulABt (ECX = out, EDX = B, arg = A, all 3 x 3): out row j = (A0 . Bj, A1 . Bj, A2 . Bj, A2 . Bj), each row
 * stored before the next is loaded. */
void phys_mat3_mul_abt(rx_addr out, rx_addr B, rx_addr A) {
    for (uint32_t j = 0; j < 3; j++) {
        rx_v4 bj = v4_ld(row_of(B, j));
        float d0 = d3(row_of(A, 0), bj), d1 = d3(row_of(A, 1), bj), d2 = d3(row_of(A, 2), bj);
        v4_st(row_of(out, j), pack3(d0, d1, d2));
    }
}
RX_FUNCTION(0x00854650, phys_mat3_mul_abt)

/* 00854ac0 Mat4x3_MulMat3t (ECX = out 4 x 3, EDX = B 4 x 3, arg = A 3 x 3): out row j = (A0 . Bj, A1 . Bj, A2 . Bj, A2 . Bj) for
 * j < 3; the fourth row multiplies the other way round (Bj . Ai). */
void phys_mat4x3_mul_mat3t(rx_addr out, rx_addr B, rx_addr A) {
    for (uint32_t j = 0; j < 4; j++) {
        rx_v4 bj = v4_ld(row_of(B, j));
        float d[3];
        for (uint32_t i = 0; i < 3; i++) d[i] = j < 3 ? d3(row_of(A, i), bj) : d3x(bj, row_of(A, i));
        v4_st(row_of(out, j), pack3(d[0], d[1], d[2]));
    }
}
RX_FUNCTION(0x00854ac0, phys_mat4x3_mul_mat3t)

/* 00854cb0 Mat3x4_MulMat4t (ECX = out 3 x 4, EDX = B 3 x 4, arg = A 4 x 4): out row j lane i = B_j . A_i (4-element dots); the
 * operand order is B first for row 0 and for lane 0 of row 1, A first for the rest. */
void phys_mat3x4_mul_mat4t(rx_addr out, rx_addr B, rx_addr A) {
    for (uint32_t j = 0; j < 3; j++) {
        rx_v4 bj = v4_ld(row_of(B, j));
        float d[4];
        for (uint32_t i = 0; i < 4; i++) {
            int b_first = j == 0 || (j == 1 && i == 0);
            d[i] = b_first ? d4x(bj, row_of(A, i)) : d4(row_of(A, i), bj);
        }
        v4_st(row_of(out, j), v4(d[0], d[1], d[2], d[3]));
    }
}
RX_FUNCTION(0x00854cb0, phys_mat3x4_mul_mat4t)

/* 00855590 Mat3x4_MulMat3x4t (ECX = out 3 x 3, EDX = B 3 x 4, arg = A 3 x 4): out row j = (A0 . Bj, A1 . Bj, A2 . Bj, A0 . Bj) with
 * 4-element dots. */
void phys_mat3x4_mul_mat3x4t(rx_addr out, rx_addr B, rx_addr A) {
    for (uint32_t j = 0; j < 3; j++) {
        rx_v4 bj = v4_ld(row_of(B, j));
        float d0 = d4(row_of(A, 0), bj), d1 = d4(row_of(A, 1), bj), d2 = d4(row_of(A, 2), bj);
        v4_st(row_of(out, j), pack3w0(d0, d1, d2));
    }
}
RX_FUNCTION(0x00855590, phys_mat3x4_mul_mat3x4t)

/* 00855240 Mat4x3_MulMat4x3t (ECX = out 4 x 4, EDX = B 4 x 3, arg = A 4 x 3): out row j = (A0 . Bj, A1 . Bj, A2 . Bj, A3 . Bj). */
void phys_mat4x3_mul_mat4x3t(rx_addr out, rx_addr B, rx_addr A) {
    for (uint32_t j = 0; j < 4; j++) {
        rx_v4 bj = v4_ld(row_of(B, j));
        float d[4];
        for (uint32_t i = 0; i < 4; i++) d[i] = d3(row_of(A, i), bj);
        v4_st(row_of(out, j), v4(d[0], d[1], d[2], d[3]));
    }
}
RX_FUNCTION(0x00855240, phys_mat4x3_mul_mat4x3t)

/* ---- block products (stack temporaries at the original's frame addresses) ---- */

/* Frame helper: ESP after a prologue `push ebp; and esp, -16; sub esp, N` and `pushes` register pushes. */
static rx_addr frame_after(rx_addr entry_sp, uint32_t sub, uint32_t pushes) { return ((entry_sp - 4) & ~15u) - sub - 4u * pushes; }
#define CALL_AT(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)
static void init_matrix3x3(rx_addr m) { SU32(m, 4, 3); SU32(m, 8, 3); SU32(m, 0, VT_MATRIX3X3); }

/* 00854f00 Mat6_MulABt (ECX = A, ESI = out, arg = B; all 6 x 6 of 3 x 3 blocks): out = A * B^T block by block, each block the sum
 * of two 3 x 3 products: out.00 = A.00 B.00^T + A.01 B.01^T, out.01 = A.00 B.10^T + A.01 B.11^T, out.10 = A.10 B.00^T + A.11 B.01^T,
 * out.11 = A.10 B.10^T + A.11 B.11^T; the first product goes to a Matrix3x3 local T, the second into the output block, which
 * then becomes T + block (slot 5). T lives at S + 0x20 with S = the frame after sub 0x68 and two pushes. */
void phys_mat6_mul_abt(rx_addr A, rx_addr out, rx_addr B) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x68, 2);
    const rx_addr T = S + 0x20;
    rx_set_sp(S);
    init_matrix3x3(T);
    for (uint32_t pass = 0; pass < 4; pass++) {                 /* out blocks 00, 01, 10, 11 */
        const rx_addr ob = out + 0x10 + 0x40 * pass;
        const rx_addr a0 = A + ((pass < 2) ? 0x10u : 0x90u), a1 = a0 + 0x40;
        const rx_addr b0 = B + ((pass & 1) ? 0x90u : 0x10u), b1 = B + ((pass & 1) ? 0xd0u : 0x50u);
        CALL_AT(S, 1, phys_mat3_mul_abt(T, a0, b0));
        CALL_AT(S, 1, phys_mat3_mul_abt(ob, a1, b1));
        phys_vcall2(ob, 5, ob, T);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00854f00, phys_mat6_mul_abt)

/* 008547d0 (ECX = out, EDX = B, arg = C; 3 x 6 matrices of two 3 x 3 blocks at +0x10 / +0x50, B a 6 x 6 of blocks): out = C * B^T:
 * out.blk1 = C.1 B.00^T + C.2 B.01^T, out.blk2 = C.1 B.10^T + C.2 B.11^T. Four Mat3_MulABt products go to the Matrix3x6 locals
 * P1 (S + 0x10, blocks S + 0x20 / S + 0x60) and P2 (S + 0xa0, blocks S + 0xb0 / S + 0xf0); the sums are the blocks' slot 5
 * (arg0 = out block, arg1 = P2 block): out = P2 + P1. S = frame after sub 0x134 and three pushes. */
void phys_mat3x6_mul_abt(rx_addr out, rx_addr B, rx_addr C) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x134, 3);
    const rx_addr P1 = S + 0x10, P2 = S + 0xa0;
    rx_set_sp(S);
    SU32(P1, 4, 3); SU32(P1, 0, 0x00976c18); SU32(P1, 8, 6);       /* Matrix3x6 */
    SU32(P2, 4, 3); SU32(P2, 0, 0x00976c18); SU32(P2, 8, 6);
    init_matrix3x3(P1 + 0x10); init_matrix3x3(P1 + 0x50); init_matrix3x3(P2 + 0x10); init_matrix3x3(P2 + 0x50);
    CALL_AT(S, 1, phys_mat3_mul_abt(P1 + 0x10, C + 0x10, B + 0x10));
    CALL_AT(S, 1, phys_mat3_mul_abt(P1 + 0x50, C + 0x50, B + 0xd0));
    CALL_AT(S, 1, phys_mat3_mul_abt(P2 + 0x10, C + 0x50, B + 0x50));
    CALL_AT(S, 1, phys_mat3_mul_abt(P2 + 0x50, C + 0x10, B + 0x90));
    phys_vcall2(P1 + 0x10, 5, out + 0x10, P2 + 0x10);
    phys_vcall2(P1 + 0x50, 5, out + 0x50, P2 + 0x50);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x008547d0, phys_mat3x6_mul_abt)

/* 00854910 (ECX = out, EDX = B, arg = C; 4 x 6 matrices of 4 x 3 blocks at +0x10 / +0x60, B a 6 x 6): out = C * B^T through
 * Mat4x3_MulMat3t into the Matrix4x6 locals P1 (S + 0x10, blocks S + 0x20 / S + 0x70) and P2 (S + 0xc0, blocks S + 0xd0 /
 * S + 0x120); out.blk1 = P2.1 + P1.1, out.blk2 = P2.2 + P1.2. S = frame after sub 0x174 and three pushes. */
void phys_mat4x6_mul_abt(rx_addr out, rx_addr B, rx_addr C) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x174, 3);
    const rx_addr P1 = S + 0x10, P2 = S + 0xc0;
    rx_set_sp(S);
    SU32(P1, 4, 4); SU32(P1, 0, 0x0093cab8); SU32(P1, 8, 6);       /* Matrix4x6 */
    SU32(P2, 4, 4); SU32(P2, 0, 0x0093cab8); SU32(P2, 8, 6);
    rx_addr blk[4] = {P1 + 0x10, P1 + 0x60, P2 + 0x10, P2 + 0x60};      /* S+0x20, S+0x70, S+0xd0, S+0x120 */
    for (int i = 0; i < 4; i++) { SU32(blk[i], 4, 4); SU32(blk[i], 8, 3); SU32(blk[i], 0, 0x0093ca4c); }   /* Matrix4x3 */
    CALL_AT(S, 1, phys_mat4x3_mul_mat3t(blk[0], C + 0x10, B + 0x10));
    CALL_AT(S, 1, phys_mat4x3_mul_mat3t(blk[1], C + 0x60, B + 0xd0));
    CALL_AT(S, 1, phys_mat4x3_mul_mat3t(blk[2], C + 0x60, B + 0x50));
    CALL_AT(S, 1, phys_mat4x3_mul_mat3t(blk[3], C + 0x10, B + 0x90));
    phys_vcall2(blk[0], 5, out + 0x10, blk[2]);
    phys_vcall2(blk[1], 5, out + 0x60, blk[3]);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00854910, phys_mat4x6_mul_abt)

/* 008550f0 (ESI = out 3 x 3, EDI = L, arg = B; 3 x 6 matrices): out = L.1 B.1^T + L.2 B.2^T: the first product goes straight to out,
 * the second to a Matrix3x3 local T (S + 0x10), then every row of out += T (out first). S = frame after sub 0x5c and one push. */
void phys_mat3x3_mul_abt_sum(rx_addr out, rx_addr L, rx_addr B) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x5c, 1);
    const rx_addr T = S + 0x10;
    rx_set_sp(S);
    init_matrix3x3(T);
    CALL_AT(S, 1, phys_mat3_mul_abt(out, L + 0x10, B + 0x10));
    CALL_AT(S, 1, phys_mat3_mul_abt(T, L + 0x50, B + 0x50));
    for (uint32_t i = 0; i < 3; i++) {
        rx_v4 o = v4_ld(row_of(out, i)), t = v4_ld(row_of(T, i));
        v4_st(row_of(out, i), v4_add(o, t));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x008550f0, phys_mat3x3_mul_abt_sum)

/* 00855190 (ESI = out 4 x 4, EDI = B, arg = C; 6-wide matrices with 4 x 3 blocks at +0x10 / +0x60): out = B.1 C.1^T + B.2 C.2^T:
 * the first product goes straight to out, the second to a Matrix4x4 local L (S + 0x10), then every row of out += L (out first).
 * S = frame after sub 0x6c and one push. */
void phys_mat4x4_mul_abt_sum(rx_addr out, rx_addr B, rx_addr C) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x6c, 1);
    const rx_addr L = S + 0x10;
    rx_set_sp(S);
    SU32(L, 4, 4); SU32(L, 8, 4); SU32(L, 0, 0x0093ca70);          /* Matrix4x4 */
    CALL_AT(S, 1, phys_mat4x3_mul_mat4x3t(out, B + 0x10, C + 0x10));
    CALL_AT(S, 1, phys_mat4x3_mul_mat4x3t(L, B + 0x60, C + 0x60));
    for (uint32_t i = 0; i < 4; i++) {
        rx_v4 o = v4_ld(row_of(out, i)), l = v4_ld(row_of(L, i));
        v4_st(row_of(out, i), v4_add(o, l));
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00855190, phys_mat4x4_mul_abt_sum)

/* 00854a90 (EDI = B, ESI = D, arg = L): L.blk1 = B.blk1 * D^T, L.blk2 = B.blk2 * D^T (Mat3x4_MulMat4t; three pushes). */
void phys_mat3x4_mul_blocks(rx_addr B, rx_addr D, rx_addr L) {
    const rx_addr sp0 = rx_sp(), cur = sp0 - 12;
    rx_set_sp(cur);
    CALL_AT(cur, 1, phys_mat3x4_mul_mat4t(L + 0x10, B + 0x10, D));
    CALL_AT(cur, 1, phys_mat3x4_mul_mat4t(L + 0x50, B + 0x50, D));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00854a90, phys_mat3x4_mul_blocks)

/* 00854a60 (EDI = B, ESI = D, arg = L): L.blk1 = B.blk1 * D^T, L.blk2 = B.blk2 * D^T (Mat3_MulABt; three pushes). */
void phys_mat3_mul_blocks(rx_addr B, rx_addr D, rx_addr L) {
    const rx_addr sp0 = rx_sp(), cur = sp0 - 12;
    rx_set_sp(cur);
    CALL_AT(cur, 1, phys_mat3_mul_abt(L + 0x10, B + 0x10, D));
    CALL_AT(cur, 1, phys_mat3_mul_abt(L + 0x50, B + 0x50, D));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00854a60, phys_mat3_mul_blocks)

/* 00855090 (ESI = out, args L and B): out.00 = L.1 * B.1^T, out.01 = L.1 * B.2^T, out.10 = L.2 * B.1^T, out.11 = L.2 * B.2^T with
 * the 3 x 4 blocks (Mat3x4_MulMat3x4t); three pushes. */
void phys_mat6_mul_3x4t_blocks(rx_addr out, rx_addr L, rx_addr B) {
    const rx_addr sp0 = rx_sp(), cur = sp0 - 12;
    rx_set_sp(cur);
    CALL_AT(cur, 1, phys_mat3x4_mul_mat3x4t(out + 0x10, L + 0x10, B + 0x10));
    CALL_AT(cur, 1, phys_mat3x4_mul_mat3x4t(out + 0x50, L + 0x10, B + 0x50));
    CALL_AT(cur, 1, phys_mat3x4_mul_mat3x4t(out + 0x90, L + 0x50, B + 0x10));
    CALL_AT(cur, 1, phys_mat3x4_mul_mat3x4t(out + 0xd0, L + 0x50, B + 0x50));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00855090, phys_mat6_mul_3x4t_blocks)

/* 00855030 (ESI = out, args L and B): the same with Mat3_MulABt on the 3 x 3 blocks. */
void phys_mat6_mul_3t_blocks(rx_addr out, rx_addr L, rx_addr B) {
    const rx_addr sp0 = rx_sp(), cur = sp0 - 12;
    rx_set_sp(cur);
    CALL_AT(cur, 1, phys_mat3_mul_abt(out + 0x10, L + 0x10, B + 0x10));
    CALL_AT(cur, 1, phys_mat3_mul_abt(out + 0x50, L + 0x10, B + 0x50));
    CALL_AT(cur, 1, phys_mat3_mul_abt(out + 0x90, L + 0x50, B + 0x10));
    CALL_AT(cur, 1, phys_mat3_mul_abt(out + 0xd0, L + 0x50, B + 0x50));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00855030, phys_mat6_mul_3t_blocks)

/* ---- coupling products of the preparation ----
 * Each takes a square matrix A (3, 4 or 6 rows) and a matrix C, replaces C by C A^T (rows of C against rows of A) and writes the
 * transposed result into B (stores in the original's order). Padding lanes follow each shape's packing. */

/* A row dot of a 3-wide stored row against a loaded row, in the requested operand order. */
static float dot3o(rx_addr row, rx_v4 x, int vec_first) { return vec_first ? d3x(x, row) : d3(row, x); }

/* 00857220 (ECX = A 3 x 3, EDX = B, arg = C; C and B are 3 x 6 / 6 x 3 of two blocks at +0x20.. and +0x60..): per block
 * R[j][i] = A_i . C_j (the first block multiplies A first for j = 0 and C first for j = 1, 2; the second block C first), then
 * C.row_j = (R[j][0], R[j][1], R[j][2], R[j][2]) and B.row_i = (R[0][i], R[1][i], R[2][i], R[2][i]). */
void phys_coupling3(rx_addr A, rx_addr B, rx_addr C) {
    for (uint32_t blk = 0; blk < 2; blk++) {
        const uint32_t base = blk ? 0x60u : 0x20u;
        rx_v4 c[3];
        for (uint32_t j = 0; j < 3; j++) c[j] = v4_ld(C + base + 0x10 * j);
        float R[3][3];
        for (uint32_t j = 0; j < 3; j++)
            for (uint32_t i = 0; i < 3; i++) R[j][i] = dot3o(row_of(A, i), c[j], blk == 1 || j != 0);
        for (uint32_t j = 0; j < 3; j++) v4_st(C + base + 0x10 * j, pack3(R[j][0], R[j][1], R[j][2]));
        for (uint32_t i = 0; i < 3; i++) v4_st(B + base + 0x10 * i, pack3(R[0][i], R[1][i], R[2][i]));
    }
}
RX_FUNCTION(0x00857220, phys_coupling3)

/* 00856b40 (ECX = B, EDX = A 4 x 4, arg = C; C and B are 4 x 6 / 6 x 4 of 3 x 4 blocks, rows +0x20.. / +0x60.. in C and
 * +0x20.. / +0x70.. in B): per block R[i][j] = A_j . C_i with 4-element dots (A first; the last two values of the second block
 * C first), C.row_i = (R[i][0..3]) and B.row_j = (R[0][j], R[1][j], R[2][j], R[0][j]). */
void phys_coupling4(rx_addr B, rx_addr A, rx_addr C) {
    for (uint32_t blk = 0; blk < 2; blk++) {
        const uint32_t cbase = blk ? 0x60u : 0x20u, bbase = blk ? 0x70u : 0x20u;
        rx_v4 c[3];
        for (uint32_t i = 0; i < 3; i++) c[i] = v4_ld(C + cbase + 0x10 * i);
        float R[3][4];
        for (uint32_t i = 0; i < 3; i++)
            for (uint32_t j = 0; j < 4; j++)
                R[i][j] = (blk == 1 && i == 2 && j >= 2) ? d4x(c[i], row_of(A, j)) : d4(row_of(A, j), c[i]);
        for (uint32_t i = 0; i < 3; i++) v4_st(C + cbase + 0x10 * i, v4(R[i][0], R[i][1], R[i][2], R[i][3]));
        for (uint32_t j = 0; j < 4; j++) v4_st(B + bbase + 0x10 * j, pack3w0(R[0][j], R[1][j], R[2][j]));
    }
}
RX_FUNCTION(0x00856b40, phys_coupling4)

/* Shared by the two 6-row couplings: T[i][j] = A_i . C_j over both halves (i < 3: A blocks +0x20 / +0x60, else +0xa0 / +0xe0;
 * C_j = C top row ctop[j] and bottom row cbot[j]); the sum is top + bottom except where `bot_first` is set. */
static float coupling6_term(rx_addr A, uint32_t i, rx_v4 ctop, rx_v4 cbot, int bot_first, int vec_first) {
    const uint32_t r = i % 3;
    const rx_addr atop = A + (i < 3 ? 0x20u : 0xa0u) + 0x10 * r, abot = A + (i < 3 ? 0x60u : 0xe0u) + 0x10 * r;
    float top = dot3o(atop, ctop, vec_first), bot = dot3o(abot, cbot, vec_first);
    return bot_first ? phys_add(bot, top) : phys_add(top, bot);
}

/* 00855810 (ECX = A 6 x 6, EDX = B, arg = C: 3 x 6, halves at +0x20.. and +0x60..): T = A C^T (6 x 3). The terms with the vector
 * first are those of the first three rows for columns 1 and 2. C.top_j = (T0j, T1j, T2j, T2j), C.bot_j = (T3j, T4j, T5j, T5j) and
 * B rows i = (Ti0, Ti1, Ti2, Ti2) at +0x20 / +0x60. Store order: B rows 0..2, C top, C bottom, B rows 3..5. */
void phys_coupling6x3(rx_addr A, rx_addr B, rx_addr C) {
    rx_v4 ctop[3], cbot[3];
    for (uint32_t j = 0; j < 3; j++) { ctop[j] = v4_ld(C + 0x20 + 0x10 * j); cbot[j] = v4_ld(C + 0x60 + 0x10 * j); }
    float T[6][3];
    for (uint32_t i = 0; i < 6; i++)
        for (uint32_t j = 0; j < 3; j++) T[i][j] = coupling6_term(A, i, ctop[j], cbot[j], 0, i < 3 && j != 0);
    for (uint32_t i = 0; i < 3; i++) v4_st(B + 0x20 + 0x10 * i, pack3(T[i][0], T[i][1], T[i][2]));
    for (uint32_t j = 0; j < 3; j++) v4_st(C + 0x20 + 0x10 * j, pack3(T[0][j], T[1][j], T[2][j]));
    for (uint32_t j = 0; j < 3; j++) v4_st(C + 0x60 + 0x10 * j, pack3(T[3][j], T[4][j], T[5][j]));
    for (uint32_t i = 3; i < 6; i++) v4_st(B + 0x60 + 0x10 * (i - 3), pack3(T[i][0], T[i][1], T[i][2]));
}
RX_FUNCTION(0x00855810, phys_coupling6x3)

/* 00855ee0 (ECX = A 6 x 6, EDX = B, arg = C: 4 x 6, halves at +0x20.. and +0x70..): T = A C^T (6 x 4), all products matrix
 * first; the sum is bottom + top for T00, T30, T40, T50. C.top_j = (T0j, T1j, T2j, T2j), C.bot_j = (T3j, T4j, T5j, T5j); B rows
 * i = (Ti0 .. Ti3) at +0x20 / +0x60. Store order: B rows 0..2, C top, C bottom, B rows 3..5. */
void phys_coupling6x4(rx_addr A, rx_addr B, rx_addr C) {
    rx_v4 ctop[4], cbot[4];
    for (uint32_t j = 0; j < 4; j++) { ctop[j] = v4_ld(C + 0x20 + 0x10 * j); cbot[j] = v4_ld(C + 0x70 + 0x10 * j); }
    float T[6][4];
    for (uint32_t i = 0; i < 6; i++)
        for (uint32_t j = 0; j < 4; j++) T[i][j] = coupling6_term(A, i, ctop[j], cbot[j], j == 0 && (i == 0 || i >= 3), 0);
    for (uint32_t i = 0; i < 3; i++) v4_st(B + 0x20 + 0x10 * i, v4(T[i][0], T[i][1], T[i][2], T[i][3]));
    for (uint32_t j = 0; j < 4; j++) v4_st(C + 0x20 + 0x10 * j, pack3(T[0][j], T[1][j], T[2][j]));
    for (uint32_t j = 0; j < 4; j++) v4_st(C + 0x70 + 0x10 * j, pack3(T[3][j], T[4][j], T[5][j]));
    for (uint32_t i = 3; i < 6; i++) v4_st(B + 0x60 + 0x10 * (i - 3), v4(T[i][0], T[i][1], T[i][2], T[i][3]));
}
RX_FUNCTION(0x00855ee0, phys_coupling6x4)

/* ---- inverses ---- */
#define MASK_XYZ_ADDR 0x0092db80u

/* cofactor vector of two rows: lane i = a[(i+2)%3] b[(i+1)%3] - a[(i+1)%3] b[(i+2)%3] (shuffles 2 0 1 and 1 2 0), W cleared */
static rx_v4 cofactor(rx_v4 a, rx_v4 b) {
    rx_v4 p = v4_mul(v4_perm(a, 2, 0, 1, 3), v4_perm(b, 1, 2, 0, 3));
    rx_v4 q = v4_mul(v4_perm(a, 1, 2, 0, 3), v4_perm(b, 2, 0, 1, 3));
    return v4_and(v4_sub(p, q), v4_ld(MASK_XYZ_ADDR));
}

/* 00859250 Mat3_Invert (ECX = out, arg = in; 3 x 3): the cofactor rows c0 = cof(r2, r1), c1 = cof(r0, r2), c2 = cof(r1, r0),
 * det = r0 . c0 (3-element dot), scale = (runtime vec4 00d61650) / det, out rows = scale c0, c1 scale, c2 scale, then the result is
 * transposed in place (slot 2 of out with out as the argument). Frame: esp = ((entry esp - 4) & ~15) - 0x10. */
void phys_mat3_invert(rx_addr out, rx_addr in) {
    const rx_addr sp0 = rx_sp();
    const rx_addr cur = ((sp0 - 4) & ~15u) - 0x10;
    rx_v4 r0 = v4_ld(row_of(in, 0)), r1 = v4_ld(row_of(in, 1)), r2 = v4_ld(row_of(in, 2));
    rx_v4 c0 = cofactor(r2, r1);
    rx_v4 c1 = cofactor(r0, r2);
    rx_v4 c2 = cofactor(r1, r0);
    float det = phys_dot3(r0, c0);
    rx_v4 scale = v4_div(v4_ld(NUMERATOR), v4_splat(det));
    v4_st(row_of(out, 0), v4_mul(scale, c0));
    v4_st(row_of(out, 1), v4_mul(c1, scale));
    v4_st(row_of(out, 2), v4_mul(c2, scale));
    rx_set_sp(cur);
    phys_vcall1(out, 2, out);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00859250, phys_mat3_invert)

/* The 6 x 6 inverse by Schur complement of the 3 x 3 blocks (shared by Mat6_Invert, Mat5_Invert): with the source blocks
 * A B / C D (A at +0x10, B +0x50, C +0x90, D +0xd0) and S = the frame after sub 0x1d4 and three pushes, the Matrix3x3 locals are
 * E (S + 0x10), V (S + 0xd0), K (S + 0x50), R (S + 0x90), H (S + 0x110), U (S + 0x150), T (S + 0x190):
 *   E = inv(A); T = C E; U = E B; V = C U; K = V - D (rows, packed); H = inv(K); out.B = U H; out.C = H T; R = U out.C;
 *   out.A = E - R (rows); out.D = H (slot 3), then negated (slot 6).
 * Every product is Mat3_Mul(ECX = right operand, EDI = left operand, ESI = result): result = left * right. */
static void mat6_invert_core(rx_addr src, rx_addr dst, rx_addr S) {
    const rx_addr E = S + 0x10, K = S + 0x50, R = S + 0x90, V = S + 0xd0, H = S + 0x110, U = S + 0x150, T = S + 0x190;
    const rx_addr locals[7] = {E, K, R, V, H, U, T};
    for (int i = 0; i < 7; i++) init_matrix3x3(locals[i]);
    CALL_AT(S, 1, phys_mat3_invert(E, src + 0x10));
    CALL_AT(S, 0, phys_mat3_mul(E, src + 0x90, T));                 /* T = C E */
    CALL_AT(S, 0, phys_mat3_mul(src + 0x50, E, U));                 /* U = E B */
    CALL_AT(S, 0, phys_mat3_mul(U, src + 0x90, V));                 /* V = C U */
    for (uint32_t i = 0; i < 3; i++) v4_st(row_of(K, i), v4_sub(v4_ld(row_of(V, i)), v4_ld(row_of(src + 0xd0, i))));
    CALL_AT(S, 1, phys_mat3_invert(H, K));
    CALL_AT(S, 0, phys_mat3_mul(H, U, dst + 0x50));                 /* out.B = U H */
    CALL_AT(S, 0, phys_mat3_mul(T, H, dst + 0x90));                 /* out.C = H T */
    CALL_AT(S, 0, phys_mat3_mul(dst + 0x90, U, R));                 /* R = U out.C */
    for (uint32_t i = 0; i < 3; i++) v4_st(row_of(dst + 0x10, i), v4_sub(v4_ld(row_of(E, i)), v4_ld(row_of(R, i))));
    phys_vcall1(dst + 0xd0, 3, H);
    phys_vcall0(dst + 0xd0, 6);
}

/* 008597e0 Mat6_Invert (ECX = source, arg = destination; both 6 x 6 of 3 x 3 blocks). Frame as mat6_invert_core. */
void phys_mat6_invert(rx_addr in, rx_addr out) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x1d4, 3);
    rx_set_sp(S);
    mat6_invert_core(in, out, S);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x008597e0, phys_mat6_invert)

/* 00859520 Mat5_Invert (args out, in): the 5 x 5 stored as the 6 x 6 blocks of a 5 x 6 matrix is padded in place with the
 * identity's missing row and column (in+0xc0, in+0x100 = the runtime zero vec4, the rows in+0x60 / 0x70 / 0x80 / 0xe0 / 0xf0
 * keep lanes 0..1 and take lane 2 / 3 of that zero, the last row takes lane 2 / 3 of the runtime vec4 00d61650), then inverted
 * like a 6 x 6. */
void phys_mat5_invert(rx_addr out, rx_addr in) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = frame_after(sp0, 0x1d4, 3);
    rx_set_sp(S);
    v4_st(in + 0xc0, v4_ld(G_ZERO));
    v4_st(in + 0x100, v4_ld(G_ZERO));
    static const uint32_t padrows[5] = {0x60, 0x70, 0x80, 0xe0, 0xf0};
    for (int i = 0; i < 5; i++) v4_st(in + padrows[i], v4_shuf(v4_ld(in + padrows[i]), v4_ld(G_ZERO), 0xa4));
    v4_st(in + 0x100, v4_shuf(v4_ld(in + 0x100), v4_ld(NUMERATOR), 0xa4));
    mat6_invert_core(in, out, S);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00859520, phys_mat5_invert)

/* 00859340 Mat4_Invert (ESI = out, EDI = in; 4 x 4): the matrix is embedded in a Matrix6x6 L (S + 0x20) with the runtime unit
 * vectors (00d615c0 X, 00d616a0 Y, 00d61680 Z, zero 00d61670): A = rows 0..2, B = (row.w, Z0.y, Z0.z, Z0.w), C = (row 3, 0, 0),
 * D = ((row3.w, X.y, X.z, X.w), Y, Z); inverted by Mat6_Invert into R (S + 0x130); out rows 0..2 = (R.A row xyz, R.B row.x), out
 * row 3 = (R.C row0 xyz, R.D row0.x). Frame: esp = ((entry - 4) & ~15) - 0x250. */
void phys_mat4_invert(rx_addr out, rx_addr in) {
    const rx_addr sp0 = rx_sp();
    const rx_addr S = ((sp0 - 4) & ~15u) - 0x250;
    const rx_addr L = S + 0x20, R = S + 0x130;
    rx_set_sp(S);
    CALL_AT(S, 0, phys_matrix6x6_construct(L));
    CALL_AT(S, 0, phys_matrix6x6_construct(R));
    rx_v4 yv = v4_ld(0x00d616a0), z0 = v4_ld(G_ZERO), xv = v4_ld(0x00d615c0), zv = v4_ld(0x00d61680);
    rx_v4 s[4];
    for (uint32_t i = 0; i < 4; i++) s[i] = v4_ld(row_of(in, i));
    /* 6 x 6 block rows: A +0x20.., B +0x60.., C +0xa0.., D +0xe0.. (relative to the object) */
    v4_st(L + 0xf0, yv);
    v4_st(L + 0x100, zv);
    for (uint32_t i = 0; i < 3; i++) v4_st(L + 0x20 + 0x10 * i, s[i]);                       /* A = rows 0..2 */
    v4_st(L + 0xa0, s[3]);                                                                   /* C row 0 = row 3 */
    for (uint32_t i = 0; i < 3; i++) v4_st(L + 0x60 + 0x10 * i, v4_movss(z0, v4_lane(s[i], 3)));   /* B = (row.w, 0, 0, 0) */
    v4_st(L + 0xb0, z0);
    v4_st(L + 0xc0, z0);                                                                     /* C rows 1, 2 */
    v4_st(L + 0xe0, v4_movss(xv, v4_lane(s[3], 3)));                                         /* D row 0 = X with lane 0 = row3.w */
    CALL_AT(S, 1, phys_mat6_invert(L, R));
    rx_v4 outr[4];
    for (uint32_t i = 0; i < 3; i++) {
        rx_v4 a = v4_ld(R + 0x20 + 0x10 * i), b = v4_ld(R + 0x60 + 0x10 * i);                /* R.A row i, R.B row i */
        outr[i] = v4(a.v[0], a.v[1], a.v[2], b.v[0]);
    }
    {
        rx_v4 c = v4_ld(R + 0xa0), d = v4_ld(R + 0xe0);                                        /* R.C row 0, R.D row 0 */
        outr[3] = v4(c.v[0], c.v[1], c.v[2], d.v[0]);
    }
    for (uint32_t i = 0; i < 4; i++) v4_st(row_of(out, i), outr[i]);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00859340, phys_mat4_invert)

/* ---- constructors of block matrices ---- */

/* 00853550 (EAX = this): Matrix3x6 = two Matrix3x3 blocks at +0x10, +0x50 (stores in the original's order). */
void phys_matrix3x6_construct(rx_addr m) {
    SU32(m, 4, 3); SU32(m, 8, 6); SU32(m, 0, 0x00976c18);
    SU32(m, 0x14, 3); SU32(m, 0x18, 3); SU32(m, 0x10, VT_MATRIX3X3);
    SU32(m, 0x54, 3); SU32(m, 0x58, 3); SU32(m, 0x50, VT_MATRIX3X3);
}
RX_FUNCTION(0x00853550, phys_matrix3x6_construct)

/* 00853710 (EAX = this): Matrix6x3 = two Matrix3x3 blocks at +0x10, +0x50. */
void phys_matrix6x3_construct(rx_addr m) {
    SU32(m, 4, 6); SU32(m, 8, 3); SU32(m, 0, 0x00976c84);
    SU32(m, 0x14, 3); SU32(m, 0x18, 3); SU32(m, 0x10, VT_MATRIX3X3);
    SU32(m, 0x54, 3); SU32(m, 0x58, 3); SU32(m, 0x50, VT_MATRIX3X3);
}
RX_FUNCTION(0x00853710, phys_matrix6x3_construct)

/* Matrix5x6 / Matrix6x5 (ESI = this): four Matrix3x3 blocks at +0x10, +0x50, +0x90, +0xd0, each cleared through its vtable
 * (slot 1) in that order. The original saves three registers (esp = entry - 12 at the calls). */
static void construct_5x6_like(rx_addr m, uint32_t vtable, uint32_t rows, uint32_t cols) {
    const rx_addr sp0 = rx_sp();
    SU32(m, 4, rows); SU32(m, 8, cols); SU32(m, 0, vtable);
    SU32(m, 0x14, 3); SU32(m, 0x18, 3); SU32(m, 0x10, VT_MATRIX3X3);
    rx_set_sp(sp0 - 12);
    SU32(m, 0x54, 3); SU32(m, 0x58, 3); SU32(m, 0x50, VT_MATRIX3X3);
    SU32(m, 0x94, 3); SU32(m, 0x98, 3); SU32(m, 0x90, VT_MATRIX3X3);
    SU32(m, 0xd4, 3); SU32(m, 0xd8, 3); SU32(m, 0xd0, VT_MATRIX3X3);
    phys_vcall0(m + 0x10, 1);
    phys_vcall0(m + 0x50, 1);
    phys_vcall0(m + 0x90, 1);
    phys_vcall0(m + 0xd0, 1);
    rx_set_sp(sp0);
}
/* 008535b0 (ESI = this): Matrix5x6 (vtable 00976c3c). */
void phys_matrix5x6_construct(rx_addr m) { construct_5x6_like(m, 0x00976c3c, 5, 6); }
RX_FUNCTION(0x008535b0, phys_matrix5x6_construct)
/* 00853640 (ESI = this): Matrix6x5 (vtable 00976c60). */
void phys_matrix6x5_construct(rx_addr m) { construct_5x6_like(m, 0x00976c60, 6, 5); }
RX_FUNCTION(0x00853640, phys_matrix6x5_construct)

/* ---- rotation matrix of a quaternion, cross-product matrices, copy ---- */

/* 00857e50 (ECX = out, arg = q = (x, y, z, w) at q, scalars): the rotation matrix with the constant -2 (0x009d7b30, read from
 * memory) and the -0 / 1 constants; rows
 *   (1 - (C + B), D + wz, F - wy, F - wy), (D - wz, 1 - (C + A), E + wx, E + wx), (F + wy, E - wx, 1 - (B + A), 1 - (B + A))
 * with x2 = x * -2 (likewise y2, z2), A = -0 - x x2, B = -0 - y y2, C = -0 - z z2, D = -0 - y x2, E = -0 - z y2, F = -0 - z x2,
 * wx = w x2, wy = w y2, wz = w z2. */
void phys_matrix3x3_from_quaternion(rx_addr out, rx_addr q) {
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 12);
    const float m2 = F32(0, 0x009d7b30), nz = rx_bf(0x80000000), one = 1.0f;
    float z2 = phys_mul(z, m2), y2 = phys_mul(y, m2), x2 = phys_mul(x, m2);
    float A = phys_sub(nz, phys_mul(x, x2));
    float B = phys_sub(nz, phys_mul(y, y2));
    float C = phys_sub(nz, phys_mul(z, z2));
    float wx = phys_mul(w, x2), wy = phys_mul(w, y2), wz = phys_mul(w, z2);
    float D = phys_sub(nz, phys_mul(y, x2));
    float zy = phys_mul(z, y2), zx = phys_mul(z, x2);
    float E = phys_sub(nz, zy), F = phys_sub(nz, zx);
    float CB = phys_add(C, B), CA = phys_add(C, A), BA = phys_add(B, A);
#ifdef RX_CONTACT_FRAME_OBSERVERS
    rx_contact_matrix_q1_spills(w,z2,A,wx,y2,wy,wz);
#endif
    v4_st(row_of(out, 0), v4(phys_sub(one, CB), phys_add(D, wz), phys_sub(F, wy), phys_sub(F, wy)));
    v4_st(row_of(out, 1), v4(phys_sub(D, wz), phys_sub(one, CA), phys_add(E, wx), phys_add(E, wx)));
    v4_st(row_of(out, 2), v4(phys_add(F, wy), phys_sub(E, wx), phys_sub(one, BA), phys_sub(one, BA)));
}
RX_FUNCTION(0x00857e50, phys_matrix3x3_from_quaternion)

/* 00857d00 (ECX = out, arg = q): the rotation matrix with the constant +2 (0x0092e330):
 * rows (1 - (zz + yy), xy + wz, zx - wy, zx - wy), (xy - wz, 1 - (zz + xx), zy + wx, zy + wx), (zx + wy, zy - wx, 1 - (yy + xx),
 * 1 - (yy + xx)) with x2 = 2x ..., xx = x x2, yy = y y2, zz = z z2, xy = y x2, zy = z y2, zx = z x2, wx = w x2, wy = w y2, wz = w z2. */
void phys_matrix3x3_from_quaternion2(rx_addr out, rx_addr q) {
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 12);
    const float two = 2.0f, one = 1.0f;
    float z2 = phys_mul(z, two), x2 = phys_mul(x, two), y2 = phys_mul(y, two);
    float xx = phys_mul(x, x2), zz = phys_mul(z, z2), yy = phys_mul(y, y2);
    float wx = phys_mul(w, x2), xy = phys_mul(y, x2), wz = phys_mul(w, z2);
    float zy = phys_mul(z, y2), zx = phys_mul(z, x2), wy = phys_mul(w, y2);
    float zzyy = phys_add(zz, yy), zzxx = phys_add(zz, xx), yyxx = phys_add(yy, xx);
#ifdef RX_CONTACT_FRAME_OBSERVERS
    rx_contact_matrix_q2_spills(wz,xx,wx,zy);
#endif
    v4_st(row_of(out, 0), v4(phys_sub(one, zzyy), phys_add(xy, wz), phys_sub(zx, wy), phys_sub(zx, wy)));
    v4_st(row_of(out, 1), v4(phys_sub(xy, wz), phys_sub(one, zzxx), phys_add(zy, wx), phys_add(zy, wx)));
    v4_st(row_of(out, 2), v4(phys_add(zx, wy), phys_sub(zy, wx), phys_sub(one, yyxx), phys_sub(one, yyxx)));
}
RX_FUNCTION(0x00857d00, phys_matrix3x3_from_quaternion2)

/* 00857ff0 (ECX = w vec4, arg = out): the cross-product matrix of w (xyz) over the cleared rows (runtime vec4 00d61670 per row):
 * rows (0, -wz, wy, wy), (wz, 0, -wx, -wx), (-wy, wx, Z2, Z3) with the zero lanes taken from the runtime vec4. */
void phys_matrix3x3_skew(rx_addr w, rx_addr out) {
    rx_v4 wv = v4_ld(w), nw = v4_sub(v4_zero(), wv);
    for (uint32_t i = 0; i < 3; i++) v4_st(row_of(out, i), v4_ld(G_ZERO));
    rx_v4 z = v4_ld(G_ZERO);
    v4_st(row_of(out, 0), v4(z.v[0], nw.v[2], wv.v[1], wv.v[1]));
    z = v4_ld(G_ZERO);
    v4_st(row_of(out, 1), v4(wv.v[2], z.v[1], nw.v[0], nw.v[0]));
    z = v4_ld(G_ZERO);
    v4_st(row_of(out, 2), v4(nw.v[1], wv.v[0], z.v[2], z.v[3]));
}
RX_FUNCTION(0x00857ff0, phys_matrix3x3_skew)

/* 00858090 (ECX = w vec4, arg = out): the transposed cross-product matrix: rows (0, wz, -wy, -wy), (-wz, 0, wx, wx),
 * (wy, -wx, Z2, Z3). */
void phys_matrix3x3_skew_t(rx_addr w, rx_addr out) {
    rx_v4 wv = v4_ld(w), nw = v4_sub(v4_zero(), wv);
    for (uint32_t i = 0; i < 3; i++) v4_st(row_of(out, i), v4_ld(G_ZERO));
    rx_v4 z = v4_ld(G_ZERO);
    v4_st(row_of(out, 0), v4(z.v[0], wv.v[2], nw.v[1], nw.v[1]));
    z = v4_ld(G_ZERO);
    v4_st(row_of(out, 1), v4(nw.v[2], z.v[1], wv.v[0], wv.v[0]));
    z = v4_ld(G_ZERO);
    v4_st(row_of(out, 2), v4(wv.v[1], nw.v[0], z.v[2], z.v[3]));
}
RX_FUNCTION(0x00858090, phys_matrix3x3_skew_t)

/* 00858d90 (ECX = source, arg = destination): copy a Matrix5x6: counts of the block matrix (+4, +8) and of every 3 x 3 block
 * (+0x14 / +0x18, +0x54 / +0x58, +0x94 / +0x98, +0xd4 / +0xd8) and their rows (+0x20.. +0x40, +0x60.. +0x80, +0xa0.. +0xc0,
 * +0xe0.. +0x100). Returns dst. */
rx_addr phys_matrix5x6_copy_from(rx_addr src, rx_addr dst) {
    SU32(dst, 4, U32(src, 4));
    SU32(dst, 8, U32(src, 8));
    for (uint32_t blk = 0; blk < 4; blk++) {
        const uint32_t b = 0x10 + 0x40 * blk;
        SU32(dst, b + 4, U32(src, b + 4));
        SU32(dst, b + 8, U32(src, b + 8));
        for (uint32_t r = 1; r <= 3; r++) v4_st(dst + b + 0x10 * r, v4_ld(src + b + 0x10 * r));
    }
    return dst;
}
RX_FUNCTION(0x00858d90, phys_matrix5x6_copy_from)
