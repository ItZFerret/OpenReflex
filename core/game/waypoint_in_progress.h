/* waypoint_in_progress.h -- the waypoint track's debug traversal (waypoint_in_progress.c). */
#pragma once
#include "rx_game.h"

/* 005399e0 (thiscall, ECX = the waypoint track; no stack arguments): by the track's type (+8: 6, 7, 8, else the default) the finite traversal of its sections
 * (00539a20 / 00539c70 / 00539c00 / 00539d20), whose output sink 00539f90 is an empty function. */
void rx_waypoint_track_debug(rx_addr track);
