/* fp_camera_test.c -- test-only driver for the view selection (development; not part of the session API).
 *
 * Views are selected through the game's own path, never by writing P+0x988 directly:
 *   api     rx_session_set_camera(s, 2)  -> the game's Player_SetCamera 0054c9b0 (the Camera key's function)
 *   key     the shipped Camera key (C, DIK scan 0x2e) through rx_session_set_input -> Keyboard_Poll -> the "Camera"
 *           ControllerRemapping / KeyMap at input block +0xf34 -> Player_UpdateCamera 0054c150 -> Player_SetCamera
 *   default no selection at all (the game's default camera; the control run for the same inputs / frame count)
 * Plan (identical advances and gameplay inputs in every mode, only the selection differs):
 *   1. run to the racing state, 2. hold W until the start-gate camera leaves (it owns the view until the vehicle is 10 units
 *   away or input arrives), 3. four frames with W (the mode selects during them), 4. one frame back to the default view,
 *   5. run to the target frame count with no input and print a gameplay fingerprint (bike / race / rider pose hash) that the
 *   three runs must share.
 * The camera is printed through rx_session_camera: position, the right / up / forward basis (finite, unit length), fov / near /
 * far.
 *
 *   cycle   four Camera key presses (C, one frame each with a release frame between): the game's own cycle 1 -> 2 -> 4 -> 0 -> 1;
 *           after every press the selected entity and the camera (finite, unit basis, fov / near / far) are printed and checked.
 *
 * Build: see tools/fp_camera_test.sh <out dir> (links the product objects, same as rx_api_test).
 * Usage: fp_camera_test [install] [frames] [api|key|cycle|default] */
#include "rx_api.h"
#include "../src/rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define CAMERA_KEY_C 0x2eu   /* DIK_C -> game key id 0x25, the "Camera" ControllerRemapping column */
#define KEY_W        0x11u   /* DIK_W */

static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8u);
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541cu);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ecu + 4u * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460u + slot * 0x12a0u;
        if (U32(p, 0x10u) == 0) return p;
    }
    return 0;
}

static double vlen(const float* v) { return sqrt((double)v[0] * v[0] + (double)v[1] * v[1] + (double)v[2] * v[2]); }

static void print_camera(rx_session* s, const char* what) {
    rx_camera c;
    memset(&c, 0, sizeof c);
    const int rc = rx_session_camera(s, &c);
    printf("%s: camera rc %d pos %.9g %.9g %.9g fov %.9g near %.9g far %.9g | r %.6g %.6g %.6g (%.6f) u %.6g %.6g %.6g (%.6f) f %.6g %.6g %.6g (%.6f)\n",
           what, rc, (double)c.position[0], (double)c.position[1], (double)c.position[2], (double)c.fov,
           (double)c.near_plane, (double)c.far_plane,
           (double)c.right[0], (double)c.right[1], (double)c.right[2], vlen(c.right),
           (double)c.up[0], (double)c.up[1], (double)c.up[2], vlen(c.up),
           (double)c.forward[0], (double)c.forward[1], (double)c.forward[2], vlen(c.forward));
}

static void print_selection(rx_addr p, const char* what) {
    if (!p) { printf("%s: no local player\n", what); return; }
    printf("%s: P %08x 0x988 %u [9c5410] %u 0x97c %08x 0x9a8 %08x 0x99c %08x 0x9a0 %u\n", what, p, U32(p, 0x988),
           U32(0, 0x009c5410), U32(p, 0x97c), U32(p, 0x9a8), U32(p, 0x99c), U32(p, 0x9a0));
}

/* the mode's camera is usable: a finite position / basis / lens, a unit right / up / forward, 0 < fov < pi, 0 < near < far */
static int camera_check(rx_session* s, const char* what) {
    rx_camera c;
    memset(&c, 0, sizeof c);
    if (rx_session_camera(s, &c) != 0) { printf("camera check %s: BAD rc\n", what); return 0; }
    int ok = 1;
    for (int i = 0; i < 3; i++) {
        if (!isfinite(c.position[i]) || !isfinite(c.right[i]) || !isfinite(c.up[i]) || !isfinite(c.forward[i])) ok = 0;
    }
    const double lr = vlen(c.right), lu = vlen(c.up), lf = vlen(c.forward);
    if (!(fabs(lr - 1.0) < 1e-3) || !(fabs(lu - 1.0) < 1e-3) || !(fabs(lf - 1.0) < 1e-3)) ok = 0;
    if (!(c.fov > 0.0f && c.fov < 3.1415927f) || !(c.near_plane > 0.0f) || !(c.far_plane > c.near_plane)) ok = 0;
    printf("camera check %s: %s (near %.9g far %.9g order %s)\n", what, ok ? "ok" : "BAD",
           (double)c.near_plane, (double)c.far_plane, c.far_plane > c.near_plane ? "ok" : "BAD");
    return ok;
}

static uint32_t pose_hash(rx_session* s) {
    static float pose[69 * 16];
    const int n = rx_session_rider_pose(s, pose, 69);
    uint32_t h = 2166136261u;
    for (int i = 0; i < n * 16; i++) {
        uint32_t b;
        memcpy(&b, &pose[i], 4);
        h = (h ^ b) * 16777619u;
    }
    return h;
}

static uint32_t bike_pose_hash(rx_session* s) {
    static float pose[64 * 16];
    const int n = rx_session_bike_pose(s, pose, 64);
    uint32_t h = 2166136261u;
    for (int i = 0; i < n * 16; i++) {
        uint32_t b;
        memcpy(&b, &pose[i], 4);
        h = (h ^ b) * 16777619u;
    }
    return h;
}

typedef struct { float speed; int32_t gear, place, laps, state; double race_time; uint32_t pose, bike_pose; } fingerprint;

static fingerprint take_fingerprint(rx_session* s) {
    fingerprint f;
    memset(&f, 0, sizeof f);
    rx_bike_state b;
    memset(&b, 0, sizeof b);
    if (rx_session_bike(s, &b) == 0) { f.speed = b.speed; f.gear = b.gear; }
    rx_race_state r;
    memset(&r, 0, sizeof r);
    if (rx_session_race(s, &r) == 0) {
        f.place = r.place; f.laps = r.laps_completed; f.state = r.state; f.race_time = (double)r.race_time;
    }
    f.pose = pose_hash(s);
    f.bike_pose = bike_pose_hash(s);
    return f;
}
static void print_fingerprint(const char* what, const fingerprint* f, rx_session* s) {
    printf("%s: bike speed %.9g gear %d | race state %d laps %d place %d time %.6f | rider pose %08x bike pose %08x | frames %llu\n",
           what, (double)f->speed, f->gear, f->state, f->laps, f->place, f->race_time, f->pose, f->bike_pose,
           (unsigned long long)rx_session_frames(s));
}

int main(int argc, char** argv) {
    const char* install = argc > 1 ? argv[1] : "game files/Archive";
    const int target = argc > 2 ? atoi(argv[2]) : 1400;
    const char* mode = argc > 3 ? argv[3] : "api";
    const int use_api = strcmp(mode, "api") == 0, use_key = strcmp(mode, "key") == 0;
    const int use_cycle = strcmp(mode, "cycle") == 0;
    if (!use_api && !use_key && !use_cycle && strcmp(mode, "default") != 0) { fprintf(stderr, "mode must be api, key, cycle or default\n"); return 2; }

    rx_session* s = rx_session_create(install);
    if (!s) { fprintf(stderr, "fp_camera_test: create failed\n"); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "fp_camera_test: load failed: %s\n", rx_session_last_error(s)); return 1; }

    printf("mode %s, target %d frames\n", mode, target);
    print_camera(s, "pre-race");

    /* 1. the race state (the countdown's start-gate camera owns the view until the gate leaves) */
    while (rx_session_get_state(s) != RX_SESSION_RACING && (int)rx_session_frames(s) < target) {
        if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); break; }
    }
    rx_addr p = local_player();
    printf("racing at frames %llu (state %d)\n", (unsigned long long)rx_session_frames(s), (int)rx_session_get_state(s));
    print_selection(p, "racing");
    print_camera(s, "gate / default at race start");

    /* 2. hold W until the start-gate camera leaves (it holds until the vehicle is 10 units away or input arrives) */
    uint8_t keys[256];
    memset(keys, 0, sizeof keys);
    keys[KEY_W] = 0x80;
    for (int i = 0; i < 40; i++) {
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); break; }
        p = local_player();
        if (p && U32(p, 0x9a8) != 0 && U32(p, 0x9a8) != U32(p, 0x99c)) { printf("start-gate camera left after %d frame(s)\n", i + 1); break; }
    }
    print_selection(p, "gate left");
    print_camera(s, "default after gate");

    /* 3. the selection: eight frames with W in every mode (the same input everywhere); api calls the setter first, key presses C
     * once (1 -> 2, the first person), cycle presses C four times with a release frame between (1 -> 2 -> 4 -> 0 -> 1) */
    const fingerprint f0 = take_fingerprint(s);
    print_fingerprint("fingerprint before select", &f0, s);
    int sel_rc = 0;
    if (use_api) { sel_rc = rx_session_set_camera(s, 2); printf("rx_session_set_camera(s, 2) rc %d\n", sel_rc); }
    if (use_cycle) {
        /* the game's Camera key cycle: from the default GamePlay entity 1 the key steps 1 -> 2 -> 4 -> 0 -> 1 */
        static const char* const step[4] = {"press 1: 1 -> 2 (first person)", "press 2: 2 -> 4 (user)",
                                            "press 3: 4 -> 0 (visual)", "press 4: 0 -> 1 (game play)"};
        static const uint32_t want[4] = {2, 4, 0, 1};
        int cycle_ok = 1;
        for (int k = 0; k < 4; k++) {
            keys[KEY_W] = 0x80;
            keys[CAMERA_KEY_C] = 0x80;
            rx_session_set_input(s, keys, NULL, 0);
            if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); cycle_ok = 0; break; }
            p = local_player();
            const uint32_t got = p ? U32(p, 0x988) : 0xffffffffu;
            printf("camera key (C) %s: 0x988 %u, entity %08x%s\n", step[k], got, p ? U32(p, 0x974u + 4u * got) : 0,
                   got == want[k] ? "" : " (UNEXPECTED)");
            if (got != want[k]) cycle_ok = 0;
            print_camera(s, step[k]);
            if (!camera_check(s, step[k])) cycle_ok = 0;
            keys[CAMERA_KEY_C] = 0x00;           /* a release frame so the next press is a new edge */
            rx_session_set_input(s, keys, NULL, 0);
            if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); cycle_ok = 0; break; }
        }
        printf("camera cycle: %s\n", cycle_ok ? "ok (1 -> 2 -> 4 -> 0 -> 1)" : "BAD");
    } else {
        for (int i = 0; i < 8; i++) {   /* the same eight W frames as the cycle mode's four press / release pairs */
            const int press = use_key && i == 0;
            keys[KEY_W] = 0x80;
            keys[CAMERA_KEY_C] = press ? 0x80 : 0x00;
            rx_session_set_input(s, keys, NULL, 0);
            if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); break; }
            if (press) printf("camera key (C) pressed on frame %d\n", i + 1);
        }
    }
    print_selection(p, "after select");
    print_camera(s, "selected");
    const fingerprint f1 = take_fingerprint(s);
    print_fingerprint("fingerprint after select", &f1, s);

    /* 4. back to the default third-person view (mode 0 = the game's GamePlay entity 1) through the same setter */
    if (use_api || use_key) {
        const int back = rx_session_set_camera(s, 0);
        printf("rx_session_set_camera(s, 0) rc %d\n", back);
    }
    keys[KEY_W] = 0x80;
    keys[CAMERA_KEY_C] = 0x00;
    rx_session_set_input(s, keys, NULL, 0);
    if (rx_session_advance(s) != 0) fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s));
    print_selection(p, "back");
    print_camera(s, "back to third/default");

    /* 5. to the same frame count in every mode, no input; the fingerprint must match the `default` control */
    memset(keys, 0, sizeof keys);
    rx_session_set_input(s, keys, NULL, 0);
    while ((int)rx_session_frames(s) < target) {
        if (rx_session_advance(s) != 0) { fprintf(stderr, "fp_camera_test: advance failed: %s\n", rx_session_last_error(s)); break; }
    }
    const fingerprint f2 = take_fingerprint(s);
    print_fingerprint("fingerprint final", &f2, s);
    {   /* optional: the final 69 rider bone matrices, for a per-bone diff between the control and the selected runs */
        const char* dump = getenv("FP_POSE_DUMP");
        if (dump) {
            static float pose[69 * 16];
            const int n = rx_session_rider_pose(s, pose, 69);
            FILE* f = fopen(dump, "wb");
            if (f) { fwrite(pose, sizeof(float), (size_t)n * 16, f); fclose(f); printf("rider pose %d bones -> %s\n", n, dump); }
        }
    }
    printf("select rc %d, frames %llu, state %d\n", sel_rc, (unsigned long long)rx_session_frames(s), (int)rx_session_get_state(s));
    rx_session_destroy(s);
    return 0;
}
