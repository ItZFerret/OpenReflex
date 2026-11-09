#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_vehicle_proxy_handle_release(rx_addr handle); /*0042af20 ECX=handle RET0*/
rx_addr rx_vehicle_proxy_handle_delete(rx_addr handle,uint32_t flags); /*0042b090 ECX=handle RET4 EAX=handle*/
#ifdef __cplusplus
}
#endif
