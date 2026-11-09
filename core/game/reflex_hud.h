/* reflex_hud.h -- the product's copy of MXUIReflexComponent, the wreck-recovery arrows (reflex_hud.c; include/rx_reflex_hud.h). */
#pragma once
#include "rx_game.h"
#include "rx_reflex_hud.h"
void rx_reflex_hud_reset(void);
/* After every dispatched event of the bound activity's channel (rx_race_hud_observe; local = the activity's local human player or 0): the four wreck events. */
void rx_reflex_hud_event(rx_addr local, rx_addr name, rx_addr payload, uint32_t bytes);
/* Once per frame after the original's frame (MXUIReflexComponent::vf11). player = the local player or 0. */
void rx_reflex_hud_tick(rx_addr activity, rx_addr player);
/* The query. 0 = no activity bound (out untouched). */
int rx_reflex_hud_fill(rx_addr player, rx_reflex_hud_state* out);
