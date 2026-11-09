/* bike_vehicle.h -- the vehicle's step listeners around the simulation (bike_vehicle.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_vehicle_pre_step(rx_addr L, float dt);    /* 0045d510 RavenVehicle::vf0_o90 (L = vehicle + 0x90) */
void bike_vehicle_post_step(rx_addr L, float dt);   /* 0045d820 RavenVehicle::vf1_o90 */
void bike_vehicle_debug_view(rx_addr V, float dt);  /* 0045d910 (EDI = vehicle) */
rx_addr bike_vehicle_position_filter(rx_addr filters, rx_addr target, rx_addr out, float dt);   /* 0042d7f0 (ESI = filters, EDX = target, EAX = out, xmm4 = dt) -> EAX = out */
#ifdef __cplusplus
}
#endif
