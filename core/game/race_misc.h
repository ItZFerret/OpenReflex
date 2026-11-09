/* race_misc.h -- the race frame's remaining parts: the RavenVehicleMotocross accessors the AI and the race use, MXGame's frame
 * (MXGame::Update and its helpers) and small engine helpers (race_misc_*.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* race_misc_vehicle.c */
double rx_moto_engine_rpm(rx_addr v);                                         /* 004669f0 vf47 (x87: the exact product) */
float rx_moto_vf48(rx_addr v);                                                /* 00466a10 vf48 (x87) */
void rx_moto_vf49(rx_addr v, uint32_t value);                                 /* 00466b30 vf49 */
uint32_t rx_moto_vf50(rx_addr v);                                             /* 00466b50 vf50 */
uint32_t rx_moto_vf46(rx_addr v);                                             /* 00462290 vf46 */
uint32_t rx_moto_vf45(rx_addr v);                                             /* 00466b00 vf45 */
void rx_moto_wheel_report(rx_addr v, rx_addr out);                           /* 00466de0 vf51 */
void rx_moto_state_report(rx_addr v, rx_addr out);                            /* 00467070 vf52 */
uint32_t rx_behavior_fix_progress_type(rx_addr self);                         /* 007f1110 (0xc) */
uint32_t rx_behavior_perform_stunt_type(rx_addr self);                        /* 0089e360 (0xf) */
uint32_t rx_behavior_air_leveling_type(rx_addr self);                         /* 008a5dc0 (9) */
uint32_t rx_behavior_seperate_type(rx_addr self);                             /* 008ac4a0 (6) */
uint32_t rx_moto_safe_point(rx_addr v, rx_addr pos, rx_addr vel, float thr);  /* 00466340 vf34 */
void rx_moto_reset_ground(rx_addr v);                                         /* 004664f0 vf33 */
uint32_t rx_spawn_point_clear(rx_addr v, uint32_t eax_in);                    /* 00462130 (AL) */
/* race_misc_landing.c */
uint32_t rx_landing_predict(rx_addr time_out, rx_addr veh, rx_addr point_out, rx_addr normal_out);   /* 00461980 */
/* race_misc_game.c */
uint32_t rx_int_hash_wang32(uint32_t key);                                    /* 00755d30 */
int rx_game_mode_index(rx_addr name);                                         /* 00755d60 */
uint32_t rx_stat4_value(void);                                                /* 008a7310 */
uint32_t rx_game_is_view_track(rx_addr game);                                 /* 0075c5f0 */
void rx_game_tick_during_sync(rx_addr self);                                  /* 00757f90 MXGameTickDuringSync::vf0 */
uint32_t rx_handle_equal(rx_addr a, rx_addr b);                               /* 00892090 */
uint32_t rx_game_player_mode(void);                                           /* 0075ec80 MXGame_PlayerMode */
void rx_game_debug_reload(rx_addr g);                                         /* 00759d60 */
void rx_records_update(rx_addr rec, float dt);                                /* 007532e0 */
/* race_misc_util.c */
rx_addr rx_field_14(rx_addr p);                                               /* 007fa6a0 */
uint32_t rx_field_get4(rx_addr p);                                            /* 007fa6c0 */
int32_t rx_array20_find(rx_addr arr, rx_addr item);                           /* 007f9e40 */
void rx_array20_remove(uint32_t index, rx_addr arr);                          /* 007f9ee0 */
int32_t rx_array40_find(rx_addr arr, rx_addr item);                           /* 007f9f80 */
rx_addr rx_vector_erase(rx_addr vec, rx_addr out, rx_addr pos);               /* 007f9140 */
uint32_t rx_net_session_state(void);                                          /* 008953c0 */
void rx_vehicle_effect_query(rx_addr v, uint32_t a, uint32_t b);              /* 00460020 */
uint32_t rx_name_hash_find(rx_addr map, rx_addr keyp, rx_addr out);          /* 007fa250 */
void rx_name_hash_free(rx_addr map);                                          /* 007fa1e0 */
uint32_t rx_vector_storage_alloc(rx_addr self, uint32_t n);                    /* 007f9570 (EDI = n, ESI = the vector) */
rx_addr rx_vector_move_range(rx_addr end, rx_addr begin, rx_addr dst);         /* 007fa2b0 (EAX, EDI, EDX) */
rx_addr rx_vector_assign(rx_addr self, rx_addr other, uint32_t ecx_in, uint32_t* ecx_out);   /* 007f8fa0 */
uint32_t rx_scene_patch_height(rx_addr b, rx_addr p, rx_addr normal_out, rx_addr g, float scale, float offset);   /* 007ea020 (stdcall; RET 0x18) -> EAX */
uint32_t rx_scene_ground_probe(rx_addr list, rx_addr point, rx_addr normal_out);   /* 007f0050 */
/* race_misc_pose.c */
void rx_pose_overrides_clear(void);                                           /* 0047c3f0 */
void rx_pose_override_start(rx_addr slot, rx_addr vehicle, rx_addr anim, uint32_t keep);   /* 0047c0b0 */
void rx_pose_override_set(rx_addr vehicle, rx_addr anim, uint32_t keep);      /* 0047c310 */
void rx_pose_override_update(rx_addr slot, float dt);                         /* 0047c190 */
void rx_pose_overrides_update(float dt);                                      /* 0047c2a0 */
void rx_effect_list_update(rx_addr list, float dt);                           /* 0047bc60 */
void rx_damage_entry_update(rx_addr entry, float dt);                         /* 00479970 (ESI = entry; stdcall dt; RET 4) */
void rx_damage_part_fall(rx_addr entry, uint32_t index, float dt);            /* 0047b960 (stdcall entry, index; xmm1 = dt; RET 8) */
void rx_post_motocard_data(rx_addr mgr, uint32_t a, uint32_t b, uint32_t c);  /* 007556d0 (stdcall; RET 0x10) */
#ifdef __cplusplus
}
#endif
