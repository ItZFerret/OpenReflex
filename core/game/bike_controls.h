/* bike_controls.h -- the control update of slot 9 (bike_controls.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_controls_update(rx_addr S, float dt);           /* 0044c170 RavenSimulationMotocross::vf9 */
void bike_controls_rider_off(rx_addr S, float dt);        /* 0044e0d0 BikeControls_UpdateRiderOff */
void bike_smooth_ground_direction(rx_addr S, float dt);   /* 0043e9f0 BikeControls_SmoothGroundDirection */
#ifdef __cplusplus
}
#endif
