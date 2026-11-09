/* race_finish.h -- the race's finish / results chain (agent "raceend", 2026-10-20): the WindDown and Post states of the activity
 * (Activity::vf27 / vf29 / vf40 / vf41 and the MXActivityRace overrides), the post scenarios' event plumbing, the records the
 * Post enter writes, and the estimated gap of an AI still behind the last human (00567d50). Adapters in oracle/race_finish.cpp,
 * group "rx_race". */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

/* Activity::vf27 00559720 (WindDown update): clocks, then when +0x2c4 runs out and no page wipe is pending: the online result
 * report (session +0x1c4c / +0x1c48 only), the standings, and the "WinddownEnd" page wipe (0055e990). */
void rx_activity_wind_down(rx_addr act, float dt);
/* 00565260 MXActivityRace::vf27 (thunk to Activity::vf27). */
void rx_race_wind_down(rx_addr act, float dt);
/* 00559600 Activity::vf40 (WindDown enter) and 00568f60 MXActivityRaceStub::vf40. */
void rx_activity_wind_down_enter(rx_addr act);
void rx_race_stub_wind_down_enter(rx_addr act);
/* 00565500 MXActivityRace::vf34 (WindDown exit) and 00568fb0 MXActivityRaceStub::vf35 (Post exit). */
void rx_race_wind_down_exit(rx_addr act);
void rx_race_stub_post_exit(rx_addr act);
void rx_race_post_exit_thunk(rx_addr act);                                      /* 0057e870 jmp 00568fb0 */

/* 00559900 Activity::vf41 (Post enter) and 00568da0 MXActivityRaceStub::vf41 (records). */
void rx_activity_post_enter(rx_addr act);
void rx_race_stub_post_enter(rx_addr act);
/* 00569770 (EAX = activity): the finishing order kept in the standings (session state 7). */
void rx_race_post_enter_thunk(rx_addr act);                                     /* 00565560 jmp 00568da0 */
void rx_race_standings_reset(rx_addr act);
/* 00568d50 MXActivityRaceStub::vf29 (Post update) -> 00569a90 MXActivityRace_UpdatePost, 00569a40 (sub-state write). */
void rx_race_stub_post(rx_addr act, float dt);
void rx_race_update_post(rx_addr act, float dt);
void rx_post_state(rx_addr act, uint32_t state);

/* race_finish_highlights.c: 00569930 MXActivityRace_FinishHighlights (stdcall activity) -> 006d8530 MXHighlightTracking_OnRaceFinished
 * (stdcall tracking, place; ESI = player, EDI = controller) -> 006d7980; the body-panel checks 0047baf0 (ECX = damage behaviour),
 * 0047bed0 (ESI = damage manager, EDI = vehicle) and 0047bf00 (EAX = damage manager, EDI = vehicle). */
void rx_race_finish_highlights(rx_addr act);
void rx_highlight_on_race_finished(rx_addr tracking, uint32_t place, rx_addr player, uint32_t controller);
uint32_t rx_damage_panels_all_gone(rx_addr behaviour);
uint32_t rx_damage_vehicle_all_gone(rx_addr mgr, rx_addr vehicle);
uint32_t rx_damage_vehicle_none_lost(rx_addr mgr, rx_addr vehicle);

/* 0055a440 (each driver's player +0x214 = 0), 0055a470 (each vehicle +0x1010 / rider +0x5c4e0 = value). */
void rx_activity_players_zero(rx_addr act);
void rx_activity_result_flags(rx_addr act, uint32_t value);

/* 0055e990 (ECX = page name, stack activity): "ShowPageWipe" on the UI manager's channel and +0xff06c = 1. The product has no UI
 * manager: the dispatch is skipped there (PRODUCT BOUNDARY), the flag is written as the original does. */
void rx_show_page_wipe(rx_addr name, rx_addr act);
/* 0055e9e0 (ECX = activity, stack the transition's name): the UI page wipe's callback: "WinddownEnd" at state 3 enters Post
 * (SetState 4); otherwise the activity's vf60(name); +0xff06c cleared either way. This is what the product calls from 0055e990. */
void rx_page_wipe_transition(rx_addr act, rx_addr name);

/* 00587300 (EAX = manager, EDI = event name, stack the activity; RET 4): dispatch a 4-byte payload {activity}. */
void rx_event_activity(rx_addr mgr, rx_addr name, rx_addr act);
/* 00587ea0 (ECX = scenario string, EDX = event name, ESI = manager, stack the activity; RET 4): dispatch the 8-byte payload
 * {activity, scenario string} by its address in the frame. */
void rx_event_scenario(rx_addr str, rx_addr name, rx_addr mgr, rx_addr act);

/* 00567d50 (ESI = activity, EDI = driver, stack the last human in the standings; RET 4): an AI still racing behind the last
 * human gets its estimated gap (+0x10b4) from the remaining distance and its speed. */
void rx_race_estimate_gap(rx_addr act, rx_addr d, rx_addr human);

/* MXRecords the Post enter writes: 00764cb0 Placing, 00764d80 Score, 00764e50 BestLap, 00765020 AddTotalTime (ECX = records,
 * EAX = slot; the value on the stack; the Rdb columns TotalPlacing / BestPlace / TotalScore / BestScore / BestLapTime /
 * TotalTime). */
uint32_t rx_records_placing(rx_addr records, uint32_t slot, int32_t place);
uint32_t rx_records_score(rx_addr records, uint32_t slot, float score);
uint32_t rx_records_best_lap(rx_addr records, uint32_t slot, float lap);
void     rx_records_add_total_time(rx_addr ref, float seconds);

#ifdef __cplusplus
}
#endif
