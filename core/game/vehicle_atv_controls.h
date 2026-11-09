#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_control_update(rx_addr sim,float dt); /*00497a70 ECX=S RET4*/
void rx_atv_air_control(rx_addr sim,float dt); /*00494910 stack S/dt RET8*/
void rx_atv_ground_control(rx_addr sim,float dt); /*00496880 ESI=S stack dt RET4*/
#ifdef __cplusplus
}
#endif
