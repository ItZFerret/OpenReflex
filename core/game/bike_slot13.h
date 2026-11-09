/* bike_slot13.h -- slot 13: wheel rolling orientation, resting pin, body probes, triangle test, the slot itself
 * (bike_slot13.c; the corrections and the terrain query are in bike_contact.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_wheels_update_rolling_orientation(rx_addr S, float dt);   /* 00445800 BikeWheels_UpdateRollingOrientation */
void bike_rest_pin(rx_addr S);                                       /* 00446f80 (resting pin) */
void bike_body_probe_contacts(rx_addr S);                            /* 00435b00 Simulation_BodyProbeContacts */
void bike_slot13(rx_addr S, float dt);                               /* 00444b90 RavenSimulationMotocross::vf13 */
uint32_t bike_wheel_triangle_contact(rx_addr out_point, rx_addr out_normal, rx_addr q, rx_addr p, rx_addr e,
                                     rx_addr tris, int32_t count, int32_t* hits);   /* 00438260 Wheel_TriangleContactTest */
#ifdef __cplusplus
}
#endif
