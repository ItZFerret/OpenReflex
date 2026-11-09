/* map_scale_probe.c -- MAP SCALE headless probe (core/plan/MAP_SCALE.md): loads an exhibition event at a map scale and rides it.
 *   map_scale_probe <install root> <event type> <location> [race frames (2400)] [scale (1)]
 * env: RX_PROBE_RIDERS (12; 1 for a Free Ride), RX_PROBE_AI (95), RX_PROBE_LAPS (2), RX_PROBE_STEER (1 / -1: the stick's sign).
 *
 * After the load it prints every slot's start position (bike node 0), then rides the local player with the pad: full throttle, the left
 * stick steering toward the race line a few nodes ahead (a Free Ride rides the throttle with a slow weave). Every 60 frames: the
 * player's position, the terrain under it (rx_session_terrain_heights) and the lowest clearance seen; at the end every rider's distance
 * from its start, laps and place, the terrain info, and a verdict line:
 *   MAPSCALE <scale> <location> start_ok <0|1> below_ground <frames> moved_ai <n>/<m> player_moved <ft> laps <player laps>
 * Build: clang -std=c11 -O2 -Icore/include -Icore/src -Icore/game -Icore/api core/tools/map_scale_probe.c -Lcore/build -lrxcore
 *        -Wl,-rpath,<abs>/core/build -o <out> (run from core/, the library's install name is build/librxcore.dylib). */
#include "rx_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int env_int(const char* n, int d) { const char* e = getenv(n); return e && *e ? atoi(e) : d; }

static float ground_at(struct rx_session* s, const rx_terrain_info* t, float x, float z, int* ok) {
    *ok = 0;
    if (!t->samples || t->spacing <= 0) return 0;
    const int ix = (int)floorf((x - t->min_x) / t->spacing + 0.5f), iz = (int)floorf((t->max_z - z) / t->spacing + 0.5f);   /* row 0 = max z (world z = max z - row * spacing) */
    if (ix < 0 || iz < 0 || ix >= (int)t->stride || iz >= (int)t->samples) return 0;
    float h = 0;
    if (rx_session_terrain_heights(s, &h, ix, iz, 1, 1) != 0) return 0;
    *ok = 1;
    return h;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: map_scale_probe <install> <event type> <location> [frames] [scale]\n"); return 2; }
    const int frames = argc > 4 ? atoi(argv[4]) : 2400;
    const float scale = argc > 5 ? (float)atof(argv[5]) : 1.0f;
    const float steer_sign = (float)env_int("RX_PROBE_STEER", 1);
    setvbuf(stdout, NULL, _IONBF, 0);

    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (scale != 1.0f && rx_session_set_map_scale(s, scale) != 0) { fprintf(stderr, "set_map_scale: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    snprintf(spec.event_type, sizeof spec.event_type, "%s", argv[2]);
    snprintf(spec.location, sizeof spec.location, "%s", argv[3]);
    const int freeride = !strcmp(argv[2], "FreeRide");
    spec.riders = env_int("RX_PROBE_RIDERS", freeride ? 1 : 12);
    spec.ai_difficulty = env_int("RX_PROBE_AI", 95);
    if (freeride) { spec.duration = 0; spec.duration_type[0] = 0; }
    else { spec.duration = env_int("RX_PROBE_LAPS", 2); snprintf(spec.duration_type, sizeof spec.duration_type, "Laps"); }
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "event_load: %s\n", rx_session_last_error(s)); return 1; }
    int lf = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && lf < 10000) { if (rx_session_advance(s) != 0) break; lf++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    printf("map scale %.3f (session %.3f), %s %s, %d load frames\n", scale, rx_session_map_scale(s), argv[2], argv[3], lf);

    rx_terrain_info t; memset(&t, 0, sizeof t);
    rx_session_terrain(s, &t);
    printf("terrain samples %u spacing %.3f x %.1f .. %.1f z %.1f .. %.1f\n", t.samples, t.spacing, t.min_x, t.max_x, t.min_z, t.max_z);

    rx_slot slots[16]; int ns = 0;
    static float pose[64 * 16];
    float start[16][3]; memset(start, 0, sizeof start);
    int start_ok = 0, started = 0;
    int below = 0; float min_clear = 1e9f; float line[3 * 16];
    float last[16][3], path[16]; memset(last, 0, sizeof last); memset(path, 0, sizeof path);
    rx_pad pad; memset(&pad, 0, sizeof pad);
    for (int f = 0; f < frames; f++) {
        rx_bike_state b; memset(&b, 0, sizeof b);
        rx_race_state r; memset(&r, 0, sizeof r);
        rx_session_bike(s, &b); rx_session_race(s, &r);
        const float px = b.position[0][0], py = b.position[0][1], pz = b.position[0][2];
        if (!started && (r.state == 1 || (freeride && f == 60))) {   /* the grid as the countdown starts */
            started = 1; ns = rx_session_slots(s, slots, 16); start_ok = ns > 0;
            for (int i = 0; i < ns; i++) {
                int n = rx_session_bike_pose_slot(s, slots[i].slot, pose, 64);
                if (n > 0) { start[i][0] = pose[12]; start[i][1] = pose[13]; start[i][2] = pose[14]; }
                int ok; float g = ground_at(s, &t, start[i][0], start[i][2], &ok);
                printf("start slot %d type %d pos %.2f %.2f %.2f ground %.2f%s\n", slots[i].slot, slots[i].type, start[i][0], start[i][1], start[i][2], g, ok ? "" : " (off terrain)");
                if (ok && (start[i][1] < g - 2.0f || start[i][1] > g + 6.0f)) start_ok = 0;
            }
            for (int i = 0; i < ns; i++) for (int j = i + 1; j < ns; j++) {
                float dx = start[i][0] - start[j][0], dz = start[i][2] - start[j][2];
                if (dx * dx + dz * dz < 1.0f) { printf("start slots %d and %d overlap\n", i, j); start_ok = 0; }
            }
        }
        float steer = 0.0f;
        int nl = freeride ? 0 : rx_session_race_line(s, line, 16);
        const float vx = b.velocity[0], vz = b.velocity[2], sp = sqrtf(vx * vx + vz * vz);
        if (nl >= 4 && sp > 3.0f) {
            float tx = line[3 * 3] - px, tz = line[3 * 3 + 2] - pz;
            float cross = (vx * tz - vz * tx) / (sp * sqrtf(tx * tx + tz * tz) + 1e-3f);
            steer = steer_sign * cross * 3.0f;
        } else if (freeride) steer = 0.3f * sinf((float)f * 0.01f);
        if (steer > 1) steer = 1; if (steer < -1) steer = -1;
        pad.right_trigger = 255; pad.left_trigger = 0;
        pad.lx = (int16_t)(steer * 32000.0f);
        rx_session_set_input(s, NULL, &pad, 1u);
        if (rx_session_advance(s) != 0) { printf("advance failed at %d: %s\n", f, rx_session_last_error(s)); break; }
        int ok; float g = ground_at(s, &t, px, pz, &ok);
        if (ok && f > 10) {
            float c = py - g;
            if (c < min_clear) min_clear = c;
            if (c < -3.0f) below++;
        }
        if (started && f % 30 == 0)   /* each rider's path length (sampled twice a second) */
            for (int i = 0; i < ns; i++) {
                if (rx_session_bike_pose_slot(s, slots[i].slot, pose, 64) <= 0) continue;
                if (last[i][0] != 0 || last[i][2] != 0) {
                    float dx = pose[12] - last[i][0], dz = pose[14] - last[i][2];
                    path[i] += sqrtf(dx * dx + dz * dz);
                }
                last[i][0] = pose[12]; last[i][1] = pose[13]; last[i][2] = pose[14];
            }
        if (f % 120 == 0)
            printf("f %5d state %d lap %d/%d prog %.3f place %d pos %.1f %.1f %.1f ground %.1f speed %.1f\n", f, r.state, r.laps_completed,
                   r.num_laps, r.lap_progress, r.place, px, py, pz, ok ? g : -1.0f, b.speed);
        if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("error: %s\n", rx_session_last_error(s)); break; }
    }
    rx_driver_result d[16]; int nd = rx_session_standings(s, d, 16);
    ns = rx_session_slots(s, slots, 16);
    int moved_ai = 0, ai = 0; float player_moved = 0; int player_laps = 0; float ai_path = 0;
    for (int i = 0; i < ns; i++) {
        int n = rx_session_bike_pose_slot(s, slots[i].slot, pose, 64);
        float dx = n > 0 ? pose[12] - start[i][0] : 0, dz = n > 0 ? pose[14] - start[i][2] : 0, dist = sqrtf(dx * dx + dz * dz);
        int laps = -1, place = -1;
        for (int k = 0; k < nd; k++) if (d[k].slot == slots[i].slot) { laps = d[k].laps_completed; place = d[k].place; }
        int ok; float g = ground_at(s, &t, pose[12], pose[14], &ok);
        printf("end slot %d type %d pos %.1f %.1f %.1f ground %.1f moved %.1f path %.1f laps %d place %d\n", slots[i].slot, slots[i].type, pose[12], pose[13], pose[14], g, dist, path[i], laps, place);
        if (slots[i].type == 1) { ai_path += path[i]; }
        if (slots[i].type == 1) { ai++; if (dist > 100.0f * scale) moved_ai++; }
        if (slots[i].local) { player_moved = dist; player_laps = laps; }
    }
    printf("MAPSCALE %.2f %s start_ok %d below_ground %d min_clearance %.2f moved_ai %d/%d ai_mean_path %.0f (%.2f x scale) player_moved %.1f laps %d\n",
           scale, argv[3], start_ok, below, min_clear, moved_ai, ai, ai ? ai_path / ai : 0.0f, ai ? ai_path / ai / scale : 0.0f, player_moved, player_laps);
    rx_session_destroy(s);
    return 0;
}
