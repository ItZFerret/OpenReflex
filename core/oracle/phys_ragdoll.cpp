// phys_ragdoll.cpp -- oracle adapters for core/game/phys_ragdoll.c (LTCG registers; callers read EDX).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_ragdoll.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

static uint32_t cookie() { return rx_rd32(0x9b45e4); }

// 00421fa0 (stdcall P; RET 4): EDX = the last product's vector, ECX = the cookie
RX_PORT(0x00421fa0, Ragdoll_JointJacobians, "rx_phys", OUT_ECX | OUT_EDX) {
    m.r[2] = phys_ragdoll_joint_jacobians(m.arg(0), m.r[2]); m.r[1] = cookie(); return m.ret(4);
}
// 00422850 (EDI P, xmm0 h): EDX = the body count (0 without bodies)
RX_PORT(0x00422850, Ragdoll_ExternalForces, "rx_phys", OUT_ECX | OUT_EDX) {
    m.r[2] = phys_ragdoll_external_forces(m.r[7], xmm(m, 0).v[0]); m.r[1] = cookie(); return m.ret(0);
}
// 00422960 (stdcall P; RET 4): EDX = the last helper call's (or the node's) EDX, ECX = the cookie
RX_PORT(0x00422960, Ragdoll_NodeInertia, "rx_phys", OUT_ECX | OUT_EDX) {
    m.r[2] = phys_ragdoll_node_inertia(m.arg(0), m.r[2]); m.r[1] = cookie(); return m.ret(4);
}
// 004232b0 (ESI P): no register output is read (the xmm4 the liveness lists is the integrator's scratch)
RX_PORT(0x004232b0, Ragdoll_SolveForces, "rx_phys", 0) { phys_ragdoll_solve_forces(m.r[6]); return m.ret(0); }
// 004238a0 (ECX P, xmm0 dt)
RX_PORT(0x004238a0, Ragdoll_Step, "rx_phys", 0) { phys_ragdoll_step(m.r[1], xmm(m, 0).v[0]); return m.ret(0); }
// 00423b80 / 00423df0 (stdcall P; RET 4): EAX = P, ECX / EDX as left (callers read 00423df0's)
RX_PORT(0x00423b80, Ragdoll_FixPositions, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.arg(0);
    uint32_t ecx = m.r[1], edx = m.r[2];
    phys_ragdoll_fix_positions(P, &ecx, &edx); m.r[0] = P; m.r[1] = ecx; m.r[2] = edx; return m.ret(4);
}
RX_PORT(0x00423df0, Ragdoll_FixVelocities, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t P = m.arg(0);
    uint32_t ecx = m.r[1], edx = m.r[2];
    phys_ragdoll_fix_velocities(P, &ecx, &edx); m.r[0] = P; m.r[1] = ecx; m.r[2] = edx; return m.ret(4);
}
