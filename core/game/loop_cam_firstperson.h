/* loop_cam_firstperson.h -- MXFirstPersonCamera::vf19 00642680 (loop_cam_firstperson.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 00642680 MXFirstPersonCamera::vf19 (ECX = camera, stack dt; ret 4): the first-person pose. The camera must have its
 * "pose valid" +0x2f4 set (the ctor / vf28 do that); otherwise it returns at once. Runs the player camera update
 * (00646940), then the head-anchored pose of the camera's controlled player: the rider's head transform (00623bd0) when
 * the player has a rider animation (P+0x1b4), the vehicle basis and its position (vtable slot 0x44), the aim / lean
 * springs and the pose (0063dee0). */
void rx_first_person_camera_vf32(rx_addr cam);       /* 00641f20 (MXPlayerCamera::vf32 + the first-person tuning) */
void rx_first_person_camera_vf28(rx_addr cam);       /* 00641460 (activation: vf19 at 1/30, the sound state, the head basis) */
void rx_first_person_camera_vf29(rx_addr cam);       /* 00641d90 (deactivation) */
rx_addr rx_fp_sound_w870(rx_addr player);            /* 0054d870 (the player's view sound object +8) */
rx_addr rx_fp_sound_w820(rx_addr player);            /* 0054d820 (the same, +0xc) */
void    rx_fp_sound_enable(rx_addr player);          /* 0054d8c0 */
void    rx_fp_sound_disable(rx_addr player);         /* 0054d980 */
void rx_first_person_camera_vf19(rx_addr cam, float dt);

#ifdef __cplusplus
}
#endif
