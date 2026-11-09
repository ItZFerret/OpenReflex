#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Development composition boundary. Each site describes one genuine call;
 * the semantic owner neither executes original code nor substitutes effects. */
rx_v4 rx_fourwheel_update_dependency(unsigned family, unsigned site,
                                     rx_addr target, rx_addr expected_sp, rx_v4 xmm0);
void rx_fourwheel_vehicle_update_utv(rx_addr vehicle,float dt);
void rx_fourwheel_vehicle_update_buggy(rx_addr vehicle,float dt);
void rx_fourwheel_vehicle_update_truck(rx_addr vehicle,float dt);
#ifdef __cplusplus
}
#endif
