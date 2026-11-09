#include "alive_gate.h"
#include "crt_atexit.h"
#include "waypoint_respawn.h"
/* race_activity.c -- the activities (Activity / MXActivityRace / MXActivityRaceStub / MXActivityFreeRide): the race session object
 * (MXPlayer +0x94). Activity: +0x21c state (0 Pre, 1 Countdown, 2 InProgress, 3 WindDown, 4 Post, 5 Inactive), +0x238 session
 * state, +0x23c kind, +0x268 / +0x26c drivers, +0x290 event manager, +0x294 clock, +0x2d0 standings, +0xff070 laps of the race.
 * (_re/notes/race-state.md, core/notes/race.md "Activity") */
#include "highlight_progress.h"
#include "race.h"
#include "../src/fmt_modtrack.h"
#include "crt.h"
#include "../src/rx_os.h"
#include "build_activity.h"
#include "race_extern.h"
#include "career_activity.h"
#include "build_boundary_audio.h"
#include "waypoint_in_progress.h"
#include "build_series_init.h"
#include "race_activity.h"
#include "race_hud.h"
#include "race_finish.h"
#include "build_ui_props.h"
#include "build_ui_world.h"
#include "build_activity_pre.h"
#include "build_activity_freeride_run.h"
#include "race_driver.h"
#include "race_misc.h"
#include "build_tables.h"
#include "build_activity_int.h"
#include "phys_terrain.h"
#include "world_veh.h"
#include "world_misc.h"
#include "world_rider.h"
#include "world_node.h"
#include "world_wreck.h"
#include "loop_player.h"
#include "loop_input.h"
#include "anim.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "anim_ik.h"
#include "fx.h"
#include "race_beh1.h"
#include "race_activity_misc.h"
#include "race_activity_ui.h"
#include "race_challenge.h"
#include "race_net_session.h"
#include "anim_scenario_misc.h"
#include "build_series_init.h"
#include "race_activity_stub.h"
#include "rider_cap.h"

/* 00556280 Activity_UiKind (EAX = activity) -> EAX: the activity kind (+0x23c); kinds from 0x1f on are series events: the series
 * manager's (MXSeriesManager, constructed on first use behind the static guard 0x9e275c bit 0) kind for the current event. */
uint32_t rx_activity_ui_kind(rx_addr act) {
    uint32_t kind = U32(act, 0x23c);
    if (kind < 0x1f) return kind;
    RACE_FRAME_PLAIN(F, 0, 1);                                  /* push ecx */
    if ((U8(0, 0x9e275c) & 1) == 0) {
        SU32(0, 0x9e275c, U32(0, 0x9e275c) | 1);
        RACE_CORE_CALL(0, (void)rx_series_manager_construct());   /* MXSeriesManager_Construct */
        RX_ATEXIT(0x00911da0);      /* _atexit */
    }
    RACE_CORE_CALL(1, kind = rx_career_current_event_type(0x009e2188));   /* 005b6a30 (stdcall manager; RET 4) */
    RACE_FRAME_END(F);
    return kind;
}
RX_FUNCTION(0x00556280, rx_activity_ui_kind)

/* ---- standings ---------------------------------------------------------------------------------------------------------------- */

/* The race time a driver is ranked by: its frozen finish time once the session is over (state 5, +0x1120 != -1), else the total. */
static float ranking_time(rx_addr d) {
    if (U32(U32(U32(d, 0x74), 0x94), 0x238) == 5 && I32(d, 0x1120) != -1) return F32(d, 0x1124);
    return rx_driver_total_time(d);
}

/* 00567520 MXActivityRaceStub::vf43 (ECX = activity, args: a, b, tie break; RET 0xc) -> EAX: the standings order of two drivers
 * (> 0: a behind b): players out of the race (+0x1e4) last; then more laps, less distance to the line (in hundredths) and less race
 * time (hundredths) first; equal and asked to break the tie: the player slot order (+0x1dc). */
int32_t rx_race_stub_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    (void)act;
    rx_addr pa = U32(a, 0x74);
    if (U32(pa, 0x1e4) != 0) {
        rx_addr pb = U32(b, 0x74);
        if (U32(pb, 0x1e4) == 0) return 1;
        if (tie == 0) return 0;
        return I32(pa, 0x1dc) >= I32(pb, 0x1dc) ? 1 : -1;
    }
    if (U32(U32(b, 0x74), 0x1e4) != 0) return -1;
    const float ta = ranking_time(a), tb = ranking_time(b);
    int32_t key = (I32(b, 0xf1c) - I32(a, 0xf1c)) * 5000000;
    key -= rx_cvtt((F32(a, 0x10cc) - F32(b, 0x10cc)) * -100.0f /* 0x977764 */);
    key -= rx_cvtt((ta - tb) * -100.0f);
    if (key != 0) return key;
    if (tie == 0) return 0;
    return I32(pa, 0x1dc) >= I32(U32(b, 0x74), 0x1dc) ? 1 : -1;
}
RX_FUNCTION(0x00567520, rx_race_stub_compare)

/* Activity vf43 (slot 43): the standings comparator. */
static int32_t compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    typedef int32_t (*fn_t)(rx_addr, rx_addr, rx_addr, uint32_t);
    rx_addr fn = race_vfn(act, 43);
    fn_t f = RX_FN(fn, fn_t);
    if (f) return f(act, a, b, tie);
    return (int32_t)race_thiscall(fn, act, 3, a, b, tie, 0);
}

/* 00559eb0 (ESI = activity): the standings (+0x2d0[i], i < +0x26c; entries before +0x300 stay fixed) by insertion sort with the
 * activity's comparator, then the places (+0x78; equal drivers share a place). */
void rx_activity_sort_standings(rx_addr act) {
    for (int32_t i = I32(act, 0x300); i < I32(act, 0x26c); i++) {
        rx_addr key = rx_stand(act, (uint32_t)i);
        int32_t j = i - 1;
        for (; j >= I32(act, 0x300); j--) {
            rx_addr o = rx_stand(act, (uint32_t)j);
            if (compare(act, o, key, 1) <= 0) break;
            rx_stand_set(act, ((uint32_t)j) + 1, rx_stand(act, (uint32_t)j));
        }
        rx_stand_set(act, ((uint32_t)j) + 1, key);
    }
    if (U32(act, 0x2d0) != 0) SU32(U32(act, 0x2d0), 0x78, 0);
    uint32_t place = 0;
    for (int32_t i = 1; i < I32(act, 0x26c); i++) {
        rx_addr d = rx_stand(act, (uint32_t)i);
        if (compare(act, d, U32(act, 0x2cc + 4 * (uint32_t)i), 0) != 0) place++;
        SU32(d, 0x78, place);
    }
}
RX_FUNCTION(0x00559eb0, rx_activity_sort_standings)

/* 00567f30 (arg = activity; RET 4): "VehicleGettingLapped" (activity, the slower rider's vehicle) for every rider about to be lapped:
 * a rider ahead in the standings with more laps less than the activity's warning distance (+0xff074) behind it on the lap. */
void rx_race_lapping_warnings(rx_addr act) {
    for (int32_t i = 0; i + 1 < I32(act, 0x26c); i++) {
        rx_addr key = rx_stand(act, ((uint32_t)i) + 1);
        for (int32_t j = i; j >= 0; j--) {
            rx_addr o = rx_stand(act, (uint32_t)j);
            if (!(I32(o, 0xf1c) > I32(key, 0xf1c))) continue;
            const float kd = F32(key, 0x10cc), od = F32(o, 0x10cc);
            if (!(od > kd) || !(F32(act, 0xff074) + kd > od)) continue;
            uint32_t p[2] = {act, U32(U32(key, 0x74), 0x1ac)};
            race_raise_event(U32(act, 0x290), 0x00941c5c /* "VehicleGettingLapped" */, 0x009cdb7c, p, 2);
        }
    }
}
RX_FUNCTION(0x00567f30, rx_race_lapping_warnings)

/* 00567c70 (EDI = driver, ESI = activity, arg = other driver; RET 4): the time gap to another driver at the same race progress
 * (+0x10b8 = other's race clock - own race clock * other's progress / own progress, progress capped at the race distance). */
void rx_race_time_gap(rx_addr d, rx_addr act, rx_addr other) {
    RACE_FRAME16_ENTER(F, 0x20);
    race_vehicle_position(U32(U32(d, 0x74), 0x1ac), F);
    const float total = (float)I32(act, 0xff070) * F32(act, 0xa6c) + F32(act, 0xa70);
    float p = rx_driver_progress(d, F);
    if (!(total > p)) p = total;
    float inv = 0.0f;
    if (!(p == 0.0f)) inv = 1.0f /* 0x931f24 */ / p;
    if (U32(other, 0x70) == 0) {
        race_vehicle_position(U32(U32(other, 0x74), 0x1ac), F + 0x10);
        float q = rx_driver_progress(other, F + 0x10);
        if (!(total > q)) q = total;
        SF32(d, 0x10b8, F32(other, 0x134) - (F32(d, 0x134) * q) * inv);
    }
    RACE_FRAME16_LEAVE(F);
}
RX_FUNCTION(0x00567c70, rx_race_time_gap)

/* ---- players ---------------------------------------------------------------------------------------------------------------- */

/* 00552ce0 (ESI = the players (game +0x7460), EDI = kind) -> EAX: how many active players (slots +0xdf8c, count byte +0xdfbc)
 * are of the kind (+0x10; 3 = all). */
uint32_t rx_players_count(rx_addr players, uint32_t kind) {
    const uint32_t n = U8(players, 0xdfbc);
    if (kind == 3) return n;
    uint32_t c = 0;
    for (uint32_t i = 0; i < n; i++)
        if (U32(rx_seat_active(players, i), 0x10) == kind) c++;
    return c;
}
RX_FUNCTION(0x00552ce0, rx_players_count)

/* 00552a10 (EAX = the players, arg = dt; RET 4): the cameras of the players that have a view: local players with a viewport
 * (+4, +8 > -1) in an activity, a TV camera on its own vehicle (+0x1d0 +0x21de0, +0x21c clear), any player in debug / a
 * finished session (Player_UpdateCameras 0054cc10). */
void rx_players_update_cameras(rx_addr players, float dt) {
    for (uint32_t i = 0; i < rx_players_count(players, 3); i++) {
        rx_addr p;
        RACE_CORE_CALL(2, p = rx_world_players_find(players, i, 3));
        if (p == 0) continue;
        int tv = 0;
        rx_addr cam = U32(p, 0x1d0);
        if (cam != 0) { rx_addr c = U32(cam, 0x21de0); tv = c != 0 && c == U32(p, 0x1ac) && U8(p, 0x21c) == 0; }
        rx_addr act = U32(p, 0x94);
        int update;
        if (act != 0 && (U32(0, 0xae1b90) != 0 || U32(act, 0xfe8) != 0)) update = 1;
        else update = (U32(p, 4) != 0 && I32(p, 8) > -1 && act != 0) || tv;
        if (!update) continue;
        RACE_CORE_CALL(1, rx_player_update_cameras(p, dt));
    }
}
RX_FUNCTION(0x00552a10, rx_players_update_cameras)

/* 00550f00 (EAX = position) -> EAX: no water (terrain material 7) within 5 units of the position along x and z (TerrainMaterialAt
 * of the view's terrain, 00402dc0 on 0x9e30d4 behind the static guard 0x9e30e0). */
static rx_addr view_terrain(void) {
    if ((U8(0, 0x9e30e0) & 1) == 0) { SU32(0, 0x9e30e0, U32(0, 0x9e30e0) | 1); SU32(0, 0x9e30dc, 0); }
    rx_addr t;
    RACE_CORE_CALL(0, t = rx_world_find_by_controller(0x009e30d4));
    return t;
}
static uint32_t material_at(rx_addr terrain, rx_addr point) {   /* TerrainMaterialAt 007c5120 (EDI = terrain, EDX = point, EAX = 0) */
    rx_regs in, o; memset(&in, 0, sizeof in);
    uint32_t m;
    RACE_CORE_CALL(0, m = phys_terrain_material_at(terrain, point, 0, &in, &o));
    return m;
}
int rx_race_dry_ground(rx_addr pos) {
    const float x = F32(pos, 0), y = F32(pos, 4), z = F32(pos, 8);
    rx_addr t = view_terrain();
    RACE_FRAME16_ENTER(F, 0x10);
    const float px[4] = {x + 5.0f /* 0x935890 */, x - 5.0f, x, x}, pz[4] = {z, z, z + 5.0f, z - 5.0f};
    int dry = 1;
    for (int i = 0; i < 4 && dry; i++) {
        SF32(F, 0, px[i]); SF32(F, 4, y); SF32(F, 8, pz[i]);
        if (material_at(t, F) == 7) dry = 0;
    }
    RACE_FRAME16_LEAVE(F);
    return dry;
}
RX_FUNCTION(0x00550f00, rx_race_dry_ground)

/* 00567b10 (arg = activity; RET 4): every rider's time behind the leader (+0x10b4 = its race clock - the leader's race clock *
 * its progress / the leader's progress; progress capped at the race distance) back to the first finished one; the leader's own
 * = minus the second's. */
void rx_race_gaps_to_leader(rx_addr act) {
    rx_addr leader = U32(act, 0x2d0);
    RACE_FRAME16_ENTER(F, 0x20);
    race_vehicle_position(U32(U32(leader, 0x74), 0x1ac), F);
    const float total = (float)I32(act, 0xff070) * F32(act, 0xa6c) + F32(act, 0xa70);
    float p = rx_driver_progress(leader, F);
    if (!(total > p)) p = total;
    float inv = 0.0f;
    if (!(p == 0.0f)) inv = 1.0f /* 0x931f24 */ / p;
    for (int32_t i = I32(act, 0x26c) - 1; i > 0; i--) {
        rx_addr d = rx_stand(act, (uint32_t)i);
        if (U32(d, 0x70) != 0) break;
        race_vehicle_position(U32(U32(d, 0x74), 0x1ac), F + 0x10);
        float q = rx_driver_progress(d, F + 0x10);
        if (!(total > q)) q = total;
        SF32(d, 0x10b4, F32(d, 0x134) - (F32(leader, 0x134) * q) * inv);
    }
    RACE_FRAME16_LEAVE(F);
    if (U32(leader, 0x70) != 0) return;
    if (I32(act, 0x26c) > 1) SF32(leader, 0x10b4, -0.0f /* 0x9774a8 */ - F32(U32(act, 0x2d4), 0x10b4));
    else SF32(leader, 0x10b4, 0.0f);
}
RX_FUNCTION(0x00567b10, rx_race_gaps_to_leader)

/* 00567e30 (EDI = activity): the end-of-race flags: a rider within 1 % of the lap (and 100 units) of the line raises
 * "EndOfRace" / "OneLapLeft" / "TwoLapsLeft" (activity, driver) with 1 / 2 / 3 laps to go. */
void rx_race_lap_flags(rx_addr act) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
        const float left = rx_driver_distance_to_line(d);
        if (!(0.01f /* 0x9322ac */ > left / F32(act, 0xa6c)) || !(100.0f /* 0x93005c */ > left)) continue;
        rx_addr name;
        switch (I32(act, 0xff070) - I32(d, 0xf1c)) {
        case 1: name = 0x00941c9c; break;   /* "EndOfRace" */
        case 2: name = 0x00941c80; break;   /* "OneLapLeft" */
        case 3: name = 0x00941c74; break;   /* "TwoLapsLeft" */
        default: continue;
        }
        rx_event_activity_driver(U32(act, 0x290), name, act, d);
    }
}
RX_FUNCTION(0x00567e30, rx_race_lap_flags)

/* 00569820 MXActivityRace_LapOpponentHighlight (arg = activity; RET 4): a local human (pad +0x1f0) who laps another rider for the
 * first time (+0x11d8) gets the highlight progress 0x24 (MXHighlightTracking_AddProgress on 0xd92edc). */
void rx_race_lap_highlight(rx_addr act) {
    race_static_dcf304();
    rx_addr g = U32(0, 0xda6cb0);
    if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) return;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i), p = U32(d, 0x74);
        if (U32(p, 4) == 0 || U32(p, 0x10) != 0 || I32(p, 0x1f0) < 0 || U32(p, 0x11d8) != 0) continue;
        for (int32_t j = 0; j < I32(act, 0x26c); j++) {
            if (j == i) continue;
            rx_addr o = U32(U32(act, 0x268), 4 * (uint32_t)j);
            if (!rx_driver_has_lapped(d, o) || U32(act, 0x238) == 5) continue;
            uint32_t a[3] = {U32(0, 0xd92edc), U32(U32(d, 0x74), 0x1f0), 0x24};
            RACE_CORE_CALL(3, (void)rx_highlight_add_progress(a[0], a[1], a[2]));
            SU32(U32(d, 0x74), 0x11d8, 1);
        }
    }
}
RX_FUNCTION(0x00569820, rx_race_lap_highlight)

/* 00566e70 MXActivityRaceStub::vf61 (ECX = activity, arg = driver index; RET 4) -> EAX: has the driver done the race's laps? */
uint32_t rx_race_driver_done(rx_addr act, uint32_t index) {
    return I32(U32(U32(act, 0x268), 4 * index), 0xf1c) >= I32(act, 0xff070);
}
RX_FUNCTION(0x00566e70, rx_race_driver_done)

/* The race music level (0x9c3edc): the activity's level +0xff080 unless its audio logic (+0x234) mutes it (+0x22148 clear). */
static void set_music_level(rx_addr act, int32_t level) {
    rx_addr audio = U32(act, 0x234);
    SU32(0, 0x9c3edc, audio != 0 ? (U32(audio, 0x22148) != 0 ? level : 0) : level);
}

/* 00565270 MXActivityRace::vf28 (ECX = activity, arg = dt; RET 4): the race in progress: the stub's update (00568a20), the music
 * fade at the start (+0xff078 timer: level from +0xff07c down to 5), then in state 2 the replay recorder (+0xb8c: 005399e0), the
 * standings' time gaps (to the leader, between neighbours), the lap flags, the lapping warnings, and for a human on the last
 * lap the music building up over the last 100 units (+0xff080 = 100 - distance). Session states 3: gaps only; 2 / 4: nothing. */
void rx_race_activity_in_progress(rx_addr act, float dt) {
    RACE_CORE_CALL(1, rx_race_stub_in_progress(act, dt));
    const float timer = F32(act, 0xff078);
    if (timer >= 0.0f) {
        const int32_t level = rx_cvtt((float)(I32(act, 0xff07c) - 5) * (timer * 0.25f /* 0x92ff84 */) + 5.0f /* 0x935890 */);
        SU32(act, 0xff080, (uint32_t)level);
        set_music_level(act, level);
        SF32(act, 0xff078, timer - dt);
    }
    if (U32(act, 0x21c) != 2) return;
    if (U32(act, 0xb8c) != 0) race_thiscall(0x005399e0, act + 0x890, 0, 0, 0, 0, 0);
    const uint32_t st = U32(act, 0x238) - 1;
    if (st > 7) return;
    if (st == 2) { rx_race_gaps_to_leader(act); return; }
    if (st == 1 || st == 3) return;
    if (U32(U32(act, 0x2d0), 0x70) == 0) rx_race_gaps_to_leader(act);
    for (int32_t i = 0; i < I32(act, 0x26c) - 1; i++)
        rx_race_time_gap(rx_stand(act, (uint32_t)i), act, rx_stand(act, ((uint32_t)i) + 1));
    rx_race_lap_flags(act);
    rx_race_lapping_warnings(act);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
        if (U32(U32(d, 0x74), 0x10) != 0 || I32(d, 0xf1c) != I32(act, 0xff070) - 1) continue;
        const float left = rx_driver_distance_to_line(d);
        if (!(100.0 /* 0x977b80 */ > (double)left)) continue;
        const int32_t level = 100 - rx_cvtt(rx_driver_distance_to_line(d));
        SU32(act, 0xff080, (uint32_t)level);
        set_music_level(act, level);
    }
}
RX_FUNCTION(0x00565270, rx_race_activity_in_progress)

/* 005680c0 MXActivityRaceStub::vf23 (ECX = activity, arg = dt; RET 4): the activity update, then its three sub-systems (+0xff090,
 * +0x114260, +0x129430): each one not yet done (vf8) is advanced (vf14). */
void rx_race_stub_update(rx_addr act, float dt) {
    RACE_FRAME_EBP(SF, 8, 0, 2);   /* the original's frame (callees keep stack addresses) */
    RACE_CORE_CALL(1, rx_activity_update(act, dt));
    static const uint32_t subs[3] = {0xff090, 0x114260, 0x129430};
    for (int i = 0; i < 3; i++) {
        rx_addr s = act + subs[i];
        if (race_thiscall(race_vfn(s, 8), s, 0, 0, 0, 0, 0) == 0) race_thiscall(race_vfn(s, 14), s, 0, 0, 0, 0, 0);
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x005680c0, rx_race_stub_update)

/* ---- the activity frame ------------------------------------------------------------------------------------------------------- */

/* vf7 (slot 7) of the drivers and the activity's other updatables: their update (dt). */
static void update_dt(rx_addr obj, uint32_t slot, float dt) {
    typedef void (*fn_t)(rx_addr, float);
    rx_addr fn = race_vfn(obj, slot);
    fn_t f = RX_FN(fn, fn_t);
    if (f) RACE_CORE_CALL(1, f(obj, dt)); else race_thiscall(fn, obj, 1, rx_fb(dt), 0, 0, 0);
}

/* 00559d30 (ESI = activity, arg = dt; RET 4): the AI's landing prediction schedule (0051ff80), every driver's update (vf7: the
 * race drivers' RaceDriver::vf7) and the other updatables (+0x27c, count +0x280, active +0x74). */
void rx_activity_update_drivers(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 0, 1);
    RACE_CORE_CALL(0, rx_air_prediction_schedule());
    for (int32_t i = 0; i < I32(act, 0x26c); i++) update_dt(U32(U32(act, 0x268), 4 * (uint32_t)i), 7, dt);
    for (int32_t i = 0; i < I32(act, 0x280); i++) {
        rx_addr o = U32(U32(act, 0x27c), 4 * (uint32_t)i);
        if (U32(o, 0x74) != 0) update_dt(o, 7, dt);
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x00559d30, rx_activity_update_drivers)

/* 0055e260 (args: activity, dt; RET 8): the 12 scripted sequences (+0x161f0 + 0x15190 * i: active, end, time) still running
 * (FUN_0074cf70 with ESI = the sequence). */
void rx_activity_update_sequences(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 0, 4);
    for (uint32_t i = 0; i < 12; i++) {
        rx_addr e = act + 0x161f4 + 0x15190 * i;
        if (U32(e, -4) == 0 || F32(e, 4) > F32(e, 0)) continue;
        RACE_CORE_CALL(2, rx_scenario_script_run(e - 0x15184, dt, act));
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x0055e260, rx_activity_update_sequences)

/* 00558930 Activity::vf24 (ECX = activity, arg = dt; RET 4): the state's update: Pre vf25, Countdown vf26, InProgress vf28,
 * WindDown vf27, Post vf29 (activity +0x21c). */
void rx_activity_state_update(rx_addr act, float dt) {
    RACE_FRAME_EBP(SF, 16, 0, 0);
    static const uint8_t slot[5] = {25, 26, 28, 27, 29};
    const uint32_t st = U32(act, 0x21c);
    if (st <= 4) update_dt(act, slot[st], dt);
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x00558930, rx_activity_state_update)

/* 00559c10 Activity::vf23 (ECX = activity, arg = dt; RET 4): the activity frame: the drivers (00559d30); once every player has a
 * vehicle: the state's update (vf24), the scenario (+0xfd8 vf31), the debug camera-target key (each driver's +0x144 cycles through
 * the activity's 0x30-byte views at +0x340, count +0x880; then vf18), the scripted sequences (0055e260) and 0055cf10. */
void rx_activity_update(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 0, 4);
    RACE_CORE_CALL(1, rx_activity_update_drivers(act, dt));
    int all = 1;
    for (int32_t i = 0; i < I32(act, 0x26c); i++)
        if (U32(U32(U32(U32(act, 0x268), 4 * (uint32_t)i), 0x74), 0x1ac) == 0) { all = 0; break; }
    if (all) {
        update_dt(act, 24, dt);
        if (U32(act, 0xfd8) != 0) update_dt(U32(act, 0xfd8), 31, dt);
        uint32_t hit = 0;
        if (U8(0, 0x9b716d) == 0) RACE_CORE_CALL(2, hit = rx_world_keymap_latched(0x00d5c188, 7, 0));
        if (hit != 0) {
            for (int32_t i = 0; i < I32(act, 0x26c); i++) {
                rx_addr d = U32(U32(act, 0x268), 4 * (uint32_t)i);
                const int32_t n = I32(act, 0x880);
                int32_t k = 0;
                while (k < n && U32(d, 0x144) != rx_start_slot(act, (uint32_t)k)) k++;   /* (w-rc-core) slots 28..31 in side storage */
                k++;
                if (!(k < n)) k = 0;
                SU32(d, 0x144, rx_start_slot(act, (uint32_t)k));
            }
            race_thiscall(race_vfn(act, 18), act, 0, 0, 0, 0, 0);
        }
        RACE_CORE_CALL(2, rx_activity_update_sequences(act, dt));
        RACE_CORE_CALL(0, rx_activity_presence(act));
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x00559c10, rx_activity_update)

/* 005594a0 Activity::vf28 (ECX = activity, arg = dt; RET 4): InProgress: the music pause (audio logic +0x234), the activity clocks
 * (+0x294, +0x2a4), vf42, the standings; and the time-limit countdown: within +0x2c0 + 1 s of the limit +0x2bc the count +0x2a0
 * steps down with "FinishCountdown Step" (activity, count). */
void rx_activity_in_progress(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 0, 2);
    rx_addr au = U32(act, 0x234);
    if (au != 0 && U32(au, 0x20018) != 0 && U32(U32(0, 0xa93fcc), 0x284) == 0 && U32(au, 0x20138) != 0 &&
        U32(U32(au, 0x20138), 0) != 0 && U32(au, 0x20014) == 0) {
        rx_audio_pause_flags(0, 1);                         /* 00805120 */
        rx_audio_pause_all(0, 0);                           /* Sound_PauseAll */
    }
    SF32(act, 0x294, F32(act, 0x294) + dt);
    SF32(act, 0x2a4, F32(act, 0x2a4) + dt);
    race_thiscall(race_vfn(act, 42), act, 0, 0, 0, 0, 0);
    RACE_CORE_CALL(0, rx_activity_sort_standings(act));
    au = U32(act, 0x234);
    if (au != 0 && U32(au, 0x20018) == 0) {
        const float limit = F32(act, 0x2bc), clock = F32(act, 0x294);
        if (limit > 0.0f && limit > clock) {
            const float rem = limit - clock;
            const int32_t n = I32(act, 0x2a0);
            if ((float)I32(act, 0x2c0) + 1.0f >= rem && (float)n >= rem) {
                SU32(act, 0x2a0, (uint32_t)(n - 1));
                RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), 0x00941bec /* "FinishCountdown Step" */, act, (uint32_t)n));
            }
        }
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x005594a0, rx_activity_in_progress)

/* The first local human player (game +0x7460 players, slots +0xdf8c, count +0xdfbc), or 0. */
static rx_addr first_human_player(void) {
    rx_addr players = U32(0, 0xa946b8) + 0x7460;
    if (I32(players, 0xdf8c) == -1) return 0;
    const uint32_t n = U8(players, 0xdfbc);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr p = rx_seat_active(players, i);
        if (U32(p, 0x10) == 0) return p;
    }
    return 0;
}
/* Disable one input mapping of a pad: a device's vf9 (pad, 1). */
static void pad_disable(rx_addr dev, uint32_t pad) { race_thiscall(race_vfn(dev, 9), dev, 2, pad, 1, 0, 0); }
/* The pad's mapping getters (world_misc.c: 00548f20 / 00548f70 pad in EAX, 00548fc0 / 00549050 pad in ESI) -> EAX; 00548bf0 /
 * 00548c40 (ESI = pad, EDI = 0: the slot's group 2 / 5 switched off; loop_player.c) have no result. */
static rx_addr pad_getter(rx_addr fn, uint32_t pad, int pad_in_eax) {
    (void)pad_in_eax;
    rx_addr r = 0;
    switch (fn) {
    case 0x00548f20: RACE_CALL_SP(0, r = rx_world_pad_steer_map(pad)); break;
    case 0x00548f70: RACE_CALL_SP(0, r = rx_world_pad_lean_map(pad)); break;
    case 0x00548fc0: RACE_CALL_SP(0, r = rx_world_pad_wreck_steer_map(pad)); break;
    case 0x00549050: RACE_CALL_SP(0, r = rx_world_pad_wreck_lean_map(pad)); break;
    case 0x00548bf0: RACE_CALL_SP(0, rx_player_slot_group2(pad, 0)); break;
    default:         RACE_CALL_SP(0, rx_player_slot_group5(pad, 0)); break;   /* 00548c40 */
    }
    return r;
}

/* 00568a20 MXActivityRaceStub::vf28 (ECX = activity, arg = dt; RET 4): InProgress (Activity::vf28), and at the end of a practice
 * session (state 6, once the clock passes +0x13e604; done +0x13e608) the first human's pad mappings switched off one by one (or,
 * while its player holds them (+0x11c8), only marked in +0x11cc) and its vehicle's controller told (vf22); then the lapping
 * highlight. */
void rx_race_stub_in_progress(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 8, 4);
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
    if (U32(act, 0x238) == 6 && U32(act, 0x13e608) == 0 && F32(act, 0x294) > F32(act, 0x13e604)) {
        rx_addr p = first_human_player();
        static const uint32_t dev_off[] = {0x348, 0x9ec, 0x127c, 0x167c, 0x748, 0x147c, 0xf34};
        if (I32(p, 0x1f0) >= 0) {
            const uint32_t pad = U32(p, 0x1f0);
            if (U32(p, 0x11c8) != 0) SU32(p, 0x11cc, U32(p, 0x11cc) | 1);
            else { pad_disable(pad_getter(0x00548f20, pad, 1), pad); pad_disable(pad_getter(0x00548f70, pad, 1), pad); }
        }
        uint32_t bit = 2;
        for (int k = 0; k < 10; k++, bit <<= 1) {
            if (I32(p, 0x1f0) < 0) continue;
            const uint32_t pad = U32(p, 0x1f0);
            if (U32(p, 0x11c8) != 0) { SU32(p, 0x11cc, U32(p, 0x11cc) | bit); continue; }
            switch (k) {
            case 0: pad_disable(U32(0, 0x9c52c4 + 4 * pad) + dev_off[0], pad); break;
            case 1: pad_getter(0x00548bf0, pad, 0); break;
            case 2: pad_disable(pad_getter(0x00548fc0, pad, 0), pad); pad_disable(pad_getter(0x00549050, pad, 0), pad); break;
            case 3: pad_disable(U32(0, 0x9c52c4 + 4 * pad) + dev_off[1], pad); break;
            case 4: pad_getter(0x00548c40, pad, 0); break;
            default: pad_disable(U32(0, 0x9c52c4 + 4 * pad) + dev_off[k - 3], pad); break;
            }
        }
        rx_addr ctl = U32(U32(p, 0x1ac), 0x234);
        race_thiscall(race_vfn(ctl, 22), ctl, 0, 0, 0, 0, 0);
        SU32(act, 0x13e608, 1);
    }
    RACE_CORE_CALL(1, rx_race_lap_highlight(act));
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x00568a20, rx_race_stub_in_progress)

/* ---- the state machine ------------------------------------------------------------------------------------------------------ */

/* 00585860 (stdcall map, key*; RET 8) -> EAX: std::map<int, std::string>::operator[] of the activity's state names (+0x240): the
 * lower bound in the red-black tree (head +0x18; node: +0 left, +4 parent, +8 right, +0xc key, +0x10 value, +0x2d is-nil); a
 * missing key (never happens in a race: every state is named at construction) is inserted with a default string at the hint
 * (00585e90). Frame: push ebp; and esp, -8; sub esp, 0x4c; push ebx / esi / edi; the pair {key, string} at +0x38, a copy of
 * the default string at +0x1c, the insert's result iterator at +0x14. */
rx_addr rx_activity_state_name(rx_addr map, rx_addr key_ptr) {
    RACE_FRAME_EBP(F, 8, 0x4c, 3);
    const int32_t key = I32(key_ptr, 0);
    const rx_addr head = U32(map, 0x18);
    rx_addr best = head;
    for (rx_addr n = U32(head, 4); U8(n, 0x2d) == 0;) {
        if (I32(n, 0xc) < key) n = U32(n, 8);
        else { best = n; n = U32(n, 0); }
    }
    if (U32(map, 0) == 0) crt_invalid_parameter_noinfo();      /* the checked iterator of the owner's proxy */
    if (best != head && !(key < I32(best, 0xc))) { RACE_FRAME_END(F); return best + 0x10; }
    const rx_addr val = F + 0x38;
    crt_str_ctor(F + 0x1c);
    SU32(val, 0, (uint32_t)key);
    crt_str_ctor_copy(val + 4, F + 0x1c);
    const rx_addr outit = F + 0x14;
    RACE_CORE_CALL(5, (void)rx_state_map_insert_hint(map, outit, val, U32(map, 0), best));
    const rx_addr node = U32(outit, 4);
    crt_str_dtor(val + 4);
    crt_str_dtor(F + 0x1c);
    if (U32(outit, 0) == 0 || node == U32(U32(U32(outit, 0), 0), 0x18)) crt_invalid_parameter_noinfo();   /* the result iterator is checked: not end() */
    RACE_FRAME_END(F);
    return node + 0x10;
}
RX_FUNCTION(0x00585860, rx_activity_state_name)

static rx_addr string_chars(rx_addr s) { return U32(s, 0x18) < 0x10 ? s + 4 : U32(s, 4); }   /* std::string: SSO under 16 */

/* 0078fd60 (EDI = buffer, ESI = size; cdecl fmt, ...): crt_format.c's rx_format_buffer, the cdecl arguments written where the
 * original pushes them (fmt, then n variadic words) */
static void race_format(rx_addr buf, uint32_t size, rx_addr fmt, int n, const uint32_t* va) {
    const rx_addr sp = rx_sp(), args = sp - 4u * (uint32_t)(n + 1);
    SU32(args, 0, fmt);
    for (int i = 0; i < n; i++) SU32(args, 4u * (uint32_t)(i + 1), va[i]);
    rx_set_sp(args);                       /* the pushed arguments are live: the callee's frames go below them */
    rx_format_buffer(buf, size, fmt, args + 4);
    rx_set_sp(sp);
}
static void format_name(rx_addr buf, rx_addr fmt, rx_addr name) { race_format(buf, 0x104, fmt, 1, &name); }

/* 005586c0 Activity_SetState (stdcall activity, state; RET 8): leaves the current state (vf30 (old, new)), raises "<old> Exited"
 * (value: the new state), enters the new one (vf36 (new, old)), raises "<new> Entered" (value: the old state) when a handler is
 * registered under that name (00593220), then stores the state (+0x21c). The event names are formatted into a 260-byte buffer on
 * the original's frame (and esp, -8; sub 0x114; 3 pushes: the buffer at +0x10). */
void rx_activity_set_state(rx_addr act, uint32_t state) {
    RACE_FRAME_EBP(F, 8, 0x114, 3);
    const rx_addr buf = F + 0x10, key = F + 0x10 + 0x104;
    const rx_addr cur = act + 0x21c;
    SU32(F + 0xc, 0, cur);
    race_thiscall(race_vfn(act, 30), act, 2, U32(act, 0x21c), state, 0, 0);
    rx_addr nm;
    RACE_CORE_CALL(2, nm = rx_activity_state_name(act + 0x240, cur));        /* 00585860 */
    format_name(buf, 0x00941dc0 /* "%s Exited" */, string_chars(nm));
    RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), buf, act, state));
    race_thiscall(race_vfn(act, 36), act, 2, state, U32(cur, 0), 0, 0);
    SU32(key, 0, state);
    RACE_CORE_CALL(2, nm = rx_activity_state_name(act + 0x240, key));
    format_name(buf, 0x00941db4 /* "%s Entered" */, string_chars(nm));
    uint32_t listening;
    RACE_CORE_CALL(2, listening = rx_world_registry_find(U32(act, 0x290), buf));
    if (listening != 0)
        RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), buf, act, U32(cur, 0)));
#ifndef RX_ORACLE
    rx_race_hud_state_entered(act, state);   /* the HUD components' "WindDown Entered" / "Post Entered" handlers (no listener exists: no event) */
    rx_alive_gate_state_entered(act, state);   /* PORT ADDITION (alive_gate.h): an Alive gate drops as InProgress begins (Reflex's "Countdown Step" 0) */
#endif
    SU32(cur, 0, state);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005586c0, rx_activity_set_state)

/* 00558880 Activity::vf36 (ECX = activity, args: new state, old state; RET 8): the state's Enter (vf37 Pre (old), vf38 Countdown
 * (old), vf39 InProgress (old), vf40 WindDown (3), vf41 Post (old)); then with the replay recorder on (0xda6cb0 +0x1c48) the
 * state change is recorded (006f7810), after the stats static guard 0xdcf304. */
void rx_activity_enter_state(rx_addr act, uint32_t state, uint32_t old) {
    RACE_FRAME_PLAIN(F, 0, 1);
    if (state <= 4) {
        static const uint8_t slot[5] = {37, 38, 39, 40, 41};
        race_thiscall(race_vfn(act, slot[state]), act, 1, state == 3 ? 3u : old, 0, 0, 0);
    }
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c48) != 0) RACE_CORE_CALL(1, rx_net_session_record_state(state));   /* 006f7810 */
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00558880, rx_activity_enter_state)

/* The engine stats root (FUN_006de1c0: the profiler block behind the 0xdcf304 guard) -> EAX. */
static rx_addr stats_root(void) {
    rx_addr r;
    RACE_CORE_CALL(0, r = rx_fx_session_manager());
    return r;
}

/* 00558ce0 Activity::vf25 (ECX = activity, arg = dt; RET 4): Pre: until the loading is done (+0xff068) and the session (0xa93fcc)
 * is ready: the music unpaused, a pending intro (+0xfe4, vf48) finished, the pads' "start" keys polled (CompoundKeyMap on the
 * active pads), the 0.5 s countdown 0x9cf5a0; with an intro time (+0x2a8 > 0): the countdown starts (SetState 1) once vf11 / vf8
 * allow and the UI property 0x94d18c is 0; without: once the countdown runs out the stats timer is set and with a finished
 * session (+0xfe8) the countdown starts. The original reuses its dt argument slot as a local (the UI property's value, a time
 * scale): written there too. */
void rx_activity_pre(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(SF, 8, 2);
    const rx_addr arg_slot = SF_sp + 4;
    if (U32(act, 0xff068) != 0) { RACE_FRAME_END(SF); return; }
    race_static_dcf304();
    rx_addr g = U32(0, 0xda6cb0), session = U32(0, 0xa93fcc);
    if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) SU32(session, 0x28c, 0);
    if (U32(session, 0x284) == 0) {
        rx_audio_pause_flags(0, 1);                         /* 00805120 */
        rx_addr au = U32(act, 0x234);
        rx_audio_pause_all((au != 0 && U32(au, 0x20014) != 0) ? 1u : 0u, 0);   /* Sound_PauseAll */
    }
    if (U32(act, 0xfe4) != 0 && race_thiscall(race_vfn(act, 48), act, 0, 0, 0, 0, 0) != 0) {
        rx_addr frame = U32(U32(0, 0xa95844), 0x10);
        RACE_CORE_CALL(0, rx_engine_frame_mark(frame));
        SU32(act, 0xfe4, 0);
        RACE_CORE_CALL(0, rx_game_reset_view_params());   /* 00559b60 (race_activity_stub.c) */
    }
    rx_addr s2 = U32(0, 0xa93fcc);
    /* UIManager::vf40 (00596460: a UI page / modal is open, +0x9c): the product has no UI manager ([0xa93fcc] = 0): nothing is open */
    if (s2 == 0 || race_thiscall(race_vfn(s2, 40), s2, 0, 0, 0, 0, 0) == 0) {
        uint32_t k[4];
        for (uint32_t i = 0; i < 4; i++) RACE_CORE_CALL(1, k[i] = rx_player_controller_present(i));
        uint32_t hit = 0;
        for (uint32_t i = 0; i < 4; i++) {
            if (k[i] == 0 || hit != 0) continue;
            RACE_CORE_CALL(2, hit = rx_compound_keymap_pressed(0x00d6aa60, 1, i));
        }
    }
    const float t = F32(0, 0x9cf5a0) - dt;
    SF32(0, 0x9cf5a0, t);
    const float intro = F32(act, 0x2a8);
    if (intro > 0.0f) {
        if (race_thiscall(race_vfn(act, 11), act, 0, 0, 0, 0, 0) != 0 && race_thiscall(race_vfn(act, 8), act, 0, 0, 0, 0, 0) != 0) {
            RACE_FRAME_END(SF); return;
        }
        SU32(arg_slot, 0, 0);
        /* UIProperty_GetInt("WaitForUI") 005dc0c0: the oracle's UI store; the product's property is absent (build_ui_props.c) */
        RACE_CORE_CALL(2, (void)rx_ui_session_get_int(0x0094d18c, arg_slot));
        if (U32(arg_slot, 0) == 0) RACE_CORE_CALL(2, rx_activity_set_state(act, 1));
        RACE_FRAME_END(SF); return;
    }
    if (intro != intro || 0.0f < t) { RACE_FRAME_END(SF); return; }
    rx_addr ses;
    RACE_CORE_CALL(0, ses = rx_fx_session_manager());
    float base;
    RACE_CORE_CALL(0, base = rx_world_timer_seconds(ses));
    const float scale = rx_session_is_networked(U32(stats_root(), 0x10)) != 0 ? 8.0f /* 0x9322b8 */ : 1.0f /* 0x931f24 */;
    SF32(arg_slot, 0, scale);
    RACE_CORE_CALL(1, rx_session_pre_timer(U32(stats_root(), 0x10), scale + base));   /* 006e4a90 (ESI = the session) */
    SF32(0, 0x9cf5a0, 0.5f /* 0x9322cc */);
    if (U32(act, 0xfe8) != 0) {
        RACE_CORE_CALL(2, rx_activity_set_state(act, 1));
        SF32(act, 0x2a8, 0.0f);
    }
    RACE_FRAME_END(SF);
}
RX_FUNCTION(0x00558ce0, rx_activity_pre)

/* ---- the end of the race ------------------------------------------------------------------------------------------------------ */

/* MXActivityRaceStub::vf61 (slot 61): has the driver of index i finished. */
static uint32_t driver_done(rx_addr act, uint32_t i) {
    typedef uint32_t (*fn_t)(rx_addr, uint32_t);
    const rx_addr fn = race_vfn(act, 61);
    fn_t f = RX_FN(fn, fn_t);
    if (f) { uint32_t r; RACE_CORE_CALL(1, r = f(act, i)); return r; }
    return race_thiscall(fn, act, 1, i, 0, 0, 0);
}

/* 00567d50 (ESI = activity, EDI = driver, arg: the last human in the standings; RET 4): an AI driver still racing behind the last
 * human gets an estimated gap (+0x10b4) from the remaining distance and its speed (race_finish.c, agent "raceend"). */
static void estimate_gap(rx_addr act, rx_addr d, rx_addr human) {
    RACE_CORE_CALL(1, rx_race_estimate_gap(act, d, human));
}

/* 00566ee0 MXActivityRaceStub::vf42 (ECX = activity): the end-of-race check, by session state (+0x238; 0 / 2 / 3: nothing):
 * - 5 (the results): with one player (+0xff088 == 1) "Player Wins" with the localized "STR_WINS" text ("%s %s": the winner's name
 *   +0x54 and the text converted from UTF-16 into a 256-byte buffer), else wait while a human player is in the standings; then
 *   WindDown (SetState 3).
 * - others: on a network session (0xda6cb0 +0x1c4c, +0x1b0 != 1) that is not a replay and before the result (+0xbc0) only the
 *   finished drivers get +0x70. In state 8 (a finish countdown: the leader's player +0x11ac >= 0, its remaining time +0x11b4) the
 *   countdown steps "FinishCountdown Step" down (+0x2a0, from +0x2c0) and at 0 the leader wins ("Player Wins", +0x1128) and the race
 *   winds down. Then the drivers are counted: humans, AI, finished humans / AI (vf61; +0x70 set), humans gone from a network race.
 *   All humans finished (or finished + gone): the gaps of the standings to the leader (+0x10b4), the AI behind the last human get
 *   an estimated gap (00567d50) plus the human's, their laps (+0xf1c) set to the race's and (two laps) an estimated finish time
 *   (+0x10c4: gap + the leader's lap time at the race's lap count); then WindDown. Some humans finished (or only AI, all finished,
 *   when 0xae1b98): after the wind-down delay (+0x2a4 > +0x2ac; a negative one is reset to 0) WindDown. No humans: WindDown once all
 *   AI finished.
 * Frame: and esp, -8; sub 0x22c; 3 pushes (the converted text at +0x28, the formatted one at +0x128). */
void rx_race_stub_check_finish(rx_addr act) {
    RACE_FRAME_EBP(F, 8, 0x22c, 3);
    const uint32_t ss = U32(act, 0x238);
    if (ss == 0 || ss == 2 || ss == 3) goto done;
    if (ss == 5) {
        if (U32(act, 0xff088) == 1) {
#ifdef RX_ORACLE
            uint32_t z = 0;
            uint32_t lang;
            RACE_CORE_CALL(1, lang = rx_ui_language_find(z));                         /* 005dfc90 (race_activity_misc.c) */
            rx_addr entry;
            RACE_CORE_CALL(0, entry = rx_ui_language_lookup(0x00941fc4 /* "STR_WINS" */, lang));   /* 005e0270 (EDI = the language) */
            uint32_t w[8] = {0xfde9 /* CP_UTF8 */, 0, U32(entry, 0x10), 0xffffffffu, F + 0x28, 0x100, 0, 0};
            rx_os_import(0x009160a8, 8, w);       /* WideCharToMultiByte */
#else
            /* UI boundary (the product has no UI string table), as in race_activity_freestyle.c's WindDown text: 005dfc90 searches
             * the UI language registry (0xaf6888 / 0xaf6950, index [0xa94074]) that only the UI fills, so the original's index check
             * would terminate the product. The localized word appears only in this HUD text; the event is still raised. */
            SU8(F + 0x28, 0, 0);
#endif
            SU8(F, 0x127, 0);
            const uint32_t va[2] = {U32(U32(act, 0x2d0), 0x74) + 0x54, F + 0x28};
            race_format(F + 0x128, 0x104, 0x00941fd0 /* "%s %s" */, 2, va);
            RACE_CORE_CALL(2, rx_event_player_wins(U32(act, 0x290), act, U32(act, 0x2d0), F + 0x128, 0));
        } else {
            for (uint32_t i = 0; (int32_t)i < I32(act, 0xff088); i++) {
                const rx_addr p = U32(rx_stand(act, i), 0x74);
                if (U32(p, 4) != 0 && U32(p, 0x10) == 0) goto done;
            }
        }
        goto wind_down;
    }
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c4c) != 0 && U32(U32(0, 0xda6cb0), 0x1b0) != 1) {
        race_static_dcf304();
        if (U32(U32(0, 0xda6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) {
            const int32_t n = I32(act, 0x26c);
            for (int32_t i = 0; i < n; i++)
                if (driver_done(act, (uint32_t)i) != 0) SU32(U32(U32(act, 0x268), 4 * i), 0x70, 1);
            goto done;
        }
    }
    if (ss == 8) {
        const rx_addr leader = U32(U32(act, 0x268), 0);
        const rx_addr p = U32(leader, 0x74);
        if (I32(p, 0x11ac) < 0) SU32(act, 0x2a0, U32(act, 0x2c0));
        else {
            const float left = F32(p, 0x11b4);
            if (!(0.0f < left) && left == left) {
                SU32(leader, 0x1128, 1);
                RACE_CORE_CALL(2, rx_event_player_wins(U32(act, 0x290), act, leader, 0, 0));
                RACE_CORE_CALL(2, rx_activity_set_state(act, 3));
            } else {
                const float first = (float)I32(act, 0x2c0) + 1.0f;
                if (!(first < left) && left == left) {
                    const int32_t step = I32(act, 0x2a0);
                    if (!((float)step < left)) {
                        SU32(act, 0x2a0, (uint32_t)(step - 1));
                        RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), 0x00941bec /* "FinishCountdown Step" */, act,
                                                                (uint32_t)step));
                    }
                } else if (left > first) SU32(act, 0x2a0, U32(act, 0x2c0));
            }
        }
    }
    {
        const int32_t n = I32(act, 0x26c);
        int32_t humans = 0, ais = 0, done_h = 0, done_ai = 0, gone = 0;
        for (int32_t i = 0; i < n; i++) if (U32(U32(U32(U32(act, 0x268), 4 * i), 0x74), 0x10) == 0) humans++;
        for (int32_t i = 0; i < n; i++) if (U32(U32(U32(U32(act, 0x268), 4 * i), 0x74), 0x10) == 1) ais++;
        for (int32_t i = 0; i < n; i++) {
            const rx_addr d = U32(U32(act, 0x268), 4 * i);
            if (driver_done(act, (uint32_t)i) != 0) {
                if (U32(U32(d, 0x74), 0x10) == 0) done_h++; else done_ai++;
                SU32(d, 0x70, 1);
            }
            race_static_dcf304();
            const rx_addr g = U32(0, 0xda6cb0);
            const rx_addr p = U32(d, 0x74);
            if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1 && U32(p, 0x10) == 0 && U32(p, 0x1e4) != 0 && U32(d, 0x70) == 0) gone++;
        }
        if (humans <= 0) {
            if (ais <= 0 || ais != done_ai) goto done;
            goto wind_down;
        }
        if (humans != gone + done_h && humans != done_h) {
            if (done_h <= 0 && (U32(0, 0xae1b98) == 0 || ais <= 0 || ais != done_ai)) goto done;
            const float t = F32(act, 0x2a4);
            if (0.0f > t) { SF32(act, 0x2a4, 0.0f); goto done; }
            if (t > F32(act, 0x2ac)) goto wind_down;
            goto done;
        }
        /* every human finished: the final gaps */
        const int32_t m = I32(act, 0x26c);
        const rx_addr lead = U32(act, 0x2d0);
        int32_t last_human = 0;
        if (m > 1) {
            const float t2 = ranking_time(U32(act, 0x2d4));
            const float t1 = ranking_time(lead);
            SF32(lead, 0x10b4, t2 - t1);
        }
        for (int32_t i = 0; i < m; i++) {
            const rx_addr d = rx_stand(act, i);
            if (U32(U32(d, 0x74), 0x10) == 0) last_human = i;
            if (i > 0) {
                const float t = ranking_time(d);
                SF32(d, 0x10b4, t - rx_driver_standings_time(lead));
            }
        }
        const float lap_time = F32(U32(act, 0x2d0), 0xf20 + 4 * U32(act, 0xff070));
        for (int32_t j = last_human + 1; j < m; j++) {
            const rx_addr d = rx_stand(act, j);
            estimate_gap(act, d, rx_stand(act, last_human));
            if (last_human != 0) SF32(d, 0x10b4, F32(rx_stand(act, last_human), 0x10b4) + F32(d, 0x10b4));
            if (U32(U32(d, 0x74), 0x10) == 1) {
                if (F32(d, 0x10c4) == 3.4028234663852886e+38f) {
                    const uint32_t laps = U32(act, 0xff070);
                    if (laps == 1) SF32(d, 0x10c4, 3.4028234663852886e+38f);
                    else if (laps == 2) SF32(d, 0x10c4, F32(d, 0x10b4) + lap_time);
                }
                SU32(d, 0xf1c, U32(act, 0xff070));
            }
        }
    }
wind_down:
    RACE_CORE_CALL(2, rx_activity_set_state(act, 3));
done:
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00566ee0, rx_race_stub_check_finish)

/* ---- lookups ------------------------------------------------------------------------------------------------------------------ */

/* 00559e60 (EAX = activity, arg: n; RET 4) -> EAX: the n-th human player's driver (a player +4 set and +0x10 == 0), or 0. */
rx_addr rx_activity_human_driver(rx_addr act, int32_t n) {
    int32_t k = 0;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * i);
        const rx_addr p = U32(d, 0x74);
        if (U32(p, 4) != 0 && U32(p, 0x10) == 0) { if (k++ == n) return d; }
    }
    return 0;
}
RX_FUNCTION(0x00559e60, rx_activity_human_driver)

/* 0055a000 (EAX = activity, args: player kind, n; RET 8) -> EAX: the n-th driver whose player is of the kind (+0x10: 0 human,
 * 1 AI), or 0. */
rx_addr rx_activity_driver_of_kind(rx_addr act, uint32_t kind, int32_t n) {
    int32_t k = 0;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * i);
        if (U32(U32(d, 0x74), 0x10) == kind) { if (k++ == n) return d; }
    }
    return 0;
}
RX_FUNCTION(0x0055a000, rx_activity_driver_of_kind)

/* 00559fd0 (EAX = player, EDI = activity) -> EAX: the player's driver: its slot (FUN_0055a050, ECX = activity, ESI = player) in
 * the network drivers (+0x27c) for a remote player (+0x908), else in the drivers (+0x268). */
rx_addr rx_activity_player_driver(rx_addr player, rx_addr act) {
    uint32_t slot;
    RACE_CORE_CALL(0, slot = (uint32_t)rx_world_group_rider_index(act, player));
    return U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4 * slot);
}
RX_FUNCTION(0x00559fd0, rx_activity_player_driver)

/* 0057b2f0 (shared slot 34 of NetworkSession and others): nothing (only the stack cookie). */
void rx_activity_nop(void) {}
RX_FUNCTION(0x0057b2f0, rx_activity_nop)

/* (d +0x10c8 + race time) - d +0x140 (FUN_00512940, EAX = driver): the race time with the time bonuses / penalties applied. */
static float adjusted_time(rx_addr d) { return F32(d, 0x10c8) + ranking_time(d) - F32(d, 0x140); }

/* 00565570 MXActivityRace::vf43 (ECX = activity, args: a, b, tie; RET 0xc) -> EAX: the standings order; in progress (+0x21c == 2)
 * the race stub's (laps / distance / time); else by the adjusted race time: -1 if a's is greater, 1 if smaller, else 0. */
int32_t rx_race_compare(rx_addr act, rx_addr a, rx_addr b, uint32_t tie) {
    if (U32(act, 0x21c) == 2) return rx_race_stub_compare(act, a, b, tie);
    const float ka = adjusted_time(a), kb = adjusted_time(b);
    if (ka > kb) return -1;
    const float a2 = adjusted_time(a), b2 = adjusted_time(b);
    return a2 > b2 ? 1 : 0;
}
RX_FUNCTION(0x00565570, rx_race_compare)

/* ---- the activity manager ----------------------------------------------------------------------------------------------------- */

/* 0055f700 (ESI = the activity manager {+0 the game state, +4 activities, +8 count, +0x1c the debug-selected one}; arg dt;
 * RET 4): the debug key (KeyMap 0xd60880 vf1 (7, 0), unless 0x9b716d) cycles the selected activity (+0xb8c of the activities;
 * past the last: the game state's +0x2210c while +0x22130, then none), a pending game state reset (+0x22104 while +0x22130:
 * FUN_00536ef0 (ECX = state +0x20030; +0x21e10, 8, 0)); then every activity not Inactive (+0x21c != 5) and not paused (+0x260)
 * is updated (vf23 (dt)); the others with a pending intro (+0xfe4) finish it (FUN_00559ac0). */
void rx_activity_manager_update(rx_addr mgr, float dt) {
    RACE_FRAME_PLAIN(F, 4, 1);   /* push ecx (the cookie); push edi */
    if (U8(0, 0x9b716d) == 0) {
        uint32_t hit = 0;
        RACE_CORE_CALL(2, hit = rx_world_keymap_latched(0x00d60880, 7, 0));
        if (hit != 0) {
            const int32_t sel = I32(mgr, 0x1c), n = I32(mgr, 8);
            if (sel > -1 && n > sel) SU32(U32(U32(mgr, 4), 4 * sel), 0xb8c, 0);
            else if (n == sel) SU32(U32(mgr, 0), 0x2210c, 0);
            SU32(mgr, 0x1c, (uint32_t)(sel + 1));
            const int32_t next = I32(mgr, 0x1c), cnt = I32(mgr, 8);
            if (cnt < next) SU32(mgr, 0x1c, 0xffffffffu);
            else if (cnt != next) SU32(U32(U32(mgr, 4), 4 * next), 0xb8c, 1);
            else if (U32(U32(mgr, 0), 0x22130) != 0) SU32(U32(mgr, 0), 0x2210c, 1);
            else SU32(mgr, 0x1c, 0xffffffffu);
        }
    }
    const rx_addr gs = U32(mgr, 0);
    if (U32(gs, 0x22130) != 0 && U32(gs + 0x21e10, 0x2f4) != 0) {
        rx_track_load(gs + 0x20030, gs + 0x21e10, 8, 0);   /* FUN_00536ef0 (.tdf07 loader; unit 11) */
        SU32(gs + 0x21e10, 0x2f4, 0);
    }
    for (int32_t i = 0; i < I32(mgr, 8); i++) {
        const rx_addr act = U32(U32(mgr, 4), 4 * i);
        if (U32(act, 0x21c) != 5 && U32(act, 0x260) == 0) {
            typedef void (*fn_t)(rx_addr, float);
            const rx_addr fn = race_vfn(act, 23);
            fn_t f = RX_FN(fn, fn_t);
            if (f) RACE_CORE_CALL(1, f(act, dt));
            else { uint32_t v = rx_fb(dt); rx_call_original(fn, RX_THISCALL, act, 1, &v); }
        } else if (U32(act, 0xfe4) != 0) {
            RACE_CORE_CALL(0, rx_scene_views_reset());     /* 00559ac0 */
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055f700, rx_activity_manager_update)

/* 00552930 (EDI = the players): under the profiler scope, the deferred work the players flagged: +0xdfc0 FUN_00553a50, +0xdfc4
 * FUN_00553c60 (stdcall players), +0xe044 Game_SetupActivityArea once the game (0xa946b8) has its area (+0x15b54). */
void rx_players_deferred(rx_addr players) {
    RACE_FRAME_PLAIN(F, 0, 3);
    race_static_dcf304();
    const rx_addr scope = race_profile_begin();
    if (U32(players, 0xdfc0) != 0) { RACE_CORE_CALL(1, rx_players_mark_disconnected(players)); SU32(players, 0xdfc0, 0); }   /* 00553a50 */
    if (U32(players, 0xdfc4) != 0) { RACE_CORE_CALL(1, rx_players_remove_left(players)); SU32(players, 0xdfc4, 0); }          /* 00553c60 */
    if (U32(players, 0xe044) != 0) {
        const rx_addr game = U32(0, 0xa946b8);
        if (U32(game, 0x15b54) != 0) {
            RACE_CORE_CALL(0, rx_game_setup_activity_area(game));
            SU32(players, 0xe044, 0);
        }
    }
    race_profile_end(scope);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00552930, rx_players_deferred)

/* The stats clock (FUN_006def20, EAX = the stats root -> xmm0). */
static float stats_clock(rx_addr root) {
    float t;
    RACE_CORE_CALL(0, t = rx_world_timer_seconds(root));
    return t;
}

/* The stats object's vf12 (slot 12: set a presence / stat value: id, key, value). The object (activity +0xfec / +0xff0 path:
 * [0x00da6cb0], the PCSession that MXGame::Startup builds through 006de390) has PCSession::vf12 006e9e00 in that slot: the
 * Steam presence / matchmaking record (+0x1d0c / [0x00da6cac] controller records, the "Presence" strings) and the Quazal
 * NetworkSession presence (NetworkSession::vf12 006e3a90), both entered only with the online-session word (+0x1c4c) set; offline
 * both return at once. ONLINE PRESENCE BOUNDARY: the product has no PCSession (the Quazal/Steam library is not core and the
 * product's offline stand-in [0x00da6cb0] is 0: QUEUE "Product driver chain"), so there is no presence to set; the caller's local
 * stats clock (activity +0xfec / +0xff0) is written either way. Oracle: the original (Verify compares it); product: nothing when
 * the session is the offline stand-in. Nothing gameplay reads the presence record (the +0x1d0c writes are this function's own). */
static void stats_set(rx_addr stats, uint32_t id, uint32_t key, int32_t value) {
#ifndef RX_ORACLE
    if (U32(0, 0x00da6cb0) == 0) { (void)stats; (void)id; (void)key; (void)value; return; }   /* the offline stand-in */
#endif
    race_thiscall(race_vfn(stats, 12), stats, 3, id, key, (uint32_t)value, 0);
}

/* 0055cf10 (EDI = activity): the online presence: every 60 s (the stats clock - +0xfec) the player count (+0x280 + drivers +0x26c,
 * key 0x12); every 20 s in a race in progress (+0xff0; the game in view-track mode 0075c5f0) the elapsed minutes + 1 (+0x294 / 60,
 * key 0x15), the +0x2bc minutes (key 0x16) and FUN_006decd0 (the clock +0x294). */
void rx_activity_presence(rx_addr act) {
    RACE_FRAME_EBP(F, 8, 8, 2);
    race_static_dcf304();
    float now = stats_clock(0x00da6ca0) - F32(act, 0xfec);
    if (now > 60.0f) {
        const uint32_t extra = U32(act, 0x280), n = U32(act, 0x26c);
        race_static_dcf304();
        const uint32_t id = U32(U32(0, 0xda6cb0), 0x1a0);
        race_static_dcf304();
        stats_set(U32(0, 0xda6cb0), id, 0x12, (int32_t)(extra + n));
        race_static_dcf304();
        SF32(act, 0xfec, stats_clock(0x00da6ca0));
    }
    uint32_t view;
    RACE_CORE_CALL(0, view = rx_game_is_view_track(U32(0, 0xa946b8)));
    if (view == 0 || U32(act, 0x21c) != 2) { RACE_FRAME_END(F); return; }
    race_static_dcf304();
    now = stats_clock(0x00da6ca0) - F32(act, 0xff0);
    if (!(now > 20.0f)) { RACE_FRAME_END(F); return; }
    const int32_t mins = rx_cvtt(F32(act, 0x294)) / 60 + 1;
    uint32_t id = U32(U32(stats_root(), 0x10), 0x1a0);
    stats_set(U32(stats_root(), 0x10), id, 0x15, mins);
    id = U32(U32(stats_root(), 0x10), 0x1a0);
    const int32_t m2 = rx_cvtt(F32(act, 0x2bc)) / 60;
    stats_set(U32(stats_root(), 0x10), id, 0x16, m2);
    SF32(act, 0xff0, stats_clock(stats_root()));
    const rx_addr root = stats_root();
    RACE_CORE_CALL(1, rx_fx_session_clock(root, F32(act, 0x294)));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055cf10, rx_activity_presence)

/* 00585c60 (ESI = a std::map iterator {+0 container proxy, +4 node}) -> EAX: the node's value (+8). */
rx_addr rx_map_iterator_value(rx_addr it) { return U32(it, 4) + 8; }
RX_FUNCTION(0x00585c60, rx_map_iterator_value)

/* 0058f270 (shared slot 2 of the EventHandlerProxy templates; ECX = proxy {+0xc object, +0x10 member function}, arg: the payload
 * pointer; RET 4): calls the handler with the payload's three words. */
void rx_event_proxy_call3(rx_addr proxy, rx_addr payload) {
    const rx_addr p = U32(payload, 0);
    uint32_t a[3] = {U32(p, 0), U32(p, 4), U32(p, 8)};
    rx_call_original(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0xc), 3, a);
}
RX_FUNCTION(0x0058f270, rx_event_proxy_call3)

/* 00555680 -> EAX: any pending network player joins (0xa81ac8 + 0xa81ad0 + 0xa81ad8 > 0). */
uint32_t rx_net_pending_players(void) { return (int32_t)(U32(0, 0xa81ad0) + U32(0, 0xa81ad8) + U32(0, 0xa81ac8)) > 0; }
RX_FUNCTION(0x00555680, rx_net_pending_players)

/* 0055eb00 BaseScenario::vf7 (ECX = scenario) -> EAX: the scenario in state 15 (+0x151b4). */
uint32_t rx_scenario_done(rx_addr sc) { return U32(sc, 0x151b4) == 0xf; }
RX_FUNCTION(0x0055eb00, rx_scenario_done)

/* 0055e710 Activity::vf11 (ECX = activity) -> EAX: may the race start: not with a session result (+0xfe8), not in session state
 * 2, not while 0xae1b90; kind 5 waits for the stats (FUN_00402540 on the stats root +0x10); then unless the front end (FUN_00413d20)
 * is in mode 1 / 2 with a screen (+0x18c) whose state (+0xc, or +0x10 when 2) is 1. */
uint32_t rx_activity_can_start(rx_addr act) {
    if (U32(act, 0xfe8) != 0 || U32(act, 0x238) == 2 || U32(0, 0xae1b90) != 0) return 0;
    if (U32(act, 0x23c) == 5) {
        if (rx_session_is_networked(U32(stats_root(), 0x10)) != 0) return 0;     /* 00402540 */
    }
    rx_addr series;
    RACE_CORE_CALL(0, series = rx_fx_series_manager());
    const uint32_t mode = U32(series, 0x190);
    if (mode != 2) {
        RACE_CORE_CALL(0, series = rx_fx_series_manager());
        if (U32(series, 0x190) != 1) return 1;
    }
    RACE_CORE_CALL(0, series = rx_fx_series_manager());
    const rx_addr scr = U32(series, 0x18c);
    if (scr == 0) return 1;
    uint32_t s = U32(scr, 0xc);
    if (s == 2) s = U32(scr, 0x10);
    return s != 1;
}
RX_FUNCTION(0x0055e710, rx_activity_can_start)

/* 005651b0 MXActivityRace::vf26 (ECX = activity, arg dt; RET 4): Countdown: with a start board (+0x2cc) outside a network race
 * the board drives it: in board state 0 / 4 (+0x480) once its timer (+0x4f4) went below 0: "Countdown Step" (0) and InProgress
 * (SetState 2), the intro time (+0x2a8) cleared; otherwise the activity's countdown (Activity::vf26). */
void rx_race_countdown(rx_addr act, float dt) {
    const rx_addr board = U32(act, 0x2cc);
    RACE_FRAME_PLAIN(F, 0, 1);   /* push edi (and push esi on the board path) */
    if (board != 0) {
        race_static_dcf304();
        const rx_addr g = U32(0, 0xda6cb0);
        if (U32(g, 0x1c4c) == 0 || U32(g, 0x1b0) == 1) {
            const rx_addr b = U32(act, 0x2cc);
            const uint32_t st = U32(b, 0x480);
            if ((st != 4 && st != 0) || !(0.0f > F32(b, 0x4f4))) { RACE_FRAME_END(F); return; }
            rx_set_sp(F - 4);
            RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), 0x00941bdc /* "Countdown Step" */, act, 0));
            RACE_CORE_CALL(2, rx_activity_set_state(act, 2));
            SF32(act, 0x2a8, 0.0f);
            RACE_FRAME_END(F);
            return;
        }
    }
    RACE_CORE_CALL(1, rx_activity_countdown(act, dt));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005651b0, rx_race_countdown)

/* ---- free ride ---------------------------------------------------------------------------------------------------------------- */

/* 005851f0 (ECX = the free ride jump tracker (activity +0xff080), EAX = activity, ESI = the local player or 0; arg dt; RET 4): a
 * pending jump (+0x100: FUN_00585330 (activity, player)); the landing delay (+0xb0) is cancelled while the rider is crashed
 * (+0x1ac +0x280) and counts down by dt; at 0 the jump's horizontal length (take-off +0xc0 / +0xc8 to landing +0xd0 / +0xd8) is
 * awarded (FUN_0054e670 (player, 2, medal, length)) for each of the three medal distances of the level (+0x234 +0x22140: +0xf0,
 * +0xfc, +0x108) it beats. Frame: and esp, -16; sub 0xc; push edi. */
void rx_free_ride_jumps(rx_addr tr, rx_addr act, rx_addr player, float dt) {
    RACE_FRAME_EBP(F, 16, 0xc, 1);
    if (U32(tr, 0x100) != 0) {
        RACE_CORE_CALL(2, rx_free_ride_discoveries(tr, act, player));          /* 00585330 (EDI = the tracker; race_activity_misc.c) */
    }
    if (F32(tr, 0xb0) > 0.0f && U32(U32(player, 0x1ac), 0x280) != 0) SF32(tr, 0xb0, 0.0f);
    float left = F32(tr, 0xb0);
    if (left > 0.0f) {
        left = left - dt;
        SF32(tr, 0xb0, left);
        if (!(0.0f < left) && left == left) {
            const float dz = F32(tr, 0xd8) - F32(tr, 0xc8);
            const float dx = F32(tr, 0xd0) - F32(tr, 0xc0);
            const float len = rx_sqrts(dz * dz + dx * dx);
            const rx_addr lv = U32(U32(U32(player, 0x94), 0x234), 0x22140);
            const float silver = F32(lv, 0xfc), gold = F32(lv, 0x108);
            for (uint32_t k = 0; k < 3; k++) {
                const float lim = k == 0 ? F32(lv, 0xf0) : k == 1 ? silver : gold;
                if (len > lim) RACE_CORE_CALL(4, rx_challenge_award(player, 2, k, rx_fb(len)));      /* 0054e670 (race_challenge.c) */
            }
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005851f0, rx_free_ride_jumps)

/* 005609d0 MXActivityFreeRide::vf28 (ECX = activity, arg dt; RET 4): InProgress: the activity's (Activity::vf28); a pending
 * notice (+0xff070) raises the session's (0xa93fcc +0x98) void() event 0x941f90 after 5 s (+0xff074); the free ride menus
 * (+0x890 while +0xbb0 and +0xb8c; the game state's +0x21e10 while +0x22130 and +0x2210c: FUN_005399e0); in progress with exactly
 * one local player (the game's player slots 0xa946b8 +0x153ec, count byte +0x1541c, players of 0x12a0 bytes at +0x7460 with
 * +0x10 == 0) and unless the audio settings say otherwise (+0x234 +0x20018) its jumps are tracked (005851f0). Frame: and esp, -8;
 * push ecx; 3 pushes. */
void rx_free_ride_in_progress(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 4, 3);
    RACE_CORE_CALL(1, rx_activity_in_progress(act, dt));
    if (U32(act, 0xff070) != 0) {
        const float t = dt + F32(act, 0xff074);
        SF32(act, 0xff074, t);
        if (!(t < 5.0f) && t == t) {
            const rx_addr mgr = rx_ui_event_channel();   /* [0xa93fcc] + 0x98 */
            SU32(act, 0xff070, 0);
            uint32_t node = 0x009e1a7c;
            crt_type_info_name(0x009cf3f0, node);   /* type_info::_name_internal_method (IAT 0x916388) */   /* type_info void() name */
            RACE_CORE_CALL(2, rx_world_event_dispatch(mgr, 0x00941f90, 0, 0));
        }
    }
    if (U32(act, 0xbb0) != 0 && U32(act, 0xb8c) != 0) RACE_CALL_SP(0, rx_waypoint_track_debug(act + 0x890));      /* 005399e0 (ECX = the track) */
    const rx_addr gs = U32(act, 0x234) + 0x21e10;
    if (gs != 0 && U32(gs, 0x320) != 0 && U32(gs, 0x2fc) != 0) RACE_CALL_SP(0, rx_waypoint_track_debug(gs));
    if (U32(act, 0x21c) == 2) {
        const rx_addr game = U32(0, 0xa946b8), base = game + 0x7460, slots = base + 0xdf8c;
        const uint32_t n = U8(game, 0x1541c);
        uint32_t locals = 0;
        for (uint32_t i = 0; i < n; i++) if (U32(rx_seat_active(base, i), 0x10) == 0) locals++;
        if (n != 0 && locals == 1) {
            rx_addr player = 0;
            if (I32(slots, 0) != -1) {
                const uint32_t m = U8(base, 0xdfbc);
                for (uint32_t i = 0; i < m; i++)
                    if (U32(rx_seat_active(base, i), 0x10) == 0) { player = rx_seat_active(base, i); break; }
            }
            if (U32(U32(act, 0x234), 0x20018) == 0) RACE_CORE_CALL(1, rx_free_ride_jumps(act + 0xff080, act, player, dt));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005609d0, rx_free_ride_in_progress)

/* 005604f0 MXActivityFreeRide::vf25 (ECX = activity, arg dt; RET 4): Pre: the activity's (Activity::vf25); then while the free
 * ride's loading screen (+0xff200, vf8 not done) is not up, no session result, 0xd60ff0 set, not 0xae1b90 and no network joins:
 * after the +0x1143d0 frame delay with the screen ready (vf7) the level is set up (FUN_0074ada0 EAX = activity, ECX = +0x234
 * +0x20150; with FUN_00747810 the session's 0x941f7c message FUN_0059b7e0) and +0xff1f0 set; while not ready the screen updates
 * (FUN_007473f0 (screen, dt)). */
void rx_free_ride_pre(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_pre(act, dt));
    if (U32(U32(act, 0x234), 0x20018) == 0 && U32(act, 0xff068) == 0) {
        const rx_addr scr = act + 0xff200;
        if (race_thiscall(race_vfn(scr, 8), scr, 0, 0, 0, 0, 0) == 0 && U32(act, 0xfe8) == 0 && U32(0, 0xd60ff0) != 0 &&
            U32(0, 0xae1b90) == 0 && rx_net_pending_players() == 0) {
            const int32_t c = I32(act, 0x1143d0);
            SU32(act, 0x1143d0, (uint32_t)(c - 1));
            if (c <= 0 && race_thiscall(race_vfn(scr, 7), scr, 0, 0, 0, 0, 0) != 0) {
                RACE_CORE_CALL(0, rx_podium_setup(act, U32(act, 0x234) + 0x20150));            /* 0074ada0 (anim_scenario_misc.c) */
                uint32_t started;
                RACE_CORE_CALL(0, started = rx_scenario_start(scr));      /* 00747810, ESI = the screen */
                if (started != 0) {
#ifdef RX_ORACLE /* UI-only transition to the intro page; product has no original UI manager. */
                    rx_regs in; memset(&in, 0, sizeof in); in.r[0] = 0x00941f7c; in.r[1] = U32(0, 0xa93fcc);
                    uint32_t z = 0;
                    rx_call_original_ex(0x0059b7e0, &in, 0, 1, &z);
#endif
                }
                SU32(act, 0xff1f0, 1);
            }
            if (race_thiscall(race_vfn(scr, 7), scr, 0, 0, 0, 0, 0) == 0) {
                RACE_CORE_CALL(2, rx_scenario_intro_update(scr, dt));
            }
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005604f0, rx_free_ride_pre)

/* 00560b70 MXActivityFreeRide::vf42 (ECX = activity): the end of a timed free ride: (outside a network race, or a replay / result)
 * once the time limit (+0x2bc > 0) is reached by the clock (+0x294): WindDown (SetState 3). */
void rx_free_ride_check_finish(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 2);
    race_static_dcf304();
    const rx_addr g = U32(0, 0xda6cb0);
    if (U32(g, 0x1c4c) != 0 && U32(g, 0x1b0) != 1) {
        race_static_dcf304();
        if (U32(U32(0, 0xda6cb0), 0x1c48) == 0 && U32(act, 0xbc0) == 0) { RACE_FRAME_END(F); return; }
    }
    const float lim = F32(act, 0x2bc);
    if (U32(U32(act, 0x234), 0x20018) == 0 && lim > 0.0f && !(F32(act, 0x294) < lim)) RACE_CORE_CALL(2, rx_activity_set_state(act, 3));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00560b70, rx_free_ride_check_finish)

/* ---- pads --------------------------------------------------------------------------------------------------------------------- */

/* 0055a1e0 (EAX = enable, arg: activity; RET 4): enables / disables the race controls of every driver's player with a pad
 * (+0x1f0 >= 0): the player's own groups (FUN_0054fcd0 / 0054fd20, EAX = player; 0054fdc0 / 0054fe10 / 0054fd70 / 0054fe60,
 * ECX = player, EDX = enable) and five key maps of the pad's control blocks (0x9c52c4[pad] +0x348 / +0x9ec / +0x127c: vf9 (pad,
 * enable); FUN_00548bf0 / 00548c40: ESI = pad, EDI = enable); a network player (+0x11c8) instead keeps the wanted state as bits
 * 1, 2, 4, 5, 6 of +0x11cc. */
static void pad_group(rx_addr p, uint32_t bit, uint32_t enable, uint32_t block, rx_addr fn) {
    const int32_t pad = I32(p, 0x1f0);
    if (pad < 0) return;
    if (U32(p, 0x11c8) != 0) {
        SU32(p, 0x11cc, enable != 0 ? (U32(p, 0x11cc) | bit) : (U32(p, 0x11cc) & ~bit));
    } else if (block != 0) {
        const rx_addr km = U32(0, 0x9c52c4 + 4 * (uint32_t)pad) + block;
        race_thiscall(race_vfn(km, 9), km, 2, (uint32_t)pad, enable, 0, 0);
    } else {
        if (fn == 0x00548bf0) RACE_CALL_SP(0, rx_player_slot_group2((uint32_t)pad, enable));
        else RACE_CALL_SP(0, rx_player_slot_group5((uint32_t)pad, enable));   /* 00548c40 */
    }
}
static void player_call(rx_addr fn, rx_addr p, uint32_t enable, int reg_ecx) {
    (void)reg_ecx;
    switch (fn) {
    case 0x0054fcd0: RACE_CALL_SP(0, rx_player_set_group0(p, enable)); break;
    case 0x0054fd20: RACE_CALL_SP(0, rx_player_set_group3(p, enable)); break;
    case 0x0054fd70: RACE_CALL_SP(0, rx_player_set_group_147c(p, enable)); break;
    case 0x0054fdc0: RACE_CALL_SP(0, rx_player_set_group_167c(p, enable)); break;
    case 0x0054fe10: RACE_CALL_SP(0, rx_player_set_group_748(p, enable)); break;
    default:         RACE_CALL_SP(0, rx_player_set_group_f34(p, enable)); break;   /* 0054fe60 */
    }
}
void rx_activity_enable_controls(uint32_t enable, rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 4);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr p = U32(U32(U32(act, 0x268), 4 * i), 0x74);
        if (I32(p, 0x1f0) < 0) continue;
        player_call(0x0054fcd0, p, enable, 0);
        pad_group(p, 0x02, enable, 0x348, 0);
        pad_group(p, 0x04, enable, 0, 0x00548bf0);
        player_call(0x0054fd20, p, enable, 0);
        pad_group(p, 0x10, enable, 0x9ec, 0);
        pad_group(p, 0x20, enable, 0, 0x00548c40);
        pad_group(p, 0x40, enable, 0x127c, 0);
        player_call(0x0054fdc0, p, enable, 1);
        player_call(0x0054fe10, p, enable, 1);
        player_call(0x0054fd70, p, enable, 1);
        player_call(0x0054fe60, p, enable, 1);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055a1e0, rx_activity_enable_controls)

/* 00502870 (EAX = a std::map<std::string, ...> keyed case-insensitively, arg: the key string; RET 4) -> EAX: the lower bound
 * node (_stricmp; the head when every key is smaller). Node: +0 left, +4 parent, +8 right, +0x10 key, +0x35 is-nil. */
rx_addr rx_name_map_lower_bound(rx_addr map, rx_addr key) {
    rx_addr best = U32(map, 0x18);
    rx_addr n = U32(best, 4);
    const rx_addr k = U32(key, 0x18) >= 0x10 ? U32(key, 4) : key + 4;
    while (U8(n, 0x35) == 0) {
        const rx_addr nk = U32(n, 0x24) >= 0x10 ? U32(n, 0x10) : n + 0x10;
        if (crt_stricmp(nk, k) < 0) n = U32(n, 8);
        else { best = n; n = U32(n, 0); }
    }
    return best;
}
RX_FUNCTION(0x00502870, rx_name_map_lower_bound)

/* ---- respawn ------------------------------------------------------------------------------------------------------------------ */

/* 00569010 MXActivityRaceStub::vf19 (ECX = activity, args: player, position out, direction out, unused; RET 0x10) -> EAX: where
 * a reset player restarts. The driver's current track node (+0x90; none: 0) is moved forward past nodes flagged "no respawn" for
 * the vehicle's class (node +0x68 + 4 * (class * 4 + kind) bit 1), through the following sections, never onto the start node
 * (track +0x9c: the next one); the driver's node / section (+0x14 / +0x18 / +0x1c) follow. The position is the node's centre
 * moved across the track (towards the side +0x10 or +0x20 the rider is on, by half the node's width), the direction the node's
 * (+0x50). Frame: and esp, -16; sub 0x44; 3 pushes (the vehicle position at +0x10). */
uint32_t rx_race_stub_respawn_point(rx_addr act, rx_addr player, rx_addr pos, rx_addr dir) {
    RACE_FRAME_EBP(F, 16, 0x44, 3);
    uint32_t idx;
    RACE_CORE_CALL(0, idx = (uint32_t)rx_world_group_rider_index(act, player));
    const rx_addr d = U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4 * idx);
    rx_addr node = U32(d, 0x90);
    if (node == 0) { RACE_FRAME_END(F); return 0; }
    rx_addr sec = U32(d, 0x14);
#define RACE_NODE_CLASS(n) ((U8((n), 0x68 + 4 * (U32(U32(U32(d, 0x74), 0x1ac), 0x198) + 4 * U32(U32(U32(d, 0x74), 0x1ac), 0x194))) & 2) != 0)
    if (RACE_NODE_CLASS(node)) {
        const rx_addr start = U32(U32(d, 0x10), 0x9c);
        rx_addr n = node;
        for (;;) {
            if (n == start) { node = U32(d, 0x18); break; }
            if (!RACE_NODE_CLASS(n)) { node = n; break; }
            n = U32(n, 0x64);
            if (n != 0) continue;
            sec = U32(U32(sec, 8), 0);
            n = U32(sec, 0);
            if (n == 0) break;
        }
    }
#undef RACE_NODE_CLASS
    /* A jump lip or a static prop can immediately wreck a recovered bike again.
     * After repeated immediate failures, retreat along the same section
     * for a run-up. Never advance progress, cross the finish or skip a sector. */
    if (fmt_modtrack_racing() && fmt_modtrack_recovery_retry(d,node,F32(d,0xf20))) {
        for(int i=0;i<3;i++) {
            rx_addr previous=U32(node,0x60);
            if(!previous || !U32(previous,0x60) || previous==U32(U32(d,0x10),0x9c))break;
            node=previous;
        }
    }
    if (node == U32(U32(d, 0x10), 0x9c)) node = U32(node, 0x60);
    SU32(d, 0x14, sec);
    SU32(d, 0x18, U32(node, 0x60));
    SU32(d, 0x1c, node);
    if (U32(node, 0x60) == 0) {
        SU32(d, 0x14, U32(U32(sec, 8), 0));
        SU32(d, 0x18, U32(sec, 0));
        SU32(d, 0x1c, U32(U32(sec, 0), 0x64));
    }
    const rx_addr p = U32(d, 0x1c);
    SU32(d, 0x90, p);
    const float bx = F32(p, 0), by = F32(p, 4), bz = F32(p, 8);
    const rx_addr veh = U32(U32(d, 0x74), 0x1ac);
    race_thiscall(race_vfn(veh, 30), veh, 1, F + 0x10, 0, 0, 0);       /* the vehicle position */
    /* the rider's horizontal offset from the node, normalized */
    const float ox = F32(F, 0x10) - F32(p, 0), oz = F32(F, 0x18) - F32(p, 8);
    float nx = ox, ny = 0.0f, nz = oz;
    const float l2 = oz * oz + ox * ox;
    if (l2 > 0.0f) { const float inv = 1.0f / rx_sqrts(l2); nx = ox * inv; ny = inv * 0.0f; nz = oz * inv; }
    SF32(F, 0x10, nx); SF32(F, 0x14, ny); SF32(F, 0x18, nz);
    /* the node's cross-track axis (+0x10 -> +0x20), normalized */
    float ex = F32(p, 0x20) - F32(p, 0x10), ey = F32(p, 0x24) - F32(p, 0x14), ez = F32(p, 0x28) - F32(p, 0x18);
    const float e2 = ez * ez + ey * ey + ex * ex;
    if (e2 > 0.0f) { const float inv = 1.0f / rx_sqrts(e2); ex = ex * inv; ey = ey * inv; ez = ez * inv; }
    const float side = ez * nz + ex * nx + ey * ny;
    const rx_addr edge = 0.0f > side ? p + 0x10 : p + 0x20;
    const float hy = (F32(edge, 4) - F32(p, 4)) * 0.0f, hz = F32(edge, 8) - F32(p, 8), hx = F32(edge, 0) - F32(p, 0);
    const float half = rx_sqrts(hz * hz + hy * hy + hx * hx) * 0.5f;
    const float m = fmt_modtrack_racing() ? 0.f : (side >= 0.0f ? 1.0f : F32(0, 0x9cd9e8) /* -1 */) * half;
    SF32(pos, 0, m * ex + bx);
    SF32(pos, 4, ey * m + by);
    SF32(pos, 8, ez * m + bz);
    SU32(dir, 0, U32(p, 0x50)); SU32(dir, 4, U32(p, 0x54)); SU32(dir, 8, U32(p, 0x58));
    RACE_FRAME_END(F);
    return 1;
}
RX_FUNCTION(0x00569010, rx_race_stub_respawn_point)

/* ---- scripted objects --------------------------------------------------------------------------------------------------------- */

/* The placement of a scripted record (+0x88 the locator's name, +0xd0 its own 4x4 matrix) into the 0x40-byte matrix at m: the
 * record's matrix, replaced by the named locator's (FUN_0074b680 (level +0x20150, name), else FUN_0074b970 (ESI = the level):
 * its vf14) or, without either, the record's matrix transformed by the level's (FUN_00401830: ECX = m times EAX = the level
 * +0x20150 matrix, into m). */
static void record_matrix(rx_addr act, rx_addr rec, rx_addr m) {
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(m, i, U32(rec, 0xd0 + i));
    const rx_addr name = rec + 0x88;
    rx_addr loc = 0;
    if (U8(name, 0) != 0) {
        RACE_CORE_CALL(2, loc = rx_scenario_anims_find_prop(U32(act, 0x234) + 0x20150, name));
        if (loc == 0) RACE_CORE_CALL(0, loc = rx_scenario_anims_find_entity(U32(act, 0x234) + 0x20150, name));
        if (loc != 0) {
            const rx_addr src = race_thiscall(race_vfn(loc, 14), loc, 0, 0, 0, 0, 0);
            for (uint32_t i = 0; i < 0x40; i += 4) SU32(m, i, U32(src, i));
            return;
        }
    }
    RACE_CORE_CALL(1, rx_fx_matrix4_mul(m, U32(act, 0x234) + 0x20150, m));
}

/* The world object of a record (FUN_007d2e70 (the world +0x234 +0x20138, type, the record's name +8)) of the level's world
 * (+0x234 +0x20150 +0x40), or 0. */
static rx_addr record_object(rx_addr act, uint32_t type, rx_addr rec) {
    const rx_addr w = U32(U32(act, 0x234) + 0x20150, 0x40);
    if (w == 0) return 0;
    rx_addr obj;
    RACE_CORE_CALL(3, obj = rx_scene_manager_find_typed(U32(U32(w, 0x234), 0x20138), type, rec + 8));
    return obj;
}

/* 0055d9d0 (ECX = activity, args: record, unused; RET 8): places the record's object of type 0x102e (vf16 (matrix)). Frame:
 * and esp, -16; sub 0x44; 3 pushes (the matrix at +0x10). */
void rx_activity_place_object(rx_addr act, rx_addr rec) {
    RACE_FRAME_EBP(F, 16, 0x44, 3);
    const rx_addr obj = record_object(act, 0x102e, rec);
    if (obj != 0) {
        record_matrix(act, rec, F + 0x10);
        race_thiscall(race_vfn(obj, 16), obj, 1, F + 0x10, 0, 0, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055d9d0, rx_activity_place_object)

/* 0055d870 (ECX = activity, args: record, unused; RET 8): the same for the type 0x102f objects (the matrix first). */
void rx_activity_place_object2(rx_addr act, rx_addr rec) {
    RACE_FRAME_EBP(F, 16, 0x44, 3);
    record_matrix(act, rec, F + 0x10);
    const rx_addr obj = record_object(act, 0x102f, rec);
    if (obj != 0) race_thiscall(race_vfn(obj, 16), obj, 1, F + 0x10, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055d870, rx_activity_place_object2)

/* 0055e100 (stdcall activity, name, arg; RET 0xc) -> EAX: starts the scripted sequence `name` (one of the 12 slots: names at
 * +0xfe330, 0x104 each; sequences at +0x1070 + 0x15190 * i): a running one restarts (+0x15188 = 0, +0x1518c = 0); else the first
 * free slot (+0x15180 clear, or its time +0x15188 not below its end +0x15184) loads the "bxml" resource (engine 0xa95844 +8 vf27
 * (name, 'bxml', 0, 0); its +0x4c held while loading) into the slot (FUN_0074ce40 EAX = its data, EDI = the slot), names the slot,
 * sets +0x1518c = arg and runs its first step (FUN_0074cf70 ESI = the slot; 0.0001, activity); the clip recorder logs the name
 * (ClipRecorder_LogEffect EDX = name). 0 when no slot is free or the resource is missing. Frame: and esp, -8; push ecx; 3
 * pushes. */
uint32_t rx_activity_start_sequence(rx_addr act, rx_addr name, uint32_t arg) {
    RACE_FRAME_EBP(F, 8, 4, 3);
    uint32_t r = 0;
    for (uint32_t i = 0; i < 12; i++) {
        if (crt_stricmp(name, act + 0xfe330 + 0x104 * i) == 0) {
            const rx_addr sq = act + 0x1070 + 0x15190 * i;
            SF32(sq, 0x15188, 0.0f);
            SU32(sq, 0x1518c, 0);
            r = 1;
            goto done;
        }
    }
    {
        uint32_t i = 0;
        for (; i < 12; i++) {
            const rx_addr e = act + 0x161f8 + 0x15190 * i;
            if (U32(e, -8) == 0 || F32(e, 0) >= F32(e, -4)) break;
        }
        if (i == 12) goto done;
        const rx_addr sq = act + 0x1070 + 0x15190 * i;
        if (sq == 0) goto done;
        const rx_addr eng = U32(U32(0, 0xa95844), 8);
        uint32_t a[4] = {name, 0x6c6d7862 /* 'bxml' */, 0, 0};
        const rx_addr res = rx_call_original(race_vfn(eng, 27), RX_THISCALL, eng, 4, a);
        SU32(F, 0xc, res);
        if (res == 0) goto done;
        SU32(res, 0x4c, 1);
        const rx_addr data = U32(res, 0x44) != 0 ? U32(res, 0x44) - 0x460 : 0;
        rx_addr loaded;
        RACE_CORE_CALL(0, loaded = rx_scenario_cast_load_script(data, sq));
        if (loaded != 0) {
            race_format(act + 0xfe330 + 0x104 * i, 0x104, 0x009419e4 /* "%s" */, 1, &name);
            SF32(sq, 0x15188, 0.0f);
            SU32(sq, 0x1518c, arg);
            RACE_CORE_CALL(2, rx_scenario_script_run(sq, 9.999999747378752e-05f, act));
        }
        SU32(res, 0x4c, 0);
        RACE_CORE_CALL(0, rx_clip_recorder_log_effect(name));
        r = 1;
    }
done:
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x0055e100, rx_activity_start_sequence)

/* ---- race scripts (holeshot, final stretch) ----------------------------------------------------------------------------------- */

/* 00569730 (EDI = activity): every driver's vehicle's sound / effect object (+0x1ac +0x234) gets its vf22. */
void rx_activity_vehicle_fx_reset(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr o = U32(U32(U32(U32(U32(act, 0x268), 4 * i), 0x74), 0x1ac), 0x234);
        race_thiscall(race_vfn(o, 22), o, 0, 0, 0, 0, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569730, rx_activity_vehicle_fx_reset)

/* 0058fde0 (shared slot 2 of the four-argument EventHandlerProxy templates; ECX = proxy, arg: the payload pointer; RET 4). */
void rx_event_proxy_call4(rx_addr proxy, rx_addr payload) {
    const rx_addr p = U32(payload, 0);
    uint32_t a[4] = {U32(p, 0), U32(p, 4), U32(p, 8), U32(p, 0xc)};
    rx_call_original(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0xc), 4, a);
}
RX_FUNCTION(0x0058fde0, rx_event_proxy_call4)

/* 0058e910 (slot 2 of EventHandlerProxy<void (ChallengeActivityAudioLogic::*)(Activity*, BaseDriver*, int, int, int)>, the "ChallengeAward" cue's proxy 0x009461c4;
 * ECX = proxy {+0xc object, +0x10 member function}, arg: the payload pointer; RET 4): calls the handler with the payload's five words. */
void rx_event_proxy_call5(rx_addr proxy, rx_addr payload) {
    const rx_addr p = U32(payload, 0);
    uint32_t a[5] = {U32(p, 0), U32(p, 4), U32(p, 8), U32(p, 0xc), U32(p, 0x10)};
    rx_call_original(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0xc), 5, a);
}
RX_FUNCTION(0x0058e910, rx_event_proxy_call5)

/* 00569d10 (stdcall activity; RET 4) -> EAX: do the race scripts run: in a series event (MXSeriesManager 0x9e2188, behind the
 * static guard 0x9e275c; mode 0x9e2318 1 / 2) of kind 1 / 2 / 5 / 8 (0x9e2198) whose event (0x9e2314; state +0xc, or +0x10 when
 * 2) is in state 5; outside a series when the session state (+0x238) is 1. */
static void series_guard(void) {
    if ((U8(0, 0x9e275c) & 1) == 0) {
        SU32(0, 0x9e275c, U32(0, 0x9e275c) | 1);
        RACE_CORE_CALL(0, (void)rx_series_manager_construct());   /* MXSeriesManager_Construct */
        RX_ATEXIT(0x00911da0);      /* _atexit */
    }
}
uint32_t rx_race_scripts_enabled(rx_addr act) {
    RACE_FRAME_PLAIN(F, 4, 3);
    uint32_t r = 0;
    series_guard();
    int series = U32(0, 0x9e2318) == 2;
    if (!series) { series_guard(); series = U32(0, 0x9e2318) == 1; }
    if (!series) r = U32(act, 0x238) == 1;
    else {
        series_guard();
        const rx_addr ev = U32(0, 0x9e2314);
        series_guard();
        const uint32_t kind = U32(0, 0x9e2198);
        if (kind == 1 || kind == 2 || kind == 5 || kind == 8) {
            uint32_t s = U32(ev, 0xc);
            if (s == 2) s = U32(ev, 0x10);
            r = s == 5;
        }
    }
    RACE_FRAME_END(F);
    return r;
}
RX_FUNCTION(0x00569d10, rx_race_scripts_enabled)

/* 00569e10 MXActivityRaceStub::vf53 (ECX = activity, arg: which; RET 4): starts a race script (0055e100) when they run: 0
 * "StartLineScript", 1 "HoleShotScript", 2 "FinalStretchScript", 3 "FinishLineScript" (2 / 3 not in kind 12). Frame: and esp,
 * -8; push ecx; push esi. */
void rx_race_start_script(rx_addr act, uint32_t which) {
    RACE_FRAME_EBP(F, 8, 4, 1);
    uint32_t on;
    RACE_CORE_CALL(1, on = rx_race_scripts_enabled(act));
    static const rx_addr names[4] = {0x00942088, 0x00942098, 0x009420a8, 0x009420bc};
    if (on != 0 && which <= 3 && (which < 2 || U32(act, 0x23c) != 0xc)) RACE_CORE_CALL(3, rx_activity_start_sequence(act, names[which], 0));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569e10, rx_race_start_script)

/* 00569eb0 (EAX = player, arg: activity; RET 4): the holeshot: the session's "HoleshotActivated" void(MXPlayer*) event (the
 * payload is the argument slot, overwritten with the player), the records (MXRecords_HoleshotWon ECX = 0xa946c0, EAX = the pad),
 * the driver's +0x7c, and the activity's "Holeshot" (activity, driver) event. */
void rx_race_holeshot(rx_addr player, rx_addr act) {
    const rx_addr slot = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 0, 4);
    const rx_addr mgr = rx_ui_event_channel();   /* the UI manager's channel [0xa93fcc] + 0x98 */
    SU32(slot, 0, player);
    uint32_t node = 0x009e1a7c;
    crt_type_info_name(0x009cbeb4, node);   /* type_info::_name_internal_method (IAT 0x916388) */
    RACE_CORE_CALL(2, rx_world_event_dispatch(mgr, 0x009420d0, slot, 4));   /* "HoleshotActivated" */
    RACE_CORE_CALL(0, rx_records_holeshot_won(U32(player, 0x1f0), U32(0, 0xa946c0)));
    uint32_t idx;
    RACE_CORE_CALL(0, idx = (uint32_t)rx_world_group_rider_index(act, player));
    const rx_addr d = U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4 * idx);
    SU32(d, 0x7c, 1);
    RACE_CORE_CALL(1, rx_event_activity_driver(U32(act, 0x290), 0x00942044 /* "Holeshot" */, act, d));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569eb0, rx_race_holeshot)

/* 00569f40 MXActivityRaceStub::vf59 (ECX = activity, args: vehicle, trigger; RET 8): a vehicle crossed a named trigger (+0xc0):
 * "HoleShot" (once, not in session state 8, not while the clip recorder plays back 0xa942b4 == 2): the holeshot (00569eb0) and,
 * without a session result, the holeshot script; "FinalStretch" (once) for the driver on its last lap: the final stretch
 * script. */
void rx_race_trigger(rx_addr act, rx_addr veh, rx_addr trig) {
    RACE_FRAME_PLAIN(F, 0, 4);
    if (U32(act, 0x238) != 8 && U32(act, 0x13e644) == 0 && U32(0, 0xa942b4) != 2 &&
        crt_stricmp(0x0094199c /* "HoleShot" */, trig + 0xc0) == 0) {
        RACE_CORE_CALL(1, rx_race_holeshot(U32(veh, 0x190), act));
        if (U32(act, 0xfe8) == 0) race_thiscall(race_vfn(act, 53), act, 1, 1, 0, 0, 0);
        SU32(act, 0x13e644, 1);
    }
    if (U32(act, 0x13e648) == 0 && U32(0, 0xa942b4) != 2 && crt_stricmp(0x0094198c /* "FinalStretch" */, trig + 0xc0) == 0) {
        rx_addr d;
        RACE_CORE_CALL(0, d = rx_activity_player_driver(U32(veh, 0x190), act));
        if (U32(d, 0xf1c) == U32(act, 0xff070) - 1) {
            if (U32(act, 0xfe8) == 0) race_thiscall(race_vfn(act, 53), act, 1, 2, 0, 0, 0);
            SU32(act, 0x13e648, 1);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00569f40, rx_race_trigger)

/* 0055e8b0 (ECX = activity, args: vehicle, trigger; RET 8): the activity's "VehicleEntered" handler (vehicle trigger channel
 * 0x00a89bc8, subscription 00587180; the proxy 00554fc0 passes the payload's {vehicle, trigger}). A trigger named "SCR_<name>"
 * (+0xc0) starts the scripted sequence <name> (0055e100 with the name's 4 bytes skipped), except while the activity is inactive
 * (kind 5) and the driver found by its vehicle (+0x1ac) is not local (+4 == 0); any other trigger goes to the race's vf59
 * (00569f40: the "HoleShot" / "FinalStretch" triggers). Frame: push ecx; push ebx, ebp, esi, edi. */
void rx_activity_vehicle_entered(rx_addr act, rx_addr veh, rx_addr trig) {
    RACE_FRAME_PLAIN(F, 4, 4);
    if (crt_strncmp(trig + 0xc0, 0x009419a8 /* "SCR_" */, 4) != 0) {
        race_thiscall(race_vfn(act, 59), act, 2, veh, trig, 0, 0);      /* MXActivityRaceStub::vf59 */
    } else {
        rx_addr p = 0;                                                  /* the driver whose vehicle is this one */
        const int32_t n = I32(act, 0x26c);
        for (int32_t i = 0; i < n; i++) {
            const rx_addr d = rx_stand(act, i);
            if (d == 0) continue;
            const rx_addr dp = U32(d, 0x74);
            if (dp == 0) continue;
            if (U32(dp, 0x1ac) == veh) { p = dp; break; }
        }
        if (!(U32(act, 0x23c) == 5 && p != 0 && U32(p, 4) == 0))
            RACE_CORE_CALL(3, rx_activity_start_sequence(act, trig + 0xc4, 0));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055e8b0, rx_activity_vehicle_entered)

/* ---- the activity's states: countdown, enter / exit -------------------------------------------------------------------------- */

/* The network players' iterator (FUN_0088c380-based, 0x10 bytes at it: +4 current entry, +8 valid): next (FUN_0088c1e0 (ECX =
 * it; arg)) and release (FUN_0088c190). */
/* BOUNDARY (online only): the Quazal selection iterator; only the network-session branch of the countdown exit (+0x1c4c != 0) uses it. */
static void net_iter_next(rx_addr it, uint32_t a) { rx_call_original(0x0088c1e0, RX_THISCALL, it, 1, &a); }
static void net_iter_release(rx_addr it) { rx_call_original(0x0088c190, RX_THISCALL, it, 0, 0); }

/* 00559190 Activity::vf26 (ECX = activity, arg dt; RET 4): Countdown. A front-end start (the game 0xa946b8 +0x15b64, not kind 8)
 * hands the named player's driver (+0x15b68: FUN_00552b20 ESI = the players) to vf14 (driver, 0), or without one FUN_00757f40 (0).
 * Network sessions clear the UI's +0x28c. With a session result (+0xfe8) the race starts at once. Until the stats clock reaches
 * the intro end (+0x2a8): every 0.2 s (0x9cf5a4) the replay of a network race waits for every remote player to be ready (+0x129
 * bit 4; then the intro ends 2 s later, +0xfe0) and the stats timer is set (FUN_006e4a90 (the intro end)). After it the count
 * (+0x2a0) steps down (one "Countdown Step" (count) each time the countdown time +0x29c falls to count * +0x2b0 / +0x2b8); below
 * 0: InProgress (SetState 2), +0x2a8 cleared. Frame: and esp, -8; sub 0x28; 2 pushes (the iterator at +0xc). */
void rx_activity_countdown(rx_addr act, float dt) {
    RACE_FRAME_EBP(F, 8, 0x28, 2);
    const rx_addr game = U32(0, 0xa946b8);
    if (U32(game, 0x15b64) != 0 && U32(act, 0x23c) != 8) {
        rx_addr found;
        RACE_CORE_CALL(1, found = rx_player_find_by_name(game + 0x7460, game + 0x15b68));   /* 00552b20 (ESI = the players) */
        rx_addr d = 0;
        if (found != 0) RACE_CORE_CALL(0, d = rx_activity_player_driver(found, act));
        if (d != 0) race_thiscall(race_vfn(act, 14), act, 2, d, 0, 0, 0);
        else RACE_CORE_CALL(1, rx_game_debug_camera_toggle(0));     /* 00757f40 */
        RACE_FRAME_END(F);
        return;
    }
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c4c) != 0 && U32(U32(0, 0xda6cb0), 0x1b0) != 1) SU32(U32(0, 0xa93fcc), 0x28c, 0);
    if (U32(act, 0xfe8) != 0) goto start;
    race_static_dcf304();
    if (F32(act, 0x2a8) > stats_clock(0x00da6ca0)) {
        const float t = F32(0, 0x9cf5a4) - dt;
        SF32(0, 0x9cf5a4, t);
        if (0.0f < t || t != t) goto done;
        if (U32(U32(stats_root(), 0x10), 0x1c48) != 0 && U32(act, 0xfe0) == 0) {
            const rx_addr it = F + 0xc;
            RACE_CORE_CALL(0, (void)rx_net_selection_begin_all(it));        /* 005544c0 (ESI = the iterator) */
            RACE_CORE_CALL(0, rx_net_selection_next(it));                   /* 00508880 */
            int ready = 1;
            while (U32(F, 0x14) != 0) {
                if ((U8(U32(F, 0x10), 0x129) & 0x10) == 0) { ready = 0; break; }
                net_iter_next(it, 0);
            }
            if (ready) {
                SF32(act, 0x2a8, stats_clock(stats_root()) + 2.0f);
                SU32(act, 0xfe0, 1);
            }
            net_iter_release(it);
        }
        SU32(F, 8, U32(act, 0x2a8));
        RACE_CORE_CALL(1, rx_session_pre_timer(U32(stats_root(), 0x10), F32(F, 8)));   /* 006e4a90 (ESI = the session) */
        SF32(0, 0x9cf5a4, 0.2f /* 0x92e388 */);
        goto done;
    }
    {
        const int32_t count = I32(act, 0x2a0);
        const float t = F32(act, 0x29c) - dt;
        const float at = (float)count * F32(act, 0x2b0) / (float)I32(act, 0x2b8);
        SF32(act, 0x29c, t);
        if (at < t || t != t) goto done;
        SU32(act, 0x2a0, (uint32_t)(count - 1));
        RACE_CORE_CALL(1, rx_event_activity_int(U32(act, 0x290), 0x00941bdc /* "Countdown Step" */, act, (uint32_t)count));
        if (I32(act, 0x2a0) >= 0) goto done;
    }
start:
    RACE_CORE_CALL(2, rx_activity_set_state(act, 2));
    SF32(act, 0x2a8, 0.0f);
done:
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559190, rx_activity_countdown)

/* 00558800 Activity::vf30 (ECX = activity, args: old state, new state; RET 8): the old state's Exit (vf31 Pre, vf32 Countdown,
 * vf33 InProgress, vf34 WindDown, vf35 Post) with the new state. */
void rx_activity_exit_state(rx_addr act, uint32_t old, uint32_t state) {
    if (old <= 4) race_thiscall(race_vfn(act, 31 + old), act, 1, state, 0, 0, 0);
}
RX_FUNCTION(0x00558800, rx_activity_exit_state)

/* 005595b0 Activity::vf33 (ECX = activity, arg: old state; RET 4): the InProgress exit (00558800's slot 31 + 2): every
 * standing driver's player (+0x74) has its stunt scorer (+0xc70) refreshed (006cc8b0). Frame: push ebp, edi (then ebx, esi). */
void rx_activity_in_progress_exit(rx_addr act, uint32_t old) {
    (void)old;
    RACE_FRAME_PLAIN(F, 0, 4);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = rx_stand(act, (uint32_t)i);
        const rx_addr scorer = U32(d, 0x74) + 0xc70;
        RACE_CORE_CALL(0, rx_fx_stunt_refresh(scorer));   /* 006cc8b0 (ESI = the scorer) */
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005595b0, rx_activity_in_progress_exit)

/* 0055a410 (ECX = activity): every driver's player gets +0x214 (racing). */
void rx_activity_players_racing(rx_addr act) {
    for (int32_t i = 0; i < I32(act, 0x26c); i++) SU32(U32(U32(U32(act, 0x268), 4 * i), 0x74), 0x214, 1);
}
RX_FUNCTION(0x0055a410, rx_activity_players_racing)

/* 00558f70 Activity::vf38 (ECX = activity, arg: old state; RET 4): Countdown Enter: the countdown time and count (+0x29c = +0x2b0,
 * +0x2a0 = +0x2b8), the replay wait cleared (+0xfe0), the controls disabled (0055a1e0), network sessions clear the UI's +0x28c,
 * every driver's vf5, +0x1068 cleared. */
void rx_activity_countdown_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 3);
    SU32(act, 0x29c, U32(act, 0x2b0));
    SU32(act, 0x2a0, U32(act, 0x2b8));
    SU32(act, 0xfe0, 0);
    RACE_CORE_CALL(1, rx_activity_enable_controls(0, act));
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c4c) != 0 && U32(U32(0, 0xda6cb0), 0x1b0) != 1) SU32(U32(0, 0xa93fcc), 0x28c, 0);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * i);
        race_thiscall(race_vfn(d, 5), d, 0, 0, 0, 0, 0);
    }
    SU32(act, 0x1068, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00558f70, rx_activity_countdown_enter)

/* 005593e0 Activity::vf39 (ECX = activity, arg: old state; RET 4): InProgress Enter: +0x1068 cleared, 0xa957d8 set, the sound
 * volume back to 1 (FUN_00805200), the players racing (0055a410), FUN_006ccbc0 (EAX = activity), network sessions set the UI's
 * +0x28c, the controls enabled, the activity's "Start" void(Activity*) event (the payload in the frame's first slot), the
 * finish countdown count (+0x2a0 = +0x2c0). */
void rx_activity_in_progress_enter(rx_addr act) {
    RACE_FRAME_PLAIN(F, 4, 3);
    SU32(act, 0x1068, 0);
    SU32(0, 0xa957d8, 1);
    rx_audio_set_3d_scale(0x3f800000);                      /* 00805200: 1.0f */
    rx_activity_players_racing(act);
    RACE_CORE_CALL(0, rx_fx_trick_table_count(act));
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c4c) != 0 && U32(U32(0, 0xda6cb0), 0x1b0) != 1) SU32(U32(0, 0xa93fcc), 0x28c, 1);
    RACE_CORE_CALL(1, rx_activity_enable_controls(1, act));
    const rx_addr mgr = U32(act, 0x290);
    SU32(F, 0xc, act);
    uint32_t node = 0x009e1a7c;
    crt_type_info_name(0x009cbd24, node);   /* type_info::_name_internal_method (IAT 0x916388) */
    RACE_CORE_CALL(2, rx_world_event_dispatch(mgr, 0x00941b94, F + 0xc, 4));   /* "Start" */
    SU32(act, 0x2a0, U32(act, 0x2c0));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005593e0, rx_activity_in_progress_enter)

/* 00559020 Activity::vf32 (ECX = activity, arg: new state; RET 4): Countdown Exit: every driver's vf6 (the start); an overtake
 * race (session state 6) staggers the AI's start delays (+0x13c = +0x130 + i * "MXGame/OvertakeStartDelayPerDriver", default 2;
 * the config 0xa95844 +0xc vf1 / vf17); network sessions clear the remote players' ready bit (+0x129 bit 4) and set +0x13a.
 * Frame: sub 0x28; 2 pushes (the delay at +8, the iterator at +0xc). */
void rx_activity_countdown_exit(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0x28, 2);
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr d = U32(U32(act, 0x268), 4 * i);
        typedef void (*fn_t)(rx_addr);
        const rx_addr fn = race_vfn(d, 6);
        fn_t f = RX_FN(fn, fn_t);
        if (f) RACE_CORE_CALL(0, f(d)); else race_thiscall(fn, d, 0, 0, 0, 0, 0);
    }
    if (U32(act, 0x238) == 6) {
        const rx_addr cfg = U32(U32(0, 0xa95844), 0xc);
        SF32(F, 8, 2.0f);
        const rx_addr e = race_thiscall(race_vfn(cfg, 1), cfg, 3, 0x0093e42c /* "MXGame" */, 0x00941dcc, 1, 0);
        if (e != 0) race_thiscall(race_vfn(cfg, 17), cfg, 3, e, F + 8, 1, 0);
        for (int32_t i = 0; i < I32(act, 0x26c) - 1; i++) {
            rx_addr d;
            RACE_CORE_CALL(2, d = rx_activity_driver_of_kind(act, 1, i));
            SF32(d, 0x13c, (float)i * F32(F, 8) + F32(d, 0x130));
        }
    }
    race_static_dcf304();
    if (U32(U32(0, 0xda6cb0), 0x1c4c) != 0 && U32(U32(0, 0xda6cb0), 0x1b0) != 1) {
        const rx_addr it = F + 0xc;
        RACE_CORE_CALL(0, (void)rx_net_selection_begin_ready(it));         /* 00585b20 (ESI = the iterator) */
        if (U32(F, 0xc) == 0) {
            SU32(F, 8, 0);
            uint32_t a = F + 8;
            /* BOUNDARY (online only): Quazal ref-counted handle assignment (0088bc40), inside the +0x1c4c != 0 session branch. */
            rx_call_original(0x0088bc40, RX_THISCALL, F + 0x10, 1, &a);
        } else {
            RACE_CORE_CALL(0, rx_net_selection_advance(it));                /* 00508800 (race_net_session.c) */
            net_iter_next(it, 1);
        }
        while (U32(F, 0x14) != 0) {
            const rx_addr p = U32(F, 0x10);
            SU8(p, 0x129, U8(p, 0x129) & 0xef);
            SU8(p, 0x13a, 1);
            net_iter_next(it, 0);
        }
        net_iter_release(it);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00559020, rx_activity_countdown_exit)

/* ---- small helpers ------------------------------------------------------------------------------------------------------------ */

/* 0055e2b0 (stdcall activity; RET 4): runs every scripted sequence still before its end to the end (FUN_0074cf70 ESI = the
 * slot; (end - time) + 0.1, activity). */
void rx_activity_finish_sequences(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 4);
    for (uint32_t i = 0; i < 12; i++) {
        const rx_addr e = act + 0x161f8 + 0x15190 * i;
        const float t = F32(e, 0), end = F32(e, -4);
        if (end > t) {
            RACE_CORE_CALL(2, rx_scenario_script_run(e - 0x15188, end - t + 0.1f /* 0x931d94 */, act));
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055e2b0, rx_activity_finish_sequences)

/* The current view set's cameras (engine 0xa95844: the view set +0x1c + 4 * +4; cameras +0x19b8, count +0x19d0). */
static rx_addr view_set(void) { const rx_addr e = U32(0, 0xa95844); return U32(e, 0x1c + 4 * U32(e, 4)); }

/* 00550350 (ECX = player): a local human's (+4 set, +0x10 == 0) camera (by its view +8) flashes (camera +0x70: mode 2 at
 * strength 0x9c5428, colour 0x9ca220 .. 0x9ca228, alpha 1) unless the strength is 0. */
void rx_player_camera_flash(rx_addr p) {
    if (U32(p, 4) == 0 || U32(p, 0x10) != 0) return;
    const float k = F32(0, 0x9c5428);
    if (k == 0.0f) return;
    const rx_addr cam = U32(view_set(), 0x19b8 + 4 * U32(p, 8));
    if (cam == 0) return;
    const rx_addr c = U32(cam, 0x70);
    SU32(c, 0, 0);
    const rx_addr e = c + 0x10;
    if (e == 0) return;
    SF32(e, 0xc, k);
    SU32(e, 0x30, 1);
    SU32(e, 0x20, U32(0, 0x9ca220)); SU32(e, 0x24, U32(0, 0x9ca224)); SU32(e, 0x28, U32(0, 0x9ca228));
    SF32(e, 0x2c, 1.0f);
    SU32(e, 4, 1);
    SF32(e, 0x10, 0.0f);
    SU32(e, 8, 2);
    SF32(e, 0x18, 0.0f);
}
RX_FUNCTION(0x00550350, rx_player_camera_flash)

/* 0055ea60 (ECX = a colour (4 floats)): every camera fades (mode 4, 0.75) to the colour. */
void rx_cameras_fade(rx_addr color) {
    for (uint32_t i = 0; i < U32(view_set(), 0x19d0); i++) {
        const rx_addr c = U32(U32(view_set(), 0x19b8 + 4 * i), 0x70);
        SU32(c, 0, 0);
        SU32(c, 0x40, 0);
        SU32(c, 0x30, U32(color, 0)); SU32(c, 0x34, U32(color, 4)); SU32(c, 0x38, U32(color, 8)); SU32(c, 0x3c, U32(color, 0xc));
        SF32(c, 0x1c, 0.75f);
        SU32(c, 0x14, 1);
        SF32(c, 0x20, 0.0f);
        SU32(c, 0x18, 4);
        SF32(c, 0x28, 0.0f);
        SF32(c, 0x2c, 1.0f);
    }
}
RX_FUNCTION(0x0055ea60, rx_cameras_fade)

/* 0055d240: every local player's vehicle (FUN_00552b90 EDX = the game's players; (i, 3); +0x1ac) gets vf49 (1). */
void rx_local_vehicles_vf49(void) {
    RACE_FRAME_PLAIN(F, 0, 3);
    const rx_addr game = U32(0, 0xa946b8);
    const int32_t n = U8(game, 0x1541c);
    for (int32_t i = 0; i < n; i++) {
        rx_addr p;
        RACE_CORE_CALL(2, p = rx_world_players_find(game + 0x7460, (uint32_t)i, 3));
        const rx_addr v = U32(p, 0x1ac);
        if (v != 0) race_thiscall(race_vfn(v, 49), v, 1, 1, 0, 0, 0);
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055d240, rx_local_vehicles_vf49)

/* 00559e20 (EDX = activity, EDI = a player id) -> EAX: the human drivers whose player +4 is the id. */
uint32_t rx_activity_count_humans_of(rx_addr act, uint32_t id) {
    uint32_t n = 0;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) {
        const rx_addr p = U32(U32(U32(act, 0x268), 4 * i), 0x74);
        if (U32(p, 0x10) == 0 && U32(p, 4) == id) n++;
    }
    return n;
}
RX_FUNCTION(0x00559e20, rx_activity_count_humans_of)

/* 00559df0 (EDX = activity, ESI = a player kind) -> EAX: the drivers whose player is of the kind (+0x10). */
uint32_t rx_activity_count_kind(rx_addr act, uint32_t kind) {
    uint32_t n = 0;
    for (int32_t i = 0; i < I32(act, 0x26c); i++) if (U32(U32(U32(U32(act, 0x268), 4 * i), 0x74), 0x10) == kind) n++;
    return n;
}
RX_FUNCTION(0x00559df0, rx_activity_count_kind)

/* 005544b0 -> EAX: the UI settings (0xd92efc). */
rx_addr rx_ui_settings(void) { return U32(0, 0xd92efc); }
RX_FUNCTION(0x005544b0, rx_ui_settings)

/* 005b9f50 UISettings_Get (App.GetUISetting; ECX = the settings, EAX = the setting index, ESI = the device) -> EAX: the settings
 * keep one block of 18 values per device (+4 + 0x48 * device) and a default block (+0x124); the selected device is +0x16c (-1:
 * none). The requested device's own block answers for the settings 0, 5, 10, 11 and 13 (and whenever it is the selected device);
 * every other setting comes from the selected device's block, or the default block when none is selected. */
uint32_t rx_ui_settings_value(rx_addr settings, uint32_t index, uint32_t device) {
    const int32_t selected = I32(settings, 0x16c);
    const rx_addr own = settings + 4 + 4 * (index + device * 0x12);
    if ((int32_t)device == selected) {
        if (selected != -1) return U32(own, 0);
        return U32(settings, 0x124 + 4 * index);
    }
    if (index == 5 || index == 10 || index == 11 || index == 13 || index == 0) return U32(own, 0);
    if (selected == -1) return U32(settings, 0x124 + 4 * index);
    return U32(settings, 4 + 4 * (index + (uint32_t)selected * 0x12));
}
RX_FUNCTION(0x005b9f50, rx_ui_settings_value)

/* 00554d30: a jump to UISettings_Get (005b9f50): the registers pass through. */
uint32_t rx_ui_settings_get(rx_addr settings, uint32_t index, uint32_t device) { return rx_ui_settings_value(settings, index, device); }
RX_FUNCTION(0x00554d30, rx_ui_settings_get)

/* 00551070 (ECX = player) -> EAX: the player's activity's level (+0x94 +0x234) has a free ride session (+0x20018) with an active
 * ghost (+0x22180 +0x60). */
uint32_t rx_player_has_ghost(rx_addr p) {
    const rx_addr lv = U32(U32(p, 0x94), 0x234);
    if (lv == 0 || U32(lv, 0x20018) == 0) return 0;
    const rx_addr g = U32(lv, 0x22180);
    return g != 0 && U32(g, 0x60) != 0;
}
RX_FUNCTION(0x00551070, rx_player_has_ghost)

/* 005060e0 / 00506130 -> EAX: a 16-aligned allocation of 0x3c / 0x20 bytes (FUN_004dc0f0 first; _aligned_malloc under the
 * allocator lock 0xd61704 while 0xda67d8). */
static rx_addr locked_aligned_alloc(uint32_t size) {
    rx_tbl_alloc_init();                                           /* 004dc0f0 */
    uint32_t cs = 0x00d61704;
    if (U8(0, 0xda67d8) != 0) rx_os_import(0x0091600c, 1, &cs);   /* EnterCriticalSection */
    SU32(rx_sp() - 8, 0, size); SU32(rx_sp() - 4, 0, 0x10);
    const rx_addr p = crt_aligned_malloc(size, 0x10);              /* _aligned_malloc (IAT 0x916358) */
    if (U8(0, 0xda67d8) != 0) rx_os_import(0x009160f8, 1, &cs);   /* LeaveCriticalSection */
    return p;
}
rx_addr rx_alloc_0x3c(void) { RACE_FRAME_PLAIN(F, 0, 0); const rx_addr p = locked_aligned_alloc(0x3c); RACE_FRAME_END(F); return p; }
RX_FUNCTION(0x005060e0, rx_alloc_0x3c)
rx_addr rx_alloc_0x20(void) { RACE_FRAME_PLAIN(F, 0, 0); const rx_addr p = locked_aligned_alloc(0x20); RACE_FRAME_END(F); return p; }
RX_FUNCTION(0x00506130, rx_alloc_0x20)

/* 00503440 / 00503750 (ESI = a std::map iterator {+0 proxy, +4 node}): ++ for trees whose is-nil byte is at +0x39 / +0x1d. */
static void tree_next(rx_addr it, uint32_t nil) {
    rx_addr n = U32(it, 4);
    rx_addr r = U32(n, 8);
    if (U8(r, nil) == 0) {
        while (U8(U32(r, 0), nil) == 0) r = U32(r, 0);
        SU32(it, 4, r);
        return;
    }
    rx_addr p = U32(n, 4);
    while (U8(p, nil) == 0 && U32(it, 4) == U32(p, 8)) { SU32(it, 4, p); p = U32(p, 4); }
    SU32(it, 4, p);
}
void rx_tree_next_39(rx_addr it) { tree_next(it, 0x39); }
RX_FUNCTION(0x00503440, rx_tree_next_39)
void rx_tree_next_1d(rx_addr it) { tree_next(it, 0x1d); }
RX_FUNCTION(0x00503750, rx_tree_next_1d)

/* 00560290 MXActivityFreeRide::vf31 (ECX = activity, arg: new state; RET 4): Pre Exit: the level's current activity (+0x234
 * +0x20190) finishes its loading (FUN_0084ea00 ESI = the level +0x20150, EDI = the engine +0x10), runs its sequences to the end and
 * resets the level (FUN_0074bd50 (level), +0x20194 cleared); then FUN_0059b860 (EAX = the session) and 0055d240. */
void rx_free_ride_pre_exit(rx_addr act) {
    const rx_addr lv = U32(act, 0x234);
    if (act == U32(lv, 0x20190)) {
        RACE_FRAME_PLAIN(F, 0, 2);
        RACE_CORE_CALL(0, rx_engine_frame_mark(U32(U32(0, 0xa95844), 0x10)));
        const rx_addr cur = U32(lv + 0x20150, 0x40);
        if (cur != 0) RACE_CORE_CALL(1, rx_activity_finish_sequences(cur));
        RACE_CORE_CALL(1, rx_scenario_cast_release(lv + 0x20150));
        SU32(lv + 0x20150, 0x44, 0);
        RACE_FRAME_END(F);
    }
#ifdef RX_ORACLE   /* UI boundary (core/plan/QUEUE.md "chain"): 0059b860 sets the UI properties of the hub routing and raises "GotoHub" on the UI: the product has no UI */
    rx_regs in; memset(&in, 0, sizeof in); in.r[0] = U32(0, 0xa93fcc);
    rx_call_original_ex(0x0059b860, &in, 0, 0, 0);
#endif
    RACE_CORE_CALL(0, rx_local_vehicles_vf49());
}
RX_FUNCTION(0x00560290, rx_free_ride_pre_exit)

/* 005609b0 MXActivityFreeRide::vf26 (ECX = activity, arg dt; RET 4): Countdown: the activity's, then straight to InProgress. */
void rx_free_ride_countdown(rx_addr act, float dt) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_countdown(act, dt));
    RACE_CORE_CALL(2, rx_activity_set_state(act, 2));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x005609b0, rx_free_ride_countdown)

/* 0057a4a0 (shared slot): a jump to Activity::vf32. */
void rx_activity_countdown_exit_thunk(rx_addr act) { rx_activity_countdown_exit(act); }
RX_FUNCTION(0x0057a4a0, rx_activity_countdown_exit_thunk)

/* 00550650 (ESI = player): a player whose reset delay ran out (+0x116c not above 0) is put back on the track: the activity's
 * respawn point (vf19 (player, position, direction, up)), or without one where its vehicle is (+0x1ac: the position +0x200 and
 * the axes of the orientation quaternion +0x210: forward (2xz + 2wy, 2yz - 2wx, 1 - 2xx - 2yy), up (2xy - 2wz, 1 - 2xx - 2zz,
 * 2yz + 2wx)); then reset there (005501f0) and +0x1170 = 2. Frame: and esp, -16; sub 0x50 (the position at +0x40, the direction
 * at +0x20, the up axis at +0x30, the quaternion at +0x10). */
static void player_respawn(rx_addr p, uint32_t state) {
    RACE_FRAME_EBP(F, 16, 0x50, 0);
    if (!(F32(p, 0x116c) > 0.0f)) {
        const rx_addr act = U32(p, 0x94);
        uint32_t ok;
#ifndef RX_ORACLE
        if (race_vfn(act, 19) == 0x00579c70) {
            RACE_CORE_CALL(4, ok = rx_waypoint_respawn_point(act, p, F + 0x40, F + 0x20, F + 0x30));
        } else
#endif
        { ok = race_thiscall(race_vfn(act, 19), act, 4, p, F + 0x40, F + 0x20, F + 0x30); }
        if (ok == 0) {
            const rx_addr v = U32(p, 0x1ac);
            for (uint32_t i = 0; i < 16; i += 4) { SU32(F + 0x40, i, U32(v, 0x200 + i)); SU32(F + 0x10, i, U32(v, 0x210 + i)); }
            const float qx = F32(F, 0x10), qy = F32(F, 0x14), qz = F32(F, 0x18), qw = F32(F, 0x1c);
            const float y2 = qy * 2.0f, z2 = qz * 2.0f, x2 = qx * 2.0f;
            const float yz2 = z2 * qy, xx2 = qx * x2;
            SF32(F, 0xc, xx2);
            const float xy2_wz2 = qx * y2 - z2 * qw;
            const float wx2 = x2 * qw;
            const float zz_xx = z2 * qz + xx2;
            const float xz2 = qx * z2, yy_xx = y2 * qy + xx2, wy2 = y2 * qw;
            SF32(F, 0x30, xy2_wz2);
            SF32(F, 0x34, 1.0f - zz_xx);
            SF32(F, 0x38, yz2 + wx2);
            SF32(F, 0x20, xz2 + wy2);
            SF32(F, 0x24, yz2 - wx2);
            SF32(F, 0x28, 1.0f - yy_xx);
        }
        RACE_CORE_CALL(1, rx_player_reset_to(p, F + 0x40, F + 0x20));
        SU32(p, 0x1170, state);
    }
    RACE_FRAME_END(F);
}
void rx_player_respawn(rx_addr p) { player_respawn(p, 2); }
RX_FUNCTION(0x00550650, rx_player_respawn)

/* 00550cc0 (ESI = player): the same respawn (00550650's code, duplicated by the compiler) for the player that entered a "ResetPlayer" trigger
 * (the "VehicleEntered" handler 0054f0b0): ends with +0x1170 = 9 instead of 2. */
void rx_player_respawn_trigger(rx_addr p) { player_respawn(p, 9); }
RX_FUNCTION(0x00550cc0, rx_player_respawn_trigger)

/* 00504ee0 (ECX = the value {int, std::string}, args: left, parent, right; RET 0xc) -> EAX: a new map node (0x3c bytes: links,
 * the key, the string copied by FUN_004ebce0 (EDI = the destination; arg the source), colour / is-nil bytes +0x38 / +0x39 0). */
rx_addr rx_map_node_int_string(rx_addr val, uint32_t l, uint32_t par, uint32_t r) {
    RACE_FRAME_EBP(F, 4, 0x14, 4);   /* the SEH record, 8 bytes, 3 pushes, the cookie */
    rx_addr n;
    RACE_CORE_CALL(0, n = rx_alloc_0x3c());
    if (n != 0) {
        SU32(n, 0, l); SU32(n, 4, par); SU32(n, 8, r);
        SU32(n, 0xc, U32(val, 0));
        RACE_CORE_CALL(1, (void)rx_tbl_rowset_copy_construct(n + 0x10, val + 4));   /* 004ebce0 (EDI = the new set; stack the source) */
        SU8(n, 0x38, 0); SU8(n, 0x39, 0);
    }
    RACE_FRAME_END(F);
    return n;
}
RX_FUNCTION(0x00504ee0, rx_map_node_int_string)

/* 005053b0 (ECX = the 16-byte value, args: left, parent, right; RET 0xc) -> EAX: a new 0x20-byte map node (the value at +0xc,
 * colour / is-nil bytes +0x1c / +0x1d 0). */
rx_addr rx_map_node_16(rx_addr val, uint32_t l, uint32_t par, uint32_t r) {
    RACE_FRAME_EBP(F, 4, 0x14, 4);   /* the SEH record, 8 bytes, 3 pushes, the cookie */
    rx_addr n;
    RACE_CORE_CALL(0, n = rx_alloc_0x20());
    if (n != 0) {
        SU32(n, 0, l); SU32(n, 4, par); SU32(n, 8, r);
        for (uint32_t i = 0; i < 16; i += 4) SU32(n, 0xc + i, U32(val, i));
        SU8(n, 0x1c, 0); SU8(n, 0x1d, 0);
    }
    RACE_FRAME_END(F);
    return n;
}
RX_FUNCTION(0x005053b0, rx_map_node_16)

/* std::_Tree::_Lrotate / _Rrotate of the red-black trees behind std::map (ECX = the node x; stack the tree; RET 4). A node's links are
 * +0 left, +4 parent, +8 right; the head node (tree +0x18) keeps the root in its +4 and has the nil flag byte set; `nil` is the offset of
 * that flag byte (0x39 in the 0x3c-byte nodes of std::map<int, std::string>, 0x1d in the 0x20-byte nodes of the 16-byte maps).
 * _Lrotate: x's right child y takes x's place (y's left subtree moves under x, y's parent is x's old parent, the root or the matching
 * child link of the parent is updated), x becomes y's left child; _Rrotate is the mirror image. */
static void tree_rotate_left(rx_addr x, rx_addr tree, uint32_t nil) {
    const rx_addr y = U32(x, 8);
    SU32(x, 8, U32(y, 0));
    if (U8(U32(y, 0), nil) == 0) SU32(U32(y, 0), 4, x);
    SU32(y, 4, U32(x, 4));
    const rx_addr head = U32(tree, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 0)) SU32(U32(x, 4), 0, y);
    else SU32(U32(x, 4), 8, y);
    SU32(y, 0, x);
    SU32(x, 4, y);
}
static void tree_rotate_right(rx_addr x, rx_addr tree, uint32_t nil) {
    const rx_addr y = U32(x, 0);
    SU32(x, 0, U32(y, 8));
    if (U8(U32(y, 8), nil) == 0) SU32(U32(y, 8), 4, x);
    SU32(y, 4, U32(x, 4));
    const rx_addr head = U32(tree, 0x18);
    if (x == U32(head, 4)) SU32(head, 4, y);
    else if (x == U32(U32(x, 4), 8)) SU32(U32(x, 4), 8, y);
    else SU32(U32(x, 4), 0, y);
    SU32(y, 8, x);
    SU32(x, 4, y);
}
void rx_tree_rotate_left_int_string(rx_addr x, rx_addr tree) { tree_rotate_left(x, tree, 0x39); }       /* 00500480 */
RX_FUNCTION(0x00500480, rx_tree_rotate_left_int_string)
void rx_tree_rotate_right_int_string(rx_addr x, rx_addr tree) { tree_rotate_right(x, tree, 0x39); }     /* 004ffe80 */
RX_FUNCTION(0x004ffe80, rx_tree_rotate_right_int_string)
void rx_tree_rotate_left_16(rx_addr x, rx_addr tree) { tree_rotate_left(x, tree, 0x1d); }              /* 00765c90 */
RX_FUNCTION(0x00765c90, rx_tree_rotate_left_16)
void rx_tree_rotate_right_16(rx_addr x, rx_addr tree) { tree_rotate_right(x, tree, 0x1d); }            /* 00765ce0 */
RX_FUNCTION(0x00765ce0, rx_tree_rotate_right_16)

/* std::_Tree::_Insert (EAX = the parent-to-be, EDI = the tree {+0 proxy, +0x18 head, +0x1c size}; args: out iterator, add as
 * left child, the value; RET 0xc): a red node (the node constructor) linked under the parent (the head's leftmost / rightmost /
 * root kept), then the red-black fix-up with the rotations (ECX = node, arg the tree) and the root black. The "map/set<T> too
 * long" length_error of a full tree is not reproduced (sizes stay tiny). Frame: sub 0x44; 3 pushes. */
static void tree_insert(rx_addr tree, rx_addr where, rx_addr out, uint32_t left, rx_addr val, uint32_t color,
                        rx_addr (*make)(rx_addr, uint32_t, uint32_t, uint32_t), void (*rot_l)(rx_addr, rx_addr), void (*rot_r)(rx_addr, rx_addr)) {
    RACE_FRAME_PLAIN(F, 0x44, 3);
    const rx_addr head = U32(tree, 0x18);
    rx_addr n;
    RACE_CORE_CALL(3, n = make(val, head, where, head));
    SU32(tree, 0x1c, U32(tree, 0x1c) + 1);
    if (where == U32(tree, 0x18)) {
        SU32(U32(tree, 0x18), 4, n);
        SU32(U32(tree, 0x18), 0, n);
        SU32(U32(tree, 0x18), 8, n);
    } else if (left) {
        SU32(where, 0, n);
        if (where == U32(U32(tree, 0x18), 0)) SU32(U32(tree, 0x18), 0, n);
    } else {
        SU32(where, 8, n);
        if (where == U32(U32(tree, 0x18), 8)) SU32(U32(tree, 0x18), 8, n);
    }
#define RB_PARENT(x) U32((x), 4)
#define RB_RED(x) (U8((x), color) == 0)
    rx_addr x = n;
    while (RB_RED(RB_PARENT(x))) {
        const rx_addr p = RB_PARENT(x), g = RB_PARENT(p);
        if (p == U32(g, 0)) {
            const rx_addr u = U32(g, 8);
            if (RB_RED(u)) {
                SU8(p, color, 1); SU8(u, color, 1); SU8(RB_PARENT(RB_PARENT(x)), color, 0);
                x = RB_PARENT(RB_PARENT(x));
            } else {
                if (x == U32(p, 8)) { x = p; RACE_CORE_CALL(1, rot_l(x, tree)); }
                SU8(RB_PARENT(x), color, 1);
                SU8(RB_PARENT(RB_PARENT(x)), color, 0);
                RACE_CORE_CALL(1, rot_r(RB_PARENT(RB_PARENT(x)), tree));
            }
        } else {
            const rx_addr u = U32(g, 0);
            if (RB_RED(u)) {
                SU8(p, color, 1); SU8(u, color, 1); SU8(RB_PARENT(RB_PARENT(x)), color, 0);
                x = RB_PARENT(RB_PARENT(x));
            } else {
                if (x == U32(p, 0)) { x = p; RACE_CORE_CALL(1, rot_r(x, tree)); }
                SU8(RB_PARENT(x), color, 1);
                SU8(RB_PARENT(RB_PARENT(x)), color, 0);
                RACE_CORE_CALL(1, rot_l(RB_PARENT(RB_PARENT(x)), tree));
            }
        }
    }
#undef RB_PARENT
#undef RB_RED
    SU8(U32(U32(tree, 0x18), 4), color, 1);
    SU32(out, 4, n);
    SU32(out, 0, U32(tree, 0));
    RACE_FRAME_END(F);
}

/* 00502e30: _Insert of the std::map<int, std::string> trees (0x3c-byte nodes 00504ee0, colour +0x38; rotations 00500480 /
 * 004ffe80). */
void rx_tree_insert_int_string(rx_addr where, rx_addr tree, rx_addr out, uint32_t left, rx_addr val) {
    tree_insert(tree, where, out, left & 0xff, val, 0x38, rx_map_node_int_string, rx_tree_rotate_left_int_string, rx_tree_rotate_right_int_string);
}
RX_FUNCTION(0x00502e30, rx_tree_insert_int_string)

/* 005035c0: _Insert of the 16-byte value trees (0x20-byte nodes 005053b0, colour +0x1c; rotations 00765c90 / 00765ce0). */
void rx_tree_insert_16(rx_addr where, rx_addr tree, rx_addr out, uint32_t left, rx_addr val) {
    tree_insert(tree, where, out, left & 0xff, val, 0x1c, rx_map_node_16, rx_tree_rotate_left_16, rx_tree_rotate_right_16);
}
RX_FUNCTION(0x005035c0, rx_tree_insert_16)

/* ---- std::vector<T*>::_Insert_n (one element) ---------------------------------------------------------------------------------- */

/* The vector helpers (register conventions): FUN_00776d20 copy [EAX: EDI .. EAX) to EDX -> EAX the end; FUN_006c8230 fill ESI
 * copies of *EDX at EDI -> EAX the end; FUN_004faae0 copy_backward [EDI, EAX) to end ECX; FUN_007cfd80 fill [arg, ECX) with *EDX
 * (cdecl). */
static rx_addr vec_copy(rx_addr first, rx_addr last, rx_addr dest) {
    rx_addr r;
    RACE_CORE_CALL(0, r = rx_fx_vector_umove(last, first, dest));   /* 00776d20 (EAX = last, EDI = first, EDX = dest) */
    return r;
}
static rx_addr vec_fill_n(rx_addr dest, uint32_t n, rx_addr val) {
    rx_addr r;
    RACE_CORE_CALL(0, r = rx_fx_fill_dwords(n, dest, val));
    return r;
}

/* 004fead0 (stdcall count; RET 4) -> EAX: the storage of a vector of `count` pointers: 4 * count bytes, 16-aligned (FUN_004dc0f0
 * first, then _aligned_malloc under the allocator lock 0xd61704 while [0xda67d8] is set). Frame: push esi. */
rx_addr rx_vector_alloc(uint32_t count) {
    RACE_FRAME_PLAIN(F, 0, 1);
    rx_tbl_alloc_init();                                           /* 004dc0f0 */
    uint32_t cs = 0x00d61704;
    if (U8(0, 0xda67d8) != 0) rx_os_import(0x0091600c, 1, &cs);   /* EnterCriticalSection */
    rx_addr p;
    RACE_CORE_CALL(2, p = crt_aligned_malloc(count * 4u, 0x10));   /* _aligned_malloc (IAT 0x916358) */
    if (U8(0, 0xda67d8) != 0) rx_os_import(0x009160f8, 1, &cs);   /* LeaveCriticalSection */
    RACE_FRAME_END(F);
    return p;
}
RX_FUNCTION(0x004fead0, rx_vector_alloc)

/* 004f3320 (free a table / vector storage block) is fx_rdb.c's rx_tbl_free (merged 2026-10-26: w-race and w-fx both ported it). */
void rx_vector_free(rx_addr storage) { rx_tbl_free(storage); }

/* 004faae0 (EAX = last, EDI = first, ECX = the end of the destination) -> EAX: std::copy_backward of the pointers [first, last)
 * to end at `dest_end`: memmove_s (IAT 0x916200) of the 4 * n bytes to dest_end - 4 * n (n = (last - first) >> 2 signed; nothing
 * when n <= 0). Frame: push esi. */
rx_addr rx_vector_copy_backward(rx_addr last, rx_addr first, rx_addr dest_end) {
    RACE_FRAME_PLAIN(F, 0, 1);
    const int32_t n = (int32_t)(last - first) >> 2;
    const uint32_t bytes = (uint32_t)n * 4u;
    const rx_addr dest = dest_end - bytes;
    if (n > 0) RACE_CORE_CALL(4, (void)crt_memmove_s(dest, bytes, first, bytes));
    RACE_FRAME_END(F);
    return dest;
}
RX_FUNCTION(0x004faae0, rx_vector_copy_backward)

/* 00501a30 (stdcall vector {+0xc first, +0x10 last, +0x14 end}, value*, unused, where; RET 0x10): inserts one pointer at
 * `where`: with no room the storage grows by half (at least to size + 1; FUN_004fead0 allocates, FUN_004f3320 frees the old
 * one) and the three parts are copied over; else the tail moves up one slot and the value is stored. The original keeps the new
 * capacity in its first argument slot and the value in its second (both written here too). The length_error of a full vector
 * (FUN_007754f0) and the exception path are not reproduced. Frame: the SEH record, sub 0xc, 3 pushes, the cookie. */
void rx_vector_insert_one(rx_addr vec, rx_addr valp, rx_addr where) {
    const rx_addr args = rx_sp() + 4;
    RACE_FRAME_EBP(F, 4, 0x18, 4);
    const rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    const uint32_t cap = first != 0 ? (uint32_t)((int32_t)(U32(vec, 0x14) - first) >> 2) : 0;
    const uint32_t size = (uint32_t)((int32_t)(last - first) >> 2);
    const uint32_t need = size + 1;
    if (cap < need) {
        const uint32_t half = cap >> 1;
        uint32_t ncap = 0x3fffffffu - half >= cap ? cap + half : 0;
        if (ncap < need) ncap = need;
        SU32(args, 0, ncap);
        rx_addr p;
        RACE_CORE_CALL(1, p = rx_vector_alloc(ncap));          /* 004fead0 */
        SU32(F + 0x14, 0, p);   /* ebp - 0x14 */
        rx_addr e = vec_copy(U32(vec, 0xc), where, p);
        e = vec_fill_n(e, 1, valp);
        vec_copy(where, U32(vec, 0x10), e);
        const rx_addr of = U32(vec, 0xc);
        const uint32_t nsize = (uint32_t)((int32_t)(U32(vec, 0x10) - of) >> 2) + 1;
        if (of != 0) RACE_CORE_CALL(1, rx_tbl_free(of));                 /* 004f3320 (stdcall p; RET 4) */
        SU32(vec, 0x14, p + 4 * ncap);
        SU32(vec, 0x10, p + 4 * nsize);
        SU32(vec, 0xc, p);
    } else {
        const uint32_t v = U32(valp, 0);
        SU32(args, 4, v);
        if ((uint32_t)((int32_t)(last - where) >> 2) < 1) {
            vec_copy(where, last, where + 4);
            const rx_addr l2 = U32(vec, 0x10);
            vec_fill_n(l2, 1u - (uint32_t)((int32_t)(l2 - where) >> 2), args + 4);
            SU32(vec, 0x10, U32(vec, 0x10) + 4);
            RACE_CORE_CALL(1, rx_fx_vector_fill(args + 4, U32(vec, 0x10) - 4, where));   /* 007cfd80 (EDX = value*, ECX = end; cdecl begin) */
        } else {
            SU32(vec, 0x10, vec_copy(last - 4, last, last));
            RACE_CORE_CALL(0, (void)rx_vector_copy_backward(last - 4, where, last));   /* 004faae0 (EAX = last - 1, EDI = where, ECX = last) */
            RACE_CORE_CALL(1, rx_fx_vector_fill(args + 4, where + 4, where));   /* 007cfd80 */
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00501a30, rx_vector_insert_one)

/* 0055e360 (ECX = the object's name, args: activity, world object; RET 8): sets up a scripted world object: the name resolved
 * (FUN_006b6a30 (ECX = the flags byte out, EDX = 0; cdecl name); 0: the name itself); the object active (+0x70) and by its type
 * (vf22): 0x103a / 0x104f with a name FUN_0074aab0 (ESI = object; name); 0x12 / 0x1039 / 0x103b / 0x1050 / 0x1081 load the
 * "anim" resource of the name (engine 0xa95844 +8 vf27) into the object's animation (FUN_00835000 EAX = +0x148, EDI = the
 * resource; new: FUN_00839c30 (EAX = the resource, ESI = object; flag bit 2), then FUN_00839bb0 (ECX = the animation, EAX = object);
 * flag bit 2: FUN_00835170 ESI = +0x148); 0x103b also clears +0x350; all but the default mark +0xa2. Flag bit 0 starts the
 * "<name>_events" sequence (0055e100 (activity, name, object)). Frame: and esp, -8; sub 0x11c; 3 pushes (the flags at +0xf, the
 * activity at +0x14, the name buffer at +0x18). */
void rx_activity_setup_object(rx_addr name, rx_addr act, rx_addr obj) {
    RACE_FRAME_EBP(F, 8, 0x11c, 3);
    SU32(F, 0x14, act);
    SU8(F, 0xf, 0);
    rx_addr picked;
    RACE_CORE_CALL(1, picked = rx_anim_alias_pick(F + 0xf, 0, name));
    const rx_addr nm = picked != 0 ? picked : name;
    SU32(F, 0x10, nm);
    SU32(obj, 0x70, 1);
    const uint32_t type = race_thiscall(race_vfn(obj, 22), obj, 0, 0, 0, 0, 0);
    int anim = 0;
    switch (type) {
    case 0x104f: case 0x103a:
        SU8(obj, 0xa2, 1);
        if (nm != 0) {
            RACE_CORE_CALL(1, rx_scenario_anim_entity_set_animation(obj, nm));
        }
        break;
    case 0x103b:
        SU32(obj, 0x350, 0);
        SU32(obj, 0x70, 1);
        SU8(obj, 0xa2, 1);
        anim = 1;
        break;
    case 0x12: case 0x1039: case 0x1050: case 0x1081:
        SU8(obj, 0xa2, 1);
        anim = 1;
        break;
    case 0x1079:
        SU8(obj, 0xa2, 1);
        break;
    default: break;
    }
    if (anim && nm != 0) {
        const rx_addr eng = U32(U32(0, 0xa95844), 8);
        uint32_t a[4] = {nm, 0x6d696e61 /* 'anim' */, 0, 0};
        const rx_addr res = rx_call_original(race_vfn(eng, 27), RX_THISCALL, eng, 4, a);
        if (res != 0) {
            rx_addr an;
            RACE_CORE_CALL(0, an = rx_character_anim_find_slot(res, obj + 0x148));
            if (an == 0) {
                uint32_t f = (U8(F, 0xf) >> 2) & 1;
                RACE_CORE_CALL(1, rx_character_entity_set_loop_animation(obj, res, f));
                an = U32(obj, 0x1f4);
            }
            RACE_CORE_CALL(0, rx_character_entity_play_slot(obj, an));
            if (U8(F, 0xf) & 4) rx_character_random_motion_time(obj + 0x148);
        }
    }
    if (U8(F, 0xf) & 1) {
        { const uint32_t v = U32(F, 0x10); race_format(F + 0x18, 0x104, 0x00941e74 /* "%s_events" */, 1, &v); }
        RACE_CORE_CALL(3, rx_activity_start_sequence(U32(F, 0x14), F + 0x18, obj));
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055e360, rx_activity_setup_object)

/* 0055dac0 (ECX = the activity (unused), args: record, unused; RET 8): creates the record's locator: FUN_0074b200 (EAX = +0x110,
 * ECX = +0xc8; args +0xc8 and the 14 floats +0xd0 .. +0x104). Frame: and esp, -16; sub 0x20; sub 0x38 for the arguments.
 * (The ECX parameter is the scenario script table's target: the registry dispatch passes it first.) */
void rx_activity_make_locator(rx_addr act, rx_addr rec) {
    (void)act;
    RACE_FRAME_EBP(F, 16, 0x20, 0);
    /* the 15 arguments where the original pushes them ([esp+4] = mode, [esp+8] the 14 floats: ScreenEffect reads f = esp+8) */
    const rx_addr sp0 = rx_sp();
    SU32(sp0, -0x3c, U32(rec, 0xc8));
    for (uint32_t i = 0; i < 14; i++) SU32(sp0, -0x38 + 4 * (int32_t)i, U32(rec, 0xd0 + 4 * i));
    RACE_CORE_CALL(15, rx_scenario_screen_effect((int32_t)U32(rec, 0x110), U32(rec, 0xc8), sp0 - 0x38));
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055dac0, rx_activity_make_locator)

/* 0055dff0 (ECX = activity, args: record, unused; RET 8): shows / hides the record's named object (FUN_0074b970: ESI = the level
 * +0x20150, EDI = the name +8): visible while the record's +0x114 > 0 (kept in the record argument's slot); the object's part
 * named +0x48 (FUN_007de980 / 007de9e0 on the model +0x210 for types 0x103a / 0x104f, on the animated model (+0x100 +0x44 +0x10,
 * +0x4c held) for 0x12 / 0x103b / 0x1081) gets bit 4 of +0xc = hidden. */
void rx_activity_show_object(rx_addr act, rx_addr rec) {
    const rx_addr slot = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 0, 4);
    rx_addr obj;
    RACE_CORE_CALL(0, obj = rx_scenario_anims_find_entity(U32(act, 0x234) + 0x20150, rec + 8));
    const uint32_t vis = F32(rec, 0x114) > 0.0f ? 1u : 0u;
    SU32(slot, 0, vis);
    const rx_addr part = rec + 0x48;
    if (obj != 0) {
        const uint32_t type = race_thiscall(race_vfn(obj, 22), obj, 0, 0, 0, 0, 0);
        rx_addr model = 0, held = 0;
        if (type == 0x103a || type == 0x104f) model = obj + 0x210;
        else if (type == 0x12 || type == 0x103b || type == 0x1081) {
            held = U32(obj, 0x100);
            model = U32(held, 0x44) + 0x10;
            SU32(held, 0x4c, 1);
        }
        if (model != 0) {
            uint32_t has;
            RACE_CORE_CALL(1, has = rx_hierarchy_has_node(model, part));
            if (has != 0) {
                rx_addr e;
                RACE_CORE_CALL(1, e = rx_hierarchy_find_node(model, part));
                SU8(e, 0xc, (U8(e, 0xc) & ~0x10u) | (U32(slot, 0) == 0 ? 0x10u : 0u));
            }
            if (held != 0) SU32(held, 0x4c, 0);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055dff0, rx_activity_show_object)

/* 0055de70 (ECX = activity, args: record, unused; RET 8): moves the record's named object (+8) to its locator (+0x88: FUN_0074b680
 * / 0074b970 on the level +0x20150): type 0x1081 is attached through FUN_0074be10 (ECX = the level, EDX = 0; object, locator, the
 * part name +0x48, the activity's vf10); type 0x1059 (a spawner) is reset (+0x794, its controller's +0x1c / +0x20, +0x364 =
 * +0x360, +0x398, +0x390) and placed; the others are placed (vf16 (the locator's vf14 matrix)) and, unless the part is
 * "Editor_Default", set up (0055e360). */
void rx_activity_place_at_locator(rx_addr act, rx_addr rec) {
    RACE_FRAME_PLAIN(F, 0, 4);
    const rx_addr lv = U32(act, 0x234) + 0x20150;
    rx_addr obj;
    RACE_CORE_CALL(0, obj = rx_scenario_anims_find_entity(lv, rec + 8));
    if (obj == 0) goto done;
    {
        rx_addr loc;
        RACE_CORE_CALL(2, loc = rx_scenario_anims_find_prop(lv, rec + 0x88));
        if (loc == 0) {
            RACE_CORE_CALL(0, loc = rx_scenario_anims_find_entity(lv, rec + 0x88));
            if (loc == 0) goto done;
        }
        if (race_thiscall(race_vfn(obj, 22), obj, 0, 0, 0, 0, 0) == 0x1081) {
            const uint32_t v = race_thiscall(race_vfn(act, 10), act, 0, 0, 0, 0, 0);
            RACE_CORE_CALL(4, rx_scenario_cast_place_entity(lv, 0, obj, loc, rec + 0x48, v));
            goto done;
        }
        if (race_thiscall(race_vfn(obj, 22), obj, 0, 0, 0, 0, 0) == 0x1059) {
            SU32(obj, 0x794, 0);
            const rx_addr c = U32(obj, 0x790);
            if (c != 0) {
                const rx_addr r = race_thiscall(race_vfn(c, 4), c, 0, 0, 0, 0, 0);
                if (r != 0) { if (U32(r, 0x1c) == 0) SU32(r, 0x20, 1); SU32(r, 0x1c, 1); }
            }
            SU8(obj, 0xa2, 1);
            const rx_addr mtx = race_thiscall(race_vfn(loc, 14), loc, 0, 0, 0, 0, 0);
            race_thiscall(race_vfn(obj, 16), obj, 1, mtx, 0, 0, 0);
            SU32(obj, 0x364, U32(obj, 0x360));
            SU32(obj, 0x70, 1);
            SF32(obj, 0x398, 0.0f);
            SU32(obj, 0x390, 0);
            goto done;
        }
        const rx_addr mtx = race_thiscall(race_vfn(loc, 14), loc, 0, 0, 0, 0, 0);
        race_thiscall(race_vfn(obj, 16), obj, 1, mtx, 0, 0, 0);
        SU32(obj, 0x70, 1);
        if (crt_stricmp(0x0093a2e0 /* "Editor_Default" */, rec + 0x48) != 0) RACE_CORE_CALL(2, rx_activity_setup_object(rec + 0x48, act, obj));
    }
done:
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055de70, rx_activity_place_at_locator)

/* 0055d4e0 (ECX = activity, args: record, unused; RET 8): places the record's world entity (FUN_007d2da0 (the world +0x234
 * +0x20138, the name +8)) at the record's matrix (+0xd0) transformed by the level's (FUN_00401830): a new entity (its vf25's vf1 0)
 * is first registered (FUN_007d3040 (ESI = the entity; world)), placed, numbered (+0x14 = the world's +0x19f4++, under its lock +4) and added
 * (FUN_007d2f70 EDI = the entity; world); entities of types 3 / 4 / 0x103a / 0x103b / 0x104f / 0x1050 / 0x1079 / 0x1081 are
 * activated (+0xa2, +0x70). Frame: and esp, -16; sub 0x44; 3 pushes (the matrix at +0x10). */
void rx_activity_place_entity(rx_addr act, rx_addr rec) {
    RACE_FRAME_EBP(F, 16, 0x44, 3);
    const rx_addr m = F + 0x10;
    for (uint32_t i = 0; i < 0x40; i += 4) SU32(m, i, U32(rec, 0xd0 + i));
    RACE_CORE_CALL(1, rx_fx_matrix4_mul(m, U32(act, 0x234) + 0x20150, m));
    rx_addr e;
    RACE_CORE_CALL(2, e = rx_scene_manager_find_named(U32(U32(act, 0x234), 0x20138), rec + 8));
    if (e != 0) {
        const rx_addr k = race_thiscall(race_vfn(e, 25), e, 0, 0, 0, 0, 0);
        if (race_thiscall(race_vfn(k, 1), k, 0, 0, 0, 0, 0) == 0) {
            const uint32_t w = U32(U32(act, 0x234), 0x20138);
            RACE_CORE_CALL(1, rx_scene_manager_remove_entity(e, w));
            race_thiscall(race_vfn(e, 16), e, 1, m, 0, 0, 0);
            const rx_addr world = U32(U32(act, 0x234), 0x20138);
            uint32_t cs = world + 4;
            rx_os_import(0x0091600c, 1, &cs);
            const uint32_t id = U32(world, 0x19f4);
            SU32(world, 0x19f4, id + 1);
            SU32(e, 0x14, id);
            rx_os_import(0x009160f8, 1, &cs);
            RACE_CORE_CALL(1, rx_scene_manager_add_entity(e, world));
        } else {
            race_thiscall(race_vfn(e, 16), e, 1, m, 0, 0, 0);
        }
        const uint32_t t = race_thiscall(race_vfn(e, 22), e, 0, 0, 0, 0, 0);
        if (t == 3 || t == 4 || t == 0x103a || t == 0x103b || t == 0x104f || t == 0x1050 || t == 0x1079 || t == 0x1081) {
            SU8(e, 0xa2, 1);
            SU32(e, 0x70, 1);
        }
    }
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x0055d4e0, rx_activity_place_entity)

/* 00568620 MXActivityRaceStub::vf39 (ECX = activity, arg: old state; RET 4): InProgress Enter of a race: the activity's
 * (Activity::vf39), the clip recorder started (0xd5a9a4: ClipRecorder_StartRecording), the nine HUD flags 0xa7f384 .. 0xa7f3a4
 * set, the first local player's driver offered to the clip recorder (ClipRecorder_Candidate ECX = 8, EDX = driver; 0, 5). An
 * overtake race (session state 6) disables that player's controls (ten groups, as 0055a1e0) and sets the start stagger
 * (+0x13e604 = (drivers - 1) * "OvertakeStartDelayPerDriver" (default 1; kept in the argument slot), +0x13e608 = 0); other
 * races reset the vehicles' effects (00569730). Without a session result the start-line script (vf53 (0)). */
void rx_race_stub_in_progress_enter(rx_addr act, uint32_t old) {
    const rx_addr slot = rx_sp() + 4;
    RACE_FRAME_PLAIN(F, 4, 1);
    SU32(F, 4, act);
    RACE_CORE_CALL(1, rx_activity_in_progress_enter(act));
    (void)old;
    if (U32(0, 0xd5a9a4) != 0) RACE_CORE_CALL(1, rx_clip_recorder_start_recording(act));
    rx_set_sp(F - 12);   /* push ebx; push esi; push edi */
    static const uint32_t flags[9] = {0xa7f38c, 0xa7f388, 0xa7f384, 0xa7f390, 0xa7f398, 0xa7f394, 0xa7f39c, 0xa7f3a0, 0xa7f3a4};
    for (int i = 0; i < 9; i++) SU32(0, flags[i], 1);
    const rx_addr base = U32(0, 0xa946b8) + 0x7460;
    rx_addr player = 0;
    if (I32(base, 0xdf8c) != -1) {
        for (uint32_t i = 0; i < U8(base, 0xdfbc); i++)
            if (U32(rx_seat_active(base, i), 0x10) == 0) { player = rx_seat_active(base, i); break; }
    }
    uint32_t idx;
    RACE_CORE_CALL(0, idx = (uint32_t)rx_world_group_rider_index(act, player));
    const rx_addr d = U32(U32(act, U32(player, 0x908) != 0 ? 0x27c : 0x268), 4 * idx);
    RACE_CORE_CALL(2, rx_world_clip_recorder_candidate(8, d, 0.0f, 5.0f));
    if (U32(act, 0x238) == 6) {
        const rx_addr p = U32(d, 0x74);
        const int32_t pad = I32(p, 0x1f0);
        if (pad >= 0) {
            if (U32(p, 0x11c8) != 0) SU32(p, 0x11cc, U32(p, 0x11cc) & ~1u);
            else {
                rx_addr dev = pad_getter(0x00548f20, (uint32_t)pad, 1);
                race_thiscall(race_vfn(dev, 9), dev, 2, (uint32_t)pad, 0, 0, 0);
                dev = pad_getter(0x00548f70, (uint32_t)pad, 1);
                race_thiscall(race_vfn(dev, 9), dev, 2, (uint32_t)pad, 0, 0, 0);
            }
            pad_group(p, 0x002, 0, 0x348, 0);
            pad_group(p, 0x004, 0, 0, 0x00548bf0);
            pad_group(p, 0x010, 0, 0x9ec, 0);
            pad_group(p, 0x020, 0, 0, 0x00548c40);
            pad_group(p, 0x040, 0, 0x127c, 0);
            pad_group(p, 0x080, 0, 0x167c, 0);
            pad_group(p, 0x100, 0, 0x748, 0);
            pad_group(p, 0x200, 0, 0x147c, 0);
            pad_group(p, 0x400, 0, 0xf34, 0);
        }
        SF32(slot, 0, 1.0f);
        const rx_addr cfg = U32(U32(0, 0xa95844), 0xc);
        const rx_addr e = race_thiscall(race_vfn(cfg, 1), cfg, 3, 0x0093e42c /* "MXGame" */, 0x00941dcc, 1, 0);
        if (e != 0) race_thiscall(race_vfn(cfg, 17), cfg, 3, e, slot, 1, 0);
        SF32(act, 0x13e604, (float)(I32(act, 0x26c) - 1) * F32(slot, 0));
        SU32(act, 0x13e608, 0);
    } else {
        RACE_CORE_CALL(0, rx_activity_vehicle_fx_reset(act));
    }
    rx_set_sp(F);
    if (U32(act, 0xfe8) == 0) race_thiscall(race_vfn(act, 53), act, 1, 0, 0, 0, 0);
    RACE_FRAME_END(F);
}
RX_FUNCTION(0x00568620, rx_race_stub_in_progress_enter)
