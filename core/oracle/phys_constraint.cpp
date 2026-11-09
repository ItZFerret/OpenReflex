// phys_constraint.cpp -- oracle adapters for core/game/phys_constraint.c (constraint vfuncs; thiscall, no stack arguments).
// The vfuncs are address-taken (liveness: EAX, EDX). Registers left by a callee kernel that the port cannot reproduce
// (EAX / EDX of vf3, EDX of vf5 / vf8 / vf9) are not declared.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_constraint.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_EDX;

// vf2: EDX / EAX = the bias's k1 / k2 bits (spilled through the stack on the way to +0x240)
RX_PORT(0x00859f90, ConstraintSlidingRotor_vf2, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1]; phys_rotor_build_jacobian(self);
    m.r[0] = U32((self + 0x24c), 0); m.r[2] = U32((self + 0x248), 0); return m.ret(0);
}
RX_PORT(0x0085a7e0, ConstraintSlidingRotor_vf3, "rx_phys", 0) { phys_rotor_build_rhs(m.r[1]); return m.ret(0); }
// vf5: EAX = B's linear record + 0x40 (EDX = what the Mat6x4_MulVec kernel leaves: not declared)
RX_PORT(0x0085b5a0, ConstraintSlidingRotor_vf5, "rx_phys", OUT_EAX) {
    const uint32_t self = m.r[1]; phys_rotor_apply_impulse(self);
    m.r[0] = U32((self + 0x30), 0) + 0x40; return m.ret(0);
}
// vf6: EDX = lane z of the runtime vec4 0x00d61660 (read through its stack copy)
RX_PORT(0x0085a8f0, ConstraintSlidingRotor_vf6, "rx_phys", OUT_EAX | OUT_EDX) {
    m.r[0] = phys_rotor_project_pose(m.r[1]); m.r[2] = U32(0x00d61668u, 0); return m.ret(0);
}
// vf7: EAX = this, EDX = A's angular record
RX_PORT(0x0085ac20, ConstraintSlidingRotor_vf7, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1];
    m.r[0] = phys_rotor_project_velocity(self); m.r[2] = U32((self + 0x2c), 0); return m.ret(0);
}
RX_PORT(0x0042f970, ConstraintSlidingRotor_vf8, "rx_phys", OUT_EAX) { m.r[0] = phys_rotor_parent_coupling(m.r[1]); return m.ret(0); }
RX_PORT(0x0042f9a0, ConstraintSlidingRotor_vf9, "rx_phys", OUT_EAX) { m.r[0] = phys_rotor_child_coupling(m.r[1]); return m.ret(0); }
RX_PORT(0x0042fcd0, ConstraintFixedBody_vf8, "rx_phys", OUT_EAX) { m.r[0] = phys_fixed_parent_coupling(m.r[1]); return m.ret(0); }
RX_PORT(0x0042fd00, ConstraintFixedBody_vf9, "rx_phys", OUT_EAX) { m.r[0] = phys_fixed_child_coupling(m.r[1]); return m.ret(0); }

// FixedBody vf1: EAX = B's linear record, EDX = A's linear record
RX_PORT(0x0085b750, ConstraintFixedBody_vf1, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1]; phys_fixed_project_pose(self);
    m.r[0] = U32((self + 0x30), 0); m.r[2] = U32((self + 0x28), 0); return m.ret(0);
}
// vf2: EAX = the stack RA matrix (Mat3_MulVec's A), EDX = the stack cross vector (its x)
RX_PORT(0x0085b8b0, ConstraintFixedBody_vf2, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t ra = phys_fixed_build_jacobian(m.r[1]); m.r[0] = ra; m.r[2] = ra + 0x50; return m.ret(0);
}
// vf3: EAX = J_B (this + 0x1a0), EDX = the stack fB vector
RX_PORT(0x0085bb00, ConstraintFixedBody_vf3, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1]; m.r[2] = phys_fixed_build_rhs(self); m.r[0] = self + 0x1a0; return m.ret(0);
}
// vf5: EAX = B's linear record + 0x40, EDX = lambda (this + 0xb90)
RX_PORT(0x0085c0d0, ConstraintFixedBody_vf5, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1]; phys_fixed_apply_impulse(self);
    m.r[0] = U32((self + 0x30), 0) + 0x40; m.r[2] = self + 0xb90; return m.ret(0);
}
// vf7: EAX = this, EDX = A's linear record + 0x10
RX_PORT(0x0085bc30, ConstraintFixedBody_vf7, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t self = m.r[1]; phys_fixed_project_velocity(self);
    m.r[0] = self; m.r[2] = U32((self + 0x28), 0) + 0x10; return m.ret(0);
}
