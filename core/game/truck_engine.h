#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004556d0 EDIself,stackdt RET4; recordedmask0, supplemental fullstate required. */
void rx_truck_engine_step_exact(rx_addr simulation,float dt);
float rx_truck_engine_dependency(rx_addr target,rx_addr expected,float input,float t);
rx_f80 rx_truck_engine_power_dependency(rx_addr expected,rx_f80 base,rx_f80 exponent);
rx_f80 rx_truck_engine_fmod_dependency(rx_addr expected);
#ifdef __cplusplus
}
#endif
