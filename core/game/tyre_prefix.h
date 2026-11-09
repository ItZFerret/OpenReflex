#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Internal phases of004d50e0, not an implementation/registration of the entry.
 * The caller owns the real aligned frame B; these phases never manufacture a
 * replacement contact state or complete an unfinished force path. */
void rx_utv_tyre_airborne_phase(rx_addr tyre,float dt);
void rx_utv_tyre_friction_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_utv_tyre_root_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre,float dt);
/* Scalar SSE input/output, with genuine RA already present at callee-entry ESP.
 * Definition belongs to the eventual exact private dependency bridge. */
float rx_utv_tyre_dependency(rx_addr target,rx_addr expected_sp,float input,float second);
/* Final phase004d750d..759f; rolling/lateral are the branch-selected live
 * vectors at750d, not values reconstructed from an unrelated source. */
void rx_utv_tyre_publish_phase(rx_addr frame,rx_addr simulation,rx_addr tyre,float dt,rx_v4 rolling,rx_v4 lateral);
void rx_utv_tyre_wheel_point_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_utv_tyre_root_normal_phase(rx_addr frame,rx_addr tyre);
void rx_utv_tyre_wheel_normal_phase(rx_addr frame,rx_addr tyre);
void rx_utv_tyre_lean_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
/* Response arithmetic617e..6a8f is drafted, with final dead-frame audit pending.
 * It is not included in closedInternalPhases or any original acceptance. */
void rx_utv_tyre_response_draft(rx_addr frame,rx_addr tyre);
void rx_utv_tyre_negative_slip_phase(rx_addr frame,rx_addr simulation,rx_addr tyre);
void rx_utv_tyre_rear_slip_phase(rx_addr frame,rx_addr simulation);
typedef struct rx_utv_tyre_published_parts {rx_v4 rolling,lateral;} rx_utv_tyre_published_parts;
rx_utv_tyre_published_parts rx_utv_tyre_force_limits_draft(rx_addr frame,rx_addr simulation,rx_addr tyre);
/* Complete control flow/arithmetic draft. NOT oracle-ready until response
 * surviving-frame audit and exact dependency bridge are complete. */
void rx_utv_tyre_step_draft(rx_addr tyre,rx_addr simulation,float dt);
void* rx_utv_tyre_bind(void* machine);

#ifdef __cplusplus
}
#endif
