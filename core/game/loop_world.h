/* loop_world.h -- the world's frame (loop_world.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_world_debug_toggle(rx_addr world);                  /* 00767690 (ESI = world) */
void     rx_world_update_players(rx_addr world);                /* 00766ec0 */
uint32_t rx_world_update(rx_addr world, float dt);              /* 007674b0 MXWorld::vf5 */
void     rx_replay_cameras_update(rx_addr world, float dt);     /* 007678b0 ReplayCameras_Update */
void     rx_pending_handlers_run(rx_addr ch);                   /* 005933a0 (stdcall channel) */
rx_addr  rx_pending_list_erase(rx_addr list, rx_addr out, rx_addr cont, rx_addr node);   /* 00705640 (EDI = list) */
#ifdef __cplusplus
}
#endif
