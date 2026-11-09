#pragma once
#include "rx_game.h"
#include "crt.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004d3820 EDI simulation, stack dt, RET4, recorded mask0. */
void rx_utv_engine_step_exact(rx_addr simulation,float dt);
float rx_utv_engine_dependency(rx_addr target,rx_addr expected_sp,float xmm0,float xmm2);
rx_f80 rx_utv_engine_power_dependency(rx_addr expected_sp,rx_f80 base,rx_f80 exponent);
#ifdef __cplusplus
}
#endif
