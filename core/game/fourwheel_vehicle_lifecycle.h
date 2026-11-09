#pragma once
#include "rx_game.h"
void rx_utv_vehicle_world_move(rx_addr v,rx_addr world);
void rx_utv_vehicle_detach(rx_addr v);
rx_addr rx_utv_vehicle_delete(rx_addr v,uint32_t flags);
void rx_buggy_vehicle_world_move(rx_addr v,rx_addr world);
void rx_buggy_vehicle_detach(rx_addr v);
rx_addr rx_buggy_vehicle_delete(rx_addr v,uint32_t flags);
void rx_truck_vehicle_world_move(rx_addr v,rx_addr world);
void rx_truck_vehicle_detach(rx_addr v);
rx_addr rx_truck_vehicle_delete(rx_addr v,uint32_t flags);
