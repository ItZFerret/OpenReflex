/* stop_audit_twice.c -- two events one after the other in ONE process (the Unity restart / career "Continue" shape the session API
 * allows: one event per session, a new session after rx_session_destroy). For each event: create, load, run to RACING + <frames>,
 * then (development hooks) the live activity's teardown (vf3, vf6(0), vf0(1): rxtrace_teardown) and rx_session_destroy. A stop in the
 * teardown, in the destroy or in the next build shows as a missing ENDED line (the product aborts) or an error line.
 *   stop_audit_twice <install root> <frames after load> "<event A>" "<event B>" [teardown 0|1]
 *   event = "<type> <typeId> <level> <locationId> <trackId> <vehicleClass> <duration> <durationType|none> <riders> <ai> <activation|->"
 * Prints: EVENT <n> <type> state=<s> frames=<n> TEARDOWN=<r>, ENDED <n>, DONE.
 * Links against the development librxcore (stop_audit_trace.sh) or the "strict" build: clang -std=c11 -I core/include stop_audit_twice.c ... */
#include "rx_api.h"
#include <dlfcn.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int run_event(const char* root, const char* desc, int n, int frames_after, int teardown) {
    char type[64], level[64], dtype[32], act[64];
    int tid, lid, trk, cls, dur, riders, ai;
    if (sscanf(desc, "%63s %d %63s %d %d %d %d %31s %d %d %63s", type, &tid, level, &lid, &trk, &cls, &dur, dtype, &riders, &ai, act) != 11) {
        printf("EVENT %d bad description\n", n); return 1;
    }
    rx_session* s = rx_session_create(root);
    if (!s) { printf("EVENT %d %s create failed: %s\n", n, type, rx_session_last_error(NULL)); return 1; }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", type);
    e.event_type_id = tid;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = lid; e.track_id = trk; e.vehicle_class = cls; e.duration = dur;
    snprintf(e.duration_type, sizeof e.duration_type, "%s", strcmp(dtype, "none") == 0 ? "" : dtype);
    e.riders = riders; e.ai_difficulty = ai;
    e.event_activation[0] = 0;
    if (strcmp(act, "-") != 0) snprintf(e.event_activation, sizeof e.event_activation, "%s", act);
    if (rx_session_event_load(s, &e) != 0) { printf("EVENT %d %s load failed: %s\n", n, type, rx_session_last_error(s)); rx_session_destroy(s); return 1; }
    uint8_t keys[256]; memset(keys, 0, sizeof keys); keys[17] = 0x80;   /* W: throttle */
    int frames = 0, racing_at = -1;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 3000) { if (rx_session_advance(s) != 0) break; frames++; }
    rx_session_set_input(s, keys, NULL, 0);
    for (int i = 0; i < 6000 && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
        if (rx_session_advance(s) != 0) break;
        frames++;
        if (rx_session_get_state(s) == RX_SESSION_RACING && racing_at < 0) racing_at = frames;
        if (racing_at >= 0 && frames >= racing_at + frames_after) break;
    }
    int td = -99;
    if (teardown) {
        int (*hook)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_teardown");
        if (hook) td = hook();
    }
    printf("EVENT %d %s state=%d frames=%d racing_at=%d TEARDOWN=%d\n", n, type, (int)rx_session_get_state(s), frames, racing_at, td);
    const int st = (int)rx_session_get_state(s);
    rx_session_destroy(s);
    printf("ENDED %d\n", n);
    return st == RX_SESSION_ERROR;
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: see the file header\n"); return 2; }
    alarm(900);
    setvbuf(stdout, NULL, _IONBF, 0);
    const int frames = atoi(argv[2]), teardown = argc > 5 ? atoi(argv[5]) : 1;
    int bad = run_event(argv[1], argv[3], 1, frames, teardown);
    bad |= run_event(argv[1], argv[4], 2, frames, teardown);
    printf("DONE bad=%d\n", bad);
    return bad;
}
