/* tutorial_test.c -- the MotoSkills tutorials through the session API (include/rx_tutorial.h): create, rx_session_tutorial_start, the frames
 * until the local slot spawned in the UI world, then lessons 1 (RIDER REFLEX: the lean in the four directions) and 2 (HOLD TIGHT: throttle,
 * the lean back from 1 s on, to the finish) as the host's pages run them (the pause menu: pause + disable; accept: unpause + the lesson; the
 * PreTutorialPopup up, then down; the Post / Failed popup up; SuspendPlayerInput where those pages set it), rx_session_tutorial_end, destroy.
 *   tutorial_test <install root> [lessons (2)]
 * Build (against the product library): clang -std=c11 -O2 -Icore/include core/tools/tutorial_test.c -L<dir> -lrxcore -o <out>
 * Exit 0 when the lessons passed (PostTutorialPopup, PlayerRider.TutorialsCompleted raised) and the end ran without an error. */
#include "rx_tutorial.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(rx_session* s, const char* what) {
    fprintf(stderr, "tutorial_test: %s: %s\n", what, s ? rx_session_last_error(s) : "");
    if (s) rx_session_destroy(s);
    return 1;
}
static rx_tutorial_state state(rx_session* s) {
    rx_tutorial_state t; memset(&t, 0, sizeof t); t.size = sizeof t; t.version = RX_TUTORIAL_API_VERSION;
    rx_session_tutorial(s, &t);
    return t;
}
static int last_event(const rx_tutorial_state* t, uint32_t* seen) {
    int ev = 0;
    while (*seen < t->event_serial) { ev = t->events[*seen % RX_TUTORIAL_EVENTS]; (*seen)++; }
    return ev;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: tutorial_test <install root> [lessons]\n"); return 2; }
    const int lessons = argc > 2 ? atoi(argv[2]) : 2;
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) return fail(NULL, "create");
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (rx_session_tutorial_start(s, &spec) != 0) return fail(s, "tutorial start");
    rx_tutorial_state t;
    int f = 0;
    for (; f < 3000; f++) {
        if (rx_session_advance(s) != 0) return fail(s, "advance (spawn)");
        t = state(s);
        if (t.spawned) break;
    }
    printf("tutorial_test: spawned after %d frames: mode %d manager off %d valid %d completed %d\n", f + 1, t.mode, t.manager_off, t.valid, t.completed);
    if (!t.spawned) return fail(s, "no spawn");
    uint32_t seen = t.event_serial;
    int passed = 0;
    for (int L = 0; L < lessons; L++) {
        /* the pause menu (TutorialPauseMenu:Start: DisableTutorial, SuspendPlayerInput TRUE, PauseActiveWorld( TRUE ) / :OnAccept:
         * SuspendPlayerInput FALSE, EnableTutorial( n ), then PreTutorialPopup:Start (SuspendPlayerInput TRUE) and the menu's End) */
        rx_session_tutorial_disable(s);
        if (rx_session_tutorial_suspend_input(s, 1) != 0) return fail(s, "suspend input");
        rx_session_tutorial_pause(s, 1);
        for (int i = 0; i < 10; i++) rx_session_advance(s);
        rx_session_tutorial_suspend_input(s, 0);
        if (rx_session_tutorial_lesson(s, L) != 0) return fail(s, "lesson");
        rx_session_tutorial_popup(s, 1);          /* PreTutorialPopup comes up in the lesson's start (ShowPreTutorialPopup) */
        rx_session_tutorial_suspend_input(s, 1);
        rx_session_tutorial_pause(s, 0);
        t = state(s);
        int ev = last_event(&t, &seen);
        printf("lesson %d: event %d lesson %d state %d current %d description '%s' activation '%s' label '%s' voice '%s'\n", L + 1, ev, t.lesson,
               t.lesson_state, t.current_tutorial, t.description, t.activation, t.hud_label, t.voice);
        if (ev != RX_TUTORIAL_EVENT_PRE_POPUP) { rx_session_tutorial_popup(s, 0); rx_session_tutorial_suspend_input(s, 0); }
        for (int i = 0; i < 30; i++) rx_session_advance(s);
        rx_session_tutorial_popup(s, 0);          /* CONTINUE: the HUD (PreTutorialPopup:End: SuspendPlayerInput FALSE) */
        rx_session_tutorial_suspend_input(s, 0);
        {   /* the lesson's objects (ShowLessonObjects: once the player's reset is over) */
            rx_tutorial_object obj[64];
            const int n = rx_session_tutorial_objects(s, obj, 64);
            int shown = 0;
            printf("lesson %d objects (%d):", L + 1, n);
            for (int i = 0; i < n; i++) if (obj[i].shown) { printf(" %s", obj[i].name); shown++; }
            printf(" [%d shown]\n", shown);
        }
        if (L == 0) {   /* SuspendPlayerInput holds the rider: the lean forward held 30 frames counts nothing while it is set */
            rx_session_tutorial_suspend_input(s, 1);
            for (int i = 0; i < 30; i++) {
                rx_pad pads[4]; memset(pads, 0, sizeof pads);
                uint8_t keys[256]; memset(keys, 0, sizeof keys); keys[0xc8] = 0x80;
                rx_session_set_input(s, keys, pads, 1);
                if (rx_session_advance(s) != 0) return fail(s, "advance (suspended)");
            }
            t = state(s);
            printf("lesson 1: 30 frames leaning forward with the input suspended: lean flags %x\n", t.lean_flags);
            if (t.lean_flags & 1) { printf("tutorial_test: FAIL (the suspended input moved the rider)\n"); rx_session_destroy(s); return 1; }
            rx_session_tutorial_suspend_input(s, 0);
        }
        int outcome = 0;
        for (int i = 0; i < 1500 && !outcome; i++) {
            rx_pad pads[4]; memset(pads, 0, sizeof pads);
            uint8_t keys[256]; memset(keys, 0, sizeof keys);
            if (L == 0) { static const uint8_t seq[4] = {0xc8, 0xd0, 0xcb, 0xcd}; if (i < 240) keys[seq[i / 60]] = 0x80; }
            else { keys[0x11] = 0x80; if (i >= 60) keys[0xd0] = 0x80; }
            rx_session_set_input(s, keys, pads, 1);
            if (rx_session_advance(s) != 0) return fail(s, "advance (lesson)");
            t = state(s);
            ev = last_event(&t, &seen);
            if (ev == RX_TUTORIAL_EVENT_POST_POPUP || ev == RX_TUTORIAL_EVENT_FAILED_POPUP) outcome = ev;
            if (i % 120 == 0) printf("  frame %4d: state %d step %d hold %.2f lean flags %x wreck %d boundary %d\n", i, t.lesson_state, t.step, t.hold_time, t.lean_flags,
                                     t.wrecked, t.boundary);
        }
        printf("lesson %d: %s after the ride: completed %d completion '%s' failed '%s' voice '%s'\n", L + 1,
               outcome == RX_TUTORIAL_EVENT_POST_POPUP ? "PASSED" : outcome ? "FAILED" : "no outcome", t.completed, t.completion, t.failed, t.voice);
        if (outcome == RX_TUTORIAL_EVENT_POST_POPUP) passed++;
        rx_session_tutorial_popup(s, 1);          /* the Post / Failed popup: Start SuspendPlayerInput TRUE, End FALSE */
        rx_session_tutorial_suspend_input(s, 1);
        for (int i = 0; i < 30; i++) rx_session_advance(s);
        rx_session_tutorial_popup(s, 0);
        rx_session_tutorial_suspend_input(s, 0);
    }
    if (rx_session_tutorial_end(s) != 0) return fail(s, "tutorial end");
    for (int i = 0; i < 30; i++) if (rx_session_advance(s) != 0) return fail(s, "advance (after the end)");
    t = state(s);
    printf("tutorial_test: ended: active %d mode %d completed %d\n", t.active, t.mode, t.completed);
    rx_session_destroy(s);
    if (passed != lessons) { printf("tutorial_test: FAIL (%d of %d lessons passed)\n", passed, lessons); return 1; }
    printf("tutorial_test: PASS\n");
    return 0;
}
