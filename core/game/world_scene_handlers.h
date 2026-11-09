/* world_scene_handlers.h -- event handlers of the event world's scene entities and of the rider clip recorder (world_scene_handlers.c). */
#pragma once
#include "rx_game.h"
void rx_popup_bale_vehicle_exited(rx_addr self, rx_addr vehicle, rx_addr trigger);      /* 006c14a0 (thiscall; RET 8) */
void rx_popdown_bale_vehicle_exited(rx_addr self, rx_addr vehicle, rx_addr trigger);    /* 006c0b60 (thiscall; RET 8) */
void rx_clip_event_stunt_begin(rx_addr self, rx_addr player, rx_addr stunt);            /* 006ab290 (thiscall on 0x00d32360; RET 8) */
void rx_clip_event_stunt_hold_begin(rx_addr self, rx_addr player, rx_addr stunt);      /* 006ab380 */
void rx_clip_event_stunt_hold_end(rx_addr self, rx_addr player, rx_addr stunt);        /* 006ab410 */
void rx_clip_event_stunt_completed(rx_addr self, rx_addr player, rx_addr stunt);       /* 006ab4a0 */
void rx_event_unregister_vehicle_exited_up(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg);   /* 006c4450 (EAX = channel; RET 0xc) */
void rx_event_unregister_vehicle_exited_down(rx_addr channel, rx_addr entity, uint32_t cb, uint32_t arg); /* 006c4330 (EAX = channel; RET 0xc) */
void rx_event_unregister_activity(rx_addr channel, rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg); /* 006c3c70 (ECX = channel, EAX = name; RET 0xc) */
void rx_event_unregister_global_c(rx_addr name, rx_addr entity, uint32_t cb, uint32_t arg);                /* 006c4a40 (ECX = name; RET 0xc) */
void rx_event_unregister_first_human(rx_addr entity, uint32_t cb, uint32_t arg);                           /* 006c3930 (RET 0xc) */
void rx_popup_bale_vf9(rx_addr self);                                                                      /* 006c0f50 (thiscall) */
void rx_popdown_bale_vf9(rx_addr self);                                                                    /* 006c0730 (thiscall) */
void rx_score_tower_unlisten(rx_addr self);                                                                /* 006c1890 (ESI = the tower) */
void rx_score_tower_vf9(rx_addr self);                                                                     /* 006c1830 (thiscall) */
void rx_track_flag_unlisten(rx_addr self);                                                                 /* 006c2d40 (ESI = the holder) */
void rx_track_flag_vf9(rx_addr self);                                                                      /* 006c2ce0 (thiscall) */
void rx_thirty_second_board_vf9(rx_addr self);                                                             /* 006b7b60 (thiscall) */
rx_addr rx_stat_table_find(rx_addr table, uint32_t key);                                                    /* 005411f0 (ECX = table, EDI = key) -> EAX */
float rx_freestyle_driver_best_score(rx_addr driver);                                                      /* 0050de10 (EAX = driver) -> xmm0 */
void rx_dataminer_activity_end(rx_addr rec, rx_addr activity);                                             /* 007538c0 (thiscall; RET 4) */
