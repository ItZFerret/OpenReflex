/* loop_cam_tpc.h -- MXThirdPersonCamera's update (loop_cam_tpc.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_third_person_camera_vf19(rx_addr cam, float dt);     /* 0064c110 MXThirdPersonCamera::vf19 */
void rx_third_person_camera_vf30(rx_addr cam, uint32_t controller, float dt);   /* 00649ed0 MXThirdPersonCamera::vf30 */
void rx_cam_water_reset(rx_addr cam);                         /* 00650210 (EAX = camera) */
#ifdef __cplusplus
}
#endif
