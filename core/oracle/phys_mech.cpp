// phys_mech.cpp -- oracle adapters for core/game/phys_mech_matrix.c and phys_mech.c (RavenPhysics mechanism package).
// Thiscall: ECX = this, stack arguments m.arg(i); EAX and the other registers as the original leaves them.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_mech.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// ---- matrix / vector virtual methods (address-taken: callers read EAX) ----
RX_PORT(0x00857c30, Matrix3x3_Clear, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix3x3_clear(m.r[1]); return m.ret(0); }
RX_PORT(0x00857c90, Matrix3x3_Transpose, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix3x3_transpose(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00857bf0, Matrix3x3_Copy, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t src = m.arg(0), cols = U32((src + 8), 0);
    m.r[0] = phys_matrix3x3_copy(m.r[1], src); m.r[2] = cols; m.r[1] = src; return m.ret(4);
}
RX_PORT(0x00858160, Matrix3x3_Sub, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t b = m.arg(1);
    m.r[0] = phys_matrix3x3_sub(m.r[1], m.arg(0), b); m.r[2] = b; return m.ret(8);
}
RX_PORT(0x008581a0, Matrix3x3_Add, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t b = m.arg(1);
    m.r[0] = phys_matrix3x3_add(m.r[1], m.arg(0), b); m.r[2] = b; return m.ret(8);
}
RX_PORT(0x008581e0, Matrix3x3_Negate, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix3x3_negate(m.r[1]); return m.ret(0); }
RX_PORT(0x00858220, Matrix3x4_Transpose, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix3x4_transpose(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00858440, Matrix4x3_Transpose, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1], edx = U32((self + 0x38), 0), ecx = U32((self + 0x48), 0);
    m.r[0] = phys_matrix4x3_transpose(self, m.arg(0)); m.r[2] = edx; m.r[1] = ecx; return m.ret(4);
}
RX_PORT(0x00858660, Matrix4x3_Sub, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t b = m.arg(1);
    m.r[0] = phys_matrix4x3_sub(m.r[1], m.arg(0), b); m.r[2] = b; return m.ret(8);
}
RX_PORT(0x008583c0, Matrix4x3_Add, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t b = m.arg(1);
    m.r[0] = phys_matrix4x3_add(m.r[1], m.arg(0), b); m.r[2] = b; return m.ret(8);
}
RX_PORT(0x00858620, Matrix4x4_Clear, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix4x4_clear(m.r[1]); return m.ret(0); }
RX_PORT(0x00858a80, Matrix4x6_Transpose, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix4x6_transpose(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00858bd0, Matrix6x4_Transpose, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix6x4_transpose(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x008588d0, Matrix4x6_CopyFrom, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t src = m.r[1], edx = U32((src + 0x68), 0);
    m.r[0] = phys_matrix4x6_copy_from(src, m.arg(0)); m.r[2] = edx; return m.ret(4);
}
RX_PORT(0x008588c0, Matrix4x6_Copy, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t src = m.arg(0), edx = U32((src + 0x68), 0);
    m.r[0] = phys_matrix4x6_copy(m.r[1], src); m.r[2] = edx; m.r[1] = src; return m.ret(4);
}
RX_PORT(0x00858e80, Matrix5x6_Clear, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix5x6_clear(m.r[1]); return m.ret(0); }
RX_PORT(0x00858f80, Matrix5x6_Transpose, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix5x6_transpose(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00858c70, Matrix5x6_Sub, "rx_phys", OUT_EAX) { m.r[0] = phys_matrix5x6_sub(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00858d80, Matrix5x6_Copy, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t src = m.arg(0), edx = U32((src + 0xd8), 0);
    m.r[0] = phys_matrix5x6_copy(m.r[1], src); m.r[2] = edx; m.r[1] = src; return m.ret(4);
}
RX_PORT(0x0042f2e0, Vec6_Clear, "rx_phys", OUT_EAX) { m.r[0] = phys_vec6_clear(m.r[1]); return m.ret(0); }
RX_PORT(0x0042f310, Vec6_Copy, "rx_phys", OUT_EAX) {
    const uint32_t src = m.arg(0);
    m.r[0] = phys_vec6_copy(m.r[1], src); m.r[1] = src; return m.ret(4);
}
RX_PORT(0x0042f3b0, Vec3_Copy, "rx_phys", OUT_EAX) { m.r[0] = phys_vec3_copy(m.r[1], m.arg(0)); return m.ret(4); }

// ---- mechanism node passes (LTCG: see each comment) ----
RX_PORT(0x0085c6d0, Mechanism_NodesSlot2, "rx_phys", 0) { phys_mech_nodes_slot2(m.arg(0)); return m.ret(4); }
RX_PORT(0x0085c700, Mechanism_NodesSlot3, "rx_phys", 0) { phys_mech_nodes_slot3(m.arg(0)); return m.ret(4); }
// 0085d300 / 0085d330: EAX = the last callee's EAX (untouched when no non-root node exists); EDX is the callee's clobber.
RX_PORT(0x0085d300, Mechanism_NodesSlot6, "rx_phys", OUT_EAX) {
    const uint32_t M = m.arg(0); const uint32_t r = phys_mech_nodes_slot6(M); if (int32_t(U32(M, 0x1c)) > 1) m.r[0] = r; return m.ret(4);
}
RX_PORT(0x0085d330, Mechanism_NodesSlot7, "rx_phys", OUT_EAX) {
    const uint32_t M = m.arg(0); const uint32_t r = phys_mech_nodes_slot7(M); if (int32_t(U32(M, 0x1c)) > 1) m.r[0] = r; return m.ret(4);
}

// 0085c9d0 (EDX = M): ECX = node count, EAX = M + 0x70 + count * 0xa0 (when count > 0).
RX_PORT(0x0085c9d0, Mechanism_ClearForces, "rx_phys", OUT_EDX) {
    const uint32_t M = m.r[2];
    phys_mech_clear_forces(M);
    const int32_t n = int32_t(U32(M, 0x1c));
    if (n > 0) { m.r[1] = uint32_t(n); m.r[0] = M + 0x70 + 0xa0u * uint32_t(n); } else m.r[1] = 0;
    return m.ret(0);
}
// 0085c500 (EDX = M, ESI = vec4): ECX = count, EAX = M + 0x20 + count * 0xa0 (when count > 0).
RX_PORT(0x0085c500, Mechanism_Translate, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t M = m.r[2];
    phys_mech_translate(M, m.r[6]);
    const int32_t n = int32_t(U32(M, 0x1c));
    if (n > 0) { m.r[1] = uint32_t(n); m.r[0] = M + 0x20 + 0xa0u * uint32_t(n); } else m.r[1] = 0;
    return m.ret(0);
}
// 0085c4b0 (EDX = M, ESI = vec4): ECX = count, EAX = M + 0x90 + count * 0xa0 (when count > 0).
RX_PORT(0x0085c4b0, Mechanism_SetVelocity, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t M = m.r[2];
    phys_mech_set_velocity(M, m.r[6]);
    const int32_t n = int32_t(U32(M, 0x1c));
    if (n > 0) { m.r[1] = uint32_t(n); m.r[0] = M + 0x90 + 0xa0u * uint32_t(n); } else m.r[1] = 0;
    return m.ret(0);
}
// 0085c730 / 0085c750 (ECX = M, EDX = vec4, stack arg = node index): EAX = index * 0xa0.
RX_PORT(0x0085c730, Mechanism_SetNodeLinearVelocity, "rx_phys", OUT_EAX | OUT_EDX) {
    m.r[0] = phys_mech_set_node_linear_velocity(m.r[1], m.r[2], m.arg(0)); return m.ret(4);
}
RX_PORT(0x0085c750, Mechanism_SetNodeAngularVelocity, "rx_phys", OUT_EAX | OUT_EDX) {
    m.r[0] = phys_mech_set_node_angular_velocity(m.r[1], m.r[2], m.arg(0)); return m.ret(4);
}
// 0085ca40 (EDI = M, stack arg dt)
RX_PORT(0x0085ca40, Mechanism_ComposeForces, "rx_phys", 0) { phys_mech_compose_forces(m.r[7], argf(m, 0)); return m.ret(4); }
// 0085d100 (ECX = M, XMM6 lane 0 = dt)
RX_PORT(0x0085d100, Mechanism_IntegratePoses, "rx_phys", 0) { phys_mech_integrate_poses(m.r[1], xmm(m, 6).v[0]); return m.ret(0); }
// 0085c770 (ECX = force, EDX = M, ESI = point, stack arg = node index): EAX = M + index * 0xa0.
RX_PORT(0x0085c770, Mechanism_AddForceAtPoint, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    m.r[0] = phys_mech_add_force_at_point(m.r[1], m.r[2], m.r[6], m.arg(0)); return m.ret(4);
}
// 0085c8b0 (ECX = M; stack index, force): EAX = M + index * 0xa0, ECX = force; EDX unchanged.
RX_PORT(0x0085c8b0, Mechanism_AddBodyForce, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t force = m.arg(1);
    m.r[0] = phys_mech_add_body_force(m.r[1], m.arg(0), force);
    m.r[1] = force;
    return m.ret(8);
}
// 0085c530 (EDX = M, ESI = rotation quaternion): ECX = count, EAX = M + 0x20 + count * 0xa0 and XMM6 = the root position as the
// last iteration loaded it (count > 0): the position before the call when count == 1, else the rotated root's.
RX_PORT(0x0085c530, Mechanism_Rotate, "rx_phys", OUT_EAX | OUT_EDX | (reflex_port::OUT_XMM0 << 6)) {
    const uint32_t M = m.r[2];
    const rx_v4 before = rx_ld4(M + 0x20);
    phys_mech_rotate(M, m.r[6]);
    const int32_t n = int32_t(U32(M, 0x1c));
    if (n > 0) {
        m.r[1] = uint32_t(n); m.r[0] = M + 0x20 + 0xa0u * uint32_t(n);
        set_xmm(m, 6, n == 1 ? before : rx_ld4(M + 0x20));
    } else m.r[1] = 0;
    return m.ret(0);
}

// ---- small helpers ----
// 00852f40 (EAX = out vec3 or 0, ECX = v vec3, stack arg = axis, XMM1 lane 0 = angle): EAX = 1 (0 when out is null).
RX_PORT(0x00852f40, Vec3_RotateAboutAxis, "rx_phys", OUT_ECX) {
    m.r[0] = phys_vec3_rotate_about_axis(m.r[0], m.r[1], m.arg(0), xmm(m, 1).v[0]); return m.ret(0);
}
// 00853220: builds the noise tables (no inputs, no outputs).
RX_PORT(0x00853220, Noise1D_InitTables, "rx_phys", 0) { phys_noise_init_tables(); return m.ret(0); }
// 00853180 (stack arg x, cdecl): XMM0 = (result, 0, 0, 0).
RX_PORT(0x00853180, Noise1D, "rx_phys", reflex_port::OUT_XMM0) {
    const float r = phys_noise_1d(argf(m, 0));
    rx_v4 x0 = {{r, 0.0f, 0.0f, 0.0f}}; set_xmm(m, 0, x0); return m.ret(0);
}
// 008523b0 (EDI = p0, EDX = p1, stack args q and out, ESI = plane normal): EAX = hit; ECX = p0; EDX = q after the second call
// (p1 when the segment is parallel to the plane and the second call was skipped).
RX_PORT(0x008523b0, Segment_PlaneIntersect, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t p0 = m.r[7], p1 = m.r[2], q = m.arg(0);
    m.r[0] = phys_segment_plane_intersect(p0, p1, q, m.arg(1), m.r[6]);
    m.r[1] = p0; m.r[2] = q;
    return m.ret(0);
}
// 0085dcf0 (ESI = contact set, stack arg = out): EAX = out; EDX = loop count (0 when empty); ECX = end pointer / count.
RX_PORT(0x0085dcf0, Contacts_Average, "rx_phys", OUT_ECX) {
    const uint32_t S = m.r[6], out = m.arg(0);
    const int32_t n = int32_t(U32(S, 0));
    m.r[0] = phys_contacts_average(S, out);
    if (n > 0) { m.r[1] = S + 0x20 + 0x30u * uint32_t(n); m.r[2] = uint32_t(n); } else { m.r[1] = uint32_t(n); m.r[2] = 0; }
    return m.ret(4);
}

// ---- preparation / solve and their dispatchers (LTCG registers; nothing live afterwards but ECX unchanged where declared) ----
RX_PORT(0x0085cb70, Mechanism_Prepare, "rx_phys", 0) { phys_mech_prepare(m.arg(0)); return m.ret(4); }
RX_PORT(0x0085cf80, Mechanism_Solve, "rx_phys", 0) { phys_mech_solve(m.arg(0)); return m.ret(4); }
// 00857b30 (ECX = A, stack arg x, ESI = Y)
RX_PORT(0x00857b30, Mat6_MulVecSub, "rx_phys", 0) { phys_mat6_mulvec_sub(m.r[1], m.arg(0), m.r[6]); return m.ret(0); }
// 00857590 (ECX = A, EDX = x, stack arg Y); ECX is kept
// (the kernels run through rx_call_original_ex, which loads every register: ECX is put back)
RX_PORT(0x00857590, Mat_MulVecSub, "rx_phys", OUT_ECX) {
    const uint32_t ecx = m.r[1]; phys_mat_mulvec_sub(ecx, m.r[2], m.arg(0)); m.r[1] = ecx; return m.ret(0);
}
// 00853900 (EAX = A, EDX = x, ESI = y); ECX is kept
RX_PORT(0x00853900, Mat_MulVec, "rx_phys", OUT_ECX) {
    const uint32_t ecx = m.r[1]; phys_mat_mulvec(m.r[0], m.r[2], m.r[6]); m.r[1] = ecx; return m.ret(0);
}
// 00853740 (ECX = B, EDI = A, ESI = out)
RX_PORT(0x00853740, Mat3_Mul, "rx_phys", 0) { phys_mat3_mul(m.r[1], m.r[7], m.r[6]); return m.ret(0); }
// 00858fe0 (ECX = A, EDX = B, stack args C and D)
RX_PORT(0x00858fe0, Mat_Congruence, "rx_phys", 0) { phys_mat_congruence(m.r[1], m.r[2], m.arg(0), m.arg(1)); return m.ret(0); }
// 00855740 (EAX = A, ECX = B, EDI = C)
RX_PORT(0x00855740, Mat_Coupling, "rx_phys", 0) { phys_mat_coupling(m.r[0], m.r[1], m.r[7]); return m.ret(0); }
