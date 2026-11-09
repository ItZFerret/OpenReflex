/* phys_constraint.h -- RavenPhysics constraint virtual methods (phys_constraint.c). All thiscall (ECX = this), no stack
 * arguments, plain ret. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_rotor_build_jacobian(rx_addr self);         /* 00859f90 ConstraintSlidingRotor::vf2 */
void phys_rotor_build_rhs(rx_addr self);              /* 0085a7e0 vf3 */
void phys_rotor_apply_impulse(rx_addr self);          /* 0085b5a0 vf5 */
rx_addr phys_rotor_project_pose(rx_addr self);        /* 0085a8f0 vf6 (EAX = B's angular record) */
rx_addr phys_rotor_project_velocity(rx_addr self);    /* 0085ac20 vf7 (EAX = this) */
rx_addr phys_rotor_parent_coupling(rx_addr self);     /* 0042f970 vf8 */
rx_addr phys_rotor_child_coupling(rx_addr self);      /* 0042f9a0 vf9 */
rx_addr phys_fixed_parent_coupling(rx_addr self);     /* 0042fcd0 ConstraintFixedBody::vf8 */
rx_addr phys_fixed_child_coupling(rx_addr self);      /* 0042fd00 vf9 */
void phys_fixed_project_pose(rx_addr self);           /* 0085b750 ConstraintFixedBody::vf1 */
rx_addr phys_fixed_build_jacobian(rx_addr self);      /* 0085b8b0 vf2 (returns the stack RA matrix: the last kernel's EAX) */
rx_addr phys_fixed_build_rhs(rx_addr self);           /* 0085bb00 vf3 (returns the stack fB vector: the last kernel's EDX) */
void phys_fixed_apply_impulse(rx_addr self);          /* 0085c0d0 vf5 */
void phys_fixed_project_velocity(rx_addr self);       /* 0085bc30 vf7 */
#ifdef __cplusplus
}
#endif
