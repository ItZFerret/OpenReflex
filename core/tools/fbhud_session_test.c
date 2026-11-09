/* fbhud_session_test.c -- w-fb-hud: RESET TRACK and CONTROLS IN A RACE through the PRODUCT session API (core/api/fbhud_session.inc).
 *   fbhud_session_test <install root> [level [location id]]      (default FR_Alpine 45, a Free Ride)
 * RESET TRACK (rx_session_terrain_reset):
 *   ruts      riding (throttle, steer pulses) changes the deformation grid and lays tyre tracks
 *   reset     after the call and one frame: the deformation grid and the collision heights equal the ones at the world's start (every
 *             texel), the tyre tracks are gone, the export's dirty rectangle is the whole grid, the session still races, the rider is where
 *             he was (the frame moved him only by his own speed, which the reset keeps) and rides on; ruts form again afterwards
 * CONTROLS IN A RACE (rx_session_remap_apply):
 *   refused   rx_session_remap_set in a loaded event still answers 2 (unchanged)
 *   applied   rx_session_remap_apply in the event answers 0; rx_session_remap_get reads the new throttle key (U); holding U rides the bike;
 *             holding W (the old key) does not
 *   same      a second session with U set before its load (the menus' path) and the same ride: the bike's path is the same as this
 *             session's after the apply at the same frame (position within 0.01 ft over 4 s): the two paths are one
 * Build: core/tools/fbhud_session_test.sh <out dir> */
#include "rx_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_session* s;
/* (rx_session_destroy resets the statically linked driver's static data, as rpm_gauge_trace found: the counter is main's local) */
#define CHECK(c, ...) do { if (c) printf("  ok   %s\n", #c); else { failures++; printf("  FAIL %s  ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static rx_remap_table defaults;
static int dik_of[256];
#define DIK_W 0x11
#define DIK_U 0x16

static int step(const uint8_t* keys) {
    if (rx_session_set_input(s, keys, NULL, 0) != 0) return 1;
    return rx_session_advance_dt(s, 1.f / 60);
}
static int idle(int frames) { uint8_t k[256] = {0}; for (int i = 0; i < frames; i++) if (step(k)) return 1; return 0; }
static int ride(int frames, int dik, int steer_dik) {
    uint8_t k[256];
    for (int i = 0; i < frames; i++) {
        memset(k, 0, sizeof k); k[dik] = 0x80;
        if (steer_dik && (i / 90) % 2 == 0) k[steer_dik] = 0x80;
        if (step(k)) return 1;
    }
    return 0;
}
static void bike(float pos[3], float* speed) {
    rx_bike_state b; memset(&b, 0, sizeof b); rx_session_bike(s, &b);
    pos[0] = b.position[0][0]; pos[1] = b.position[0][1]; pos[2] = b.position[0][2]; if (speed) *speed = b.speed;
}
static float dist(const float a[3], const float b[3]) { return sqrtf((a[0] - b[0]) * (a[0] - b[0]) + (a[1] - b[1]) * (a[1] - b[1]) + (a[2] - b[2]) * (a[2] - b[2])); }

static int load(const char* install, const char* level, int loc, const int32_t* remap_before) {
    s = rx_session_create(install);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (remap_before && rx_session_remap_set(s, remap_before) != 0) { printf("remap_set before the load: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "FreeRide"); e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = loc; e.track_id = 0; e.vehicle_class = -1; e.riders = 1; e.ai_difficulty = 0; e.duration = 0; e.duration_type[0] = 0; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f++ < 3000) if (rx_session_advance_dt(s, 1.f / 60)) break;
    for (int i = 0; i < 3000 && rx_session_get_state(s) != RX_SESSION_RACING; i++) if (rx_session_advance_dt(s, 1.f / 60)) break;
    if (rx_session_get_state(s) != RX_SESSION_RACING) { printf("not racing (state %d): %s\n", (int)rx_session_get_state(s), rx_session_last_error(s)); return 1; }
    return 0;
}

static float *grid(int deform, int n) {
    float* g = malloc(sizeof(float) * (size_t)n * (size_t)n);
    if (!g) return NULL;
    const int rc = deform ? rx_session_terrain_deform(s, g, 0, 0, n, n) : rx_session_terrain_heights(s, g, 0, 0, n, n);
    if (rc) { free(g); return NULL; }
    return g;
}
static long differ(const float* a, const float* b, int n, float* max) {
    long c = 0; float m = 0;
    for (long i = 0; i < (long)n * n; i++) { float d = fabsf(a[i] - b[i]); if (d > 0) { c++; if (d > m) m = d; } }
    if (max) *max = m;
    return c;
}
static int tracks(void) { rx_track_info ti; memset(&ti, 0, sizeof ti); rx_session_tracks(s, NULL, 0, &ti); return ti.triangles; }

/* the default bindings with the throttle on U */
static void throttle_on_u(int32_t v[38]) {
    for (int i = 0; i < 19; i++) { v[i] = defaults.keyboard[i]; v[19 + i] = defaults.pad[i]; }
    v[0] = defaults.scan[DIK_U];
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: fbhud_session_test <install> [level [location id]]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    int failures = 0;
    const char* level = argc > 2 ? argv[2] : "FR_Alpine";
    const int loc = argc > 3 ? atoi(argv[3]) : 45;
    memset(&defaults, 0, sizeof defaults); rx_remap_defaults(&defaults);
    for (int d = 255; d > 0; d--) if (defaults.scan[d] > 0 && defaults.scan[d] < 256) dik_of[defaults.scan[d]] = d;
    const int W = dik_of[defaults.keyboard[0] & 255];   /* the default throttle key (W) */
    printf("fbhud_session_test: Free Ride %s; throttle key DIK %02x (W %02x), U id %d\n", level, W, DIK_W, defaults.scan[DIK_U]);
    CHECK(W == DIK_W, "the default throttle is W (%02x)", W);

    /* ---------------------------------------------------------------- RESET TRACK */
    if (load(argv[1], level, loc, NULL)) return 1;
    rx_terrain_info ti; memset(&ti, 0, sizeof ti);
    if (rx_session_terrain(s, &ti) != 0) { printf("no terrain\n"); return 1; }
    const int n = (int)ti.samples;
    printf("  terrain %d x %d, gain %.2f\n", n, n, ti.gain);
    if (idle(240)) return 1;   /* (standing on the ground at the start, as spawn_marker_test) */
    rx_session_terrain(s, &ti);
    float* d0 = grid(1, n); float* h0 = grid(0, n);
    int tr0 = tracks();
    { float q0[3], sp; bike(q0, &sp); printf("  start at %.1f %.1f %.1f speed %.1f\n", q0[0], q0[1], q0[2], sp); }
    for (int k = 0; k < 3; k++)
    {
        if (ride(300, W, k == 2 ? 0x1e : 0)) { printf("ride: %s\n", rx_session_last_error(s)); return 1; }
        float q[3], sp; bike(q, &sp); printf("  riding: %.1f %.1f %.1f speed %.1f\n", q[0], q[1], q[2], sp);
    }
    if (idle(300)) return 1;   /* (the bike rolls to a stop) */
    float* d1 = grid(1, n); float m1 = 0; long c1 = differ(d0, d1, n, &m1); int tr1 = tracks();
    printf("  after the ride: %ld texels deformed (max %.4f), tyre-track triangles %d -> %d\n", c1, m1, tr0, tr1);
    CHECK(c1 > 0, "ruts: the ride deformed nothing");
    CHECK(tr1 > tr0, "ruts: no tyre tracks (%d)", tr1);
    float p0[3], v0; bike(p0, &v0);
    rx_session_terrain(s, &ti);   /* (the export looked once this frame: the reset must still resync it) */
    CHECK(rx_session_terrain_reset(s) == 0, "reset answered: %s", rx_session_last_error(s));
    if (idle(1)) return 1;
    rx_session_terrain(s, &ti);
    float* d2 = grid(1, n); float* h2 = grid(0, n);
    float m2 = 0, mh = 0; long c2 = differ(d0, d2, n, &m2), ch = differ(h0, h2, n, &mh);
    float p1[3], v1; bike(p1, &v1);
    printf("  after the reset: %ld texels differ from the start (max %.5f), heights %ld (max %.5f), tracks %d, dirty %d,%d - %d,%d, bike moved %.3f ft (speed %.2f -> %.2f)\n",
           c2, m2, ch, mh, tracks(), ti.dirty_x0, ti.dirty_z0, ti.dirty_x1, ti.dirty_z1, dist(p0, p1), v0, v1);
    CHECK(c2 == 0, "reset: %ld deformation texels differ from the world's start", c2);
    CHECK(ch == 0, "reset: %ld heights differ from the world's start", ch);
    CHECK(tracks() == 0 || tracks() < tr1 / 10, "reset: tyre tracks %d", tracks());
    CHECK(ti.dirty_x0 == 0 && ti.dirty_z0 == 0 && ti.dirty_x1 == n && ti.dirty_z1 == n, "reset: the export's dirty rectangle %d,%d - %d,%d", ti.dirty_x0, ti.dirty_z0, ti.dirty_x1, ti.dirty_z1);
    /* (the bike may still roll: the reset must not move it otherwise, nor change its speed: one frame's travel at its speed) */
    CHECK(fabsf(dist(p0, p1) - v0 / 60.f) < 0.1f && fabsf(v1 - v0) < 1.f, "reset: the bike moved %.3f ft in the frame at %.2f ft/s (%.3f expected), speed %.2f -> %.2f", dist(p0, p1), v0, v0 / 60.f, v0, v1);
    CHECK(rx_session_get_state(s) == RX_SESSION_RACING, "reset: state %d", (int)rx_session_get_state(s));
    if (ride(600, W, 0)) return 1;
    float* d3 = grid(1, n); long c3 = differ(d0, d3, n, NULL); float p2[3]; bike(p2, NULL);
    CHECK(c3 > 0 && dist(p1, p2) > 30.f, "after the reset the ride goes on and ruts form again (%ld texels, %.0f ft)", c3, dist(p1, p2));
    free(d0); free(h0); free(d1); free(d2); free(h2); free(d3);

    /* ---------------------------------------------------------------- CONTROLS IN A RACE */
    int32_t u[38]; throttle_on_u(u);
    if (idle(240)) return 1;
    CHECK(rx_session_remap_set(s, u) == 2, "refused: remap_set in the event must stay refused");
    const int apply_rc = rx_session_remap_apply(s, u);
    CHECK(apply_rc == 0, "applied: remap_apply answered %d (%s)", apply_rc, rx_session_last_error(s));
    rx_remap_table now; memset(&now, 0, sizeof now); rx_session_remap_get(s, &now);
    CHECK(now.keyboard[0] == defaults.scan[DIK_U], "applied: the table's throttle %d", now.keyboard[0]);
    float a0[3], sa0; bike(a0, &sa0);
    if (ride(240, DIK_U, 0)) return 1;
    float a1[3], sa1; bike(a1, &sa1);
    printf("  holding U: %.1f -> %.1f ft/s, %.0f ft\n", sa0, sa1, dist(a0, a1));
    CHECK(sa1 > sa0 + 5.f && dist(a0, a1) > 30.f, "applied: U must ride the bike (%.1f ft/s)", sa1);
    if (idle(420)) return 1;
    float b0[3], sb0; bike(b0, &sb0);
    if (ride(150, DIK_W, 0)) return 1;
    float b1[3], sb1; bike(b1, &sb1);
    printf("  holding W: %.1f -> %.1f ft/s\n", sb0, sb1);
    CHECK(sb1 <= sb0 + 1.f, "applied: W (the old key) must not throttle (%.1f -> %.1f)", sb0, sb1);
    rx_session_destroy(s); s = NULL;

    /* same: the menus' path (set before the load) and the race's (apply in it at frame F), the same ride after F */
    const int F = 200, R = 240;
    float pa[R][3], pb[R][3];
    for (int run = 0; run < 2; run++) {
        if (load(argv[1], level, loc, run == 0 ? u : NULL)) return 1;
        if (idle(F)) return 1;
        if (run == 1 && rx_session_remap_apply(s, u) != 0) { printf("apply: %s\n", rx_session_last_error(s)); return 1; }
        for (int i = 0; i < R; i++) { if (ride(1, DIK_U, 0)) return 1; bike(run == 0 ? pa[i] : pb[i], NULL); }
        rx_session_destroy(s); s = NULL;
    }
    float worst = 0; for (int i = 0; i < R; i++) { float d = dist(pa[i], pb[i]); if (d > worst) worst = d; }
    printf("  the menus' path vs the race's: %.0f ft ridden, the paths differ by %.5f ft at most\n", dist(pa[0], pa[R - 1]), worst);
    CHECK(dist(pa[0], pa[R - 1]) > 30.f && worst < 0.01f, "same: the two paths differ by %.4f ft", worst);

    printf("%s\n", failures ? "FAILED" : "PASS");
    return failures ? 1 : 0;
}
