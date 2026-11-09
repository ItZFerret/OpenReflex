/* reflex_hud_test.c -- the wreck-recovery arrows' export (rx_session_reflex_hud, include/rx_reflex_hud.h) on a real ride. No emulator, no Unity.
 *
 *   reflex_hud_test <install root> <scenario> [direction]
 *       scenario  success: a wreck is forced and the stick is flicked in the direction it asks (the recovery succeeds);
 *                 fail:    the same wreck, no flick (the clock runs out, "WreckRecoveryFailed");
 *                 wrong:   the flick goes the wrong way (still a fall);
 *                 minor:   the lightest wreck class (kinds 1, 4, 7, 10: Player_UpdateWreckRecovery's "cancel" - the game recovers by itself when the
 *                          window ends, raising "WreckRecoverySuccess"; the component shows nothing);
 *                 natural: no forcing - rides on at full throttle with hard steering until the game wrecks the rider by itself (up to 6000 frames).
 *       direction up | down | left | right: the wreck type forced (WreckChecker pending type +0x1c: 5 / 6 / 8 / 7 -> the class table 00637ee0 /
 *                 00637ff0 gives the kinds 3 / 6 / 9 / 12), default up.
 *   Rides the National reference event (rx_session_event_default) with the autopilot-free straight line (W held), forces the wreck once the
 *   bike is above 25 mph and logs the exported state every frame it changes ("REFLEX ..."). The forced wreck is the only thing the test
 *   writes into the game's memory (the checker's pending wreck type and direction, the game's own debug hook of the stuck-rider bail), the
 *   input goes through rx_session_set_input (the right stick / the arrow keys, as the game's presets read them).
 *   Exit 0 = the expected sequence was seen (success: shown, lit arrow = the direction, success_events 1, hidden after; fail: shown, a
 *   fail event, hidden after; minor: never shown), 1 otherwise.
 *
 *   build: core/build.sh, then
 *     clang -std=c11 -O1 -Iinclude -Isrc -Igame -Iapi -o build/reflex_hud_test tools/reflex_hud_test.c build/obj/<all objects> -lz -lc++
 */
#include "rx_api.h"
#include "rx_reflex_hud.h"
#include "rx_mem.h"

#include <execinfo.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define GAME_GLOBAL 0x00a946b8u

static void on_signal(int sig) {
    char text[120];
    const int n = snprintf(text, sizeof text, "reflex_hud_test: signal %d\n", sig);
    if (n > 0) (void)write(2, text, (size_t)n);
    void* frames[40];
    const int count = backtrace(frames, 40);
    backtrace_symbols_fd(frames, count, 2);
    _exit(128 + sig);
}

static rx_addr local_player(void) {
    const rx_addr g = U32(0, GAME_GLOBAL);
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541c);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) == 0) return p;
    }
    return 0;
}

static const char* dir_name(int d) { static const char* n[] = {"none", "UP", "RIGHT", "DOWN", "LEFT"}; return d >= 0 && d < 5 ? n[d] : "?"; }

static void keys_none(uint8_t* keys) { memset(keys, 0, 256); }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: reflex_hud_test <install root> <success|fail|wrong|minor|natural> [up|down|left|right]\n"); return 2; }
    const char* scenario = argv[2];
    const char* dirname = argc > 3 ? argv[3] : "up";
    uint32_t type = 5; int want = RX_REFLEX_DIR_UP;
    if (!strcmp(dirname, "down")) { type = 6; want = RX_REFLEX_DIR_DOWN; }
    else if (!strcmp(dirname, "left")) { type = 8; want = RX_REFLEX_DIR_LEFT; }
    else if (!strcmp(dirname, "right")) { type = 7; want = RX_REFLEX_DIR_RIGHT; }
    const int natural = !strcmp(scenario, "natural"), minor = !strcmp(scenario, "minor");
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1800);
    setvbuf(stdout, NULL, _IONBF, 0);
    printf("reflex_hud_test: sizeof rx_reflex_hud_state %d (sizeof query %d)\n", (int)sizeof(rx_reflex_hud_state), (int)rx_session_sizeof("rx_reflex_hud_state"));

    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("reflex_hud_test: FAIL session create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (getenv("RX_API_TEST_EVENT_TYPE")) snprintf(spec.event_type, sizeof spec.event_type, "%s", getenv("RX_API_TEST_EVENT_TYPE"));
    if (rx_session_event_load(s, &spec) != 0) { printf("reflex_hud_test: FAIL event load: %s\n", rx_session_last_error(s)); return 1; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) { if (rx_session_advance(s) != 0) break; frames++; }
    printf("reflex_hud_test: %d load frames, state %d\n", frames, (int)rx_session_get_state(s));
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("reflex_hud_test: FAIL %s\n", rx_session_last_error(s)); return 1; }

    uint8_t keys[256]; rx_pad pads[4];
    memset(pads, 0, sizeof pads); keys_none(keys);
    rx_reflex_hud_state last; memset(&last, 0, sizeof last);
    int forced_at = -1, shown_at = -1, hidden_at = -1, ever_shown = 0, end_frame = 0, fails0 = 0, succ0 = 0, flicked = 0;
    float max_mph = 0; uint32_t fail_seen = 0, success_seen = 0, lit_at_show = 0; int dir_at_show = 0;
    const int max_frames = natural ? 6000 : 4000;
    for (int f = 0; f < max_frames; f++) {
        rx_race_state race; memset(&race, 0, sizeof race); (void)rx_session_race(s, &race);
        rx_bike_state bike; memset(&bike, 0, sizeof bike); (void)rx_session_bike(s, &bike);
        const float mph = bike.speed * 0.68182f;
        if (mph > max_mph) max_mph = mph;
        /* the ride: W (DIK 0x11) held; natural: swerve hard from 30 mph on */
        keys_none(keys); keys[0x11] = 0x80;
        pads[0].right_trigger = 255;
        if (natural && mph > 30.0f) { pads[0].lx = ((f / 40) & 1) ? 32000 : -32000; }
        /* after the forced wreck: the flick */
        rx_reflex_hud_state h; memset(&h, 0, sizeof h); h.size = sizeof h; h.version = RX_REFLEX_HUD_VERSION;
        if (rx_session_reflex_hud(s, &h) != 0) { printf("reflex_hud_test: FAIL query\n"); return 1; }
        if (h.valid && h.time_left > 0.0f && forced_at >= 0 && !flicked && (!strcmp(scenario, "success") || !strcmp(scenario, "wrong"))) {
            int d = want;
            if (!strcmp(scenario, "wrong")) d = want == RX_REFLEX_DIR_UP ? RX_REFLEX_DIR_DOWN : RX_REFLEX_DIR_UP;
            /* a short flick, the right stick (the game's default preset reads it as RiderReflexStick; the compound keys / mouse give the same) */
            const int16_t big = 30000;
            if (d == RX_REFLEX_DIR_UP) pads[0].ry = big; else if (d == RX_REFLEX_DIR_DOWN) pads[0].ry = -big;
            else if (d == RX_REFLEX_DIR_LEFT) pads[0].rx = -big; else pads[0].rx = big;
            if (h.shown) flicked = 1;
        }
        rx_session_set_input(s, keys, pads, 1);
        if (!natural && forced_at < 0 && mph > 25.0f && race.state == 2) {
            const rx_addr p = local_player();
            const rx_addr anim = p ? U32(p, 0x1b4) : 0;
            if (anim) {
                const rx_addr checker = anim + 0x5b54c;
                SU32(checker, 0x1c, type);            /* a pending wreck of this type ... */
                SU32(checker, 0x20, minor ? 0u : 2u);   /* ... direction 0: the impact th[0x10] + 0.001 (kind - 2: the cancel kinds), 2: th[0x14] - 0.001 (the kind itself) */
                forced_at = f;
                printf("reflex_hud_test: frame %d (race frame %d): wreck type %u forced at %.1f mph\n", f, race.state, type, mph);
            }
        }
        if (rx_session_advance(s) != 0) { printf("reflex_hud_test: advance failed: %s\n", rx_session_last_error(s)); break; }
        memset(&h, 0, sizeof h); h.size = sizeof h; h.version = RX_REFLEX_HUD_VERSION;
        (void)rx_session_reflex_hud(s, &h);
        if (h.valid && (h.shown != last.shown || h.lit_mask != last.lit_mask || h.mode != last.mode || h.begin_events != last.begin_events ||
                        h.success_events != last.success_events || h.fail_events != last.fail_events || h.fail_any_events != last.fail_any_events ||
                        h.reset_events != last.reset_events || h.last_result != last.last_result)) {
            printf("REFLEX f%d: shown %u lit %x dir %s mode %u class %u type %u vehicle %u source %u(hand %u motion %u) time %.3f/%.3f progress %.3f/%.2f stick %.2f %.2f | begin %u success %u fail %u (any %u) reset %u last %u | recovered %u falls %u\n",
                   f, h.shown, h.lit_mask, dir_name(h.direction), h.mode, h.wreck_class, h.wreck_type, h.vehicle_kind, h.input_source, h.handedness, h.motion,
                   h.time_left, h.time_max, h.progress, h.threshold, h.stick_x, h.stick_y, h.begin_events, h.success_events, h.fail_events, h.fail_any_events,
                   h.reset_events, h.last_result, h.recovered, h.falls);
            if (h.shown && !last.shown && shown_at < 0) { shown_at = f; lit_at_show = h.lit_mask; dir_at_show = h.direction; }
            if (!h.shown && last.shown) hidden_at = f;
        }
        if (h.shown) ever_shown = 1;
        if (h.valid && h.time_left > 0 && forced_at >= 0 && (f - forced_at) % 6 == 0 && (f - forced_at) < 120)
            printf("  f%d time %.3f/%.3f progress %.3f stick %.2f %.2f mode %u shown %u\n", f, h.time_left, h.time_max, h.progress, h.stick_x, h.stick_y, h.mode, h.shown);
        if (h.valid) { fail_seen = h.fail_events; success_seen = h.success_events; }
        last = h;
        end_frame = f;
        if (forced_at >= 0 && f > forced_at + 150) break;
        if (natural && ever_shown && !h.shown && h.mode == 0 && (h.success_events || h.fail_events)) break;
    }
    (void)fails0; (void)succ0;
    printf("reflex_hud_test: ended at frame %d, max %.1f mph, forced at %d, shown at %d, hidden at %d, lit at show %x dir %s, success %u fail %u\n",
           end_frame, max_mph, forced_at, shown_at, hidden_at, lit_at_show, dir_name(dir_at_show), success_seen, fail_seen);
    int ok = 1;
    if (!natural && forced_at < 0) { printf("reflex_hud_test: FAIL the wreck was never forced\n"); ok = 0; }
    if (minor) { if (ever_shown) { printf("reflex_hud_test: FAIL the arrows showed for a minor wreck\n"); ok = 0; }
                  if (success_seen < 1) { printf("reflex_hud_test: FAIL the minor wreck did not end in \"WreckRecoverySuccess\"\n"); ok = 0; } }
    else {
        if (!ever_shown) { printf("reflex_hud_test: FAIL the arrows never showed\n"); ok = 0; }
        if (!natural && dir_at_show != want) { printf("reflex_hud_test: FAIL direction %s, wanted %s\n", dir_name(dir_at_show), dir_name(want)); ok = 0; }
        if (!strcmp(scenario, "success") && success_seen < 1) { printf("reflex_hud_test: FAIL no success event\n"); ok = 0; }
        if ((!strcmp(scenario, "fail") || !strcmp(scenario, "wrong")) && fail_seen < 1) { printf("reflex_hud_test: FAIL no fail event\n"); ok = 0; }
        if (hidden_at < 0 && ever_shown) { printf("reflex_hud_test: FAIL the arrows never hid\n"); ok = 0; }
    }
    printf("reflex_hud_test: %s\n", ok ? "PASS" : "FAIL");
    rx_session_destroy(s);
    return ok ? 0 : 1;
}
