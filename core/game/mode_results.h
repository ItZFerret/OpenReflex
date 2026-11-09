/* mode_results.h -- the standings' values of the Waypoint / Freestyle / Section Race results (mode_results.c). */
#pragma once
#include "rx_game.h"
#include "rx_mode_results.h"
/* game = the session's game block, player = the local player. 0 = no activity (out untouched). */
int rx_mode_results_fill(rx_addr game, rx_addr player, rx_mode_results* out);
