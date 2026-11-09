/* bike_post.h -- slots 16 / 17 / 32: after the pose integration (bike_post.c). */
#pragma once
#include "bike_math.h"
#ifdef __cplusplus
extern "C" {
#endif
void bike_pitch_spring(rx_addr S, float dt);       /* 0043ed40 (slot 17 helper) */
void bike_slot17(rx_addr S, float dt);             /* 0043c1a0 RavenSimulationMotocross::vf17 */
void bike_slot16(rx_addr S);                       /* 0043e200 RavenSimulationMotocross::vf16 */
void bike_slot32(rx_addr S);                       /* 0044ef70 RavenSimulationMotocross::vf32 */
#ifdef __cplusplus
}
#endif
