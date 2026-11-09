/* Hand-translated MXActivityWaypointRace. Isolated candidate; validation pending. */
#pragma once
#include "rx_game.h"
rx_addr rx_waypoint_activity_construct(rx_addr activity, uint32_t type, rx_addr world);
void rx_waypoint_activity_destroy(rx_addr activity);
rx_addr rx_waypoint_activity_sdtor(rx_addr activity, uint32_t flags);
void rx_waypoint_activity_vf1(rx_addr activity, rx_addr row);
void rx_waypoint_activity_vf2(rx_addr activity, rx_addr overlay);
void rx_waypoint_activity_vf4(rx_addr activity);
void rx_waypoint_activity_vf5(rx_addr activity);
void rx_waypoint_start_slots(rx_addr activity);
void rx_waypoint_gate_distances(rx_addr activity);
void rx_waypoint_collect_markers(rx_addr activity);
void rx_waypoint_subscribe_lap(rx_addr channel, rx_addr activity);
void rx_waypoint_scene_collect_typed(rx_addr array, rx_addr scene, uint32_t type, rx_addr group);
int32_t rx_waypoint_compare_gates(rx_addr left, rx_addr right);
float rx_waypoint_route_length(rx_addr activity); /* 00579fd0 EDX=activity -> XMM0 */
rx_addr rx_waypoint_gate_at(int32_t index, rx_addr activity); /* 0057a7d0 ECX=index EDX=activity */
void rx_waypoint_set_marker(rx_addr activity, int32_t index, uint32_t active); /* 0057a8d0 RET12 */
void rx_waypoint_select_marker(rx_addr activity, int32_t index); /* 0057a940 RET8 */
void rx_waypoint_advance_markers(rx_addr activity, int32_t index, rx_addr driver); /* 0057a9b0 EDI=activity,EAX=index,stack driver */
uint32_t rx_waypoint_activity_vf8(rx_addr activity);
void rx_waypoint_activity_vf18(rx_addr activity);
void rx_waypoint_activity_vf23(rx_addr activity, float dt);
void rx_waypoint_activity_vf26(rx_addr activity, float dt);
void rx_waypoint_activity_vf38(rx_addr activity, uint32_t previous);
rx_addr rx_waypoint_activity_vf44(rx_addr activity, rx_addr player);
uint32_t rx_waypoint_activity_vf48(rx_addr activity);
void rx_waypoint_activity_vf52(rx_addr activity);
void rx_waypoint_activity_base_destroy(rx_addr activity); /* 005566b0 */
void rx_waypoint_state_map_destroy(rx_addr map); /* 00556660 */
void rx_waypoint_state_nodes_destroy(rx_addr node); /* 00586060 */
void rx_waypoint_activity_vf37(rx_addr activity, uint32_t previous);
void rx_waypoint_activity_vf25(rx_addr activity, float dt);
void rx_waypoint_activity_vf42(rx_addr activity);
uint32_t rx_waypoint_activity_vf61(rx_addr activity, uint32_t index);
void rx_waypoint_lap_completed(rx_addr activity, rx_addr driver, int32_t lap, uint32_t flag, int32_t laps);
float rx_waypoint_remaining_metric(rx_addr activity, rx_addr driver);
int32_t rx_waypoint_activity_vf43(rx_addr activity, rx_addr first, rx_addr second, uint32_t unused);
void rx_waypoint_activity_vf46(rx_addr activity, uint32_t unused, uint32_t available, float time);
void rx_waypoint_activity_vf39(rx_addr activity, uint32_t previous);
void rx_waypoint_show_results(rx_addr activity);
void rx_waypoint_post_state(rx_addr activity, uint32_t state);
void rx_waypoint_update_post(rx_addr activity, float dt);
void rx_waypoint_activity_vf29(rx_addr activity, float dt);
void rx_waypoint_activity_vf35(rx_addr activity, uint32_t next);
void rx_waypoint_activity_vf58(rx_addr activity);
void rx_waypoint_activity_vf41(rx_addr activity, uint32_t previous);
