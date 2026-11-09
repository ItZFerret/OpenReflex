/* world_flag_holders.h -- the track's flag holders' (ScoreTowerFlagHolder 0x00960094, TrackSideFlagHolder 0x00960214) activity handlers and event templates (world_flag_holders.c). */
#pragma once
#include "rx_game.h"
void rx_event_register_proxy(rx_addr channel, rx_addr name, uint32_t proxy_vt, rx_addr type_info, rx_addr entity, uint32_t cb, uint32_t arg);   /* build_scene_props.c */

void rx_event_subscribe_activity_driver(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);     /* 006c45f0 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc) */
void rx_event_unsubscribe_activity_driver(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c46b0 (the same) */
void rx_event_unsubscribe_global(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);                              /* 006c4580 (ECX = name; the channel [0x00a93f74]; stack entity, callback, argument; RET 0xc) */
void rx_score_tower_first_human(rx_addr self, rx_addr activity);                                                         /* 006c18e0 (thiscall; RET 4) */
void rx_score_tower_activity_init(rx_addr self, rx_addr activity);                                                       /* 006c1910 (thiscall; RET 4) */
void rx_score_tower_activity_uninit(rx_addr self, rx_addr activity);                                                     /* 006c1990 (thiscall; RET 4) */
void rx_score_tower_two_laps(rx_addr self, rx_addr activity, rx_addr driver);                                            /* 006c1b30 (thiscall; RET 8) */
void rx_score_tower_end_of_race(rx_addr self, rx_addr activity, rx_addr driver);                                         /* 006c1b60 (thiscall; RET 8) */
void rx_score_tower_apply_state(rx_addr self, uint32_t state);                                                           /* 006c1a90 (EAX = the tower, ECX = the state 0 .. 3) */
void rx_score_tower_set_flag_texture(rx_addr self, uint32_t which);                                                      /* 006c1b80 (stdcall tower, which; RET 8) */

void rx_event_subscribe_activity_vehicle(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);     /* 006c4ab0 (ECX = channel, EAX = name; stack entity, callback, argument; RET 0xc) */
void rx_event_unsubscribe_activity_vehicle(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c4b70 (the same) */
void rx_track_flag_set_flag_texture(rx_addr holder, uint32_t which);                                                     /* 006c30f0 (stdcall; RET 8) */
void rx_track_flag_activity_init(rx_addr self, rx_addr activity);                                                        /* 006c2d90 (thiscall; RET 4) */
void rx_track_flag_activity_uninit(rx_addr self, rx_addr activity);                                                      /* 006c2e60 (thiscall; RET 4) */
void rx_track_flag_vehicle_wrecked(rx_addr self, rx_addr activity, rx_addr vehicle);                                     /* 006c2f60 (thiscall; RET 8) */
void rx_track_flag_vehicle_lapped(rx_addr self, rx_addr activity, rx_addr vehicle);                                      /* 006c3020 (thiscall; RET 8) */
