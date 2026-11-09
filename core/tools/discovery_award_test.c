/* discovery_award_test.c -- the career Free Ride's discoveries found one after another, run by the product library (no emulator).
 *   discovery_award_test <install root> [level (FR_Cheyenne)] [location id (46)]
 *   build: core/build.sh <dir>, then clang -std=c11 -O1 -Iinclude -Isrc -Igame -o OUT tools/discovery_award_test.c DIR/obj/(all .o) -lz -lc++
 *
 * Playtester report 2026-11-08 (FR_Cheyenne, "the challenges Free Ride"): the game stopped just before the third discovery with
 * "rx_session_advance: unported game function 00582430 reached". The discovery check (00585330, race_activity_misc.c) awards a medal
 * when the found count reaches a third / two thirds / all of the level's discoveries (0054e670, kind 7); the award raises
 * "ChallengeAwardAchieved", whose Free Ride audio handler (the vcall thunk 00592bb0 -> FreeRideActivityAudioLogic::vf38 00582430,
 * race_activity_audio.c) was not ported.
 *
 * 1. The Free Ride of the level with InCareerMode (rx_session_event_in_career_mode: discovery mode, A + 0xff180 = 1), run to InProgress.
 * 2. Each discovery collectable not yet found (the tracker A + 0xff080: +0 the count, +4 the entities, +0x84 the number found) is
 *    moved onto the bike (its world matrix translation +0x60, which Entity::vf17 reads); the game's own per-frame check finds it,
 *    awards the medal of the new count and raises the events. 120 frames are run after each one.
 * Exit 0 = every discovery found and the session healthy after the last; a stop prints the step and the session error. */
#include "rx_api.h"
#include "../src/rx_mem.h"
#include "../game/rx_activation_api.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static char g_step[128] = "start";
static void on_signal(int sig) {
    char buf[240];
    const int n = snprintf(buf, sizeof buf, "discovery_award_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}

static int fail(const char* what, rx_session* s) {
    fprintf(stderr, "discovery_award_test: FAIL %s (during: %s)%s%s\n", what, g_step, s ? ": " : "", s ? rx_session_last_error(s) : "");
    return 1;
}

/* the local player and its activity (the game's player slots, as the API's local_player) */
static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return 0;
    const uint32_t n = U8(g, 0x1541c);
    for (uint32_t i = 0; i < n && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) == 0) return p;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: discovery_award_test <install root> [level] [location id]\n"); return 2; }
    const char* install = argv[1];
    const char* level = argc > 2 ? argv[2] : "FR_Cheyenne";
    const int location = argc > 3 ? atoi(argv[3]) : 46;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1200);
    setvbuf(stdout, NULL, _IONBF, 0);

    snprintf(g_step, sizeof g_step, "load %s", level);
    rx_session* s = rx_session_create(install);
    if (!s) { fprintf(stderr, "discovery_award_test: create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (rx_session_event_in_career_mode(s, 1) != 0) return fail("in_career_mode", s);
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = location;
    e.track_id = 0;
    e.duration = 0;
    e.duration_type[0] = 0;
    e.riders = 1;
    e.ai_difficulty = 0;
    e.vehicle_class = -1;
    if (rx_session_event_load(s, &e) != 0) return fail("event load", s);
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) {
        if (rx_session_advance(s) != 0) break;
        frames++;
    }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) return fail("load frames", s);

    snprintf(g_step, sizeof g_step, "run to InProgress");
    rx_addr P = 0, A = 0;
    for (int i = 0; i < 20000; i++) {
        P = local_player();
        A = P ? U32(P, 0x94) : 0;
        if (A && U32(A, 0x21c) == 2) break;
        if (rx_session_advance(s) != 0) return fail("advance", s);
        A = 0;
    }
    if (!A) return fail("the Free Ride never InProgress", s);
    const rx_addr tr = A + 0xff080;
    const int32_t n = I32(tr, 0);
    printf("discovery_award_test: %s loaded in %d frames; activity %08x vtable %08x, discovery mode %u, discoveries %d, found %u\n",
           level, frames, A, U32(A, 0), U32(A, 0xff180), n, U32(tr, 0x84));
    if (U32(A, 0) != 0x0094329cu || U32(A, 0xff180) != 1) return fail("not the Free Ride in discovery mode", NULL);
    if (n <= 0) return fail("the level has no discoveries", NULL);

    int found_all = 1;
    for (int32_t k = 0; k < n; k++) {
        const rx_addr d = U32(tr + 4, 4u * (uint32_t)k);
        if (U32(d, 0x140) != 0) continue;
        rx_bike_state b;
        if (rx_session_bike(s, &b) != 0) return fail("bike", s);
        const uint32_t before = U32(tr, 0x84);
        snprintf(g_step, sizeof g_step, "discovery %d (the %u. found of %d)", k, before + 1, n);
        SF32(d, 0x60, b.position[0][0]); SF32(d, 0x64, b.position[0][1]); SF32(d, 0x68, b.position[0][2]);
        int got = -1;
        for (int i = 0; i < 120; i++) {
            if (rx_session_advance(s) != 0) return fail("advance", s);
            if (rx_session_get_state(s) == RX_SESSION_ERROR) return fail("session error", s);
            if (got < 0 && U32(tr, 0x84) != before) got = i + 1;
        }
        if (got < 0) { printf("discovery_award_test: discovery %d not found after 120 frames\n", k); found_all = 0; continue; }
        printf("discovery_award_test: discovery %d found after %d frames: %u of %d; the session healthy 120 frames on\n", k, got, U32(tr, 0x84), n);
    }
    snprintf(g_step, sizeof g_step, "after the discoveries");
    for (int i = 0; i < 300; i++) if (rx_session_advance(s) != 0) return fail("advance", s);
    if (!found_all || I32(tr, 0x84) != n) return fail("not every discovery found", NULL);
    printf("discovery_award_test: PASS %s: %d of %d discoveries found (medals at %d, %d, %d), 300 frames after the last\n",
           level, I32(tr, 0x84), n, n / 3, 2 * (n / 3), n);
    rx_session_destroy(s);
    return 0;
}
