/* race_activity_teardown.h -- the race activity's teardown (race_activity_teardown.c): MXActivityRace::vf3 / MXActivityRaceStub::vf3
 * and the race's handler-unsubscription templates. Activity::vf3 and the event-system destruction: build_activity_teardown.h. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void rx_race_unlisten_stunt(rx_addr name, rx_addr act, rx_addr method);       /* 005882b0 (ECX = name; stdcall act, method; RET 8) */
void rx_race_unlisten_physics_stunt(rx_addr act);                              /* 00588310 (stdcall act; RET 4) */
void rx_race_unlisten_lap_completed(rx_addr channel, rx_addr act, rx_addr method);   /* 00588a80 (EAX = channel; stdcall act, method; RET 8) */
void rx_race_unlisten_timed_section(rx_addr act);                              /* 00588ae0 (stdcall act; RET 4) */
void rx_race_stub_vf3(rx_addr act);                                            /* 00566c80 MXActivityRaceStub::vf3 (thiscall; ends in a jump to Activity::vf3) */
void rx_race_vf3(rx_addr act);                                                 /* 00564c10 MXActivityRace::vf3 (thiscall; ends in a jump to 00566c80) */
void rx_race_driver_unlisten_network(rx_addr registry, rx_addr drv);           /* 0053a980 (EAX = registry; stdcall driver; RET 4) */
void rx_net_player_state_clear(rx_addr st);                                    /* 007006e0 (EAX = the network player state) */
void rx_race_driver_vf2(rx_addr drv);                                          /* 0050ef80 RaceDriver::vf2 (thiscall) */
rx_addr rx_behavior_sdtor(rx_addr b, uint32_t flags);                          /* 0052aa00 Behavior::vf0 (thiscall; RET 4) -> this */
void rx_tracked_entry_remove(rx_addr e);                                       /* 0051ff30 (ESI = entry) */
rx_addr rx_behavior_air_leveling_sdtor(rx_addr b, uint32_t flags);             /* 0051f3b0 BehaviorAirLeveling::sdtor (thiscall; RET 4) -> this */
void rx_fix_forward_unlisten_wrecked(rx_addr channel, rx_addr b);              /* 0053b8b0 (EAX = channel; stdcall behaviour; RET 4) */
void rx_fix_forward_vf2(rx_addr b);                                            /* 00523460 BehaviorFixForwardProgress::vf2 (thiscall) */
rx_addr rx_race_driver_sdtor(rx_addr d, uint32_t flags);                       /* 00513530 RaceDriver::sdtor (thiscall; RET 4) -> this */
void rx_race_stub_destroy(rx_addr act);                                        /* 005659e0 MXActivityRaceStub's destructor (ECX = activity) */
rx_addr rx_race_sdtor(rx_addr act, uint32_t flags);                            /* 00564af0 MXActivityRace::sdtor (thiscall; RET 4) -> this */
#ifdef __cplusplus
}
#endif
