/* compass_native_test.c -- the HUD compass' markers (rx_session_compass, game/rx_compass_api.h) of the challenges Free Ride and its challenges.
 *   compass_native_test <install root> [frames (900)]
 * Runs the Race & Ride challenges Free Ride of FR_Alpine (InCareerMode) and each challenge started from an area of it (HillClimb, Trailblazer,
 * TargetPractice, BigAir ...), reads the compass marks after the frames and prints them. Exit 0 = the Free Ride lists its areas, TargetPractice its
 * targets and Trailblazer its finish.
 *   build: core/build.sh, then clang as for activation_native_test. */
#include "rx_api.h"
#include "../game/rx_activation_api.h"
#include "../game/rx_compass_api.h"
#include "../src/rx_mem.h"
#include "../src/rx_fn.h"
#include "../game/build_alloc.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static const char* g_step = "start";
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "compass_native_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}

static int fail(const char* what, rx_session* s) {
    fprintf(stderr, "compass_native_test: FAIL %s%s%s\n", what, s ? ": " : "", s ? rx_session_last_error(s) : "");
    return 1;
}

/* the local player's activity (the game's player slots, as the API's local_player) */
static rx_addr local_activity(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return 0;
    const uint32_t n = U8(g, 0x1541c);
    for (uint32_t i = 0; i < n && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) == 0) return U32(p, 0x94);
    }
    return 0;
}
static void read_activation(rx_session* s, rx_activation_state* a) {
    memset(a, 0, sizeof *a);
    a->size = sizeof *a;
    a->version = RX_ACTIVATION_API_VERSION;
    if (rx_session_activation(s, a) != 0) memset(a, 0, sizeof *a);
}

/* a session of the challenges Free Ride (activation "" ) or of a challenge started from it */
static rx_session* start_session(const char* install, const char* activation, int32_t track) {
    rx_session* s = rx_session_create(install);
    if (!s) { fprintf(stderr, "compass_native_test: create: %s\n", rx_session_last_error(NULL)); return NULL; }
    if (rx_session_event_in_career_mode(s, 1) != 0) { fail("in_career_mode", s); return NULL; }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "FR_Alpine");
    e.location_id = 45;
    e.track_id = track;
    e.duration = 0;
    e.duration_type[0] = 0;
    e.riders = 1;
    e.ai_difficulty = 0;
    e.vehicle_class = -1;
    snprintf(e.event_activation, sizeof e.event_activation, "%s", activation);
    if (rx_session_event_load(s, &e) != 0) { fail("event load", s); return NULL; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) {
        if (rx_session_advance(s) != 0) break;
        frames++;
    }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { fail("load frames", s); return NULL; }
    printf("compass_native_test: \"%s\" track %d loaded in %d frames\n", activation, track, frames);
    return s;
}

/* frames until the local activity is InProgress (A + 0x21c == 2) */
static int run_to_in_progress(rx_session* s, int max) {
    for (int i = 0; i < max; i++) {
        const rx_addr A = local_activity();
        if (A && U32(A, 0x21c) == 2) return i;
        if (rx_session_advance(s) != 0) return -1;
    }
    return -1;
}



static const char* kType[] = { "?", "AREA", "TARGET", "FINISH" };
static int read_compass(rx_session* s, rx_compass_state* c) {
    memset(c, 0, sizeof *c);
    c->size = sizeof *c; c->version = RX_COMPASS_API_VERSION;
    return rx_session_compass(s, c);
}
static void print_compass(const char* what, const rx_compass_state* c) {
    printf("compass_native_test: %s: valid %u challenge_kind %d discovery %u marks %u\n", what, c->valid, c->challenge_kind, c->discovery_mode, c->count);
    for (uint32_t i = 0; i < c->count; i++) {
        const rx_compass_mark* m = &c->marks[i];
        printf("compass_native_test:   %s index %d aux %d at %.1f %.1f %.1f\n", m->type >= 1 && m->type <= 3 ? kType[m->type] : "?", m->index, m->aux, m->pos[0], m->pos[1], m->pos[2]);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: compass_native_test <install root> [frames]\n"); return 2; }
    const char* install = argv[1];
    const int frames = argc > 2 ? atoi(argv[2]) : 900;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1800);
    setvbuf(stdout, NULL, _IONBF, 0);
    int failed = 0;
    g_step = "free ride";
    rx_session* s = start_session(install, "", 0);
    if (!s) return 1;
    if (run_to_in_progress(s, 20000) < 0) return fail("free ride never InProgress", s);
    rx_compass_state c;
    if (read_compass(s, &c) != 0) return fail("rx_session_compass", s);
    print_compass("Free Ride", &c);
    int areas = 0, types[16] = {0};
    for (uint32_t i = 0; i < c.count; i++) if (c.marks[i].type == RX_COMPASS_AREA) { areas++; types[c.marks[i].index & 15]++; }
    if (!c.valid || c.challenge_kind != -1 || !c.discovery_mode || areas == 0) { fprintf(stderr, "compass_native_test: FAIL the Free Ride has no area marks\n"); failed = 1; }
    rx_session_destroy(s);
    static const struct { const char* activity; int32_t track; } kChallenges[] = { { "HillClimb", 0 }, { "Trailblazer", 0 }, { "TargetPractice", 0 }, { "BigAir", 0 }, { "Pathfinder", 0 }, { "StepUp", 0 } };
    for (unsigned k = 0; k < sizeof kChallenges / sizeof *kChallenges; k++) {
        if (!types[(k == 0 ? 1 : k == 1 ? 6 : k == 2 ? 8 : k == 3 ? 0 : k == 4 ? 3 : 4)]) { printf("compass_native_test: no %s area in this level\n", kChallenges[k].activity); continue; }
        g_step = kChallenges[k].activity;
        s = start_session(install, kChallenges[k].activity, kChallenges[k].track);
        if (!s) return 1;
        if (run_to_in_progress(s, 20000) < 0) return fail("the challenge never InProgress", s);
        for (int i = 0; i < frames; i++) if (rx_session_advance(s) != 0) return fail("advance", s);
        if (read_compass(s, &c) != 0) return fail("rx_session_compass", s);
        print_compass(kChallenges[k].activity, &c);
        if (!c.valid) { fprintf(stderr, "compass_native_test: FAIL %s: not valid\n", kChallenges[k].activity); failed = 1; }
        if (!strcmp(kChallenges[k].activity, "TargetPractice") && (c.challenge_kind != 8 || c.count == 0)) { fprintf(stderr, "compass_native_test: FAIL TargetPractice has no targets\n"); failed = 1; }
        if (!strcmp(kChallenges[k].activity, "Trailblazer") && (c.challenge_kind != 6 || c.count != 1 || c.marks[0].type != RX_COMPASS_FINISH)) { fprintf(stderr, "compass_native_test: FAIL Trailblazer has no finish\n"); failed = 1; }
        rx_session_destroy(s);
    }
    printf("compass_native_test: %s\n", failed ? "FAIL" : "PASS");
    return failed;
}
