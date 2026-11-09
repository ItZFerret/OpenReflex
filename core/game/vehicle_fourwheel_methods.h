#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_sim_utv_apply_tuning(rx_addr s);
void rx_sim_buggy_apply_tuning(rx_addr s);
void rx_sim_truck_apply_tuning(rx_addr s);
void rx_sim_buggy_geometry(rx_addr s); /*004ba270 vf16*/
void rx_sim_truck_geometry(rx_addr s); /*004564d0 vf16*/
void rx_sim_utv_geometry(rx_addr s); /*004d4750 vf16*/
void rx_sim_utv_reset(rx_addr s,rx_addr p,rx_addr q);
void rx_sim_buggy_reset(rx_addr s,rx_addr p,rx_addr q);
void rx_sim_truck_reset(rx_addr s,rx_addr p,rx_addr q);
#ifdef __cplusplus
}
#endif
