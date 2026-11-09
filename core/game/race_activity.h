/* race_activity.h -- the activities (race_activity*.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t rx_activity_ui_kind(rx_addr act);                                      /* 00556280 Activity_UiKind */
int32_t rx_race_stub_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);  /* 00567520 MXActivityRaceStub::vf43 */
void rx_activity_sort_standings(rx_addr act);                                   /* 00559eb0 */
void rx_race_lapping_warnings(rx_addr act);                                     /* 00567f30 */
void rx_race_time_gap(rx_addr d, rx_addr act, rx_addr other);                   /* 00567c70 */
uint32_t rx_players_count(rx_addr players, uint32_t kind);                      /* 00552ce0 */
void rx_players_update_cameras(rx_addr players, float dt);                      /* 00552a10 */
int rx_race_dry_ground(rx_addr pos);                                            /* 00550f00 */
void rx_race_gaps_to_leader(rx_addr act);                                       /* 00567b10 */
void rx_race_lap_flags(rx_addr act);                                            /* 00567e30 */
void rx_race_lap_highlight(rx_addr act);                                        /* 00569820 */
uint32_t rx_race_driver_done(rx_addr act, uint32_t index);                      /* 00566e70 MXActivityRaceStub::vf61 */
void rx_race_activity_in_progress(rx_addr act, float dt);                       /* 00565270 MXActivityRace::vf28 */
void rx_race_stub_update(rx_addr act, float dt);                                /* 005680c0 MXActivityRaceStub::vf23 */
void rx_activity_update_drivers(rx_addr act, float dt);                         /* 00559d30 */
void rx_activity_update_sequences(rx_addr act, float dt);                       /* 0055e260 */
void rx_activity_state_update(rx_addr act, float dt);                           /* 00558930 Activity::vf24 */
void rx_activity_update(rx_addr act, float dt);                                 /* 00559c10 Activity::vf23 */
void rx_activity_in_progress(rx_addr act, float dt);                            /* 005594a0 Activity::vf28 */
void rx_race_stub_in_progress(rx_addr act, float dt);                           /* 00568a20 MXActivityRaceStub::vf28 */
rx_addr rx_activity_state_name(rx_addr map, rx_addr key_ptr);                   /* 00585860 */
void rx_activity_set_state(rx_addr act, uint32_t state);                        /* 005586c0 */
void rx_activity_enter_state(rx_addr act, uint32_t state, uint32_t old);        /* 00558880 Activity::vf36 */
void rx_activity_pre(rx_addr act, float dt);                                    /* 00558ce0 Activity::vf25 */
void rx_race_stub_check_finish(rx_addr act);                                     /* 00566ee0 MXActivityRaceStub::vf42 */
rx_addr rx_activity_human_driver(rx_addr act, int32_t n);                        /* 00559e60 */
rx_addr rx_activity_driver_of_kind(rx_addr act, uint32_t kind, int32_t n);      /* 0055a000 */
rx_addr rx_activity_player_driver(rx_addr player, rx_addr act);                 /* 00559fd0 */
void rx_activity_nop(void);                                                     /* 0057b2f0 */
int32_t rx_race_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);       /* 00565570 MXActivityRace::vf43 */
void rx_activity_manager_update(rx_addr mgr, float dt);                         /* 0055f700 */
void rx_players_deferred(rx_addr players);                                      /* 00552930 */
void rx_activity_presence(rx_addr act);                                         /* 0055cf10 */
rx_addr rx_map_iterator_value(rx_addr it);                                     /* 00585c60 */
void rx_event_proxy_call3(rx_addr proxy, rx_addr payload);                      /* 0058f270 */
uint32_t rx_net_pending_players(void);                                          /* 00555680 */
uint32_t rx_scenario_done(rx_addr sc);                                          /* 0055eb00 BaseScenario::vf7 */
uint32_t rx_activity_can_start(rx_addr act);                                    /* 0055e710 Activity::vf11 */
void rx_race_countdown(rx_addr act, float dt);                                  /* 005651b0 MXActivityRace::vf26 */
void rx_free_ride_jumps(rx_addr tr, rx_addr act, rx_addr player, float dt);     /* 005851f0 */
void rx_free_ride_in_progress(rx_addr act, float dt);                           /* 005609d0 MXActivityFreeRide::vf28 */
void rx_free_ride_pre(rx_addr act, float dt);                                   /* 005604f0 MXActivityFreeRide::vf25 */
void rx_free_ride_check_finish(rx_addr act);                                    /* 00560b70 MXActivityFreeRide::vf42 */
void rx_activity_enable_controls(uint32_t enable, rx_addr act);                 /* 0055a1e0 */
rx_addr rx_name_map_lower_bound(rx_addr map, rx_addr key);                      /* 00502870 */
uint32_t rx_race_stub_respawn_point(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir);   /* 00569010 vf19 */
uint32_t rx_free_ride_respawn_point(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up); /* 00561650 vf19 */
void rx_activity_place_object(rx_addr act, rx_addr rec);                        /* 0055d9d0 */
void rx_activity_place_object2(rx_addr act, rx_addr rec);                       /* 0055d870 */
uint32_t rx_activity_start_sequence(rx_addr act, rx_addr name, uint32_t arg);   /* 0055e100 */
void rx_activity_vehicle_fx_reset(rx_addr act);                                 /* 00569730 */
void rx_event_proxy_call4(rx_addr proxy, rx_addr payload);                      /* 0058fde0 */
void rx_event_proxy_call5(rx_addr proxy, rx_addr payload);                      /* 0058e910 */
uint32_t rx_race_scripts_enabled(rx_addr act);                                  /* 00569d10 */
void rx_race_start_script(rx_addr act, uint32_t which);                         /* 00569e10 MXActivityRaceStub::vf53 */
void rx_race_holeshot(rx_addr player, rx_addr act);                             /* 00569eb0 */
void rx_race_trigger(rx_addr act, rx_addr veh, rx_addr trig);                   /* 00569f40 MXActivityRaceStub::vf59 */
void rx_activity_vehicle_entered(rx_addr act, rx_addr veh, rx_addr trig);       /* 0055e8b0 "VehicleEntered" handler */
void rx_activity_countdown(rx_addr act, float dt);                              /* 00559190 Activity::vf26 */
void rx_activity_exit_state(rx_addr act, uint32_t old, uint32_t state);         /* 00558800 Activity::vf30 */
void rx_activity_in_progress_exit(rx_addr act, uint32_t old);                   /* 005595b0 Activity::vf33 */
void rx_activity_players_racing(rx_addr act);                                   /* 0055a410 */
void rx_activity_countdown_enter(rx_addr act);                                  /* 00558f70 Activity::vf38 */
void rx_activity_in_progress_enter(rx_addr act);                                /* 005593e0 Activity::vf39 */
void rx_activity_countdown_exit(rx_addr act);                                   /* 00559020 Activity::vf32 */
void rx_activity_finish_sequences(rx_addr act);                                 /* 0055e2b0 */
void rx_player_camera_flash(rx_addr p);                                         /* 00550350 */
void rx_cameras_fade(rx_addr color);                                            /* 0055ea60 */
void rx_local_vehicles_vf49(void);                                              /* 0055d240 */
uint32_t rx_activity_count_humans_of(rx_addr act, uint32_t id);                 /* 00559e20 */
uint32_t rx_activity_count_kind(rx_addr act, uint32_t kind);                    /* 00559df0 */
rx_addr rx_ui_settings(void);                                                   /* 005544b0 */
rx_addr  rx_vector_alloc(uint32_t count);                                       /* 004fead0 (RET 4) */
void     rx_vector_free(rx_addr storage);                                      /* 004f3320 (RET 4) */
rx_addr  rx_vector_copy_backward(rx_addr last, rx_addr first, rx_addr dest_end);  /* 004faae0 (EAX, EDI, ECX) */
uint32_t rx_ui_settings_value(rx_addr settings, uint32_t index, uint32_t device);   /* 005b9f50 UISettings_Get (ECX, EAX, ESI) */
uint32_t rx_ui_settings_get(rx_addr settings, uint32_t index, uint32_t device);     /* 00554d30: the same (a jump) */
uint32_t rx_player_has_ghost(rx_addr p);                                        /* 00551070 */
rx_addr rx_alloc_0x3c(void);                                                    /* 005060e0 */
rx_addr rx_alloc_0x20(void);                                                    /* 00506130 */
void rx_tree_next_39(rx_addr it);                                               /* 00503440 */
void rx_tree_next_1d(rx_addr it);                                               /* 00503750 */
void rx_free_ride_pre_exit(rx_addr act);                                        /* 00560290 MXActivityFreeRide::vf31 */
void rx_free_ride_countdown(rx_addr act, float dt);                             /* 005609b0 MXActivityFreeRide::vf26 */
void rx_activity_countdown_exit_thunk(rx_addr act);                             /* 0057a4a0 */
void rx_player_respawn(rx_addr p);                                              /* 00550650 */
void rx_player_respawn_trigger(rx_addr p);                                      /* 00550cc0 (ESI = player): the same, +0x1170 = 9 */
rx_addr rx_map_node_int_string(rx_addr val, uint32_t l, uint32_t par, uint32_t r);   /* 00504ee0 */
rx_addr rx_map_node_16(rx_addr val, uint32_t l, uint32_t par, uint32_t r);      /* 005053b0 */
void rx_tree_insert_int_string(rx_addr where, rx_addr tree, rx_addr out, uint32_t left, rx_addr val);   /* 00502e30 */
void rx_tree_insert_16(rx_addr where, rx_addr tree, rx_addr out, uint32_t left, rx_addr val);           /* 005035c0 */
void rx_tree_rotate_left_int_string(rx_addr x, rx_addr tree);   /* 00500480 (ECX = x; stack tree; RET 4) */
void rx_tree_rotate_right_int_string(rx_addr x, rx_addr tree);  /* 004ffe80 */
void rx_tree_rotate_left_16(rx_addr x, rx_addr tree);           /* 00765c90 */
void rx_tree_rotate_right_16(rx_addr x, rx_addr tree);          /* 00765ce0 */
void rx_vector_insert_one(rx_addr vec, rx_addr valp, rx_addr where);            /* 00501a30 */
void rx_activity_setup_object(rx_addr name, rx_addr act, rx_addr obj);         /* 0055e360 */
void rx_activity_make_locator(rx_addr act, rx_addr rec);                        /* 0055dac0 */
void rx_activity_show_object(rx_addr act, rx_addr rec);                         /* 0055dff0 */
void rx_activity_place_at_locator(rx_addr act, rx_addr rec);                    /* 0055de70 */
void rx_activity_place_entity(rx_addr act, rx_addr rec);                        /* 0055d4e0 */
void rx_race_stub_in_progress_enter(rx_addr act, uint32_t old);                 /* 00568620 MXActivityRaceStub::vf39 */
/* race_activity_audio.c: ActivityAudioLogic */
uint32_t rx_audio_cue_count(rx_addr map, rx_addr key);                          /* 00585ad0 */
void rx_audio_play_cue(rx_addr logic, rx_addr cue);                             /* 005812f0 */
void rx_race_audio_end_of_race(rx_addr l, uint32_t act, uint32_t driver);       /* 00582da0 (w-sfx) */
void rx_race_audio_restart(rx_addr l, uint32_t act);                         /* 00582d70 */
void rx_race_audio_two_laps(rx_addr l, uint32_t act, uint32_t driver);          /* 00582dc0 */
void rx_race_audio_one_lap(rx_addr l, uint32_t act, uint32_t driver);           /* 00582de0 */
void rx_race_audio_holeshot(rx_addr l, uint32_t act, uint32_t driver);          /* 00582e40 */
void rx_race_audio_best_lap(rx_addr l, uint32_t driver, float time);            /* 00582e00 */
void rx_race_audio_lapped(rx_addr l, uint32_t act, uint32_t vehicle);           /* 00582e20 */
void rx_race_audio_player_wins(rx_addr l, uint32_t act, uint32_t driver, uint32_t c, uint32_t d);   /* 00583fb0 */
void rx_race_audio_eliminated(rx_addr l, uint32_t act, uint32_t driver, uint32_t other);           /* 00583a70 */
void rx_race_audio_timed_section(rx_addr l, uint32_t act, uint32_t driver, int32_t n);             /* 00582e60 */
void rx_audio_end_race_cheer(rx_addr logic);                                    /* 00581760 (ESI = logic) */
void rx_audio_cue(rx_addr logic, rx_addr name, uint32_t a, uint32_t b);         /* 005811a0 */
void rx_free_ride_audio_award_achieved(rx_addr logic, rx_addr player, uint32_t kind, uint32_t medal);   /* 00582430 FreeRideActivityAudioLogic::vf38 */
rx_addr rx_set_int_find(rx_addr set, rx_addr out, rx_addr key);                  /* 00585a40 (EDI = set; RET 8) */
uint32_t rx_activity_has_local_human(rx_addr act);                             /* 00559da0 (ECX = activity) */
uint32_t rx_audio_default_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b);   /* 00580870 */
uint32_t rx_audio_countdown_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr step, rx_addr b); /* 005809d0 */
uint32_t rx_audio_human_modifier(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr driver, rx_addr b); /* 00580a80 */
uint32_t rx_audio_modifier_nop(rx_addr mod, rx_addr name, rx_addr cue, rx_addr act, rx_addr a, rx_addr b);       /* 00555de0 */
void rx_audio_place_cues(rx_addr logic);                                        /* 00581570 */
void rx_audio_update(rx_addr logic);                                            /* 00581010 vf31 */
void rx_audio_driver_cheer(rx_addr logic, rx_addr driver);                      /* 005818c0 vf6 */
void rx_audio_stunt_end(rx_addr logic, rx_addr driver);                         /* 005819f0 vf9 */
void rx_audio_countdown_go(void);                                               /* 005816e0 */
void rx_audio_countdown_step(rx_addr logic, rx_addr act, uint32_t step);                     /* 00582150 vf27 */
void rx_audio_in_progress(rx_addr logic, rx_addr act);                          /* 00582270 */
void rx_audio_free_ride_in_progress(rx_addr logic, rx_addr act);                /* 005822e0 */
void rx_audio_pre_exited(rx_addr logic);                                        /* 00581f90 vf16 */
void rx_audio_countdown_entered(rx_addr logic);                                 /* 00581fd0 vf17 */
void rx_audio_countdown_exited(rx_addr logic);                                  /* 00581ff0 vf18 */
void rx_audio_in_progress_exited(rx_addr logic, uint32_t act, uint32_t state);  /* 00583130 vf20 */
void rx_audio_wind_down_entered(rx_addr logic, uint32_t a, uint32_t b);         /* 00583170 vf21 */
void rx_audio_wind_down_exited(rx_addr logic, uint32_t a, uint32_t b);          /* 00582070 vf22 */
void rx_audio_post_entered(rx_addr logic, uint32_t a, uint32_t b);              /* 005848a0 vf23 */
void rx_audio_post_exited(rx_addr logic, uint32_t a, uint32_t b);               /* 005820d0 vf24 */
void rx_audio_crash(rx_addr logic, rx_addr act, rx_addr driver);                /* 00581bc0 vf12 */
rx_addr rx_audio_name(void);                                                    /* 00555df0 vf34 */
#ifdef __cplusplus
}
#endif
