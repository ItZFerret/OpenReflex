/* race_restart.h -- App.RestartActivity (race_restart.c): the game's restart of the local event's activities. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_audio_logic_vf33(rx_addr logic);      /* ActivityAudioLogic::vf33 005810e0 (ECX = logic; plain ret) */
void rx_activities_restart(rx_addr manager);   /* 0055f670 (EDI = the world's activity manager, world + 0x21da0; plain ret) */
void rx_game_restart_activity(void);           /* MXGame_RestartActivity 00759d00 (plain ret; the Lua binding App_RestartActivity 0072e570 calls it) */
#ifdef __cplusplus
}
#endif
