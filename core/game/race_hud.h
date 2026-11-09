/* race_hud.h -- the product's copies of the race HUD components that the original drives from typed events (race_hud.c). */
#pragma once
#include "rx_game.h"
#include "rx_race_hud.h"
#include "rx_stunt_hud.h"
#include "reflex_hud.h"
void rx_race_hud_reset(void);
/* After every dispatched event (rx_world_event_dispatch, product only): the HUD components' event handlers. */
void rx_race_hud_observe(rx_addr channel, rx_addr name, rx_addr payload, uint32_t bytes);
/* Activity_SetState (005586c0) reached `state`: where the "<state> Entered" handlers of the dashboard components run (the original raises
 * that event only when a handler is registered, and the product registers none). */
void rx_race_hud_state_entered(rx_addr activity, uint32_t state);
/* Once per frame after the original's frame (the components' per-frame update). player = the local player or 0. */
void rx_race_hud_tick(rx_addr player);
/* The query: game = the session's game block, player = the local player. 0 = no activity (out untouched). */
int rx_race_hud_fill(rx_addr game, rx_addr player, rx_race_hud_state* out);
/* The stunt feedback overlay's ring (include/rx_stunt_hud.h). 0 = no activity bound (out untouched). */
int rx_stunt_hud_fill(rx_addr player, rx_stunt_hud_state* out);
