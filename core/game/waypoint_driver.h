#pragma once
#include "rx_game.h"
/*00516730 originalECX=driver,EAX=player,RET0. Typed direct composition.*/
rx_addr rx_waypoint_driver_construct(rx_addr driver,rx_addr player);
rx_addr rx_waypoint_driver_sdtor(rx_addr driver,uint32_t flags); /*005167b0*/
void rx_waypoint_driver_vf6(rx_addr driver); /*00517800*/
uint32_t rx_waypoint_driver_vf12(rx_addr driver); /*005183d0*/
float rx_waypoint_driver_vf13(rx_addr driver); /*00518400, originalFLDm32*/
float rx_waypoint_driver_distance_to_next(rx_addr driver); /*00517d20*/
rx_addr rx_waypoint_driver_find_behavior22(rx_addr driver); /*0053b400*/
float rx_waypoint_driver_progress(rx_addr driver); /*00517bb0*/
float rx_waypoint_driver_lap_scaled_progress(rx_addr driver); /*00517b70*/

void rx_waypoint_activity_vf47(rx_addr activity,rx_addr driver); /*00578d20*/
rx_addr rx_waypoint_get_free_ride_rule(rx_addr driver); /*00587690*/
rx_addr rx_waypoint_get_advance_rule(rx_addr driver); /*0058a480*/
rx_addr rx_waypoint_get_avoid_wreck(rx_addr driver); /*005876f0*/
rx_addr rx_waypoint_get_suck_racing(rx_addr driver); /*005888b0*/
rx_addr rx_waypoint_get_air_leveling(rx_addr driver); /*005877a0*/
rx_addr rx_waypoint_get_fix_forward(rx_addr driver); /*005877f0*/
rx_addr rx_waypoint_get_brake_turn(rx_addr driver); /*00588590*/
rx_addr rx_waypoint_get_racing_lines(rx_addr driver); /*00587e00*/
rx_addr rx_waypoint_get_waypoint_navigation(rx_addr driver); /*0058a4d0*/
rx_addr rx_waypoint_get_avoid_objects(rx_addr driver); /*00587890*/
rx_addr rx_waypoint_get_cornering(rx_addr driver); /*0058a520*/
rx_addr rx_waypoint_get_avoid_flip_back(rx_addr driver); /*005878e0*/

/*Offline lifecycle only: actual network payload ownership remains unclosed.*/
void rx_waypoint_driver_vf1(rx_addr driver);
void rx_waypoint_driver_vf2(rx_addr driver);
void rx_waypoint_emit_pose(rx_addr channel,rx_addr driver,rx_addr matrix,uint32_t flag);
float rx_waypoint_driver_total_time(rx_addr driver); /*00518430 EDI=driver,XMM0*/
void rx_waypoint_driver_vf3(rx_addr driver); /*00516a30 offline typed path*/
