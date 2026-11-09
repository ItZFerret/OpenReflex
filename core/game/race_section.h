/* race_section.h -- the Section Race (MXActivitySectionRace, activity type 0xc) and its SectionRaceDriver. Files: race_section.c (the
 * activity), race_section_driver.c (the driver), race_section_audio.c (SectionRaceActivityAudioLogic), race_section_zone.c (the scene's
 * SectionRaceZone entity), (the shared Activity virtuals are race_champ.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* ---- MXActivitySectionRace (0x13e680 bytes, vtable 0x00944a74): race_section.c ---- */
rx_addr  rx_section_audio_logic_construct(rx_addr logic);                             /* 00582e80 (ESI = logic) -> logic */
rx_addr  rx_section_race_construct(rx_addr act, uint32_t type, rx_addr world);        /* 0056a210 (EDI = activity; stdcall type, world; RET 8) */
rx_addr  rx_section_race_sdtor(rx_addr act, uint32_t flags);                          /* 0056a2c0 slot 0 (RET 4) */
void     rx_section_race_vf2(rx_addr act, rx_addr arg);                               /* 0056a300 slot 2 (RET 4) */
void     rx_section_race_vf3(rx_addr act);                                            /* 0056a520 slot 3 */
void     rx_section_race_vf4(rx_addr act);                                            /* 0056a680 slot 4 */
void     rx_poker_race_vf16(rx_addr act);                                             /* 00571490 slot 16 */
void     rx_section_race_vf18(rx_addr act);                                           /* 0056a6b0 slot 18 */
void     rx_section_race_vf23(rx_addr act, float dt);                                 /* 0056a750 slot 23 (RET 4) */
void     rx_section_race_vf28(rx_addr act, float dt);                                 /* 0056ac70 slot 28 (RET 4) */
void     rx_section_race_vf38(rx_addr act, uint32_t old_state);                       /* 0056a730 slot 38 (RET 4) */
void     rx_section_race_vf42(rx_addr act);                                           /* 0056a930 slot 42 */
int32_t  rx_section_race_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie);    /* 0056adf0 slot 43 (RET 0xc) */
int32_t  rx_section_compare_best(rx_addr a, rx_addr b);                               /* 0056ae40 (EAX = a; stdcall b; RET 4) */
rx_addr  rx_section_race_vf44(rx_addr act, rx_addr player);                           /* 0056acc0 slot 44 (RET 4) */
void     rx_section_race_vf47(rx_addr act, rx_addr driver);                           /* 0056a720 slot 47 (RET 4) */
void     rx_section_race_vf52(void);                                                  /* 0056b020 slot 52 */
void     rx_section_race_leaders(rx_addr act);                                        /* 0056acf0 (EDX = activity) */
void     rx_section_race_net_removed_listen(rx_addr channel, rx_addr act);            /* 00588b80 (EAX = the session channel; stdcall act; RET 4) */
void     rx_section_race_net_activity_listen(rx_addr channel, rx_addr act);           /* 00588c40 */
void     rx_section_race_net_removed_unlisten(rx_addr channel, rx_addr act);          /* 00588d00 */
void     rx_section_race_net_activity_unlisten(rx_addr channel, rx_addr act);         /* 00588d60 */
void     rx_section_net_object_removed(rx_addr act, uint32_t kind, rx_addr object);   /* 0056af70 (thiscall; RET 8) */
uint32_t rx_section_track_audio_kind(rx_addr level, uint32_t area);                   /* 0056aed0 (EDI = the level name, ESI = the area) */

/* ---- SectionRaceDriver (0x1170 bytes, vtable 0x0093fd14): race_section_driver.c ---- */
rx_addr  rx_section_driver_construct(rx_addr d, rx_addr player);                      /* 00512db0 (ESI = driver, EAX = player) */
void     rx_section_driver_vf1(rx_addr d);                                            /* 00512e00 slot 1 */
void     rx_section_driver_vf2(rx_addr d);                                            /* 00512eb0 slot 2 */
void     rx_section_driver_vf3(rx_addr d);                                            /* 00512fc0 slot 3 */
void     rx_section_driver_vf7(rx_addr d, float dt);                                  /* 00513020 slot 7 (RET 4) */
void     rx_section_driver_vf17(rx_addr d);                                           /* 005132e0 slot 17 */
void     rx_section_driver_entered(rx_addr d, rx_addr vehicle, int32_t section, int32_t entering);   /* 00513080 (thiscall; RET 0xc) */
void     rx_section_driver_best(rx_addr d, rx_addr out_index, rx_addr out_value);             /* 005133f0 (ECX = driver; stdcall out index, out value; RET 8) */
void     rx_section_driver_listen(rx_addr d);                                         /* 0053ace0 (stdcall driver; RET 4) */
void     rx_section_driver_unlisten(rx_addr d);                                       /* 0053ada0 (stdcall driver; RET 4) */
void     rx_section_event_driver_int(rx_addr manager, rx_addr name, rx_addr driver, int32_t value);   /* 0053aea0 (ESI = manager, EDX = name, ECX = value; stdcall driver; RET 4) */
uint32_t rx_race_driver_vf12(rx_addr d);                                              /* 005125c0 slot 12 */
float    rx_race_driver_vf13(rx_addr d);                                              /* 005125f0 slot 13 */

/* ---- SectionRaceActivityAudioLogic (0xa4 bytes, vtable 0x00942d9c): race_section_audio.c ---- */
void     rx_section_audio_vf3(rx_addr logic);                                         /* 00582ef0 slot 3 */
void     rx_section_audio_vf4(rx_addr logic);                                         /* 00582ff0 slot 4 */

/* ---- SectionRaceZone (scene entity 0x1042, vtable 0x00938c5c): race_section_zone.c ---- */
rx_addr  rx_section_zone_create(void);                                                /* the factory arm of 004046a0 for the type 0x1042 (00404f95) */
rx_addr  rx_section_zone_sdtor(rx_addr zone, uint32_t flags);                         /* 0040aa00 slot 0 (RET 4) */
void     rx_section_zone_load(rx_addr zone, rx_addr reader);                          /* 0040aa30 slot 5 (RET 4) */
void     rx_section_zone_tick(rx_addr zone, float dt);                                /* 0040aae0 slot 19 (RET 4) */
void     rx_section_zone_post(uint32_t index, uint32_t start, rx_addr vehicle);       /* 004198e0 (ECX = index, EDX = start; stdcall vehicle; RET 4) */
void     rx_section_zone_player_inside(rx_addr zone, rx_addr player);                 /* 0040ac80 (EAX = zone, ESI = player) */

/* ---- SectionRaceActivityAudioLogic handlers: race_section_audio.c ---- */
void     rx_section_audio_listen(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);     /* 0058ba90 (EAX = name, ECX = channel; RET 8) */
void     rx_section_audio_unlisten(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method);   /* 0058bb50 (EAX = name, ECX = channel; RET 8) */
void     rx_section_audio_winner(rx_addr logic, rx_addr driver, int32_t section);     /* 005830f0 (thiscall; RET 8) */
void     rx_section_audio_lost(rx_addr logic, rx_addr driver, int32_t section);       /* 00583110 (thiscall; RET 8) */

#ifdef __cplusplus
}
#endif
