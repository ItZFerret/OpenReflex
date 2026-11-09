// race_finish.cpp -- oracle adapters for core/game/race_finish.c (the WindDown / Post states, the page-wipe callback, the post
// scenario events, the estimated gap and the records the Post enter writes). Group "rx_race".
#include "race_oracle.h"
extern "C" {
#include "../game/race.h"
#include "../game/race_finish.h"
}
using namespace reflex_port;

// the x87 register file is not part of the C model: keep it as the original leaves it (the adapters of race_activity.cpp do the
// same under the name ActFpuKeep; this file needs its own name).
struct FinFpuKeep { reflex_vm::Machine& m; std::vector<reflex_f80::F80> fpu; explicit FinFpuKeep(reflex_vm::Machine& mm) : m(mm), fpu(mm.fpu) {} ~FinFpuKeep() { m.fpu = fpu; } };

RX_PORT(0x00559720, Activity_WindDown, "rx_race", 0) { FinFpuKeep k(m); rx_activity_wind_down(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00565260, Race_WindDown, "rx_race", 0) { FinFpuKeep k(m); rx_race_wind_down(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00559600, Activity_WindDownEnter, "rx_race", 0) { FinFpuKeep k(m); rx_activity_wind_down_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x00568f60, RaceStub_WindDownEnter, "rx_race", 0) { FinFpuKeep k(m); rx_race_stub_wind_down_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x00565500, Race_WindDownExit, "rx_race", 0) { rx_race_wind_down_exit(m.r[1]); return m.ret(4); }
RX_PORT(0x0057e870, Race_PostExitThunk, "rx_race", 0) { FinFpuKeep k(m); rx_race_post_exit_thunk(m.r[1]); return m.ret(4); }
RX_PORT(0x00568fb0, RaceStub_PostExit, "rx_race", 0) { FinFpuKeep k(m); rx_race_stub_post_exit(m.r[1]); return m.ret(4); }
RX_PORT(0x00559900, Activity_PostEnter, "rx_race", 0) { FinFpuKeep k(m); rx_activity_post_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x00569770, Race_StandingsReset, "rx_race", 0) { rx_race_standings_reset(m.r[0]); return m.ret(0); }
RX_PORT(0x00565560, Race_PostEnterThunk, "rx_race", 0) { FinFpuKeep k(m); rx_race_post_enter_thunk(m.r[1]); return m.ret(4); }
RX_PORT(0x00568da0, RaceStub_PostEnter, "rx_race", 0) { FinFpuKeep k(m); rx_race_stub_post_enter(m.r[1]); return m.ret(4); }
RX_PORT(0x00568d50, RaceStub_Post, "rx_race", 0) { FinFpuKeep k(m); rx_race_stub_post(m.r[1], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00569a90, Race_UpdatePost, "rx_race", 0) { FinFpuKeep k(m); rx_race_update_post(m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00569a40, Race_PostState, "rx_race", 0) { FinFpuKeep k(m); rx_post_state(m.r[6], m.r[0]); return m.ret(0); }
RX_PORT(0x0055a440, Activity_PlayersZero, "rx_race", 0) { rx_activity_players_zero(m.r[1]); return m.ret(0); }
RX_PORT(0x0055a470, Activity_ResultFlags, "rx_race", 0) { rx_activity_result_flags(m.r[2], m.r[6]); return m.ret(0); }
RX_PORT(0x0055e990, Activity_ShowPageWipe, "rx_race", 0) { rx_show_page_wipe(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x0055e9e0, Activity_PageWipeTransition, "rx_race", 0) { FinFpuKeep k(m); rx_page_wipe_transition(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587300, Event_Activity, "rx_race", 0) { rx_event_activity(m.r[0], m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00587ea0, Event_Scenario, "rx_race", 0) { rx_event_scenario(m.r[1], m.r[2], m.r[6], m.arg(0)); return m.ret(4); }
RX_PORT(0x00567d50, Race_EstimateGap, "rx_race", 0) { FinFpuKeep k(m); rx_race_estimate_gap(m.r[6], m.r[7], m.arg(0)); return m.ret(4); }
RX_PORT(0x00764cb0, Records_Placing, "rx_race", OUT_EAX) { FinFpuKeep k(m); m.r[0] = rx_records_placing(m.r[1], m.r[0], int32_t(m.arg(0))); return m.ret(4); }
RX_PORT(0x00764d80, Records_Score, "rx_race", OUT_EAX) { FinFpuKeep k(m); m.r[0] = rx_records_score(m.r[1], m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00764e50, Records_BestLap, "rx_race", OUT_EAX) { FinFpuKeep k(m); m.r[0] = rx_records_best_lap(m.r[1], m.r[0], rx_oracle::f32(m.arg(0))); return m.ret(4); }
RX_PORT(0x00765020, Records_AddTotalTime, "rx_race", 0) { FinFpuKeep k(m); rx_records_add_total_time(m.arg(0), rx_oracle::f32(m.arg(1))); return m.ret(0); }

// race_finish_highlights.c: the race finish's highlight progress (00569930 -> 006d8530 -> 006d7980) and the body-panel checks.
RX_PORT(0x00569930, Race_FinishHighlights, "rx_race", 0) { FinFpuKeep k(m); rx_race_finish_highlights(m.arg(0)); return m.ret(4); }
RX_PORT(0x006d8530, HighlightTracking_OnRaceFinished, "rx_race", 0) { rx_highlight_on_race_finished(m.arg(0), m.arg(1), m.r[6], m.r[7]); return m.ret(8); }
RX_PORT(0x0047baf0, Damage_PanelsAllGone, "rx_race", OUT_EAX) { m.r[0] = rx_damage_panels_all_gone(m.r[1]); return m.ret(0); }
RX_PORT(0x0047bed0, Damage_VehicleAllGone, "rx_race", OUT_EAX) { m.r[0] = rx_damage_vehicle_all_gone(m.r[6], m.r[7]); return m.ret(0); }
RX_PORT(0x0047bf00, Damage_VehicleNoneLost, "rx_race", OUT_EAX) { m.r[0] = rx_damage_vehicle_none_lost(m.r[0], m.r[7]); return m.ret(0); }
