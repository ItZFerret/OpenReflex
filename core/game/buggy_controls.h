#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004be4c0, ECX simulation, stack dt, RET4, recorded10005. */
void rx_buggy_controls_step_exact(rx_addr simulation,float dt);
float rx_buggy_controls_dependency(rx_addr target,rx_addr expected_sp,float xmm0,float xmm1,float xmm2);
#ifdef __cplusplus
}
#endif
