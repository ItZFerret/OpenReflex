#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Original EDI-only simulation input, RET0; recorded10002 ECX/x87. */
void rx_utv_select_power_gear(rx_addr simulation); /*004d3350*/
void rx_utv_select_coast_gear(rx_addr simulation); /*004d35c0*/
/* Exact private curve ABI: caller has written real RA at entry ESP. */
float rx_utv_force_gear_curve(rx_addr expected_sp,float value);
void rx_buggy_select_power_gear(rx_addr);
void rx_buggy_select_coast_gear(rx_addr);
void rx_truck_select_power_gear(rx_addr);
void rx_truck_select_coast_gear(rx_addr);
#ifdef __cplusplus
}
#endif
