/* bike_response.h -- contact response and the rest of slot 15 (bike_response.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_contact_response(rx_addr S);   /* 004360b0 BikeSim_ContactResponse */
#ifdef __cplusplus
}
#endif
#ifdef __cplusplus
extern "C" {
#endif
void bike_clamp_angular_velocities(rx_addr S);   /* 00443af0 Bike_ClampAngularVelocities */
int  bike_wheels_spin_relative(rx_addr S);       /* 00447810 BikeWheels_SpinRelativeCorrection */
void bike_slot15(rx_addr S, float dt);           /* 00443c30 RavenSimulationMotocross::vf15 */
#ifdef __cplusplus
}
#endif
