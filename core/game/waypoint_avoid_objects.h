#pragma once
#include "rx_game.h"
uint32_t rx_waypoint_avoid_objects_update(rx_addr behavior,float dt);
float rx_waypoint_avoid_square(float value);
float rx_waypoint_avoid_track_lateral(rx_addr driver,rx_addr position);
void rx_waypoint_obstacle_copy_initial(rx_addr destination,rx_addr source);
void rx_waypoint_obstacle_copy(rx_addr destination,rx_addr source);
