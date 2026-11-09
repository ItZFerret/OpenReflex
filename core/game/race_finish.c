#include "replay_clip.h"
/* race_finish.c -- the race's finish / results chain (agent "raceend", 2026-10-20). Hand transliterations of the activity's
 * WindDown and Post states and of the records the Post enter writes; see race_finish.h and core/notes/race.md "raceend".
 *
 * PRODUCT BOUNDARIES (oracle the original, product nothing; nothing gameplay reads them):
 *  - 006d8420 (MXHighlightTracking_OnRaceStarted) in Activity::vf41: its highlights (0x17 .. 0x1d) are online-only
 *    (MXGame_PlayerMode == 2), nothing the offline product reads; the race-finish highlights (00569930 -> 006d8530 -> 006d7980,
 *    with the body-panel checks 0047baf0 / 0047bed0 / 0047bf00) are ported in race_finish_highlights.c;
 *  - 006aa8d0 ClipRecorder_StartPlayback remains the existing presentation boundary.
 *    StopPlayback006aa990 now uses its complete native owner; replay activation remains unported;
 *  - the UI manager's page wipe: the product has no UI manager ([0xa93fcc] = 0), so 0055e990 cannot raise "ShowPageWipe" on
 *    its channel and the wipe animation does not exist. The product runs the game's own page-wipe callback 0055e9e0 at the
 *    request instead (every page: "WinddownEnd" and the scenarios' "Scenario" fades): that callback is what the UI raises when the wipe ends, it is the only UI
 *    consumer of the wipe, and nothing gameplay reads the animation. +0xff06c is written / cleared by the same two functions
 *    as the original;
 *  The RaceFinishScenario (vtable 0x009433d4: 0074a1e0 / 0074a340 / 0074a540, anim_scenario_finish.c) is ported and runs in the
 *  product (w-podium): Post sub-state 1 plays the finish replay and the podium as the original. */
#include "race.h"
#include "waypoint_playback.h"
#include "rider_cap.h"
#ifndef RX_ORACLE
#include "career_owned_results.h"
#endif
#include "crt.h"
#include "race_extern.h"
#include "race_activity.h"
#include "race_driver.h"
#include "race_finish.h"
#include "waypoint_activity.h"
#include "bike_sim.h"
#include "fx.h"
#include "world_veh.h"
#include "loop_world.h"
#include "loop_player.h"
#include "build_tables.h"
#include "build_vehicle.h"
#include "build_activity.h"
#include "build_ui_world.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim_scenario_race_intro.h"

/* ---- boundaries ------------------------------------------------------------------------------------------------------------- */

static void highlight_start(rx_addr tracking, rx_addr player, uint32_t index) {
#ifdef RX_ORACLE
    const uint32_t a[3] = {(uint32_t)tracking, (uint32_t)player, index};
    rx_call_original(0x006d8420, RX_STDCALL, 0, 3, a);
#else
    (void)tracking; (void)player; (void)index;
#endif
}
/* 00569930 MXActivityRace_FinishHighlights (race_finish_highlights.c): the finished humans' highlight progress, ported. */
static void finish_highlights(rx_addr act) {
    RACE_CORE_CALL(1, rx_race_finish_highlights(act));
}
static void clip_start_playback(uint32_t type) {
#ifdef RX_ORACLE
    const uint32_t a[1] = {type};
    rx_call_original(0x006aa8d0, RX_STDCALL, 0, 1, a);
#else
    RACE_CORE_CALL(1,rx_replay_start((int32_t)type));
#endif
}
static void clip_stop_playback(void) {
#ifdef RX_ORACLE
    rx_call_original(0x006aa990, RX_CDECL, 0, 0, 0);
#else
    RACE_CORE_CALL(0, rx_clip_recorder_stop_playback());
#endif
}
/* 00559a40 (EAX = activity): "Show Activity Results" with the activity as its 4-byte payload: the registered port
 * (waypoint_activity_runtime.c), which passes the payload's address and size to 004020c0 as the original does (push 4; lea eax,
 * [esp + 0xc]; push eax). The career's results owner sees the event inside that dispatch (career_owned_results.c, product only). */
static void show_activity_results(rx_addr act) {
    RACE_CORE_CALL(0, rx_waypoint_show_results(act));
}

/* the 12-byte MXRecords ref {row, 0, table} on the game stack (the callees read guest memory, never a host pointer) */
static void records_add_int(rx_addr table, uint32_t row, int32_t delta, rx_addr name) {
    const rx_addr s = rx_sp();
    rx_set_sp(s - 0x10);
    SU32(s, 0, row); SU32(s, 4, 0); SU32(s, 8, table);
    RACE_CORE_CALL(3, rx_records_add_int(s, delta, name));
    rx_set_sp(s);
}
static void records_add_total_time(rx_addr table, uint32_t row, float seconds) {
    const rx_addr s = rx_sp();
    rx_set_sp(s - 0x10);
    SU32(s, 0, row); SU32(s, 4, 0); SU32(s, 8, table);
    RACE_CORE_CALL(2, rx_records_add_total_time(s, seconds));
    rx_set_sp(s);
}

/* ---- the WindDown state ----------------------------------------------------------------------------------------------------- */

/* 00559720 Activity::vf27 (ECX = activity, arg dt; RET 4): the clocks (+0x294, +0x2a4 up, +0x2c4 down). Once +0x2c4 <= 0 and
 * no page wipe is pending (+0xff06c == 0) and the network replay flag is clear: online sessions (session +0x1c4c, +0x1b0 != 1,
 * +0x1c48) sort the drivers +0x2d4 by the activity's comparator (vf43 (a, b, 1)) and report their player +0xc into the session's
 * vf29; then the standings and the "WinddownEnd" page wipe (0055e990 with ECX = the name). Frame: sub 0x34; push ebx / esi / edi. */
void rx_activity_wind_down(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0x34, 3);
    SF32(act, 0x294, rx_adds(F32(act, 0x294), dt));
    SF32(act, 0x2a4, rx_adds(F32(act, 0x2a4), dt));
    SF32(act, 0x2c4, rx_subs(F32(act, 0x2c4), dt));
    race_static_dcf304();
    const rx_addr sess = U32(0, 0xda6cb0);
    if (U32(sess, 0x1c48) != 0) SU32(act, 0xfdc, 0);
    if (U32(act, 0xfdc) == 0 && F32(act, 0x2c4) <= 0.0f && U32(act, 0xff06c) == 0) {
        rx_addr root;
        RACE_CORE_CALL(0, root = rx_fx_session_manager());
        const rx_addr session = U32(root, 0x10);
        if (U32(session, 0x1c4c) != 0 && U32(session, 0x1b0) != 1) {
            rx_addr root2;
            RACE_CORE_CALL(0, root2 = rx_fx_session_manager());
            if (U32(U32(root2, 0x10), 0x1c48) != 0) {
                const int32_t n = I32(act, 0x26c);
                if (n > 1) {
                    /* the original's insertion sort over the drivers +0x2d4 (the comparator vf43 (prev, key, 1)) */
                    for (int32_t i = 1; i < n; i++) {
                        const uint32_t key = rx_stand(act, ((uint32_t)i) + 1);
                        int32_t j = i - 1;
                        while (j >= 0) {
                            const uint32_t prev = rx_stand(act, ((uint32_t)j) + 1);
                            const int32_t cmp = (int32_t)race_thiscall(race_vfn(act, 43), act, 3, prev, key, 1, 0);
                            if (cmp <= 0) break;
                            rx_stand_set(act, ((uint32_t)(j + 1)) + 1, prev);
                            j--;
                        }
                        rx_stand_set(act, ((uint32_t)(j + 1)) + 1, key);
                    }
                }
                RACE_CORE_CALL(0, rx_activity_sort_standings(act));
                /* the drivers' player +0xc (their controller slots) to the session's vf29. The original's buffer is in its
                 * 0x34-byte frame and this is the online-only branch; a C local stands in (NOT REACHED offline). */
                const rx_addr ss = rx_sp();
                rx_set_sp(ss - 0x40);
                const int32_t m = I32(act, 0x26c);
                for (int32_t i = 0; i < m && i < 16; i++)
                    SU32(ss, 4 * (uint32_t)i,
                         U32(U32(U32(act, 0x268), 4 * (uint32_t)i), 0x74) != 0
                             ? U32(U32(U32(U32(act, 0x268), 4 * (uint32_t)i), 0x74), 0xc) : 0);
                const rx_addr g = U32(0, 0xda6cb0);
                race_thiscall(race_vfn(g, 29), g, 2, (uint32_t)ss, (uint32_t)m, 0, 0);
                rx_set_sp(ss);
            }
        }
        RACE_CORE_CALL(0, rx_activity_sort_standings(act));
        RACE_CORE_CALL(1, rx_show_page_wipe(0x00941de8 /* "WinddownEnd" */, act));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559720, rx_activity_wind_down)

/* 00565260 MXActivityRace::vf27: `fld [esp+4]; push ecx; fstp [esp]; call Activity::vf27; ret 4`. */
void rx_race_wind_down(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(F, 0, 0);   /* the pushed slot the thunk writes dt into */
    RACE_CORE_CALL(1, rx_activity_wind_down(act, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00565260, rx_race_wind_down)

/* 00559600 Activity::vf40 (ECX = activity, arg old state; RET 4): +0x2c4 = 3 s, the UI's loading page resumed
 * (MXGame_ResumeFromUI(1) when the UI manager has +0x288), +0xfdc = 0; online sessions: the session clock (+0x294), every
 * driver's vf4 / vf14, and +0xfdc = 1 unless the replay recorder is on. Frame: push ebx / esi / edi. */
void rx_activity_wind_down_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 3);
    SF32(act, 0x2c4, 3.0f /* 0x92fe48 */);
    const rx_addr ui = U32(0, 0xa93fcc);   /* PRODUCT BOUNDARY: the UI manager is 0 there (the original's +0x288 page test) */
    if (ui != 0 && U32(ui, 0x288) != 0) RACE_CORE_CALL(1, rx_game_resume_from_ui(U32(0, 0xa946b8), 1));
    race_static_dcf304();
    SU32(act, 0xfdc, 0);
    const rx_addr sess = U32(0, 0xda6cb0);
    if (U32(sess, 0x1c4c) != 0 && U32(sess, 0x1b0) != 1) {
        race_static_dcf304();
        RACE_CORE_CALL(1, rx_fx_session_clock(0, F32(act, 0x294)));
        for (int32_t i = 0; i < I32(act, 0x26c); i++) {
            const rx_addr d = U32(U32(act, 0x2d0), 4 * (uint32_t)i);
            race_thiscall(race_vfn(d, 4), d, 0, 0, 0, 0, 0);
            race_thiscall(race_vfn(d, 14), d, 0, 0, 0, 0, 0);
        }
        race_static_dcf304();
        if (U32(U32(0, 0xda6cb0), 0x1c48) == 0) SU32(act, 0xfdc, 1);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559600, rx_activity_wind_down_enter)

/* 00568f60 MXActivityRaceStub::vf40 (ECX = activity, arg old state; RET 4): Activity::vf40, then with session states 1 / 5 / 6 /
 * 7 / 8 the leader's gap (+0x10b4) cleared. */
void rx_race_stub_wind_down_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_wind_down_enter(act));
    const uint32_t ss = U32(act, 0x238);
    if (ss == 1 || ss == 5 || ss == 6 || ss == 7 || ss == 8) SF32(U32(act, 0x2d0), 0x10b4, 0.0f);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568f60, rx_race_stub_wind_down_enter)

/* 00565500 MXActivityRace::vf34 (ECX = activity, arg new state; RET 4): the recorded replay's activity clock into 0xa942a4
 * (0xa942b4 cleared), +0xff080 = 5 and the UI's 0x9c3edc = 5 (0 when the level's +0x22148 is set). */
void rx_race_wind_down_exit(rx_addr act) {
    if (U32(0, 0xa942b4) == 1) {
        const rx_addr a = U32(0, 0xa942b8);
        const float t = F32(a, 0x294);
        SU32(0, 0xa942b4, 0);
        SF32(0, 0xa942a4, t);
    }
    const rx_addr w = U32(act, 0x234);
    SU32(act, 0xff080, 5);
    uint32_t v = 5;
    if (w != 0) v = U32(w, 0x22148) != 0 ? 5u : 0u;   /* neg / sbb: 5 only when the level's +0x22148 is set */
    SU32(0, 0x9c3edc, v);
}
RX_FUNCTION(0x00565500, rx_race_wind_down_exit)

/* ---- the Post state --------------------------------------------------------------------------------------------------------- */

/* 0055a440 (ECX = activity): every driver's player +0x214 = 0. */
void rx_activity_players_zero(rx_addr act) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++)
        SU32(U32(U32(U32(act, 0x268), 4 * (uint32_t)i), 0x74), 0x214, 0);
}
RX_FUNCTION(0x0055a440, rx_activity_players_zero)

/* 0055a470 (EDX = activity, ESI = value): every driver's vehicle +0x1010 and rider +0x5c4e0 = value. */
void rx_activity_result_flags(rx_addr act, uint32_t value) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr p = U32(U32(U32(act, 0x268), 4 * (uint32_t)i), 0x74);
        SU32(U32(p, 0x1ac), 0x1010, value);
        SU32(U32(p, 0x1b4), 0x5c4e0, value);
    }
}
RX_FUNCTION(0x0055a470, rx_activity_result_flags)

/* 00559900 Activity::vf41 (ECX = activity, arg old state; RET 4): the players' +0x214 cleared, the controls off (0055a1e0
 * with 0), then per driver: the highlight start (006d8420, PRODUCT BOUNDARY), the vehicle's model +0x148 = 0xff and its vf32(1),
 * the rider's +0x5b200 / +0x5bb64 / +0x5b6d0 / +0xae70, the player's +0x11a0 = 0 and the driver's vf4; +0xff064 = 0.
 * Frame: sub 8; push ebx / ebp / esi / edi. */
void rx_activity_post_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 8, 3);
    RACE_CORE_CALL(0, rx_activity_players_zero(act));
    RACE_CORE_CALL(1, rx_activity_enable_controls(0, act));
    const rx_addr save = rx_sp();
    if (I32(act, 0x26c) > 0) rx_set_sp(save - 4);   /* push edi (inside the n > 0 branch) */
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
        const rx_addr p = U32(d, 0x74);
        if (U32(p, 4) != 0 && U32(p, 0x10) == 0 && I32(p, 0x1f0) >= 0)
            highlight_start(U32(0, 0xd92edc), p, (uint32_t)i);
        const rx_addr veh = U32(p, 0x1ac);
        if (veh != 0) {
            SU8(U32(veh, 0x1328), 0x148, 0xff);
            race_thiscall(race_vfn(veh, 32), veh, 1, 1, 0, 0, 0);
        }
        const rx_addr rider = U32(p, 0x1b4);
        if (rider != 0) {
            SU8(rider, 0x5b200, 0xff);
            SU8(rider, 0x5bb64, 1);
            SU32(rider, 0x5b6d0, 0);
            SU32(rider, 0xae70, 0);
        }
        SU32(p, 0x11a0, 0);
        race_thiscall(race_vfn(d, 4), d, 0, 0, 0, 0, 0);
    }
    rx_set_sp(save);
    SF32(act, 0xff064, 0.0f);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559900, rx_activity_post_enter)

/* 00569770 (EAX = activity): the standings' finishing order kept: every driver's player +0x11c0 = 12 - i, +0x11bc = i,
 * the activity's +0x13e60c[i] = the driver, +0x11c4 = 12 - i (the first n-1), then the standings +0x2d0[i] = +0x13e60c[i]. */
void rx_race_standings_reset(rx_addr act) {
    const int32_t n = I32(act, 0x26c);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
        const rx_addr p = U32(d, 0x74);
        SU32(p, 0x11c0, (uint32_t)(12 - i));
        SU32(p, 0x11bc, (uint32_t)i);
        SU32(act, 0x13e60c + 4 * (uint32_t)i, (uint32_t)d);
    }
    for (int32_t i = 0; i + 1 < n; i++)
        SU32(U32(U32(act, 0x13e60c + 4 * (uint32_t)i), 0x74), 0x11c4, (uint32_t)(12 - i));
    for (int32_t i = 0; i + 1 < n; i++)
        rx_stand_set(act, (uint32_t)i, U32(act, 0x13e60c + 4 * (uint32_t)i));
}
RX_FUNCTION(0x00569770, rx_race_standings_reset)

/* 00568da0 MXActivityRaceStub::vf41 (ECX = activity, arg old state; RET 4): Activity::vf41; with session state 7 the standings
 * reordered (00569770); the highlights (00569930, PRODUCT BOUNDARY); then for each of the +0x300 finishers in the standings
 * (+0x2d0): TotalEvents +1, MXRecords_Placing(i + 1), TotalLaps = the race's laps, TotalTime (00512870 or the replay's
 * +0x1124), MXRecords_Score (the player's +0xcec), MXRecords_BestLap (the driver's +0x10c4); +0x13e640 = 0. Frame: sub 0x28;
 * push ebx / ebp / (the old state) / esi / edi. */
void rx_race_stub_post_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0x28, 3);
    RACE_CORE_CALL(1, rx_activity_post_enter(act));
    if (U32(act, 0x238) == 7) RACE_CORE_CALL(0, rx_race_standings_reset(act));
    finish_highlights(act);
    const rx_addr save = rx_sp();
    if (I32(act, 0x300) > 0) rx_set_sp(save - 8);   /* push esi / edi (inside the loop's branch) */
    const rx_addr records = U32(0, 0xa946c0);
    const int32_t finishers = I32(act, 0x300);
    for (int32_t i = 0; i < finishers; i++) {
        const rx_addr d = rx_stand(act, (uint32_t)i);
        const rx_addr p = U32(d, 0x74);
        if (U32(p, 4) == 0 || U32(p, 0x10) != 0) continue;
        const int32_t pad = I32(p, 0x1f0);
        if (pad == -1) continue;
        const rx_addr e = records + 0x18 * (uint32_t)pad;
        const uint32_t row = U32(e, 4);
        if (row != 0xffffffffu) records_add_int(U32(e, 0), row, 1, 0x0096d99c /* "TotalEvents" */);
        uint32_t placed = 0;
        RACE_CORE_CALL(1, placed = rx_records_placing(records, (uint32_t)pad, i + 1));
        (void)placed;
        if (pad != -1) {
            const rx_addr e2 = records + 0x18 * (uint32_t)pad;
            const uint32_t row2 = U32(e2, 4);
            if (row2 != 0xffffffffu) records_add_int(U32(e2, 0), row2, (int32_t)U32(act, 0xff070), 0x0096d990 /* "TotalLaps" */);
        }
        float total;
        if (U32(U32(p, 0x94), 0x238) == 5 && I32(d, 0x1120) != -1) total = F32(d, 0x1124);
        else { RACE_CORE_CALL(0, total = rx_driver_total_time(d)); }
        if (pad != -1) {
            const rx_addr e3 = records + 0x18 * (uint32_t)pad;
            const uint32_t row3 = U32(e3, 4);
            if (row3 != 0xffffffffu) records_add_total_time(U32(e3, 0), row3, total);
        }
        uint32_t sc = 0;
        RACE_CORE_CALL(1, sc = rx_records_score(records, (uint32_t)pad, F32(p, 0xcec)));
        (void)sc;
        uint32_t bl = 0;
        RACE_CORE_CALL(1, bl = rx_records_best_lap(records, (uint32_t)pad, F32(d, 0x10c4)));
        (void)bl;
    }
    rx_set_sp(save);
    SU32(act, 0x13e640, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568da0, rx_race_stub_post_enter)

/* 00565560 MXActivityRace::vf41 (`jmp 00568da0`): the Post enter's vtable slot for the race class; direct (jump). */
void rx_race_post_enter_thunk(rx_addr act) { rx_race_stub_post_enter(act); }
RX_FUNCTION(0x00565560, rx_race_post_enter_thunk)

/* 0057e870 MXActivityRace::vf35 (`jmp 00568fb0`): the Post exit's vtable slot for the race class. The core function is called
 * directly (a jump adds no return address). */
void rx_race_post_exit_thunk(rx_addr act) { rx_race_stub_post_exit(act); }
RX_FUNCTION(0x0057e870, rx_race_post_exit_thunk)

/* 00568fb0 MXActivityRaceStub::vf35 (ECX = activity, arg new state; RET 4): the controls on (0055a1e0 with 1), the world's
 * +0x21de0 = 0, the replay cameras updated (0.033 s), 0xa941b8 / 0xa942b8 = 0, the replay playback stopped (006aa990, PRODUCT
 * BOUNDARY), the podium released (0074b180 with 1) and the result flags cleared (0055a470 with 0). Frame: push esi / edi. */
void rx_race_stub_post_exit(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 2);
    RACE_CORE_CALL(1, rx_activity_enable_controls(1, act));
    const rx_addr w = U32(act, 0x234);
    SU32(w, 0x21de0, 0);
    RACE_CORE_CALL(2, rx_replay_cameras_update(w, 0.032999999821186066f /* 0x977818 */));
    SU32(0, 0xa941b8, 0);
    clip_stop_playback();
    SU32(0, 0xa942b8, 0);
    RACE_CORE_CALL(1, rx_podium_release(w + 0x20150, 1));
    RACE_CORE_CALL(0, rx_activity_result_flags(act, 0));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568fb0, rx_race_stub_post_exit)

/* 00569a40 (EAX = the Post sub-state, ESI = activity): +0x13e640 = the state; state 2 starts the replay playback at clip type 8
 * (006aa8d0, PRODUCT BOUNDARY) and raises "Show Activity Results" with the activity as its payload. */
void rx_post_state(rx_addr act, uint32_t state) {
    RACE_FRAME_PLAIN(F, 0, 3);
    SU32(act, 0x13e640, state);
    if (state == 2) {
        clip_start_playback(8);
        show_activity_results(act);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569a40, rx_post_state)

/* 00569a90 MXActivityRace_UpdatePost (EAX = activity, ESI = activity, stack dt; RET 4): the Post sub-state +0x13e640:
 *  0: the finish scenario (activity +0x114260) is started (podium setup 0074ada0, 00747810); if it starts, "Post Scenarios
 *     Start" ({ "Finish", activity }) and sub-state 1; else sub-state 2 (the results reel).
 *  1: the finish scenario updated (007473f0) until its vf8 says done; then "Post Scenarios End" and sub-state 2.
 *  2: the result flags set (0055a470 with 1).
 *  3: the results scenario (activity +0x129430) updated until done; then "Post Scenarios End" ({ "Results", activity }), the
 *     podium released (0074b180 with 1) and "Post State Complete".
 * Frame: push ebp; and esp, -8; push esi / edi. */
void rx_race_update_post(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 0, 2);
    switch (U32(act, 0x13e640)) {
    case 0: {
        const uint32_t ready = race_thiscall(race_vfn(act, 12), act, 0, 0, 0, 0, 0);
        if (ready == 0) { RACE_CORE_CALL(0, rx_post_state(act, 2)); break; }
        RACE_CORE_CALL(0, rx_podium_setup(act, U32(act, 0x234) + 0x20150));
        const rx_addr sc = act + 0x114260;
        uint32_t started;
        RACE_CORE_CALL(0, started = rx_scenario_start(sc));
        if (started == 0) { RACE_CORE_CALL(0, rx_post_state(act, 2)); break; }
        RACE_CORE_CALL(1, rx_event_scenario(0x00941eac /* "Finish" */, 0x00941c04 /* "Post Scenarios Start" */, U32(act, 0x290), act));
        SU32(act, 0x13e640, 1);
        break;
    }
    case 1: {
        RACE_CORE_CALL(2, rx_scenario_intro_update(act + 0x114260, dt));
        const rx_addr sc = act + 0x114260;
        if (race_thiscall(race_vfn(sc, 8), sc, 0, 0, 0, 0, 0) == 0) break;
        RACE_CORE_CALL(1, rx_event_scenario(0x00941eac /* "Finish" */, 0x00941c1c /* "Post Scenarios End" */, U32(act, 0x290), act));
        RACE_CORE_CALL(0, rx_post_state(act, 2));
        break;
    }
    case 2:
        RACE_CORE_CALL(0, rx_activity_result_flags(act, 1));
        break;
    case 3: {
        RACE_CORE_CALL(2, rx_scenario_intro_update(act + 0x129430, dt));
        const rx_addr sc = act + 0x129430;
        if (race_thiscall(race_vfn(sc, 8), sc, 0, 0, 0, 0, 0) == 0) break;
        RACE_CORE_CALL(1, rx_event_scenario(0x00941fd8 /* "Results" */, 0x00941c1c /* "Post Scenarios End" */, U32(act, 0x290), act));
        RACE_CORE_CALL(2, rx_podium_release(U32(act, 0x234) + 0x20150, 1));
        RACE_CORE_CALL(1, rx_event_activity(U32(act, 0x290), 0x00941c48 /* "Post State Complete" */, act));
        break;
    }
    default:
        break;
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569a90, rx_race_update_post)

/* 00568d50 MXActivityRaceStub::vf29 (ECX = activity, arg dt; RET 4): while the Post clock +0xff064 <= 0, when the activity's
 * vf57 says so the results page is raised (00559a40); the clock += dt; 00569a90. Frame: push esi. */
void rx_race_stub_post(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (F32(act, 0xff064) <= 0.0f) {
        if (race_thiscall(race_vfn(act, 57), act, 0, 0, 0, 0, 0) != 0) show_activity_results(act);
    }
    SF32(act, 0xff064, rx_adds(F32(act, 0xff064), dt));
    RACE_CORE_CALL(1, rx_race_update_post(act, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568d50, rx_race_stub_post)

/* ---- the UI page wipe (product boundary) ------------------------------------------------------------------------------------- */

/* 0055e990 (ECX = the page's name, stack the activity; RET 4): the UI manager's "ShowPageWipe" event with the name as its 4-byte
 * payload (EAX = the manager's channel +0x98, EDI = "ShowPageWipe"), then the activity's +0xff06c = 1. Frame: sub 8; push
 * esi / edi. */
void rx_show_page_wipe(rx_addr name, rx_addr act) {
    RACE_FRAME_PLAIN(F, 8, 2);
#ifdef RX_ORACLE
    const rx_addr ui = U32(0, 0xa93fcc);
    if (ui != 0) {
        const rx_addr ch = U32(ui, 0x98);
        SU32(F, 0, (uint32_t)name);
        rx_regs in; memset(&in, 0, sizeof in); in.r[0] = ch; in.r[7] = 0x00941e9c;
        uint32_t a[2] = {F, 4};
        rx_call_original_ex(0x004020c0, &in, 0, 2, a);
    }
#else
    /* the manager's channel [0xa93fcc] + 0x98: no UI manager, but its event channel exists (build_ui_world.c), so the event is
     * raised as the original does - to its (empty) handler list. Payload: the name's slot (the original's lea of [esp+0x10]). */
    SU32(F, 0, (uint32_t)name);
    RACE_CORE_CALL(2, rx_world_event_dispatch(rx_ui_event_channel(), 0x00941e9c /* "ShowPageWipe" */, F, 4));
#endif
    SU32(act, 0xff06c, 1);
#ifndef RX_ORACLE
    /* PRODUCT BOUNDARY (the file header): no UI manager, so no "ShowPageWipe" channel and no wipe animation. Run the game's
     * own page-wipe callback (0055e9e0) at the request, as the original's UI does when its wipe ends: "WinddownEnd" enters Post
     * exactly as the original's UI would after the wipe; any other page ("Scenario": the scenarios' fade-out near the end of a
     * step, anim_scenario_intro.c) gets the activity's vf60(name) - SharedVf_Ret4 00679000, empty, on every activity - and the
     * pending flag +0xff06c cleared. Without the clear the flag stayed set after the first scenario fade for the rest of the
     * activity, and IntroScenario_Update (007473f0) ignores SkipToNextScenario while it is set: the finish / podium scenarios
     * after a race could not be skipped (w-uibatch). */
    RACE_CORE_CALL(1, rx_page_wipe_transition(act, name));
#endif
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055e990, rx_show_page_wipe)

/* 0055e9e0 (ECX = activity, stack the transition's name; RET 4): the inline strcmp against "WinddownEnd" (crt_strcmp: -1 / 0 /
 * 1); equal at state 3 -> SetState(4); otherwise the activity's vf60(name); +0xff06c = 0 either way. Frame: push esi / edi. */
void rx_page_wipe_transition(rx_addr act, rx_addr name) {
    RACE_FRAME_PLAIN(F, 0, 2);
    if (crt_strcmp(name, 0x00941de8 /* "WinddownEnd" */) == 0 && U32(act, 0x21c) == 3) {
        RACE_CORE_CALL(2, rx_activity_set_state(act, 4));
    } else {
        race_thiscall(race_vfn(act, 60), act, 1, (uint32_t)name, 0, 0, 0);
    }
    SU32(act, 0xff06c, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055e9e0, rx_page_wipe_transition)

/* ---- the Post scenarios' events ---------------------------------------------------------------------------------------------- */

/* 00587300 (EAX = activity, EDI = event name, stack the manager; RET 4): a 4-byte payload {activity}. Frame: push ecx. */
void rx_event_activity(rx_addr mgr, rx_addr name, rx_addr act) {
    /*00587300 pushECX holds Activity*, then the original type name and
     * pointer/4-byte payload are passed to the actual dispatcher. */
    RACE_FRAME_PLAIN(F, 0, 1);
    SU32(F, 0, act);
    (void)crt_type_info_name(0x009cbd24, 0x009e1a7c);
    RACE_CORE_CALL(2, rx_world_event_dispatch(mgr, name, F, 4));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00587300, rx_event_activity)

/* 00587ea0 (ECX = the scenario's string, EDX = event name, ESI = manager, stack the activity; RET 4): "Post Scenarios Start" /
 * "Post Scenarios End" with the 8-byte payload {activity, the scenario's string} (void(Activity*, char const*), type_info
 * 0x9cf660) built in the frame and passed by its address. Frame: sub esp, 0xc (the payload at +0 / +4, the /GS cookie at +8,
 * xor the ESP after the sub); push edi; the type's name (push node) from that ESP. */
void rx_event_scenario(rx_addr str, rx_addr name, rx_addr mgr, rx_addr act) {
    RACE_FRAME_PLAIN(F, 0xc, 1);
    const rx_addr payload = F + 4;                                            /* [esp + 8] after push edi */
    SU32(payload, 8, U32(0, 0x009b45e4) ^ payload);                           /* the security cookie */
    SU32(payload, 4, str);                                                    /* mov [esp + 8], ecx (before push node) */
    SU32(payload, 0, act);                                                    /* mov [esp + 8], eax (after push node) */
    crt_type_info_name(0x009cf660, 0x009e1a7c);                               /* type_info::_name_internal_method (IAT 0x916388) */
    RACE_CORE_CALL(2, rx_world_event_dispatch(mgr, name, payload, 8));        /* 004020c0 (EAX = manager, EDI = name) */
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00587ea0, rx_event_scenario)

/* ---- the estimated gap ------------------------------------------------------------------------------------------------------- */

/* 00567d50 (ESI = activity, EDI = driver, stack the last human in the standings; RET 4): an unfinished AI's gap (+0x10b4) to
 * the last human: progress = min(the race's laps * +0xa6c + +0xa70, rx_driver_progress), inv = 0 or 1 / progress; then
 * human's +0x134 - (the driver's +0x134 * min(driver progress, ...) * inv). Frame: and esp, -16; sub 0x2c; push ebx. */
void rx_race_estimate_gap(rx_addr act, rx_addr d, rx_addr human) {
    RACE_FRAME_EBP(F, 16, 0x2c, 1);
    const rx_addr pos1 = F + 0x10, pos2 = F + 0x20;
    RACE_CORE_CALL(1, bike_get_vec_c0(U32(U32(d, 0x74), 0x1ac), pos1));
    const float lap_len = F32(act, 0xa6c), start = F32(act, 0xa70);
    const float total = rx_adds(rx_muls((float)I32(act, 0xff070), lap_len), start);
    float p1;
    RACE_CORE_CALL(1, p1 = rx_driver_progress(d, pos1));
    float m1 = (total > p1) ? p1 : total;
    float inv;
    if (m1 != 0.0f) inv = rx_divs(1.0f, m1); else inv = 0.0f;
    if (U32(d, 0x70) == 0) {
        RACE_CORE_CALL(1, bike_get_vec_c0(U32(U32(d, 0x74), 0x1ac), pos2));
        float p2;
        RACE_CORE_CALL(1, p2 = rx_driver_progress(d, pos2));
        const float m2 = (total > p2) ? p2 : total;
        const float scaled = rx_muls(F32(d, 0x134), m2);
        const float gap = rx_muls(scaled, inv);
        SF32(d, 0x10b4, rx_subs(F32(human, 0x134), gap));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00567d50, rx_race_estimate_gap)

/* ---- the records (career statistics) ----------------------------------------------------------------------------------------- */

/* 00764cb0 MXRecords_Placing (ECX = records, EAX = slot, stack place; RET 4) -> EAX 1 when the best place improved. */
uint32_t rx_records_placing(rx_addr records, uint32_t slot, int32_t place) {
    RACE_FRAME_PLAIN(F, 0xc, 4);
    if (slot == 0xffffffffu) return 0;
    const rx_addr e = records + 0x18 * slot;
    const uint32_t row = U32(e, 4);
    if (row == 0xffffffffu) return 0;
    const uint32_t table = U32(e, 0);
    records_add_int(table, row, place, 0x0096d9a8 /* "TotalPlacing" */);
    const uint32_t col = rx_tbl_column_index(table, 0x0096da98 /* "BestPlace" */);
    const rx_addr cell = rx_tbl_cell_word(table, col, row);
    if (place < I32(cell, 0)) {
        if (place != I32(cell, 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
        if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        SU32(F, 0, (uint32_t)place);
        RACE_CORE_CALL(3, rx_tbl_set_int(U32(U32(table, 0x10), 4 * col), row, F));
        RACE_FRAME_END(F);
        return 1;
    }
    RACE_FRAME_END(F);
    return 0;
}
RX_FUNCTION(0x00764cb0, rx_records_placing)

/* 00764d80 MXRecords_Score (ECX = records, EAX = slot, stack float score; RET 4) -> EAX 1 when the best score improved. */
uint32_t rx_records_score(rx_addr records, uint32_t slot, float score) {
    RACE_FRAME_PLAIN(F, 0xc, 4);
    if (slot == 0xffffffffu) return 0;
    const rx_addr e = records + 0x18 * slot;
    const uint32_t row = U32(e, 4);
    if (row == 0xffffffffu) return 0;
    const uint32_t table = U32(e, 0);
    const int32_t iv = rx_cvtt(score);           /* cvttss2si */
    records_add_int(table, row, iv, 0x00957c04 /* "TotalScore" */);
    const uint32_t col = rx_tbl_column_index(table, 0x009626b0 /* "BestScore" */);
    const rx_addr cell = rx_tbl_cell_word(table, col, row);
    if (iv > I32(cell, 0)) {
        if (iv != I32(cell, 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
        if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        SU32(F, 0, (uint32_t)iv);
        RACE_CORE_CALL(3, rx_tbl_set_int(U32(U32(table, 0x10), 4 * col), row, F));
        RACE_FRAME_END(F);
        return 1;
    }
    RACE_FRAME_END(F);
    return 0;
}
RX_FUNCTION(0x00764d80, rx_records_score)

/* 00764e50 MXRecords_BestLap (ECX = records, EAX = slot, stack float lap; RET 4) -> EAX 1 when the best lap improved. */
uint32_t rx_records_best_lap(rx_addr records, uint32_t slot, float lap) {
    RACE_FRAME_PLAIN(F, 0xc, 3);
    if (slot == 0xffffffffu) return 0;
    const rx_addr e = records + 0x18 * slot;
    const uint32_t row = U32(e, 4);
    if (row == 0xffffffffu) return 0;
    const uint32_t table = U32(e, 0);
    const uint32_t col = rx_tbl_column_index(table, 0x00957c28 /* "BestLapTime" */);
    const rx_addr cell = rx_tbl_cell_word(table, col, row);
    if (F32(cell, 0) > lap) {                   /* comiss cell, lap; jbe skip (NaN skips) */
        if (lap != F32(cell, 0)) SU32(table, 0x68, U32(table, 0x68) + 1);
        if (col >= (U32(table, 0x14) - U32(table, 0x10)) >> 2) crt_invalid_parameter_noinfo();
        SF32(F, 0, lap);
        RACE_CORE_CALL(3, rx_tbl_set_float(U32(U32(table, 0x10), 4 * col), row, F));
        RACE_FRAME_END(F);
        return 1;
    }
    RACE_FRAME_END(F);
    return 0;
}
RX_FUNCTION(0x00764e50, rx_records_best_lap)

/* 00765020 MXRecords_AddTotalTime (stack &ref {row, 0, table}, float seconds; no RET): TotalTime += seconds saturated at
 * FLT_MAX (x86 comiss: a NaN cell keeps FLT_MAX - cell). Frame: push ebx / ebp / esi / edi. */
void rx_records_add_total_time(rx_addr ref, float seconds) {
    RACE_FRAME_PLAIN(F, 0, 4);
    const uint32_t table = U32(ref, 8), row = U32(ref, 0);
    const uint32_t col = rx_tbl_column_index(table, 0x00957c1c /* "TotalTime" */);
    const rx_addr cell = rx_tbl_cell_word(table, col, row);
    const float cur = F32(cell, 0);
    const float room = rx_subs(3.4028234663852886e+38f /* 0x977494 */, cur);
    const float add = !(room > seconds) ? room : seconds;   /* comiss room, seconds; jbe keep room */
    /* the original keeps the sum in its first argument slot (the ref pointer's dword, already used to read the row / table) and
     * hands that slot as the value: `movss [esp+0x1c], xmm0` overwrites arg0 (verified: [ref - 8] in the probe ends as the sum,
     * the ref itself is untouched). */
    const rx_addr val = F + 0x14;   /* entry ESP + 4: the first argument slot, past the four pushes */
    SF32(val, 0, rx_adds(cur, add));
    RACE_CORE_CALL(2, rx_tbl_set_cell_float(table, 0x00957c1c /* "TotalTime" */, row, val));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00765020, rx_records_add_total_time)

/* Isolated Waypoint continuation: preserve the existing highlight boundary,
 * exposing its existing owner without duplicating its profile/panel logic. */
#include "waypoint_lap_support.h"
void rx_waypoint_finish_highlights(rx_addr activity) { finish_highlights(activity); }
