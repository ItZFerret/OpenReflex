#pragma once
#include "rx_game.h"
/* Original stdcall(P, XMLReader*), RET8. These are parameter blocks, NOT sim
 * pointers or host rx_streams. No vf8 reload entry is registered by this slice. */
uint32_t rx_utv_params_apply(rx_addr parameters, rx_addr reader); /*004cb3f0*/
uint32_t rx_buggy_params_apply(rx_addr parameters, rx_addr reader); /*004b0bc0*/
uint32_t rx_truck2wd_params_apply(rx_addr parameters, rx_addr reader); /*0044f7a0*/
