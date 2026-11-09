#pragma once
#include "rx_game.h"
/* Narrow wrappers around existing canonical presentation-only lap helpers. */
rx_addr rx_waypoint_lap_message(rx_addr original_entry, rx_addr activity, rx_addr driver, int winner, int online);
void rx_waypoint_lap_presence(uint32_t controller, uint32_t key, uint32_t value);
void rx_waypoint_finish_highlights(rx_addr activity);
