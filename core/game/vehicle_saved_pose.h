/* vehicle_saved_pose.h -- 00460070: a vehicle placed back at the pose its simulation saved. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00460070 (EAX = vehicle; no stack arguments; plain RET). Called when bike_state_divergence_check (00434d00) reports a
 * non-finite simulation state: places the vehicle at the position/orientation stored in its saved-pose block (0045f600). */
void rx_vehicle_restore_saved_pose(rx_addr vehicle);
#ifdef __cplusplus
}
#endif
