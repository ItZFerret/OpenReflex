#pragma once
#include "rx_game.h"
/* GenericBrake is shared by driver classes; these are actual original owners. */
uint32_t rx_generic_brake_update(rx_addr behavior, float dt);       /*00530ff0 vf4*/
void rx_generic_brake_scan(rx_addr behavior);                       /*00531030 EAX*/
float rx_generic_brake_contact_time(rx_addr behavior, rx_addr other_driver, float max_move); /*005311d0 EDI,EAX,stack*/
float rx_generic_brake_crossing_time(rx_addr other_driver, rx_addr behavior); /*00531430 ECX,stack*/
