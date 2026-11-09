#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_utv_force_stage_exact(rx_addr simulation,float dt);
void rx_buggy_force_stage_exact(rx_addr simulation,float dt);
void rx_truck_force_stage_exact(rx_addr simulation,float dt);
/* Original register ABI, caller frame already written. first supplies the
 * wrapper's one SSE result before any nested call can replace XMM0. */
void rx_fourwheel_force_stage_dependency(rx_addr target,rx_addr expected_sp,int first,float timer);
#ifdef __cplusplus
}
#endif
