/* restored_champ_weekend.c -- RESTORED CONTENT (w-champweekend, 2026-10-29). NOT an original-logic port.
 *
 * The original ships the championship sub-event activity classes (MXActivityRaceChampIntro / Practice / Heat / MainEvent / LCQ:
 * race_champ.c, verified ports) but the PC game never builds one: its Series rows are all Tour series, its championship tables are
 * empty, the front end's EventType buffer (16 bytes) can only name "RaceChampIntro", and the console's series sequence that set the
 * classes up is not in the PC executable (no MXSeriesEvent class names a championship type). This file supplies what that missing
 * sequence did, as new code that calls the verified ports and changes none of them:
 *   - the activity named by the stage: the event's activity-table row (ActivityType, NumLaps) and every player's requested activity
 *     (the Player List rows that the world's player join matches), the same writes MXSeriesEvent_SetupActivity (005a9290) makes for a
 *     career race, done after the arcade event's StartEvent;
 *   - the waypoint gates: the waypoint practice / main event's vf5 (MXActivityWaypointRace::vf5 00578500) loads the area's gates only
 *     up to the intro type; slot 5 of their vtables loads the area on the intro's type first (restored_wp_vf5);
 *   - the racing line: the classes' vf5 (0057e0b0 / 0057e410) do not load the track's racing line that MXActivityRaceStub::vf5 loads
 *     (the console let an earlier sub event load it into the area); slot 5 of their vtables is pointed at a restoration function that
 *     loads it (FUN_00536ef0, as the stub does) and then calls the class's own vf5;
 *   - the intro's podium cast: MXActivityRaceChampIntro's (and the Waypoint / Freestyle intros') vf37 is Activity::vf37 (0057e900 -> 005589c0), which does not bind the
 *     activity to the level's scenario cast (0074ada0) as MXActivityRaceStub::vf37 does, so RaceIntroScenario found no cast entry and
 *     called a null vtable; slot 37 of the intro's vtable is pointed at a restoration function that runs Activity::vf37 and then the
 *     cast setup the stub's vf37 makes.
 * The vtables live in the session's guest image: the patch lasts for the session (the next session starts from the loaded image).
 * Addresses 0xeffe0xx0 are restoration-only function ids (no original function lives there). */
#include "restored_champ_weekend.h"
#include "rx_champ_weekend.h"   /* RX_CHAMP_TIMED_LAPS */
#include <string.h>
#include "race_champ.h"
#include "race_extern.h"
#include "build_activity.h"
#include "build_activity_util.h"
#include "build_activity_pre.h"
#include "build_tables.h"
#include "anim_scenario_misc.h"
#include "waypoint_activity.h"
#include "race_activity_freestyle.h"

#define RESTORED_RACE_VF5   0xeffe0010u   /* the race classes' slot 5: the racing line, then the class's vf5 */
#define RESTORED_INTRO_VF37 0xeffe0020u   /* the intro's slot 37: Activity::vf37, then the podium cast */
#define RESTORED_WP_VF5     0xeffe0030u   /* the waypoint practice / main event's slot 5: the area's gates, then the class's vf5 */
#define RESTORED_FMX_VF5    0xeffe0060u   /* the Freestyle practice / main event's slot 5: the area's subscene, then the class's vf5 */
#define RESTORED_TIMED_VF42 0xeffe0040u   /* TIMED RACES: the race classes' slot 42: the clock, then the class's end-of-race check */
#define RESTORED_TIMED_VF61 0xeffe0050u   /* TIMED RACES: the race classes' slot 61: finished at the laps, or crossed after the flag */

/* the championship race classes' vtables (race_champ.h) */
#define VT_INTRO    0x00943654u
#define VT_PRACTICE 0x00943754u
#define VT_HEAT     0x00943854u
#define VT_MAIN     0x00943954u
#define VT_LCQ      0x00943a54u
#define VT_WP_PRACTICE 0x00943d14u
#define VT_WP_MAIN     0x00943e14u
#define VT_WP_INTRO    0x00943f14u
#define VT_FMX_INTRO   0x009443ccu
#define VT_FMX_PRACTICE 0x009444ccu
#define VT_FMX_MAIN     0x009445ccu

/* the exe's own column names */
#define S_ACTIVITY_TYPE   0x0093dd90u   /* "ActivityType" */
#define S_NUM_LAPS        0x0093dd64u   /* "NumLaps" */
#define S_SERIES_POINTS   0x0094d8d4u   /* "SeriesPoints" */
#define S_SERIES_POINT_ID 0x0094d830u   /* "SeriesPointId" */
#define S_PLACE           0x0094d8e4u   /* "Place" */
#define S_POINTS          0x0094d6b8u   /* "Points" */

/* The track's objects. Activity::vf5 (00556c30) loads the activity area's subscene and finalizes the physics world (00769030: the
 * forest, then the broadphase grids: their extent set to the union of every keyed / static box, the cells rebuilt) only for the race /
 * intro types (up to 0x21): the championship practice / heat / main event / LCQ classes (0x22 .. 0x29) and the Freestyle practice /
 * mains (0x3a .. 0x3c) never run it. The track's props (banners, tuff blocks, tire stacks, hay bales) are in the world's object vectors
 * either way (the same record counts as a single race), but without the finalize the grids keep the zero extent of their construction:
 * a grid holds only what its first cell takes (NAT_Concord: 500 of the 728 static objects, 40 of the 58 keyed ones; SX_Springfield: 40
 * of the 669 keyed ones, the hay bales) and the vehicles' contact queries miss the rest, so the bike rode through them (the playtest
 * of 2026-11-03; core/tools/champ_collision_probe.c measures it). The console's series loaded the area with an earlier sub event.
 * Here it is loaded once on the discipline's base type (the same area a single race of the event loads: Activity::vf5 names the area
 * by the type, rx_activity_area_name), before the class's own vf5 (which then finds +0xec8 set). */
static void restored_load_area(rx_addr act, uint32_t base_type) {
    if (U32(act, 0x238) == 0 || U32(act, 0xec8) != 0) return;
    const uint32_t type = U32(act, 0x23c);
    SU32(act, 0x23c, base_type);
    ACT_CALL(0, rx_activity_vf5(act));
    SU32(act, 0x23c, type);
}

/* slot 5 of the race classes: the racing line into the activity's track block (+0x890), as MXActivityRaceStub::vf5 (00565a30) does
 * for a race (FUN_00536ef0: the level +0x20030, the block, the type's track kind, the area +0x220), when the block is still empty (+0xbb0) and the
 * activity is a live one (+0x238); then the class's own vf5 (the intro's / heat's 0057e0b0, else 0057e410). */
static void restored_race_vf5(rx_addr act) {
    const rx_addr vt = U32(act, 0);
    const rx_addr world = U32(act, 0x234);
    /* the track kind MXActivityRaceStub::vf5 (00565a30) loads for the type's group: National 0, Supercross 1, Supermoto 2, Opencross 3,
     * Minimoto 5, Endurocross 9 (the intro, 0x1f: the series' base type, rx_series_current_type) */
    uint32_t type = U32(act, 0x23c), kind = 0;
    if (type == 0x1f) type = rx_series_current_type();
    switch (type) {
    case 1: case 0x26: case 0x27: case 0x28: case 0x29: kind = 1; break;
    case 2: case 0x2e: case 0x2f: kind = 3; break;
    case 3: case 0x32: case 0x33: case 0x34: case 0x35: kind = 2; break;
    case 4: case 0x36: case 0x37: case 0x38: case 0x39: kind = 5; break;
    case 7: case 0x2a: case 0x2b: case 0x2c: case 0x2d: kind = 9; break;
    default: kind = 0; break;
    }
    if (U32(act, 0xbb0) == 0 && U32(act, 0x238) != 0) {
        uint32_t loaded;
        ACT_CALLR(loaded, 3, rx_track_load(world + 0x20030, act + 0x890, kind, U32(act, 0x220)));
        (void)loaded;
    }
    {   /* the track's objects (above): National 0x22 .. 0x25 on National's area, Supercross 0x26 .. 0x29 on Supercross's */
        const uint32_t t = U32(act, 0x23c);
        if (t >= 0x22 && t <= 0x25) restored_load_area(act, 0);
        else if (t >= 0x26 && t <= 0x29) restored_load_area(act, 1);
    }
    if (vt == VT_INTRO || vt == VT_HEAT) rx_champ_heat_vf5(act);
    else rx_champ_lcq_vf5(act);
}
RX_FUNCTION(RESTORED_RACE_VF5, restored_race_vf5)

/* slot 37 of the intros (race and waypoint): Activity::vf37, then the scenario cast bound to the activity (0074ada0 on the world's
 * +0x20150 block), the call MXActivityRaceStub::vf37 (00568140) and MXActivityWaypointRace::vf37 (00578f40) make on every state entry. */
static void restored_intro_vf37(rx_addr act) {
    RACE_FRAME_PLAIN(F, 0, 1);
    RACE_CORE_CALL(1, rx_activity_vf37(act));
    RACE_CORE_CALL(0, rx_podium_setup(act, U32(act, 0x234) + 0x20150));
    RACE_FRAME_END(F);
}
RX_FUNCTION(RESTORED_INTRO_VF37, restored_intro_vf37)

/* slot 5 of the waypoint practice / main event: MXActivityWaypointRace::vf5 (00578500) loads the area's subscene (the gates) through
 * Activity::vf5 only for types up to the intro (0x21), so a practice (0x30) or main event (0x31) had no gates (the original then divides
 * by zero in BehaviorRoamToWaypointGate::vf6). The console's series loaded them with the intro; here Activity::vf5 runs once on the
 * intro's type first (the area +0xec8 still empty, a live activity +0x238), then the class's own vf5. (The stimulus of w-champ and
 * champ_native_test do the same.) */
static void restored_wp_vf5(rx_addr act) {
    if (U32(act, 0x238) != 0 && U32(act, 0xec8) == 0) {
        const uint32_t type = U32(act, 0x23c);
        SU32(act, 0x23c, 0x21);
        ACT_CALL(0, rx_activity_vf5(act));
        SU32(act, 0x23c, type);
    }
    ACT_CALL(0, rx_waypoint_activity_vf5(act));
}
RX_FUNCTION(RESTORED_WP_VF5, restored_wp_vf5)

/* slot 5 of the Freestyle practice / main events: the area's subscene on Freestyle's type (5), then their own vf5 (0057ded0) */
static void restored_fmx_vf5(rx_addr act) {
    restored_load_area(act, 5);
    ACT_CALL(0, rx_freestyle_champ_main_vf5(act));
}
RX_FUNCTION(RESTORED_FMX_VF5, restored_fmx_vf5)

/* an int cell written as Table_SetCellInt does (the activity table's revision 0x00ae1b80 counts a change) */
static void set_activity_cell(rx_addr column_name, uint32_t row, uint32_t value) {
    const uint32_t column = rx_tbl_column_index(ACT_ACTIVITY_TBL, column_name);
    if (column >= ((U32(ACT_ACTIVITY_TBL, 0x14) - U32(ACT_ACTIVITY_TBL, 0x10)) >> 2)) act_unreached(0x00916224, "restored champ: activity column");
    if (value != U32(rx_tbl_cell_word(ACT_ACTIVITY_TBL, column, row), 0)) SU32(0, 0x00ae1b80, U32(0, 0x00ae1b80) + 1);
    ACT_FRAME(F, 4, 0);
    SU32(F, 0, value);
    ACT_CALL(3, rx_tbl_set_int(U32(U32(ACT_ACTIVITY_TBL, 0x10), 4 * column), row, F));
    ACT_END(F);
}

/* a Player List int cell written as Table_SetCellInt does (the Player List's revision 0x00ae0e70 counts a change) */
static void set_player_cell(rx_addr column_name, uint32_t row, uint32_t value) {
    if (value != act_cell(ACT_PLAYER_LIST, column_name, row)) SU32(0, 0x00ae0e70, U32(0, 0x00ae0e70) + 1);
    ACT_FRAME(F, 4, 0);
    SU32(F, 0, value);
    ACT_CALL(3, rx_tbl_set_int(act_column(ACT_PLAYER_LIST, column_name), row, F));
    ACT_END(F);
}

uint32_t rx_restored_champ_player_rows(void) { return rx_tbl_row_count(ACT_PLAYER_LIST); }

void rx_restored_champ_player_name(uint32_t row, char* out, uint32_t capacity) {
    const rx_addr str = rx_tbl_cell_string(act_column(ACT_PLAYER_LIST, 0x00936f00 /* "PlayerName" */), row);
    const rx_addr p = rx_tbl_str_cstr(str);
    uint32_t i = 0;
    for (; i + 1 < capacity && U8(p, i); i++) out[i] = (char)U8(p, i);
    out[i] = 0;
}

/* ---- TIMED RACES (PORT ADDITION: not original logic; include/rx_champ_weekend.h) ---------------------------------------------------
 * The clock is the activity's own InProgress clock (A+0x294, Activity::vf28 adds dt before it calls vf42). While it runs the lap total
 * A+0xff070 is RX_CHAMP_TIMED_LAPS (the row's NumLaps, rx_restored_champ_apply); at the clock's end it becomes the leader's completed
 * laps + 1 + extra (the standings' first, A+0x2d0); once a driver reaches it (the leader takes the flag) every driver's lap count is
 * kept, and slot 61 answers "finished" for a driver whose count rose after that: the original's end-of-race check (vf42 00566ee0) then
 * marks it finished (+0x70) and counts it as it counts a driver at the lap total. Only host statics and A+0xff070 are written. The
 * drivers are the activity's list (A+0x268, A+0x26c drivers: the field size, no fixed rider count). */
#define TIMED_MAX_DRIVERS 64
static struct {
    int on;                        /* the session armed a timed stage */
    float seconds;
    int32_t extra, expired, flag;
    float expired_at, flag_at, hold_until;
    int32_t laps_at_flag[TIMED_MAX_DRIVERS];
    int32_t finished[TIMED_MAX_DRIVERS], finish_laps[TIMED_MAX_DRIVERS];   /* a driver's laps when it was flagged (+0x70 set): it rides on, its count too */
    struct { rx_addr vt, vf42, vf61; } classes[4];   /* the race classes' own slots, read before the patch */
} timed;

static int32_t timed_drivers(rx_addr act) { const int32_t n = I32(act, 0x26c); return n < 0 ? 0 : n > TIMED_MAX_DRIVERS ? TIMED_MAX_DRIVERS : n; }
static rx_addr timed_driver(rx_addr act, int32_t i) { return U32(U32(act, 0x268), 4u * (uint32_t)i); }
static int32_t timed_laps(rx_addr d) { return d ? I32(d, 0xf1c) : 0; }   /* the race driver's completed laps (RaceDriver +0xf1c) */
static int timed_class(rx_addr act) {
    const rx_addr vt = U32(act, 0);
    for (int i = 0; i < 4; i++) if (timed.classes[i].vt == vt) return i;
    return -1;
}

/* the laps of every driver at the frame it finished (the finished flag +0x70 rising) */
static void timed_capture(rx_addr act) {
    const int32_t n = timed_drivers(act);
    for (int32_t i = 0; i < n; i++) {
        const rx_addr d = timed_driver(act, i);
        if (d && !timed.finished[i] && I32(d, 0x70) != 0) { timed.finished[i] = 1; timed.finish_laps[i] = timed_laps(d); }
    }
}

/* the clock (every InProgress frame, before the class's end-of-race check) */
static void timed_tick(rx_addr act) {
    if (!timed.on || I32(act, 0x21c) != 2) return;
    const float clock = F32(act, 0x294);
    if (!timed.expired && clock >= timed.seconds) {
        const rx_addr leader = U32(act, 0x2d0);
        const int32_t laps = timed_laps(leader);
        int32_t total = laps + 1 + timed.extra;
        if (total > RX_CHAMP_TIMED_LAPS) total = RX_CHAMP_TIMED_LAPS;
        SU32(act, 0xff070, (uint32_t)total);
        timed.expired = 1;
        timed.expired_at = clock;
    }
    if (timed.expired && !timed.flag) {
        const int32_t n = timed_drivers(act), total = I32(act, 0xff070);
        for (int32_t i = 0; i < n; i++) if (timed_laps(timed_driver(act, i)) >= total) timed.flag = 1;
        if (timed.flag) {
            for (int32_t i = 0; i < n; i++) timed.laps_at_flag[i] = timed_laps(timed_driver(act, i));
            timed.flag_at = clock;
            /* the race waits for the riders still on track (each finishes at its next crossing) at most 1.5 of the leader's average lap
             * (30 s at least) after the flag; a rider who has not crossed by then is left to the original's end of race */
            const float lap = total > 0 ? clock / (float)total : 60.0f;
            timed.hold_until = clock + (1.5f * lap > 30.0f ? 1.5f * lap : 30.0f);
        }
    }
}

static uint32_t restored_timed_vf61(rx_addr act, uint32_t index);
static void restored_timed_vf42(rx_addr act) {
    if (timed.on) timed_capture(act);
    timed_tick(act);
    const int c = timed_class(act);
    if (c < 0) act_unreached(RESTORED_TIMED_VF42, "restored champ: timed vf42 on an unpatched class");
    /* after the flag every rider finishes at its next crossing: the original's check would end the race as soon as the humans are
     * done (projecting the AI still on track), so until every driver is finished (or the hold runs out) the finished ones are marked
     * here as the check marks them (+0x70 from slot 61) and the check itself waits */
    if (timed.flag && I32(act, 0x21c) == 2 && F32(act, 0x294) < timed.hold_until) {
        const int32_t n = timed_drivers(act);
        int all = 1;
        for (int32_t i = 0; i < n; i++) {
            const rx_addr d = timed_driver(act, i);
            if (!d || I32(d, 0x70) != 0) continue;
            if (restored_timed_vf61(act, (uint32_t)i)) SU32(d, 0x70, 1); else all = 0;
        }
        timed_capture(act);
        if (!all) return;
    }
    race_thiscall(timed.classes[c].vf42, act, 0, 0, 0, 0, 0);
    if (timed.on) timed_capture(act);   /* (the check marks the drivers it finished: the last InProgress frame's too) */
}
RX_FUNCTION(RESTORED_TIMED_VF42, restored_timed_vf42)

static uint32_t restored_timed_vf61(rx_addr act, uint32_t index) {
    const int c = timed_class(act);
    if (c < 0) act_unreached(RESTORED_TIMED_VF61, "restored champ: timed vf61 on an unpatched class");
    typedef uint32_t (*fn_t)(rx_addr, uint32_t);
    fn_t f = RX_FN(timed.classes[c].vf61, fn_t);
    uint32_t done;
    if (f) { RACE_FRAME_PLAIN(F, 0, 2); RACE_CORE_CALL(1, done = f(act, index)); RACE_FRAME_END(F); }
    else done = race_thiscall(timed.classes[c].vf61, act, 1, index, 0, 0, 0);
    if (done || !timed.flag || (int32_t)index >= timed_drivers(act)) return done;
    return timed_laps(timed_driver(act, (int32_t)index)) > timed.laps_at_flag[index];
}
RX_FUNCTION(RESTORED_TIMED_VF61, restored_timed_vf61)

void rx_restored_champ_timed_arm(float seconds, int32_t extra) {
    memset(&timed, 0, sizeof timed);
    timed.on = 1;
    timed.seconds = seconds;
    timed.extra = extra;
    static const uint32_t race_vt[4] = {VT_PRACTICE, VT_HEAT, VT_MAIN, VT_LCQ};
    for (int i = 0; i < 4; i++) {
        timed.classes[i].vt = race_vt[i];
        timed.classes[i].vf42 = U32(race_vt[i], 42u * 4u);
        timed.classes[i].vf61 = U32(race_vt[i], 61u * 4u);
        SU32(race_vt[i], 42u * 4u, RESTORED_TIMED_VF42);
        SU32(race_vt[i], 61u * 4u, RESTORED_TIMED_VF61);
    }
}

int32_t rx_restored_champ_timed_finish_laps(rx_addr act, rx_addr driver) {
    if (!timed.on || !act) return -1;
    const int32_t n = timed_drivers(act);
    for (int32_t i = 0; i < n; i++) if (timed_driver(act, i) == driver) return timed.finished[i] ? timed.finish_laps[i] : -1;
    return -1;
}

int rx_restored_champ_timed_state(rx_addr act, struct rx_restored_champ_clock* out) {
    memset(out, 0, sizeof *out);
    if (!timed.on) return 0;
    out->seconds = timed.seconds; out->extra = timed.extra; out->expired = timed.expired; out->flag = timed.flag;
    out->expired_at = timed.expired_at; out->flag_at = timed.flag_at;
    if (act) { out->elapsed = I32(act, 0x21c) >= 2 ? F32(act, 0x294) : 0.0f; out->num_laps = I32(act, 0xff070); }
    return 1;
}

int rx_restored_champ_apply(uint32_t base_type, uint32_t type, int32_t laps, const int32_t* in_field, const int32_t* zone, uint32_t rows) {
    int32_t row;
    ACT_CALLR(row, 1, rx_activity_find_row(ACT_ACTIVITY_TBL, base_type, 0));
    if (row < 0) return -1;
    memset(&timed, 0, sizeof timed);   /* lap only until rx_restored_champ_timed_arm (host statics) */
    set_activity_cell(S_ACTIVITY_TYPE, (uint32_t)row, type);
    if (laps > 0) set_activity_cell(S_NUM_LAPS, (uint32_t)row, (uint32_t)laps);
    const uint32_t area = act_cell(ACT_ACTIVITY_TBL, 0x0093dda0 /* "ActivityArea" */, (uint32_t)row);
    const uint32_t players = rx_tbl_row_count(ACT_PLAYER_LIST);
    for (uint32_t r = 0; r < players; r++) {
        /* the stage's field, as MXSeriesEvent_SetupNextActivityPlayers (005a8af0) and SetupActivity (005a9290) make it: a rider of the
         * stage is Active and requests the stage's activity; a rider out of it is not Active and requests none (0x3d) */
        const int out = in_field && r < rows && in_field[r] == 0;
        if (in_field && r < rows && in_field[r] >= 0) set_player_cell(0x0093def8 /* "Active" */, r, out ? 0u : 1u);
        ACT_CALL(2, rx_player_list_request_activity(r, out ? 0x3du : type, out ? 0u : area));
        /* the start gate: the Player List's StartZone (the column the career's gate pick writes, 005a9e00; the join reads it, 0054cd20) */
        if (zone && r < rows && zone[r] >= 0) set_player_cell(0x0093df00 /* "StartZone" */, r, (uint32_t)zone[r]);
    }
    static const uint32_t race_vt[5] = {VT_INTRO, VT_PRACTICE, VT_HEAT, VT_MAIN, VT_LCQ};
    for (int i = 0; i < 5; i++) SU32(race_vt[i], 0x14, RESTORED_RACE_VF5);
    SU32(VT_INTRO, 0x94, RESTORED_INTRO_VF37);
    SU32(VT_WP_INTRO, 0x94, RESTORED_INTRO_VF37);   /* the waypoint intro's slot 37 is the same Activity::vf37 (0057e900) */
    SU32(VT_FMX_INTRO, 0x94, RESTORED_INTRO_VF37);  /* and the Freestyle intro's (MXActivityFreestyleChampIntro, 0057e900) */
    SU32(VT_FMX_PRACTICE, 0x14, RESTORED_FMX_VF5);
    SU32(VT_FMX_MAIN, 0x14, RESTORED_FMX_VF5);
    SU32(VT_WP_PRACTICE, 0x14, RESTORED_WP_VF5);
    SU32(VT_WP_MAIN, 0x14, RESTORED_WP_VF5);
    return 0;
}

uint32_t rx_restored_champ_type_of(rx_addr name) {
    uint32_t type;
    ACT_CALLR(type, 1, rx_event_type_of(name));
    return type;
}

int32_t rx_restored_champ_points(int32_t id, int32_t place) {
    rx_addr t;
    ACT_CALLR(t, 1, rx_tbl_find(RX_TBL_DEFAULT_DB, S_SERIES_POINTS));
    if (!t) return 0;
    int32_t found = -1;
    uint32_t matches = 0;
    for (uint32_t row = 0; row < rx_tbl_row_count(t); row++) {
        if ((int32_t)act_cell(t, S_SERIES_POINT_ID, row) != id || (int32_t)act_cell(t, S_PLACE, row) != place) continue;
        found = (int32_t)row;
        matches++;
    }
    return matches == 1 ? (int32_t)act_cell(t, S_POINTS, (uint32_t)found) : 0;
}

int32_t rx_restored_champ_base_type(int32_t type) {
    switch (type) {
    case 0x1f: case 0x22: case 0x23: case 0x24: case 0x25: return 0;   /* RaceChampIntro, NationalChamp Practice / Heat / MainEvent / MainEvent2 */
    case 0x26: case 0x27: case 0x28: case 0x29: return 1;              /* SupercrossChamp Practice / Heat / MainEvent / LCQ */
    case 0x20: case 0x3a: case 0x3b: case 0x3c: return 5;              /* FreestyleChampIntro, FreestyleChamp Practice / MainEvent / MainEvent2 */
    case 0x21: case 0x30: case 0x31: return 6;                         /* WaypointRaceChampIntro, WaypointRaceChamp Practice / MainEvent */
    default: return type;
    }
}

