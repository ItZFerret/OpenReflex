// race_player.cpp -- oracle adapters for core/game/race_player.c.
#include "oracle.h"
extern "C" {
#include "../game/race.h"
}
using namespace reflex_port;

RX_PORT(0x00550280, Player_ResetFade, "rx_race", 0) { rx_player_reset_fade(m.r[6]); return m.ret(0); }
// 005501f0 (EDX = position, ECX = orientation, arg 0 = player; RET 4).
RX_PORT(0x005501f0, Player_ResetTo, "rx_race", 0) { rx_player_reset_to(m.arg(0), m.r[2], m.r[1]); return m.ret(4); }
RX_PORT(0x00550800, Player_ResetToTrack, "rx_race", 0) { rx_player_reset_to_track(m.r[6]); return m.ret(0); }
RX_PORT(0x005507a0, Player_ResetOfftrack, "rx_race", 0) { rx_player_reset_offtrack(m.r[6]); return m.ret(0); }
// 00550920 (ESI = player).
RX_PORT(0x00550920, Player_ResetInPlace, "rx_race", 0) { rx_player_reset_in_place(m.r[6]); return m.ret(0); }
// 00550b70 (ESI = player).
RX_PORT(0x00550b70, Player_ResetTerrain, "rx_race", 0) { rx_player_reset_terrain(m.r[6]); return m.ret(0); }
