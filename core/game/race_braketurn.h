/* BehaviorBrakeTurn's bounded state machine, original vtable00940050. */
#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_brake_turn_enable(rx_addr b, uint32_t enabled);       /*00522eb0 ECX=b, stack enabled; RET4 */
uint32_t rx_brake_turn_steer(rx_addr b);                      /*00522fc0 ESI=b */
uint32_t rx_brake_turn_clutch(rx_addr b);                     /*00523230 ECX=b */
uint32_t rx_brake_turn_wait_pullback(rx_addr b);              /*005232a0 EDX=b */
uint32_t rx_brake_turn_pullback(rx_addr b);                   /*00523300 ECX=b */
#ifdef __cplusplus
}
#endif
