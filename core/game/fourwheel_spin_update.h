#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original simulation vf17: ECX=self, unused stack dt, RET4, live10005.
 * UTV/Truck share452650; Buggy4b5080 only changes four output offsets. */
void rx_fourwheel_spin_update_truck(rx_addr self,float dt);
void rx_fourwheel_spin_update_buggy(rx_addr self,float dt);
/* The one semantic kernel optionally exposes its already-computed final
 * packed subtraction; callers own RET4. No original execution or imports. */
void rx_fourwheel_spin_update_observed(rx_addr self,unsigned buggy,rx_v4 *live);
#ifdef __cplusplus
}
#endif
