/* race.h -- MX vs ATV Reflex race layer: activities (Activity / MXActivityRace / MXActivityRaceStub / MXActivityFreeRide),
 * drivers (BaseDriver / RaceDriver / NetworkDriver / FreeRideDriver), race rules (Rule*), the AI behaviours (Behavior*), the AI
 * vehicle controllers of RavenVehicleMotocross and the game frame (MXGame::Update) (core/game/race_*.c).
 * Layouts, conventions and what is known about the AI: core/notes/race.md. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* race_rules.c -- the race rules each driver runs per frame (driver +0xa0[i], count +0xee4) */
int  rx_race_line_crossed(rx_addr line, rx_addr pos, rx_addr prev);              /* 0051a810 */
void rx_rule_finish_line_update(rx_addr rule, float dt);                          /* 0051a7a0 RuleFinishLine::vf5 */
void rx_rule_track_advance_update(rx_addr rule, float dt);                        /* 0051d400 RuleTrackAdvance::vf5 */
int  rx_player_is_resetting(rx_addr player);                                      /* 005501d0 */
void rx_rule_backwards_update(rx_addr rule, float dt);                            /* 00519990 RuleBackwards::vf5 */
float rx_rule_backwards_distance(rx_addr query);                                  /* 00519ce0 */
void rx_rule_backwards_reset(rx_addr rule);                                       /* 00519cb0 */
void rx_rule_track_advance_section_changed(rx_addr rule);                         /* 0051d890 */
rx_addr rx_driver_track_advance_rule(rx_addr driver);                             /* 0053b670 (2) */
rx_addr rx_driver_rule_7(rx_addr driver);                                         /* 00554c90 (7) */
void rx_rule_offtrack_update(rx_addr rule, float dt);                             /* 0051b130 RuleOfftrackTimePenalty::vf5 */
float rx_track_distance_between(rx_addr a, rx_addr b, rx_addr pos_b, rx_addr section, rx_addr pos_a);   /* 0051b690 */
float rx_offtrack_shortcut_penalty(rx_addr rule, rx_addr pos_b, rx_addr a, rx_addr b, rx_addr pos_a, rx_addr section); /* 0051b890 */
int rx_offtrack_count_markers(rx_addr rule, rx_addr from, rx_addr pos, rx_addr section, rx_addr to);    /* 0051b9e0 */
void rx_offtrack_reset(rx_addr rule);                                             /* 0051b080 */
void rx_offtrack_left_track(rx_addr rule);                                        /* 0051b8f0 */
void rx_rule_offtrack_reset_state(rx_addr rule);                                  /* 0051b010 */
int  rx_player_is_local_human(rx_addr player);                                    /* 00552870 */
int  rx_cannonball_inside(rx_addr pos, rx_addr out_point);                        /* 00519fa0 */
void rx_rule_cannonball_update(rx_addr rule, float dt);                           /* 0051a300 RuleCannonball::vf5 */
uint32_t rx_rule_finish_line_type(rx_addr self);                                  /* 0051a790 (shared vf3: 4) */
uint32_t rx_behavior_brake_turn_type(rx_addr self);                               /* 0051b0e0 BehaviorBrakeTurn::vf5 (0xb) */
/* a driver's rule / behaviour by type id (0 if none) */
rx_addr rx_driver_rule_0(rx_addr driver);                                         /* 0053ae50 */
rx_addr rx_driver_rule_1(rx_addr driver);                                         /* 0053ae00 */
rx_addr rx_driver_offtrack_penalty_rule(rx_addr driver);                          /* 0053aaa0 (0xb) */
rx_addr rx_driver_behavior_1(rx_addr driver);                                     /* 0053baa0 */
rx_addr rx_driver_navigate_behavior(rx_addr driver);                              /* 0053a310 (2) */
rx_addr rx_driver_roam_behavior(rx_addr driver);                                  /* 0053a560 (0xe) */
rx_addr rx_driver_behavior_3(rx_addr driver);                                     /* 0053ac30 */
rx_addr rx_driver_behavior_5(rx_addr driver);                                     /* 0053abe0 */
rx_addr rx_driver_air_leveling_behavior(rx_addr driver);                          /* 0053a510 (9) */
rx_addr rx_driver_fix_progress_behavior(rx_addr driver);                          /* 0053ab40 (0xc) */
rx_addr rx_driver_lap_pacing_behavior(rx_addr driver);                            /* 0053aaf0 (0x13) */
rx_addr rx_driver_suck_racing_behavior(rx_addr driver);                           /* 0053ab90 (0x17) */

int  rx_cannonball_exit(rx_addr pos, rx_addr out_point);                          /* 0051a110 */
void rx_cannonball_enter(rx_addr rule, uint32_t ebp);                             /* 0051a690 (EDI = rule, EBP the caller's) */
rx_addr rx_fade_layer(rx_addr fade);                                              /* 00519ed0 (ECX = fade) */
float rx_track_advance_distance(rx_addr rule);                                    /* 0051d8c0 (ESI = rule) -> xmm0 */
void rx_player_launch(rx_addr player, rx_addr point);                             /* 0054f150 (ECX = player, EDI = point) */
void rx_player_reset_in_place(rx_addr player);                                    /* 00550920 (ESI = player) */
void rx_player_reset_terrain(rx_addr player);                                     /* 00550b70 (ESI = player) */
void rx_player_finish_reset(rx_addr player);                                      /* 0054e280 (ESI = player) */

/* race_driver_ai.c -- the AI rider's control outputs */
void rx_ai_control_smooth(rx_addr driver, rx_addr controls, float dt);            /* 0050b330 (ECX = driver, EDX = controls, xmm2 = dt) */
void rx_lap_time_text(rx_addr out, float seconds, uint32_t with_hundredths);      /* 00790100 (EAX = out, xmm0, EDI) */
void rx_ai_debug_overlay(rx_addr driver);                                         /* 00509940 (ESI = driver) */

/* race_net.c -- the network player (NetworkDriver) */
uint32_t rx_net_ref_valid(rx_addr obj);                                           /* 00884950 (ECX = ref) -> EAX (AL) */
rx_addr rx_net_current_station(rx_addr out);                                      /* 0089c8a0 (cdecl out) -> out */
void rx_net_stamp(rx_addr net);                                                   /* 005088c0 (ECX = net) */
uint32_t rx_net_snapshot_bind(rx_addr player, rx_addr slot);                      /* 00508b90 (EAX = player, stack slot; RET 4) */
uint32_t rx_net_bind_player(rx_addr net, rx_addr player);                         /* 0050bc70 (EAX = net, stack player; RET 4) */
rx_addr rx_net_snapshot_init(rx_addr s);                                          /* 00700310 (ESI = snapshot) -> EAX */
void rx_net_snapshot_controls(rx_addr s, rx_addr net);                            /* 006fed10 (EAX = snapshot, ECX = net) */
uint32_t rx_net_player_snapshot(rx_addr net, rx_addr s);                          /* 006feb00 (EAX = net, ECX = snapshot) -> EAX */
void rx_net_pose_copy(rx_addr src, rx_addr dst);                                  /* 00431cf0 (ECX = src, stack dst; RET 4) */
void rx_vehicle_body_link_clear(rx_addr v);                                       /* 0045d4d0 (EAX = vehicle) */
void rx_vehicle_net_controls_reset(rx_addr v);                                    /* 005084d0 (ECX = vehicle) */
void rx_pad_net_hold(rx_addr pad);                                                /* 0061c6b0 (EAX = pad block) */
void rx_player_net_pause_flag(rx_addr p);                                         /* 0054e110 (ESI = player) */
void rx_net_player_reset(rx_addr driver, rx_addr snapshot);                       /* 0050e7e0 (EDI = driver, stack snapshot; RET 4) */
uint32_t rx_net_player_update(rx_addr net, float dt);                             /* 006fe210 (stdcall net, dt; RET 8) -> EAX */
void rx_game_debug_camera_toggle(rx_addr player);                                 /* 00757f40 (stdcall player; RET 4) */

/* race_sound.c -- the rules' sound calls */
void rx_sound_stop_now(rx_addr ev);                                               /* 0080ae10 (ESI = event) */
void rx_sound_stop(rx_addr ev, uint32_t ebp);                                     /* 0080ade0 (EAX = event) */
void rx_sound_set_volume(rx_addr ev, float v, uint32_t ebp);
void rx_audio_event_apply_volume(rx_addr ev);                                      /* 0080b490 AudioEvent_ApplyVolume (EAX = ESI = event) */
void rx_audio_event_invalid_handle(rx_addr ev);                                    /* 0080a8e0 (ESI = event) */
void rx_audio_event_reset_flags(rx_addr ev);                                       /* 0080bde0 (cdecl event) */
void rx_audio_event_unlist_entry(rx_addr event_slot);                              /* 0080dcd0 (ECX = &event) */
uint32_t rx_fmod_event_stop(rx_addr fmod_event, uint32_t immediate);               /* 00900378 FMOD::Event::stop (the audio DLL boundary) */
uint32_t rx_fmod_event_set_volume(rx_addr fmod_event, float volume);               /* 00900342 FMOD::Event::setVolume */
uint32_t rx_fmod_event_set_callback(rx_addr fmod_event, uint32_t callback, uint32_t user);   /* 00900366 FMOD::Event::setCallback */                      /* 0080b410 (ESI = event, xmm0) */
void rx_channel_set_volume(rx_addr ch, float v);                                  /* 00807130 (EAX = channel, xmm0) */
void rx_channel_fade(rx_addr ch, float time);                                     /* 00807170 (ECX = channel; RET 4) */
void rx_sound_world_name(rx_addr out);                                            /* 00581350 */
void rx_sound_world_path(rx_addr out);                                            /* 00581490 */

/* race_math.c -- geometry helpers */
float rx_closest_point_on_segment(rx_addr out, rx_addr p, rx_addr a, rx_addr b);                             /* 00407430 (ECX out, ESI p, EAX a, cdecl b) -> xmm0 */
void rx_race_to_vehicle_frame(rx_addr vehicle, rx_addr v);                                                     /* 005084f0 */
rx_addr rx_race_pose_reset(rx_addr rec);                                                                      /* 005089d0 */
int rx_race_point_near_segment(rx_addr p, rx_addr a, rx_addr b, float radius, rx_addr out);                    /* 0050b850 */
int rx_race_segments_cross_xz(rx_addr p0, rx_addr p1, rx_addr q0, rx_addr q1, rx_addr out);                   /* 0050b990 */
void rx_track_look_ahead(rx_addr node, rx_addr out_pos, rx_addr out_dir, float distance);                     /* 0050a650 */
void race_look_ahead(rx_addr node, float distance, float pos[3], float dir[3]);                                /* (0050a650 in C) */
void rx_track_look_ahead_xz(rx_addr node, rx_addr out_pos, float distance, rx_addr out_dir);                  /* 0050a5f0 */
uint32_t race_rand(void);                                                                                     /* the inlined LCG */
rx_addr rx_race_choose_section(rx_addr driver, rx_addr list, uint32_t count, uint32_t prefer, rx_addr* slot);  /* 0050a360 */
int rx_race_on_track_ahead(rx_addr driver, rx_addr node);                                                     /* 0050aa70 */
float rx_race_heading_factor(rx_addr a, rx_addr b, rx_addr dir);                                             /* 0050b150 */
void rx_race_limit_throttle_in_turns(rx_addr driver, rx_addr controls);                                       /* 0050b220 */
float rx_race_pow_difference(float x, float a, float b, float c, float base);                                 /* 0050b0f0 */
int rx_race_rider_near(rx_addr driver, int side, float distance, float width);                                /* 0050a8b0 */
void rx_race_probe_rays(rx_addr driver, rx_addr centre, rx_addr left, rx_addr right);                         /* 00509fc0 */
int rx_race_parts_loading(rx_addr parts);                                                                     /* 0053ffb0 */
float rx_race_track_lateral(rx_addr driver, rx_addr pos);                                                     /* 00509e30 */
void rx_race_flat_direction(rx_addr v, rx_addr out);                                                          /* 0050a570 */
int rx_race_lean_recover(rx_addr driver, float dt, rx_addr orig_frame);                                       /* 0050ae20 */

/* race_loader.c -- per-frame vehicle part loading */
int rx_loader_parts_ready(rx_addr parts);                                                                     /* 0053cb80 */
void rx_loader_update(rx_addr loader, rx_addr owner, float dt);                                               /* 0053cc90 */

/* race_ai_params.c -- AI tuning parameters by personality and difficulty */
float race_ai_param(rx_addr driver, rx_addr block, uint32_t* idx, int* expl);
float rx_ai_param_dd20(rx_addr driver);                                                                       /* 0051dd20 */
float rx_ai_param_de00(rx_addr driver);                                                                       /* 0051de00 */
float rx_ai_param_dea0(rx_addr driver);                                                                       /* 0051dea0 */
float rx_ai_param_df40(rx_addr driver);                                                                       /* 0051df40 */
float rx_ai_param_e090(rx_addr driver);                                                                       /* 0051e090 */
float rx_ai_param_e0e0(rx_addr driver);                                                                       /* 0051e0e0 */
float rx_ai_param_e130(rx_addr driver);                                                                       /* 0051e130 */
float rx_ai_param_e180(rx_addr driver);                                                                       /* 0051e180 */
float rx_ai_param_de50(rx_addr driver);                                                                       /* 0051de50 */
float rx_ai_param_def0(rx_addr driver);                                                                       /* 0051def0 */
void rx_ai_param_range_dd70(rx_addr driver, rx_addr out);                                                     /* 0051dd70 */
void rx_ai_controls_off(rx_addr controls);                                                                    /* 0050ac80 */
void rx_ai_controls_on(rx_addr controls);                                                                     /* 0050acc0 */

/* race_player.c -- MXPlayer reset helpers */
void rx_player_reset_fade(rx_addr player);                                                                    /* 00550280 */
void rx_player_reset_to(rx_addr player, rx_addr pos, rx_addr orient);                                         /* 005501f0 */
void rx_player_reset_to_track(rx_addr player);                                                                /* 00550800 */
void rx_player_reset_offtrack(rx_addr player);                                                                /* 005507a0 */

/* race_event.c -- typed events (manager = the activity's event manager, activity +0x290; name = a C string in .rdata) */
void race_raise_event(rx_addr manager, rx_addr name, rx_addr type_info, const uint32_t* payload, int words);  /* (inlined raisers) */
void rx_event_driver_int(rx_addr manager, rx_addr name, rx_addr driver, uint32_t value);                       /* 0053a470 */
void rx_event_activity_driver(rx_addr manager, rx_addr name, rx_addr activity, rx_addr driver);               /* 0053a6d0 */
void rx_event_activity_int(rx_addr manager, rx_addr name, rx_addr activity, uint32_t value);                  /* 005875e0 */
void rx_event_player_wins(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr text, uint32_t value);    /* 00587d00 */
void rx_event_player_eliminated(rx_addr manager, rx_addr activity, rx_addr driver, rx_addr next);               /* 0053ac80 (ESI = manager, ECX = driver, EDX = next; stack activity; RET 4) */
void rx_event_driver_3int(rx_addr manager, rx_addr name, rx_addr driver, uint32_t a, uint32_t b, uint32_t c); /* 0053a9e0 */
void rx_event_driver(rx_addr manager, rx_addr name, rx_addr driver);                                          /* 0053a770 */
void rx_event_best_lap(rx_addr manager, rx_addr driver, float t);                                              /* 0053aa40 */
void rx_event_vehicle_landed(rx_addr manager, rx_addr player, rx_addr record);                                 /* 00554c40 */
void rx_event_player_2int(rx_addr manager, rx_addr name, rx_addr player, uint32_t a, uint32_t b);             /* 0058d7d0 */

/* race_track.c -- the track's node graph */
typedef struct {                       /* the query of 005387c0 (in memory: +0x04 .. +0x20; +0x00 = the result pointer) */
    rx_addr pos, section, node, stop;
    float ahead, behind;
    uint32_t use_stop, follow;
} race_track_query;
int race_track_find(const race_track_query* q, int second_edges, rx_addr* out_section, rx_addr* out_node);
int rx_track_segment_contains(int second_edges, rx_addr a, rx_addr b, rx_addr pos, float length, int from_end);   /* 00538e00 */
int rx_track_find_segment(rx_addr query, int second_edges);                                                   /* 005387c0 */
void rx_track_nearest_node(rx_addr start, rx_addr pos, rx_addr out, float* budget);                           /* 005381f0 */
void rx_track_nearest_node_all(rx_addr track, rx_addr dir, int second_edges, rx_addr pos, rx_addr out);       /* 00538440 */
void rx_track_project_clamped_xz(rx_addr a, rx_addr b, rx_addr p, rx_addr out);                              /* 005389c0 */
float rx_track_project_xz(rx_addr a, rx_addr b, rx_addr p, rx_addr out);                                      /* 00538b30 */

#ifdef __cplusplus
}
#endif
