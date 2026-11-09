#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Exact private phases of00457070; source draft, no product registration. */
void rx_truck_tyre_airborne_phase(rx_addr tyre,float dt);
void rx_truck_tyre_lean_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_truck_tyre_friction_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_truck_tyre_root_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre,float dt);
void rx_truck_tyre_wheel_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_truck_tyre_root_normal_phase(rx_addr frame,rx_addr tyre);
void rx_truck_tyre_wheel_normal_phase(rx_addr frame,rx_addr tyre);
typedef struct rx_truck_tyre_parts {rx_v4 rolling,lateral;} rx_truck_tyre_parts;
void rx_truck_tyre_response_phase(rx_addr frame);
void rx_truck_tyre_rear_slip_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
rx_truck_tyre_parts rx_truck_tyre_limits_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
/* Complete source draft, not registered or runtime accepted. */
void rx_truck_tyre_step_exact(rx_addr tyre,rx_addr simulation,float dt);
void* rx_truck_tyre_bind(void* machine);
float rx_truck_tyre_dependency(rx_addr target,rx_addr expected,float input,float second);
#ifdef __cplusplus
}
#endif
