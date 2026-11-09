/* build_activity_pre.h -- Activity's Pre state enter (build_activity_pre.c). */
#pragma once
#include "rx_game.h"

void rx_physics_world_reset_bodies(rx_addr pw);                       /* 00770d50 (ECX = the physics world) */
void rx_session_queue_timer_event(rx_addr channel, uint32_t kind, float value);   /* 00707320 (stdcall; RET 0xc) */
void rx_session_pre_timer(rx_addr session, float seconds);           /* 006e4a90 (ESI = the session [0xda6cb0]; stack float) */
void rx_scene_views_reset(void);                                      /* 00559ac0 */
void rx_damage_manager_release_targets(rx_addr mgr);                  /* 0047bce0 (EAX = the manager 0x00db1410) */
void rx_activity_vf37(rx_addr activity);                              /* Activity::vf37 005589c0 (thiscall; RET 4: the old state) */
uint32_t rx_free_ride_vf54(rx_addr activity);                          /* MXActivityFreeRide::vf54 005602f0 (thiscall) -> EAX */
void     rx_free_ride_collectables_hide(rx_addr tracker, rx_addr activity);   /* 00585790 (ESI = the tracker; stdcall activity; RET 4) */
void     rx_entity_bodies_flag_clear(rx_addr entity);                  /* 00819be0 (stdcall; RET 4) */
void     rx_free_ride_pickups_hide(rx_addr activity);                  /* 00561820 (stdcall; RET 4) */
void     rx_free_ride_vf37(rx_addr activity, uint32_t old);            /* MXActivityFreeRide::vf37 005603c0 (thiscall; RET 4) */
