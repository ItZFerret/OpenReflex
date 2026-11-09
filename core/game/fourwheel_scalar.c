#include "fourwheel_scalar.h"
/* 0044f680, seven bytes: UTV and TrophyTruck share the same field. */
float rx_fourwheel_scalar_truck(rx_addr simulation) { return F32(simulation, 0x5618); }
RX_FUNCTION(0x0044f680, rx_fourwheel_scalar_truck)
/* 004a2510, seven bytes: Buggy's corresponding field is sixteen bytes earlier. */
float rx_fourwheel_scalar_buggy(rx_addr simulation) { return F32(simulation, 0x5608); }
RX_FUNCTION(0x004a2510, rx_fourwheel_scalar_buggy)
