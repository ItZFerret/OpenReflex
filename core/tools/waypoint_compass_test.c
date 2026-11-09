/* waypoint_compass_test.c -- the Waypoint race's compass gates (rx_session_compass, game/rx_compass_api.h: RX_COMPASS_GATE, waypoint_gate).
 *   waypoint_compass_test <install root> [level ...]   (default FR_Alpine FR_Cheyenne FR_Gibson)
 * Loads each level's Waypoint race (track 1, the local rider alone), runs it to InProgress and checks what the original's
 * MXUICompassComponent (UI kind 6) would hold: one GATE mark per WaypointRaceGate of the race's group (at most 16 slots), each gate index
 * (+0x280) exactly once, the race's gate count (rx_session_waypoint) equal to the marks', the local driver's last gate -1 (before the
 * first gate) and not finished. Prints the slots (slot, gate index, position).
 * Exit 0 = every level passes.
 *   build: C=core O=<out>; source core/tools/product_link.sh; rx_product_objs; rx_link_driver <out>/waypoint_compass_test core/tools/waypoint_compass_test.c */
#include "rx_api.h"
#include "../game/rx_compass_api.h"
#include "../game/rx_waypoint_api.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* g_step = "start";
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "waypoint_compass_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    _exit(128 + sig);
}

static int level_id(const char* level) {
    if (!strcmp(level, "FR_Alpine")) return 45;
    if (!strcmp(level, "FR_Cheyenne")) return 46;
    if (!strcmp(level, "FR_Gibson")) return 47;
    return -1;
}

static int run_level(const char* install, const char* level) {
    printf("waypoint_compass_test: == %s\n", level);
    g_step = "create";
    rx_session* s = rx_session_create(install);
    if (!s) { fprintf(stderr, "waypoint_compass_test: create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "WaypointRace");
    e.event_type_id = 14;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = level_id(level);
    e.track_id = 1;
    e.duration = 1;
    e.riders = 1;
    g_step = "event load";
    if (rx_session_event_load(s, &e) != 0) { fprintf(stderr, "waypoint_compass_test: FAIL %s event load: %s\n", level, rx_session_last_error(s)); rx_session_destroy(s); return 1; }
    g_step = "frames";
    int frames = 0, in_progress = 0;
    rx_waypoint_state w;
    for (; frames < 30000; frames++) {
        if (rx_session_get_state(s) == RX_SESSION_ERROR || rx_session_advance(s) != 0) break;
        memset(&w, 0, sizeof w); w.size = sizeof w; w.version = RX_WAYPOINT_API_VERSION;
        rx_race_state r;
        if (rx_session_get_state(s) != RX_SESSION_LOADING && rx_session_race(s, &r) == 0 && r.state == 2) { in_progress = 1; break; }
    }
    if (!in_progress) { fprintf(stderr, "waypoint_compass_test: FAIL %s never InProgress (%d frames): %s\n", level, frames, rx_session_last_error(s)); rx_session_destroy(s); return 1; }
    for (int i = 0; i < 60; i++) if (rx_session_advance(s) != 0) break;
    rx_compass_state c;
    memset(&c, 0, sizeof c); c.size = sizeof c; c.version = RX_COMPASS_API_VERSION;
    memset(&w, 0, sizeof w); w.size = sizeof w; w.version = RX_WAYPOINT_API_VERSION;
    if (rx_session_compass(s, &c) != 0 || rx_session_waypoint(s, &w) != 0) { fprintf(stderr, "waypoint_compass_test: FAIL %s the readers refused\n", level); rx_session_destroy(s); return 1; }
    printf("waypoint_compass_test: %s InProgress after %d frames: compass valid %u challenge_kind %d marks %u waypoint_gate %d finished %u; race gate_count %d gate_index %d\n",
           level, frames, c.valid, c.challenge_kind, c.count, c.waypoint_gate, c.waypoint_finished, w.gate_count, w.gate_index);
    int gates = 0, seen[64] = {0}, bad = 0;
    for (uint32_t i = 0; i < c.count; i++) {
        const rx_compass_mark* m = &c.marks[i];
        if (m->type != RX_COMPASS_GATE) continue;
        printf("waypoint_compass_test:   WaypointGate%d = gate index %d at %.1f %.1f %.1f\n", m->index + 1, m->aux, m->pos[0], m->pos[1], m->pos[2]);
        if (m->index != gates) bad = 1;
        if (m->aux < 0 || m->aux >= 64 || seen[m->aux]++) bad = 1;
        gates++;
    }
    int expect = w.gate_count < 16 ? w.gate_count : 16;
    int ok = c.valid && c.challenge_kind == -1 && gates > 0 && gates == expect && !bad && c.waypoint_gate == w.gate_index && c.waypoint_gate == -1 && !c.waypoint_finished;
    printf("waypoint_compass_test: %s %s (%d gate marks, the race has %d)\n", ok ? "PASS" : "FAIL", level, gates, w.gate_count);
    rx_session_destroy(s);
    return ok ? 0 : 1;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: waypoint_compass_test <install root> [level ...]\n"); return 2; }
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1800);
    setvbuf(stdout, NULL, _IONBF, 0);
    static const char* deflt[] = { "FR_Alpine", "FR_Cheyenne", "FR_Gibson" };
    int failed = 0;
    if (argc > 2) for (int i = 2; i < argc; i++) failed |= run_level(argv[1], argv[i]);
    else for (unsigned i = 0; i < 3; i++) failed |= run_level(argv[1], deflt[i]);
    printf("waypoint_compass_test: %s\n", failed ? "FAIL" : "PASS");
    return failed;
}
