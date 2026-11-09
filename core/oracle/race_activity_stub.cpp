// race_activity_stub.cpp -- oracle adapters for core/game/race_activity_stub.c (group rx_race).
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_activity_stub.h"
}
using namespace reflex_port;

// 005797b0 MXActivityRaceStub::vf17 (ECX = activity)
RX_PORT(0x005797b0, RaceStub_vf17, "rx_race", 0) { rx_race_stub_vf17(m.r[1]); return m.ret(0); }
// 00566d00 MXActivityRaceStub::vf18 (ECX = activity; ends in a jump to Activity::vf18, which the core runs as part of the function)
RX_PORT(0x00566d00, RaceStub_vf18, "rx_race", 0) { rx_race_stub_vf18(m.r[1]); return m.ret(0); }
// 00566da0 (ESI = activity, EDI = 0)
RX_PORT(0x00566da0, RaceStub_KindSetup, "rx_race", 0) { rx_race_stub_kind_setup(m.r[6]); return m.ret(0); }
// 0056a120 (stdcall the level name; RET 4) -> EAX
RX_PORT(0x0056a120, Race_TrackKind, "rx_race", OUT_EAX) { m.r[0] = rx_race_track_kind(m.arg(0)); return m.ret(4); }
// 0056a0b0 (stdcall activity; RET 4)
RX_PORT(0x0056a0b0, Race_ClearProps, "rx_race", 0) { rx_race_clear_props(m.arg(0)); return m.ret(4); }
// 00568140 MXActivityRaceStub::vf37 (ECX = activity, stack the old state; RET 4); the MXActivityRace slot 00565030 is a one-instruction jump to it
RX_PORT(0x00568140, RaceStub_vf37, "rx_race", 0) { rx_race_stub_vf37(m.r[1]); return m.ret(4); }
// 0055a390 (stdcall activity; RET 4)
RX_PORT(0x0055a390, Race_DriversFade, "rx_race", 0) { rx_race_drivers_fade(m.arg(0)); return m.ret(4); }
// 00621570 (ECX = the rider animation object)
RX_PORT(0x00621570, RiderAnim_FadeDefault, "rx_race", 0) { rx_rider_anim_fade_default(m.r[1]); return m.ret(0); }
// 0055e6e0 (EAX = activity, ECX = level)
RX_PORT(0x0055e6e0, Activity_SetTrackLevel, "rx_race", 0) { rx_activity_set_track_level(m.r[0], m.r[1]); return m.ret(0); }
// 0055eb40 BaseScenario::vf11 (ECX = scenario) -> EAX
RX_PORT(0x0055eb40, Scenario_IsActive, "rx_race", OUT_EAX) { m.r[0] = rx_scenario_is_active(m.r[1]); return m.ret(0); }
// 005681e0 MXActivityRaceStub::vf25 (ECX = activity, stack dt; RET 4)
RX_PORT(0x005681e0, RaceStub_vf25, "rx_race", 0) { rx_race_stub_vf25(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
// 0055eb60 MXActivityRaceStub::vf48 (ECX = activity) -> EAX
RX_PORT(0x0055eb60, RaceStub_vf48, "rx_race", OUT_EAX) { m.r[0] = rx_race_stub_vf48(m.r[1]); return m.ret(0); }
// 0055cda0 (ECX = activity; stack kind, float; RET 8)
RX_PORT(0x0055cda0, Activity_NetTimerEvent, "rx_race", 0) { rx_activity_net_timer_event(m.r[1], m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(8); }
// 00559b60 (no arguments)
RX_PORT(0x00559b60, Game_ResetViewParams, "rx_race", 0) { rx_game_reset_view_params(); return m.ret(0); }
// 005676e0 (ECX = activity; stack driver, lap, flag, laps; RET 0x10)
RX_PORT(0x005676e0, Activity_LapCompleted, "rx_race", 0) { rx_activity_lap_completed(m.r[1], m.arg(0), int32_t(m.arg(1)), m.arg(2), int32_t(m.arg(3))); return m.ret(0x10); }
