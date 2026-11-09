#pragma once
#include "rx_game.h"
rx_addr rx_career_player_table(rx_addr manager,uint32_t key);
void rx_career_request_player_states(rx_addr manager,uint32_t from,uint32_t to);
void rx_career_apply_player_states(rx_addr manager);
void rx_career_clear_requested_states(rx_addr manager);
/* The first series is the destination. Zero copies; otherwise destination += source * multiplier. */
void rx_career_accumulate_standings(rx_addr manager,int32_t destination,int32_t source,int32_t multiplier);
/* Declared for the Single owner; implementation/verification remains a separate closure. */
void rx_career_update_standings(rx_addr manager);
