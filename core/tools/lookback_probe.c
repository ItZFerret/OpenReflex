/* lookback_probe.c (w-lookback) -- the stock camera's look back (UserCamQuickBack: D-pad down, keyboard 3) across the transitions a player
 * meets, headless through the PRODUCT session API: is the view forward again once the input is let go?
 *
 *   lookback_probe <install root> [scenario|all] [level [location id]]      (default all, FR_Alpine 45: a Free Ride)
 *   env: LBP_TRACE=1 a line per frame (else only the phase changes and the checks)
 *
 * Each scenario is a fresh session of the same Free Ride: stand 2 s, ride 3 s (throttle), then the scenario's script with the look back
 * held on the pad (XINPUT D-pad down 0x2) or the keyboard (DIK 0x04 "3"), the release, and 2 s riding on. Per frame:
 *   q    the player's look direction P+0x994 (Player_UpdateCamera 0054c150: -1 none, 0 / 1 quick left / right, 2 back)
 *   cur  the current camera entity P+0x9a8 is the quick camera P+0x998
 *   dot  the render camera's horizontal forward . the bike's horizontal heading (+1 looking ahead, -1 looking back)
 * Checks per scenario (exit 0 = all pass): while held (riding, not wrecked) the view looks back (q 2, dot < -0.5) and 1 s after the release
 * it looks ahead (q -1, the quick camera not current, dot > 0.5) and stays so to the end.
 * Scenarios:
 *   plain        hold 1 s, release
 *   kb           the keyboard's 3 instead of the pad
 *   instant      held through rx_session_spawn_at_instant (the SPAWN MARKER respawn), released 0.5 s later
 *   instant_rel  the instant respawn and the release on the same frame
 *   slow         held through rx_session_spawn_at (the reset key's path, 0.5 s fade), released after it
 *   slow_rel     released during the slow respawn's fade
 *   reset        the RESET key pressed while held
 *   menu         the client's menu / pause frames: pads and keys zeroed, pad mask 0, 20 frames, then back with nothing held
 *   menu_held    the same, the D-pad still held when the frames come back (the client masks it until let go: buttons 0)
 *   disc         the pad gone (mask 0) while held, back 20 frames later with nothing held
 *   camkey       the Camera key tapped while held (the cycle of views)
 *   setcam       rx_session_set_driving_camera (the options page / the client's camera cycle) while held
 *   left         the quick look left (D-pad left) held, then down while still held, left released first
 *   mouse        the left mouse button (the look back's mouse binding) down, then up (rx_session_set_mouse: Raw Input button flags)
 *   mouse_noup   the left mouse button down and never up: the game keeps looking back (Raw Input semantics, the original's too). PASSES when
 *                it stays stuck: it shows the mechanism of the w-lookback report (the client lost the UP when its mouse feed stopped)
 *   mouse_menu   the fixed client: LEFT down while riding, a menu takes the mouse while held: its UP goes with the menu's first frame
 * Build: core/tools/lookback_probe.sh <out dir> */
#include "rx_api.h"
#include "rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_session* s;
static int dik_of[256];
static rx_remap_table remap;
#define KB(col) (dik_of[remap.keyboard[(col)] & 255] & 255)
static int failures, trace;

static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8u);
    for (uint32_t i = 0; i < U8(g, 0x1541c) && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot < 16 && U32(g + 0x7460 + slot * 0x12a0, 0x10) == 0) return g + 0x7460 + slot * 0x12a0;
    }
    return 0;
}

typedef struct { int q, cur_quick, wrecked; float dot, speed; } look_t;
static look_t look(void) {
    look_t r; memset(&r, 0, sizeof r); r.q = -9; r.dot = 0;
    const rx_addr p = local_player();
    if (p) { r.q = I32(p, 0x994); r.cur_quick = U32(p, 0x998) != 0 && U32(p, 0x9a8) == U32(p, 0x998); }
    rx_camera c; rx_bike_state b;
    if (rx_session_camera(s, &c) == 0 && rx_session_bike(s, &b) == 0) {
        const float* q = b.orientation[0];
        float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
        float cx = c.forward[0], cz = c.forward[2];
        const float lb = hypotf(fx, fz), lc = hypotf(cx, cz);
        if (lb > 1e-3f && lc > 1e-3f) r.dot = (fx * cx + fz * cz) / (lb * lc);
        r.speed = b.speed;
    }
    rx_onboard_state o; memset(&o, 0, sizeof o);
    if (rx_session_onboard_state(s, &o) == 0) r.wrecked = o.wrecked;
    return r;
}

/* one frame: the throttle, the look back on the pad / keyboard, other keys, the pad mask */
typedef struct { int dpad, kb3, throttle, pad_mask, extra_dik, extra_buttons; } in_t;
static int frame_no;
static const char* phase = "";
static int step(in_t in) {
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    rx_pad pad; memset(&pad, 0, sizeof pad);
    if (in.throttle) pad.right_trigger = 255;
    if (in.dpad) pad.buttons |= 0x2;
    pad.buttons |= (uint16_t)in.extra_buttons;
    if (in.kb3) keys[0x04] = 0x80;
    if (in.extra_dik) keys[in.extra_dik & 255] = 0x80;
    if (in.pad_mask == 0) memset(&pad, 0, sizeof pad);
    if (rx_session_set_input(s, keys, &pad, (uint32_t)in.pad_mask) != 0) return 1;
    if (rx_session_advance_dt(s, 1.f / 60)) return 1;
    frame_no++;
    if (trace) { look_t l = look(); printf("    f%-5d %-12s in d%d k%d m%d x%02x b%04x  q %2d cur_quick %d dot %+.2f speed %5.1f wrecked %d\n", frame_no, phase,
                                          in.dpad, in.kb3, in.pad_mask, in.extra_dik, in.extra_buttons, l.q, l.cur_quick, l.dot, l.speed, l.wrecked); }
    return 0;
}
static int run_frames(int n, in_t in) { for (int i = 0; i < n; i++) if (step(in)) return 1; return 0; }

static int load(const char* install, const char* level, int loc) {
    s = rx_session_create(install);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = loc; e.track_id = 0; e.vehicle_class = -1; e.riders = 1; e.ai_difficulty = 0; e.duration = 0; e.duration_type[0] = 0; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f++ < 3000) if (rx_session_advance_dt(s, 1.f / 60)) break;
    for (int i = 0; i < 3000 && rx_session_get_state(s) != RX_SESSION_RACING; i++) if (rx_session_advance_dt(s, 1.f / 60)) break;
    if (rx_session_get_state(s) != RX_SESSION_RACING) { printf("not racing (state %d): %s\n", (int)rx_session_get_state(s), rx_session_last_error(s)); return 1; }
    memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    memset(dik_of, 0, sizeof dik_of);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    frame_no = 0;
    return 0;
}

/* held: the frames' worst look while the input was held (riding); after: from 60 frames after the release to the end */
typedef struct { int held_frames, held_back, after_frames, after_ahead, first_bad; look_t worst_after; } verdict_t;
static void note_held(verdict_t* v) { look_t l = look(); if (l.wrecked) return; v->held_frames++; if (l.q == 2 && l.dot < -0.5f) v->held_back++; }
static int hold(verdict_t* v, int n, in_t in) { for (int i = 0; i < n; i++) { if (step(in)) return 1; note_held(v); } return 0; }
static int after(verdict_t* v, int n, in_t in) {
    for (int i = 0; i < n; i++) {
        if (step(in)) return 1;
        if (i < 60) continue;
        look_t l = look(); v->after_frames++;
        if (l.q == -1 && !l.cur_quick && l.dot > 0.5f) v->after_ahead++;
        else if (v->first_bad < 0) { v->first_bad = frame_no; v->worst_after = l; }
    }
    return 0;
}

static int scenario(const char* install, const char* level, int loc, const char* name) {
    if (load(install, level, loc)) return 1;
    float marker[3], dir[3];
    const in_t ride = {0, 0, 1, 1, 0, 0}, ride_back = {1, 0, 1, 1, 0, 0};
    phase = "stand"; if (run_frames(120, (in_t){0, 0, 0, 1, 0, 0})) return 1;
    if (rx_session_spawn_pose(s, marker, dir) != 0) { printf("pose failed\n"); return 1; }
    phase = "ride"; if (run_frames(180, ride)) return 1;
    verdict_t v; memset(&v, 0, sizeof v); v.first_bad = -1;
    int expect_held = 1, expect_stuck = 0;
    phase = "held";
    if (!strcmp(name, "plain")) {
        if (hold(&v, 60, ride_back)) return 1;
    } else if (!strcmp(name, "kb")) {
        if (hold(&v, 60, (in_t){0, 1, 1, 1, 0, 0})) return 1;
    } else if (!strcmp(name, "instant") || !strcmp(name, "instant_rel")) {
        if (hold(&v, 30, ride_back)) return 1;
        const int rc = rx_session_spawn_at_instant(s, marker, dir);
        printf("  spawn_at_instant rc %d\n", rc);
        if (!strcmp(name, "instant") && hold(&v, 30, ride_back)) return 1;
    } else if (!strcmp(name, "slow") || !strcmp(name, "slow_rel")) {
        if (hold(&v, 30, ride_back)) return 1;
        const int rc = rx_session_spawn_at(s, marker, dir);
        printf("  spawn_at rc %d\n", rc);
        phase = "fade";
        if (run_frames(!strcmp(name, "slow") ? 45 : 10, ride_back)) return 1;
    } else if (!strcmp(name, "reset")) {
        if (hold(&v, 30, ride_back)) return 1;
        phase = "reset key";
        if (run_frames(3, (in_t){1, 0, 1, 1, KB(12), 0})) return 1;
        if (run_frames(45, ride_back)) return 1;
    } else if (!strcmp(name, "menu") || !strcmp(name, "menu_held") || !strcmp(name, "disc")) {
        if (hold(&v, 30, ride_back)) return 1;
        phase = !strcmp(name, "disc") ? "pad gone" : "menu";
        if (run_frames(20, (in_t){0, 0, 0, 0, 0, 0})) return 1;
        if (!strcmp(name, "menu_held")) { phase = "masked"; if (run_frames(30, ride)) return 1; }   /* the client masks the still-held D-pad: 0 */
    } else if (!strcmp(name, "camkey")) {
        if (hold(&v, 30, ride_back)) return 1;
        phase = "camera key";
        if (run_frames(3, (in_t){1, 0, 1, 1, KB(10), 0})) return 1;
        phase = "held";
        if (run_frames(30, ride_back)) return 1;
    } else if (!strcmp(name, "setcam")) {
        if (hold(&v, 30, ride_back)) return 1;
        printf("  set_driving_camera 0 rc %d\n", rx_session_set_driving_camera(s, 0));
        if (run_frames(10, ride_back)) return 1;
        printf("  set_driving_camera 1 rc %d\n", rx_session_set_driving_camera(s, 1));
        if (run_frames(30, ride_back)) return 1;
    } else if (!strcmp(name, "left")) {
        phase = "left";
        if (run_frames(30, (in_t){0, 0, 1, 1, 0, 0x4})) return 1;
        phase = "left+down";
        if (run_frames(30, (in_t){1, 0, 1, 1, 0, 0x4})) return 1;
        phase = "held";
        if (hold(&v, 30, ride_back)) return 1;
    } else if (!strcmp(name, "mouse") || !strcmp(name, "mouse_noup")) {
        /* the left mouse button: UserCamQuickBack's mouse binding (KeyMap +0x4444: device 4, code 0x1b = the mouse's button 0 record). Raw Input
         * semantics (Mouse_AccumulateRawInput 00817cb0): a DOWN holds the button until an UP, there is no per-frame state */
        printf("  mouse LEFT DOWN rc %d\n", rx_session_set_mouse(s, 0, 0, 0, 0x1));
        if (hold(&v, 60, ride)) return 1;
        if (!strcmp(name, "mouse")) printf("  mouse LEFT UP rc %d\n", rx_session_set_mouse(s, 0, 0, 0, 0x2));
        else { printf("  (no LEFT UP: what the game sees when the host loses the release)\n"); expect_stuck = 1; }
    } else if (!strcmp(name, "mouse_menu")) {
        /* the fixed client (Unity OriginalWorld/MouseButtonFeed.cs): LEFT down while riding, then a menu / the pause takes the mouse while it is
         * held: the UP the game is owed goes with the menu's first frame (and the button's later release is not the game's) */
        printf("  mouse LEFT DOWN rc %d\n", rx_session_set_mouse(s, 0, 0, 0, 0x1));
        if (hold(&v, 30, ride)) return 1;
        printf("  menu opens: mouse LEFT UP rc %d\n", rx_session_set_mouse(s, 0, 0, 0, 0x2));
        phase = "menu";
        if (run_frames(20, (in_t){0, 0, 0, 0, 0, 0})) return 1;
    } else { printf("unknown scenario %s\n", name); return 1; }
    phase = "released";
    if (after(&v, 180, ride)) return 1;
    look_t end = look();
    const int ok_held = !expect_held || v.held_frames == 0 || v.held_back * 10 >= v.held_frames * 8;
    const int ok_after = expect_stuck ? v.after_ahead == 0 : v.after_frames > 0 && v.after_ahead == v.after_frames;   /* (mouse_noup: the mechanism, stuck as expected) */
    printf("LBP %-12s held %d/%d back  after %d/%d ahead  end q %d cur_quick %d dot %+.2f wrecked %d  %s\n", name, v.held_back, v.held_frames,
           v.after_ahead, v.after_frames, end.q, end.cur_quick, end.dot, end.wrecked, ok_held && ok_after ? "PASS" : "FAIL");
    if (!ok_after) printf("    first bad frame %d: q %d cur_quick %d dot %+.2f wrecked %d speed %.1f\n", v.first_bad, v.worst_after.q, v.worst_after.cur_quick,
                          v.worst_after.dot, v.worst_after.wrecked, v.worst_after.speed);
    if (!(ok_held && ok_after)) failures++;
    rx_session_destroy(s); s = NULL;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: lookback_probe <install> [scenario|all] [level [location id]]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    trace = getenv("LBP_TRACE") && atoi(getenv("LBP_TRACE")) != 0;
    const char* which = argc > 2 ? argv[2] : "all";
    const char* level = argc > 3 ? argv[3] : "FR_Alpine";
    const int loc = argc > 4 ? atoi(argv[4]) : 45;
    static const char* all[] = {"plain", "kb", "instant", "instant_rel", "slow", "slow_rel", "reset", "menu", "menu_held", "disc", "camkey", "setcam", "left", "mouse", "mouse_noup", "mouse_menu"};
    printf("lookback_probe: FreeRide %s\n", level);
    for (size_t i = 0; i < sizeof all / sizeof *all; i++) {
        if (strcmp(which, "all") && strcmp(which, all[i])) continue;
        if (scenario(argv[1], level, loc, all[i])) { printf("LBP %s: run failed\n", all[i]); failures++; if (s) { rx_session_destroy(s); s = NULL; } }
    }
    printf("lookback_probe: %s (%d failing)\n", failures ? "FAIL" : "PASS", failures);
    return failures ? 1 : 0;
}
