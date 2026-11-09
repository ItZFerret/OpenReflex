/* race_beh2.h -- the AI behaviours of 0x0052a000-0x00535fff (core/game/race_beh2_*.c): BehaviorSeperate (riding around the other
 * riders), BehaviorTakeLine and BehaviorSplitSection (keeping to a lane of the track), BehaviorSuckRacing (letting the humans catch
 * up), the spline scans of BehaviorOverrideSpline and the shared Behavior::vf7 (enable). Layouts and what each one does:
 * scratchpad notes_beh2.md (merged into core/notes/race.md). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* race_beh2_seperate.c -- BehaviorSeperate (vtable 0x0093f9a0) */
uint32_t rx_behavior_seperate_update(rx_addr beh, float dt);                                   /* 00531940 BehaviorSeperate::vf4 */
uint32_t rx_behavior_seperate_reset(rx_addr beh);                                    /* 005318f0 BehaviorSeperate::vf6 */
void  rx_seperate_pick_target(rx_addr beh);                                          /* 00531a70 */
void  rx_seperate_choose_side(rx_addr beh);                                          /* 00531c20 */
int   rx_seperate_target_clear(rx_addr beh);                                         /* 005321f0 */
int   rx_seperate_keep_passing(rx_addr beh);                                         /* 005322c0 */
void  rx_seperate_avoid_riders(rx_addr beh);                                         /* 00532910 */
void  rx_seperate_follow(rx_addr beh);                                               /* 00532dd0 */
void  rx_seperate_check_hold(rx_addr beh);                                           /* 00532fd0 */
float rx_seperate_contact_time(rx_addr beh, rx_addr other, float max_time, float max_move);   /* 00533070 */
float rx_seperate_crossing_time(rx_addr other, rx_addr beh, float max_time);                  /* 005332f0 */
void  rx_vehicle_bound_centre(rx_addr vehicle, rx_addr centre, rx_addr radius);               /* 00533510 */

/* race_beh2_line.c -- BehaviorTakeLine (0x0093f978), BehaviorSplitSection (0x0093fc70) */
uint32_t rx_behavior_take_line_update(rx_addr beh, float dt);                                  /* 005344e0 BehaviorTakeLine::vf4 */
uint32_t rx_behavior_take_line_ready(rx_addr beh);                                   /* 00534470 */
uint32_t rx_behavior_split_section_update(rx_addr beh, float dt);                              /* 00533970 BehaviorSplitSection::vf4 */

/* race_beh2_perform_stunt.c -- BehaviorPerformStunt (the Free Ride AI's stunts) */
uint32_t rx_behavior_perform_stunt_update(rx_addr beh, float dt);                    /* 0052aac0 BehaviorPerformStunt::vf4 (RET 4) */
void  rx_perform_stunt_idle(rx_addr beh);                                            /* 0052ae10 (ESI) state 0 */
void  rx_perform_stunt_plan_ground(rx_addr beh);                                     /* 0052aed0 (EAX) state 1 */
void  rx_perform_stunt_plan_air(rx_addr beh);                                        /* 0052af90 (EAX) state 2 */
void  rx_perform_stunt_wait(rx_addr beh);                                            /* 0052b2f0 (EAX) state 3 */
void  rx_perform_stunt_run(rx_addr beh);                                             /* 0052b390 (ESI) state 4 */
void  rx_perform_stunt_next(rx_addr beh);                                            /* 0052b410 (ESI) state 5 */
void  rx_perform_stunt_takeoff_test(rx_addr beh);                                    /* 0052ab80 (ESI) mode 0 */
float rx_perform_stunt_plan(rx_addr beh, float budget, uint32_t flag);               /* 0052b0a0 (ESI; stack budget, flag; RET 8) -> xmm0 */
float rx_driver_stunt_tuning_a(rx_addr driver);                                      /* 0051dff0 (EAX = driver) -> xmm0 */
float rx_driver_stunt_tuning_b(rx_addr driver);                                      /* 0051e040 (EAX = driver) -> xmm0 */
rx_addr rx_str_find_nocase(rx_addr haystack, rx_addr needle);                        /* 0078fcf0 (EAX = haystack, EDI = needle) */

uint32_t rx_roam_activity_type(rx_addr name);                                         /* 00508920 (stack C string; RET 4), race_beh2_perform_init.c */
uint32_t rx_behavior_roam_world_pick_area(rx_addr self);                              /* 0052ed50 (stack behaviour; RET 4) */

/* race_beh2_roam_world.c -- BehaviorRoamWorld (the Free Ride AI's roaming) */
uint32_t rx_behavior_roam_world_update(rx_addr beh, float dt);                       /* 0052fcf0 BehaviorRoamWorld::vf4 (RET 4) */
uint32_t rx_roam_area_matches(rx_addr beh, rx_addr area);                            /* 0052efa0 (EAX = beh, ESI = area) */
uint32_t rx_roam_class_allowed(uint32_t type, uint32_t klass, uint32_t kind);        /* 00530dc0 (ECX = type, EAX = class; stack kind; RET 4) */
rx_addr rx_roam_nearest_area(rx_addr beh, rx_addr out);                              /* 00530590 (EDI = out; stack beh; RET 4) */
void  rx_roam_choose_area(rx_addr beh, rx_addr out);                                 /* 0052f0e0 (stack beh, out; RET 8) */
uint32_t rx_roam_pick_target(rx_addr beh);                                           /* 0052fca0 (ESI = beh) */
uint32_t rx_roam_target_ok(rx_addr beh);                                             /* 0052f4a0 (ECX = beh) */
uint32_t rx_roam_arrive(rx_addr beh);                                                /* 0052f990 (ESI = beh) */
rx_addr rx_roam_follow(rx_addr beh);                                                 /* 0052f510 (stack beh; RET 4) */
void  rx_roam_arrived(rx_addr beh);                                                  /* 00530740 (stack beh; RET 4) */
float rx_waypoint_line_project(rx_addr end, rx_addr begin, rx_addr point, rx_addr out);   /* 00538a60 (EAX = end, ECX = begin, ESI = point; stack out; RET 0) -> xmm0 = t */

/* race_beh2_misc.c -- BehaviorSuckRacing (0x0093fcc0), BehaviorOverrideSpline's spline scans, Behavior::vf7 */
uint32_t rx_behavior_suck_racing_update(rx_addr beh, float dt);                                /* 00534ff0 BehaviorSuckRacing::vf4 */
float rx_driver_run_distance(rx_addr beh, uint32_t* edx_out);                                  /* 00526c60 (EDI = behaviour) -> xmm0 */
void rx_suck_racing_mode1(rx_addr beh);                                                        /* 005352c0 (ESI = behaviour) */
void rx_suck_racing_mode2(rx_addr beh);                                                        /* 00535380 (ESI = behaviour) */
rx_addr rx_driver_behavior_12(rx_addr driver);                                                 /* 0053b960 (build_activity_driver.c; arg driver; RET 4) */
rx_addr rx_driver_behavior_14(rx_addr driver);                                                 /* 0053b450 (build_activity_driver.c; arg driver; RET 4) */
uint32_t rx_behavior_suck_racing_type(rx_addr self);                                 /* 00534fe0 BehaviorSuckRacing::vf5 (0x17) */
uint32_t rx_behavior_suck_racing_reset(rx_addr beh);                                 /* 00534fa0 BehaviorSuckRacing::vf6 */
void  rx_suck_racing_decide(rx_addr beh);                                            /* 00535090 */
void  rx_suck_racing_hold(rx_addr beh);                                              /* 00535480 */
rx_addr rx_override_spline_scan(rx_addr beh, float range);                           /* 0052a380 */
rx_addr rx_override_spline_scan_start(rx_addr beh);                                  /* 0052a6d0 */
uint32_t rx_behavior_set_enabled(rx_addr self, uint32_t enabled);                    /* 0052dc40 (vf7 of 28 classes) */

#ifdef __cplusplus
}
#endif
