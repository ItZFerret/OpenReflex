#pragma once
#include "../src/rx_mem.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_buggy_position(rx_addr v,rx_addr out);
rx_addr rx_buggy_rotation(rx_addr v,rx_addr out);
rx_addr rx_buggy_velocity(rx_addr v,rx_addr out);
rx_addr rx_buggy_angular_velocity(rx_addr v,rx_addr out);
rx_addr rx_buggy_body_velocity(rx_addr v,rx_addr out);
rx_addr rx_buggy_body_angular_velocity(rx_addr v,rx_addr out);
rx_f80 rx_buggy_engine_rpm(rx_addr v);
rx_f80 rx_buggy_engine_limit(rx_addr v);
uint32_t rx_buggy_gear(rx_addr v);
uint32_t rx_buggy_contact_flag(rx_addr v);
uint32_t rx_buggy_contact_state(rx_addr v);
uint32_t rx_buggy_all_wheels_contact(rx_addr v);
uint32_t rx_buggy_mode_two(rx_addr v);
rx_addr rx_truck_position(rx_addr v,rx_addr out);
rx_addr rx_truck_rotation(rx_addr v,rx_addr out);
rx_addr rx_truck_velocity(rx_addr v,rx_addr out);
rx_addr rx_truck_angular_velocity(rx_addr v,rx_addr out);
rx_addr rx_truck_body_velocity(rx_addr v,rx_addr out);
rx_addr rx_truck_body_angular_velocity(rx_addr v,rx_addr out);
rx_f80 rx_truck_engine_rpm(rx_addr v);
rx_f80 rx_truck_engine_limit(rx_addr v);
uint32_t rx_truck_gear(rx_addr v);
uint32_t rx_truck_contact_flag(rx_addr v);
uint32_t rx_truck_contact_state(rx_addr v);
uint32_t rx_truck_all_wheels_contact(rx_addr v);
uint32_t rx_truck_mode_two(rx_addr v);
#ifdef __cplusplus
}
#endif
