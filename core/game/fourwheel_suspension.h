#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Complete3073/3076/3134B originals4d0180/4b59d0/452fa0.
 * EDI simulation, RET0; recorded mask0 unchanged. No registration.
 * This revision REPLACES the accepted UTV owner TU in a future isolated link;
 * never link both definitions of rx_utv_suspension_step_exact. */
void rx_utv_suspension_step_exact(rx_addr simulation);
void rx_buggy_suspension_step_exact(rx_addr simulation);
void rx_truck_suspension_step_exact(rx_addr simulation);
/* Actual private entry/RA/arguments already written. family0/1/2.
 * Exact site chooses ForceAtPoint RET4 or cookie RET0, no generic dispatch. */
void rx_family_suspension_dependency(unsigned family,rx_addr expected_sp);
void* rx_family_suspension_bind(void* machine);
#ifdef __cplusplus
}
#endif
