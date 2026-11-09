#pragma once
#include "rx_game.h"
void rx_waypoint_intro_vf0(rx_addr scenario);
void rx_waypoint_intro_vf1(rx_addr scenario, uint32_t step);
uint32_t rx_waypoint_intro_vf3(rx_addr scenario);
uint32_t rx_waypoint_intro_vf13(rx_addr scenario);
rx_addr rx_waypoint_chief_clip(rx_addr player, uint32_t index, rx_addr fallback);
rx_addr rx_waypoint_camera_clip(rx_addr player, uint32_t index, rx_addr fallback);
void rx_waypoint_intro_place_chiefs(rx_addr scenario, uint32_t step);
void rx_waypoint_intro_cameras(rx_addr scenario, uint32_t step);
void rx_waypoint_podium_scalars(rx_addr podium);
uint32_t rx_waypoint_finish_script_exists(rx_addr activity);
uint32_t rx_waypoint_activity_vf7(rx_addr activity);
uint32_t rx_waypoint_finish_vf3(rx_addr scenario);
void rx_waypoint_finish_vf0(rx_addr scenario);
void rx_waypoint_results_vf0(rx_addr scenario);
void rx_waypoint_finish_vf6(rx_addr scenario);
void rx_waypoint_results_vf6(rx_addr scenario);
uint32_t rx_waypoint_results_vf3(rx_addr scenario);
void rx_waypoint_post_vf12(void);
