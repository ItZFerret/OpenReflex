#pragma once
#include "../src/rx_mem.h"
float rx_controller_stick_drive_axis(uint32_t controller); /*005485e0 ESI=controller, RET0, x87 result*/
uint32_t rx_controller_compound_available(rx_addr action,uint32_t controller); /*00811c90 thiscall RET4*/
