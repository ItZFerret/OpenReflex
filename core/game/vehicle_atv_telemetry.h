#pragma once
#include "world_veh.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_f80 rx_atv_engine_rpm(rx_addr vehicle); /*00486620 ECX RET0 ST0*/
rx_f80 rx_atv_grip_factor(rx_addr vehicle,uint32_t axle); /*00486850 ECX,stackaxle RET4 ST0*/
void rx_atv_snapshot_vectors(rx_addr vehicle,rx_addr out); /*00486cc0 ECX,stackout RET4*/
void rx_atv_snapshot_values(rx_addr vehicle,rx_addr out); /*00487510 ECX,stackout RET4*/
#ifdef __cplusplus
}
#endif
