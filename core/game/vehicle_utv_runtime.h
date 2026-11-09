#pragma once
#include "../src/rx_mem.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_utv_publish_rider(rx_addr vehicle,float dt); /*004c6780 ECX V, stack dt RET4*/
void rx_utv_position(rx_addr vehicle,rx_addr out); /*004c6ec0 ECX V, stack out RET4*/
void rx_utv_rotation(rx_addr vehicle,rx_addr out); /*004c6ee0*/
void rx_utv_velocity(rx_addr vehicle,rx_addr out); /*004c6f00*/
void rx_utv_angular_velocity(rx_addr vehicle,rx_addr out); /*004c6f20*/
void rx_utv_body_velocity(rx_addr vehicle,rx_addr out); /*004c6f40*/
void rx_utv_body_angular_velocity(rx_addr vehicle,rx_addr out); /*004c6f60*/
rx_f80 rx_utv_engine_rpm(rx_addr vehicle); /*004c6f80 ECX V RET0 ST0, main x87 PC53*/
rx_f80 rx_utv_max_rpm(rx_addr vehicle); /*004c6fa0 ECX V RET0 ST0*/
uint32_t rx_utv_gear(rx_addr vehicle); /*004c6fb0 ECX V RET0 EAX*/
uint32_t rx_utv_rider_trigger(rx_addr vehicle); /*004c6fc0*/
uint32_t rx_utv_airborne(rx_addr vehicle); /*004c6fd0*/
uint32_t rx_utv_all_wheels_contact(rx_addr vehicle); /*004c6fe0*/
uint32_t rx_utv_rider_flag(rx_addr vehicle); /*004c7040*/
void rx_utv_project_body_positions(rx_addr simulation); /*004cdb70 ECX S RET0*/
void rx_utv_clamp_angular_velocity(rx_addr simulation); /*004cdbe0 stack S RET4*/
#ifdef __cplusplus
}
#endif
