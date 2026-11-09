#pragma once
#include "career_presentation.h"
#include "career_composition.h"
#ifndef RX_ORACLE
/* Bind/unbind on owning worker; caller retains adapter/queue outside guest reset. */
uint32_t rx_career_presentation_bind(rx_career_presentation*,rx_career_host*);
void rx_career_presentation_unbind(rx_career_presentation*);
void rx_career_presentation_observe_fade(uint32_t seconds_bits,uint32_t color);
void rx_career_presentation_dispatch_fade_complete(void*unused);
void rx_career_presentation_observe_page_wipe(uint32_t guest_name);
void rx_career_presentation_dispatch_page_wipe_transition(void*,const char*name);
void rx_career_presentation_dispatch_page_wipe_complete(void*,const char*name);
#endif
