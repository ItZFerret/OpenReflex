#pragma once
#include "../src/rx_mem.h"
/* Simulation vf23: thiscall sim, stack controller, RET4. */
void rx_fourwheel_bind_controller_truck(rx_addr sim,rx_addr controller); /*004d9460 Truck/UTV*/
void rx_fourwheel_bind_controller_buggy(rx_addr sim,rx_addr controller); /*004be490 Buggy*/
