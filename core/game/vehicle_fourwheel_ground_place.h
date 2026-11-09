#pragma once
#include "../src/rx_mem.h"
/* thiscall sim; five pointer arguments, RET 0x14. Outputs are vec4 buffers. */
void rx_fourwheel_ground_place_truck(rx_addr sim,rx_addr pos,rx_addr dir,rx_addr out_pos,rx_addr out_fwd,rx_addr out_up); /*004cc980 UTV/Truck*/
void rx_fourwheel_ground_place_buggy(rx_addr sim,rx_addr pos,rx_addr dir,rx_addr out_pos,rx_addr out_fwd,rx_addr out_up); /*004b2180 Buggy*/
