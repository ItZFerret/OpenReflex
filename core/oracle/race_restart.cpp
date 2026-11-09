// race_restart.cpp -- oracle adapters for core/game/race_restart.c (group rx_race): App.RestartActivity.
#include "race_oracle.h"
extern "C" {
#include "../game/race_restart.h"
}
using namespace reflex_port;

// 0055f670 (EDI = the world's activity manager; plain ret): the restart virtual of every activity with drivers
RX_PORT(0x0055f670, Activities_Restart, "rx_race", 0) { rx_activities_restart(m.r[7]); return m.ret(0); }
// 00759d00 MXGame_RestartActivity (plain ret): the Lua binding App_RestartActivity 0072e570 -> here
RX_PORT(0x00759d00, Game_RestartActivity, "rx_race", 0) { rx_game_restart_activity(); return m.ret(0); }
// 005810e0 ActivityAudioLogic::vf33 (the logic's sounds stopped, the audio thread flushed) has no adapter on purpose: its oracle body IS a call of the
// original (an audio-thread round trip a port cannot make: race_restart.c), as rx_mxworld_load_base's; core callers reach it through RX_FUNCTION.
