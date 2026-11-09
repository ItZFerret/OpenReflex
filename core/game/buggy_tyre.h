#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Typed private phases; entered aligned frame B belongs to eventual owner.
 * Original comparison and detailed surviving-frame audit remain pending. */
void rx_buggy_tyre_airborne_phase(rx_addr tyre,float dt);
void rx_buggy_tyre_lean_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_buggy_tyre_friction_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_buggy_tyre_root_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre,float dt);
void rx_buggy_tyre_wheel_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_buggy_tyre_root_normal_phase(rx_addr frame,rx_addr tyre);
void rx_buggy_tyre_wheel_normal_phase(rx_addr frame,rx_addr tyre);
typedef struct rx_buggy_tyre_parts {rx_v4 rolling,lateral;} rx_buggy_tyre_parts;
void rx_buggy_tyre_response_phase(rx_addr frame);
void rx_buggy_tyre_rear_slip_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
rx_buggy_tyre_parts rx_buggy_tyre_limits_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_buggy_tyre_publish_phase(rx_addr frame,rx_addr simulation,rx_addr tyre,float dt,rx_buggy_tyre_parts parts);
/* Full source draft; not registered and not accepted. */
void rx_buggy_tyre_step_exact(rx_addr tyre,rx_addr simulation,float dt);
/* Genuine RA/arguments already written at entered rx_sp; binding must validate
 * actual returned PC/ESP before cleanup. No native fallback is supplied here. */
float rx_buggy_tyre_dependency(rx_addr target,rx_addr expected,float input,float second);
void* rx_buggy_tyre_bind(void* machine);
#ifdef __cplusplus
}
#endif
