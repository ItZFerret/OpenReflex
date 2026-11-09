#pragma once
#include "career_properties.h"
#ifndef RX_ORACLE
uint32_t rx_career_property_int(rx_addr key,uint32_t initial);
void rx_career_property_string(rx_addr ui,rx_addr key,rx_addr output,uint32_t capacity);
/*Explicit offline presence boundary, NOT full0059cd40 implementation. Rejects
 * online modes or a real PCSession rather than silently dropping metadata.*/
void rx_career_offline_session_metadata(rx_addr ui);
#endif
