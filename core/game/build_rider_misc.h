/* build_rider_misc.h -- small rider-unit helpers ported while chaining the product (build_rider_misc.c). */
#pragma once
#include "rx_game.h"
void rx_rider_vec8_mark_all(rx_addr vec);   /* 0063b360 (stdcall vector; RET 4) */
void rx_rider_vec8_clear_all(rx_addr vec);  /* 0063b3f0 (stdcall vector; RET 4) */
