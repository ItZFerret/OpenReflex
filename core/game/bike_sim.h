/* bike_sim.h -- the simulation framework of the motocross vehicle (bike_sim.c): RavenSimulation / RavenSimulationMotocross
 * virtual slots, the fixed-step scheduler and step, input block, replay record, pose override and small helpers. */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- tiny slots and record setters */
uint32_t bike_vehicle_type_id(void);                                   /* 00430270 RavenVehicle::vf22 */
void     bike_clear_wheel_contact_words(rx_addr S);                    /* 004392d0 RavenSimulationMotocross::vf3 */
void     bike_rider_record_set_a0(rx_addr rec, float v);               /* 004304b0 RiderRecord_SetFloatA0 */
void     bike_rider_record_set_a4(rx_addr rec, float v);               /* 004304c0 RiderRecord_SetFloatA4 */
void     bike_rider_record_set_a8(rx_addr rec, float v);               /* 004304d0 RiderRecord_SetFloatA8 */
void     bike_rider_record_set_88(rx_addr rec, float v);               /* 004304e0 RiderRecord_SetFloat88 */
void     bike_rider_record_set_ac(rx_addr rec, float v);               /* 004304f0 RiderRecord_SetFloatAC */
void     bike_rider_record_set_vec90(rx_addr rec, rx_addr src);        /* 00430490 RiderRecord_SetVec90 */
void     bike_rider_record_set_vec60(rx_addr rec, rx_addr src);        /* 00430500 RiderRecord_SetVec60 */
void     bike_rider_record_set_vec70(rx_addr rec, rx_addr src);        /* 00430520 RiderRecord_SetVec70 */
void     bike_rider_record_set_vec80(rx_addr rec, rx_addr src);        /* 00430540 RiderRecord_SetVec80 */
uint32_t bike_both_wheels_in_contact(rx_addr S);                       /* 00447730 vf29 */
uint32_t bike_wheel_contact_scalars_set(rx_addr S);                    /* 00447750 vf30 */
uint32_t bike_wheel_contact_normal(rx_addr S, rx_addr out);            /* 00447790 vf31: normalized wheel contact normal sum */
uint32_t bike_body_probe_contact_any(rx_addr S);                       /* 0043b5e0 vf5 */
void     bike_get_vec_c0(rx_addr obj, rx_addr out);                    /* 004301f0 shared vf */
void     bike_model_node_set_matrix(rx_addr ref, rx_addr matrix);      /* 004300a0 ModelNodeRef_SetMatrix */
void     bike_model_node_set_matrix_regs(rx_addr ref, rx_addr matrix, uint32_t out[3]);   /* 004300a0 -> EAX / ECX / EDX */
void     bike_init_input_block(rx_addr block);                         /* 00430850 Simulation_InitInputBlock */
uint32_t bike_moto_contact_or_impulse(rx_addr S);                      /* 0043b4a0 RavenSimulationMotocross::vf4 */
void     bike_moto_save_or_restore_pose(rx_addr S, float dt);          /* 0043c440 RavenSimulationMotocross::vf7 */
uint32_t bike_moto_resting_axis_flat(rx_addr S);                       /* 0044c010 RavenSimulationMotocross::vf27 */
void     bike_moto_set_pose_override_targets(rx_addr S, rx_addr targets);   /* 0044e570 RavenSimulationMotocross::vf10 */
void     bike_moto_begin_pose_override(rx_addr S);                          /* 00446cd0 RavenSimulationMotocross::vf33 */
void     bike_end_pose_override(rx_addr S);                                 /* 00434ef0 Simulation_EndPoseOverride */
void     bike_moto_apply_pose_override(rx_addr S);                          /* 0044e830 RavenSimulationMotocross::vf34 */
void     bike_get_replay_state(rx_addr S, rx_addr out);                     /* 00433120 Simulation_GetReplayState */
void     bike_replay_record_encode(rx_addr rec, rx_addr S);                 /* 00433550 ReplayRecord_Encode */
void     bike_read_controller_input(uint32_t controller, rx_addr block);    /* 004308a0 Simulation_ReadControllerInput */
void     bike_set_input_block(rx_addr S, rx_addr block);                    /* 00433380 RavenSimulation::vf24 */
void     bike_moto_set_input_block(rx_addr S, rx_addr block);               /* 0044bff0 RavenSimulationMotocross::vf24 */
void     bike_moto_read_controller(rx_addr S, uint32_t controller);         /* 0044bfb0 RavenSimulationMotocross::vf23 */
uint32_t bike_sim_activity(rx_addr S);                                 /* 004321a0 RavenSimulation::vf28 */
uint32_t bike_sim_leave_rest_test(rx_addr S);                          /* 00432360 RavenSimulation::vf26 */
uint32_t bike_sim_enter_rest_test(rx_addr S);                          /* 004324d0 RavenSimulation::vf25 */
void     bike_moto_apply_pending_tuning(rx_addr S);                    /* 0043b640 RavenSimulationMotocross::vf8 */
uint32_t bike_state_divergence_check(rx_addr S);                       /* 00434d00 FUN_00434d00 */
void     bike_apply_debug_key_forces(rx_addr S);                       /* 00435320 Simulation_ApplyDebugKeyForces */
void     bike_handle_debug_keys(rx_addr S);                            /* 00435030 Simulation_HandleDebugKeys */
int      bike_apply_external_impulse(rx_addr S);                       /* 004355d0 Simulation_ApplyExternalImpulse */
void     bike_integrate_step(rx_addr S, float dt);                     /* 00431d60 RavenSimulation_IntegrateStep */
void     bike_update_fixed_steps(rx_addr S, float frame_dt);           /* 00430ef0 RavenSimulation::UpdateFixedSteps */
void     bike_remote_smooth(rx_addr S, float dt);                      /* 00432680: S=EDI dt=s0; RET 4 */
void     bike_sim_update(rx_addr S, float frame_dt);                   /* 00430eb0 RavenSimulation::vf1 */

void rx_sim_set_position(rx_addr sim, rx_addr v);                                    /* 0048d2f0 (ECX = the sim, stack a vec4; RET 4) */

#ifdef __cplusplus
}
#endif
