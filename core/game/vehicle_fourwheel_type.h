#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_buggy_set_simulation_type(rx_addr vehicle,uint32_t type); /*004a2d00 ECX V,stacktype RET4*/
void rx_utv_set_simulation_type(rx_addr vehicle,uint32_t type); /*004c0a00*/
void rx_truck_set_simulation_type(rx_addr vehicle,uint32_t type); /*0046acf0*/
#ifdef __cplusplus
}
#endif
