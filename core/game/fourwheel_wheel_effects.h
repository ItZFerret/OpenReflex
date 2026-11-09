#pragma once
#include "rx_game.h"
/* stdcall(vehicle,dt), RET8. Returns the original live ECX cookie for adapters;
 * no semantic EAX output. Buggy also preserves the incoming x87 stack. */
uint32_t rx_fourwheel_wheel_effects_utv(rx_addr vehicle,float dt);
uint32_t rx_fourwheel_wheel_effects_buggy(rx_addr vehicle,float dt);
uint32_t rx_fourwheel_wheel_effects_truck(rx_addr vehicle,float dt);
