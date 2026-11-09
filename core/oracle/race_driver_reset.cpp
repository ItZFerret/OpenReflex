// race_driver_reset.cpp -- oracle adapters for core/game/race_driver_reset.c (group rx_race).
#include "oracle.h"
extern "C" {
#include "../game/race_driver_reset.h"
}
using namespace reflex_port;

// 00508ab0 (EAX = the player)
RX_PORT(0x00508ab0, Player_ResetBest, "rx_race", 0) { rx_player_reset_best(m.r[0]); return m.ret(0); }
// RaceDriver::vf3 0050f090 (ECX = the driver)
RX_PORT(0x0050f090, RaceDriver_vf3, "rx_race", 0) { rx_race_driver_vf3(m.r[1]); return m.ret(0); }
// RuleOfftrackTimePenalty::vf6 0050ed00 (ECX = the rule)
RX_PORT(0x0050ed00, RuleOfftrackPenalty_vf6, "rx_race", 0) { rx_rule_offtrack_penalty_vf6(m.r[1]); return m.ret(0); }
