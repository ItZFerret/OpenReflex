#pragma once
#include "rx_game.h"
/* ESI simulation, RET0, recorded mask10002 ECX/x87. Complete original
 * Ackermann wheel-quaternion helpers, distinct angle/output field layouts. */
void rx_fourwheel_steering_quaternions(rx_addr simulation); /*004d4190 UTV/truck*/
void rx_buggy_steering_quaternions(rx_addr simulation);     /*004b9cb0*/
