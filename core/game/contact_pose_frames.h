#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_contact_rotor_pose_exact(rx_addr self);
void* rx_contact_pose_bind(void* previous);
void* rx_contact_matrix_bind(void* previous);
int rx_contact_pose_active(void);
rx_addr rx_contact_pose_base(void);
void rx_contact_pose_matrix(unsigned role,rx_addr out,rx_addr input,rx_addr matrix);
void rx_contact_pose_relative(rx_v4 body_quaternion);
void rx_contact_pose_normalization(rx_v4 constrained,rx_v4 finite_mask);
/* Actual scalar intermediates of the existing canonical matrix owner. */
void rx_contact_matrix_q1_spills(float w,float z2,float xx,float wx,float y2,float wy,float wz);
void rx_contact_matrix_q2_spills(float wz,float xx,float wx,float zy);
#ifdef __cplusplus
}
#endif
