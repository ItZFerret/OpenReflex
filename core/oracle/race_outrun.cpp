// race_outrun.cpp -- oracle adapters for core/game/race_outrun.c (group rx_race): the Outrun format's timed sections
// (TimedRaceSection, scene entity 0x107b) and MXActivityRace's "EnteredTimedSection" handler chain.
#include "race_oracle.h"
extern "C" {
#include "../game/race_outrun.h"
}
using namespace reflex_port;

// TimedRaceSection slot 0 (thiscall; flags; RET 4) -> EAX = this
RX_PORT(0x00413960, TimedSection_Sdtor, "rx_race", OUT_EAX) { m.r[0] = rx_timed_section_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// TimedRaceSection::vf5 (thiscall; the RSData reader; RET 4)
RX_PORT(0x00413990, TimedSection_Load, "rx_race", 0) { rx_timed_section_load(m.r[1], m.arg(0)); return m.ret(4); }
// 00554df0 (ESI = channel, ECX = driver, EDX = number; stdcall activity; RET 4): "TimedSectionEntered"
RX_PORT(0x00554df0, TimedSection_EnteredEvent, "rx_race", 0) { rx_event_timed_section_entered(m.r[6], m.r[1], m.r[2], m.arg(0)); return m.ret(4); }
// 0054f4b0 (ECX = player, EAX = section): the player's section clock
RX_PORT(0x0054f4b0, TimedSection_PlayerEntered, "rx_race", 0) { rx_player_timed_section_entered(m.r[1], m.r[0]); return m.ret(0); }
// 00569550 (thiscall activity; vehicle, section; RET 8): the Outrun race's handler
RX_PORT(0x00569550, Race_EnteredTimedSection, "rx_race", 0) { rx_race_entered_timed_section(m.r[1], m.arg(0), m.arg(1)); return m.ret(8); }

// ---- the challenge page's prize (core/game/race_challenge_payout.c) ----
extern "C" {
#include "../game/race_challenge_payout.h"
}
// 0073ec10 App.GetChallengeAwardLevel's native (EAX = the activity type) -> EAX
RX_PORT(0x0073ec10, ChallengeAwardLevel, "rx_race", OUT_EAX) { m.r[0] = uint32_t(rx_challenge_award_level(m.r[0])); return m.ret(0); }
