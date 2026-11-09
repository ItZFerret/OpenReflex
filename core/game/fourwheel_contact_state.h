#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_fourwheel_all_contact_truck(rx_addr simulation);
uint32_t rx_fourwheel_all_contact_buggy(rx_addr simulation);
uint32_t rx_fourwheel_grounded_truck(rx_addr simulation);
uint32_t rx_fourwheel_grounded_buggy(rx_addr simulation);
rx_addr rx_fourwheel_contact_normal_truck(rx_addr simulation,rx_addr output);
rx_addr rx_fourwheel_contact_normal_buggy(rx_addr simulation,rx_addr output);
void rx_fourwheel_save_recovery_pose(rx_addr simulation);
#ifdef __cplusplus
}
#endif
