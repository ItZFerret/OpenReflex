/* phys_ragdoll.c -- the rider ragdoll (rider animation's rig P = anim + 0x380, phys_rigbody.c): the joint solver substeps
 * (004238a0) and their stages. Joints (P+0x1b20 count, records of 0x170 bytes from P+0x1b30): +0 body a, +0x10 anchor in a,
 * +0x20 body b, +0x30 anchor in b, +0x40 / +0x70 the anchors' cross matrices in world space (a negated), +0xa0 the velocity
 * bias. Only crashes run this (the five test worlds' scripted drives never do; Concord drive 7 does). */
#include "phys_ragdoll.h"
#include "world_veh.h"
#include "phys_rig_math.h"
#include "phys_rigbody.h"

#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)
#define ANG(P, i) ((P) + 0x70u * (uint32_t)(i))
/* The helpers are called with the stack pointer at the frame base: rx_call_original_ex pushes their arguments as the original. */

/* 00421fa0 (stdcall P; RET 4): per joint: the anchors' cross matrices turned to world space, -(R_a [r_a]x) (+0x40) and
 * R_b [r_b]x (+0x70), and the centripetal bias R_a (W_a (W_a r_a)) - R_b (W_b (W_b r_b)) (+0xa0; W = [w]x, w the body-frame
 * angular velocities). Frame: push ebp; and esp, -16; sub esp, 0x184; push ebx / esi / edi (zero +0x10, vectors +0x20 / +0x30
 * / +0x40 / +0x50, W_b +0x60, R_b +0x90, R_a +0xc0, W_a +0xf0, [r_a]x +0x120, [r_b]x +0x150). Returns the EDX left (the last
 * product's vector; the entry EDX without joints). */
uint32_t phys_ragdoll_joint_jacobians(rx_addr P, uint32_t edx_in) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x184 - 12;
    rx_set_sp(L);
    const rx_addr t1 = L + 0x20, t2 = L + 0x30, ub = L + 0x40, ua = L + 0x50, WB = L + 0x60, RB = L + 0x90, RA = L + 0xc0;
    const rx_addr WA = L + 0xf0, SA = L + 0x120, SB = L + 0x150;
    uint32_t edx = edx_in;
    if (I32(P, 0x1b20) > 0) v4_st(L + 0x10, v4_zero());
    for (int32_t j = 0; j < I32(P, 0x1b20); j++) {
        const rx_addr J = P + 0x1b30 + 0x170u * (uint32_t)j;
        const uint32_t a = U32(J, 0), b = U32(J, 0x20);
        rig_m3_from_quat(RA, ANG(P, a) + 0xb00);
        rig_m3_from_quat(RB, ANG(P, b) + 0xb00);
        rig_m3_skew(SA, J + 0x10);
        rig_m3_skew(SB, J + 0x30);
        rig_m3_mul(J + 0x40, RA, SA);
        rig_m3_mul(J + 0x70, RB, SB);
        for (int r = 0; r < 3; r++) v4_st(J + 0x40 + 0x10u * (uint32_t)r, v4_sub(v4_ld(L + 0x10), v4_ld(J + 0x40 + 0x10u * (uint32_t)r)));
        rig_m3_skew(WA, ANG(P, a) + 0xb10);
        rig_m3_skew(WB, ANG(P, b) + 0xb10);
        rig_m3_mul_vec(t1, J + 0x10, WA);
        rig_m3_mul_vec(t2, t1, WA);
        rig_m3_mul_vec(ua, t2, RA);
        rig_m3_mul_vec(t1, J + 0x30, WB);
        rig_m3_mul_vec(t2, t1, WB);
        rig_m3_mul_vec(ub, t2, RB);
        v4_st(J + 0xa0, v4_sub(v4_ld(ua), v4_ld(ub)));
        edx = t2;
    }
    rx_set_sp(sp0);
    return edx;
}
RX_FUNCTION(0x00421fa0, phys_ragdoll_joint_jacobians)

/* 00422850 (EDI = P, xmm0 = substep h): the bodies' external forces: linear +0xc0 = mass * gravity (P+0) + force (+0xd0), angular
 * +0xb40 = torque (+0xb50) - [w]x (I w) (gyroscopic, I = +0xb20); then the accumulated impulses / h are added (+0xe0, +0xb60).
 * Frame: push ebp; and esp, -16; sub esp, 0x68; push ebx / esi (I w +0x10, product +0x20, [w]x +0x30). Returns the EDX left. */
uint32_t phys_ragdoll_external_forces(rx_addr P, float h) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x68 - 8;
    rx_set_sp(L);
    const rx_v4 inv_h = v4_splat((float)(1.0 / (double)h));
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_addr l = P + 0x80u * (uint32_t)i, a = ANG(P, i) + 0xb10;
        v4_st(l + 0xc0, v4_add(v4_mul(v4_ld(l + 0xa0), v4_ld(P)), v4_ld(l + 0xd0)));
        v4_st(a + 0x30, v4_ld(a + 0x40));
        v4_st(L + 0x10, v4_mul(v4_ld(a + 0x10), v4_ld(a)));
        rig_m3_skew(L + 0x30, a);
        rig_m3_mul_vec(L + 0x20, L + 0x10, L + 0x30);
        v4_st(a + 0x30, v4_sub(v4_ld(a + 0x30), v4_ld(L + 0x20)));
    }
    int32_t i = 0;
    for (; i < I32(P, 0x7c); i++) {
        const rx_addr l = P + 0x80u * (uint32_t)i + 0xc0, a = ANG(P, i) + 0xb40;
        v4_st(l, v4_add(v4_mul(v4_ld(l + 0x20), inv_h), v4_ld(l)));
        v4_st(a, v4_add(v4_mul(v4_ld(a + 0x20), inv_h), v4_ld(a)));
    }
    rx_set_sp(sp0);
    return (uint32_t)i;
}
RX_FUNCTION(0x00422850, phys_ragdoll_external_forces)

/* ---- calls of the world's 3x3 helpers with the registers tracked (callers of the ragdoll stages read the EDX they leave) ---- */
enum { R_EAX, R_ECX, R_EDX, R_EBX, R_ESP, R_EBP, R_ESI, R_EDI };
/* The world matrix helpers are the core's ports (world_veh.h): called at the original's ESP. Their leftover EDX is
 * what 00422960 returns: 0041f430 leaves EDX = out, 0041f650 leaves EDX as it was (EAX = dst, ECX = src). */
static void t_mul(rx_regs* r, rx_addr out, rx_addr A, rx_addr B) {          /* 0041f430: out = A B (EDX, ESI, ECX) */
    const rx_addr sp = r->r[R_ESP];
    r->r[R_EDX] = out; r->r[R_ESI] = A; r->r[R_ECX] = B;
    rx_set_sp(sp - 4);
    rx_world_matrix3_mul(out, A, B);
    rx_set_sp(sp);
}
static void t_transpose(rx_regs* r, rx_addr dst, rx_addr src) {             /* 0041f650 (ECX src; cdecl dst); EAX = dst */
    const rx_addr sp = r->r[R_ESP];
    r->r[R_ECX] = src; r->r[R_EAX] = dst;
    rx_set_sp(sp - 8);
    rx_world_matrix3_transpose(dst, src);
    rx_set_sp(sp);
}
static void m3_copy(rx_addr dst, rx_addr src) { for (uint32_t o = 0; o < 0x30; o += 0x10) v4_st(dst + o, v4_ld(src + o)); }
static void m3_sub_from(rx_addr dst, rx_addr m) { for (uint32_t o = 0; o < 0x30; o += 0x10) v4_st(dst + o, v4_sub(v4_ld(dst + o), v4_ld(m + o))); }
static void m3_add(rx_addr dst, rx_addr a, rx_addr b) { for (uint32_t o = 0; o < 0x30; o += 0x10) v4_st(dst + o, v4_add(v4_ld(a + o), v4_ld(b + o))); }

/* 00422960 (stdcall P; RET 4): the articulated inertias of the ragdoll's nodes, leaves first (P+0x9390 count, the order from
 * P+0x9394; nodes of 0x230 bytes from P+0x39e0: +0 kind (0 = a body with a ball joint, else fixed), +4 body / joint, +0xc
 * parent (-5 = none), +0x10 child count, +0x14 children, +0x40 / +0x70 / +0xa0 / +0xd0 the 6x6 inertia's blocks (a fixed node
 * only +0x40), +0x100.. its inverse's blocks, +0x1c0 / +0x1f0 the transforms to the parent).
 *   1. each node's own inertia: mass * I3 (P+0xa0 + 0x80 body, times the identity rows 0xd615c0 / 0xd616a0 / 0xd61680) and the
 *      body inertia (P+0xb20 + 0x70 body) on the diagonal, zero elsewhere (only +0x40..0x60 zeroed for a fixed node); the parent
 *      transforms: identity and the joint's +0x40 (fixed), or minus identity and the transpose of the parent's joint +0x70;
 *   2. each node minus its children's transformed inertias (C^T I_c C with the child's transforms), then the inverse (3x3, or
 *      the 2x2 block inverse 0041f800) and the transforms pre-multiplied by it.
 * Frame: push ebp; and esp, -16; sub esp, 0x204 (cookie +0x200); push ebx / esi / edi: products +0x50 / +0x80 / +0xb0 / +0xe0 /
 * +0x110, transposes +0x140 / +0x1a0 (through +0x170 / +0x1d0). Returns the EDX left. */
uint32_t phys_ragdoll_node_inertia(rx_addr P, uint32_t edx_in) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x204 - 12;
    rx_set_sp(L);
    SU32(L, 0x20c, U32(0, 0x009b45e4) ^ (L + 0xc));
    rx_regs r = phys_regs();
    r.r[R_EDX] = edx_in; r.r[R_EBX] = P; r.r[R_EBP] = sp0 - 4; r.r[R_ESP] = L;
#define NODE(k) (P + 0x39e0u + 0x230u * (uint32_t)(k))
    const rx_v4 e0 = v4_ld(0x00d615c0), e1 = v4_ld(0x00d616a0), e2 = v4_ld(0x00d61680);
    for (int32_t i = 0; i < I32(P, 0x9390); i++) {
        const rx_addr N = NODE(U32(P, 0x9394 + 4u * (uint32_t)i));
        r.r[R_EDX] = N;
        for (uint32_t o = 0x40; o <= 0x60; o += 0x10) v4_st(N + o, v4_ld(0x00d61670));
        if (U32(N, 0) == 0) {
            for (uint32_t o = 0x70; o <= 0xf0; o += 0x10) v4_st(N + o, v4_ld(0x00d61670));
            const rx_addr m = P + 0xa0 + 0x80u * U32(N, 4), I = P + 0xb20 + 0x70u * U32(N, 4);
            v4_st(N + 0x40, v4_mul(v4_ld(m), e0)); v4_st(N + 0x50, v4_mul(v4_ld(m), e1)); v4_st(N + 0x60, v4_mul(v4_ld(m), e2));
            v4_st(N + 0xd0, v4_mul(v4_ld(I), e0)); v4_st(N + 0xe0, v4_mul(v4_ld(I), e1)); v4_st(N + 0xf0, v4_mul(v4_ld(I), e2));
        }
        if (I32(N, 0xc) == -5) continue;
        rx_addr src;
        v4_st(N + 0x1c0, e0); v4_st(N + 0x1d0, e1); v4_st(N + 0x1e0, e2);
        if (U32(N, 0) != 0) {
            src = P + 0x1b70 + 0x170u * U32(N, 4);
        } else {
            const uint32_t j = U32(NODE(U32(N, 0xc)), 4);
            for (uint32_t o = 0x1c0; o <= 0x1e0; o += 0x10) v4_st(N + o, v4_sub(v4_zero(), v4_ld(N + o)));
            t_transpose(&r, L + 0x170, P + 0x1b30 + 0x170u * j + 0x70);
            r.r[R_EDX] = N;
            src = L + 0x170;
        }
        m3_copy(N + 0x1f0, src);
    }
    for (int32_t i = 0; i < I32(P, 0x9390); i++) {
        const rx_addr N = NODE(U32(P, 0x9394 + 4u * (uint32_t)i));
        for (int32_t c = 0; c < I32(N, 0x10); c++) {
            const rx_addr C = NODE(U32(N, 0x14 + 4u * (uint32_t)c));
            const rx_addr M50 = L + 0x50, M80 = L + 0x80, Mb0 = L + 0xb0, M140 = L + 0x140, M1a0 = L + 0x1a0;
            SU32(L, 0x20, C + 0x1f0);
            if (U32(N, 0) != 0) {
                t_transpose(&r, L + 0x1d0, C + 0x1c0); m3_copy(M140, L + 0x1d0);
                t_transpose(&r, L + 0x1d0, C + 0x1f0); m3_copy(M1a0, L + 0x1d0);
                t_mul(&r, M50, C + 0x40, C + 0x1c0);
                t_mul(&r, Mb0, C + 0x70, C + 0x1f0);
                m3_add(M80, M50, Mb0);
                t_mul(&r, M50, M140, M80);
                m3_sub_from(N + 0x40, M50);
                t_mul(&r, M50, C + 0xa0, C + 0x1c0);
                t_mul(&r, Mb0, C + 0xd0, C + 0x1f0);
                m3_add(M80, M50, Mb0);
                t_mul(&r, M50, M1a0, M80);
                m3_sub_from(N + 0x40, M50);
            } else {
                t_transpose(&r, L + 0x1d0, C + 0x1c0); m3_copy(Mb0, L + 0x1d0);
                t_transpose(&r, L + 0x1d0, C + 0x1f0); m3_copy(M140, L + 0x1d0);
                t_mul(&r, M80, C + 0x40, C + 0x1c0);
                t_mul(&r, M50, Mb0, M80);  m3_sub_from(N + 0x40, M50);
                t_mul(&r, M50, M140, M80); m3_sub_from(N + 0xa0, M50);
                t_mul(&r, M80, C + 0x40, C + 0x1f0);
                t_mul(&r, M50, Mb0, M80);  m3_sub_from(N + 0x70, M50);
                t_mul(&r, M50, M140, M80); m3_sub_from(N + 0xd0, M50);
            }
        }
        if (U32(N, 0) != 0) {
            {   /* Matrix3_Inverse (ECX dst; cdecl src): the core's port, EDX as it was */
                const rx_addr sp = r.r[R_ESP];
                r.r[R_ESI] = N + 0x100; r.r[R_ECX] = N + 0x100;
                rx_set_sp(sp - 8);
                rx_world_matrix3_inverse(N + 0x100, N + 0x40);
                rx_set_sp(sp);
            }
            if (I32(N, 0xc) == -5) continue;
            t_mul(&r, L + 0xe0, N + 0x100, N + 0x1c0);
            t_mul(&r, L + 0x110, N + 0x100, N + 0x1f0);
            m3_copy(N + 0x1c0, L + 0xe0);
            m3_copy(N + 0x1f0, L + 0x110);
        } else {
            {   /* the 2x2 block inverse (cdecl 8 arguments): the core's port, EDX as it was */
                const rx_addr sp = r.r[R_ESP];
                r.r[R_ESI] = N + 0x100; r.r[R_ECX] = N + 0x160; r.r[R_EDX] = N + 0x190;
                rx_set_sp(sp - 36);
                rx_world_block_inverse(N + 0x40, N + 0x70, N + 0xa0, N + 0xd0, N + 0x100, N + 0x130, N + 0x160, N + 0x190);
                rx_set_sp(sp);
            }
            if (I32(N, 0xc) == -5) continue;
            t_mul(&r, L + 0xe0, N + 0x100, N + 0x1c0);
            t_mul(&r, L + 0x110, N + 0x130, N + 0x1f0);
            rx_v4 row0 = v4_add(v4_ld(L + 0x110), v4_ld(L + 0xe0));     /* kept in a register */
            v4_st(L + 0x180, v4_add(v4_ld(L + 0x120), v4_ld(L + 0xf0)));
            v4_st(L + 0x190, v4_add(v4_ld(L + 0x130), v4_ld(L + 0x100)));
            t_mul(&r, L + 0xe0, N + 0x160, N + 0x1c0);
            t_mul(&r, L + 0x110, N + 0x190, N + 0x1f0);
            m3_add(N + 0x1f0, L + 0x110, L + 0xe0);
            v4_st(N + 0x1c0, row0); v4_st(N + 0x1d0, v4_ld(L + 0x180)); v4_st(N + 0x1e0, v4_ld(L + 0x190));
        }
    }
#undef NODE
    rx_set_sp(sp0);
    return r.r[R_EDX];
}
RX_FUNCTION(0x00422960, phys_ragdoll_node_inertia)

/* 004232b0 (ESI = P): the articulated solve for the joint forces with the node inertias of 00422960 (node +0x20 / +0x30 the
 * bias 6-vector):
 *   1. leaves first: a fixed node's bias from its joint (J = P+0x1b30 + 0x170 node.joint, bodies a = J+0, b = J+0x20): the
 *      difference of the bodies' gravity terms (c * b at P+0xc0 / P+0xb0 + 0x80 body) plus the joint matrices (J+0x40, J+0x70)
 *      times the bodies' angular terms (+0xb40 * +0xb30 + 0x70 body) plus the joint bias (J+0xa0); zero for the others; minus
 *      each child's bias through the transposes of its transforms (+0x1c0 / +0x1f0);
 *   2. roots first: the bias times the node's inverse inertia, minus the parent's through the node's transforms, and for a
 *      fixed node the resulting force applied to its two bodies (linear +0xc0 +/-, angular +0xb40 through the joint matrices'
 *      transposes).
 * Frame: push ebp; and esp, -16; sub esp, 0xf8 (cookie +0xf4); push ebx / edi: products +0x30 / +0x40 / +0x50, transposes +0x60
 * / +0x90 (through +0xc0). */
static void t_mul_vec(rx_addr out, rx_addr v, rx_addr M) {                  /* 0041f5c0: out = M v (ECX, EDX; cdecl M) */
    SU32(rx_sp() - 4, 0, M);                                   /* cdecl: M pushed */
    PHYS_CALL_SP(4, rx_world_matrix3_mul_vec3(out, v, M));
}
static void transpose_into(rx_addr L, rx_addr dst, rx_addr src) { rig_m3_transpose(L + 0xc0, src); m3_copy(dst, L + 0xc0); }

void phys_ragdoll_solve_forces(rx_addr P) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xf8 - 8;
    rx_set_sp(L);
    SU32(L, 0xfc, U32(0, 0x009b45e4) ^ (L + 8));
#define NODE(k) (P + 0x39e0u + 0x230u * (uint32_t)(k))
#define LIN(b) (P + 0xc0u + 0x80u * (uint32_t)(b))
#define ANGF(b) (P + 0xb40u + 0x70u * (uint32_t)(b))
    const rx_addr V30 = L + 0x30, V40 = L + 0x40, V50 = L + 0x50, T60 = L + 0x60, T90 = L + 0x90;
    for (int32_t i = 0; i < I32(P, 0x9390); i++) {
        const rx_addr N = NODE(U32(P, 0x9394 + 4u * (uint32_t)i));
        if (U32(N, 0) != 0) {
            const rx_addr J = P + 0x1b30 + 0x170u * U32(N, 4);
            const uint32_t a = U32(J, 0), b = U32(J, 0x20);
            SU32(L, 0xc, b);
            const rx_v4 lin = v4_sub(v4_mul(v4_ld(P + 0xc0 + 0x80u * a), v4_ld(P + 0xb0 + 0x80u * a)),
                                     v4_mul(v4_ld(P + 0xc0 + 0x80u * b), v4_ld(P + 0xb0 + 0x80u * b)));
            v4_st(V40, v4_mul(v4_ld(ANGF(a)), v4_ld(ANGF(a) - 0x10)));
            t_mul_vec(V50, V40, J + 0x40);
            const rx_v4 s = v4_add(v4_ld(V50), lin);
            v4_st(V30, s);
            v4_st(V40, v4_mul(v4_ld(ANGF(b)), v4_ld(ANGF(b) - 0x10)));
            t_mul_vec(V50, V40, J + 0x70);
            v4_st(N + 0x20, v4_add(v4_add(v4_ld(V50), s), v4_ld(J + 0xa0)));
        } else {
            v4_st(N + 0x20, v4_ld(0x00d61670));
            v4_st(N + 0x30, v4_ld(0x00d61670));
        }
        for (int32_t c = 0; c < I32(N, 0x10); c++) {
            SU32(L, 0xc, (uint32_t)c);
            const rx_addr C = NODE(U32(N, 0x14 + 4u * (uint32_t)c));
            transpose_into(L, T90, C + 0x1c0);
            transpose_into(L, T60, C + 0x1f0);
            t_mul_vec(V30, C + 0x20, T90);
            if (U32(N, 0) != 0) {
                t_mul_vec(V40, C + 0x30, T60);
                v4_st(N + 0x20, v4_sub(v4_ld(N + 0x20), v4_add(v4_ld(V40), v4_ld(V30))));
            } else {
                t_mul_vec(V40, C + 0x20, T60);                          /* (EDX left at the child's +0x20) */
                v4_st(N + 0x20, v4_sub(v4_ld(N + 0x20), v4_ld(V30)));
                v4_st(N + 0x30, v4_sub(v4_ld(N + 0x30), v4_ld(V40)));
            }
        }
    }
    for (int32_t i = I32(P, 0x9390) - 1; i >= 0; i--) {
        const rx_addr N = NODE(U32(P, 0x9394 + 4u * (uint32_t)i)), z = N + 0x20;
        SU32(L, 0x10, z);
        if (U32(N, 0) != 0) {
            t_mul_vec(V30, z, N + 0x100);
            v4_st(z, v4_ld(V30));
        } else {
            t_mul_vec(V30, z, N + 0x100);
            t_mul_vec(V40, N + 0x30, N + 0x130);
            const rx_v4 top = v4_add(v4_ld(V40), v4_ld(V30));
            t_mul_vec(V30, z, N + 0x160);
            t_mul_vec(V40, N + 0x30, N + 0x190);
            v4_st(z, top);
            v4_st(N + 0x30, v4_add(v4_ld(V40), v4_ld(V30)));
        }
        if (I32(N, 0xc) != -5) {
            const rx_addr Pn = NODE(U32(N, 0xc));
            if (U32(N, 0) != 0) {
                SU32(L, 0xc, Pn);
                t_mul_vec(V30, Pn + 0x20, N + 0x1c0);
                t_mul_vec(V40, Pn + 0x30, N + 0x1f0);
                v4_st(z, v4_sub(v4_ld(z), v4_add(v4_ld(V40), v4_ld(V30))));
            } else {
                t_mul_vec(V30, Pn + 0x20, N + 0x1c0);
                t_mul_vec(V40, Pn + 0x20, N + 0x1f0);                  /* (EDX left at the parent's +0x20) */
                v4_st(z, v4_sub(v4_ld(z), v4_ld(V30)));
                v4_st(N + 0x30, v4_sub(v4_ld(N + 0x30), v4_ld(V40)));
            }
        }
        if (U32(N, 0) == 0) continue;
        /* a fixed node's joint force onto its two bodies */
        const rx_addr J = P + 0x1b30 + 0x170u * U32(N, 4);
        const uint32_t a = U32(J, 0), b = U32(J, 0x20);
        SU32(L, 0xc, b);
        const rx_v4 f = v4_ld(z);
        v4_st(V50, f);
        transpose_into(L, T60, J + 0x40);
        t_mul_vec(V30, V50, T60);
        v4_st(LIN(a), v4_add(f, v4_ld(LIN(a))));
        v4_st(ANGF(a), v4_add(v4_ld(V30), v4_ld(ANGF(a))));
        transpose_into(L, T60, J + 0x70);
        t_mul_vec(V30, V50, T60);
        v4_st(LIN(b), v4_sub(v4_ld(LIN(b)), f));
        v4_st(ANGF(b), v4_add(v4_ld(ANGF(b)), v4_ld(V30)));
    }
#undef NODE
#undef LIN
#undef ANGF
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004232b0, phys_ragdoll_solve_forces)

/* 004238a0 (ECX = P, xmm0 = dt): the ragdoll step: P+0x10 substeps of h = dt / n (in double), each the external forces, the joint
 * jacobians, the node inertias, the force solve and the integration; then every body's quaternion (+0xb00) normalized (the
 * 4-lane sum (q1^2 + q3^2) + (q0^2 + q2^2); 0 where the length is 0, the NaN vec4 0092dc30 where the sum is infinite). Clears
 * P+0x943c first. Frame: push ebp; and esp, -16; push ecx (h); push ebx / esi / edi. */
void phys_ragdoll_step(rx_addr P, float dt) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 16;
    rx_set_sp(L);
    const float h = (float)((double)dt / (double)(float)I32(P, 0x10));
    SU32(P, 0x943c, 0);
    SF32(L, 0xc, h);
    for (int32_t i = 0; i < I32(P, 0x10); i++) {
        uint32_t edx = 0;
        CALL_AT_SP(L, 0, edx = phys_ragdoll_external_forces(P, h));
        SU32(L - 4, 0, P);
        CALL_AT_SP(L, 1, edx = phys_ragdoll_joint_jacobians(P, edx));
        SU32(L - 4, 0, P);
        CALL_AT_SP(L, 1, phys_ragdoll_node_inertia(P, edx));
        CALL_AT_SP(L, 0, phys_ragdoll_solve_forces(P));
        CALL_AT_SP(L, 0, phys_rigbody_integrate(P, h));
    }
    const rx_v4 inf = v4_ld(0x0092dc20), nan = v4_ld(0x0092dc30);
    for (int32_t i = 0; i < I32(P, 0x7c); i++) {
        const rx_addr q = ANG(P, i) + 0xb00;
        const rx_v4 v = v4_ld(q);
        const float n2 = phys_hsum4(v4_mul(v, v)), n = phys_sqrt(n2);
        rx_v4 out;
        for (int k = 0; k < 4; k++) {
            const int finite = inf.v[k] != n2, nonzero = 0.0f != n;
            out.v[k] = finite ? (nonzero ? phys_div(v.v[k], n) : rx_bf(0)) : nan.v[k];
        }
        v4_st(q, out);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x004238a0, phys_ragdoll_step)

/* the joint anchor of body `b` (anchor r in its frame) in world space relative to the body: q (r, 0) q*  (q = P+0xb00 + 0x70 b) */
static rx_v4 anchor_offset(rx_addr P, uint32_t b, rx_v4 r) {
    const rx_v4 q = v4_ld(ANG(P, b) + 0xb00);
    return phys_quat_mul(q, phys_quat_mul(r, phys_quat_conj(q)));
}

/* 00423b80 (stdcall P; RET 4): the fixed joints' positions enforced, roots last (the node order backwards): body b's position
 * (P + 0x80 (b + 1)) = body a's + a's anchor offset - b's (anchors J+0x10 / J+0x30 with w masked off, 0092db80). *ecx / *edx =
 * the registers left (ECX the last node or joint, EDX (a + 1) * 0x80 of the last fixed joint; count - 1 without nodes). */
void phys_ragdoll_fix_positions(rx_addr P, uint32_t* ecx, uint32_t* edx) {
    const rx_v4 mask = v4_ld(0x0092db80);
    *ecx = (uint32_t)(I32(P, 0x9390) - 1);
    for (int32_t i = I32(P, 0x9390) - 1; i >= 0; i--) {
        const rx_addr N = P + 0x39e0 + 0x230u * U32(P, 0x9394 + 4u * (uint32_t)i);
        *ecx = N;
        if (U32(N, 0) == 0) continue;
        const rx_addr J = P + 0x1b30 + 0x170u * U32(N, 4);
        const uint32_t a = U32(J, 0), b = U32(J, 0x20);
        *ecx = J;
        const rx_v4 wa = v4_add(anchor_offset(P, a, v4_and(v4_ld(J + 0x10), mask)), v4_ld(P + 0x80u * (a + 1)));
        const rx_v4 rb = anchor_offset(P, b, v4_and(v4_ld(J + 0x30), mask));
        *edx = 0x80u * (a + 1);
        v4_st(P + 0x80u * (b + 1), v4_sub(wa, rb));
    }
}
RX_FUNCTION(0x00423b80, phys_ragdoll_fix_positions)

/* 00423df0 (stdcall P; RET 4): the same for the velocities: body b's linear velocity (P+0x90 + 0x80 b) = a's + the rotated
 * (w x r_a) - the rotated (w_b x r_b) (body-frame angular velocities +0xb10; the exe's cross product phys_cross_ba (r, w)).
 * *ecx / *edx = the registers left (ECX the last node or b * 0x80, EDX a * 0x80 of the last fixed joint; count - 1 without
 * nodes). */
void phys_ragdoll_fix_velocities(rx_addr P, uint32_t* ecx, uint32_t* edx) {
    *ecx = (uint32_t)(I32(P, 0x9390) - 1);
    for (int32_t i = I32(P, 0x9390) - 1; i >= 0; i--) {
        const rx_addr N = P + 0x39e0 + 0x230u * U32(P, 0x9394 + 4u * (uint32_t)i);
        *ecx = N;
        if (U32(N, 0) == 0) continue;
        const rx_addr J = P + 0x1b30 + 0x170u * U32(N, 4);
        const uint32_t a = U32(J, 0), b = U32(J, 0x20);
        const rx_v4 va = phys_cross_ba(v4_ld(J + 0x10), v4_ld(ANG(P, a) + 0xb10));
        const rx_v4 wa = v4_add(anchor_offset(P, a, va), v4_ld(P + 0x90 + 0x80u * a));
        const rx_v4 vb = phys_cross_ba(v4_ld(J + 0x30), v4_ld(ANG(P, b) + 0xb10));
        const rx_v4 rb = anchor_offset(P, b, vb);
        *edx = 0x80u * a; *ecx = 0x80u * b;
        v4_st(P + 0x90 + 0x80u * b, v4_sub(wa, rb));
    }
}
RX_FUNCTION(0x00423df0, phys_ragdoll_fix_velocities)
