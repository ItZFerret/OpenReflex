#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004b9070 EDIself,stackdt RET4, recordedmask0. Native unregistered. */
void rx_buggy_engine_step_exact(rx_addr simulation,float dt);
float rx_buggy_engine_dependency(rx_addr target,rx_addr expected,float input,float t);
rx_f80 rx_buggy_engine_power_dependency(rx_addr expected,rx_f80 base,rx_f80 exponent);
#ifdef __cplusplus
}
#endif
