/* loop_cam_misc.h -- the start gate / replay / replay tower cameras (loop_cam_misc.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
float   rx_player_analog7_active(uint32_t index);                   /* 00548620 (EAX) -> xmm0 */
float   rx_camera_hcontrol(uint32_t index);                         /* 005486c0 (ESI) -> xmm0 */
float   rx_camera_vcontrol(uint32_t index);                         /* 00548710 (ESI) -> xmm0 */
float   rx_camera_dolly(uint32_t index);                            /* 00548760 (ESI) -> xmm0 */
void    rx_start_gate_camera_vf19(rx_addr cam, float dt);           /* 006481a0 MXStartGateCamera::vf19 */
rx_addr rx_replay_camera_controller(void);                          /* 00648990 -> EAX */
rx_addr rx_replay_focus_point(void);                                /* 006489e0 -> EAX = 0xa95990 */
void    rx_replay_camera_vf19(rx_addr cam, float dt);               /* 00648aa0 MXReplayCamera::vf19 */
void    rx_replay_tower_camera_vf19(rx_addr cam, float dt);         /* 00649160 MXReplayTowerCamera::vf19 */
void    rx_cam_lift_above_water2(rx_addr cam, rx_addr pos, rx_addr out);   /* 00647950 (ECX, EAX; stack out) */
void    rx_quick_camera_vf19(rx_addr cam, float dt);                /* 00646ff0 MXQuickCamera::vf19 */
void    rx_player_stick7(uint32_t index, rx_addr out);               /* 005487d0 (ESI = index; cdecl out) */
#ifdef __cplusplus
}
#endif
