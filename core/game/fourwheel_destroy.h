#pragma once
#include "rx_game.h"
void rx_fourwheel_sim_destruct_utv(rx_addr simulation);
void rx_fourwheel_sim_destruct_buggy(rx_addr simulation);
void rx_fourwheel_sim_destruct_truck(rx_addr simulation);
rx_addr rx_fourwheel_sim_delete_utv(rx_addr simulation,uint32_t flags);
rx_addr rx_fourwheel_sim_delete_buggy(rx_addr simulation,uint32_t flags);
rx_addr rx_fourwheel_sim_delete_truck(rx_addr simulation,uint32_t flags);
