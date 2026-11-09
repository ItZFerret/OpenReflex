/* build_activity_freeride_run.h -- MXActivityFreeRide's run-time methods (build_activity_freeride_run.c). */
#pragma once
#include "rx_game.h"

uint32_t rx_scene_group_query(rx_addr scene, uint32_t type, rx_addr out, int32_t cap, rx_addr name, rx_addr flag);   /* 007d2820 (stdcall; RET 0x18) */
uint32_t rx_event_type_index(rx_addr name);                                                                         /* 005555a0 (cdecl) */
void     rx_free_ride_triggers_refresh(rx_addr activity);                                                           /* 005605e0 (stdcall; RET 4) */
void     rx_free_ride_vf5(rx_addr activity);                                                                        /* 00560110 (thiscall) */
void     rx_free_ride_vf2(rx_addr activity, rx_addr arg);                                                           /* 0055ff70 (thiscall; RET 4) */
void     rx_free_ride_flag_marker(rx_addr activity);                                                                /* 0055ffd0 (ESI = activity) */
uint32_t rx_reset_marker_flag_set_spot(rx_addr flag_e, rx_addr spot);                                                   /* 0055fd50 ResetMarkerFlagEntity::vf28 (thiscall; RET 4) */
void     rx_free_ride_start_slots(rx_addr activity);                                                                /* 00560cd0 (stdcall; RET 4) */
void     rx_free_ride_vf4(rx_addr activity);                                                                        /* 00560070 (thiscall) */
void     rx_audio_logic_vf29_shared(rx_addr logic, rx_addr activity);                                              /* 00584450 (thiscall; RET 4) */
void     rx_free_ride_audio_logic_vf3(rx_addr logic);                                                              /* 00582330 (thiscall; build_activity_audio.c) */
rx_addr  rx_free_ride_vf44(rx_addr activity, rx_addr player);                                                       /* 00560220 (thiscall; RET 4) */
void     rx_free_ride_vf47(rx_addr activity, rx_addr driver);                                                       /* 00560c20 (thiscall; RET 4; build_activity_driver.c) */
void     rx_free_ride_driver_vf1(rx_addr driver);                                                                   /* FreeRideDriver::vf1 0050bce0 (thiscall; build_activity_driver.c) */
void     rx_free_ride_vf52(rx_addr activity);                                                                      /* 00561620 (thiscall) */
void     rx_activity_vf17(rx_addr activity);                                                                       /* Activity::vf17 00557cd0 (thiscall) */
void     rx_activity_vf15(rx_addr activity);                                                                       /* Activity::vf15 00557a70 (thiscall) */
void     rx_driver_reset_vehicle(rx_addr driver);                                                                  /* 0050abe0 (ESI = driver) */
void     rx_world_set_terrain_wetness(rx_addr world, float v);                                                     /* 00767df0 (EAX = world; stdcall float; RET 4) */
void     rx_mud_records_clear_amounts(void);                                                                       /* 00409380 */
void     rx_mud_records_clear_wetness(void);                                                                       /* 00409470 */
void     rx_activity_vf18(rx_addr activity);                                                                       /* Activity::vf18 00557de0 (thiscall) */
void     rx_free_ride_vf18(rx_addr activity);                                                                      /* MXActivityFreeRide::vf18 00561600 (thiscall) */
void     rx_records_reset(rx_addr records, uint32_t slot);                                                         /* MXRecords_Reset 00764710 (ECX = records, EAX = slot; build_activity_records.c) */
void     rx_audio_logic_vf25(rx_addr logic);                                                                       /* ActivityAudioLogic::vf25 00582100 (thiscall; RET 8) */
void     rx_audio_logic_vf26(rx_addr logic);                                                                       /* ActivityAudioLogic::vf26 00582120 (thiscall; RET 8) */
void     rx_vslot26_thunk(rx_addr obj, uint32_t a0, uint32_t a1);                                                 /* 00592ba0 (thiscall; a tail jump to virtual 26; RET 8) */
rx_addr  rx_game_current_world(void);                                                                               /* 0075c530 */
rx_addr  rx_activity_music_entity(rx_addr activity);                                                               /* 0055e560 (stdcall; RET 4) */
void     rx_audio_logic_vf35(rx_addr logic);                                                                       /* ActivityAudioLogic::vf35 005821a0 (thiscall) */
rx_addr  rx_activity_music_entity2(rx_addr activity);                                                              /* 0055e620 (stdcall; RET 4) */
void     rx_audio_logic_vf15(rx_addr logic);                                                                       /* ActivityAudioLogic::vf15 00581f50 (thiscall) */
void     rx_audio_logic_vf36(rx_addr logic);                                                                       /* ActivityAudioLogic::vf36 005821e0 (thiscall) */
void     rx_audio_logic_vf37(rx_addr logic);                                                                       /* ActivityAudioLogic::vf37 00582220 (thiscall) */
void     rx_vslot15_thunk(rx_addr obj, uint32_t a0, uint32_t a1);                                                  /* 00592a60 (thiscall; tail jump to virtual 15; RET 8) */
uint32_t rx_player_controller_present(uint32_t slot);                                                           /* 004dd3d0 (stdcall; RET 4) */
void     rx_level_config_set_name(rx_addr config, rx_addr name);                                                  /* Fn_00756040 (EAX = name, ESI = config) */
void     rx_records_slots_start(rx_addr records);                                                                  /* Fn_00753200 (EAX = the save-game object) */
void     rx_records_slots_attempt(rx_addr records);                                                                /* Fn_00753250 (EAX = the save-game object) */
void     rx_records_set_activity(rx_addr records, rx_addr activity);                                               /* Fn 00753760 (thiscall; RET 4) */
uint32_t rx_event_type_id_of(uint32_t type);                                                                       /* FUN_004e7780 (stdcall; RET 4; build_activity_records.c) */
uint32_t rx_records_set_activity_regs(uint32_t which, uint32_t* valid);                                          /* the registers 00753760 leaves (adapter only): 0 EAX, 1 EDX */
uint32_t rx_session_is_networked(rx_addr session);                                                              /* 00402540 (EAX = the session) -> EAX */
void     rx_world_players_leave(rx_addr world);                                                                    /* FUN_00766930 (stdcall; RET 4) */
void     rx_shared_clear_5c(rx_addr self);                                                                          /* 005d86e0 (thiscall) */
rx_addr  rx_activity_find_by_kind_area(rx_addr manager, uint32_t kind, uint32_t area);                           /* 0055f630 (EAX = the manager, EDI = the kind; stack the area; RET 4) */
