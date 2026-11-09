#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004d1ce0/004b7530: ECX simulation, stackdt RET4, full10005.
 * Source-only wrapper: active helper's private native frame proof is pending. */
void rx_utv_truck_solve_vf12_draft(rx_addr simulation,float dt);
void rx_buggy_solve_vf12_draft(rx_addr simulation,float dt);
#ifdef __cplusplus
}
#endif
