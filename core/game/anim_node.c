/* anim_node.c -- node hierarchy helpers used by the animation code (0x007dxxxx).
 * Node record (16 bytes): +0x00 hierarchy, +0x0b local matrix index, +0x0c flags (bit 0 world valid, bit 2), +0x0d / +0x0e /
 * +0x0f child / first child / next sibling indices (0xff = none). Hierarchy: +0x08 records, +0x18 local matrices (0x40 each, rows
 * x y z translation), +0x34 dirty. (core/notes/anim.md) */
#include "anim.h"
#include <string.h>
#include "anim_extern.h"
#include "crt_util.h"
#include "build_model.h"
#include "loop_platform.h"

static rx_addr node_local_matrix(rx_addr rec) { return U32(U32(rec, 0), 0x18) + 0x40u * U8(rec, 0xb); }

/* 007dcdf0 Node_SetLocalRotation (ESI = record, arg = quaternion x y z w; RET 4): the record's local rotation rows from the unit
 * quaternion. The whole matrix is written with translation = the global 0xa95d50 vector (w 1) while Node_Invalidate runs, then the
 * old translation row is put back. */
void rx_node_set_local_rotation(rx_addr rec, rx_addr quat) {
    ANIM_FRAME_ENTER(A, 0x50);                                /* push ebp; and esp, -16; sub esp, 0x50 */
    const rx_addr L = A - 0x50;                               /* the products wz wy xx wx, then the matrix rows */
    float x = F32(quat, 0), y = F32(quat, 4), z = F32(quat, 8), w = F32(quat, 0xc);
    float x2 = x * 2.0f /* 0x92e330 */, y2 = y * 2.0f, z2 = z * 2.0f;
    float zz = z * z2, wx = w * x2, wz = w * z2, wy = w * y2, xx = x * x2, yy = y * y2, xy = y * x2, yz = z * y2, xz = z * x2;
    rx_addr m = node_local_matrix(rec);
    rx_v4 old_t = rx_ld4(m + 0x30);
    rx_v4 r0 = {{1.0f /* 0x931f24 */ - (zz + yy), xy + wz, xz - wy, 0.0f}};
    rx_v4 r1 = {{xy - wz, 1.0f - (zz + xx), yz + wx, 0.0f}};
    rx_v4 r2 = {{xz + wy, yz - wx, 1.0f - (yy + xx), 0.0f}};
    rx_v4 r3 = {{F32(0, 0xa95d50), F32(0, 0xa95d54), F32(0, 0xa95d58), 1.0f}};
    SF32(L, 0, wz); SF32(L, 4, wy); SF32(L, 8, xx); SF32(L, 0xc, wx);
    rx_st4(L + 0x10, r0); rx_st4(L + 0x20, r1); rx_st4(L + 0x30, r2); rx_st4(L + 0x40, r3);
    rx_st4(m, r0); rx_st4(m + 0x10, r1); rx_st4(m + 0x20, r2); rx_st4(m + 0x30, r3);
    rx_regs r = anim_regs(); r.r[ANIM_ESI] = rec;
    anim_node_invalidate_from(rec, &r, 0x007dcf95);
    rx_st4(node_local_matrix(rec) + 0x30, old_t);
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x007dcdf0, rx_node_set_local_rotation)

/* 007dd160 Node_SetLocalTranslation (ECX = record, stack = vec3; RET 4): the local matrix's translation row, then Node_Invalidate. */
void rx_node_set_local_translation(rx_addr rec, rx_addr v) {
    float x = F32(v, 0), y = F32(v, 4), z = F32(v, 8);
    rx_addr m = node_local_matrix(rec);
    SF32(m, 0x30, x); SF32(m, 0x34, y); SF32(m, 0x38, z);
    anim_node_invalidate(rec);
}
RX_FUNCTION(0x007dd160, rx_node_set_local_translation)

/* 007dd380 Node_LocalRotation (EAX = record, ESI = quaternion out): the quaternion of the record's local rotation rows. */
void rx_node_local_rotation(rx_addr rec, rx_addr out) { anim_matrix_to_quat(node_local_matrix(rec), out); }
RX_FUNCTION(0x007dd380, rx_node_local_rotation)

/* 007deaa0 NodeSet_NextUnvisited (EDI = walk: +0x04 cursor): returns the cursor record and advances the depth-first walk: marks it
 * visited (flags bit 1), then its first child (+0x0e), its next sibling (+0x0d), or the first unvisited sibling of an ancestor
 * (climbing +0x0f while the ancestors are visited); none: 0. */
rx_addr rx_nodeset_next_unvisited(rx_addr walk) {
    return rx_nodeset_next_unvisited_regs(walk, anim_reg_get(ANIM_EBX), anim_reg_get(ANIM_ESI));
}
RX_FUNCTION(0x007deaa0, rx_nodeset_next_unvisited)

/* The same with the caller's EBX / ESI given (the words it pushes), for callers that know the original's registers at the call
 * (Node_EvaluateWorld 007db890). Entered with the stack pointer at the return slot. */
rx_addr rx_nodeset_next_unvisited_regs(rx_addr walk, uint32_t caller_ebx, uint32_t caller_esi) {
    const rx_addr sp0 = rx_sp();                              /* its pushes (dead stack read back later): ebx, then esi to climb */
    rx_addr cur = U32(walk, 4);
    if (cur == 0) return 0;
    SU32(sp0 - 4, 0, caller_ebx);
    SU8(cur, 0xc, U8(cur, 0xc) | 2);
    SU32(walk, 4, 0);
#define NODE_AT(r, link) (U32(U32(r, 0), 8) + 16u * U8(r, link))
    if (U8(cur, 0xe) != 0xff && !(U8(NODE_AT(cur, 0xe), 0xc) & 2)) { SU32(walk, 4, NODE_AT(cur, 0xe)); return cur; }
    if (U8(cur, 0xd) != 0xff && !(U8(NODE_AT(cur, 0xd), 0xc) & 2)) { SU32(walk, 4, NODE_AT(cur, 0xd)); return cur; }
    SU32(sp0 - 8, 0, caller_esi);
    rx_addr p = cur;
    while (U8(p, 0xf) != 0xff) {
        p = NODE_AT(p, 0xf);
        if (!(U8(p, 0xc) & 2)) break;
        for (rx_addr q = p; U8(q, 0xd) != 0xff;) {
            q = NODE_AT(q, 0xd);
            if (!(U8(q, 0xc) & 2)) { SU32(walk, 4, q); break; }
        }
        if (U32(walk, 4) != 0) break;
    }
#undef NODE_AT
    return cur;
}

/* 007ddb40 (not in the list): NodeSet_Init: ECX = destination, EDX = source, stack flag 0; RET 4. The product runs the loader's
 * port of it (rx_nodeset_clone, build_model.c, verified there against the original), with the flag pushed and the return address
 * as the original's call leaves them; the oracle build keeps calling the original (so the rx_anim oracle does not link the loader). */
static void nodeset_init_from(rx_addr dst, rx_addr src) {
#ifdef RX_ORACLE
    rx_regs r = anim_regs(); r.r[ANIM_ECX] = dst; r.r[ANIM_EDX] = src; uint32_t a = 0;
    rx_call_original_ex(0x007ddb40, &r, 0, 1, &a);
    anim_ret_slot(4, 0x007dd96e);
#else
    ANIM_PUSH(0); ANIM_PUSH(0x007dd96e);
    rx_nodeset_clone(dst, src, 0);
    rx_set_sp(rx_sp() + 8);
#endif
}

/* 007dd930 NodeSet_CopyFrom (ESI = destination, EDI = source): the source hierarchy evaluated, then its node records' flags (world
 * valid / bit 2 cleared), local (+0x18) and world (+0x1c) matrices, bounds (+0x20, 0x50 each) and the optional +0x24 / +0x28
 * matrix arrays copied for count + extra (+0x0c + u16 +0x38) nodes (the destination initialised from the source first if empty). */
/* One of the copies as the original makes it: memcpy's three arguments and the return address on the game stack, and memcpy's own
 * frame (crt_memcpy writes it from crt_caller_regs) with this function's registers: EBX = n * 0x40, EBP its frame, ESI the
 * destination set, EDI the source set. (A light event record built later at the same depth picks up the size argument.) */
static void copy_block(rx_addr d, rx_addr s, uint32_t bytes, rx_addr ret, const uint32_t regs[4]) {
    const rx_addr sp = rx_sp();
    SU32(sp, -4, bytes); SU32(sp, -8, s); SU32(sp, -0xc, d); SU32(sp, -0x10, ret);
    const uint32_t saved[4] = {crt_caller_regs[3], crt_caller_regs[5], crt_caller_regs[6], crt_caller_regs[7]};
    crt_caller_regs[3] = regs[0]; crt_caller_regs[5] = regs[1]; crt_caller_regs[6] = regs[2]; crt_caller_regs[7] = regs[3];
    crt_memcpy(d, s, bytes);
    crt_caller_regs[3] = saved[0]; crt_caller_regs[5] = saved[1]; crt_caller_regs[6] = saved[2]; crt_caller_regs[7] = saved[3];
}
void rx_nodeset_copy_from(rx_addr dst, rx_addr src) {
    /* the original's frame: push ebp; and esp, -8; push ecx (the record count local); push ebx */
    const rx_addr sp0 = rx_sp(), A8 = (sp0 - 4u) & ~7u, count = A8 - 4;
    SU32(sp0 - 4, 0, anim_reg_get(ANIM_EBP));
    SU32(A8 - 8, 0, anim_reg_get(ANIM_EBX));
    rx_set_sp(A8 - 8);
    if (I32(src, 0xc) > 0) {
        SU32(dst, 0x30, 0);
        uint32_t n = U16(src, 0x38) + U32(src, 0xc);
        SU32(count, 0, n);
        anim_node_evaluate_world(U32(src, 0), 1, 0x007dd95d);
        if (U32(dst, 0xc) == 0) nodeset_init_from(dst, src);
        const uint32_t regs[4] = {n << 6, sp0 - 4, dst, src};
        copy_block(U32(dst, 0x18), U32(src, 0x18), n << 6, 0x007dd97f, regs);
        copy_block(U32(dst, 0x1c), U32(src, 0x1c), n << 6, 0x007dd990, regs);
        copy_block(U32(dst, 0x20), U32(src, 0x20), n * 0x50u, 0x007dd9ab, regs);
        if (U32(src, 0x24)) copy_block(U32(dst, 0x24), U32(src, 0x24), n << 6, 0x007dd9c0, regs);
        if (U32(src, 0x28)) copy_block(U32(dst, 0x28), U32(src, 0x28), n << 6, 0x007dd9d5, regs);
        for (int32_t i = 0; i < (int32_t)n; i++) {
            SU8(U32(dst, 8), 16u * (uint32_t)i + 0xc, U8(U32(src, 8), 16u * (uint32_t)i + 0xc));
            SU8(U32(dst, 8), 16u * (uint32_t)i + 0xc, U8(U32(dst, 8), 16u * (uint32_t)i + 0xc) & 0xfe);
            SU8(U32(dst, 8), 16u * (uint32_t)i + 0xc, U8(U32(dst, 8), 16u * (uint32_t)i + 0xc) & 0xfb);
        }
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007dd930, rx_nodeset_copy_from)

/* 007de980 Hierarchy_HasNode (ESI = hierarchy: +0x08 records, +0x0c count; stack name; RET 4): 1 when a record's name (u16
 * offset +0x08 into its owner's string table +0x10; 0xffff none) equals the name (_stricmp), else 0. */
uint32_t rx_hierarchy_has_node(rx_addr h, rx_addr name) {
    for (int32_t i = 0; i < I32(h, 0xc); i++) {
        rx_addr rec = U32(h, 8) + 16u * (uint32_t)i;
        if (U16(rec, 8) == 0xffff) continue;
        if (anim_stricmp(U16(rec, 8) + U32(U32(rec, 0), 0x10), name) == 0) return 1;
    }
    return 0;
}
RX_FUNCTION(0x007de980, rx_hierarchy_has_node)

/* 007de9e0 Hierarchy_FindNode (EDI = hierarchy, stack name; RET 4): the record of that name (as above), or 0; clears +0x30. */
rx_addr rx_hierarchy_find_node(rx_addr h, rx_addr name) {
    SU32(h, 0x30, 0);
    for (int32_t i = 0; i < I32(h, 0xc); i++) {
        rx_addr rec = U32(h, 8) + 16u * (uint32_t)i;
        if (U16(rec, 8) == 0xffff) continue;
        if (anim_stricmp(U16(rec, 8) + U32(U32(rec, 0), 0x10), name) == 0) return U32(h, 8) + 16u * (uint32_t)i;
    }
    return 0;
}
RX_FUNCTION(0x007de9e0, rx_hierarchy_find_node)

/* 007df2e0 Octree_ReplaceItem (ECX = octree node: +0x20 item count, +0x24..+0x40 eight children, +0x44 items; args new, old;
 * RET 8): in every leaf below the node, the first item == old becomes new. */
void rx_octree_replace_item(rx_addr node, rx_addr item_new, rx_addr item_old) {
    while (U32(node, 0x24) != 0) {
        for (int k = 0; k < 7; k++) rx_octree_replace_item(U32(node, 0x24 + 4 * k), item_new, item_old);
        node = U32(node, 0x40);
    }
    for (uint32_t i = 0; i < U32(node, 0x20); i++)
        if (U32(node, 0x44 + 4 * i) == item_old) { SU32(node, 0x44 + 4 * i, item_new); return; }
}
RX_FUNCTION(0x007df2e0, rx_octree_replace_item)

/* 007dea70 (world, inlined here: a leaf that only touches the records): starts a depth-first walk of the hierarchy: every
 * record's visited bit (flags +0x0c bit 1) cleared, the cursor (+0x04) = first. */
static void hierarchy_walk_start(rx_addr h, rx_addr first) {
    for (int32_t k = 0; k < I32(h, 0xc); k++) SU8(U32(h, 8) + 16u * (uint32_t)k, 0xc, U8(U32(h, 8) + 16u * (uint32_t)k, 0xc) & 0xfd);
    SU32(h, 4, first);
}

static rx_v4 v4_splat_lane(rx_v4 a, int i) { return rx_v4_splat(a.v[i]); }
/* cmpeqps: all ones in the lanes that compare equal */
static rx_v4 v4_eq_mask(rx_v4 a, rx_v4 b) {
    rx_v4 r; for (int i = 0; i < 4; i++) { uint32_t m = a.v[i] == b.v[i] ? 0xffffffffu : 0; memcpy(&r.v[i], &m, 4); } return r;
}
static rx_v4 node_v4_max(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_maxss(a.v[i], b.v[i]); return r; }
static rx_v4 node_v4_min(rx_v4 a, rx_v4 b) { rx_v4 r; for (int i = 0; i < 4; i++) r.v[i] = rx_minss(a.v[i], b.v[i]); return r; }
/* cmpeqps on lanes x y z, summed as integers == -3: all three equal. */
static int v4_eq3(rx_v4 a, rx_v4 b) {
    int32_t s = 0;
    for (int i = 0; i < 3; i++) s += a.v[i] == b.v[i] ? -1 : 0;
    return s == -3;
}
/* A row of a matrix times the 4x4 (c0, c1, c2, t), as the original pairs it: (w t + y c1) + (z c2 + x c0). */
static rx_v4 row_times(rx_v4 m, rx_v4 c0, rx_v4 c1, rx_v4 c2, rx_v4 t) {
    rx_v4 a = rx_v4_add(rx_v4_mul(v4_splat_lane(m, 3), t), rx_v4_mul(v4_splat_lane(m, 1), c1));
    rx_v4 b = rx_v4_add(rx_v4_mul(v4_splat_lane(m, 2), c2), rx_v4_mul(v4_splat_lane(m, 0), c0));
    return rx_v4_add(a, b);
}

/* 007dbde0 Node_UpdateSubtreeBounds (ESI = record): the node's subtree box (bounds +0x20 center, +0x30 half extents, +0x44 radius)
 * in its own space: a leaf copies its own box (+0x00 / +0x10 / +0x40); otherwise the world matrices are brought up to date and
 * every node below is walked (depth first), its own box transformed by its world matrix times the inverse of this node's
 * (rotation transposed, translation -t R^T in double precision) and merged; boxes whose extents equal the "invalid" marker
 * 0xd61670 are skipped (no box at all: the marker and radius 0). Marks the record's bounds valid (flags bit 2). */
void rx_node_update_subtree_bounds(rx_addr rec) {
    /* push ebp; and esp, -16; sub esp, 0xc8; push ebx, edi (the caller's), on every path */
    ANIM_FRAME_ENTER(A, 0xd0);
    SU32(A - 0xcc, 0, anim_reg_get(ANIM_EBX)); SU32(A - 0xd0, 0, anim_reg_get(ANIM_EDI));
    if (U8(rec, 0xc) & 0xc) { SU8(rec, 0xc, U8(rec, 0xc) | 4); ANIM_FRAME_LEAVE(A); return; }
    SU8(rec, 0xc, U8(rec, 0xc) | 4);
    rx_addr h = U32(rec, 0);
#define BOUNDS(r) (U32(U32(r, 0), 0x20) + 0x50u * U8(r, 0xb))
    if (U8(rec, 0xe) == 0xff) {
        rx_st4(BOUNDS(rec) + 0x20, rx_ld4(BOUNDS(rec)));
        rx_st4(BOUNDS(rec) + 0x30, rx_ld4(BOUNDS(rec) + 0x10));
        anim_x87_copy_f32(BOUNDS(rec) + 0x44, BOUNDS(rec) + 0x40);
        SU8(rec, 0xc, U8(rec, 0xc) | 4);
        ANIM_FRAME_LEAVE(A);
        return;
    }
    anim_node_evaluate_world(rec, 1, 0x007dbe62);
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
    rx_v4 invalid = rx_ld4(0x00d61670);
    int empty = 1;
    rx_v4 lo, hi;
    rx_addr b = BOUNDS(rec);
    if (v4_eq3(rx_ld4(b + 0x10), invalid)) { lo = rx_ld4(0x00d615e0); hi = rx_ld4(0x00d615f0); }
    else { lo = rx_v4_sub(rx_ld4(b), rx_ld4(b + 0x10)); hi = rx_v4_add(rx_ld4(b + 0x10), rx_ld4(b)); empty = 0; }
    /* the original's locals as it leaves them (dead stack that a later light record copies): base = its ESP */
    const rx_addr base = A - 0xd0;
    {
        rx_v4 col1 = {{r0.v[1], r1.v[1], r2.v[1], r3.v[1]}};
        rx_st4(base + 0x30, zero); rx_st4(base + 0x50, col1);
        rx_st4(base + 0x10, c2); rx_st4(base + 0x20, c1); rx_st4(base + 0xc0, c0); rx_st4(base + 0x90, t);
        rx_st4(base + 0x80, v4_eq_mask(rx_ld4(b + 0x10), invalid));
        rx_st4(base + 0xa0, lo); rx_st4(base + 0xb0, hi);
    }
    /* the walk start and the walk steps as the original calls them: their return addresses, and the registers they save (the
     * walk start ESI = the record; a step EBX = the "nothing yet" flag, ESI = the record) */
    hierarchy_walk_start(h, U32(h, 8) + 16u * U8(rec, 0xe));
    SU32(A - 0xd4, 0, 0x007dc201);
    if (I32(h, 0xc) > 0) SU32(A - 0xd8, 0, rec);
    const uint32_t ebx0 = anim_reg_get(ANIM_EBX), esi0 = anim_reg_get(ANIM_ESI);
    anim_reg_set(ANIM_ESI, rec);
    for (rx_addr n, ret = 0x007dc208;; ret = 0x007dc41a) {
        anim_reg_set(ANIM_EBX, (uint32_t)empty);
        ANIM_PUSH(ret);
        n = rx_nodeset_next_unvisited(U32(rec, 0));
        rx_set_sp(rx_sp() + 4);
        if (n == 0) break;
        rx_addr m = U32(U32(n, 0), 0x1c) + 0x40u * U8(n, 0xb);
        rx_v4 q0 = row_times(rx_ld4(m), c0, c1, c2, t), q1 = row_times(rx_ld4(m + 0x10), c0, c1, c2, t);
        rx_v4 q2 = row_times(rx_ld4(m + 0x20), c0, c1, c2, t), q3 = row_times(rx_ld4(m + 0x30), c0, c1, c2, t);
        rx_addr nb = U32(U32(n, 0), 0x20) + 0x50u * U8(n, 0xb);
        rx_v4 c = rx_ld4(nb), e = rx_ld4(nb + 0x10);
        rx_st4(base + 0xc0, c);
        rx_v4 center = rx_v4_add(rx_v4_add(rx_v4_mul(v4_splat_lane(c, 2), q2),
                                           rx_v4_add(rx_v4_mul(v4_splat_lane(c, 0), q0), rx_v4_mul(v4_splat_lane(c, 1), q1))), q3);
        rx_v4 a1 = rx_v4_mul(v4_splat_lane(e, 1), q1), a2 = rx_v4_mul(v4_splat_lane(e, 2), q2), a0 = rx_v4_mul(v4_splat_lane(e, 0), q0);
        rx_v4 ext = rx_v4_add(rx_v4_add(node_v4_max(rx_v4_sub(zero, a1), a1), node_v4_max(rx_v4_sub(zero, a0), a0)), node_v4_max(rx_v4_sub(zero, a2), a2));
        rx_st4(base + 0x80, v4_eq_mask(ext, invalid));
        if (v4_eq3(ext, invalid)) continue;
        lo = node_v4_min(lo, rx_v4_sub(center, ext));
        hi = node_v4_max(hi, rx_v4_add(ext, center));
        rx_st4(base + 0xa0, lo); rx_st4(base + 0xb0, hi);
        empty = 0;
    }
    anim_reg_set(ANIM_EBX, ebx0); anim_reg_set(ANIM_ESI, esi0);
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
        rx_st4(b + 0x20, rx_ld4(0x00d61670));
        rx_st4(b + 0x30, rx_ld4(0x00d61670));
        SF32(b, 0x44, 0.0f);
    }
    SU8(rec, 0xc, U8(rec, 0xc) | 4);
#undef BOUNDS
    ANIM_FRAME_LEAVE(A);
}
RX_FUNCTION(0x007dbde0, rx_node_update_subtree_bounds)

/* 007dd450 Node_SubtreeBounds (EAX = record, EDI = half extents out; stack: center out, radius out; RET 8): updates the subtree
 * box, then copies center / extents / radius out (through the x87). */
void rx_node_subtree_bounds(rx_addr rec, rx_addr ext_out, rx_addr center_out, rx_addr radius_out) {
    /* push ebx (the caller's), push esi (the caller's; then ESI = the record), call: the return address on the stack */
    const rx_addr sp0 = rx_sp();
    ANIM_PUSH(anim_reg_get(ANIM_EBX)); ANIM_PUSH(anim_reg_get(ANIM_ESI)); ANIM_PUSH(0x007dd45d);
    const uint32_t esi = anim_reg_get(ANIM_ESI), ebx = anim_reg_get(ANIM_EBX);
    anim_reg_set(ANIM_ESI, rec); anim_reg_set(ANIM_EBX, center_out);
    rx_node_update_subtree_bounds(rec);
    anim_reg_set(ANIM_ESI, esi); anim_reg_set(ANIM_EBX, ebx);
    rx_set_sp(sp0);
    rx_addr b = U32(U32(rec, 0), 0x20) + 0x50u * U8(rec, 0xb);
    for (int i = 0; i < 3; i++) anim_x87_copy_f32(center_out + 4u * (uint32_t)i, b + 0x20 + 4u * (uint32_t)i);
    for (int i = 0; i < 3; i++) anim_x87_copy_f32(ext_out + 4u * (uint32_t)i, b + 0x30 + 4u * (uint32_t)i);
    anim_x87_copy_f32(radius_out, b + 0x44);
}
RX_FUNCTION(0x007dd450, rx_node_subtree_bounds)

#ifndef RX_ORACLE
/* The product build's register file behind anim_reg_get / anim_reg_set (anim_extern.h): the frame pointer the frame macros keep
 * (dead data there; the oracle build reads and writes the emulated CPU instead, oracle/anim_oracle.cpp). */
/* the register model: the running guest thread's (rx_cpu, w-jobthreads) */
uint32_t anim_reg_get(int i) { return rx_cpu_cur()->anim_regs[i]; }
void anim_reg_set(int i, uint32_t v) { rx_cpu_cur()->anim_regs[i] = v; }
#endif

/* 007dd4d0 (EDI = the node set): its element array (+0x3c) and its second block (+0x2c) freed (_aligned_free 0x916350), each
 * under the allocator lock 0xd61704 while 0xda67d8 is set. Frame: push ebx; push ebp; push esi; EBP = [EnterCriticalSection],
 * EBX = [_aligned_free], ESI = the block; the calls' arguments and return addresses (007dd4ec / 007dd4f5 / 007dd50c, 007dd51f /
 * 007dd522; the last leave after the pops: 007dd53c) and _aligned_free's / free's frames as the original leaves them (the
 * package unload's model destruction runs this on the database thread, whose dead stack stays: crt_frames.c). Entered with the
 * stack pointer at the return slot. */
static void nodeset_cs(rx_addr e, int enter, rx_addr ret) {
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 8);                         /* the import call pushes the argument at e + 4 and a return slot at e */
    if (enter) loop_enter_cs(0x00d61704); else loop_leave_cs(0x00d61704);
    SU32(e, 4, 0x00d61704); SU32(e, 0, ret);
    rx_set_sp(sp);
}
void rx_nodeset_release(rx_addr set) {
    const rx_addr E = rx_sp();
    const uint32_t ebx = anim_reg_get(ANIM_EBX), ebp = anim_reg_get(ANIM_EBP), esi = anim_reg_get(ANIM_ESI);
    const uint32_t enter = U32(0, 0x0091600c), afree = U32(0, 0x00916350);
    SU32(E, -4, ebx); SU32(E, -8, ebp); SU32(E, -0xc, esi);
    rx_set_sp(E - 0xc);
    for (int k = 0; k < 2; k++) {
        const rx_addr p = U32(set, k ? 0x2c : 0x3c);
        if (U8(0, 0x00da67d8) != 0) nodeset_cs(E - 0x14, 1, k ? 0x007dd51fu : 0x007dd4ecu);
        SU32(E, -0x10, p); SU32(E, -0x14, k ? 0x007dd522u : 0x007dd4f5u);
        const crt_regs4 r = {afree, enter, p, set};
        crt_aligned_free_at(E - 0x14, r, p);
        if (U8(0, 0x00da67d8) != 0) nodeset_cs(k ? E - 8 : E - 0x14, 0, k ? 0x007dd53cu : 0x007dd50cu);
    }
    rx_set_sp(E);
}
RX_FUNCTION(0x007dd4d0, rx_nodeset_release)
