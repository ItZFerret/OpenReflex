/* phys_constraint.c -- RavenPhysics constraint (articulated-body node) virtual methods: ConstraintSlidingRotor (the bike's
 * suspension / steering joints) and ConstraintFixedBody. A constraint joins two bodies of the mechanism (each body = the linear
 * record and the angular record of a mechanism node: +0x00 pose (position / orientation quaternion), +0x10 velocity, +0x40
 * resolved force; native/game/notes/mechanism.md):
 *   +0x28 / +0x2c  body A linear / angular record*     +0x30 / +0x34  body B linear / angular record*
 * Slot 2 builds the joint's Jacobian blocks, 3 the right-hand side, 5 applies the constraint force, 6 / 7 project the pose and
 * the velocity, 8 / 9 return the parent's / child's coupling (00858fe0 congruence). Matrix objects as mechanism.md (+0 vtable,
 * +4 rows, +8 cols, rows of vec4 from +0x10). Stack matrices passed to the kernels live at the original's frame offsets. */
#include "phys_constraint.h"
#include "phys_mech.h"
#ifdef RX_CONTACT_FRAME_OBSERVERS
#include "contact_pose_frames.h"
#endif

#define CALL_AT(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)
#define VT_MATRIX3X3 0x0093ca04u

static void matrix3x3_header(rx_addr m) { SU32(m, 4, 3); SU32(m, 8, 3); SU32(m, 0, VT_MATRIX3X3); }
static rx_v4 row(rx_addr m, int r) { return v4_ld(m + 0x10 + 0x10u * (uint32_t)r); }
static void set_row(rx_addr m, int r, rx_v4 v) { v4_st(m + 0x10 + 0x10u * (uint32_t)r, v); }
static rx_v4 neg(rx_v4 v) { return v4_sub(v4_zero(), v); }

/* 00859f90 ConstraintSlidingRotor::vf2 (ECX = this): the Jacobian blocks of the sliding rotor. RA / RB = body A's / B's
 * orientation matrices (00857e50 / 00857d00), the joint frame p = qA * q_joint (+0xb0) and its two transverse axes
 * w1 = p v1 p^-1, w2 = p v2 p^-1 (v1 / v2 = the runtime axis vec4s 0x00d61680 / 0x00d616a0); a1 = RA * (+0x90), a2 = RA *
 * (+0xa0) (the slide axes, +0xf0 / +0x100), d = RA^T (pB - pA). Blocks: +0x140 / +0x150 = -(rows 0, 1 of [a1; a2; 0] RA^T
 * skewT(d)^T), +0x160 / +0x170 = -(rows of [w1; w2; 0] RA^T), +0x1a0 / +0x1b0 = -a1 / -a2, +0x210 / +0x220 = rows of
 * [w1; w2; 0] RB^T; +0x240 = the bias (s1, s2, k1, k2) from the relative velocity: s = c . a with
 * c = (d x wA) x wA - 2 (vB - vA)... in the exe's cross product order, k = (w x wA) . (wB - wA) (wA / wB = the bodies' world
 * angular velocities). Frame: push ebp; and esp, -16; sub esp, 0x27c; push esi (eight Matrix3x3 locals). */
void phys_rotor_build_jacobian(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x27c - 4;
    rx_set_sp(L);
    const rx_addr RA = L + 0x170, RAt = L + 0xf0, RB = L + 0x230, AX = L + 0x130, W = L + 0xb0, T = L + 0x70;
    const rx_addr P = L + 0x1b0, SK = L + 0x1f0, a1 = L + 0x50, a2 = L + 0x30, dl = L + 0x20, dv = L + 0x10;
    const rx_addr mats[8] = {RA, RAt, RB, AX, W, T, P, SK};
    for (int i = 0; i < 8; i++) matrix3x3_header(mats[i]);
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion(RA, U32(self, 0x2c)));
    CALL_AT(L, 1, phys_matrix3x3_transpose(RA, RAt));
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion2(RB, U32(self, 0x34)));
    CALL_AT(L, 1, phys_mat3_mulvec(a1, self + 0x90, RA));
    CALL_AT(L, 1, phys_mat3_mulvec(a2, self + 0xa0, RA));
    /* the joint frame and its transverse axes in world space */
    const rx_v4 p = phys_quat_mul(v4_ld(U32(self, 0x2c)), v4_ld(self + 0xb0));
    const rx_v4 cp = phys_quat_conj(p);
    const rx_v4 w1 = phys_quat_mul(p, phys_quat_mul(v4_xyz(v4_ld(0x00d61680)), cp));
    const rx_v4 w2 = phys_quat_mul(p, phys_quat_mul(v4_xyz(v4_ld(0x00d616a0)), cp));
    v4_st(dv, v4_sub(v4_ld(U32(self, 0x30)), v4_ld(U32(self, 0x28))));
    CALL_AT(L, 1, phys_mat3_mulvec(dl, dv, RAt));
    CALL_AT(L, 1, phys_matrix3x3_skew_t(dl, SK));
    v4_st(self + 0xf0, v4_ld(a1));
    set_row(AX, 0, v4_ld(a1)); set_row(AX, 1, v4_ld(a2)); set_row(AX, 2, v4_ld(0x00d61670));
    set_row(W, 0, w1); set_row(W, 1, w2); set_row(W, 2, v4_ld(0x00d61670));
    v4_st(self + 0x100, v4_ld(a2));
    CALL_AT(L, 1, phys_mat3_mul_abt(T, AX, RAt));            /* T = [a1; a2; 0] RA^T */
    CALL_AT(L, 1, phys_mat3_mul_abt(P, T, SK));              /* P = T skewT(d)^T */
    v4_st(self + 0x140, neg(row(P, 0)));
    v4_st(self + 0x150, neg(row(P, 1)));
    CALL_AT(L, 1, phys_mat3_mul_abt(T, W, RAt));             /* T = [w1; w2; 0] RA^T, negated */
    set_row(T, 0, neg(row(T, 0))); set_row(T, 1, neg(row(T, 1)));
    v4_st(self + 0x160, row(T, 0));
    v4_st(self + 0x170, row(T, 1));
    v4_st(self + 0x1a0, neg(v4_ld(a1)));
    set_row(T, 2, neg(row(T, 2)));
    v4_st(self + 0x1b0, neg(v4_ld(a2)));
    CALL_AT(L, 1, phys_mat3_mul_abt(T, W, RB));              /* T = [w1; w2; 0] RB^T */
    v4_st(self + 0x210, neg(row(T, 0)));
    v4_st(self + 0x220, neg(row(T, 1)));
    /* the bias from the relative motion */
    const rx_v4 qa = v4_ld(U32(self, 0x2c)), qb = v4_ld(U32(self, 0x34));
    const rx_v4 wa = phys_quat_mul_ex(qa, phys_quat_mul(v4_xyz(v4_ld(U32(self, 0x2c) + 0x10)), phys_quat_conj(qa)), 1);
    const rx_v4 rel_v = v4_mul(v4_sub(v4_ld(U32(self, 0x30) + 0x10), v4_ld(U32(self, 0x28) + 0x10)), v4_splat(2.0f));
    const rx_v4 c = phys_cross_ba(v4_sub(phys_cross_ba(v4_ld(dv), wa), rel_v), wa);
    const float s1 = phys_hsum3(v4_mul(c, v4_ld(a1))), s2 = phys_hsum3(v4_mul(c, v4_ld(a2)));
    const rx_v4 wb = phys_quat_mul(qb, phys_quat_mul(v4_xyz(v4_ld(U32(self, 0x34) + 0x10)), phys_quat_conj(qb)));
    const rx_v4 dw = v4_sub(wb, wa);
    const float k1 = phys_hsum3(v4_mul(phys_cross_ba(w1, wa), dw));
    const float k2 = phys_hsum3(v4_mul(phys_cross_ba(w2, wa), dw));
    v4_st(self + 0x240, v4(s1, s2, k1, k2));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00859f90, phys_rotor_build_jacobian)

/* 0085a7e0 ConstraintSlidingRotor::vf3 (ECX = this): the right-hand side: Vec6 fA / fB = each body's (force * inverse mass,
 * torque * inverse inertia) (+0x40 * +0x30 of the linear and angular records); +0x260 = (J_A fA + J_B fB) + bias, with the
 * Jacobians this+0xd0 / this+0x180 (Matrix4x6, 00853a30). Frame: push ebp; and esp, -16; sub esp, 0xbc; push esi
 * (Vec6 locals at +0x50 / +0x80, Vec4 results at +0x30 / +0x10). */
void phys_rotor_build_rhs(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xbc - 4;
    rx_set_sp(L);
    const rx_addr fa = L + 0x50, fb = L + 0x80, ra = L + 0x30, rb = L + 0x10;
    SU32(fa, 0, 0x0093cb00u); SU32(fb, 0, 0x0093cb00u);     /* Vec6 */
    SU32(ra, 0, 0x0093cb2cu); SU32(rb, 0, 0x0093cb2cu);     /* Vec4 */
    const rx_addr bodies[4] = {U32(self, 0x28), U32(self, 0x2c), U32(self, 0x30), U32(self, 0x34)};
    v4_st(fa + 0x10, v4_mul(v4_ld(bodies[0] + 0x40), v4_ld(bodies[0] + 0x30)));
    v4_st(fa + 0x20, v4_mul(v4_ld(bodies[1] + 0x40), v4_ld(bodies[1] + 0x30)));
    SU32(fa, 4, 6); SU32(fb, 4, 6);
    v4_st(fb + 0x10, v4_mul(v4_ld(bodies[2] + 0x40), v4_ld(bodies[2] + 0x30)));
    SU32(ra, 4, 4); SU32(rb, 4, 4);
    v4_st(fb + 0x20, v4_mul(v4_ld(bodies[3] + 0x40), v4_ld(bodies[3] + 0x30)));
    CALL_AT(L, 1, phys_mat4x6_mulvec(fa, ra, self + 0xd0));
    CALL_AT(L, 1, phys_mat4x6_mulvec(fb, rb, self + 0x180));
    v4_st(self + 0x260, v4_add(v4_add(v4_ld(ra + 0x10), v4_ld(rb + 0x10)), v4_ld(self + 0x240)));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085a7e0, phys_rotor_build_rhs)

/* the couplings of slot 8 / 9 (thiscall, no arguments): 00858fe0 congruence of this's blocks; returns the output block.
 * The call is made after push esi and two argument pushes. */
static rx_addr coupling(rx_addr self, uint32_t a, uint32_t b, uint32_t out, uint32_t c) {
    const rx_addr sp0 = rx_sp();
    CALL_AT(sp0 - 4, 2, phys_mat_congruence(self + a, self + b, self + out, self + c));   /* after push esi */
    rx_set_sp(sp0);
    return self + out;
}
/* 0042f970 ConstraintSlidingRotor::vf8: the parent's coupling into this+0x800 (from +0x670, +0x720, +0x5d0) */
rx_addr phys_rotor_parent_coupling(rx_addr self) { return coupling(self, 0x670, 0x720, 0x800, 0x5d0); }
RX_FUNCTION(0x0042f970, phys_rotor_parent_coupling)
/* 0042f9a0 ConstraintSlidingRotor::vf9: the child's coupling into this+0x910 (from +0x490, +0x520, +0x270) */
rx_addr phys_rotor_child_coupling(rx_addr self) { return coupling(self, 0x490, 0x520, 0x910, 0x270); }
RX_FUNCTION(0x0042f9a0, phys_rotor_child_coupling)
/* 0042fcd0 ConstraintFixedBody::vf8: the parent's coupling into this+0xbf0 (from +0x970, +0xa80, +0x750) */
rx_addr phys_fixed_parent_coupling(rx_addr self) { return coupling(self, 0x970, 0xa80, 0xbf0, 0x750); }
RX_FUNCTION(0x0042fcd0, phys_fixed_parent_coupling)
/* 0042fd00 ConstraintFixedBody::vf9: the child's coupling into this+0xd00 (from +0x530, +0x640, +0x310) */
rx_addr phys_fixed_child_coupling(rx_addr self) { return coupling(self, 0x530, 0x640, 0xd00, 0x310); }
RX_FUNCTION(0x0042fd00, phys_fixed_child_coupling)

/* Matrix6x4 local: two Matrix3x4 blocks at +0x10 / +0x50 (vtables 0093cadc / 0093ca28) */
static void matrix6x4_header(rx_addr m) {
    SU32(m, 8, 4);
    SU32(m, 0x10, 0x0093ca28u); SU32(m, 0x14, 3); SU32(m, 0x18, 4);
    SU32(m, 0x50, 0x0093ca28u); SU32(m, 0x54, 3); SU32(m, 0x58, 4);
    SU32(m, 4, 6); SU32(m, 0, 0x0093cadcu);
}
static void vec6_zero(rx_addr v) {
    SU32(v, 0, 0x0093cb00u); SU32(v, 4, 6);
    v4_st(v + 0x10, v4_ld(0x00d61670)); v4_st(v + 0x20, v4_ld(0x00d61670));
}

/* 0085b5a0 ConstraintSlidingRotor::vf5 (ECX = this): applies the constraint impulse lambda (+0x7e0): the transposed Jacobians
 * (vfunc 2 of this+0xd0 / this+0x180 into two Matrix6x4 locals) times lambda (00853e80) into two Vec6 locals, added to the four
 * records' resolved forces +0x40 (A: force + J_A^T lambda (linear), (J_A^T lambda) + torque (angular); B likewise).
 * Frame: push ebp; and esp, -16; sub esp, 0x198; push esi / edi (matrices at +0x70 / +0x100, vectors at +0x40 / +0x10). */
void phys_rotor_apply_impulse(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x198 - 8;
    rx_set_sp(L);
    const rx_addr ja = L + 0x70, jb = L + 0x100, va = L + 0x40, vb = L + 0x10;
    matrix6x4_header(ja); matrix6x4_header(jb);
    vec6_zero(va); vec6_zero(vb);
    phys_vcall1(self + 0xd0, 2, ja);                         /* Matrix4x6 transpose */
    phys_vcall1(self + 0x180, 2, jb);
    CALL_AT(L, 1, phys_mat6x4_mulvec(va, self + 0x7e0, ja));
    CALL_AT(L, 1, phys_mat6x4_mulvec(vb, self + 0x7e0, jb));
    const rx_addr la = U32(self, 0x28) + 0x40, aa = U32(self, 0x2c) + 0x40;
    const rx_addr lb = U32(self, 0x30) + 0x40, ab = U32(self, 0x34) + 0x40;
    v4_st(la, v4_add(v4_ld(la), v4_ld(va + 0x10)));
    v4_st(aa, v4_add(v4_ld(va + 0x20), v4_ld(aa)));
    v4_st(lb, v4_add(v4_ld(lb), v4_ld(vb + 0x10)));
    v4_st(ab, v4_add(v4_ld(vb + 0x20), v4_ld(ab)));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085b5a0, phys_rotor_apply_impulse)

/* 0085a8f0 ConstraintSlidingRotor::vf6 (ECX = this): projects body B's pose onto the joint: the slide axis r1 = RA (+0x80)
 * through A's anchor pA + r2 (r2 = RA (+0x70)): pB = (pA + r2) + ((pB - (pA + r2)) . r1) r1; the rotation: the relative
 * rotation in the joint frame u = conj(qj) (conj(qA) qB) (qj = +0xb0) keeps only its x and w (y, z from the runtime vec4
 * 0x00d61660), normalized (guarded as phys_normalize3, with the dot4 sum), then qB = (qA qj) u. Returns B's angular record (EAX).
 * Frame: push ebp; and esp, -16; sub esp, 0xfc; push esi (RA / RA^T / RB at +0x30 / +0x70 / +0xb0). */
rx_addr phys_rotor_project_pose(rx_addr self) {
    const rx_addr sp0 = rx_sp();
#ifdef RX_CONTACT_FRAME_OBSERVERS
    const rx_addr L = rx_contact_pose_active() ? rx_contact_pose_base() : ((sp0 - 4) & ~15u) - 0xfc - 4;
#else
    const rx_addr L = ((sp0 - 4) & ~15u) - 0xfc - 4;
#endif
    rx_set_sp(L);
    const rx_addr RA = L + 0x30, RAt = L + 0x70, RB = L + 0xb0, r1 = L + 0x10, r2 = L + 0x20;
    matrix3x3_header(RA); matrix3x3_header(RAt); matrix3x3_header(RB);
    #ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_matrix(0,RA,U32(self,0x2c),0);else
#endif
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion(RA, U32(self, 0x2c)));
    #ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_matrix(1,RAt,RA,0);else
#endif
    CALL_AT(L, 1, phys_matrix3x3_transpose(RA, RAt));
    #ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_matrix(2,RB,U32(self,0x34),0);else
#endif
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion2(RB, U32(self, 0x34)));
    #ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_matrix(3,r1,self+0x80,RA);else
#endif
    CALL_AT(L, 1, phys_mat3_mulvec(r1, self + 0x80, RA));
    #ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_matrix(4,r2,self+0x70,RA);else
#endif
    CALL_AT(L, 1, phys_mat3_mulvec(r2, self + 0x70, RA));
    const rx_addr pb = U32(self, 0x30);
    const rx_v4 axis = v4_ld(r1), anchor = v4_add(v4_ld(U32(self, 0x28)), v4_ld(r2));
    const float s = phys_hsum3(v4_mul(v4_sub(v4_ld(pb), anchor), axis));
    v4_st(pb, v4_add(v4_mul(v4_splat(s), axis), anchor));
    const rx_v4 qa = v4_ld(U32(self, 0x2c)), qb = v4_ld(U32(self, 0x34)), qj = v4_ld(self + 0xb0);
    const rx_v4 rel = phys_quat_mul(phys_quat_conj(qa), qb);
#ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_relative(qb);
#endif
    const rx_v4 t = phys_quat_mul(v4_mul(v4(-1.0f, -1.0f, -1.0f, 1.0f), qj), rel);
    const rx_v4 z = v4_ld(0x00d61660);
    const rx_v4 u = v4(t.v[0], z.v[1], z.v[2], t.v[3]);
    /* This site's ee/40/f0/aa sequence is specifically (W+Y)+(Z+X),
     * including NaN operand priority; the generic hsum4 has a different order. */
    const rx_v4 squares = v4_mul(u,u);
    const float n2 = phys_add(phys_add(squares.v[3],squares.v[1]),phys_add(squares.v[2],squares.v[0]));
    const rx_v4 roots = v4_sqrt(v4_splat(n2));
    const rx_v4 finite = v4_mask(n2!=F32(0,0x92dc20),n2!=F32(0,0x92dc24),n2!=F32(0,0x92dc28),n2!=F32(0,0x92dc2c));
    const rx_v4 nonzero = v4_mask(roots.v[0]!=0.f,roots.v[1]!=0.f,roots.v[2]!=0.f,roots.v[3]!=0.f);
    const rx_v4 uh = v4_or(v4_and(v4_and(v4_div(u,roots),nonzero),finite),v4_andn(finite,v4_ld(0x92dc30)));
#ifdef RX_CONTACT_FRAME_OBSERVERS
    if(rx_contact_pose_active())rx_contact_pose_normalization(u,finite);
#endif
    v4_st(U32(self, 0x34), phys_quat_mul(phys_quat_mul(qa, qj), uh));
    rx_set_sp(sp0);
    return U32(self, 0x34);
}
RX_FUNCTION(0x0085a8f0, phys_rotor_project_pose)

#include "fourwheel_nodes_exact.h"
#include "fx_util.h"
#include <stdlib.h>

/* 0085ac20 ConstraintSlidingRotor::vf7 (ECX = this): projects body B's velocity onto the joint. With the joint frame
 * P = qA qj (qj = +0xb0) and the world angular velocities wA = qA (wA_local) qA^-1, wB = qB (wB_local) qB^-1:
 *   the hinge axis h = P v P^-1 (v = the runtime vec4 0x00d615c0, xyz), wB' = xyz(((wB - wA) . h) h + wA), and B's stored
 *   angular velocity becomes P^-1 (wB' P) (sic: the joint frame, not B's own orientation);
 *   the slide axis r1 = qA (+0x80) qA^-1, c = wA x (pB - pA) (the exe's cross order), and vB = c + (((vB - vA) - c) . r1) r1 + vA.
 * No calls and no stack objects (the frame only spills temporaries). EAX = this on return. */
static rx_addr rotor_project_velocity(rx_addr self, int exact) {
    const rx_addr la = U32(self, 0x28), aa = U32(self, 0x2c), lb = U32(self, 0x30), ab = U32(self, 0x34);
    fx_frame f;
    if (exact) {
        const rx_addr entry = rx_sp(), bottom = ((entry - 4) & ~15u) - 0x70;
        /* The native vf15 binding is for real heap-owned nodes/bodies, never
         * guest stack aliases. Fail rather than claiming broader alias parity. */
        const rx_addr p[] = {self, la, aa, lb, ab};
        const uint32_t n[] = {0xc0, 0x20, 0x20, 0x20, 0x20};
        for (unsigned i=0; i<5; ++i) if (p[i] < entry && p[i] + n[i] > bottom) abort();
        fx_frame_enter(&f, 0x68, 16, FX_ESI, FX_EDI);
        fx_set_reg(FX_EAX, self); fx_set_reg(FX_EDX, aa); fx_set_reg(FX_ECX, lb);
        fx_set_reg(FX_ESI, ab); fx_set_reg(FX_EDI, la);
    }
    const rx_v4 qa = v4_ld(aa), qb = v4_ld(ab);
    const rx_v4 frame = phys_quat_mul(qa, v4_ld(self + 0xb0));
    const rx_v4 wa = phys_quat_mul(qa, phys_quat_mul(v4_xyz(v4_ld(aa + 0x10)), phys_quat_conj(qa)));
    phys_quat_observation wb_spills, hinge_spills;
    const rx_v4 wb = phys_quat_mul_observed(qb, phys_quat_mul(v4_xyz(v4_ld(ab + 0x10)), phys_quat_conj(qb)), 1, exact ? &wb_spills : NULL);
    const rx_v4 cframe = phys_quat_conj(frame);
    const rx_v4 hinge = phys_quat_mul(frame, phys_quat_mul_observed(v4_xyz(v4_ld(0x00d615c0)), cframe, 0, exact ? &hinge_spills : NULL));
    if (exact) {
        /* Final values in all six written spill vectors. These are snapshots
         * of the actual single canonical computations above, not a second
         * response calculation. Transient register/SIMD states are not exposed. */
        v4_st(f.E + 0x60, wb_spills.raw_x); /* 0085ade1 */
        v4_st(f.E + 0x50, wb_spills.raw_y); /* 0085adf4 */
        v4_st(f.E + 0x40, wb_spills.r2);    /* 0085ae0e */
        v4_st(f.E + 0x20, hinge_spills.zy); /* final 0085ae86 */
        v4_st(f.E + 0x30, hinge_spills.value); /* final 0085ae93 */
        v4_st(f.E + 0x10, hinge);          /* final 0085af00 */
    }
    const float s = phys_hsum3(v4_mul(v4_sub(wb, wa), hinge));
    const rx_v4 wb_new = v4_xyz(v4_add(v4_mul(v4_splat(s), hinge), wa));
    v4_st(ab + 0x10, phys_quat_mul(cframe, phys_quat_mul(wb_new, frame)));
    /* the slide axis and the linear velocity */
    const rx_v4 axis = phys_quat_mul(qa, phys_quat_mul(v4_xyz(v4_ld(self + 0x80)), phys_quat_conj(qa)));
    const rx_v4 va = v4_ld(la + 0x10);
    const rx_v4 c = phys_cross_ba(v4_sub(v4_ld(lb), v4_ld(la)), wa);
    const float t = phys_hsum3(v4_mul(v4_sub(v4_sub(v4_ld(lb + 0x10), va), c), axis));
    v4_st(lb + 0x10, v4_add(c, v4_add(v4_mul(v4_splat(t), axis), va)));
    if (exact) fx_frame_leave(&f);
    return self;
}
rx_addr phys_rotor_project_velocity(rx_addr self) { return rotor_project_velocity(self, 0); }
rx_addr phys_rotor_project_velocity_exact(rx_addr self) { return rotor_project_velocity(self, 1); }
RX_FUNCTION(0x0085ac20, phys_rotor_project_velocity)

/* ---- ConstraintFixedBody: body B welded to body A at the offset r (+0x70, A's frame) with the relative orientation +0x80.
 * Jacobian blocks (Matrix3x3, +0xa0..+0x270), Matrix6x6 Jacobians this+0x90 / this+0x1a0 (00853.. layouts), the bias +0x2c0,
 * the right-hand side +0x2f0, lambda +0xb90. */
#include "phys_misc.h"

/* 0085b750 ConstraintFixedBody::vf1 (ECX = this): snaps B onto A: qB = qA q_rel (+0x80), pB = qA r qA^-1 + pA. */
void phys_fixed_project_pose(rx_addr self) {
    const rx_v4 qa = v4_ld(U32(self, 0x2c));
    v4_st(U32(self, 0x34), phys_quat_mul(qa, v4_ld(self + 0x80)));
    const rx_v4 qa2 = v4_ld(U32(self, 0x2c));               /* reloaded: B's record may alias */
    const rx_v4 r = phys_quat_mul(qa2, phys_quat_mul(v4_xyz(v4_ld(self + 0x70)), phys_quat_conj(qa2)));
    v4_st(U32(self, 0x30), v4_add(r, v4_ld(U32(self, 0x28))));
}
RX_FUNCTION(0x0085b750, phys_fixed_project_pose)

/* 0085b8b0 ConstraintFixedBody::vf2 (ECX = this): the Jacobian blocks. RA / RB = the bodies' orientation matrices (both
 * 00857e50), K = skewT(r) (into the stack matrix that first held RA^T): +0xe0 = -(RA K^T), +0x120 = 0, +0x160 = -RA,
 * +0xa0 = rows (v0, v1, v2) and +0x1b0 = -(v0, v1, v2) (the runtime axis vec4s 0x00d615c0 / 0x00d616a0 / 0x00d61680),
 * +0x1f0 = +0x230 = 0, +0x270 = RB (Matrix3x3 vfuncs 1 clear / 3 copy / 6 negate); the bias +0x2c0 = RA (w x (w x r))
 * (w = A's local angular velocity, the exe's cross order), +0x2d0 = 0. Frame: push ebp; and esp, -16; sub esp, 0x134;
 * push ebx / esi / edi (RA^T +0x10, RA +0x50, cross +0xa0, result +0x90, RB +0xb0, RB^T +0xf0). Returns RA's stack address
 * (the last kernel's EAX; its EDX = the cross vector, RA + 0x50). */
rx_addr phys_fixed_build_jacobian(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x134 - 12;
    rx_set_sp(L);
    const rx_addr K = L + 0x10, RA = L + 0x50, RB = L + 0xb0, RBt = L + 0xf0, res = L + 0x90, cr = L + 0xa0;
    matrix3x3_header(RA); matrix3x3_header(RB); matrix3x3_header(K); matrix3x3_header(RBt);
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion(RA, U32(self, 0x2c)));
    CALL_AT(L, 1, phys_matrix3x3_from_quaternion(RB, U32(self, 0x34)));
    CALL_AT(L, 1, phys_matrix3x3_transpose(RA, K));          /* RA^T, dead: overwritten by K */
    CALL_AT(L, 1, phys_matrix3x3_transpose(RB, RBt));
    matrix3x3_header(K);
    CALL_AT(L, 1, phys_matrix3x3_skew_t(self + 0x70, K));
    static const rx_addr axes[3] = {0x00d615c0u, 0x00d616a0u, 0x00d61680u};
    for (int i = 0; i < 3; i++) set_row(self + 0xa0, i, v4_ld(axes[i]));
    CALL_AT(L, 1, phys_mat3_mul_abt(self + 0xe0, RA, K));
    phys_vcall0(self + 0xe0, 6);
    phys_vcall0(self + 0x120, 1);
    phys_vcall1(self + 0x160, 3, RA);
    phys_vcall0(self + 0x160, 6);
    for (int i = 0; i < 3; i++) set_row(self + 0x1b0, i, v4_ld(axes[i]));
    phys_vcall0(self + 0x1b0, 6);
    phys_vcall0(self + 0x1f0, 1);
    phys_vcall0(self + 0x230, 1);
    phys_vcall1(self + 0x270, 3, RB);
    const rx_v4 w = v4_ld(U32(self, 0x2c) + 0x10);
    v4_st(cr, phys_cross_ba(phys_cross_ba(v4_ld(self + 0x70), w), w));
    CALL_AT(L, 1, phys_mat3_mulvec(res, cr, RA));
    v4_st(self + 0x2c0, v4_ld(res));
    v4_st(self + 0x2d0, v4_ld(0x00d61670));
    rx_set_sp(sp0);
    return RA;
}
RX_FUNCTION(0x0085b8b0, phys_fixed_build_jacobian)

/* 0085bb00 ConstraintFixedBody::vf3 (ECX = this): the right-hand side, as the rotor's with 6-row blocks: Vec6 fA / fB = each
 * body's (force * inverse mass, torque * inverse inertia); rA = J_A fA, rB = J_B fB (Mat6_MulVec, J_A = this+0x90,
 * J_B = this+0x1a0); +0x2f0 / +0x300 = (rA + rB) + bias (+0x2c0 / +0x2d0). Frame: push ebp; and esp, -16; sub esp, 0xdc;
 * push esi (fB +0x10, rA +0x40, fA +0x70, rB +0xa0). Returns fB's stack address (the last kernel's EDX). */
rx_addr phys_fixed_build_rhs(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xdc - 4;
    rx_set_sp(L);
    const rx_addr fb = L + 0x10, ra = L + 0x40, fa = L + 0x70, rb = L + 0xa0;
    const rx_addr vec6[4] = {fa, fb, ra, rb};
    for (int i = 0; i < 4; i++) SU32(vec6[i], 0, 0x0093cb00u);
    const rx_addr bodies[4] = {U32(self, 0x28), U32(self, 0x2c), U32(self, 0x30), U32(self, 0x34)};
    v4_st(fa + 0x10, v4_mul(v4_ld(bodies[0] + 0x40), v4_ld(bodies[0] + 0x30)));
    v4_st(fa + 0x20, v4_mul(v4_ld(bodies[1] + 0x40), v4_ld(bodies[1] + 0x30)));
    v4_st(fb + 0x10, v4_mul(v4_ld(bodies[2] + 0x40), v4_ld(bodies[2] + 0x30)));
    for (int i = 0; i < 4; i++) SU32(vec6[i], 4, 6);
    v4_st(fb + 0x20, v4_mul(v4_ld(bodies[3] + 0x40), v4_ld(bodies[3] + 0x30)));
    CALL_AT(L, 1, phys_mat6_mulvec(ra, fa, self + 0x90));
    CALL_AT(L, 1, phys_mat6_mulvec(rb, fb, self + 0x1a0));
    v4_st(self + 0x2f0, v4_add(v4_add(v4_ld(ra + 0x10), v4_ld(rb + 0x10)), v4_ld(self + 0x2c0)));
    v4_st(self + 0x300, v4_add(v4_add(v4_ld(ra + 0x20), v4_ld(rb + 0x20)), v4_ld(self + 0x2d0)));
    rx_set_sp(sp0);
    return fb;
}
RX_FUNCTION(0x0085bb00, phys_fixed_build_rhs)

/* 0085c0d0 ConstraintFixedBody::vf5 (ECX = this): applies lambda (+0xb90): Matrix6x6 locals (0042f1d0) = the transposed
 * Jacobians (vfunc 2 of this+0x90 / this+0x1a0), vA = J_A^T lambda, vB = J_B^T lambda (Mat6_MulVec) added to the records'
 * resolved forces +0x40. Frame: push ebp; and esp, -16; sub esp, 0x29c; push esi (vB +0x10, vA +0x40, J_A^T +0x70,
 * J_B^T +0x180). */
void phys_fixed_apply_impulse(rx_addr self) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x29c - 4;
    rx_set_sp(L);
    const rx_addr vb = L + 0x10, va = L + 0x40, ja = L + 0x70, jb = L + 0x180;
    CALL_AT(L, 0, phys_matrix6x6_construct(ja));
    CALL_AT(L, 0, phys_matrix6x6_construct(jb));
    vec6_zero(va); vec6_zero(vb);
    phys_vcall1(self + 0x90, 2, ja);
    phys_vcall1(self + 0x1a0, 2, jb);
    CALL_AT(L, 1, phys_mat6_mulvec(va, self + 0xb90, ja));
    CALL_AT(L, 1, phys_mat6_mulvec(vb, self + 0xb90, jb));
    const rx_addr la = U32(self, 0x28) + 0x40, aa = U32(self, 0x2c) + 0x40;
    const rx_addr lb = U32(self, 0x30) + 0x40, ab = U32(self, 0x34) + 0x40;
    v4_st(la, v4_add(v4_ld(la), v4_ld(va + 0x10)));
    v4_st(aa, v4_add(v4_ld(aa), v4_ld(va + 0x20)));
    v4_st(lb, v4_add(v4_ld(lb), v4_ld(vb + 0x10)));
    v4_st(ab, v4_add(v4_ld(ab), v4_ld(vb + 0x20)));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0085c0d0, phys_fixed_apply_impulse)

/* 0085bc30 ConstraintFixedBody::vf7 (ECX = this): B's velocity follows A: with A's world angular velocity
 * wA = xyz(qA w_local qA^-1), B's stored angular velocity = qB^-1 (wA qB); vB = qA (w_local x r) qA^-1 + vA (the exe's cross
 * order; the last product splats qA.z first). No calls, no stack objects. */
void phys_fixed_project_velocity(rx_addr self) {
    const rx_addr aa = U32(self, 0x2c), ab = U32(self, 0x34);
    const rx_v4 qa = v4_ld(aa), w_local = v4_ld(aa + 0x10);
    const rx_v4 wa = v4_xyz(phys_quat_mul(qa, phys_quat_mul(v4_xyz(w_local), phys_quat_conj(qa))));
    const rx_v4 qb = v4_ld(ab);
    v4_st(ab + 0x10, phys_quat_mul(phys_quat_conj(qb), phys_quat_mul(wa, qb)));
    const rx_v4 c = phys_cross_ba(v4_ld(self + 0x70), v4_ld(aa + 0x10));
    const rx_v4 qa2 = v4_ld(aa);
    const rx_v4 v = phys_quat_mul_ex(qa2, phys_quat_mul(c, phys_quat_conj(qa2)), 1);
    v4_st(U32(self, 0x30) + 0x10, v4_add(v, v4_ld(U32(self, 0x28) + 0x10)));
}
RX_FUNCTION(0x0085bc30, phys_fixed_project_velocity)
