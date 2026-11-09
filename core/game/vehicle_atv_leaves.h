#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_vehicle_set_control_mode(rx_addr v,uint32_t mode);
void rx_atv_apply_pending_tuning(rx_addr s);
void rx_atv_clear_contact_history(rx_addr s);
rx_addr rx_atv_vehicle_body_angular_velocity(rx_addr v,rx_addr out);
rx_f80 rx_atv_vehicle_max_rpm(rx_addr v);
uint32_t rx_atv_vehicle_clutch_override(rx_addr v);
rx_f80 rx_atv_vehicle_wheel_load(rx_addr v,uint32_t axle);
uint32_t rx_atv_vehicle_has_wheel_tag(rx_addr v,uint32_t tag);
uint32_t rx_atv_vehicle_collision_type(rx_addr v,uint32_t tag);
uint32_t rx_atv_needs_contact_resolution(rx_addr s);
uint32_t rx_atv_body_contact_bits(rx_addr s);
uint32_t rx_atv_vehicle_outside_terrain(rx_addr v);
#ifdef __cplusplus
}
#endif
