/* freeride_any_level.h -- RESTORATION (not original game code): Free Ride on every level (freeride_any_level.c). */
#pragma once
#include "rx_game.h"

/* After MXActivityFreeRide's start slots (00560cd0) fell back to the original's all-default fill (no free ride start in the activity's
 * group: slot k at (2 k, 0, 0)), the twelve slots (A + 0x340 + 0x30 k: position, direction +0x10) get a spawn from the level's own
 * data. Product only; nothing calls it on a level with free ride starts. */
void rx_fr_any_level_spawn(rx_addr activity);

/* What the last call used: 0 nothing (the original's default stays), 1 the scene's free ride starts (0x1014) outside the group,
 * 2 the arena's Freestyle starts (0x1011), 3 the race track's start record, 4 the start gate model (w-fresnob: a level whose track definition is lost). */
extern uint32_t rx_fr_any_level_source;
