#pragma once
#include "rx_game.h"
rx_f80 rx_fourwheel_snapshot_dependency(unsigned family,unsigned method,unsigned site,rx_addr target,unsigned kind);
void rx_utv_snapshot_vectors(rx_addr vehicle,rx_addr output);
void rx_utv_snapshot_values(rx_addr vehicle,rx_addr output);
void rx_buggy_snapshot_vectors(rx_addr vehicle,rx_addr output);
void rx_buggy_snapshot_values(rx_addr vehicle,rx_addr output);
void rx_truck_snapshot_vectors(rx_addr vehicle,rx_addr output);
void rx_truck_snapshot_values(rx_addr vehicle,rx_addr output);
