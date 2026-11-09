#pragma once
#include "rx_game.h"
void rx_cam_ramp_init(float limit, uint32_t base_bits);   /* 0067f3c0 (XMM2 = limit, stack = base; RET 4) */
void rx_cam_ramp_setup(void);                              /* 00674d90 */
