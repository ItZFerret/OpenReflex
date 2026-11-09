#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_vehicle_base_destruct(rx_addr v); /*0045d3c0 ECX=V RET0*/
rx_addr rx_atv_vehicle_delete(rx_addr v,uint32_t flags); /*0047c780 ECX=V RET4 EAX=V*/
void rx_vehicle_base_detach(rx_addr v); /*00460230 ECX=V RET0*/
void rx_atv_vehicle_detach(rx_addr v); /*0047cbc0 ECX=V RET0*/
void rx_atv_listener_pre_step(rx_addr listener,float dt); /*0047d670 ECX=V+90 RET4*/
#ifdef __cplusplus
}
#endif
