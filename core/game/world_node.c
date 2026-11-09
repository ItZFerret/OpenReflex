/* world_node.c -- the node hierarchy (0x007db7e0 .. 0x007dd3f1): world matrices of a tree of nodes from their local matrices.
 *
 * Node set (hierarchy): +4 traversal cursor (a record, 0: none), +8 records (16 bytes each), +0xc record count, +0x18 local
 * matrices, +0x1c world matrices (64 bytes each, 16-byte aligned, rows 0..3, row 3 the translation), +0x30, +0x34 dirty flag.
 * Record: +0 its node set, +0xb matrix index, +0xc flags (bit 0 world matrix valid, bit 1 visited by the traversal 007deaa0,
 * bit 2 cleared up the parent chain on invalidation), +0xd next sibling, +0xe first child, +0xf parent (0xff: none).
 * World(rec) = Local(rec) x World(parent): row i = sum over k of L[i][k] * R[k] (R = the parent world rows). */
#include "world_node.h"
#include "world_util.h"
#include "world_node_extern.h"

#define NODE_NONE 0xffu

static inline rx_addr node_record(rx_addr set, uint32_t index) { return U32(set, 8) + 16u * index; }
static inline rx_addr node_local(rx_addr rec) { return U32(U32(rec, 0), 0x18) + 64u * U8(rec, 0xb); }
static inline rx_addr node_world(rx_addr rec) { return U32(U32(rec, 0), 0x1c) + 64u * U8(rec, 0xb); }
static inline rx_addr node_parent(rx_addr rec) { return node_record(U32(rec, 0), U8(rec, 0xf)); }

/* Row l of the product, the two operand groupings the evaluator uses (each product is splat(l lane) * R row, l first):
 *   form a: (w R3 + y R1) + (z R2 + x R0)        form b: (z R2 + x R0) + (w R3 + y R1) */
static inline rx_v4 row_term(rx_v4 l, int k, rx_v4 r) { return v4_mul(v4_lane(l, k), r); }
static inline rx_v4 row_form_a(rx_v4 l, const rx_v4 R[4]) {
    return v4_add(v4_add(row_term(l, 3, R[3]), row_term(l, 1, R[1])), v4_add(row_term(l, 2, R[2]), row_term(l, 0, R[0])));
}
static inline rx_v4 row_form_b(rx_v4 l, const rx_v4 R[4]) {
    return v4_add(v4_add(row_term(l, 2, R[2]), row_term(l, 0, R[0])), v4_add(row_term(l, 3, R[3]), row_term(l, 1, R[1])));
}
static inline void load_rows(rx_addr a, rx_v4 R[4]) { for (int i = 0; i < 4; i++) R[i] = v4_ld(a + 16u * (uint32_t)i); }

/* World = Local x World(parent) of rec, form b (the original's 007dbab0 / 007dbc77 blocks). Their spills: [esp+0x20] = R1,
 * [esp+0x30] = R0 (dead stack left as the original's). */
static void evaluate_form_b(rx_addr rec, rx_addr esp) {
    rx_v4 R[4];
    load_rows(node_world(node_parent(rec)), R);
    const rx_addr L = node_local(rec);
    v4_st(esp + 0x20, R[1]);
    v4_st(esp + 0x30, R[0]);
    rx_v4 out[4];
    for (int i = 0; i < 4; i++) out[i] = row_form_b(v4_ld(L + 16u * (uint32_t)i), R);
    const rx_addr W = node_world(rec);
    for (int i = 0; i < 4; i++) v4_st(W + 16u * (uint32_t)i, out[i]);
}

/* 007db890 Node_EvaluateWorld (thiscall ECX = record, stack subtree flag; RET 4). When the record's world matrix is invalid:
 * marks the set dirty (+0x34 = 1), collects the record and its invalid ancestors ([esp+0x50 + 4k]), evaluates them top-down
 * (the topmost: world = local without a parent, else form a; the rest form b), setting bit 0 of each. With the flag and a first
 * child (+0xe): clears bit 1 of every record of the set, starts the set's cursor (+4) at the child and evaluates (form b) every
 * record the traversal 007deaa0 yields; then the cursor and +0x30 = 0. (The original's prefetch loops have no effect.)
 * Frame: push ebp; and esp, -16; sub esp, 0x144; ebx / esi / edi pushed. Callers read no register result. */
void rx_world_node_evaluate_world(rx_addr rec, uint32_t subtree) {
    world_frame f = world_frame_enter(0x144, 3, 1);
    const rx_addr chain = f.esp + 0x50;   /* the records to evaluate, rec first */
    SU32(f.esp, 0x18, rec);
    SU32(f.esp, 0x1c, 0);                 /* "evaluated" (only selects a prefetch loop) */
    if (!(U8(rec, 0xc) & 1)) {
        SU32(U32(rec, 0), 0x34, 1);
        SU32(f.esp, 0x1c, 1);
        uint32_t n = 0;
        rx_addr cur = rec;
        SU32(chain, 0, rec);
        n = 1;
        while (U8(cur, 0xf) != NODE_NONE) {
            const rx_addr parent = node_parent(cur);
            if (U8(parent, 0xc) & 1) break;
            SU32(chain, 4 * n, parent);
            n++;
            cur = parent;
        }
        int32_t k = (int32_t)n - 1;
        const rx_addr top = U32(chain, 4u * (uint32_t)k);
        if (U8(top, 0xf) != NODE_NONE) {
            /* form a; spills [esp+0x20] = R0, [esp+0x30] = R2, [esp+0x40] = z R2 of row 2 */
            rx_v4 R[4];
            load_rows(node_world(node_parent(top)), R);
            const rx_addr L = node_local(top);
            rx_v4 out[4];
            v4_st(f.esp + 0x20, R[0]);
            for (int i = 0; i < 4; i++) out[i] = row_form_a(v4_ld(L + 16u * (uint32_t)i), R);
            v4_st(f.esp + 0x30, R[2]);
            v4_st(f.esp + 0x40, row_term(v4_ld(L + 0x20), 2, R[2]));
            const rx_addr W = node_world(top);
            for (int i = 0; i < 4; i++) v4_st(W + 16u * (uint32_t)i, out[i]);
        } else {
            const rx_addr L = node_local(top), W = node_world(top);
            for (int i = 0; i < 4; i++) v4_st(W + 16u * (uint32_t)i, v4_ld(L + 16u * (uint32_t)i));
        }
        SU8(top, 0xc, U8(top, 0xc) | 1);
        for (k--; k >= 0; k--) {
            const rx_addr r = U32(chain, 4u * (uint32_t)k);
            evaluate_form_b(r, f.esp);
            SU8(r, 0xc, U8(r, 0xc) | 1);
        }
    }
    if (subtree != 0 && U8(rec, 0xe) != NODE_NONE) {
        const rx_addr set = U32(rec, 0);
        const rx_addr child = node_record(set, U8(rec, 0xe));
        for (int32_t i = 0; i < I32(set, 0xc); i++) {
            const rx_addr flags = U32(set, 8) + 16u * (uint32_t)i + 0xc;
            SU8(flags, 0, U8(flags, 0) & 0xfd);
        }
        SU32(set, 4, child);
        /* the calls at the original's ESP (return addresses 007dbc6f, then 007dbdb5) with its EBX = the first child and ESI = the
         * record, then the evaluated record's local matrix offset (+0xb * 64): 007deaa0 pushes them */
        uint32_t ret = 0x007dbc6f, esi = rec;
        for (;;) {
            SU32(f.esp, -4, ret);
            rx_set_sp(f.esp - 4);
            const rx_addr r = rx_nodeset_next_unvisited_regs(U32(rec, 0), child, esi);
            rx_set_sp(f.esp);
            if (r == 0) break;
            evaluate_form_b(r, f.esp);
            SU8(r, 0xc, U8(r, 0xc) | 1);
            ret = 0x007dbdb5; esi = 64u * U8(r, 0xb);
        }
        const rx_addr s = U32(rec, 0);
        SU32(s, 4, 0);
        SU32(s, 0x30, 0);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x007db890, rx_world_node_evaluate_world)

/* The words 007db890's prologue saves (dead stack read later by original code): the caller's EBP at entry ESP - 4 (push ebp) and
 * EBX / ESI / EDI at the aligned frame's base + 8 / + 4 / + 0. sp0 = the entry ESP (the return slot); written by the caller that
 * knows the registers (the oracle adapter). */
void rx_world_node_evaluate_world_saved_regs(rx_addr sp0, uint32_t ebx, uint32_t ebp, uint32_t esi, uint32_t edi) {
    const rx_addr esp = ((sp0 - 4u) & ~15u) - 0x144u - 12u;
    SU32(sp0, -4, ebp);
    SU32(esp, 8, ebx);
    SU32(esp, 4, esi);
    SU32(esp, 0, edi);
}

/* ---- invalidation --------------------------------------------------------------------------------------------------- */

/* One level of 007db7e0 at stack pointer sp (its return address slot). The original pushes the caller's EBX / ESI / EDI and,
 * for the recursion over a child's siblings, the return address 0x7db83a: code later reads that dead stack, so the same words
 * are written at the same addresses (caller_* = the registers the original holds at the call).
 * The register model follows the disassembly exactly (the function leaves EAX / ECX / EDX to its caller): entry EAX keeps
 * its upper 24 bits with AL replaced by the record's +0xc flags; the slow path ends with EAX = the record (or the topmost
 * ancestor), ECX's low byte replaced by the record's +0xf parent index over whatever the child walk / sibling recursion
 * left, and DL = 0xfb over the last hierarchy word the child walk / recursion loaded. */
static rx_node_invalidate_regs invalidate_at(rx_addr rec, rx_addr sp, uint32_t caller_ebx, uint32_t caller_esi,
                                             uint32_t caller_edi, uint32_t entry_eax, uint32_t entry_edx) {
    SU32(sp - 4, 0, caller_ebx);
    const uint8_t flags = U8(rec, 0xc);
    rx_node_invalidate_regs r;
    r.eax = (entry_eax & 0xffffff00u) | flags;   /* 007db7e3 mov al, [ebx+0xc] */
    r.ecx = rec;
    r.edx = entry_edx;
    if (!(flags & 1)) return r;
    SU8(rec, 0xc, flags & 0xfe);
    SU32(U32(rec, 0), 0x34, 1);
    SU32(sp - 8, 0, caller_esi);
    uint32_t eax = rec + 0xe, ecx = rec, edx = entry_edx;
    if (U8(rec, 0xe) != NODE_NONE) {
        SU32(sp - 0xc, 0, caller_edi);
        rx_addr child = rec;
        for (;;) {
            ecx = U8(eax, 0);                          /* 007db810 movzx ecx, byte [eax] */
            edx = U32(child, 0);                       /* 007db813 mov edx, [esi] */
            child = (ecx << 4) + U32(edx, 8);          /* down the first-child chain */
            ecx = child;
            eax = child;
            uint8_t al = U8(child, 0xd);
            rx_addr next = child;
            while (al != NODE_NONE) {                  /* the child's siblings, recursively */
                ecx = U32(next, 0);
                eax = ((uint32_t)al << 4) + U32(ecx, 8);
                next = eax;
                ecx = next;
                SU32(sp - 0x10, 0, 0x007db83a);
                const rx_node_invalidate_regs q = invalidate_at(next, sp - 0x10, rec, child, next, eax, edx);
                eax = q.eax;
                ecx = q.ecx;
                edx = q.edx;
                al = U8(next, 0xd);                    /* 007db83a mov al, [edi+0xd] */
            }
            const uint8_t cf = U8(child, 0xc);         /* 007db841 */
            if (!(cf & 1)) break;
            SU8(child, 0xc, cf & 0xfe);
            if (U8(child, 0xe) == NODE_NONE) break;
            eax = child + 0xe;                         /* 007db851 lea eax, [esi+0xe] */
        }
    }
    r.edx = (edx & 0xffffff00u) | 0xfbu;               /* 007db857 mov dl, 0xfb */
    SU8(rec, 0xc, U8(rec, 0xc) & 0xfb);
    ecx = (ecx & 0xffffff00u) | U8(rec, 0xf);          /* 007db85c mov cl, [ebx+0xf] */
    eax = rec;                                         /* 007db85f mov eax, ebx */
    while ((ecx & 0xff) != NODE_NONE) {                /* 007db867 up the parent chain */
        eax = U32(eax, 0);
        ecx = ((ecx & 0xff) << 4) + U32(eax, 8);
        eax = ecx;
        SU8(eax, 0xc, U8(eax, 0xc) & 0xfb);
        ecx = (ecx & 0xffffff00u) | U8(eax, 0xf);
    }
    r.eax = eax;
    r.ecx = ecx;
    return r;
}

/* 007db7e0 with its return registers (the original leaves EAX / ECX / EDX to its caller; entry_eax / entry_edx are the
 * registers at the call, which its fast path partly preserves). */
rx_node_invalidate_regs rx_world_node_invalidate_regs(rx_addr rec, uint32_t entry_eax, uint32_t entry_edx,
                                                      uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi) {
    return invalidate_at(rec, rx_sp(), caller_ebx, caller_esi, caller_edi, entry_eax, entry_edx);
}

/* 007db7e0 Node_Invalidate (ECX = record; RET 0): when the record's world matrix is valid: clears that bit, marks the set dirty
 * (+0x34 = 1) and invalidates the subtree (the first-child chain iteratively while valid, each child's siblings recursively);
 * then clears bit 2 of the record and of every ancestor. caller_ebx / esi / edi: the caller's registers, which the original
 * pushes (see invalidate_at). */
void rx_world_node_invalidate(rx_addr rec, uint32_t caller_ebx, uint32_t caller_esi, uint32_t caller_edi) {
    (void)rx_world_node_invalidate_regs(rec, 0, 0, caller_ebx, caller_esi, caller_edi);
}
RX_FUNCTION(0x007db7e0, rx_world_node_invalidate)

/* ---- setting world matrices ----------------------------------------------------------------------------------------- */

/* The inverse of a rigid world matrix P: the transposed rotation (w lanes from the runtime vector 0xd61660 lane 0) and the
 * translation -T . row i summed in double precision ((y + x) + z; w lane 0xd61690 lane 3). */
static void rigid_inverse(const rx_v4 P[4], rx_v4 inv[4]) {
    const float k0 = F32(0, 0x00d61660);
    const rx_v4 nt = v4_sub(v4_zero(), P[3]);
    float d[3];
    for (int i = 0; i < 3; i++) {
        double s = (double)P[i].v[1] * (double)nt.v[1];
        s = s + (double)P[i].v[0] * (double)nt.v[0];
        s = s + (double)P[i].v[2] * (double)nt.v[2];
        d[i] = (float)s;
    }
    inv[0] = v4(P[0].v[0], P[1].v[0], P[2].v[0], k0);
    inv[1] = v4(P[0].v[1], P[1].v[1], P[2].v[1], k0);
    inv[2] = v4(P[0].v[2], P[1].v[2], P[2].v[2], k0);
    inv[3] = v4(d[0], d[1], d[2], F32(0, 0x00d61690 + 12));
}

/* 007dc890 Node_SetWorld (EDI = record, ESI = 16-byte aligned world matrix; RET 0): the local matrix that makes the record's
 * world matrix [ESI]: without a parent local = W; else (the parent evaluated first) local = W x inverse(parent world) (form a).
 * Then Node_Invalidate. (A null record: the original multiplies record 0's local matrix by W, form b; kept.) */
void rx_world_node_set_world(rx_addr rec, rx_addr src, uint32_t caller_ebx) {
    world_frame f = world_frame_enter(0xb0, 0, 1);
    rx_v4 W[4];
    load_rows(src, W);
    if (rec == 0) {
        const rx_addr a = (U8(0, 0xb) << 6) + U32(U32(0, 0), 0x18);
        rx_v4 R[4], out[4];
        load_rows(a, R);
        for (int i = 0; i < 4; i++) out[i] = row_form_b(W[i], R);
        for (int i = 0; i < 4; i++) v4_st(a + 16u * (uint32_t)i, out[i]);
    } else if (U8(rec, 0xf) == NODE_NONE) {
        const rx_addr a = node_local(rec);
        for (int i = 0; i < 4; i++) v4_st(a + 16u * (uint32_t)i, v4_ld(src + 16u * (uint32_t)i));
    } else {
        WORLD_CALL_SP(4, rx_world_node_evaluate_world(node_parent(rec), 0));
        rx_v4 P[4], inv[4], out[4];
        load_rows(node_world(node_parent(rec)), P);
        rigid_inverse(P, inv);
        load_rows(src, W);   /* the original reads W again after the evaluation */
        for (int i = 0; i < 4; i++) out[i] = row_form_a(W[i], inv);
        const rx_addr a = node_local(rec);
        for (int i = 0; i < 4; i++) v4_st(a + 16u * (uint32_t)i, out[i]);
    }
    WORLD_CALL_SP(0, rx_world_node_invalidate(rec, caller_ebx, src, rec));
    world_frame_leave(&f);
}
RX_FUNCTION(0x007dc890, rx_world_node_set_world)

/* 007dcfb0 Node_SetWorldRotation (thiscall record; stdcall quaternion (x y z w); RET 4): the rotation matrix of the quaternion
 * (translation row = the global 0xa95d50 vector, w 1) is set as the world matrix (Node_SetWorld on a frame copy), then the old
 * local translation row is put back. (caller_ebx: see Node_Invalidate.) */
void rx_world_node_set_world_rotation(rx_addr rec, rx_addr quat, uint32_t caller_ebx) {
    world_frame f = world_frame_enter(0x68, 2, 1);
    const float x = F32(quat, 0), y = F32(quat, 4), z = F32(quat, 8), w = F32(quat, 0xc);
    const float z2 = phys_mul(z, 2.0f /* 0x92e330 */), x2 = phys_mul(x, 2.0f), y2 = phys_mul(y, 2.0f);
    const float zz = phys_mul(z, z2), wx = phys_mul(w, x2), wz = phys_mul(w, z2), wy = phys_mul(w, y2);
    const float xx = phys_mul(x, x2), yy = phys_mul(y, y2), xy = phys_mul(y, x2), yz = phys_mul(z, y2), xz = phys_mul(z, x2);
    const float one = 1.0f /* 0x931f24 */;
    const rx_addr m = f.esp + 0x20;
    v4_st(m, v4(phys_sub(one, phys_add(zz, yy)), phys_add(xy, wz), phys_sub(xz, wy), 0.0f));
    v4_st(m + 0x10, v4(phys_sub(xy, wz), phys_sub(one, phys_add(zz, xx)), phys_add(yz, wx), 0.0f));
    v4_st(m + 0x20, v4(phys_add(xz, wy), phys_sub(yz, wx), phys_sub(one, phys_add(yy, xx)), 0.0f));
    v4_st(m + 0x30, v4(F32(0, 0x00a95d50), F32(0, 0x00a95d54), F32(0, 0x00a95d58), one));
    const rx_v4 keep = v4_ld(node_local(rec) + 0x30);
    WORLD_CALL_SP(0, rx_world_node_set_world(rec, m, caller_ebx));
    v4_st(node_local(rec) + 0x30, keep);
    world_frame_leave(&f);
}
RX_FUNCTION(0x007dcfb0, rx_world_node_set_world_rotation)

/* 007dc560 Node_WorldToLocalPoint (ESI = record, EDI = out; stdcall point; RET 4; EAX = out): the point in the record's frame:
 * the evaluated world matrix's rigid inverse applied, ((y col1 + x col0) + z col2) + T'. */
rx_addr rx_world_node_world_to_local_point(rx_addr rec, rx_addr out, rx_addr point) {
    world_frame f = world_frame_enter(0xb0, 0, 1);
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(rec, 0));
    rx_v4 P[4], inv[4];
    load_rows(node_world(rec), P);
    rigid_inverse(P, inv);
    const rx_v4 p = v4_ld(point);
    rx_v4 r = v4_add(v4_mul(v4_lane(p, 1), inv[1]), v4_mul(v4_lane(p, 0), inv[0]));
    r = v4_add(v4_mul(v4_lane(p, 2), inv[2]), r);
    v4_st(out, v4_add(r, inv[3]));
    world_frame_leave(&f);
    return out;
}
RX_FUNCTION(0x007dc560, rx_world_node_world_to_local_point)

/* 007dd1b0 Node_SetWorldPosition (stdcall record, vector; RET 8): the local translation (row 3 x y z) that puts the record's
 * world position at the vector: through the parent's Node_WorldToLocalPoint, or directly without a parent; then
 * Node_Invalidate. (A null record: the vector is added to record 0's local row 3; kept.) */
void rx_world_node_set_world_position(rx_addr rec, rx_addr v, uint32_t caller_esi, uint32_t caller_edi) {
    world_frame f = world_frame_enter(0x34, 3, 1);
    const rx_addr buf = f.esp + 0x10;
    v4_st(buf, v4_ld(v));
    uint32_t esi = caller_esi, edi = caller_edi;
    if (rec == 0) {
        const rx_addr a = (U8(0, 0xb) << 6) + U32(U32(0, 0), 0x18);
        const uint32_t keep_w = U32(a, 0x3c);
        v4_st(a + 0x30, v4_add(v4_ld(a + 0x30), v4_ld(buf)));
        SU32(a, 0x3c, keep_w);
    } else {
        rx_addr a;
        if (U8(rec, 0xf) != NODE_NONE) {
            esi = node_parent(rec); edi = f.esp + 0x20;
            rx_addr r;
            WORLD_CALL_SP(4, r = rx_world_node_world_to_local_point(esi, edi, buf));
            v4_st(buf, v4_ld(r));
        }
        a = node_local(rec);
        SU32(a, 0x30, U32(buf, 0)); SU32(a, 0x34, U32(buf, 4)); SU32(a, 0x38, U32(buf, 8));
    }
    WORLD_CALL_SP(0, rx_world_node_invalidate(rec, rec, esi, edi));
    world_frame_leave(&f);
}
RX_FUNCTION(0x007dd1b0, rx_world_node_set_world_position)

/* 007dd3a0 Node_GetWorldPosition (ESI = record, EDI = out; RET 0): row 3 of the evaluated world matrix. */
void rx_world_node_get_world_position(rx_addr rec, rx_addr out) {
    world_frame f = world_frame_enter(0x10, 0, 1);
    rx_addr base;
    uint32_t idx;
    if (rec == 0) { base = U32(U32(0, 0), 0x18); idx = U8(0, 0xb); }
    else { WORLD_CALL_SP(4, rx_world_node_evaluate_world(rec, 0)); base = U32(U32(rec, 0), 0x1c); idx = U8(rec, 0xb); }
    v4_st(out, v4_ld((idx << 6) + base + 0x30));
    world_frame_leave(&f);
}
RX_FUNCTION(0x007dd3a0, rx_world_node_get_world_position)

/* 007dd320 Node_GetWorldMatrix (EDI = record, ESI = 64-byte out (unaligned); RET 0): the evaluated world matrix. */
void rx_world_node_get_world_matrix(rx_addr rec, rx_addr out) {
    WORLD_CALL_SP(4, rx_world_node_evaluate_world(rec, 0));
    const rx_addr a = node_world(rec);
    for (uint32_t i = 0; i < 4; i++) v4_st(out + 16 * i, v4_ld(a + 16 * i));
}
RX_FUNCTION(0x007dd320, rx_world_node_get_world_matrix)
