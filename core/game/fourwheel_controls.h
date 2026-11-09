#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004d9490: ECX simulation, stack dt, RET4, live10005. Isolated candidate. */
void rx_utv_controls_step_exact(rx_addr simulation,float dt);
/* Private exact-frame dependency bridge. C already wrote the real return
 * address/arguments and selected the special input registers. */
float rx_utv_controls_dependency(rx_addr target,rx_addr expected_sp,
 float xmm0,float xmm1,float xmm2);
#ifdef __cplusplus
}
#endif
