/* alive_props_probe.c (w-fb-models) -- the physics props (bales, Tuff Blocks, tyre stacks, ... : rx_session_props) of an AI race, headless,
 * through the PRODUCT session API: where each one starts against the terrain, which ones the riders knock, and where those end up (above /
 * on / below the ground) over time. Playtester report: "bales / tough blocks don't move under the map properly on alive tracks". Any level
 * of any install (a Reflex track gives the reference). Not shipped with the game.
 *   alive_props_probe <install root> <type> <type id> <level> <location id> <class> <laps> <AI difficulty> [frames] [riders]
 * (the arguments of alive_race_test). Prints PROP lines for the props that moved and a RESULT line.
 * Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/alive_props_probe.c */
#include "rx_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXP 16384

static rx_terrain_info ti;
static struct rx_session* S;
/* the collision ground at world x / z (bilinear over the grid; NAN outside) */
static float ground(float x, float z) {
    if (ti.spacing <= 0) return NAN;
    float fx = (x - ti.min_x) / ti.spacing, fz = (ti.max_z - z) / ti.spacing;   /* grid row 0 = max z */
    int c = (int)floorf(fx), r = (int)floorf(fz);
    if (c < 0 || r < 0 || c + 1 >= (int)ti.samples || r + 1 >= (int)ti.samples) return NAN;
    float h[4];   /* rows r (h[0], h[1]) and r + 1 (h[2], h[3]) */
    if (rx_session_terrain_heights(S, h, c, r, 2, 2) != 0) return NAN;
    float tx = fx - c, tz = fz - r;
    float a = h[0] + (h[1] - h[0]) * tx, b = h[2] + (h[3] - h[2]) * tx;
    return a + (b - a) * tz;
}

struct track { int moved, traced; float max_dist; float x0, y0, z0, g0, min_rel, max_drop, last_rel, x, y, z; int below_frames; };

int main(int argc, char** argv) {
    if (argc < 9) { fprintf(stderr, "usage: alive_props_probe <install root> <type> <type id> <level> <location id> <class> <laps> <difficulty> [frames] [riders]\n"); return 2; }
    setvbuf(stdout, NULL, _IOLBF, 0);
    const int frames = argc > 9 ? atoi(argv[9]) : 6 * 60 * 60;
    const int riders = argc > 10 ? atoi(argv[10]) : 12;
    S = rx_session_create(argv[1]);
    if (!S) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("RX_MAP_SCALE_BASE") && rx_session_set_map_scale_base(S, (float)atof(getenv("RX_MAP_SCALE_BASE"))) != 0) { printf("RESULT base: %s\n", rx_session_last_error(S)); return 1; }
    if (getenv("RX_MAP_SCALE") && rx_session_set_map_scale(S, (float)atof(getenv("RX_MAP_SCALE"))) != 0) { printf("RESULT scale: %s\n", rx_session_last_error(S)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.track_id = 0; e.vehicle_class = atoi(argv[6]); e.duration = atoi(argv[7]); snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = atoi(argv[8]); e.event_activation[0] = 0;
    if (rx_session_event_load(S, &e) != 0) { printf("RESULT event load failed: %s\n", rx_session_last_error(S)); return 1; }
    int f = 0;
    while (rx_session_get_state(S) == RX_SESSION_LOADING && f < 3000) { if (rx_session_advance(S) != 0) break; f++; }
    if (rx_session_get_state(S) == RX_SESSION_ERROR) { printf("RESULT load error: %s\n", rx_session_last_error(S)); return 1; }
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    rx_prop* p = calloc(MAXP, sizeof *p); struct track* t = calloc(MAXP, sizeof *t);
    int n0 = -1, kinds[4] = {0}, bodies = 0, visible = 0;
    for (f = 0; f < frames; f++) {
        rx_session_set_input(S, keys, NULL, 0);
        if (rx_session_advance(S) != 0 || rx_session_get_state(S) == RX_SESSION_ERROR) { printf("RESULT error at frame %d: %s\n", f, rx_session_last_error(S)); return 1; }
        if (rx_session_get_state(S) < RX_SESSION_COUNTDOWN) continue;
        if (f % 6) continue;
        rx_session_terrain(S, &ti);
        int n = rx_session_props(S, p, MAXP); if (n > MAXP) n = MAXP;
        if (n0 < 0) {
            n0 = n;
            float relmin = 1e9f, relmax = -1e9f; int under = 0, nan = 0;
            for (int i = 0; i < n; i++) {
                struct track* q = &t[i];
                q->x0 = q->x = p[i].matrix[12]; q->y0 = q->y = p[i].matrix[13]; q->z0 = q->z = p[i].matrix[14];
                q->g0 = ground(q->x0, q->z0); q->min_rel = q->last_rel = q->y0 - q->g0;
                if (p[i].kind >= 0 && p[i].kind < 4) kinds[p[i].kind]++;
                bodies += (p[i].flags & 8) != 0; visible += (p[i].flags & 1) != 0;
                if (isnan(q->g0)) { nan++; continue; }
                if (q->last_rel < relmin) relmin = q->last_rel;
                if (q->last_rel > relmax) relmax = q->last_rel;
                if (q->last_rel < -0.5f) under++;
            }
            printf("frame %d: %d props (PhysicsEntity %d, PopUpBale %d, PopDownBale %d, StepUpBar %d), %d with a body, %d visible; "
                   "terrain grid %u x %.3f ft from (%.1f, %.1f); start height over the ground %.2f .. %.2f ft, %d more than 0.5 ft below, %d off the grid\n",
                   f, n, kinds[0], kinds[1], kinds[2], kinds[3], bodies, visible, ti.samples, ti.spacing, ti.min_x, ti.min_z, relmin, relmax, under, nan);
            for (int i = 0; i < n && i < 400; i++)
                if (getenv("PROBE_ALL") || t[i].last_rel < -0.5f)
                    printf("START %d id %d kind %d flags %u at (%.1f, %.2f, %.1f) ground %.2f rel %.2f axes %.3f %.3f %.3f\n", i, p[i].entity_id, p[i].kind, p[i].flags, t[i].x0, t[i].y0, t[i].z0, t[i].g0, t[i].last_rel,
                           sqrtf(p[i].matrix[0] * p[i].matrix[0] + p[i].matrix[1] * p[i].matrix[1] + p[i].matrix[2] * p[i].matrix[2]),
                           sqrtf(p[i].matrix[4] * p[i].matrix[4] + p[i].matrix[5] * p[i].matrix[5] + p[i].matrix[6] * p[i].matrix[6]),
                           sqrtf(p[i].matrix[8] * p[i].matrix[8] + p[i].matrix[9] * p[i].matrix[9] + p[i].matrix[10] * p[i].matrix[10]));
            continue;
        }
        for (int i = 0; i < n && i < n0; i++) {
            struct track* q = &t[i];
            float x = p[i].matrix[12], y = p[i].matrix[13], z = p[i].matrix[14];
            if (fabsf(x - q->x0) + fabsf(y - q->y0) + fabsf(z - q->z0) > 0.05f && !q->moved) {
                q->moved = 1;
                printf("frame %d: prop %d (id %d kind %d flags %u) moves from (%.1f, %.2f, %.1f), %.2f over the ground\n", f, i, p[i].entity_id, p[i].kind, p[i].flags, q->x0, q->y0, q->z0, q->y0 - q->g0);
            }
            if (!q->moved) continue;
            q->x = x; q->y = y; q->z = z;
            float g = ground(x, z), rel = y - g;
            if (getenv("PROBE_TRACE") && q->traced < atoi(getenv("PROBE_TRACE")))   /* PROBE_TRACE=N: N samples of each moved prop (every 6 frames) */
            { q->traced++; printf("TRACE %d f %d (%.2f, %.2f, %.2f) ground %.2f rel %.2f flags %u up %.2f |y| %.3f\n", i, f, x, y, z, g, rel, p[i].flags, p[i].matrix[5], sqrtf(p[i].matrix[4] * p[i].matrix[4] + p[i].matrix[5] * p[i].matrix[5] + p[i].matrix[6] * p[i].matrix[6])); }
            if (!isnan(rel)) { q->last_rel = rel; if (rel < q->min_rel) q->min_rel = rel; if (rel < -1.0f) q->below_frames++; }
            if (q->y0 - y > q->max_drop) q->max_drop = q->y0 - y;
            { float d = sqrtf((x - q->x0) * (x - q->x0) + (z - q->z0) * (z - q->z0)); if (d > q->max_dist) q->max_dist = d; }
        }
    }
    int moved = 0, sunk = 0, fell = 0;
    for (int i = 0; i < n0; i++) {
        struct track* q = &t[i];
        if (!q->moved) continue;
        moved++;
        int bad = q->last_rel < -1.0f || q->max_drop > 6.0f;
        sunk += q->last_rel < -1.0f; fell += q->max_drop > 6.0f;
        printf("PROP %d id %d kind %d: from (%.1f, %.2f, %.1f) [ground %.2f] to (%.1f, %.2f, %.1f) [ground %.2f]; height over the ground now %.2f, lowest %.2f ft, "
               "dropped %.1f ft below its start, pushed up to %.1f ft, %d samples more than 1 ft under%s\n", i, p[i].entity_id, p[i].kind, q->x0, q->y0, q->z0, q->g0, q->x, q->y, q->z,
               ground(q->x, q->z), q->last_rel, q->min_rel, q->max_drop, q->max_dist, q->below_frames, bad ? "  UNDER THE MAP" : "");
    }
    printf("RESULT %s %s: %d props, %d moved, %d end more than 1 ft under the ground, %d dropped more than 6 ft (%.0f s)\n", argv[2], argv[4], n0, moved, sunk, fell, f / 60.0);
    rx_session_destroy(S);
    return 0;
}
