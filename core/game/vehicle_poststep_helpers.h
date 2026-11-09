#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_vehicle_mechanism_node_poststep(rx_addr mechanism); /*0085c6a0 stack M RET4*/
void rx_vehicle_wheel_offset_debug(rx_addr simulation,rx_addr wheel,float offset); /*004d7fa0 EDX sim, stack wheel/offset RET8*/
void rx_vehicle_wheel_axis_debug(rx_addr simulation,rx_addr wheel); /*00459de0 EDX sim, stack wheel RET4*/
#ifdef __cplusplus
}
#endif
