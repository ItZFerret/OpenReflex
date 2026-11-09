// race_audio_cues.cpp -- oracle adapters for RaceActivityAudioLogic's cue handlers (core/game/race_activity_audio.c; group rx_sfx, w-sfx):
// the original subscribes them for the cues its level's cue map holds and the events call them through the delegates.
#include "oracle.h"
extern "C" {
#include "../game/race_activity.h"
}
using namespace reflex_port;
using rx_oracle::f32;

#define G "rx_sfx"
RX_PORT(0x00582da0, RaceAudio_EndOfRace, G, 0) { rx_race_audio_end_of_race(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582dc0, RaceAudio_TwoLapsLeft, G, 0) { rx_race_audio_two_laps(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582de0, RaceAudio_OneLapLeft, G, 0) { rx_race_audio_one_lap(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582e40, RaceAudio_Holeshot, G, 0) { rx_race_audio_holeshot(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00582e00, RaceAudio_BestLap, G, 0) { rx_race_audio_best_lap(m.r[1], m.arg(0), f32(m.arg(1))); return m.ret(8); }
RX_PORT(0x00582e20, RaceAudio_VehicleLapped, G, 0) { rx_race_audio_lapped(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }
RX_PORT(0x00583fb0, RaceAudio_PlayerWins, G, 0) { rx_race_audio_player_wins(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10); }
RX_PORT(0x00583a70, RaceAudio_Eliminated, G, 0) { rx_race_audio_eliminated(m.r[1], m.arg(0), m.arg(1), m.arg(2)); return m.ret(0xc); }
RX_PORT(0x00582e60, RaceAudio_TimedSection, G, 0) { rx_race_audio_timed_section(m.r[1], m.arg(0), m.arg(1), (int32_t)m.arg(2)); return m.ret(0xc); }
// 00581760 (ESI = logic): the end of race's crowd cheer (one draw of the game's LCG)
RX_PORT(0x00581760, RaceAudio_EndRaceCheer, G, 0) { rx_audio_end_race_cheer(m.r[6]); return m.ret(0); }
