#pragma once
#include "rx_game.h"
void rx_powercross_countdown_prepare(uint32_t enabled);
void rx_powercross_countdown_bind(rx_addr activity);
void rx_powercross_countdown_unbind(void);
void rx_powercross_countdown_tick(float dt);
void rx_powercross_countdown_reset(void);
int rx_powercross_countdown_copy(rx_addr activity,uint32_t*visible,int32_t*step,uint32_t*exited,float*remaining,uint32_t*events);
void rx_powercross_countdown_activity_lifecycle(rx_addr channel,rx_addr name,rx_addr payload,uint32_t size);
