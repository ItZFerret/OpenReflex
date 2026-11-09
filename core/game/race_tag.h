/* race_tag.h -- the Tag party mode (MXActivityTag, activity type 9) and its TagDriver, RuleTag and TagActivityAudioLogic.
 * Files: race_tag.c (the activity), race_tag_ball.c (the ball's physics and the holder handover), race_tag_driver.c (TagDriver,
 * RuleTag), race_tag_audio.c (TagActivityAudioLogic). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- MXActivityTag (0xff3b0 bytes, vtable 0x009446cc): race_tag.c ----
 * Fields added by the class (Activity ends at +0xff070):
 *   +0xff070  the HUD fade (0 .. 0.25 while nobody holds the ball, -1 = hidden; 0.25 set by vf2)
 *   +0xff078  the driver holding the ball (0 = the ball is free)
 *   +0xff07c  the start zone of the round (MiniGameStartZone of the activity row)
 *   +0xff080  team mode (0 = free for all)        +0xff084 / +0xff088  the two teams' round wins   +0xff08c  the last round's team (-1)
 *   +0xff090  the "TagBall" scene entity (a physics object)
 *   +0xff0a0  the ball's start position (vec3), +0xff0b0 its start orientation (quaternion)
 *   +0xff0c0  a PlaceableParticleEffect (0x2a0 bytes: "Cloud_Fire" on the ball when particles are on, [0xaf650c])
 *   +0xff360  the start entity the ball is pushed back to (the zone after the round's start zone)
 *   +0xff364  the water timer, +0xff368 the splash flag, +0xff370 the water kicks, +0xff374 the out-of-range resets
 *   +0xff36c  a round end decided by the host (online)   +0xff378  the ball pushed out of a cannonball zone
 *   +0xff380  the network TagBall object (online; 0 offline)    +0xff384  its send timer   +0xff388  the ball's physics is active
 *   +0xff38c  the time to hold the ball to win a round (60 s)  +0xff390  the rounds to win the match (the activity row's NumLaps)
 *   +0xff394  the match is over   +0xff398  the round's winner   +0xff39c  the wind-down timer (3 s)   +0xff3a0  a new match
 *   +0xff3a4  the ball is shown (+0xff3a8 frames before it is)
 * The TagDriver adds +0xf10 the activity, +0xf14 the time it held the ball this round, +0xf18 its total, +0xf1c the rounds won, +0xf20
 * its team, +0xf24 a water kick pending, +0xf28 the rider's head bone. */
rx_addr  rx_tag_construct(rx_addr act, uint32_t type, rx_addr world);       /* 0056d550 (stack act; ECX = type, EAX = world; RET 4) */
rx_addr  rx_tag_sdtor(rx_addr act, uint32_t flags);                         /* 0056d680 slot 0 (RET 4) */
void     rx_tag_vf1(rx_addr act, rx_addr ref);                              /* 0056d6e0 slot 1 (RET 4) */
void     rx_tag_vf2(rx_addr act, rx_addr arg);                              /* 0056d720 slot 2 (RET 4) */
void     rx_tag_vf3(rx_addr act);                                           /* 0056e1a0 slot 3 */
void     rx_tag_vf4(rx_addr act);                                           /* 0056d9d0 slot 4 */
void     rx_tag_vf15(rx_addr act);                                          /* 0056fe60 slot 15 */
void     rx_tag_vf16(rx_addr act);                                          /* 0056feb0 slot 16 */
void     rx_tag_vf18(rx_addr act);                                          /* 0056e510 slot 18 */
void     rx_tag_vf25(rx_addr act, float dt);                                /* 0056e800 slot 25 (RET 4) */
void     rx_tag_vf27(rx_addr act, float dt);                                /* 00570b30 slot 27 (RET 4) */
void     rx_tag_vf28(rx_addr act, float dt);                                /* 0056e940 slot 28 (RET 4) */
void     rx_tag_vf29(rx_addr act, float dt);                                /* 00570f00 slot 29 (RET 4) */
void     rx_tag_vf32(rx_addr act, uint32_t next);                           /* 0056fe30 slot 32 (RET 4) */
void     rx_tag_vf39(rx_addr act, uint32_t old_state);                      /* 0056e8d0 slot 39 (RET 4) */
void     rx_tag_vf40(rx_addr act, uint32_t old_state);                      /* 00570970 slot 40 (RET 4) */
void     rx_tag_vf42(rx_addr act);                                          /* 005700a0 slot 42 */
int32_t  rx_tag_vf43(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);      /* 0056e790 slot 43 (RET 0xc) */
rx_addr  rx_tag_vf44(rx_addr act, rx_addr player);                         /* 0056e6e0 slot 44 (RET 4) */
void     rx_tag_vf47(rx_addr act, rx_addr driver);                          /* 0056e750 slot 47 (RET 4) */
void     rx_tag_ball_physics(rx_addr act, uint32_t on);                     /* 0056e390 (stdcall act, on; RET 8) */
void     rx_tag_place_round(rx_addr act);                                   /* 0056da60 (stdcall act; RET 4) */
void     rx_tag_net_removed_listen(rx_addr channel, rx_addr act);           /* 00589170 (EAX = the session channel; stdcall act; RET 4) */
void     rx_tag_net_activity_listen(rx_addr channel, rx_addr act);          /* 00589230 */
void     rx_tag_net_round_listen(rx_addr channel, rx_addr act);             /* 005892f0 */
void     rx_tag_net_activity_unlisten(rx_addr channel, rx_addr act);        /* 005893c0 */
void     rx_tag_net_removed_unlisten(rx_addr channel, rx_addr act);         /* 00589420 */
void     rx_tag_net_round_unlisten(rx_addr channel, rx_addr act);           /* 00589480 */
void     rx_tag_net_object_removed(rx_addr act, uint32_t kind, rx_addr object);   /* 00570300 (thiscall; RET 8) */
void     rx_tag_net_round_complete(rx_addr act, uint32_t handle, uint32_t holder, uint32_t score);   /* 005708d0 (thiscall; RET 0xc) */
rx_addr  rx_placeable_effect_construct(rx_addr e);                         /* 006964d0 (ECX = the effect): scene_fx_entities.c */

/* ---- the ball: race_tag_ball.c ---- */
void     rx_tag_ball_impulse(rx_addr act, rx_addr impulse);                 /* 0056e320 (stdcall act, impulse; RET 8) */
void     rx_tag_ball_visible(rx_addr act);                                  /* 0056f210 (EAX = act) */
void     rx_tag_ball_water(rx_addr act, float dt);                          /* 0056f290 (stdcall act, dt; RET 8) */
void     rx_tag_ball_cannonball(rx_addr act);                               /* 0056fa50 (ESI = act) */
void     rx_tag_ball_ground(rx_addr act);                                   /* 0056fcf0 (ESI = act) */
void     rx_tag_ball_splash_reset(rx_addr act);                             /* 00570fc0 (stdcall act; RET 4) */
void     rx_tag_holder_changed(rx_addr act, uint32_t holder);               /* 00570320 (thiscall; RET 4): the network ball's holder handle */
void     rx_tag_hide_arrows(rx_addr act);                                   /* 00589540 (stdcall act; RET 4) */
float    rx_tag_ball_contact_value(rx_addr act);                            /* 00570fc0 (stdcall act; RET 4) -> xmm0 */
void     rx_world_body_velocity_clear(rx_addr world, rx_addr body);         /* 00770d00 (ECX = world; stdcall body; RET 4) */
void     rx_tag_wake_quad(rx_addr set, rx_addr velocity, rx_addr p0, rx_addr p1, rx_addr p2, rx_addr p3, float size, float dt);   /* 007efe90 (EAX = set, ECX = velocity; RET 0x18) */

/* ---- TagDriver (0xf30 bytes, vtable 0x0093fe24) and RuleTag (0x14 bytes, vtable 0x0093fe70): race_tag_driver.c ---- */
rx_addr  rx_tag_driver_construct(rx_addr d, rx_addr act, rx_addr player, uint32_t team);   /* 00514ce0 (ECX = driver, EAX = player; stdcall act, team; RET 8) */
rx_addr  rx_tag_driver_sdtor(rx_addr d, uint32_t flags);                    /* 00514d30 slot 0 (RET 4) */
void     rx_tag_driver_vf1(rx_addr d);                                      /* 00514d60 slot 1 */
void     rx_tag_driver_vf2(rx_addr d);                                      /* 00514e40 slot 2 */
void     rx_tag_driver_vf3(rx_addr d);                                      /* 00514f20 slot 3 */
void     rx_tag_driver_vf7(rx_addr d, float dt);                            /* 00515140 slot 7 (RET 4) */
void     rx_tag_driver_vf9(rx_addr d, rx_addr event);                       /* 00515490 slot 9 (RET 4) */
void     rx_tag_driver_vf16(rx_addr d, uint32_t kind, rx_addr object);      /* 005155d0 slot 16 (RET 8) */
void     rx_tag_driver_vf17(rx_addr d);                                     /* 00515650 slot 17 */
void     rx_tag_driver_ai(rx_addr d, float dt);                             /* 00515330 (ESI = driver; stdcall dt; RET 4) */
void     rx_tag_driver_water_kick(rx_addr d);                               /* 00515440 (ESI = driver) */
rx_addr  rx_tag_driver_rule(rx_addr d);                                     /* 0053b120 (stdcall driver; RET 4) -> its RuleTag */
rx_addr  rx_tag_rule_create(rx_addr act);                                   /* 005894e0 (EAX = ...) -> the static RuleTag */
void     rx_tag_rule_vf1(rx_addr rule, rx_addr driver);                     /* 0051bd30 slot 1 (RET 4) */
void     rx_tag_rule_vf5(rx_addr rule, float dt);                           /* 0051bd70 slot 5 (RET 4) */

/* ---- TagActivityAudioLogic (0x98 bytes, vtable 0x00942f7c): race_tag_audio.c ---- */
void     rx_tag_audio_vf3(rx_addr logic);                                   /* 00583ab0 slot 3 */
void     rx_tag_audio_vf4(rx_addr logic);                                   /* 00583d30 slot 4 */
void     rx_tag_audio_listen_round_started(rx_addr channel, rx_addr logic);  /* 0058c4b0 (EAX = channel; stdcall logic; RET 4) */
void     rx_tag_audio_listen_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);   /* 0058c570 (ECX = channel, EAX = name; RET 8) */
void     rx_tag_audio_listen_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);     /* 0058c630 (ECX = channel, EAX = name; RET 8) */
void     rx_tag_audio_listen_player_wins(rx_addr channel, rx_addr logic);    /* 0058c6f0 (EAX = channel; stdcall logic; RET 4) */
void     rx_tag_audio_unlisten_round_started(rx_addr channel, rx_addr logic);    /* 0058c7b0 */
void     rx_tag_audio_unlisten_activity(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);   /* 0058c810 */
void     rx_tag_audio_unlisten_driver(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);     /* 0058c870 */
void     rx_tag_audio_unlisten_player_wins(rx_addr channel, rx_addr logic);  /* 0058c8d0 */
void     rx_tag_audio_round_started(rx_addr logic, rx_addr act, uint32_t round, uint32_t best, uint32_t rounds);   /* 00584df0 (RET 0x10) */
void     rx_tag_audio_round_over(rx_addr logic, rx_addr act);               /* 00583a30 (RET 4) */
void     rx_tag_audio_match_over(rx_addr logic, rx_addr act);               /* 00583a50 (RET 4) */
void     rx_tag_audio_ball_dropped(rx_addr logic, rx_addr act, rx_addr driver);   /* 00583fe0 (RET 8) */
void     rx_tag_audio_ball_grabbed(rx_addr logic, rx_addr act, rx_addr driver);   /* 00584000 (RET 8) */

#ifdef __cplusplus
}
#endif
