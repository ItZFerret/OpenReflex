#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00487b30: stdcall V, dt, RET8. Four-wheel contact/deformation and water publication. */
void rx_atv_vehicle_wheels(rx_addr v, float dt);
/* Shared source water leaves reached by the wheel update. */
rx_addr rx_atv_water_singleton(void); /* 00424340, cdecl, EAX result */
void rx_atv_vehicle_add_water_force(rx_addr v, float amount); /* 0045f500, EAX=V, stack float, RET4 */
#ifdef __cplusplus
}
#endif
