/* Original ATV simulation control/contact methods, vtable0093d054. */
#pragma once
#include "../src/rx_mem.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_f80 rx_atv_steering_angle(rx_addr s); /*0047c460 ECX=s -> ST0*/
void rx_atv_read_controller(rx_addr s,uint32_t controller); /*0049ee00 ECX=s,RET4*/
void rx_atv_set_relative_pose(rx_addr s,rx_addr pose); /*004a0e60 ECX=s,RET4*/
void rx_atv_update_contact_filters(rx_addr s,float dt); /*00496310 EDI=s,RET4*/
void rx_atv_post_step(rx_addr s); /*0049d7c0 EDX=s,RET0*/
void rx_atv_ground_place(rx_addr s,rx_addr pos,rx_addr dir,rx_addr out_pos,rx_addr out_fwd,rx_addr out_up); /*0048f9e0 RET20*/
void rx_atv_roll_spring(rx_addr s,float dt); /*004a17b0 stackself,dt RET8*/
void rx_atv_publish_wheel_spin(rx_addr s,float dt); /*004931a0 ECX=s RET4*/
void rx_atv_apply_axle_torques(rx_addr s); /*00498db0 stackself RET4*/
void rx_atv_clamp_body_spin(rx_addr s); /*0048d430 stackself RET4*/
void rx_atv_apply_resistance(rx_addr s); /*00493770 EDI=s RET0*/
#ifdef __cplusplus
}
#endif
