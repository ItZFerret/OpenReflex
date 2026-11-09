#pragma once
#include "rx_game.h"
/* Native profile semantics of 006d7980 / 006d7d60, both stdcall three arguments.
 * Steam is absent under the product's existing offline platform policy. */
uint32_t rx_highlight_add_progress(rx_addr tracking, uint32_t controller, uint32_t index);
uint32_t rx_highlight_is_unlocked(rx_addr tracking, uint32_t controller, uint32_t index);
