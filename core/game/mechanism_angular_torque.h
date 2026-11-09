#pragma once
#include "rx_game.h"
/*0085c9a0: ECX mechanism, stack body index and aligned torque pointer; RET8.
 * Returns the destination accumulator; preserves EDX and x87. */
rx_addr rx_mechanism_add_body_torque(rx_addr mechanism, uint32_t index, rx_addr torque);
