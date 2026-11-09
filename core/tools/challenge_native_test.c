/* challenge_native_test.c -- the end-of-challenge page's core state and the restart, run by the product library (no emulator).
 *   challenge_native_test <install root> [activity (HillClimb)] [track (0)] [rounds (2)]
 *   build: core/build.sh, then clang -std=c11 -O1 -Iinclude -Isrc -Igame -o <out> tools/challenge_native_test.c (every object in build/obj) -lz -lc++
 *
 * A challenge started the way the open front end starts it from a Free Ride activation area (EventType FreeRide, EventTypeActivation,
 * TrackId, InCareerMode), ridden with the throttle held until the activity reaches Post (state 4). Each round reads
 * rx_session_challenge (kind, state, medal flags, value, criteria, the profile's record after the award). Between rounds the popup's
 * RESTART: rx_session_challenge_restart (MXGame_RestartActivity), after which the same challenge must run again from the start
 * (state back to InProgress, the medal flags and the value cleared) and end in Post again. Exit 0 = every step as expected. */
#include "rx_api.h"
#include "../game/rx_challenge_api.h"
#include "../game/rx_activation_api.h"
#include "../src/rx_mem.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static const char* g_step = "start";
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "challenge_native_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}
static int fail(const char* what, rx_session* s) {
    fprintf(stderr, "challenge_native_test: FAIL %s%s%s\n", what, s ? ": " : "", s ? rx_session_last_error(s) : "");
    return 1;
}
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
static int read_state(rx_session* s, rx_challenge_state* c) {
    memset(c, 0, sizeof *c);
    c->size = sizeof *c;
    c->version = RX_CHALLENGE_API_VERSION;
    return rx_session_challenge(s, c);
}
static void print_state(const char* tag, const rx_challenge_state* c) {
    printf("challenge_native_test: %s: valid %u kind %d state %d area %d medal %d (gold %d silver %d bronze %d) value %.3f%s criteria %.2f %.2f %.2f "
           "record %s%.3f counts %d %d %d\n", tag, c->valid, c->kind, c->state, c->area, c->medal, c->gold, c->silver, c->bronze, c->value,
           c->value_valid ? "" : " (none)", c->criteria[0], c->criteria[1], c->criteria[2], c->record_valid ? "" : "none ", c->record,
           c->record_counts[0], c->record_counts[1], c->record_counts[2]);
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: challenge_native_test <install root> [activity] [track] [rounds]\n"); return 2; }
    const char* install = argv[1];
    const char* activity = argc > 2 ? argv[2] : "HillClimb";
    const int track = argc > 3 ? atoi(argv[3]) : 0;
    const int rounds = argc > 4 ? atoi(argv[4]) : 2;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1800);
    setvbuf(stdout, NULL, _IONBF, 0);

    g_step = "load";
    rx_session* s = rx_session_create(install);
    if (!s) return fail("create", NULL);
    if (rx_session_event_in_career_mode(s, 1) != 0) return fail("in_career_mode", s);
    /* HOLD_FRAMES=<n>: the PreChallengePopup's WaitForUI hold (rx_session_wait_for_ui) from before the load; the challenge must stay in Pre
     * (state 0) for n frames of the running world, then start once the hold is released */
    const int hold_frames = getenv("HOLD_FRAMES") ? atoi(getenv("HOLD_FRAMES")) : 0;
    if (hold_frames > 0 && rx_session_wait_for_ui(s, 1) != 0) return fail("wait_for_ui", s);
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
    snprintf(e.event_activation, sizeof e.event_activation, "%s", activity);
    if (rx_session_event_load(s, &e) != 0) return fail("event load", s);
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) { if (rx_session_advance(s) != 0) break; frames++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) return fail("load frames", s);
    printf("challenge_native_test: %s track %d loaded in %d frames\n", activity, track, frames);

    rx_challenge_state c;
    if (hold_frames > 0) {
        g_step = "held in Pre";
        int worst = 0;
        for (int i = 0; i < hold_frames; i++) {
            if (rx_session_advance(s) != 0) return fail("advance (held)", s);
            if (read_state(s, &c) != 0) return fail("rx_session_challenge", s);
            if (!c.valid) return fail("the local activity is not a challenge", NULL);
            if (c.state != 0) { worst = c.state; printf("challenge_native_test: held: state %d at frame %d\n", c.state, i + 1); break; }
        }
        print_state("held", &c);
        if (worst != 0) return fail("the challenge left Pre while WaitForUI was held", NULL);
        if (rx_session_wait_for_ui(s, 0) != 0) return fail("wait_for_ui release", s);
        printf("challenge_native_test: hold released after %d frames\n", hold_frames);
    }
    uint8_t keys[256];
    memset(keys, 0, sizeof keys);
    keys[0xc8] = 0x80;   /* DIK_UP: throttle (the default keyboard map) */
    for (int round = 1; round <= rounds; round++) {
        char step[64];
        snprintf(step, sizeof step, "round %d: to InProgress", round);
        g_step = step;
        int guard = 0;
        for (;;) {
            if (read_state(s, &c) != 0) return fail("rx_session_challenge", s);
            if (!c.valid) return fail("the local activity is not a challenge", NULL);
            if (c.state == 2) break;
            if (++guard > 20000) return fail("never InProgress", NULL);
            if (rx_session_advance(s) != 0) return fail("advance", s);
        }
        print_state("InProgress", &c);
        if (c.gold || c.silver || c.bronze || c.medal != 0) return fail("medal flags set at the start of a round", NULL);
        if (c.value > 1.0f) return fail("value not cleared at the start of a round", NULL);
        snprintf(step, sizeof step, "round %d: to Post", round);
        g_step = step;
        rx_session_set_input(s, keys, NULL, 0);
        int last_state = 2;
        for (int i = 0; i < 40000; i++) {
            /* test only: POKE_VALUE=<value> writes the class's value cell (HillClimb / Trailblazer +0xff3b0, the HillClimb keeps its best; Target Practice the hits +0xff434) so a
             * medal is won without riding the course; the medal flags, the award and the profile row are the game's */
            if (getenv("POKE_VALUE") && i == 120) { const rx_addr A = local_activity(); if (A && U32(A, 0xff070) == 8) SU32(A, 0xff434, (uint32_t)atoi(getenv("POKE_VALUE"))); else if (A) SF32(A, 0xff3b0, (float)atof(getenv("POKE_VALUE"))); }
            if (rx_session_advance(s) != 0) return fail("advance", s);
            read_state(s, &c);
            if (c.state != last_state) { printf("challenge_native_test: round %d frame %d state %d value %.3f\n", round, i + 1, c.state, c.value); last_state = c.state; }
            if (c.state == 4) break;
        }
        if (c.state != 4) return fail("never reached Post", NULL);
        for (int i = 0; i < 5; i++) rx_session_advance(s);
        read_state(s, &c);
        print_state("Post", &c);
        if (!c.record_valid && c.medal != 0) return fail("a medal was won but the profile has no stored value", NULL);
        if (c.medal != 0 && c.record_counts[c.medal - 1] < 1) return fail("a medal was won but its count is 0", NULL);
        for (int i = 0; i < 300; i++) if (rx_session_advance(s) != 0) return fail("Post advance", s);
        read_state(s, &c);
        print_state("Post +300", &c);
        if (round < rounds) {
            g_step = "restart";
            { const rx_addr sess = U32(0, 0x00da6cb0); printf("challenge_native_test: session %08x vtable %08x vf16 %08x\n", sess, sess ? U32(sess, 0) : 0, sess && U32(sess, 0) ? U32(U32(sess, 0), 0x40) : 0); }
            const int r = rx_session_challenge_restart(s);
            printf("challenge_native_test: rx_session_challenge_restart -> %d\n", r);
            if (r != 0) return fail("restart", s);
            read_state(s, &c);
            print_state("after restart", &c);
            if (rx_session_get_state(s) == RX_SESSION_ERROR) return fail("session error after restart", s);
        }
    }
    rx_session_destroy(s);
    printf("challenge_native_test: PASS\n");
    return 0;
}
