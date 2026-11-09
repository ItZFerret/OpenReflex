/* build_crt_atexit.c -- the CRT's _atexit (0x008f9fbf) in the product: the game's function-local statics register their destructors with it, and the core never exits (the host
 * unloads the library), so nothing is registered. The oracle build runs the original through rx_call_original (its exit table is part of the emulated memory), so this
 * is a product function only. */
#include "rx_game.h"

#ifndef RX_ORACLE
uint32_t rx_crt_atexit(uint32_t handler) { (void)handler; return 0; }
RX_FUNCTION(0x008f9fbf, rx_crt_atexit)
#endif
