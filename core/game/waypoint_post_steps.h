#pragma once
#include "rx_game.h"
void rx_post_place_object(rx_addr locator_name,rx_addr activity,rx_addr object,rx_addr animation); /*0055a560*/
void rx_post_vehicle_animation_name(uint32_t kind,rx_addr name,rx_addr out); /*0055e310*/
void rx_post_push_bike(rx_addr vehicle,rx_addr chief,rx_addr locator,rx_addr podium); /*0074c0d0*/
void rx_post_place_vehicle(rx_addr vehicle,rx_addr position,rx_addr forward,rx_addr animation); /*0074c1e0*/
void rx_post_vehicle_at_locator(rx_addr actor,rx_addr podium,rx_addr locator_name); /*0074c480*/
void rx_post_place_chief(rx_addr scenario,rx_addr player,rx_addr locator_name,uint32_t push_bike,rx_addr actor_name); /*0074f380*/
void rx_post_lose_walk(rx_addr scenario); /*0074f410*/
void rx_post_step_cameras(rx_addr scenario,uint32_t step); /*0074f510*/
void rx_post_step_entities(rx_addr scenario,uint32_t step); /*0074f590*/
void rx_post_podium_actors(rx_addr scenario); /*0074f620*/
void rx_post_winner_mound(rx_addr scenario); /*0074f7e0*/
void rx_post_debug_standings(rx_addr scenario,int32_t step); /*0074f260*/
void rx_waypoint_finish_vf1(rx_addr scenario,uint32_t step); /*0074eda0*/
void rx_waypoint_results_vf1(rx_addr scenario,uint32_t step); /*0074fbe0*/
