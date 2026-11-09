/* world_board_handlers.h -- ThirtySecondBoard's activity handlers (world_board_handlers.c) and the track / terrain-list helpers they use. */
#pragma once
#include "rx_game.h"
void rx_thirty_second_board_first_human(rx_addr self, rx_addr activity);                       /* 006b7b90 (thiscall; RET 4; tail jump to 006b8ab0) */
void rx_track_walk(rx_addr out, rx_addr track, float distance);                                 /* 00538310 (ESI = out {section, node, 0.0}; stack track block, distance; RET 8) */
rx_addr rx_terrain_for_scene(rx_addr list, rx_addr scene);                                      /* 0042d960 (EDX = the terrain list 0x009e30d4, EDI = scene; EAX = terrain or 0) */
