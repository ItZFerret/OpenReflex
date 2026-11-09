/* camera_distance_probe.c (w-camfix) -- the game's own driving camera against the local bike, per frame, headless: what a custom camera
 * preset (Unity: OriginalWorld/ReflexCameraRig.cs) sits on, since the preset's DISTANCE / HEIGHT are offsets from this camera's pose.
 *
 *   camera_distance_probe <install root> <event type> <location> [racing frames (1800)] [DrivingCamera preset 0..3 (-1: the default)] [map scale (1)]
 *   env: CDP_RIDERS (1), CDP_EVERY (30: a CAM line every n frames), CDP_STILL (0; 1: no input, the bike stays at the gate),
 *        CDP_PACE (1: the autopilot's target speeds times this)
 *
 * Rides the local player with career_series_test.c's race-line autopilot once the race runs (state 2). Every CDP_EVERY frames a line
 *   CAM f <frame> state <race state> speed <ft/s> dist <|cam - bike|> back <along the view> up <cam.y - bike.y> fov <deg> near <near> far <far>
 * and at the end per race state (0 Pre, 1 Countdown, 2 InProgress): min / mean / max of dist, back, up, fov and near, then
 *   CDPSUM <location> preset <p> scale <s> race_dist min <..> mean <..> max <..> slow_mean <speed < 20> fast_mean <speed > 45> near <min..max>
 * Build: core/tools/camera_distance_probe.sh <out dir> (links the product objects, tools/product_link.sh). */
#include "rx_api.h"
#include "rx_ui_settings.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int env_int(const char* n, int d) { const char* e = getenv(n); return e && *e ? atoi(e) : d; }

/* ---- the race-line autopilot (career_series_test.c) ---- */
static float wrap_angle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
struct autopilot { float stuck_clock, reverse_clock, pace; };
static void autopilot_pad(struct rx_session* s, struct autopilot* pilot, rx_pad* pad) {
    rx_bike_state bike; float line[64 * 3];
    const int points = rx_session_bike(s, &bike) == 0 ? rx_session_race_line(s, line, 64) : 0;
    if (points < 2) return;
    const float x = bike.position[0][0], z = bike.position[0][2];
    int nearest = 0; float best = 1e30f;
    for (int i = 0; i < points && i < 6; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
    const float look = 18.0f + bike.speed * 0.6f;
    int target = nearest;
    for (int i = nearest; i < points; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z; target = i; if (dx * dx + dz * dz >= look * look) break; }
    const float tx = line[3 * target] - x, tz = line[3 * target + 2] - z;
    const float* q = bike.orientation[0];
    const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
    const float error = wrap_angle(atan2f(tx, tz) - atan2f(fx, fz));
    float steer = error * 2.2f; if (steer > 1.0f) steer = 1.0f; if (steer < -1.0f) steer = -1.0f;
    const float speed = bike.speed, turn = fabsf(error);
    const float desired = pilot->pace * (turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f);
    if (speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f; else pilot->stuck_clock = 0.0f;
    if (pilot->stuck_clock > 4.0f) { pilot->reverse_clock = 1.5f; pilot->stuck_clock = 0.0f; }
    if (pilot->reverse_clock > 0.0f) { pilot->reverse_clock -= 1.0f / 60.0f; pad->left_trigger = 255; pad->lx = (int16_t)(-steer * 32000.0f); }
    else {
        pad->lx = (int16_t)(steer * 32000.0f);
        if (speed < desired) pad->right_trigger = 255; else if (speed > desired + 6.0f) pad->left_trigger = 160; else pad->right_trigger = 120;
    }
}

struct stat { int n; double sum; float lo, hi; };
static void add(struct stat* s, float v) { if (!s->n || v < s->lo) s->lo = v; if (!s->n || v > s->hi) s->hi = v; s->sum += v; s->n++; }
static double mean(const struct stat* s) { return s->n ? s->sum / s->n : 0; }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: camera_distance_probe <install> <event type> <location> [racing frames] [preset] [scale]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    const int racing_frames = argc > 4 ? atoi(argv[4]) : 1800, preset = argc > 5 ? atoi(argv[5]) : -1;
    const float scale = argc > 6 ? (float)atof(argv[6]) : 1.0f;
    const int every = env_int("CDP_EVERY", 30), still = env_int("CDP_STILL", 0);
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (preset >= 0) { int32_t before = -9; const int rc = rx_session_ui_setting(s, "DrivingCamera", preset, 1, &before); printf("DrivingCamera %d -> %d (was %d)\n", preset, rc, before); }
    if (scale != 1.0f && rx_session_set_map_scale(s, scale) != 0) { printf("set_map_scale: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]);
    snprintf(e.location, sizeof e.location, "%s", argv[3]);
    e.riders = env_int("CDP_RIDERS", 1); e.ai_difficulty = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("event load: %s\n", rx_session_last_error(s)); return 1; }
    struct autopilot pilot = {0, 0, 1.0f};
    if (getenv("CDP_PACE")) pilot.pace = (float)atof(getenv("CDP_PACE"));   /* the autopilot's target speeds x this (faster riding) */
    struct stat dist[4], back[4], up[4], fov[4], nearp[4], slow, fast;
    memset(dist, 0, sizeof dist); memset(back, 0, sizeof back); memset(up, 0, sizeof up); memset(fov, 0, sizeof fov); memset(nearp, 0, sizeof nearp);
    memset(&slow, 0, sizeof slow); memset(&fast, 0, sizeof fast);
    int raced = 0, cam_preset = -9;
    for (long f = 0; f < 30000 && raced < racing_frames; f++) {
        rx_race_state r; memset(&r, 0, sizeof r); rx_session_race(s, &r);
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        rx_pad pad; memset(&pad, 0, sizeof pad);
        if (!still && r.valid && r.state == 2) autopilot_pad(s, &pilot, &pad);
        rx_session_set_input(s, keys, &pad, 1);
        if (rx_session_advance(s) != 0) { printf("advance failed: %s\n", rx_session_last_error(s)); return 1; }
        if (rx_session_get_state(s) == RX_SESSION_LOADING) continue;
        rx_session_race(s, &r);
        rx_camera c; rx_bike_state b;
        if (rx_session_camera(s, &c) != 0 || rx_session_bike(s, &b) != 0) continue;
        rx_camera_bindings cb; memset(&cb, 0, sizeof cb);
        if (rx_session_camera_bindings(s, &cb) == 0 && cb.driving_camera != cam_preset) { cam_preset = cb.driving_camera; printf("frame %ld: driving_camera %d (race state %d)\n", f, cam_preset, r.state); }
        const float dx = c.position[0] - b.position[0][0], dy = c.position[1] - b.position[0][1], dz = c.position[2] - b.position[0][2];
        const float d = sqrtf(dx * dx + dy * dy + dz * dz);
        const float bk = -(dx * c.forward[0] + dy * c.forward[1] + dz * c.forward[2]);   /* the bike's depth in the view */
        const float fd = c.fov * 57.29578f;
        const int st = r.valid && r.state >= 0 && r.state <= 3 ? r.state : 3;
        if (r.valid && r.state <= 2) { add(&dist[st], d); add(&back[st], bk); add(&up[st], dy); add(&fov[st], fd); add(&nearp[st], c.near_plane); }
        if (r.valid && r.state == 2) { raced++; if (b.speed < 20) add(&slow, d); else if (b.speed > 45) add(&fast, d); }
        if (every > 0 && f % every == 0)
            printf("CAM f %ld state %d speed %.1f dist %.2f back %.2f up %.2f fov %.1f near %.3f far %.0f\n", f, r.valid ? r.state : -1, b.speed, d, bk, dy, fd, c.near_plane, c.far_plane);
    }
    static const char* names[4] = {"Pre", "Countdown", "InProgress", "other"};
    for (int i = 0; i < 3; i++)
        if (dist[i].n) printf("STATE %-10s frames %5d dist %.2f/%.2f/%.2f back %.2f/%.2f/%.2f up %.2f/%.2f/%.2f fov %.1f/%.1f near %.3f/%.3f\n", names[i], dist[i].n,
                              dist[i].lo, mean(&dist[i]), dist[i].hi, back[i].lo, mean(&back[i]), back[i].hi, up[i].lo, mean(&up[i]), up[i].hi, fov[i].lo, fov[i].hi, nearp[i].lo, nearp[i].hi);
    printf("CDPSUM %s preset %d scale %.2f race_dist min %.2f mean %.2f max %.2f slow_mean %.2f (%d) fast_mean %.2f (%d) near %.3f..%.3f gate_dist %.2f\n", argv[3], cam_preset, scale,
           dist[2].lo, mean(&dist[2]), dist[2].hi, mean(&slow), slow.n, mean(&fast), fast.n, nearp[2].lo, nearp[2].hi, mean(&dist[1]));
    rx_session_destroy(s);
    return raced ? 0 : 1;
}
