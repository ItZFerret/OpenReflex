/* race_champ.h -- the championship race activity classes (race_champ.c, group rx_race): MXActivityRaceChampIntro / Practice / Heat /
 * MainEvent / LCQ (vtables 0x943654 / 0x943754 / 0x943854 / 0x943954 / 0x943a54) and MXActivityWaypointRaceChampIntro / Practice /
 * MainEvent (0x943f14 / 0x943d14 / 0x943e14), the base-class virtuals they share (Activity::vf16 / vf20 / vf45, the slot 14
 * placement swap, the race stub's slot 9 / 45), and the callees those need. The factory cases are in build_activity_race.c.
 * Calling conventions of the original are in each comment (EAX / ECX / ESI / EDI = register arguments, RET n = callee cleanup). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* the series manager's current event type (the guard 0x9e275c, MXSeriesManager_Construct, 005b6a30) */
uint32_t rx_series_current_type(void);

/* ---- constructors of the waypoint championship classes (the factory's cases 0x30 / 0x31) ---- */
rx_addr rx_champ_wp_practice_construct(rx_addr act, uint32_t type, rx_addr world);   /* 0057ea60 (ESI = activity, ECX = type, EAX = world) -> EAX */
rx_addr rx_champ_wp_main_construct(rx_addr act, uint32_t type, rx_addr world);       /* 0057e920 (ESI = activity, ECX = type, EAX = world) -> EAX */

/* ---- the destructors (thiscall; flags; RET 4) -> this ---- */
rx_addr rx_champ_heat_sdtor(rx_addr act, uint32_t flags);            /* 0057dfb0 */
rx_addr rx_champ_intro_sdtor(rx_addr act, uint32_t flags);           /* 0057e050 */
rx_addr rx_champ_lcq_sdtor(rx_addr act, uint32_t flags);             /* 0057e1b0 */
rx_addr rx_champ_main_sdtor(rx_addr act, uint32_t flags);            /* 0057e310 */
rx_addr rx_champ_practice_sdtor(rx_addr act, uint32_t flags);        /* 0057e820 */
rx_addr rx_champ_wp_intro_sdtor(rx_addr act, uint32_t flags);        /* 0057e880 */
rx_addr rx_champ_wp_main_sdtor(rx_addr act, uint32_t flags);         /* 0057e960 */
rx_addr rx_champ_wp_practice_sdtor(rx_addr act, uint32_t flags);     /* 0057eaa0 */

/* ---- vf2 (the activity's start: thiscall; the argument is ignored; RET 4) ---- */
void rx_champ_intro_vf2(rx_addr act, rx_addr arg);                   /* 0057e090 */
void rx_champ_heat_vf2(rx_addr act, rx_addr arg);                    /* 0057e1f0 */
void rx_champ_main_vf2(rx_addr act, rx_addr arg);                    /* 0057e350 */
void rx_champ_wp_intro_vf2(rx_addr act, rx_addr arg);                /* 0057e8c0 */
void rx_champ_wp_main_vf2(rx_addr act, rx_addr arg);                 /* 0057e9b0 */
/* the stunt-event listener templates of MainEvent::vf2 / vf3 (EventHandlerProxy 0x945d04 / 0x945d24) */
void rx_champ_stunt_listen(rx_addr name, rx_addr self, rx_addr method);      /* 0058a9a0 (ECX = event name; stdcall self, method; RET 8) */
void rx_champ_physics_stunt_listen(rx_addr self);                            /* 0058aa60 (stdcall self; RET 4) */
void rx_champ_stunt_unlisten(rx_addr name, rx_addr self, rx_addr method);    /* 0058ab20 (ECX = event name; stdcall self, method; RET 8) */
void rx_champ_physics_stunt_unlisten(rx_addr self);                          /* 0058ab80 (stdcall self; RET 4) */
void rx_champ_unlisten_lap(rx_addr channel, rx_addr act);                    /* 0058a940 (EAX = channel; stdcall act; RET 4) */
/* the stunt event handlers (stdcall-like members: ECX = activity; the player is the first argument) */
void rx_champ_main_stunt_begin(rx_addr act, rx_addr player, rx_addr desc);                     /* 0057e7c0 (RET 8) */
void rx_champ_main_stunt_failed(rx_addr act, rx_addr player, rx_addr desc);                    /* 0057e7f0 (RET 8) */
void rx_champ_main_physics_stunt_begin(rx_addr act, rx_addr player, rx_addr desc, uint32_t t); /* 0057e790 (RET 0xc) */

/* ---- vf3 (the activity's end: thiscall) ---- */
void rx_champ_heat_vf3(rx_addr act);                                 /* 0057e210 */
void rx_champ_main_vf3(rx_addr act);                                 /* 0057e390 */
void rx_champ_wp_intro_vf3(rx_addr act);                             /* 0057e8f0 (a jump to 00578b30) */

/* ---- vf5 (the area's assets, the lap pacing; thiscall) ---- */
void rx_champ_heat_vf5(rx_addr act);                                 /* 0057e0b0 (the intro's and the heat's) */
void rx_champ_lcq_vf5(rx_addr act);                                  /* 0057e410 (the practice's, the main event's and the LCQ's) */
void rx_champ_wp_intro_vf5(rx_addr act);                             /* 0057e8e0 (a jump to 00578500) */

/* ---- the rest of the virtuals ---- */
uint32_t rx_race_stub_vf9(rx_addr act);                              /* 0055eb70 MXActivityRaceStub::vf9 (a jump to the finish scenario's vf9) */
void rx_champ_place_swap(rx_addr act, rx_addr driver, int32_t place);   /* 005693d0 slot 14 (thiscall; RET 8) */
void rx_activity_vf16(rx_addr act);                                  /* 00557b40 Activity::vf16 (the restart) */
void rx_player_restart_reset(rx_addr player);                        /* 0054e190 (EAX = the player) */
void rx_champ_vf15(rx_addr act);                                     /* 0057eaf0 slot 15 */
void rx_champ_vf17(rx_addr act);                                     /* 0057e270 slot 17 */
void rx_activity_vf20(rx_addr act, rx_addr player);                  /* 0055d120 Activity::vf20 (thiscall; RET 4) */
void rx_waypoint_activity_vf20(rx_addr act, rx_addr player);         /* 0057a3e0 MXActivityWaypointRace::vf20 (thiscall; RET 4) */
void rx_waypoint_activity_vf16(rx_addr act);                         /* 00578cc0 MXActivityWaypointRace::vf16 (thiscall) */
void rx_champ_wp_practice_vf16(rx_addr act);                         /* 0057eb60 */
void rx_champ_wp_main_vf16(rx_addr act);                             /* 0057e9c0 (a jump to 00578cc0) */
void rx_champ_intro_vf25(rx_addr act, float dt);                     /* 0057e1a0 */
void rx_champ_wp_intro_vf25(rx_addr act, float dt);                  /* 0057e910 */
void rx_champ_heat_vf28(rx_addr act, float dt);                      /* 0057e000 */
void rx_champ_lcq_vf28(rx_addr act, float dt);                       /* 0057e2d0 */
void rx_champ_practice_vf28(rx_addr act, float dt);                  /* 0057e860 */
void rx_champ_main_vf28(rx_addr act, float dt);                      /* 0057e4f0 */
void rx_champ_wp_main_vf28(rx_addr act, float dt);                   /* 0057e9e0 */
void rx_champ_heat_vf29(rx_addr act, float dt);                      /* 0057c010 (the heat's and the LCQ's) */
void rx_activity_vf29(rx_addr act, float dt);                        /* 00559a00 Activity::vf29 (the base Post update: Free Ride) */
void rx_champ_main_vf29(rx_addr act, float dt);                      /* 0057e6b0 */
void rx_champ_main_vf34(rx_addr act, uint32_t unused);               /* 0057e700 */
void rx_champ_vf35(rx_addr act, uint32_t unused);                    /* 0057e720 Activity::vf35 */
void rx_champ_wp_vf35(rx_addr act, uint32_t next);                   /* 0057e9f0 (a jump to 00579b20) */
void rx_champ_wp_vf37(rx_addr act, uint32_t old);                    /* 0057e900 (a jump to 005589c0) */
void rx_champ_heat_vf38(rx_addr act, uint32_t old);                  /* 0057dff0 (a jump to 00568340) */
void rx_champ_main_vf38(rx_addr act, uint32_t old);                  /* 0057e4b0 */
void rx_champ_wp_main_vf42(rx_addr act);                             /* 0057e9d0 (a jump to 00579300) */
void rx_activity_vf45(rx_addr act, rx_addr from, rx_addr to);        /* 00557570 Activity::vf45 (thiscall; RET 8) */
void rx_race_stub_vf45(rx_addr act, rx_addr from, rx_addr to);       /* 00567ac0 MXActivityRaceStub::vf45 (thiscall; RET 8) */
void rx_champ_vf52(rx_addr act);                                     /* 0057ea30 (most of the championship classes) */
void rx_champ_main_vf52(rx_addr act);                                /* 0057e760 */
#ifdef __cplusplus
}
#endif
