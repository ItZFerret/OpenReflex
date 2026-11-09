/* bike_tire.h -- the tire / contact force of one wheel (bike_tire.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_tire_contact_force(rx_addr S, float dt, rx_addr W);   /* 00447f10 BikeTire_ContactForce */
#ifdef __cplusplus
}
#endif
