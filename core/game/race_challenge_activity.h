/* race_challenge_activity.h -- the Free Ride challenge activities (race_challenge_activity.c): MXActivityChallenge (vtable 0x009450cc)
 * and its subclasses BigAir (0x15), HillClimb (0x16), Pathfinder (0x18), StepUp (0x19), TrailBlazer (0x1b) and TargetPractice (0x1d),
 * built by the activity factory 0055ef80; the challenge's driver (ChallengeDriver, vtable 0x009400a4), its boundary rule
 * (RuleChallengeBoundary, vtable 0x00940188) and the shared Activity methods only the challenges reach.
 *
 * Virtual functions follow the core's virtual signature (rx_addr this, the stack arguments in order) and return EAX, or a float for
 * an x87 return (vf62: the challenge's value). Float stack arguments are C floats. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- construction (the factory's cases) ---- */
rx_addr  rx_challenge_construct(rx_addr A, uint32_t type, rx_addr world);       /* 0057aa30 (EAX = world, ECX = type; stdcall A; RET 4) -> A */
rx_addr  rx_pathfinder_construct(rx_addr A, uint32_t type, rx_addr world);      /* 0057b740 (ESI = A, EAX = world, ECX = type) -> A */
rx_addr  rx_step_up_construct(rx_addr A, uint32_t type, rx_addr world);         /* 0057c3e0 (ESI = A, EAX = world, ECX = type) -> A */
void     rx_trail_blazer_listen(rx_addr A);                                      /* 0058a7b0 (stdcall A; RET 4): "VehicleEntered" -> 0057d3f0 */
void     rx_trail_blazer_unlisten(rx_addr A);                                    /* 0058a870 (stdcall A; RET 4) */
rx_addr  rx_challenge_audio_logic_construct(rx_addr l);                          /* 00584e50 (ESI = l) -> l */
void     rx_track_block_construct(rx_addr block);                                /* 00536ca0 (ESI = block) */

/* ---- MXActivityChallenge ---- */
void     rx_challenge_destroy(rx_addr A);                                        /* 0057aab0 (ECX = A) */
rx_addr  rx_challenge_sdtor(rx_addr A, uint32_t flags);                          /* 0057aa90 vf0 */
void     rx_challenge_vf2(rx_addr A, rx_addr arg);                               /* 0057ab00 vf2 (the activity's start) */
void     rx_challenge_vf3(rx_addr A);                                            /* 0057ae60 vf3 (the players leave) */
void     rx_challenge_vf4(rx_addr A);                                            /* 0057abd0 vf4 ("ChallengeAward" event type) */
void     rx_challenge_vf15(rx_addr A);                                           /* 0057afc0 vf15 */
void     rx_challenge_vf16(rx_addr A);                                           /* 0057afe0 vf16 (restart) */
void     rx_challenge_vf18(rx_addr A);                                           /* 0057ae90 vf18 (reset) */
void     rx_challenge_vf23(rx_addr A, float dt);                                 /* 0057af40 vf23 (update) */
void     rx_challenge_vf27(rx_addr A, float dt);                                 /* 0057b030 vf27 (WindDown update) */
void     rx_challenge_vf32(rx_addr A, uint32_t state);                           /* 0057c2f0 vf32 (Countdown exit) */
void     rx_challenge_vf33(rx_addr A, uint32_t state);                           /* 0057af90 vf33 (InProgress exit) */
void     rx_challenge_vf38(rx_addr A, uint32_t state);                           /* 0057af10 vf38 (Countdown enter) */
void     rx_challenge_vf39(rx_addr A, uint32_t state);                           /* 0057af30 vf39 (InProgress enter) */
void     rx_challenge_vf40(rx_addr A, uint32_t state);                           /* 0057b020 vf40 (WindDown enter) */
void     rx_challenge_vf41(rx_addr A, uint32_t state);                           /* 0057b050 vf41 (Post enter: the medals) */
rx_addr  rx_challenge_vf44(rx_addr A, rx_addr player);                           /* 0057aeb0 vf44 (a ChallengeDriver) */
void     rx_challenge_vf47(rx_addr A, rx_addr driver);                           /* 0057aef0 vf47 (the driver's rules) */
void     rx_challenge_vf52(rx_addr A);                                           /* 0057b140 vf52 */
void     rx_challenge_vf61(rx_addr A);                                           /* 0057acc0 vf61 (the start entity's pose) */
void     rx_challenge_update_tail(rx_addr A, float dt);                          /* 0057d750 vf23 of the subclasses */
void     rx_challenge_vf1(rx_addr A, rx_addr ref);                               /* 0057c220 vf1 (Activity::vf1) */
float    rx_challenge_value_zero(rx_addr A);                                     /* 005189f0 vf62 of BigAir / Pathfinder / StepUp (fldz) */
void     rx_challenge_load_track(rx_addr A);                                     /* 0057b1a0 (stdcall A; RET 4) */
uint32_t rx_challenge_start_type(uint32_t kind);                                 /* 0057ac50 (EAX = kind) -> the start entity's type */
float    rx_challenge_medal_value(rx_addr A, uint32_t medal);                    /* 0057b160 (ECX = A, ESI = medal) -> xmm0 */
void     rx_challenge_award_raise(rx_addr channel, rx_addr driver, uint32_t m2, rx_addr A, uint32_t m1, uint32_t m0);   /* 0058a5d0 */
rx_addr  rx_challenge_rule_get(rx_addr driver);                                  /* 0058a570 (ECX = driver): RuleChallengeBoundary */

/* ---- the shared Activity methods ---- */
uint32_t rx_activity_vf19(rx_addr A, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up);   /* 00794f60 Activity::vf19 (0) */
void     rx_activity_vf58(rx_addr A);                                            /* 00559a80 Activity::vf58 ("Post State Complete") */

/* ---- HillClimb / BigAir / TrailBlazer / TargetPractice ---- */
rx_addr  rx_hill_climb_sdtor(rx_addr A, uint32_t flags);                         /* 0057b240 */
void     rx_hill_climb_vf1(rx_addr A, rx_addr ref);                              /* 0057b510 */
void     rx_hill_climb_vf18(rx_addr A);                                          /* 0057b320 */
void     rx_hill_climb_vf28(rx_addr A, float dt);                                /* 0057b270 */
void     rx_hill_climb_vf42(rx_addr A);                                          /* 0057b470 */
float    rx_hill_climb_value(rx_addr A);                                         /* 0055ed00 vf62 (also TrailBlazer's) */
void     rx_hill_climb_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2);      /* 0057b370 */
rx_addr  rx_big_air_sdtor(rx_addr A, uint32_t flags);                            /* 0055eca0 */
void     rx_big_air_vf18(rx_addr A);                                             /* 0057b5e0 */
void     rx_big_air_vf28(rx_addr A, float dt);                                   /* 0057b550 */
void     rx_big_air_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2);         /* 0057b620 */
rx_addr  rx_trail_blazer_sdtor(rx_addr A, uint32_t flags);                       /* 0057d370 */
void     rx_trail_blazer_vf18(rx_addr A);                                        /* 0057d3c0 */
void     rx_trail_blazer_vf28(rx_addr A, float dt);                              /* 0057d3a0 */
void     rx_trail_blazer_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2);    /* 0057d440 */
void     rx_trail_blazer_vehicle_entered(rx_addr A, rx_addr vehicle, rx_addr trigger);   /* 0057d3f0 (thiscall; RET 8) */
rx_addr  rx_target_practice_sdtor(rx_addr A, uint32_t flags);                    /* 0057d500 */
void     rx_target_practice_vf1(rx_addr A, rx_addr ref);                         /* 0057d5b0 */
void     rx_target_practice_vf2(rx_addr A, rx_addr arg);                         /* 0057d530 */
void     rx_target_practice_vf28(rx_addr A, float dt);                           /* 0057d780 */
void     rx_target_practice_vf37(rx_addr A, uint32_t state);                     /* 0057d590 */
void     rx_target_practice_vf42(rx_addr A);                                     /* 0057d540 */
void     rx_target_practice_vf47(rx_addr A, rx_addr driver);                     /* 0057d6a0 */
float    rx_target_practice_value(rx_addr A);                                    /* 0055ed10 vf62 */
void     rx_target_practice_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2); /* 0057d9e0 */
void     rx_target_practice_collect(rx_addr A, rx_addr name);                    /* 0057d6d0 (ESI = A; stdcall group name; RET 4) */
void     rx_target_practice_clear(rx_addr A);                                    /* 0057d7b0 (EAX = A) */
void     rx_target_practice_hits(rx_addr A, rx_addr player);                     /* 0057d880 (EDI = A; stdcall player; RET 4) */
void     rx_target_hit_event(rx_addr channel, uint32_t hits, uint32_t total, rx_addr player, rx_addr pos);   /* 0058a8e0 */

/* ---- ChallengeDriver / RuleChallengeBoundary ---- */
rx_addr  rx_challenge_driver_sdtor(rx_addr d, uint32_t flags);                   /* 005188c0 */
void     rx_challenge_driver_vf1(rx_addr d);                                     /* 00516020 (-> NetworkDriver::vf1) */
void     rx_network_driver_vf2(rx_addr d);                                       /* 005188f0 (-> BaseDriver::vf2) */
void     rx_network_driver_vf16(rx_addr d, uint32_t kind, rx_addr ref);          /* 0050eb50 NetworkDriver::vf16 (RET 8) */
void     rx_challenge_driver_vf3(rx_addr d);                                     /* 00518900 (reset) */
void     rx_challenge_driver_vf4(rx_addr d);                                     /* 0050bee0 (FreeRideDriver::vf4 too) */
void     rx_challenge_driver_vf7(rx_addr d, float dt);                           /* 005189c0 (update) */
void     rx_driver_rules_tick(rx_addr d);                                        /* 00516190 (stdcall driver; RET 4) */
uint32_t rx_challenge_boundary_vf1(rx_addr rule, rx_addr driver);                /* 0051daa0 */
void     rx_challenge_boundary_vf5(rx_addr rule, float dt);                      /* 0051dbe0 */
uint32_t rx_challenge_boundary_outside(rx_addr rule, rx_addr pos);               /* 0051dae0 (EAX = rule, ECX = position) -> off the track */

void     rx_targetpractice_collectable_vf19(rx_addr e, float dt);                                   /* 0040e730 (build_scene_activity.c) */

/* ---- Pathfinder / StepUp (race_challenge_pathfinder.c; no shipped level stages them) ---- */
void     rx_pathfinder_listen(rx_addr channel, rx_addr A);                         /* 0058a640 (EAX = channel; stdcall A; RET 4) */
void     rx_pathfinder_unlisten(rx_addr channel, rx_addr A);                       /* 0058a700 (EAX = channel; stdcall A; RET 4) */
rx_addr  rx_pathfinder_roam_behavior_get(rx_addr d);                               /* 0058a760 (ECX = driver) */
rx_addr  rx_pathfinder_sdtor(rx_addr A, uint32_t flags);                           /* 0057b7f0 */
void     rx_pathfinder_vf5(rx_addr A);                                             /* 0057b830 */
void     rx_pathfinder_vf2(rx_addr A, rx_addr arg);                                /* 0057b8a0 */
void     rx_pathfinder_vf4(rx_addr A);                                             /* 0057b9a0 */
void     rx_pathfinder_vf67(rx_addr A);                                            /* 0057ba10 */
void     rx_pathfinder_vf3(rx_addr A);                                             /* 0057bb60 */
void     rx_pathfinder_vf18(rx_addr A);                                            /* 0057bbe0 */
void     rx_pathfinder_vf16(rx_addr A);                                            /* 0057bc10 */
void     rx_pathfinder_vf23(rx_addr A, float dt);                                  /* 0057bc30 */
void     rx_pathfinder_vf47(rx_addr A, rx_addr d);                                 /* 0057bc50 */
void     rx_capture_vf37(rx_addr A, uint32_t state);                               /* 0057bd30 MXActivityCapture::vf37 */
void     rx_capture_vf39(rx_addr A, uint32_t state);                               /* 005775c0 MXActivityCapture::vf39 */
void     rx_pathfinder_vf38(rx_addr A, uint32_t state);                            /* 0057bda0 */
void     rx_pathfinder_vf25(rx_addr A, float dt);                                  /* 0057be20 */
void     rx_pathfinder_vf28(rx_addr A, float dt);                                  /* 0057be50 */
void     rx_pathfinder_vf41(rx_addr A, uint32_t state);                            /* 0057c050 */
rx_addr  rx_pathfinder_vf44(rx_addr A, rx_addr player);                            /* 0057c1d0 (a PathfinderDriver) */
void     rx_pathfinder_vf20(rx_addr A, rx_addr player);                            /* 0057c230 */
void     rx_pathfinder_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2);        /* 0057c320 */
rx_addr  rx_step_up_sdtor(rx_addr A, uint32_t flags);                              /* 0055ecd0 */
void     rx_step_up_set_level(rx_addr A, uint32_t level);                          /* 0057c640 (EAX = A, ECX = level) */
void     rx_step_up_place(rx_addr A, uint32_t restart);                            /* 0057cbb0 (EDI = A; stdcall restart; RET 4) */
void     rx_step_up_fail(rx_addr A);                                               /* 0057c6d0 (EAX = A) */
void     rx_step_up_clear(rx_addr A);                                              /* 0057c760 (EAX = A) */
uint32_t rx_step_up_past_bar(rx_addr A, rx_addr player);                           /* 0057cdf0 (ESI = A; stdcall player; RET 4) */
uint32_t rx_step_up_between_uprights(rx_addr A, rx_addr pos);                      /* 0057ce90 (EDI = A, ESI = position) */
void     rx_step_up_vf2(rx_addr A, rx_addr arg);                                   /* 0057cd30 */
void     rx_step_up_vf18(rx_addr A);                                               /* 0057cb20 */
void     rx_step_up_vf28(rx_addr A, float dt);                                     /* 0057c810 */
void     rx_step_up_vf64(rx_addr A, rx_addr m0, rx_addr m1, rx_addr m2);           /* 0057cd00 */
void     rx_step_up_vf65(rx_addr A);                                               /* 0057c9d0 */
void     rx_step_up_vf61(rx_addr A);                                               /* 0057cf50 */

/* ---- the challenges' audio logic (ChallengeActivityAudioLogic, vtable 0x009431fc) and the base state cues it keeps
 * (race_activity_audio.c, build_activity_audio.c) ---- */
void     rx_audio_logic_vehicle_wrecked(rx_addr logic, uint32_t act, rx_addr vehicle);              /* 00581ba0 ActivityAudioLogic::vf10 */
void     rx_audio_logic_in_progress_exited(rx_addr logic, uint32_t a, uint32_t b);                  /* 00582030 vf20 */
void     rx_audio_logic_wind_down_entered(rx_addr logic, uint32_t a, uint32_t b);                   /* 00582050 vf21 */
void     rx_audio_logic_post_entered(rx_addr logic, uint32_t a, uint32_t b);                        /* 00582090 vf23 */
void     rx_audio_logic_finish_countdown_step(rx_addr logic, uint32_t act, uint32_t step);          /* 00582180 vf28 */
void     rx_challenge_audio_award(rx_addr logic, rx_addr act, rx_addr driver, uint32_t m2, uint32_t m1, uint32_t m0);   /* 00585150 */
void     rx_challenge_audio_target_found(rx_addr logic, rx_addr player, uint32_t hits, uint32_t total, rx_addr pos);   /* 005851a0 */
void     rx_challenge_audio_hit_discovery(rx_addr logic, uint32_t a, uint32_t b, uint32_t c);       /* 005851d0 */
rx_addr  rx_challenge_audio_logic_sdtor(rx_addr logic, uint32_t flags);                             /* 00584e90 vf38 */
void     rx_challenge_audio_logic_vf3(rx_addr logic);                                               /* 00584ed0 (the cue handlers) */
void     rx_challenge_audio_logic_vf4(rx_addr logic);                                               /* 00585010 (they leave) */

#ifdef __cplusplus
}
#endif
