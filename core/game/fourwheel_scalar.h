#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Simulation slot19: FLD m32; RET. Same native float ABI as rx_sim_vf19. */
float rx_fourwheel_scalar_truck(rx_addr simulation);
float rx_fourwheel_scalar_buggy(rx_addr simulation);
#ifdef __cplusplus
}
#endif
