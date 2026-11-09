#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*004d0d90/004b65e0: EDI simulation, stackdt RET4, full10005.
 * Full bodies; native private Slerp/MechanismRotate frame closure is pending. */
void rx_utv_truck_active_solve_draft(rx_addr simulation,float dt);
void rx_buggy_active_solve_draft(rx_addr simulation,float dt);
/* Genuine entrySP/RA already written, never a generic special-ABI cast. */
rx_v4 rx_fourwheel_active_dependency(rx_addr target,rx_addr expected_sp,rx_v4 xmm0,rx_v4 xmm1,rx_v4 xmm2);
void* rx_fourwheel_active_bind(void* machine);
#ifdef __cplusplus
}
#endif
