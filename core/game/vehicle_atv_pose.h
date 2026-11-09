#pragma once
#include "../src/rx_mem.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_atv_place_composite(rx_addr vehicle); /*0047cfa0 ECX=V RET0*/
void rx_atv_update_model_pose(rx_addr vehicle); /*0047e850 ECX=V, unused stack dt RET4*/
#ifdef __cplusplus
}
#endif
