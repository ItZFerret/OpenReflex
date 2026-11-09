/* race_activity_stub.h -- MXActivityRaceStub / MXActivityRace virtuals of the race session that the event world runs before and during the intro
 * (race_activity_stub.c). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
void     rx_race_stub_vf17(rx_addr act);                    /* 005797b0 MXActivityRaceStub::vf17 (thiscall) */
void     rx_race_stub_vf18(rx_addr act);                    /* 00566d00 MXActivityRaceStub::vf18 (thiscall; ends in a jump to Activity::vf18) */
void     rx_race_stub_kind_setup(rx_addr act);              /* 00566da0 (ESI = the activity, EDI = 0) */
uint32_t rx_race_track_kind(rx_addr name);                  /* 0056a120 (stdcall the level name; RET 4) */
void     rx_race_clear_props(rx_addr act);                  /* 0056a0b0 (stdcall activity; RET 4) */
void     rx_race_stub_vf37(rx_addr act);                    /* 00568140 MXActivityRaceStub::vf37 / MXActivityRace::vf37 thunk 00565030 (thiscall; RET 4: the old state) */
void     rx_race_drivers_fade(rx_addr act);                 /* 0055a390 (stdcall activity; RET 4) */
void     rx_rider_anim_fade_default(rx_addr ra);            /* 00621570 (ECX = the rider animation object) */
void     rx_activity_set_track_level(rx_addr act, uint32_t level);   /* 0055e6e0 (EAX = activity, ECX = level) */
uint32_t rx_scenario_is_active(rx_addr sc);                 /* 0055eb40 BaseScenario::vf11 (thiscall) */
void     rx_race_stub_vf25(rx_addr act, float dt);          /* 005681e0 MXActivityRaceStub::vf25 (thiscall; RET 4) */
void     rx_race_stub_vf31(rx_addr act);                     /* 005798f0 MXActivityRaceStub::vf31 (RET 4) */
void     rx_race_stub_vf38(rx_addr act, uint32_t old_state); /* 00568340 MXActivityRaceStub::vf38 (RET 4) */
void     rx_race_vf38(rx_addr act, uint32_t old_state);      /* 00565040 MXActivityRace::vf38 (RET 4) */
void     rx_race_stub_reset_vehicle_maps(rx_addr act);       /* 005695b0 (ECX = the activity, stack: activity; RET 4) */
uint32_t rx_race_stub_vf48(rx_addr act);                    /* 0055eb60 MXActivityRaceStub::vf48 */
void     rx_activity_net_timer_event(rx_addr act, uint32_t kind, float v);   /* 0055cda0 (thiscall; RET 8) */
void     rx_game_reset_view_params(void);                  /* 00559b60 */
void     rx_activity_lap_completed(rx_addr act, rx_addr drv, int32_t lap, uint32_t flag, int32_t laps);   /* 005676e0 (thiscall; RET 0x10) */
#ifdef __cplusplus
}
#endif
