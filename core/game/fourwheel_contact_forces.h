#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/*00452a90, EDI simulation, RET0, recorded10006 ECX/EDX/x87. */
void rx_fourwheel_apply_contact_forces(rx_addr simulation);
/* Exact static-call observer; caller has written actual return/stack words. */
float rx_fourwheel_contact_dependency(rx_addr target,rx_addr expected_sp,float xmm0);
void rx_buggy_apply_contact_forces(rx_addr);
#ifdef __cplusplus
}
#endif
