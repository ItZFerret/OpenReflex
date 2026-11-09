/* race_driver_reset.h -- RaceDriver::vf3 (race_driver_reset.c). */
#pragma once
#include "rx_game.h"
void rx_player_reset_best(rx_addr player);     /* 00508ab0 (EAX = the player) */
void rx_race_driver_vf3(rx_addr d);            /* RaceDriver::vf3 0050f090 (thiscall) */
void rx_rule_offtrack_penalty_vf6(rx_addr rule);   /* RuleOfftrackTimePenalty::vf6 0050ed00 (ECX = the rule) */
