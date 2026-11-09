/* loop_cam_shake.h -- the chase camera's shake (loop_cam_shake.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_cam_shake(rx_addr cam, float dt);         /* 006519b0 (thiscall; ret 4) */
void rx_cam_noise_shake(rx_addr cam, float dt);   /* 00652800 (ESI = camera; stack dt; ret 4) */
#ifdef __cplusplus
}
#endif
