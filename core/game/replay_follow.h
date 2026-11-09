#pragma once
#include "rx_game.h"
void rx_replay_follow_begin(rx_addr simulation, int32_t end, int32_t start); /*EDI,ECX,stack*/
void rx_replay_follow_step(rx_addr simulation, float dt); /*EDI,stack*/
