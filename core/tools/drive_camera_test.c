/* drive_camera_test.c -- the saved camera reaches the race: rx_session_ui_setting("DrivingCamera") written before the event loads (as the
 * client's OpenReflexSimulation applies its GameUiSettings) is the camera the local player rides with once the race runs (the players the
 * event makes apply the setting: Player_ApplyDrivingCamera 0054aef0), not the guest default (VISUAL).
 *   drive_camera_test <install root> [preset 0..3, default 1 = GAME PLAY; -1: write nothing, the old client]
 *   build: core/tools/drive_camera_test.sh <out dir> (after core/build.sh)
 * Exit 0 = the camera at the race's frames is the preset written. */
#include "rx_api.h"
#include "rx_ui_settings.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int camera(rx_session* s) { rx_camera_bindings b; memset(&b, 0, sizeof b); return rx_session_camera_bindings(s, &b) == 0 ? b.driving_camera : -9; }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: drive_camera_test <install root> [preset]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    int want = argc > 2 ? atoi(argv[2]) : 1;
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (want >= 0) { int32_t before = -9; int rc = rx_session_ui_setting(s, "DrivingCamera", want, 1, &before); printf("DrivingCamera %d written before the load -> %d (was %d)\n", want, rc, before); }
    rx_event_spec spec; rx_session_event_default(&spec);
    printf("event %s / %s, riders %d\n", spec.event_type, spec.location, spec.riders);
    if (rx_session_event_load(s, &spec) != 0) { printf("event load: %s\n", rx_session_last_error(s)); return 1; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) { if (rx_session_advance(s) != 0) break; frames++; }
    printf("loaded after %d frames, camera %d\n", frames, camera(s));
    int racing = 0, wrong = 0, last = -2;
    for (int i = 0; i < 9000; i++) {
        if (rx_session_advance(s) != 0) { printf("advance %d failed: %s\n", i, rx_session_last_error(s)); return 1; }
        rx_race_state r; memset(&r, 0, sizeof r); rx_session_race(s, &r);
        int c = camera(s);
        if (c != last || i % 600 == 0) { printf("frame %4d: race state %d valid %d countdown %d camera %d\n", i, r.state, r.valid, r.countdown_step, c); last = c; }
        if (r.valid && r.state == 2) { racing++; if (want >= 0 && c != want) wrong++; if (racing >= 300) break; }
    }
    printf("racing frames %d, frames on another camera %d, camera now %d\n", racing, wrong, camera(s));
    rx_session_destroy(s);
    if (want < 0) return 0;
    if (racing == 0) { printf("FAIL the race never ran\n"); return 1; }
    if (wrong) { printf("FAIL the race rode another camera than DrivingCamera %d\n", want); return 1; }
    printf("PASS the race rides DrivingCamera %d\n", want);
    return 0;
}
