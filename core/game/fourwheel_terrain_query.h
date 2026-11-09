#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_utv_wheel_terrain_query_exact(rx_addr wheel,rx_addr simulation);
void rx_buggy_wheel_terrain_query_exact(rx_addr wheel,rx_addr simulation);
void rx_truck_wheel_terrain_query_exact(rx_addr wheel,rx_addr simulation);
/* Prewritten original private frame. Complete original/natively typed bridge
 * implementations are separate proof obligations; no target registration. */
void rx_fourwheel_query_dependency(rx_addr target,rx_addr expected_sp);
void* rx_fourwheel_query_bind(void* machine);
#ifdef __cplusplus
}
#endif
