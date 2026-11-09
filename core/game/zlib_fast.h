#pragma once
#include "rx_game.h"
/*008ef330 cdecl(stream*,initial_avail_out),RET0,recordedmask10004.
 * Caller establishes actual inflate fast-path preconditions (>=6input,>=258out).
 * Never independently register/activate without genuine reference fixtures. */
void rx_guest_inflate_fast(rx_addr stream,uint32_t out_start);
