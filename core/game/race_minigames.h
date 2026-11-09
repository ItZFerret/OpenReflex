/* race_minigames.h -- the last three party modes of the original, which no installed level can run (their scenes' entities are not
 * in any shipped level, see core/plan/QUEUE.md "## w-partylast"): EggHunt (MXActivityEggHunt, activity type 0xf, "Greed" in the data),
 * Graffiti (MXActivityGraffiti, 0xb) and Capture (MXActivityCapture, 0x10; "CTF" in the data). Files: race_egghunt.c, race_graffiti.c,
 * race_capture.c. Verified with the oracle's test fixtures (core/oracle/partylast_probe.cpp). The session refuses an event of these
 * modes when the level lacks what their start needs (api/rx_session.c minigame_content_missing, rx_session_event_load returns 3); each
 * activity's vf2 still stops (act_unreached) where the original would fault. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- MXActivityEggHunt (0x100130 bytes, vtable 0x00944e24): race_egghunt.c ----
 * Fields added by the class (Activity ends at +0xff070):
 *   +0xff0a0  the round's start zone (MiniGameStartZone)   +0xff0a8  the eggs' zone (cycles through the area's start entities)
 *   +0xff0b0  the eggs' zone's position   +0xff0c0  an egg was collected (the respawn clock runs)   +0xff0c4  the respawn clock (30 s)
 *   +0xff0d0  ten GreedObjectModel (0x1a0 bytes each; vtable 0x944d6c / IRenderable 0x944de4)
 *   +0x100110 the round   +0x100114 the rounds to win (4)   +0x100118 the match is over (never set by the original)
 *   +0x10011c the winner (never set: every round ends by the clock, a tie)   +0x100120 the wind-down timer (3 s)   +0x100124 a new match
 *   +0x100128 the network egg hunt object (online; 0 offline)   +0x10012c the eggs' zone's entity
 * GreedObjectModel (a ModelEntity): +0x100 the model (the scene's "GreedModel"), +0x140 the spin angle (2.0944 rad/s), +0x150 its base
 * matrix, +0x190 collected, +0xa2 visible.
 * EggHuntDriver (0xf20 bytes, vtable 0x0093ff24: a NetworkDriver): +0xf10 the activity, +0xf14 / +0xf18 (cleared, not otherwise used),
 * +0xf1c the eggs collected. */
rx_addr  rx_egghunt_construct(rx_addr act, uint32_t type, rx_addr world);        /* 00574730 (stack act; ECX = type, EAX = world; RET 4) */
rx_addr  rx_egghunt_sdtor(rx_addr act, uint32_t flags);                          /* 00574840 slot 0 (RET 4) */
void     rx_egghunt_destroy(rx_addr act);                                        /* 00574860 (ECX = act) */
void     rx_egghunt_vf2(rx_addr act, rx_addr arg);                               /* 005748f0 slot 2 (RET 4) */
void     rx_egghunt_vf3(rx_addr act);                                            /* 00574b90 slot 3 */
void     rx_egghunt_vf4(rx_addr act);                                            /* 00574cd0 slot 4 */
void     rx_egghunt_vf15(rx_addr act);                                           /* 00574d20 slot 15 */
void     rx_egghunt_vf16(rx_addr act);                                           /* 00574d40 slot 16 */
void     rx_egghunt_vf18(rx_addr act);                                           /* 005757b0 slot 18 */
void     rx_egghunt_vf25(rx_addr act, float dt);                                 /* 00575af0 slot 25 (RET 4) */
void     rx_egghunt_vf27(rx_addr act, float dt);                                 /* 00575e00 slot 27 (RET 4) */
void     rx_egghunt_vf28(rx_addr act, float dt);                                 /* 00575b60 slot 28 (RET 4) */
void     rx_egghunt_vf40(rx_addr act, uint32_t old_state);                       /* 005758e0 slot 40 (RET 4) */
void     rx_egghunt_vf42(rx_addr act);                                           /* 00575eb0 slot 42 */
int32_t  rx_egghunt_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);       /* 005758a0 slot 43 (RET 0xc) */
rx_addr  rx_egghunt_vf44(rx_addr act, rx_addr player);                           /* 00575840 slot 44 (RET 4) */
void     rx_egghunt_vf47(rx_addr act, rx_addr driver);                           /* 00575ed0 slot 47 (RET 4) */
void     rx_egghunt_place_round(rx_addr act);                                    /* 00574dd0 (stdcall act; RET 4) */
void     rx_egghunt_spawn_eggs(rx_addr act);                                     /* 00575440 (EDI = act) */
uint32_t rx_egghunt_collect(rx_addr obj, rx_addr driver, rx_addr player);       /* 00574660 (ESI = egg, EDI = driver; stdcall player; RET 4) */
void     rx_egghunt_net_removed_listen(rx_addr channel, rx_addr act);            /* 00589ed0 (EAX = channel; stdcall act; RET 4) */
void     rx_egghunt_net_picked_listen(rx_addr channel, rx_addr act);             /* 00589f90 */
void     rx_egghunt_net_removed_unlisten(rx_addr channel, rx_addr act);          /* 0058a050 */
void     rx_egghunt_net_picked_unlisten(rx_addr channel, rx_addr act);           /* 0058a0b0 */
void     rx_egghunt_player_wins(rx_addr manager, rx_addr winner, rx_addr text, rx_addr act);   /* 0058a110 (ESI, ECX, EDX; stdcall act) */
rx_addr  rx_greed_model_construct(rx_addr e);                                    /* 0055ec30 (ECX = entity) */
void     rx_greed_model_vf16(rx_addr e, rx_addr m);                              /* 00576040 slot 16 (RET 4) */
void     rx_greed_model_vf19(rx_addr e, float dt);                               /* 00576070 slot 19 (RET 4) */
void     rx_greed_model_vf28(rx_addr e, rx_addr p);                              /* 00574630 slot 28 (RET 4) */
rx_addr  rx_egghunt_driver_sdtor(rx_addr d, uint32_t flags);                     /* 00515ff0 slot 0 (RET 4) */
void     rx_egghunt_driver_vf3(rx_addr d);                                       /* 00516030 slot 3 */
void     rx_egghunt_driver_vf7(rx_addr d, float dt);                             /* 005160b0 slot 7 (RET 4) */
void     rx_egghunt_audio_vf3(rx_addr logic);                                    /* 005848f0 */
void     rx_egghunt_audio_vf4(rx_addr logic);                                    /* 00584b70 */
void     rx_egghunt_audio_listen_round_started(rx_addr channel, rx_addr logic);                         /* 0058cff0 */
void     rx_egghunt_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058d0b0 */
void     rx_egghunt_audio_listen_collected(rx_addr channel, rx_addr logic);                             /* 0058d170 */
void     rx_egghunt_audio_listen_player_wins(rx_addr channel, rx_addr logic);                           /* 0058d230 */
void     rx_egghunt_audio_unlisten_round_started(rx_addr channel, rx_addr logic);                       /* 0058d2f0 */
void     rx_egghunt_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method); /* 0058d350 */
void     rx_egghunt_audio_unlisten_collected(rx_addr channel, rx_addr logic);                           /* 0058d3b0 */
void     rx_egghunt_audio_unlisten_player_wins(rx_addr channel, rx_addr logic);                         /* 0058d410 */
void     rx_egghunt_audio_collected(rx_addr logic, rx_addr act, rx_addr driver);                        /* 00584e10 (RET 8) */
void     rx_egghunt_audio_spawned(rx_addr logic, rx_addr act);                                          /* 00584e30 (RET 4) */

/* ---- MXActivityCapture (0x1014b0 bytes, vtable 0x00944fd4): race_capture.c ----
 * Fields added by the class (Activity ends at +0xff070):
 *   +0xff070  the red flag (MXCaptureFlag 0x190 bytes: a GreedObjectModel-like ModelEntity)   +0xff200  the blue flag
 *   +0xff390 / +0xff394  0.25   +0xff398  the carrier of the red flag (a blue rider)   +0xff39c  the carrier of the blue flag
 *   +0xff3a0  the red base's matrix (row 3 its position, +0xff3d0)   +0xff3e0  the blue base's (+0xff410)
 *   +0xff424 / +0xff428  the red / blue team's score   +0xff42c  the score to win (30)   +0xff430 / +0xff431  the red / blue flag at home
 *   +0xff434  the network CTF object (online; 0 offline)   +0xff440  twelve PlaceableParticleEffects (0x2b0 each: +0x2a0 the team,
 *   +0x2a4 taken)   +0x101480 / +0x101484  the red / blue base entities   +0x1014ac  the match is over
 * CaptureDriver (0xf20 bytes, vtable 0x0093ff74: a NetworkDriver): +0xf10 the activity, +0xf14 the team, +0xf18 its smoke effect,
 * +0xf1c carrying (byte), +0xee8 = 6. RuleCapture (0x20 bytes, vtable 0x0093ffc0, kind 0xd): +4 the driver, +8 enabled, +0xc carrying,
 * +0x10 the grab clock (5 s), +0x14 the grab radius^2 (144), +0x18 the own-base clock. */
rx_addr  rx_capture_construct(rx_addr act, uint32_t type, rx_addr world);        /* 00576290 (stack act; ECX = type, EAX = world; RET 4) */
rx_addr  rx_capture_smoke_construct(rx_addr e);                                  /* 005764e0 (ECX = effect) */
void     rx_capture_destroy(rx_addr act);                                        /* 005764f0 (ECX = act) */
rx_addr  rx_capture_sdtor(rx_addr act, uint32_t flags);                          /* 005764c0 slot 0 (RET 4) */
void     rx_capture_net_removed_listen(rx_addr channel, rx_addr act);            /* 0058a170 (EAX = channel; stdcall act; RET 4) */
void     rx_capture_net_event_listen(rx_addr channel, rx_addr act);              /* 0058a230 */
void     rx_capture_flag_attach(rx_addr found, rx_addr flag, rx_addr scene, rx_addr name);   /* 00575fc0 (EAX, ESI; stdcall scene, name; RET 8) */
void     rx_capture_place_starts(rx_addr act);                                   /* 00576b60 (stdcall act; RET 4) */
void     rx_capture_base_position(rx_addr out, uint32_t team, rx_addr act);      /* 00577ad0 (EAX = out, EDX = team; stdcall act; RET 4) */
void     rx_capture_flag_home(uint32_t team, rx_addr act);                       /* 00577b80 (EAX = team; stdcall act; RET 4) */
void     rx_capture_score(rx_addr act, uint32_t team);                           /* 00577c30 (ESI = act; stdcall team; RET 4) */
void     rx_capture_flags_touch(rx_addr act);                                    /* 00578200 (EDI = act) */
void     rx_capture_vf2(rx_addr act, rx_addr arg);                               /* 005765c0 slot 2 (RET 4) */
void     rx_capture_vf3(rx_addr act);                                            /* 00576f40 slot 3 */
void     rx_capture_vf4(rx_addr act);                                            /* 00576ab0 slot 4 */
void     rx_capture_vf15(rx_addr act);                                           /* 00577a00 slot 15 */
void     rx_capture_vf16(rx_addr act);                                           /* 00577a60 slot 16 */
void     rx_capture_vf18(rx_addr act);                                           /* 00577010 slot 18 */
uint32_t rx_capture_vf19(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir, rx_addr up);   /* 00577d20 slot 19 (RET 0x10) */
void     rx_capture_vf27(rx_addr act, float dt);                                 /* 00577b60 slot 27 (RET 4) */
void     rx_capture_vf28(rx_addr act, float dt);                                 /* 00577630 slot 28 (RET 4) */
void     rx_capture_vf38(rx_addr act, uint32_t old_state);                       /* 005772b0 slot 38 (RET 4) */
void     rx_capture_vf42(rx_addr act);                                           /* 00577280 slot 42 */
int32_t  rx_capture_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);       /* 00577240 slot 43 (RET 0xc) */
rx_addr  rx_capture_vf44(rx_addr act, rx_addr player);                           /* 005770f0 slot 44 (RET 4) */
void     rx_capture_vf47(rx_addr act, rx_addr driver);                           /* 00577200 slot 47 (RET 4) */
rx_addr  rx_capture_rule_create(rx_addr driver);                                 /* 0058a2f0 (ECX = driver) */
rx_addr  rx_capture_driver_sdtor(rx_addr d, uint32_t flags);                     /* 005161d0 slot 0 (RET 4) */
rx_addr  rx_driver_rule_capture(rx_addr d);                                      /* 0053b290 (stdcall driver; RET 4) */
void     rx_capture_driver_vf3(rx_addr d);                                       /* 00516200 slot 3 */
void     rx_capture_driver_human(rx_addr d, float dt);                           /* 00516490 (stdcall driver, dt; RET 8) */
void     rx_capture_driver_ai(rx_addr d, float dt);                              /* 00516380 (ESI = driver; stdcall dt; RET 4) */
void     rx_capture_driver_vf7(rx_addr d, float dt);                             /* 005162a0 slot 7 (RET 4) */
void     rx_capture_driver_vf9(rx_addr d, rx_addr rule);                         /* 005164d0 slot 9 (RET 4) */
void     rx_capture_rule_vf1(rx_addr rule, rx_addr driver);                      /* 0051ca80 slot 1 (RET 4) */
uint32_t rx_capture_rule_vf5(rx_addr rule, float dt);                            /* 0051cab0 slot 5 (RET 4) */

/* ---- MXActivityGraffiti (0xff090 bytes, vtable 0x0094497c): race_graffiti.c ----
 * Fields added by the class (Activity ends at +0xff070): +0xff070 the capture points (an array of GraffitiCapturePoint*), +0xff074 their
 * count, +0xff078 the capacity, +0xff07c the growth (1), +0xff080 the count found, +0xff084 the network graffiti object (online; 0).
 * GraffitiDriver (0xf30 bytes, vtable 0x0093fc24: a NetworkDriver): +0xf10 the activity, +0xf14 the points held, +0xf18, +0xf1c the score,
 * +0xf20 a capture in progress, +0xf24 its clock (-1). GraffitiCapturePoint (the scene entity type 0x1040, 0xcb0 bytes; race_graffiti_point.c):
 * +0xb00 LinkedTriggerID, +0xb44 counted, +0xb48 the riders inside (12-byte records {vehicle, state, inside}, count +0xb4c, capacity +0xb50,
 * growth +0xb54), +0xb5c the holder's vehicle, +0xb60 its +0x190, +0xb64 its score, +0xb70 .. +0xb7c the colour, +0xb80 the activity,
 * +0xb90 the label's transform, +0xbd0 the label (AnimatedTextIcon3dBillboard: render, oracle only), +0xc48 its text object. */
rx_addr  rx_graffiti_audio_construct(rx_addr logic);                             /* 00582450 (ESI = logic) */
rx_addr  rx_graffiti_construct(rx_addr act, uint32_t type, rx_addr world);       /* 00563d90 (stack act; ECX = type, EAX = world; RET 4) */
rx_addr  rx_graffiti_sdtor(rx_addr act, uint32_t flags);                         /* 00563e10 slot 0 (RET 4) */
void     rx_graffiti_net_removed_listen(rx_addr channel, rx_addr act);           /* 00587ef0 (EAX = channel; stdcall act; RET 4) */
void     rx_graffiti_net_captured_listen(rx_addr channel, rx_addr act);          /* 00587fb0 */
void     rx_graffiti_net_removed_unlisten(rx_addr channel, rx_addr act);         /* 00588070 */
void     rx_graffiti_net_captured_unlisten(rx_addr channel, rx_addr act);        /* 005880d0 */
rx_addr  rx_graffiti_point_trigger(rx_addr p);                                   /* 00408400 (stdcall point; RET 4) */
void     rx_graffiti_point_label(rx_addr p);                                     /* 00408220 (EDI = point) */
rx_addr  rx_graffiti_driver_of(rx_addr act, rx_addr vehicle);                    /* 00564970 (EDI = act; stdcall vehicle; RET 4) */
void     rx_graffiti_vf2(rx_addr act, rx_addr arg);                              /* 00563e60 slot 2 (RET 4) */
void     rx_graffiti_vf3(rx_addr act);                                           /* 00564320 slot 3 */
void     rx_graffiti_vf15(rx_addr act);                                          /* 00564960 slot 15 */
void     rx_graffiti_vf18(rx_addr act);                                          /* 00564450 slot 18 */
void     rx_graffiti_vf23(rx_addr act, float dt);                                /* 005646c0 slot 23 (RET 4) */
void     rx_graffiti_vf40(rx_addr act, uint32_t old_state);                      /* 00564560 slot 40 (RET 4) */
void     rx_graffiti_vf42(rx_addr act);                                          /* 005648c0 slot 42 */
int32_t  rx_graffiti_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);      /* 005644f0 slot 43 (RET 0xc) */
rx_addr  rx_graffiti_driver_construct(rx_addr d, rx_addr player, rx_addr act);   /* 0050df10 (ECX = d, EAX = player; stdcall act; RET 4) */
rx_addr  rx_graffiti_vf44(rx_addr act, rx_addr player);                          /* 005644c0 slot 44 (RET 4) */
void     rx_graffiti_driver_vf1(rx_addr d);                                      /* 0050df80 slot 1 */
void     rx_graffiti_driver_vf3(rx_addr d);                                      /* 0050dfa0 slot 3 */
void     rx_graffiti_driver_vf7(rx_addr d, float dt);                            /* 0050e030 slot 7 (RET 4) */
void     rx_graffiti_driver_vf17(rx_addr d);                                     /* 0050e120 slot 17 */
void     rx_graffiti_audio_listen_captured(rx_addr channel, rx_addr logic);      /* 0058b190 */
void     rx_graffiti_audio_unlisten_captured(rx_addr channel, rx_addr logic);    /* 0058b250 */
void     rx_graffiti_audio_vf29(rx_addr logic, rx_addr act);                     /* 00582490 slot 29 (RET 4) */
void     rx_graffiti_audio_vf30(rx_addr logic);                                  /* 005824f0 slot 30 */
void     rx_graffiti_audio_vf32(rx_addr logic);                                  /* 00582510 slot 32 */
void     rx_audio_logic_vf19(rx_addr logic, uint32_t a, uint32_t b);             /* 00582010 ActivityAudioLogic::vf19 (RET 8) */
void     rx_graffiti_audio_captured(rx_addr logic, rx_addr driver);              /* 00582520 (RET 4) */

/* ---- GraffitiCapturePoint: race_graffiti_point.c ---- */
rx_addr  rx_graffiti_point_create(void);                                         /* the factory's case 0x1040 (004046a0) */
rx_addr  rx_graffiti_point_construct(rx_addr p);                                 /* 004075c0 (EDI = point) */
void     rx_graffiti_point_destroy(rx_addr p);                                   /* 004076c0 (EAX = point) */
rx_addr  rx_graffiti_point_sdtor(rx_addr p, uint32_t flags);                     /* 00407670 slot 0 (RET 4) */
rx_addr  rx_graffiti_point_sdtor_render(rx_addr r, uint32_t flags);              /* 0041b170 IRenderable slot 0 (ECX = point + 0x90) */
uint32_t rx_graffiti_point_vf5(rx_addr p, uint32_t reader);                      /* 00407780 slot 5 (RET 4) */
void     rx_graffiti_point_vf19(rx_addr p, float dt);                            /* 00407ec0 slot 19 (RET 4) */
void     rx_graffiti_point_on_init(rx_addr p, rx_addr act);                      /* 00407930 "Activity Init" (RET 4) */
void     rx_graffiti_point_set_holder(rx_addr p, rx_addr vehicle);               /* 00407970 (ESI = point, EAX = vehicle) */
rx_addr  rx_graffiti_point_record(rx_addr p, rx_addr vehicle);                   /* 00407a30 (stdcall point, vehicle; RET 8) */
void     rx_graffiti_point_records_grow(rx_addr v, uint32_t cap);                /* 0041a530 (ESI = vector; stdcall capacity; RET 4) */
void     rx_graffiti_point_add_rider(rx_addr vehicle, rx_addr p);                /* 00407a70 (EAX = vehicle; stdcall point; RET 4) */
void     rx_graffiti_point_on_entered(rx_addr p, rx_addr vehicle, rx_addr trigger);   /* 00407af0 "VehicleEntered" (RET 8) */
void     rx_graffiti_point_on_exited(rx_addr p, rx_addr vehicle, rx_addr trigger);    /* 00407c00 "VehicleExited" (RET 8) */
void     rx_graffiti_point_record_update(rx_addr p, rx_addr r);                  /* 00407d10 (ECX = point, ESI = record) */
void     rx_graffiti_point_remove_rider(rx_addr p, rx_addr vehicle);             /* 00407e00 (EAX = point; stdcall vehicle; RET 4) */
void     rx_graffiti_point_prune(rx_addr p);                                     /* 00407e60 (EAX = point) */
void     rx_graffiti_driver_captured(rx_addr d, uint32_t score);                 /* 0050e0a0 (EAX = driver; stdcall score; RET 4) */
void     rx_graffiti_point_capture(rx_addr p, rx_addr record, uint32_t score);   /* 00408090 (EAX = point; stdcall record, score; RET 8) */
void     rx_graffiti_point_capture_vehicle(rx_addr p, rx_addr vehicle, uint32_t score);   /* 00408110 (EAX = point; RET 8) */
void     rx_graffiti_point_score(rx_addr p, rx_addr record, int32_t score);      /* 00407f10 (EAX = point; stdcall record, score; RET 8) */
void     rx_graffiti_point_on_score(rx_addr p, rx_addr player, float score);     /* 00408180 "StuntFinalScore" (RET 8) */
void     rx_graffiti_point_on_begins(rx_addr p, rx_addr player, uint32_t a, uint32_t b);  /* 004081c0 "StuntBegins" (RET 0xc) */
void     rx_graffiti_point_on_failed(rx_addr p, rx_addr player);                 /* 004081e0 "StuntFailed" (RET 4) */
void     rx_graffiti_point_on_kickout(rx_addr p, rx_addr player, rx_addr stunt); /* 00408200 "StuntKickOut" (RET 8) */
void     rx_graffiti_point_listen_trigger(rx_addr name, rx_addr p, rx_addr method, rx_addr adj);     /* 00419220 (ECX = name; RET 0xc) */
void     rx_graffiti_point_listen_init(rx_addr p, rx_addr method, rx_addr adj);                      /* 004192f0 */
void     rx_graffiti_point_listen_score(rx_addr p, rx_addr method, rx_addr adj);                     /* 004193c0 */
void     rx_graffiti_point_listen_begins(rx_addr p, rx_addr method, rx_addr adj);                    /* 00419490 */
void     rx_graffiti_point_listen_failed(rx_addr p, rx_addr method, rx_addr adj);                    /* 00419560 */
void     rx_graffiti_point_listen_kickout(rx_addr p, rx_addr method, rx_addr adj);                   /* 00419630 */
void     rx_graffiti_point_unlisten_trigger(rx_addr name, rx_addr p, rx_addr method, rx_addr adj);   /* 00418f80 (ECX = name; RET 0xc) */
void     rx_graffiti_point_unlisten_init(rx_addr p, rx_addr method, rx_addr adj);                    /* 00418ff0 */
void     rx_graffiti_point_unlisten_score(rx_addr p, rx_addr method, rx_addr adj);                   /* 00419060 */
void     rx_graffiti_point_unlisten_begins(rx_addr p, rx_addr method, rx_addr adj);                  /* 004190d0 */
void     rx_graffiti_point_unlisten_failed(rx_addr p, rx_addr method, rx_addr adj);                  /* 00419140 */
void     rx_graffiti_point_unlisten_kickout(rx_addr p, rx_addr method, rx_addr adj);                 /* 004191b0 */
void     rx_event_proxy_call_member_player_float(rx_addr proxy, rx_addr payload);                   /* 0041a0d0 (RET 4) */
void     rx_event_proxy_call_member_player_int2(rx_addr proxy, rx_addr payload);                    /* 006c57d0 (RET 4) */

#ifdef __cplusplus
}
#endif
