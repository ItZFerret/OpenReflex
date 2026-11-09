/* build_model.c -- see build_model.h. Hand transliteration of ModelAssetIO::vf3 007d53c0, the ModelAsset constructor FUN_007d3f30,
 * FUN_007d40e0, ModelAsset_LoadStatic 007d4650, the node set builders FUN_007dd590 / FUN_007de510 / FUN_007ddb40 (+ FUN_007dd540) and the
 * NodeSet_CopyFrom 007dd930 glue from the disassembly. SSE sequences are done lane by lane in the original's operation order; the
 * double precision parts (cvtps2pd / mulsd / addsd) in C double. */
#include "build_model.h"
#include "build_alloc.h"
#include "build_load.h"
#include "world_node.h"
#include "anim.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

#define ZERO_VEC 0x00d61670u
#define IDENT    0x00d61600u     /* the runtime identity matrix (4 x 16 bytes) */

static uint32_t align16(uint32_t v) { return (v + 0xf) & ~0xfu; }
static uint32_t x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static rx_v4 splat_lane(rx_v4 m, int i) { return rx_v4_splat(m.v[i]); }
/* a row times the matrix P (rows P0..P3): (w * P3 + y * P1) + (z * P2 + x * P0) */
static rx_v4 row_times(rx_v4 m, rx_v4 P0, rx_v4 P1, rx_v4 P2, rx_v4 P3) {
    rx_v4 a = rx_v4_add(rx_v4_mul(splat_lane(m, 3), P3), rx_v4_mul(splat_lane(m, 1), P1));
    rx_v4 b = rx_v4_add(rx_v4_mul(splat_lane(m, 2), P2), rx_v4_mul(splat_lane(m, 0), P0));
    return rx_v4_add(a, b);
}
static void copy_bytes(rx_addr d, rx_addr s, uint32_t n) { for (uint32_t k = 0; k < n; k++) SU8(d, k, U8(s, k)); }
static void copy_matrix_const(rx_addr d) { for (uint32_t k = 0; k < 4; k++) rx_st4(d + 16 * k, rx_ld4(IDENT + 16 * k)); }

/* ---- node sets ---- */

/* the node records' initial state (the loops of FUN_007dd590 / FUN_007ddb40) */
static void init_record(rx_addr r) {
    SU8(r, 0xf, 0xff); SU8(r, 0xd, 0xff); SU8(r, 0xe, 0xff); SU16(r, 8, 0xffff); SU32(r, 0, 0); SU8(r, 0xc, 0);
}

/* FUN_007dd540 (EDX = source set, stack flag): the block size of a set with the source's counts (flag 1: one node more when the source has no extra) */
static uint32_t nodeset_block_size(rx_addr src, int flag) {
    uint32_t n = ((uint32_t)U16(src, 0x38) | (flag ? 1u : 0u)) + U32(src, 0xc);
    uint32_t ex = n > 1 ? align16(n << 6) * 2 : 0;
    return align16(n * 0x50u) + (align16(n << 6) + n * 8) * 2 + U32(src, 0x14) + ex;
}

/* FUN_007ddb40 (ECX = destination, EDX = source, stack flag; RET 4): the destination gets its own block laid out like the source and its
 * contents; node records are re-linked to the destination. Flag 1 (used by the scene unit's ModelAsset clone 007d4d60): when the source has
 * no extra node, a new root node (index = count, identity matrices, empty bounds) becomes the parent of the old root. Flag 0 is the
 * loader's use (verified against the original). */
void rx_nodeset_clone(rx_addr d, rx_addr src, int flag) {
    uint32_t cnt = U32(src, 0xc), extra = U16(src, 0x38);
    uint32_t nflag = (flag ? 1u : 0u) | extra, nalloc = cnt + nflag, n = cnt + extra;
    if (U32(d, 0xc) != 0) rx_nodeset_reset(d);                      /* FUN_007dd890: a used destination is released first */
    uint32_t total = align16(nodeset_block_size(src, flag));
    rx_addr blk = rx_crt_aligned_malloc(total, 0x80);
    SU16(d, 0x3a, total); SU32(d, 0x3c, blk);
    uint32_t mb = align16(nalloc << 6);
    if (cnt > 1) { SU32(d, 0x28, blk); blk += mb; SU32(d, 0x24, blk); blk += mb; }
    SU32(d, 0x18, blk); blk += mb;
    SU32(d, 0x1c, blk); blk += mb;
    SU32(d, 0x20, blk); blk += align16(nalloc * 0x50u);
    SU32(d, 8, blk);
    SU32(d, 0x14, U32(src, 0x14));
    SU32(d, 0x10, blk + (nalloc << 4));
    SU32(d, 0xc, cnt);
    SU16(d, 0x38, nflag);
    copy_bytes(U32(d, 0x18), U32(src, 0x18), n << 6);
    copy_bytes(U32(d, 0x1c), U32(src, 0x1c), n << 6);
    copy_bytes(U32(d, 0x20), U32(src, 0x20), n * 0x50u);
    copy_bytes(U32(d, 0x10), U32(src, 0x10), U32(d, 0x14));
    for (uint32_t i = 0; i < U32(d, 0xc); i++) init_record(U32(d, 8) + 16 * i);
    if (U32(d, 0xc) > 1) {
        if (U32(src, 0x24)) copy_bytes(U32(d, 0x24), U32(src, 0x24), n << 6);
        if (U32(src, 0x28)) copy_bytes(U32(d, 0x28), U32(src, 0x28), n << 6);
    }
    rx_addr r0 = U32(src, 0), r4 = U32(src, 4);
    SU32(d, 0, r0 ? ((r0 - U32(src, 8)) & ~0xfu) + U32(d, 8) : 0);
    SU32(d, 4, r4 ? ((r4 - U32(src, 8)) & ~0xfu) + U32(d, 8) : 0);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr dr = U32(d, 8) + 16 * i, sr = U32(src, 8) + 16 * i;
        SU32(dr, 0, d); SU8(dr, 0xc, U8(sr, 0xc)); SU8(dr, 0xb, i);
        SU8(dr, 0xf, U8(sr, 0xf)); SU8(dr, 0xd, U8(sr, 0xd)); SU8(dr, 0xe, U8(sr, 0xe));
        SU16(dr, 8, U16(sr, 8));
    }
    if (flag && extra == 0) {   /* the extra root node */
        uint32_t i = cnt;
        rx_addr recs = U32(d, 8), oldroot = U32(d, 0), nr = recs + 16 * i;
        SU32(d, 0, nr);
        init_record(nr);
        SU32(nr, 0, d); SU8(nr, 0xb, i);
        SU8(oldroot, 0xf, (nr - recs) >> 4);
        SU8(nr, 0xe, (oldroot - recs) >> 4);
        for (uint32_t k = 0; k < 4; k++) { rx_st4(U32(d, 0x18) + 0x40 * i + 16 * k, rx_ld4(IDENT + 16 * k)); rx_st4(U32(d, 0x1c) + 0x40 * i + 16 * k, rx_ld4(IDENT + 16 * k)); }
        rx_addr nb = U32(d, 0x20) + 0x50 * i;
        rx_st4(nb + 0x20, rx_ld4(ZERO_VEC)); rx_st4(nb + 0x30, rx_ld4(ZERO_VEC)); SF32(nb, 0x44, 0.0f);
        rx_st4(nb, rx_ld4(ZERO_VEC)); rx_st4(nb + 0x10, rx_ld4(ZERO_VEC)); SF32(nb, 0x40, 0.0f);
    }
    SU32(d, 0x34, 0);
}
static void nodeset_init_from(rx_addr d, rx_addr src) { rx_nodeset_clone(d, src, 0); }

/* NodeSet_CopyFrom 007dd930 (ESI = destination, EDI = source) */
static void nodeset_copy_from(rx_addr dst, rx_addr src) {
    if (I32(src, 0xc) <= 0) return;
    SU32(dst, 0x30, 0);
    uint32_t n = (uint32_t)U16(src, 0x38) + U32(src, 0xc);
    rx_world_node_evaluate_world(U32(src, 0), 1);
    if (U32(dst, 0xc) == 0) nodeset_init_from(dst, src);
    copy_bytes(U32(dst, 0x18), U32(src, 0x18), n << 6);
    copy_bytes(U32(dst, 0x1c), U32(src, 0x1c), n << 6);
    copy_bytes(U32(dst, 0x20), U32(src, 0x20), n * 0x50u);
    if (U32(src, 0x24)) copy_bytes(U32(dst, 0x24), U32(src, 0x24), n << 6);
    if (U32(src, 0x28)) copy_bytes(U32(dst, 0x28), U32(src, 0x28), n << 6);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr r = U32(dst, 8) + 16 * i;
        SU8(r, 0xc, U8(U32(src, 8) + 16 * i, 0xc));
        SU8(r, 0xc, U8(r, 0xc) & 0xfe);
        SU8(r, 0xc, U8(r, 0xc) & 0xfb);
    }
}

/* FUN_007de510 (stack: set): per node the inverse of its world matrix (the product of the local matrices up the parent chain) into +0x24 */
static void nodeset_inverse_worlds(rx_addr ns) {
    SU32(ns, 0x30, 0);
    for (uint32_t i = 0; i < U32(ns, 0xc); i++) {
        rx_addr rec = U32(ns, 8) + 16 * i;
        rx_addr lm = U32(ns, 0x18) + 0x40 * i;
        rx_v4 W0 = rx_ld4(lm), W1 = rx_ld4(lm + 0x10), W2 = rx_ld4(lm + 0x20), W3 = rx_ld4(lm + 0x30);
        uint8_t p = U8(rec, 0xf);
        while (p != 0xff) {
            rx_addr prec = U32(U32(rec, 0), 8) + 16u * p;
            rx_addr pm = U32(U32(prec, 0), 0x18) + 0x40u * U8(prec, 0xb);
            rx_v4 P0 = rx_ld4(pm), P1 = rx_ld4(pm + 0x10), P2 = rx_ld4(pm + 0x20), P3 = rx_ld4(pm + 0x30);
            rx_v4 n0 = row_times(W0, P0, P1, P2, P3), n1 = row_times(W1, P0, P1, P2, P3);
            rx_v4 n2 = row_times(W2, P0, P1, P2, P3), n3 = row_times(W3, P0, P1, P2, P3);
            W0 = n0; W1 = n1; W2 = n2; W3 = n3;
            p = U8(prec, 0xf);
        }
        rx_v4 nt = rx_v4_sub(rx_v4_splat(0.0f), W3);
        float c0 = F32(0, 0x00d61660);
        rx_v4 Cx = {{W0.v[0], W1.v[0], W2.v[0], c0}}, Cy = {{W0.v[1], W1.v[1], W2.v[1], c0}}, Cz = {{W0.v[2], W1.v[2], W2.v[2], c0}};
        #define DOT(W) ((float)(((double)W.v[1] * (double)nt.v[1] + (double)W.v[0] * (double)nt.v[0]) + (double)W.v[2] * (double)nt.v[2]))
        float a = DOT(W0), b = DOT(W1), c = DOT(W2);
        #undef DOT
        rx_addr o = U32(ns, 0x24) + 0x40 * i;
        rx_st4(o, Cx); rx_st4(o + 0x10, Cy); rx_st4(o + 0x20, Cz);
        rx_v4 last = {{a, b, c, F32(0, 0x00d6169c)}};   /* lane 3 of the 1.0 splat 0xd61690 */
        rx_st4(o + 0x30, last);
    }
}

/* FUN_007dd590 (ECX = node count, stack: stream, name pool size; ESI = set): the set of the file */
static void nodeset_load(rx_addr ns, uint32_t nn, uint32_t npool, rx_stream* s) {
    uint32_t mb = align16(nn << 6);
    uint32_t total = align16(align16(nn * 0x50u) + (mb + nn * 8) * 2 + (nn > 1 ? align16(nn << 6) * 2 : 0) + npool);
    rx_addr blk = rx_crt_aligned_malloc(total, 0x80);
    SU16(ns, 0x3a, total); SU32(ns, 0x3c, blk);
    if ((int32_t)nn > 1) { SU32(ns, 0x28, blk); blk += mb; SU32(ns, 0x24, blk); blk += mb; }
    SU32(ns, 0x18, blk); blk += mb;
    SU32(ns, 0x1c, blk); blk += mb;
    SU32(ns, 0x20, blk); blk += align16(nn * 0x50u);
    SU32(ns, 0x14, npool); SU32(ns, 0xc, nn); SU32(ns, 8, blk); SU32(ns, 0x10, blk + (nn << 4));
    for (uint32_t i = 0; i < nn; i++) init_record(U32(ns, 8) + 16 * i);
    for (uint32_t i = 0; i < nn; i++) {
        rx_stream_read(s, U32(ns, 0x18) + 0x40 * i, 0x40);
        copy_matrix_const(U32(ns, 0x1c) + 0x40 * i);
    }
    for (uint32_t i = 0; i < nn; i++) {
        uint32_t w[11]; rx_stream_read_host(s, w, 0x2c);
        rx_addr rec = U32(ns, 8) + 16 * i, ld = U32(ns, 0x20) + 0x50 * i;
        SU32(rec, 0, ns); SU8(rec, 0xb, i); SU16(rec, 8, w[0]);
        for (int k = 0; k < 8; k++) SU32(ld, 4 * k, w[3 + k]);          /* e1 .. e8 */
        SU32(ld, 0x40, w[2]);                                           /* e0 */
        rx_st4(ld + 0x20, rx_ld4(ZERO_VEC)); rx_st4(ld + 0x30, rx_ld4(ZERO_VEC));
        SF32(ld, 0x44, 0.0f);
        int32_t parent = (int32_t)w[1];
        if (parent >= 0) {
            rx_addr prec = U32(ns, 8) + 16u * (uint32_t)parent;
            SU8(rec, 0xf, parent);
            SU8(rec, 0xd, U8(prec, 0xe));
            SU8(rec, 0xe, 0xff);
            SU8(prec, 0xe, i);
        } else {
            SU8(rec, 0xf, 0xff);
            SU32(ns, 0, rec);
            float e5 = rx_bf(w[7]), e6 = rx_bf(w[8]), e7 = rx_bf(w[9]);
            float lo = rx_bf(0xce6e6b28u) /* 0x977918 */, hi = rx_bf(0x4e6e6b28u) /* 0x977914 */;
            if (lo > e5 || e5 > hi || lo > e6 || e6 > hi || lo > e7 || e7 > hi) {
                rx_addr rl = U32(ns, 0x20) + 0x50u * U8(rec, 0xb);
                rx_st4(rl + 0x10, rx_ld4(ZERO_VEC)); rx_st4(rl, rx_ld4(ZERO_VEC));
            }
        }
    }
    rx_stream_read(s, U32(ns, 0x10), U32(ns, 0x14));
    if ((int32_t)U32(ns, 0xc) > 1) nodeset_inverse_worlds(ns);
}

/* ---- Node_UpdateSubtreeBounds 007dbde0 (ESI = record) and ModelAsset_RootBounds 007dd450 ----
 * Same transliteration as anim_node.c's rx_node_update_subtree_bounds (the node's subtree box in its own space), kept here because that
 * one starts its walk through the original's FUN_007dea70 (rx_call_original_ex), which the product cannot run; the walk start is done
 * inline (all records' visited bit cleared, the cursor set). */
static rx_v4 v4_max(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_maxss(a.v[i], b.v[i]); return r; }
static rx_v4 v4_min(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_minss(a.v[i], b.v[i]); return r; }
static int v4_eq3(rx_v4 a, rx_v4 b) {
    int32_t s = 0;
    for (int i = 0; i < 3; i++) s += a.v[i] == b.v[i] ? -1 : 0;
    return s == -3;
}
static void x87_copy_f32(rx_addr dst, rx_addr src) { SU32(dst, 0, x87_copy(U32(src, 0))); }

void rx_model_update_subtree_bounds(rx_addr rec) {
    if (U8(rec, 0xc) & 0xc) { SU8(rec, 0xc, U8(rec, 0xc) | 4); return; }
    SU8(rec, 0xc, U8(rec, 0xc) | 4);
    rx_addr h = U32(rec, 0);
#define BOUNDS(r) (U32(U32(r, 0), 0x20) + 0x50u * U8(r, 0xb))
    if (U8(rec, 0xe) == 0xff) {
        rx_st4(BOUNDS(rec) + 0x20, rx_ld4(BOUNDS(rec)));
        rx_st4(BOUNDS(rec) + 0x30, rx_ld4(BOUNDS(rec) + 0x10));
        x87_copy_f32(BOUNDS(rec) + 0x44, BOUNDS(rec) + 0x40);
        SU8(rec, 0xc, U8(rec, 0xc) | 4);
        return;
    }
    rx_world_node_evaluate_world(rec, 1);
    rx_addr w = U32(U32(rec, 0), 0x1c) + 0x40u * U8(rec, 0xb);
    rx_v4 r0 = rx_ld4(w), r1 = rx_ld4(w + 0x10), r2 = rx_ld4(w + 0x20), r3 = rx_ld4(w + 0x30);
    rx_v4 zero = rx_v4_splat(0.0f), w0 = rx_ld4(0x00d61660);
    rx_v4 neg_t = rx_v4_sub(zero, r3);
    rx_v4 c0 = {{r0.v[0], r1.v[0], r2.v[0], w0.v[0]}};
    rx_v4 c1 = {{r0.v[1], r1.v[1], r2.v[1], w0.v[0]}};
    rx_v4 c2 = {{r0.v[2], r1.v[2], r2.v[2], w0.v[0]}};
    float d0 = (float)(((double)r0.v[1] * neg_t.v[1] + (double)r0.v[0] * neg_t.v[0]) + (double)r0.v[2] * neg_t.v[2]);
    float d1 = (float)(((double)r1.v[1] * neg_t.v[1] + (double)r1.v[0] * neg_t.v[0]) + (double)r1.v[2] * neg_t.v[2]);
    float d2 = (float)(((double)r2.v[1] * neg_t.v[1] + (double)r2.v[0] * neg_t.v[0]) + (double)r2.v[2] * neg_t.v[2]);
    rx_v4 t = {{d0, d1, d2, F32(0, 0x00d6169c)}};               /* lane 3 from the 1.0 splat 0xd61690 */
    rx_v4 invalid = rx_ld4(ZERO_VEC);
    int empty = 1;
    rx_v4 lo, hi;
    rx_addr b = BOUNDS(rec);
    if (v4_eq3(rx_ld4(b + 0x10), invalid)) { lo = rx_ld4(0x00d615e0); hi = rx_ld4(0x00d615f0); }
    else { lo = rx_v4_sub(rx_ld4(b), rx_ld4(b + 0x10)); hi = rx_v4_add(rx_ld4(b + 0x10), rx_ld4(b)); empty = 0; }
    /* FUN_007dea70 (ECX = set, EDI = first): the visited bit of every record cleared, the cursor set */
    for (uint32_t k = 0; k < U32(h, 0xc); k++) SU8(U32(h, 8) + 16 * k, 0xc, U8(U32(h, 8) + 16 * k, 0xc) & 0xfd);
    SU32(h, 4, U32(h, 8) + 16u * U8(rec, 0xe));
    for (rx_addr n; (n = rx_nodeset_next_unvisited(U32(rec, 0))) != 0;) {
        rx_addr m = U32(U32(n, 0), 0x1c) + 0x40u * U8(n, 0xb);
        rx_v4 q0 = row_times(rx_ld4(m), c0, c1, c2, t), q1 = row_times(rx_ld4(m + 0x10), c0, c1, c2, t);
        rx_v4 q2 = row_times(rx_ld4(m + 0x20), c0, c1, c2, t), q3 = row_times(rx_ld4(m + 0x30), c0, c1, c2, t);
        rx_addr nb = U32(U32(n, 0), 0x20) + 0x50u * U8(n, 0xb);
        rx_v4 c = rx_ld4(nb), e = rx_ld4(nb + 0x10);
        rx_v4 center = rx_v4_add(rx_v4_add(rx_v4_mul(splat_lane(c, 2), q2),
                                           rx_v4_add(rx_v4_mul(splat_lane(c, 0), q0), rx_v4_mul(splat_lane(c, 1), q1))), q3);
        rx_v4 a1 = rx_v4_mul(splat_lane(e, 1), q1), a2 = rx_v4_mul(splat_lane(e, 2), q2), a0 = rx_v4_mul(splat_lane(e, 0), q0);
        rx_v4 ext = rx_v4_add(rx_v4_add(v4_max(rx_v4_sub(zero, a1), a1), v4_max(rx_v4_sub(zero, a0), a0)), v4_max(rx_v4_sub(zero, a2), a2));
        if (v4_eq3(ext, invalid)) continue;
        lo = v4_min(lo, rx_v4_sub(center, ext));
        hi = v4_max(hi, rx_v4_add(ext, center));
        empty = 0;
    }
    SU32(U32(rec, 0), 4, 0);
    SU32(U32(rec, 0), 0x30, 0);
    b = BOUNDS(rec);
    if (!empty) {
        rx_st4(b + 0x20, rx_v4_add(hi, lo));
        rx_st4(b + 0x30, rx_v4_sub(hi, lo));
        rx_st4(b + 0x20, rx_v4_mul(rx_v4_splat(0.5f /* 0x9322cc */), rx_ld4(b + 0x20)));
        rx_st4(b + 0x30, rx_v4_mul(rx_v4_splat(0.5f), rx_ld4(b + 0x30)));
        rx_v4 s = rx_ld4(b + 0x30);
        float sx = s.v[0] * s.v[0], sy = s.v[1] * s.v[1], sz = s.v[2] * s.v[2];
        SF32(b, 0x44, sqrtf((sx + sz) + sy));
    } else {
        rx_st4(b + 0x20, rx_ld4(ZERO_VEC));
        rx_st4(b + 0x30, rx_ld4(ZERO_VEC));
        SF32(b, 0x44, 0.0f);
    }
    SU8(rec, 0xc, U8(rec, 0xc) | 4);
#undef BOUNDS
}

/* ---- the ModelAsset ---- */

/* FUN_007d3f30 (ESI = model) */
static void model_construct(rx_addr m) {
    SU32(m, 0, 0x00972844);
    for (uint32_t o = 8; o <= 0x44; o += 4) SU32(m, o, 0);
    SU32(m, 0x4c, 0); SU16(m, 0x48, 0);
    for (uint32_t o = 0x50; o <= 0x8c; o += 4) SU32(m, o, 0);
    SU16(m, 0x88, 0);
    rx_ld_vec_init(m + 0x124);                     /* ReflexAlignedAlloc(4) proxy, begin / end / capacity 0 */
    SU32(m, 0x94, 0); SU8(m, 0x98, 0); SU32(m, 0x118, 0); SU32(m, 0x120, 0); SU32(m, 0x11c, 0); SU32(m, 0x90, 0xffffffffu);
}

/* FUN_007f91a0 (ECX = vector of 0x10-byte elements, stack n): reserve on an empty vector */
static void binding_reserve(rx_addr v, uint32_t n) {
    uint32_t cap = U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 4 : 0;
    if (cap >= n) return;
    rx_addr b = rx_reflex_aligned_alloc(rx_mul_sat(n, 0x10));
    SU32(v, 0x14, b + (n << 4)); SU32(v, 0x10, b); SU32(v, 0xc, b);
}

/* FUN_00416f10 (EAX = vector, stack element pointer): push_back within the reserved capacity */
static void binding_push(rx_addr v, const uint32_t e[4]) {
    uint32_t b = U32(v, 0xc), end = U32(v, 0x10);
    uint32_t cap = b ? (U32(v, 0x14) - b) >> 4 : 0, size = (end - b) >> 4;
    if (size < cap) {
        for (int k = 0; k < 4; k++) SU32(end, 4 * k, e[k]);
        SU32(v, 0x10, end + 0x10);
        return;
    }
    /* growth (FUN_00417780): capacity + capacity / 2, at least size + 1 */
    uint32_t ncap = cap + cap / 2; if (ncap < size + 1) ncap = size + 1;
    rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(ncap, 0x10));
    for (uint32_t k = 0; k < size * 16; k++) SU8(nb, k, U8(b, k));
    for (int k = 0; k < 4; k++) SU32(nb + size * 16, 4 * k, e[k]);
    if (b) rx_reflex_aligned_free(b);
    SU32(v, 0xc, nb); SU32(v, 0x10, nb + (size + 1) * 16); SU32(v, 0x14, nb + ncap * 16);
}

/* ModelAsset_LoadStatic 007d4650 (ECX = stream, stack: model, version) */
static void model_load_static(rx_addr self, rx_stream* s, uint32_t version) {
    uint32_t hw[0x2a];
    for (int k = 0; k < 0x2a; k++) hw[k] = 0;
    rx_stream_read_host(s, hw, version == 3 ? 0xa0 : 0xa4);
    uint32_t nn = hw[1], npool = hw[2], nl = hw[3];
    nodeset_load(self + 0x10, nn, npool, s);
    nodeset_copy_from(self + 0x50, self + 0x10);
    rx_model_update_subtree_bounds(U32(self + 0x50, 0));
    {   /* strncpy(+0x98, name, 0x7f); the name is the header's bytes 0x20.. */
        uint8_t nm[0x80]; for (int k = 0; k < 0x80; k++) nm[k] = ((const uint8_t*)hw)[0x20 + k < 0xa4 ? 0x20 + k : 0];
        uint32_t k = 0; for (; k < 0x7f && nm[k]; k++) SU8(self, 0x98 + k, nm[k]);
        for (; k < 0x7f; k++) SU8(self, 0x98 + k, 0);
        SU8(self, 0x117, 0);
    }
    SU32(self, 0x94, hw[0]);                  /* the flags word (the first header word) */
    rx_addr model_surface = rx_load_find_handle("surface", self + 0x98);
    SU32(self, 0x13c, model_surface);
    SU32(self, 0x118, nl);
    SU32(self, 0x120, version == 3 ? 0 : hw[40]);
    rx_addr lodtmp = 0, poolA = 0, bind = 0, poolB = 0, refs = 0;
    if (nl) { lodtmp = rx_reflex_aligned_alloc(rx_mul_sat(nl, 0xc)); for (uint32_t k = 0; k < nl; k++) { SU32(lodtmp, 12 * k, 0); SU32(lodtmp, 12 * k + 4, 0); SU32(lodtmp, 12 * k + 8, 0); } }
    if (hw[4]) poolA = rx_reflex_aligned_alloc(hw[4]);
    if (hw[6]) bind = rx_reflex_aligned_alloc(rx_mul_sat(hw[6], 0x14));
    if (hw[5]) poolB = rx_reflex_aligned_alloc(hw[5]);
    SU32(self, 0x11c, hw[6]);
    /* the LOD vector: nl default LodEntry's (one proxy block each) */
    if (nl) {
        rx_addr lods = rx_reflex_aligned_alloc(rx_mul_sat(nl, 0x1c));
        SU32(self, 0x130, lods); SU32(self, 0x134, lods + nl * 0x1c); SU32(self, 0x138, lods + nl * 0x1c);
        for (uint32_t k = 0; k < nl; k++) { SU32(lods, 0x1c * k, 0); rx_ld_vec_init(lods + 0x1c * k + 4); }
    }
    if (U32(self, 0x11c)) refs = rx_reflex_aligned_alloc(rx_mul_sat(U32(self, 0x11c), 4));
    if (U32(self, 0x118)) {
        rx_stream_read(s, lodtmp, nl * ((version != 3) * 4 + 8));
        rx_stream_read(s, bind, U32(self, 0x11c) * 0x14);
        rx_stream_read(s, poolA, hw[4]);
        rx_stream_read(s, poolB, hw[5]);
        rx_stream_read(s, refs, U32(self, 0x11c) * 4);
        for (uint32_t k = 0; k < U32(self, 0x118); k++) {
            uint32_t cnt = U32(lodtmp, 12 * k), first = U32(lodtmp, 12 * k + 4), dist = U32(lodtmp, 12 * k + 8);
            rx_addr lod = U32(self, 0x130) + 0x1c * k;
            binding_reserve(lod + 4, cnt);
            if ((int32_t)dist >= 0xffff) dist = 0xffff;
            SU32(lod, 0, dist);
            rx_addr hsurf = model_surface;
            for (int32_t j = 0; j < (int32_t)cnt; j++) {
                rx_addr r = bind + 0x14 * (first + (uint32_t)j);
                rx_addr sname = poolB + U32(r, 4), mname = poolA + U32(r, 0);
                rx_addr hs = rx_load_find_handle("surface", sname);
                if (hs) hsurf = hs;
                rx_addr hm = rx_load_find_handle("material", mname);
                if (!hm) hm = rx_load_find_handle_c("material", "Editor_Default");
                uint32_t e[4] = {U32(refs, 4 * (first + (uint32_t)j)), U32(r, 8), hsurf, hm};
                binding_push(lod + 4, e);
            }
        }
    }
    /* ModelAsset_RootBounds on the file's set, then the draw distance */
    rx_addr root = U32(self + 0x10, 0);
    rx_model_update_subtree_bounds(root);
    rx_addr rb = U32(U32(root, 0), 0x20) + 0x50u * U8(root, 0xb);
    uint32_t radius = x87_copy(U32(rb, 0x44));
    if (U32(self, 0x120) == 0) {
        double v = (double)rx_bf(radius) * 135.0;
        int64_t t = (v != v || v >= 9223372036854775808.0 || v < -9223372036854775808.0) ? INT64_MIN : (int64_t)v;
        SU32(self, 0x120, (uint32_t)t);
    }
    uint32_t d = U32(self, 0x120);
    if (d >= 0xffff) d = 0xffff;
    SU32(self, 0x120, d);
    rx_reflex_aligned_free(lodtmp); rx_reflex_aligned_free(poolA); rx_reflex_aligned_free(bind);
    rx_reflex_aligned_free(poolB); rx_reflex_aligned_free(refs);
}

/* ModelAssetIO::vf3 007d53c0 */
rx_addr rx_model_asset_io_create(rx_stream* s) {
    rx_addr m = rx_reflex_aligned_alloc(0x140);
    if (m) model_construct(m);
    uint32_t magic = rx_stream_u32(s), version = rx_stream_u32(s);
    (void)magic;
    if (!m) return 0;
    if (version == 1 || version == 2) { fprintf(stderr, "rxcore: model version %u is not ported (FUN_007d4f00 / ModelAsset_LoadVariant)\n", version); abort(); }
    model_load_static(m, s, version);
    return m;
}

/* ---- a node set extracted from a subtree ---------------------------------------------------------------------------------------- */

/* FUN_007dd890 (ESI = set): the set's storage released (the block at +0x3c and the one at +0x2c, each _aligned_free under the allocator lock)
 * and every field cleared (the words +0 .. +0x34, +0x3c, and the u16 at +0x38) */
void rx_nodeset_reset(rx_addr set) {
    rx_crt_aligned_free(U32(set, 0x3c));
    rx_crt_aligned_free(U32(set, 0x2c));
    for (uint32_t o = 0; o <= 0x34; o += 4) SU32(set, o, 0);
    SU32(set, 0x3c, 0); SU16(set, 0x38, 0);
}
RX_FUNCTION(0x007dd890, rx_nodeset_reset)

/* FUN_007dea70 (ECX = set, EDI = the first record to walk): every record's visited bit (flags bit 1) cleared and the walk cursor (+4) set */
void rx_nodeset_walk_from(rx_addr set, rx_addr start) {
    for (int32_t i = 0; i < I32(set, 0xc); i++) SU8(U32(set, 8), 0xc + 0x10u * (uint32_t)i, U8(U32(set, 8), 0xc + 0x10u * (uint32_t)i) & 0xfd);
    SU32(set, 4, start);
}
RX_FUNCTION(0x007dea70, rx_nodeset_walk_from)

/* the pool string of a record (name offset +8, 0xffff none) in its owner's pool (+0x10) */
static rx_addr record_name(rx_addr rec) { return U32(U32(rec, 0), 0x10) + U16(rec, 8); }
static uint32_t cstr_len(rx_addr s) { uint32_t n = 0; while (U8(s, n)) n++; return n; }

/* FUN_007ddfa0 (ECX = source set, stack destination set, map; RET 8): the source nodes whose map entry (one dword per source node) is not
 * negative become the destination's nodes at that index (matrices, bounds, records re-linked through the map, names copied into a new pool);
 * the source's extra nodes (u16 at +0x38) follow the selected ones. A destination that has nodes is released first; a fresh one gets one
 * 128-aligned block laid out as nodeset_block_size (inverse world / spare matrices only above one selected node). The root is the
 * selected root, else the first record without a parent; the walk cursor is mapped likewise; the root's local (and inverse world) matrix is
 * the identity. */
void rx_nodeset_extract(rx_addr src, rx_addr dst, rx_addr map) {
    const int32_t count = I32(src, 0xc);
    const int32_t extra = (int32_t)U16(src, 0x38);
    const int32_t total = count + extra;
    uint32_t chars = 0; int32_t sel = 0;
    for (int32_t i = 0; i < count; i++) {
        if (I32(map, 4u * (uint32_t)i) < 0) continue;
        sel++;
        const rx_addr rec = U32(src, 8) + 16u * (uint32_t)i;
        if (U16(rec, 8) != 0xffff) chars += cstr_len(record_name(rec)) + 1;
    }
    if (U16(src, 0x38) != 0) {
        const rx_addr root = U32(src, 0);
        if (U16(root, 8) != 0xffff) chars += cstr_len(record_name(root)) + 1;
    }
    const int32_t n = extra + sel;
    if (U32(dst, 0xc) != 0) rx_nodeset_reset(dst);
    if (U32(dst, 0xc) == 0) {
        const uint32_t inverse = n > 1 ? (uint32_t)n * 0x80u : 0;
        const uint32_t matrices = (uint32_t)n * 0x40u;
        const uint32_t size = ((chars + (uint32_t)n * 0xe0u + inverse + 0xfu) >> 4) << 4;
        rx_addr blk = rx_crt_aligned_malloc(size, 0x80);
        SU16(dst, 0x3a, size); SU32(dst, 0x3c, blk);
        if (sel > 1) { SU32(dst, 0x28, blk); SU32(dst, 0x24, blk + matrices); blk += 2 * matrices; }
        SU32(dst, 0x18, blk); SU32(dst, 0x1c, blk + matrices); blk += 2 * matrices;
        SU32(dst, 0x20, blk); blk += (uint32_t)n * 0x50u;
        SU32(dst, 0x10, blk + (uint32_t)n * 0x10u); SU32(dst, 8, blk); SU32(dst, 0x14, chars);
    }
    SU32(dst, 0xc, (uint32_t)sel);
    SU16(dst, 0x38, U16(src, 0x38));
    for (int32_t j = 0; j < n; j++) {
        const rx_addr r = U32(dst, 8) + 16u * (uint32_t)j;
        init_record(r);
        SU32(r, 0, dst); SU8(r, 0xb, (uint32_t)j);
    }
    {   /* the root */
        int32_t m = -1;
        const rx_addr root = U32(src, 0);
        if (root != 0 && (int32_t)U8(root, 0xb) < count) m = I32(map, 4u * U8(root, 0xb));
        if (root != 0 && (int32_t)U8(root, 0xb) < count && m >= 0) SU32(dst, 0, (uint32_t)m * 0x10u + U32(dst, 8));
        else if (U16(dst, 0x38) != 0) SU32(dst, 0, U32(dst, 0xc) * 0x10u + U32(dst, 8));
    }
    uint32_t pool = 0;
    for (int32_t i = 0; i < total; i++) {
        int32_t m;
        if (i < count) { m = I32(map, 4u * (uint32_t)i); if (m < 0) continue; }
        else m = (int32_t)U32(dst, 0xc);
        const uint32_t d16 = (uint32_t)m * 0x40u, s16 = (uint32_t)i * 0x40u;
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(dst, 0x18) + d16, k, U32(U32(src, 0x18) + s16, k));
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(dst, 0x1c) + d16, k, U32(U32(src, 0x1c) + s16, k));
        for (uint32_t k = 0; k < 0x50; k += 4) SU32(U32(dst, 0x20) + (uint32_t)m * 0x50u, k, U32(U32(src, 0x20) + (uint32_t)i * 0x50u, k));
        if (U32(dst, 0xc) > 1) {
            if (U32(src, 0x24) != 0) for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(dst, 0x24) + d16, k, U32(U32(src, 0x24) + s16, k));
            if (U32(src, 0x28) != 0) for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(dst, 0x28) + d16, k, U32(U32(src, 0x28) + s16, k));
        }
        const rx_addr dr = U32(dst, 8) + 16u * (uint32_t)m, sr = U32(src, 8) + 16u * (uint32_t)i;
        SU8(dr, 0xc, U8(sr, 0xc));
        const uint32_t parent = U8(sr, 0xf);
        if (parent != 0xff) {
            const int32_t pm = I32(map, 4 * parent);
            if (pm < 0) { if (U32(dst, 0) != 0) SU8(dr, 0xf, U8(U32(dst, 0), 0xb)); }
            else SU8(dr, 0xf, (uint8_t)pm);
            if (U8(dr, 0xf) != 0xff) {
                rx_addr p = U32(dst, 8) + 16u * U8(dr, 0xf);
                if (U8(p, 0xe) == 0xff) SU8(p, 0xe, U8(dr, 0xb));
                else {
                    p = U32(dst, 8) + 16u * U8(p, 0xe);
                    while (U8(p, 0xd) != 0xff) p = U32(dst, 8) + 16u * U8(p, 0xd);
                    SU8(p, 0xd, U8(dr, 0xb));
                }
            }
        }
        if (U16(sr, 8) == 0xffff) SU16(dr, 8, 0xffff);
        else {
            const rx_addr name = record_name(sr);
            const uint32_t room = U32(dst, 0x14) - 1 - pool;
            crt_strncpy(U32(dst, 0x10) + pool, name, room);
            SU8(U32(dst, 0x10) + pool, room, 0);
            SU16(dr, 8, pool);
            pool += cstr_len(name) + 1;
        }
    }
    if (U32(dst, 0) == 0) {
        for (int32_t j = 0; j < n; j++) if (U8(U32(dst, 8) + 16u * (uint32_t)j, 0xf) == 0xff) { SU32(dst, 0, U32(dst, 8) + 16u * (uint32_t)j); break; }
    }
    SU32(dst, 4, 0);
    const rx_addr cursor = U32(src, 4);
    if (cursor != 0) {
        if ((int32_t)U8(cursor, 0xb) < count && I32(map, 4u * U8(cursor, 0xb)) >= 0) SU32(dst, 4, (uint32_t)I32(map, 4u * U8(cursor, 0xb)) * 0x10u + U32(dst, 8));
        else if (cursor == U32(src, 0)) SU32(dst, 4, U32(dst, 0));
    }
    rx_addr root = U32(dst, 0);
    if (U16(dst, 0x38) != 0 && U8(root, 0xe) != 0xff) root = U32(dst, 8) + 16u * U8(root, 0xe);
    {
        const rx_addr set = U32(root, 0);
        for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(set, 0x18) + 0x40u * U8(root, 0xb), k, U32(IDENT, k));
        if (U32(dst, 0x24) != 0) for (uint32_t k = 0; k < 0x40; k += 4) SU32(U32(set, 0x24) + 0x40u * U8(root, 0xb), k, U32(IDENT, k));
    }
    SU32(dst, 0x34, 0);
}
RX_FUNCTION(0x007ddfa0, rx_nodeset_extract)

/* 007dda20 (ESI = source set, stack destination set, node name; RET 8): the subtree of the named node of the source copied into the
 * destination (a set without nodes). The map gives the root of the subtree 0 and its descendants 1, 2, ... in walk order (the walk
 * 007deaa0 starts at the node's last child, +0xe); every other node -1. An unknown name copies nothing. The destination's records
 * then lose their "world valid" (1) and "dirty" (4) flags. */
void rx_nodeset_extract_subtree(rx_addr src, rx_addr dst, rx_addr name) {
    if (I32(src, 0xc) > 0) {
        SU32(dst, 0x30, 0);
        rx_world_node_evaluate_world(U32(src, 0), 1);
        if (U32(dst, 0xc) == 0) {
            const rx_addr node = rx_hierarchy_find_node(src, name);
            if (node == 0) return;
            const rx_addr map = rx_reflex_aligned_alloc(rx_mul_sat(U32(src, 0xc), 4));
            crt_memset(map, -1, U32(src, 0xc) * 4);
            SU32(map, 4u * U8(node, 0xb), 0);
            uint32_t next = 1;
            if (U8(node, 0xe) != 0xff) {
                rx_nodeset_walk_from(src, U32(node, 0) ? U32(U32(node, 0), 8) + 16u * U8(node, 0xe) : 0);
                for (rx_addr r = rx_nodeset_next_unvisited(src); r != 0; r = rx_nodeset_next_unvisited(src)) SU32(map, 4u * U8(r, 0xb), next++);
                SU32(src, 4, 0); SU32(src, 0x30, 0);
            }
            rx_nodeset_extract(src, dst, map);
            rx_reflex_aligned_free(map);
        }
        const int32_t n = (int32_t)U16(dst, 0x38) + I32(dst, 0xc);
        for (int32_t j = 0; j < n; j++) {
            const rx_addr f = U32(dst, 8) + 0xc + 0x10u * (uint32_t)j;
            SU8(f, 0, U8(f, 0) & 0xfe);
            SU8(f, 0, U8(f, 0) & 0xfb);
        }
    }
}
RX_FUNCTION(0x007dda20, rx_nodeset_extract_subtree)
