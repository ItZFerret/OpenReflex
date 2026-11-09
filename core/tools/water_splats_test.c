/* water_splats_test.c -- the water wake's splat hand-off (include/rx_water.h, api/water_session.inc; w-water). Not shipped.
 *
 *   water_splats_test <game files dir or its Archive dir> <stillwater|alpine> [race frames (default 4800)] [hash out file]
 *
 * stillwater: a National at NAT_Stillwater, 12 riders, the player holding the throttle with a short steer every 4 s (corebench's pad);
 * the field seldom leaves the track for the river. alpine: FR_Alpine's Free Ride (one rider, spawned on the boat dock), the throttle
 * held and the steer of WST_STEER (a pad script "frame:keys,...", keys among T throttle, L, R, B brake; default "0:T") into the lake. After every frame it reads rx_session_water_splats(after the last frame read) and checks the contract:
 * whole frames, oldest first, increasing frame numbers, frame <= rx_session_frames, 30 floats per centre, finite vertices with the
 * Wake_AddSplatQuad / Wake_AddSplat uv set {0, 0.5, 1}, strengths in (0, 1], nothing dropped or truncated. Per 600 frames it prints
 * the splat frames / splats so far and the FNV hash of every rider's bike pose + the game's RNG draw count (corebench's hash):
 * the same hash on a core without the hand-off says the hand-off leaves the simulation alone. PASS needs splats on at least one
 * frame. Build (Mac; reads the water set from the arena, so it links the product objects):
 *   bash core/build.sh && clang -std=c11 -O1 -Icore/include -Icore/src -Icore/game -o water_splats_test core/tools/water_splats_test.c \
 *     $(sed 's|^|core/|' core/build/objs.list) -lz -lc++ */
#include "rx_api.h"
#include "../src/rx_mem.h"
#include "rx_rng.h"
#include "rx_water.h"
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv(uint64_t h, const void* p, size_t n) { const uint8_t* b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
static void pad(uint8_t keys[256], const rx_remap_table* r, int frame) {
    memset(keys, 0, 256);
    keys[r->keyboard[0] & 255] = 0x80;
    const int phase = frame % 240;
    if (phase >= 120 && phase < 150) keys[r->keyboard[2] & 255] = 0x80;
    if (phase >= 200 && phase < 220) keys[r->keyboard[3] & 255] = 0x80;
}
#define FAIL(...) do { printf("FAIL "); printf(__VA_ARGS__); printf("\n"); return 1; } while (0)

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: water_splats_test <game files dir> <stillwater|alpine> [race frames] [hash out]\n"); return 2; }
    const int alpine = strcmp(argv[2], "alpine") == 0;
    const int frames = argc > 3 ? atoi(argv[3]) : 4800;
    char root[4096]; snprintf(root, sizeof root, "%s", argv[1]);
    { size_t L = strlen(root); while (L && root[L - 1] == '/') root[--L] = 0; char probe[4200]; snprintf(probe, sizeof probe, "%s/Archive", root);
      FILE* f = fopen(probe, "r"); if (f) { fclose(f); snprintf(root, sizeof root, "%s", probe); } }
    FILE* hout = argc > 4 ? fopen(argv[4], "w") : NULL;
    struct rx_session* s = rx_session_create(root);
    if (!s) FAIL("create: %s", rx_session_last_error(NULL));
    rx_event_spec e; rx_session_event_default(&e);
    if (alpine) {
        snprintf(e.event_type, sizeof e.event_type, "FreeRide"); e.event_type_id = 9;
        snprintf(e.location, sizeof e.location, "FR_Alpine"); e.location_id = 45;
        e.track_id = 0; e.duration = 0; e.duration_type[0] = 0; e.riders = 1; e.ai_difficulty = 0; e.vehicle_class = -1; e.event_activation[0] = 0;
    } else {
        snprintf(e.event_type, sizeof e.event_type, "National"); e.event_type_id = 10;
        snprintf(e.location, sizeof e.location, "NAT_Stillwater"); e.location_id = 56;
        e.track_id = 0; e.vehicle_class = 2; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
        e.riders = 12; e.ai_difficulty = 95; e.event_activation[0] = 0;
    }
    if (rx_session_event_load(s, &e) != 0) FAIL("event load: %s", rx_session_last_error(s));
    int loadf = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && loadf < 3000) { if (rx_session_advance(s) != 0) break; loadf++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) FAIL("load: %s", rx_session_last_error(s));
    printf("loaded %s in %d frames\n", e.location, loadf);
    float tx = 0, tz = 0, plane = 0; int have_target = 0;
    if (alpine) {
        int pre = 0;
        while (rx_session_get_state(s) != RX_SESSION_RACING && pre < 20000) { if (rx_session_advance(s) != 0) FAIL("pre: %s", rx_session_last_error(s)); pre++; }
        printf("free ride InProgress after %d frames\n", pre);
        rx_bike_state b; if (rx_session_bike(s, &b) != 0) FAIL("bike");
        rx_terrain_info ti; memset(&ti, 0, sizeof ti);
        if (rx_session_terrain(s, &ti) != 0 || !ti.samples) FAIL("terrain");
        float* row = malloc(sizeof(float) * ti.samples); float best = 1e30f;
        for (rx_addr it = U32(0, 0x00d72aac + 0x10); it && it < U32(0, 0x00d72aac + 0x14); it += 4) {   /* the set (007efef0): +0x10 / +0x14 */
            const rx_addr w = U32(U32(it, 0), 0x54);   /* the surface's patch: +0xa0 plane point, +0xb0 box */
            const float x0 = F32(w, 0xb0), z0 = F32(w, 0xb4), x1 = F32(w, 0xb8), z1 = F32(w, 0xbc), y = F32(w, 0xa4);
            printf("water surface %08x box x %.0f..%.0f z %.0f..%.0f plane y %.1f\n", w, x0, x1, z0, z1, y);
            for (uint32_t r = 0; r < ti.samples; r += 4) {
                const float z = ti.max_z - (float)r * ti.spacing; if (z < z0 || z > z1) continue;
                rx_session_terrain_heights(s, row, 0, (int32_t)r, (int32_t)ti.samples, 1);
                for (uint32_t c2 = 0; c2 < ti.samples; c2 += 4) {
                    const float x = ti.min_x + (float)c2 * ti.spacing; if (x < x0 || x > x1 || row[c2] > y - 4.0f) continue;
                    const float d = (x - b.position[0][0]) * (x - b.position[0][0]) + (z - b.position[0][2]) * (z - b.position[0][2]);
                    if (d < best) { best = d; tx = x; tz = z; plane = y; have_target = 1; }
                }
            }
        }
        free(row);
        if (!have_target) FAIL("no water below the planes");
        printf("bike at (%.1f %.1f %.1f); water target (%.1f, %.1f) %.0f ft away, plane %.1f\n", b.position[0][0], b.position[0][1], b.position[0][2], tx, tz, sqrtf(best), plane);
    }

    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    uint8_t keys[256]; static float pose[50 * 16];
    static float v[5 * 0x5a0 / 5 * RX_WATER_SPLAT_FRAMES], c[2 * 0x30 * RX_WATER_SPLAT_FRAMES];
    uint64_t hash = 0xcbf29ce484222325ull, after = 0;
    long splat_frames = 0, splats = 0, verts = 0, max_frame_splats = 0, first = -1;
    for (int f = 1; f <= frames; f++) {
        if (alpine) {   /* the heading at the target, then on past it (60 ft beyond, along the line) */
            rx_bike_state b; rx_pad gp; memset(&gp, 0, sizeof gp); memset(keys, 0, sizeof keys);
            if (rx_session_bike(s, &b) == 0) {
                float gx = tx, gz = tz;
                const float dx = tx - b.position[0][0], dz = tz - b.position[0][2];
                if (dx * dx + dz * dz < 60.0f * 60.0f && b.speed > 3.0f) { gx = b.position[0][0] + b.velocity[0]; gz = b.position[0][2] + b.velocity[2]; }
                float err = 0.0f;
                if (b.speed > 3.0f) { err = atan2f(gx - b.position[0][0], gz - b.position[0][2]) - atan2f(b.velocity[0], b.velocity[2]);
                    while (err > 3.14159265f) err -= 6.2831853f; while (err < -3.14159265f) err += 6.2831853f; }
                float st = err * 2.5f; st = st > 1 ? 1 : st < -1 ? -1 : st;
                gp.lx = (int16_t)(st * 32000.0f); gp.right_trigger = (uint8_t)(fabsf(err) > 1.2f ? 120 : 220);
            }
            rx_session_set_input(s, keys, &gp, 1);
        } else {
            pad(keys, &remap, f);
            rx_session_set_input(s, keys, NULL, 0);
        }
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) FAIL("race frame %d: %s", f, rx_session_last_error(s));
        { int n = rx_session_bike_pose(s, pose, 50); if (n > 0) hash = fnv(hash, pose, (size_t)n * 16 * sizeof(float));
          for (int sl = 0; sl < 16; sl++) { n = rx_session_bike_pose_slot(s, sl, pose, 50); if (n > 0) hash = fnv(hash, pose, (size_t)n * 16 * sizeof(float)); } }
        rx_water_splats w; memset(&w, 0, sizeof w); w.size = sizeof w; w.version = RX_WATER_SPLATS_VERSION; w.after_frame = after; w.views = 1;
#ifndef WST_BASE   /* -DWST_BASE: built against a core without the hand-off (the hash comparison only) */
        if (rx_session_water_splats(s, &w, v, (uint32_t)(sizeof v / sizeof v[0] / 5), c, (uint32_t)(sizeof c / sizeof c[0] / 2)) != 0) FAIL("rx_session_water_splats rc");
#endif
        if (w.truncated || w.dropped) FAIL("frame %d: truncated %u dropped %u", f, w.truncated, w.dropped);
        if (w.views != 1) FAIL("views %u", w.views);
        const uint64_t now = rx_session_frames(s);
        uint32_t vo = 0, co = 0;
        for (uint32_t k = 0; k < w.frames; k++) {
            if (w.frame[k] <= after || w.frame[k] > now || (k && w.frame[k] <= w.frame[k - 1])) FAIL("frame order %llu after %llu now %llu", (unsigned long long)w.frame[k], (unsigned long long)after, (unsigned long long)now);
            const uint32_t fv = w.frame_vertices[k], fc = w.frame_centres[k];
            if (fv != 6 * fc || fc == 0) FAIL("frame %llu: %u vertices for %u centres", (unsigned long long)w.frame[k], fv, fc);
            for (uint32_t i = vo; i < vo + fv; i++) {
                const float* p = v + 5 * i;
                for (int j = 0; j < 5; j++) if (!isfinite(p[j])) FAIL("vertex %u not finite", i);
                if (!(p[2] > 0.0f && p[2] <= 1.0f)) FAIL("strength %g", p[2]);
                for (int j = 3; j < 5; j++) if (p[j] != 0.0f && p[j] != 0.5f && p[j] != 1.0f) FAIL("uv %g", p[j]);
                if (fabsf(p[0]) > 1.0f || fabsf(p[1]) > 1.0f) FAIL("offset (%g, %g) beyond the target", p[0], p[1]);
            }
            for (uint32_t i = co; i < co + fc; i++) if (!isfinite(c[2 * i]) || !isfinite(c[2 * i + 1])) FAIL("centre not finite");
            if (first < 0) { first = f; printf("first splats at race frame %d (session frame %llu): %u splats, centre (%.1f, %.1f), strength %.3f\n", f, (unsigned long long)w.frame[k], fc, c[2 * co], c[2 * co + 1], v[5 * vo + 2]); }
            vo += fv; co += fc; splat_frames++; splats += fc; verts += fv; if ((long)fc > max_frame_splats) max_frame_splats = fc;
        }
        if (vo != w.vertices || co != w.centres) FAIL("totals");
        after = w.last_frame;
        if (alpine && f % 60 == 0) { rx_bike_state b; if (rx_session_bike(s, &b) == 0) printf("frame %5d bike (%.1f %.1f %.1f) speed %.1f, %.0f ft from the target, splats %ld\n", f, b.position[0][0], b.position[0][1], b.position[0][2], b.speed, sqrtf((tx - b.position[0][0]) * (tx - b.position[0][0]) + (tz - b.position[0][2]) * (tz - b.position[0][2])), splats); }
        if (f % 600 == 0) {
            rx_rng_state g; memset(&g, 0, sizeof g); g.size = sizeof g; g.version = 1; if (rx_session_rng(s, &g) == 0) hash = fnv(hash, &g.game_count, sizeof g.game_count);
            printf("race %5d: splat frames %ld splats %ld (max %ld per frame) hash %016llx\n", f, splat_frames, splats, max_frame_splats, (unsigned long long)hash);
            if (hout) fprintf(hout, "%d %016llx\n", f, (unsigned long long)hash);
        }
    }
    rx_session_destroy(s);
    if (hout) fclose(hout);
#ifndef WST_BASE
    if (!splat_frames) FAIL("no splats in %d race frames", frames);
#endif
    printf("PASS water splats: %ld frames with splats, %ld splats, %ld vertices, max %ld per frame\n", splat_frames, splats, verts, max_frame_splats);
    return 0;
}
