#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_pin_and_water(rx_addr s,float dt); /*00493630 ECX=s RET4*/
void rx_atv_begin_relative_pose(rx_addr s); /*0048e8a0 ECX=s RET0*/
void rx_atv_apply_relative_pose(rx_addr s); /*004a1250 ECX=s RET0*/
void rx_atv_control_step(rx_addr s,float dt); /*0049ee30 ECX=s RET4*/
void rx_atv_rider_off_control(rx_addr s,float dt); /*004a0ab0 EDI=s RET4*/
void rx_atv_tire_contact_force(rx_addr s,float dt,rx_addr w); /*0049a340 EDI=s RET8*/
void rx_atv_step_forces(rx_addr s,float dt); /*00490b80 ECX=s RET4*/
void rx_atv_steering_quaternions(rx_addr s); /*00499220 ESI=s RET0*/
void rx_atv_update_engine(rx_addr s,float dt); /*004985e0 EDI=s stack dt RET4*/
void rx_atv_apply_suspension(rx_addr s); /*00493c70 EDI=s RET0*/
void rx_atv_choose_power_gear(rx_addr s); /*00498110 EDI=s RET0*/
void rx_atv_choose_coast_gear(rx_addr s); /*00498380 EDI=s RET0*/
#ifdef __cplusplus
}
#endif
