#pragma once
#include "rx_game.h"
#ifndef RX_ORACLE
/* One session/worker owner. Begin explicitly clears and enables the career store.
 * Missing active keys stay missing, never leak from an exhibition spec. */
void rx_owned_properties_begin(void);
void rx_owned_properties_reset(void); /*free entries, retain active state*/
void rx_owned_properties_dispose(void); /*free entries and disable*/
uint32_t rx_owned_properties_active(void);
void rx_owned_property_set_int(rx_addr key,uint32_t value);
/*Matches actual005dbf50: at most31 value bytes + NUL. Keys dynamically owned,
 * no arbitrary key/count cap. This is the original UI value representation.*/
void rx_owned_property_set_string(rx_addr key,rx_addr value);
void rx_owned_property_erase(rx_addr key); /*005dc230 product semantic boundary*/
uint32_t rx_owned_property_get_int(rx_addr key,rx_addr out,uint32_t include_wait_fallback);
uint32_t rx_owned_property_get_string(rx_addr key,rx_addr output,uint32_t capacity);
/* Exhibition sessions only: the UI property InCareerMode (the Race & Ride challenges Free Ride), 0 = absent. Before the event load. */
void rx_owned_property_set_in_career_mode(uint32_t value);
/* Exhibition sessions only: hold (1) / release (0) the UI property WaitForUI (PreChallengePopup). Cleared by rx_owned_properties_dispose. */
void rx_owned_property_set_wait_for_ui(uint32_t value);
#endif
