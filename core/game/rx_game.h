/* rx_game.h -- base include of the core's game code (core/game/ *.c): the memory model, x86-exact float helpers and the function
 * registry. Each subsystem declares its functions in its own header (game/<subsystem>.h), one rx_<name> per original function
 * with its address in the comment. Shared by the product library and the oracle build. */
#pragma once
#include "../src/rx_mem.h"
#include "../src/rx_x86.h"
#include "../src/rx_fn.h"
