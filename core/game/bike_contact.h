/* bike_contact.h -- slot 13 of the motocross simulation (wheel / body contacts against the terrain and objects, position
 * corrections, wheel rolling orientation, resting pin), bike_contact.c. */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_v4 bike_wheel_position_correction(rx_addr W, rx_addr S);          /* 00431ee0 BikeWheel_ContactPositionCorrection */
rx_v4 bike_wheel_position_correction_rider_off(rx_addr W, rx_addr S);/* 0043cfe0 BikeWheel_ContactPositionCorrectionRiderOff */
void  bike_body_probe_apply_average_correction(rx_addr S);           /* 00438180 BodyProbe_ApplyAverageCorrection */
void  bike_wheel_apply_contact_correction(rx_addr S);                /* 004456d0 BikeWheel_ApplyContactCorrection */
void  bike_wheel_query_terrain_contact(rx_addr W, rx_addr S);        /* 00444a40 BikeWheel_QueryTerrainContact */
#ifdef __cplusplus
}
#endif
