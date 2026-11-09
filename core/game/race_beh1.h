/* race_beh1.h -- the AI behaviours, part 1 (Behavior* classes 0x0051f000..0x00529fff): AirLeveling, AvoidFlipBack,
 * AvoidTrackBoundaries, AvoidWreck, BlueYellowFlag, BrakeTurn, FixForwardProgress, HandleBraking, LapPacing, MonitorCorner,
 * NavigateTrack, Offtrack, OverrideSpline (core/game/race_beh1_*.c). A behaviour: +0x00 vtable, +0x04 enabled, +0x08 driver.
 * The driver runs its behaviours in priority order (driver +0xe8[i], count +0xee0) through vf4(dt); the first that returns nonzero
 * takes control (driver +0x2ec = its index). Layouts and what each behaviour does: core/notes/race.md "AI behaviours". */
#pragma once
#include "rx_game.h"
#include "../src/rx_x87.h"
#ifdef __cplusplus
extern "C" {
#endif

/* race_beh1_brake.c -- BehaviorHandleBraking */
uint32_t rx_behavior_handle_braking_type(rx_addr self);                                /* 005248f0 vf5 (0x11) */
void  rx_behavior_handle_braking_reset(rx_addr b);                                /* 005248d0 vf6 */
float rx_behavior_braking_cast(int side, rx_addr b);                              /* 00524ce0 */
rx_f80 rx_behavior_braking_mean(rx_addr b);                                       /* 00524bb0 (st0) */
void  rx_behavior_braking_apply(float f, rx_addr b);                              /* 00524c50 */
void  rx_behavior_braking_cast_step(rx_addr b);                                   /* 005249a0 */
void  rx_behavior_braking_cast_center(rx_addr b);                                 /* 00524a40 (mode 1) */
void  rx_behavior_braking_cast_past(rx_addr b);                                   /* 00524af0 (mode 2) */
uint32_t rx_behavior_handle_braking_update(rx_addr b, float dt);                  /* 00524900 vf4 */


/* race_beh1_small.c -- AvoidWreck, Offtrack, BrakeTurn, BlueYellowFlag, MonitorCorner, NavigateTrack / OverrideSpline dispatch */
uint32_t rx_behavior_avoid_wreck_type(rx_addr self);                              /* 005223b0 vf5 (0x1a) */
void rx_behavior_avoid_wreck_reset(rx_addr b);                                    /* 00522390 vf6 */
uint32_t rx_behavior_avoid_wreck_update(rx_addr b, float dt);                     /* 005223e0 vf4 */
uint32_t rx_behavior_offtrack_type(rx_addr self);                                 /* 00529140 vf5 (7) */
void rx_behavior_offtrack_watch(rx_addr b);                                       /* 00529150 */
void rx_behavior_offtrack_aim(rx_addr b);                                         /* 00529190 (state 1) */
void rx_behavior_offtrack_return(rx_addr b);                                      /* 00529330 (states 2..4, stack arg; RET 4) */
uint32_t rx_behavior_offtrack_update(rx_addr b, float dt);                        /* 005298b0 vf4 */
uint32_t rx_behavior_brake_turn_update(rx_addr b, float dt);                      /* 00522ef0 vf4 */
void rx_behavior_blue_flag_find(rx_addr b);                                       /* 00522640 */
uint32_t rx_behavior_blue_flag_update(rx_addr b, float dt);                       /* 005225c0 vf4 */
void rx_behavior_blue_flag_move(rx_addr b);                                       /* 00522a70 (state 1) */
rx_addr rx_behavior_corner_find(rx_addr node, float dist, rx_addr b);             /* 00526b40 */
uint32_t rx_behavior_corner_last(rx_addr node, rx_addr section, rx_addr b);      /* 00526b90 */
float    rx_behavior_corner_length(rx_addr node, rx_addr section, rx_addr b);    /* 00526bf0 */
uint32_t rx_behavior_corner_direction(rx_addr node, uint32_t cls);               /* 005380f0 */
uint32_t rx_behavior_node_side(rx_addr obj, int32_t side);                        /* 00526dd0 (EAX = object, stack side; RET 4) */
void rx_behavior_monitor_corner_watch(rx_addr b);                                 /* 00526a30 */
void rx_behavior_monitor_corner_wait(rx_addr b);                                  /* 00526ab0 (state 1) */
void rx_behavior_monitor_corner_hold(rx_addr b);                                  /* 00526b00 (state 2) */
uint32_t rx_behavior_monitor_corner_update(rx_addr b, float dt);                  /* 00526970 vf4 */
uint32_t rx_behavior_navigate_track_update(rx_addr b, float dt);                  /* 00527300 vf4 */
uint32_t rx_behavior_override_spline_update(rx_addr b, float dt);                 /* 00529bf0 vf4 */
void rx_behavior_override_spline_reset(rx_addr b);                                /* 00529ba0 vf6 */

/* race_beh1_air.c -- BehaviorAirLeveling, BehaviorAvoidFlipBack */
void rx_gate_rule_target(rx_addr out, rx_addr rule);                              /* 0050ed70 (EAX = out, ECX = rule) */
void rx_air_prediction_copy(rx_addr normal_out, rx_addr point_out, rx_addr rec, rx_addr time_out);   /* 0051fea0 */
void rx_air_prediction_update(rx_addr rec, float dt);                             /* 0051fe00 */
void rx_air_prediction_schedule(void);                                            /* 0051ff80 */
uint32_t rx_behavior_air_leveling_update(rx_addr b, float dt);                    /* 0051f480 vf4 */
uint32_t rx_behavior_avoid_flip_back_update(rx_addr b, float dt);                 /* 00520050 vf4 */

/* race_beh1_bounds.c -- BehaviorAvoidTrackBoundaries */
float rx_behavior_boundary_push(rx_addr b, rx_addr ea, rx_addr eb);               /* 00521b40 */
float rx_behavior_boundary_push_mirrored(rx_addr b, rx_addr ea, rx_addr eb);      /* 00521ee0 */
uint32_t rx_behavior_avoid_boundaries_update(rx_addr b, float dt);                /* 00522280 vf4 */

/* race_beh1_progress.c -- BehaviorFixForwardProgress */
uint32_t rx_behavior_fix_progress_update(rx_addr b, float dt);                    /* 00523570 vf4 */
uint32_t rx_behavior_fix_progress_race_type(rx_addr b);                           /* 00524050 (EAX = behaviour) */
uint32_t rx_behavior_fix_progress_steer_away(rx_addr b, rx_addr pos, uint32_t side);   /* 00523a70 (ECX = position, EDX = side, stack b) */
uint32_t rx_behavior_fix_progress_back_up(rx_addr b, rx_addr pos, uint32_t side);      /* 00523ca0 (EAX = position, ESI = b, stack side) */
uint32_t rx_behavior_fix_progress_side(rx_addr b);                                /* 00523ed0 (ESI = b) */
uint32_t rx_behavior_fix_progress_near(rx_addr b);                                /* 005240d0 (ESI = b) */

/* race_beh1_pace.c -- BehaviorLapPacing */
void rx_behavior_lap_pacing_reset(rx_addr b);                                     /* 005254e0 vf6 */
void rx_behavior_lap_pacing_new_lap(rx_addr b);                                   /* 00525790 */
void rx_behavior_lap_pacing_classify(rx_addr b);                                  /* 00525ac0 */
uint32_t rx_behavior_lap_pacing_suspended(rx_addr b);                             /* 00525930 */
void rx_behavior_lap_pacing_speed_up(rx_addr b);                                  /* 00525b30 */
void rx_behavior_lap_pacing_gap(rx_addr b, float gap);                            /* 005256d0 (EAX = behaviour, xmm1 = gap) */
void rx_behavior_lap_pacing_start(rx_addr b, float fraction);                     /* 00525d60 (xmm0 = fraction, stack b; RET 4) */
uint32_t rx_behavior_lap_pacing_update(rx_addr b, float dt);                      /* 005257e0 vf4 */

/* race_beh1_nav.c -- BehaviorNavigateTrack (racing line) / BehaviorOverrideSpline */
void rx_behavior_navigate_pick_side(rx_addr b, uint32_t when_airborne, uint32_t force);   /* 00528d50 */
float rx_behavior_navigate_line_offset(rx_addr b, rx_addr node, int32_t side);    /* 00528940 */
float rx_behavior_navigate_corner_offset(rx_addr b, rx_addr node, int32_t cur, int32_t side, float rnd);  /* 00528bd0 (mode 1) */
float rx_behavior_navigate_apex_offset(rx_addr b, rx_addr node, int32_t cur, int32_t side, float rnd);    /* 00528a70 (mode 2) */
void rx_behavior_navigate_follow(rx_addr b);                                      /* 00527360 */
void rx_behavior_navigate_line(rx_addr b);                                        /* 005278e0 (mode 1, stack arg; RET 4) */
void rx_behavior_navigate_apex(rx_addr b);                                        /* 00527fc0 (mode 2, stack arg; RET 4) */
uint32_t rx_behavior_override_spline_wait(rx_addr b);                             /* 00529c40 */
uint32_t rx_behavior_override_spline_enter(rx_addr b);                            /* 00529ce0 */
uint32_t rx_behavior_override_spline_follow(rx_addr b);                           /* 00529eb0 */

#ifdef __cplusplus
}
#endif
