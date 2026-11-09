/* bike_slot12.h -- slot 12: grounded / airborne corrections after velocity integration (bike_slot12.c, bike_air.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void  bike_slot12(rx_addr S, float dt);                                          /* 00443610 RavenSimulationMotocross::vf12 */
void  bike_clamp_root_spin(rx_addr S, uint32_t* lane1, uint32_t* sum);          /* 0043f270 Simulation_ClampRootAngularVelocity */
float bike_stick_circle_to_square(float x, float y, rx_addr out_a, rx_addr out_b);   /* 00440c40 Stick_CircleToSquare */
uint32_t bike_stunt_latches(rx_addr S, float dt);                               /* 00440da0 (EDI = S) */
uint32_t bike_air_control(rx_addr S, float dt);                                 /* 0043f2e0 (stdcall S, dt) */
void  bike_ground_correction(rx_addr S, float dt);                              /* 00441300 (ESI = S, stack dt) */
#ifdef __cplusplus
}
#endif
