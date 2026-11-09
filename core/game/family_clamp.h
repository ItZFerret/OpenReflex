#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Isolated canonical owner revision. Link it INSTEAD OF main vehicle_utv_runtime.c
 * exactly once; ordinary UTV wrapper retains its original limit/status values. */
void rx_family_clamp_angular_velocity(rx_addr simulation,rx_addr wheel_limit,unsigned status_offset);

#ifdef __cplusplus
}
#endif
