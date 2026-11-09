/* race_driver.h -- the drivers (race_driver*.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
float rx_float_get(rx_addr p);                                                  /* 00508b60 */
void rx_float_set4(rx_addr obj, float v);                                       /* 00508b70 */
void rx_copy_pair_20(rx_addr obj, rx_addr out);                                 /* 00508c00 */
rx_addr rx_field_b58(rx_addr obj);                                              /* 00508a90 */
uint32_t rx_debug_keymap_get(rx_addr map, uint32_t a, uint32_t b);              /* 005089a0 DebugKeyMap::vf2 */
rx_addr rx_race_project_on(rx_addr a, rx_addr b, rx_addr out);                  /* 0050deb0 */
float rx_driver_distance_to_line(rx_addr d);                                    /* 00512620 */
float rx_driver_total_time(rx_addr d);                                          /* 00512870 */
float rx_driver_standings_time(rx_addr d);                                      /* 00512910 */
void rx_driver_eliminate(rx_addr d, uint32_t remaining);                       /* 00512c80 (ECX = driver, EAX = riders left) */
rx_addr rx_stats_static_construct(void);                                        /* 006de1f0: the session/stats static block 0xda6ca0 */
void race_static_dcf304(void);
float rx_driver_progress(rx_addr d, rx_addr pos);                               /* 00512690 */
void race_vehicle_position(rx_addr vehicle, rx_addr out);
int rx_driver_has_lapped(rx_addr a, rx_addr b);                                 /* 00512d00 */
void rx_race_driver_on_rule(rx_addr d, rx_addr rule);                           /* 00512140 RaceDriver::vf9 */
/* race_driver_update.c */
void rx_base_driver_update(rx_addr d, float dt);                                /* 00509370 BaseDriver::vf7 */
void rx_network_driver_update(rx_addr d, float dt);                             /* 0050e510 NetworkDriver::vf7 */
void rx_ai_start_line_controls(rx_addr d);                                      /* 00511760 */
void rx_ai_race_start(rx_addr d);                                               /* 0050fef0 */
void rx_ai_delayed_start(rx_addr d);                                            /* 0050fac0 (ESI = driver) */
void rx_race_driver_countdown(rx_addr d);                                       /* 0050fd30 RaceDriver::vf6 */
void rx_race_driver_update(rx_addr d, float dt);                                /* 0050f620 RaceDriver::vf7 */
void rx_free_ride_rules_update(rx_addr d);                                      /* 0050bfa0 */
void rx_free_ride_ai_update(rx_addr d, float dt);                               /* 0050c070 (EAX = driver; stack dt; RET 4) */
void rx_free_ride_steer(rx_addr d);                                             /* 0050c500 (stdcall; RET 4) */
void rx_free_ride_timer(rx_addr d);                                             /* 0050c7f0 (ECX = driver) */
void rx_free_ride_debug_draw(rx_addr d);                                        /* 0050ab00 (ESI = driver) */
uint32_t rx_ai_start_lean(rx_addr d, rx_addr rec);                              /* 0050b590 (EAX = driver, EDI = rec; RET 0) -> EAX */
void rx_ai_lean_start(rx_addr d, rx_addr orig_frame);                           /* 0050ad00 (ESI = driver) */
uint32_t rx_debug_keymap_latched(rx_addr self, uint32_t mask, uint32_t pad);    /* 00508970 DebugKeyMap::vf1 (ECX; RET 8) -> EAX */
void rx_ai_wreck_controls_off(rx_addr d);                                       /* 00509c20 (EAX = driver) */
void rx_free_ride_driver_update(rx_addr d, float dt);                           /* 0050bf30 FreeRideDriver::vf7 */
void rx_human_driver_rules(rx_addr d, float dt, rx_addr orig_frame);            /* 00511430 */
void rx_driver_update_highlight(rx_addr d);                                     /* 00511f00 */
int rx_driver_predict_side(rx_addr d, rx_addr out, rx_addr orig_frame);         /* 00511f80 */
void rx_ai_track_node_reactions(rx_addr d, float dt);                           /* 005118c0 */
void rx_ai_race_update(rx_addr d, float dt);                                    /* 00510010 */
void rx_driver_vf5(rx_addr d);                                                    /* 0050fb00 RaceDriver::vf5 */
void rx_race_driver_vf4(rx_addr d);                                              /* 0050f5d0 RaceDriver::vf4 */
float rx_driver_start_chance(rx_addr d);                                          /* 0051dcd0 (EAX = the driver) -> xmm0 */

#ifdef __cplusplus
}
#endif
