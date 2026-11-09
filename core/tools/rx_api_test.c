/* rx_api_test.c -- the first-call driver of the OPEN REFLEX session API (core/include/rx_api.h).
 *   rx_api_test <install root> [race frames (10)] [level]
 *
 * Creates a session from the install, loads the reference event (or the level given as the third argument), advances through
 * MXGame::Update's load states (at most 10000 frames), then runs the requested race frames and reads every first-ride getter.
 * It does not fake a pass: if the product stops on an unported game function (rx_call_original), the abort handler prints the
 * step and the backtrace; the address is the line rx_mem prints on stderr. Exit: 0 the session ran, 1 an API call failed,
 * 128+signal when the game aborted. */
#include "rx_api.h"

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static char g_step_buf[160];
static const char* g_step = "start";

static void step(const char* s) {
    g_step = s;
    fprintf(stderr, "rx_api_test: %s\n", s);
}

static void stepf(const char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(g_step_buf, sizeof g_step_buf, fmt, ap);
    va_end(ap);
    g_step = g_step_buf;
    fprintf(stderr, "rx_api_test: %s\n", g_step_buf);
}

static void on_signal(int sig) {
    char buf[320];
    const int n = snprintf(buf, sizeof buf, "rx_api_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}

static const char* state_name(rx_session_state s) {
    switch (s) {
        case RX_SESSION_LOADING: return "loading";
        case RX_SESSION_COUNTDOWN: return "countdown";
        case RX_SESSION_RACING: return "racing";
        case RX_SESSION_FINISHED: return "finished";
        case RX_SESSION_ERROR: return "error";
    }
    return "?";
}

static void print_error(const char* what, const rx_session* s) {
    const char* e = rx_session_last_error(s);
    fprintf(stderr, "rx_api_test: %s failed: %s\n", what, e && *e ? e : "(no error text)");
}

int main(int argc, char** argv) {
    if (argc < 2) {
        fprintf(stderr, "usage: rx_api_test <install root> [race frames (10)] [level]\n");
        return 2;
    }
    const int frames = argc > 2 ? atoi(argv[2]) : 10;
    const char* level = argc > 3 ? argv[3] : NULL;

    signal(SIGABRT, on_signal);
    signal(SIGSEGV, on_signal);
    signal(SIGBUS, on_signal);
    signal(SIGALRM, on_signal);
    {   /* a hung game path (e.g. the null package cache before the parts-loader stand-in) would otherwise never report */
        const char* w = getenv("RX_API_TEST_WATCHDOG");
        alarm(w && atoi(w) > 0 ? (unsigned)atoi(w) : 300u);
    }
    setvbuf(stdout, NULL, _IONBF, 0);

    printf("rx_api_test: api version %u\n", rx_session_api_version());

    step("session create (memory, install, image, startup)");
    rx_session* s = rx_session_create(argv[1]);
    if (!s) {
        print_error("rx_session_create", NULL);
        return 1;
    }
    printf("rx_api_test: session version %u state %d (%s)\n", rx_session_version(s),
           (int)rx_session_get_state(s), state_name(rx_session_get_state(s)));

    step("event load (spec, level, front-end state, EnsureLocalPlayers, SetupEvent, StartEvent)");
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (level) snprintf(spec.location, sizeof spec.location, "%s", level);
    if (getenv("RX_API_TEST_EVENT_TYPE")) snprintf(spec.event_type, sizeof spec.event_type, "%s", getenv("RX_API_TEST_EVENT_TYPE"));   /* another activity type by its enum name (e.g. NationalChampMainEvent) */
    /* MODS/TRACKS: RX_API_TEST_MOUNT=<folder> reads the level from a custom track's folder (rx_session_mount_level); a Free Ride
     * (RX_API_TEST_EVENT_TYPE=FreeRide) is the client's custom Free Ride: one rider, no rules, no location row; RX_API_TEST_RIDERS /
     * RX_API_TEST_LAPS set a race's riders and laps */
    if (!strcmp(spec.event_type, "FreeRide")) {
        spec.event_type_id = 9; spec.location_id = -1; spec.track_id = 0; spec.duration = 0; spec.duration_type[0] = 0;
        spec.riders = 1; spec.ai_difficulty = 0; spec.vehicle_class = -1;
    }
    if (getenv("RX_API_TEST_RIDERS")) spec.riders = atoi(getenv("RX_API_TEST_RIDERS"));
    if (getenv("RX_API_TEST_LAPS")) { spec.duration = atoi(getenv("RX_API_TEST_LAPS")); snprintf(spec.duration_type, sizeof spec.duration_type, "Laps"); }
    if (getenv("RX_API_TEST_LOCATION_ID")) spec.location_id = atoi(getenv("RX_API_TEST_LOCATION_ID"));
    if (getenv("RX_API_TEST_MOUNT")) {
        if (rx_session_mount_level(s, spec.location, getenv("RX_API_TEST_MOUNT")) != 0) { print_error("rx_session_mount_level", s); return 1; }
        printf("rx_api_test: mounted %s from %s\n", spec.location, getenv("RX_API_TEST_MOUNT"));
    }
    printf("rx_api_test: event '%s' location '%s' riders %d ai %d vehicle class %d duration %d %s\n",
           spec.event_type, spec.location, spec.riders, spec.ai_difficulty, spec.vehicle_class,
           spec.duration, spec.duration_type);
    if (getenv("RX_MAP_SCALE")) {   /* MAP SCALE: the event's track scaled (rx_session_set_map_scale) */
        const float ms = (float)atof(getenv("RX_MAP_SCALE"));
        if (rx_session_set_map_scale(s, ms) != 0) { print_error("rx_session_set_map_scale", s); return 1; }
        printf("rx_api_test: map scale %.3f\n", rx_session_map_scale(s));
    }
    if (rx_session_event_load(s, &spec) != 0) {
        print_error("rx_session_event_load", s);
        rx_session_destroy(s);
        return 1;
    }

    /* The world is not up yet: this pass proves the getters' no-data paths return without touching unmapped objects. The
     * real values are read after the load (if the product reaches it; the player-spawn stop aborts in the load frames). */
    step("pre-race getters (no world yet: the no-data paths)");
    static float rider_pose[69 * 16];
    static float bike_pose[64 * 16];
    {
        rx_bike_state b; rx_camera c; rx_race_state r; rx_terrain_info t; rx_particle_info p; rx_mud_state m;
        rx_driver_result d[16]; rx_slot sl[16];
        memset(&b, 0, sizeof b); memset(&c, 0, sizeof c); memset(&r, 0, sizeof r); memset(&t, 0, sizeof t);
        memset(&p, 0, sizeof p); memset(&m, 0, sizeof m); memset(d, 0, sizeof d); memset(sl, 0, sizeof sl);
        printf("rx_api_test: pre-race getters bike %d camera %d race %d terrain %d particles %d mud %d standings %d slots %d\n",
               rx_session_bike(s, &b), rx_session_camera(s, &c), rx_session_race(s, &r),
               rx_session_terrain(s, &t), rx_session_particles(s, &p), rx_session_mud(s, -1, &m),
               rx_session_standings(s, d, 16), rx_session_slots(s, sl, 16));
        printf("rx_api_test: pre-race rider pose %d bike pose %d\n",
               rx_session_rider_pose(s, rider_pose, 69), rx_session_bike_pose(s, bike_pose, 64));
        rx_ambient_info a;
        memset(&a, 0, sizeof a);
        printf("rx_api_test: pre-race ambient %d entities %d\n", rx_session_ambient(s, &a), a.entities);
    }

    step("load frames (MXGame::Update states 3..0xb)");
    int load_frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && load_frames < 10000) {
        if ((load_frames % 100) == 0) stepf("load frame %d (state %s)", load_frames, state_name(rx_session_get_state(s)));
        if (rx_session_advance(s) != 0) break;
        load_frames++;
    }
    printf("rx_api_test: %d load frames, state %d (%s), game frames %llu\n", load_frames,
           (int)rx_session_get_state(s), state_name(rx_session_get_state(s)),
           (unsigned long long)rx_session_frames(s));
    if (rx_session_get_state(s) == RX_SESSION_ERROR) {
        print_error("the load", s);
        rx_session_destroy(s);
        return 1;
    }

    stepf("race frames (%d)", frames);
    for (int i = 0; i < frames; i++) {
        stepf("race frame %d of %d", i + 1, frames);
        if (rx_session_advance(s) != 0) {
            fprintf(stderr, "rx_api_test: advance %d returned an error\n", i + 1);
            break;
        }
    }
    printf("rx_api_test: after %d race frames: state %d (%s), frames %llu, dt %.4f, game time %.3f\n",
           frames, (int)rx_session_get_state(s), state_name(rx_session_get_state(s)),
           (unsigned long long)rx_session_frames(s), rx_session_frame_dt(s), rx_session_game_time(s));
    if (rx_session_get_state(s) == RX_SESSION_ERROR) print_error("the race frames", s);

    step("getters: bike / rider / bike pose / camera");
    rx_bike_state bike;
    memset(&bike, 0, sizeof bike);
    const int bike_rc = rx_session_bike(s, &bike);
    if (bike_rc == 0)
        printf("rx_api_test: bike speed %.3f gear %d frames %u pos %.2f %.2f %.2f\n",
               bike.speed, bike.gear, bike.frames,
               bike.position[0][12], bike.position[0][13], bike.position[0][14]);
    else printf("rx_api_test: bike: rc %d\n", bike_rc);

    const int bones = rx_session_rider_pose(s, rider_pose, 69);
    const int nodes = rx_session_bike_pose(s, bike_pose, 64);
    rx_camera camera;
    memset(&camera, 0, sizeof camera);
    const int cam_rc = rx_session_camera(s, &camera);
    printf("rx_api_test: rider bones %d, bike pose nodes %d, camera rc %d (%.2f %.2f %.2f fov %.3f)\n",
           bones, nodes, cam_rc, camera.position[0], camera.position[1], camera.position[2], camera.fov);

    step("getters: race / standings / slots");
    rx_race_state race;
    memset(&race, 0, sizeof race);
    const int race_rc = rx_session_race(s, &race);
    printf("rx_api_test: race rc %d valid %d state %d laps %d/%d place %d riders %d time %.3f start %.3f countdown %d\n",
           race_rc, race.valid, race.state, race.laps_completed, race.num_laps, race.place, race.riders,
           race.race_time, race.start_timer, race.countdown_step);
    rx_driver_result standings[16];
    memset(standings, 0, sizeof standings);
    const int nstand = rx_session_standings(s, standings, 16);
    for (int i = 0; i < nstand; i++)
        printf("rx_api_test:   standing %d: slot %d local %d place %d laps %d finished %d time %.3f\n",
               i, standings[i].slot, standings[i].local, standings[i].place, standings[i].laps_completed,
               standings[i].finished, standings[i].race_time);
    rx_slot slots[16];
    memset(slots, 0, sizeof slots);
    const int nslots = rx_session_slots(s, slots, 16);
    for (int i = 0; i < nslots; i++)
        printf("rx_api_test:   slot %d: type %d local %d vehicle %08x rider %08x rows %d/%d\n",
               slots[i].slot, slots[i].type, slots[i].local, slots[i].vehicle, slots[i].rider,
               slots[i].vehicle_row, slots[i].rider_row);

    step("getters: terrain / particles / ambient / mud");
    rx_terrain_info terrain;
    memset(&terrain, 0, sizeof terrain);
    const int terrain_rc = rx_session_terrain(s, &terrain);
    printf("rx_api_test: terrain rc %d samples %u stride %u spacing %.3f min %.1f %.1f max %.1f %.1f gain %.3f version %u\n",
           terrain_rc, terrain.samples, terrain.stride, terrain.spacing, terrain.min_x, terrain.min_z,
           terrain.max_x, terrain.max_z, terrain.gain, terrain.deform_version);
    if (terrain_rc == 0 && terrain.samples > 0 && terrain.stride > 0) {
        static float heights[4 * 4], deform[4 * 4];
        const int hr = rx_session_terrain_heights(s, heights, 0, 0, 4, 4);
        const int dr = rx_session_terrain_deform(s, deform, 0, 0, 4, 4);
        printf("rx_api_test: terrain patch 4x4: heights rc %d (%.2f .. %.2f), deform rc %d (%.3f .. %.3f)\n",
               hr, heights[0], heights[15], dr, deform[0], deform[15]);
    }
    rx_particle_info particles;
    memset(&particles, 0, sizeof particles);
    const int part_rc = rx_session_particles(s, &particles);
    printf("rx_api_test: particles rc %d live %d normal %d additive %d soft %d texture '%s'\n",
           part_rc, particles.live, particles.normal, particles.additive, particles.soft, particles.texture);
    rx_ambient_info ambient;
    memset(&ambient, 0, sizeof ambient);
    const int amb_rc = rx_session_ambient(s, &ambient);
    printf("rx_api_test: ambient rc %d entities %d objects %d matrices %d fade %.3f lod divisor %.1f\n",
           amb_rc, ambient.entities, ambient.objects, ambient.matrices, ambient.fade_start, ambient.lod_divisor);
    rx_mud_state mud;
    memset(&mud, 0, sizeof mud);
    const int mud_rc = rx_session_mud(s, -1, &mud);
    printf("rx_api_test: mud rc %d record %d vehicle %08x rider %08x amount %.3f wetness %.3f count %d\n",
           mud_rc, mud.record, mud.vehicle, mud.rider, mud.amount[0], mud.wetness[0], rx_session_mud_count(s));

    const rx_session_state final_state = rx_session_get_state(s);
    if (final_state == RX_SESSION_ERROR) print_error("the session", s);
    step("destroy");
    rx_session_destroy(s);
    return final_state == RX_SESSION_ERROR ? 1 : 0;
}
