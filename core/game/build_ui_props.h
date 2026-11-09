#pragma once
#include "rx_game.h"
#ifndef RX_ORACLE
uint32_t rx_ui_prop_get_int(rx_addr key, rx_addr out);   /* UIProperty_GetInt's product replacement: the host's event spec (build_ui_props.c) */
uint32_t rx_ui_prop_get_string(rx_addr key,rx_addr output,uint32_t capacity);
#endif
