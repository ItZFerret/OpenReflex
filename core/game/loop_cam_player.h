/* loop_cam_player.h -- MXPlayerCamera, MXAnimatedCamera and the camera path (loop_cam_player.c). Register arguments first (named in
 * each comment), then the stack arguments in order. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_cam_vehicle_class(rx_addr cam);                                   /* 006467c0 (EAX = camera) -> EAX; ECX = the vehicle */
void     rx_cam_reload_tuning(rx_addr cam);                                     /* 00646a50 (EDI) */
rx_addr  rx_cam_follow_point(rx_addr cam, rx_addr out);                         /* 006468a0 (EAX, ESI) -> EAX = out */
void     rx_player_camera_vf19(rx_addr cam, float dt);                          /* 00646940 MXPlayerCamera::vf19 */
void     rx_player_camera_vf31(rx_addr cam);                                    /* 00646bf0 MXPlayerCamera::vf31 */
void     rx_animated_camera_vf19(rx_addr cam, float dt);                        /* 0063d7e0 MXAnimatedCamera::vf19 */
void     rx_animated_camera_at(rx_addr cam, float t);                           /* 0063d600 (ESI; stack t) */
uint32_t rx_keytrack_count_before(uint32_t n, rx_addr keys, float t);          /* 004dbe00 (EAX, EDI, xmm0) */
void     rx_cam_path_sample(rx_addr path, float t, rx_addr m, rx_addr fov, rx_addr a, rx_addr b);   /* 0063e850 (stdcall) */
#ifdef __cplusplus
}
#endif
