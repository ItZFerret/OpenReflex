#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_vehicle_wheel_block_destruct(rx_addr a); /*0042f9f0 EAX=a RET0*/
void rx_vehicle_chassis_block_destruct(rx_addr a); /*0042fd50 EAX=a RET0*/
void rx_vehicle_mechanism_destruct(rx_addr a); /*0085c390 EAX=a RET0*/
void rx_vehicle_sim_vector_destruct(rx_addr a); /*0081f6e0 EAX=a RET0*/
void rx_atv_sim_destruct(rx_addr s); /*0048cfc0 ECX=s RET0*/
rx_addr rx_atv_sim_delete(rx_addr s,uint32_t flags); /*0048c730 ECX=s RET4 EAX=s*/
#ifdef __cplusplus
}
#endif
