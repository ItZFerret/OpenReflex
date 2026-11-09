#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_vehicle_position(rx_addr v,rx_addr out); /*00486560 V30 RET4*/
void rx_atv_vehicle_rotation(rx_addr v,rx_addr out); /*00486580 V36 RET4*/
void rx_atv_vehicle_velocity(rx_addr v,rx_addr out); /*004865a0 V37 RET4*/
void rx_atv_vehicle_angular_velocity(rx_addr v,rx_addr out); /*004865c0 V38 RET4*/
void rx_atv_vehicle_body_velocity(rx_addr v,rx_addr out); /*004865e0 V40 RET4*/
uint32_t rx_atv_vehicle_all_wheels_contact(rx_addr v);
uint32_t rx_atv_vehicle_water_contact(rx_addr v);
uint32_t rx_atv_vehicle_rider_trigger(rx_addr v);
uint32_t rx_atv_vehicle_airborne(rx_addr v);
uint32_t rx_atv_vehicle_rider_flag(rx_addr v);
uint32_t rx_atv_vehicle_rider_action(rx_addr v);
void rx_atv_vehicle_update(rx_addr v,float dt); /*0047d870 V29 RET4*/
void rx_atv_vehicle_contact_filters(rx_addr v,float dt); /*00489c80 EAX=V RET4*/
void rx_atv_vehicle_world_move(rx_addr v,rx_addr world); /*004847e0 V31 RET4*/
void rx_atv_vehicle_publish_rider(rx_addr v,float dt); /*00484c50 V35 RET4*/
void rx_atv_vehicle_post_step(rx_addr v); /*00486520 V61 RET0*/
uint32_t rx_atv_vehicle_safe_pose(rx_addr v,rx_addr position,rx_addr rotation,uint32_t thresholdBits,uint32_t unused); /*00486100 V34 RET16*/
#ifdef __cplusplus
}
#endif
