/* champ_native_test.c -- the championship race activity classes (core/game/race_champ.c) run by the product library, no emulator.
 *   champ_native_test <install root> <activity type: 34 35 36 41 | 48 49> [race frames (600)]
 *
 * The product's event spec names the activity type with the UI's EventType string, which SetupEvent keeps in a 16-byte buffer (like
 * the original): only "RaceChampIntro" fits, and the PC game's tables have no championship series. So this driver, like the oracle
 * stimulus (core/oracle/race_champ_probe.cpp), builds an arcade event (National NAT_Concord, or WaypointRace FR_Gibson for the
 * waypoint classes) and changes the type of the event's activity-table row after the event load, so the world's factory (0055ef80)
 * builds the championship class, which the core then starts and runs with its own code:
 *   - the race classes' vf5 (0057e0b0 / 0057e410) do not load the racing-line track MXActivityRaceStub::vf5 loads (the console flow
 *     lets another activity do it): slot 5 of their vtables is pointed at a function of this driver that loads it first;
 *   - the waypoint classes' vf5 (shared MXActivityWaypointRace::vf5) loads the area's gates only below the intro type: the driver's
 *     slot 5 loads the area on the intro's type first.
 * Linked with the product library's objects (builds/champ: core/build.sh <dir>; the objects in <dir>/obj), because it uses the
 * core's internal headers. Exit 0 = the load and every race frame ran. */
#include "rx_api.h"
#include "../game/build_activity.h"
#include "../game/build_activity_util.h"
#include "../game/build_tables.h"
#include "../game/build_vehicle.h"
#include "../game/race_champ.h"
#include "../game/waypoint_activity.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

#define FAKE_RACE_VF5 0xefff0f50u
#define FAKE_WP_VF5 0xefff0f60u

/* the race classes: the track (as MXActivityRaceStub::vf5 does for the National audio kind), then the class's own vf5 */
static void champ_race_vf5(rx_addr act) {
    const rx_addr vt = U32(act, 0);
    const rx_addr world = U32(act, 0x234);
    if (I32(act, 0xff070) == -1) SU32(act, 0xff070, 3);   /* StartEvent leaves NumLaps -1 in a row of a type it did not set up */
    if (U32(act, 0xbb0) == 0 && U32(act, 0x238) != 0) rx_track_load(world + 0x20030, act + 0x890, 0, U32(act, 0x220));
    if (vt == 0x00943654 || vt == 0x00943854) rx_champ_heat_vf5(act);
    else rx_champ_lcq_vf5(act);
}
RX_FUNCTION(FAKE_RACE_VF5, champ_race_vf5)

/* the waypoint classes: the area's subscene (the gates) on the intro's type, then MXActivityWaypointRace::vf5 */
static void champ_wp_vf5(rx_addr act) {
    if (U32(act, 0x238) != 0 && U32(act, 0xec8) == 0) {
        const uint32_t type = U32(act, 0x23c);
        SU32(act, 0x23c, 0x21);
        rx_activity_vf5(act);
        SU32(act, 0x23c, type);
    }
    rx_waypoint_activity_vf5(act);
}
RX_FUNCTION(FAKE_WP_VF5, champ_wp_vf5)

static const char* g_step = "start";
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "champ_native_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: champ_native_test <install root> <activity type> [race frames]\n"); return 2; }
    const uint32_t type = (uint32_t)atoi(argv[2]);
    const int frames = argc > 3 ? atoi(argv[3]) : 600;
    const int waypoint = type == 0x21 || type == 0x30 || type == 0x31;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1200);
    setvbuf(stdout, NULL, _IONBF, 0);

    g_step = "session create";
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "champ_native_test: rx_session_create failed\n"); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (waypoint) {
        snprintf(spec.event_type, sizeof spec.event_type, "WaypointRace");
        snprintf(spec.location, sizeof spec.location, "FR_Gibson");
        spec.event_type_id = 14; spec.location_id = 47; spec.duration = 1;
    }
    g_step = "event load";
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "champ_native_test: event load: %s\n", rx_session_last_error(s)); return 1; }

    /* the event's row in the activity table: its type becomes the championship type; slot 5 of the championship classes' vtables */
    g_step = "patch the activity row and the vtables";
    const uint32_t base = waypoint ? 6 : 0;
    const int32_t row = rx_activity_find_row(ACT_ACTIVITY_TBL, base, 0);
    if (row < 0) { fprintf(stderr, "champ_native_test: no activity row of type %u\n", base); return 1; }
    const rx_addr cell = rx_tbl_cell_word(ACT_ACTIVITY_TBL, rx_tbl_column_index(ACT_ACTIVITY_TBL, 0x0093dd90 /* "ActivityType" */), (uint32_t)row);
    SU32(cell, 0, type);
    static const uint32_t race_vt[5] = {0x00943654, 0x00943754, 0x00943854, 0x00943954, 0x00943a54};
    for (int i = 0; i < 5; i++) SU32(race_vt[i], 0x14, FAKE_RACE_VF5);
    SU32(0x00943d14, 0x14, FAKE_WP_VF5);   /* waypoint practice */
    SU32(0x00943e14, 0x14, FAKE_WP_VF5);   /* waypoint main event */
    /* and the players' requested activity (the Player List rows), which the world's player-join matches against the activities */
    const uint32_t players = rx_tbl_row_count(ACT_PLAYER_LIST);
    for (uint32_t r = 0; r < players; r++) {
        const rx_addr stack = rx_sp();
        rx_set_sp(stack - 12);
        rx_player_list_request_activity(r, type, 0);
        rx_set_sp(stack);
    }
    printf("champ_native_test: activity row %d type %u -> %u, %u players\n", row, base, type, players);

    g_step = "load frames";
    int load_frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && load_frames < 10000) {
        if (rx_session_advance(s) != 0) break;
        load_frames++;
    }
    printf("champ_native_test: %d load frames, state %d, game frames %llu\n", load_frames, (int)rx_session_get_state(s), (unsigned long long)rx_session_frames(s));
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { fprintf(stderr, "champ_native_test: the load failed: %s\n", rx_session_last_error(s)); return 1; }

    g_step = "race frames";
    int last_state = -1;
    for (int i = 0; i < frames; i++) {
        if (rx_session_advance(s) != 0) { fprintf(stderr, "champ_native_test: advance %d: %s\n", i + 1, rx_session_last_error(s)); return 1; }
        rx_race_state race; memset(&race, 0, sizeof race);
        if (rx_session_race(s, &race) == 0 && (race.state != last_state || (i % 600) == 0)) {
            printf("champ_native_test: frame %d race state %d laps %d/%d place %d time %.2f\n", i + 1, race.state, race.laps_completed, race.num_laps, race.place, race.race_time);
            last_state = race.state;
        }
    }
    rx_race_state race; memset(&race, 0, sizeof race);
    rx_session_race(s, &race);
    printf("champ_native_test: after %d race frames: session state %d, race state %d, laps %d/%d, place %d, time %.2f\n", frames, (int)rx_session_get_state(s), race.state,
           race.laps_completed, race.num_laps, race.place, race.race_time);
    rx_session_destroy(s);
    return 0;
}
