/* alive_race_test.c (w-aliveai) -- an AI race on any installed level, headless, through the PRODUCT session API: lap times, crashes and
 * stuck riders of every computer rider (core/plan/ALIVE_RACING.md). Written for the converted MX vs ATV Alive tracks; it runs any race of any
 * install (the Reflex levels give the reference numbers). Not shipped with the game.
 *   alive_race_test <install root> <type> <type id> <level> <location id> <class> <laps> <AI difficulty> [frames] [riders]
 *     e.g. alive_race_test builds/alive/NAT_Farm/map-a6-race National 10 NAT_Farm -1 2 3 95
 *          alive_race_test "game files/Archive" National 10 NAT_Concord 50 2 3 95
 * The player stays at the gate with no input (no human lap: the AI's pace is its own); the race runs until every computer rider finished
 * or `frames` (default 30 minutes of game time at 60 fps) passed. Per computer rider: place, laps, race time, best lap, every lap time, the
 * crashes (rising edges of the vehicle's wreck state V+0x280), the time spent wrecked, and the longest stall (continuous time under 5 ft/s
 * after the start, not finished). Then a RESULT line: finishers, winner, median, mean best lap, crashes per rider, the longest stall.
 * RX_RACE_MOUNT=<folder> (w-fb-customtrack): the level is a custom track's files in that folder (Mods/Tracks), as the client mounts it.
 * RX_MAP_SCALE=<s> / RX_MAP_SCALE_BASE=<b> (w-alivegates): the race at a MAP SCALE and its track's default (the Alive defaults: 1.1 / 1.3).
 * Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/alive_race_test.c */
#include "rx_api.h"
#include "rx_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXS 40   /* (w-rc-core) up to 32 riders */
#define MAXLAPS 32

static int cmpf(const void* a, const void* b) { float x = *(const float*)a, y = *(const float*)b; return x < y ? -1 : x > y; }

struct rider {
    int seen, laps, crashes, wrecked_prev, finished;
    float laptime[MAXLAPS], wrecked_s, stall_s, stall_max_s, stall_x, stall_z;
};

int main(int argc, char** argv) {
    if (argc < 9) {
        fprintf(stderr, "usage: alive_race_test <install root> <type> <type id> <level> <location id> <class> <laps> <difficulty> [frames] [riders]\n");
        return 2;
    }
    setvbuf(stdout, NULL, _IOLBF, 0);   /* (w-rc-core) line-buffered: a killed run keeps its lines */
    const int frames = argc > 9 ? atoi(argv[9]) : 30 * 60 * 60;
    const int riders = argc > 10 ? atoi(argv[10]) : 12;
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    /* (w-alivegates) RX_MAP_SCALE_BASE / RX_MAP_SCALE: the track's default scale and the scale (an Alive National at its default: 1.1 / 1.1) */
    if (getenv("RX_MAP_SCALE_BASE") && rx_session_set_map_scale_base(s, (float)atof(getenv("RX_MAP_SCALE_BASE"))) != 0) { printf("RESULT base: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("RX_MAP_SCALE") && rx_session_set_map_scale(s, (float)atof(getenv("RX_MAP_SCALE"))) != 0) { printf("RESULT scale: %s\n", rx_session_last_error(s)); return 1; }
    /* (w-fb-customtrack) RX_RACE_MOUNT=<folder>: the level is a custom track read from that folder (Mods/Tracks: rx_session_mount_level) */
    if (getenv("RX_RACE_MOUNT") && *getenv("RX_RACE_MOUNT") && rx_session_mount_level(s, argv[4], getenv("RX_RACE_MOUNT")) != 0) { printf("RESULT mount: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.track_id = 0; e.vehicle_class = atoi(argv[6]); e.duration = atoi(argv[7]); snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = atoi(argv[8]); e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT event load failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f < 3000) { if (rx_session_advance(s) != 0) break; f++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT load error: %s\n", rx_session_last_error(s)); return 1; }
    printf("loaded after %d frames\n", f);
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    rx_driver_result r[MAXS]; rx_slot sl[MAXS];
    struct rider rd[MAXS]; memset(rd, 0, sizeof rd);
    const float dt = 1.0f / 60.0f;
    int started = 0, placed = 0, n = 0;
    for (f = 0; f < frames; f++) {
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) {
            printf("RESULT error at frame %d: %s\n", f, rx_session_last_error(s));
            return 1;
        }
        n = rx_session_standings(s, r, MAXS);
        const int ns = rx_session_slots(s, sl, MAXS);
        if (!placed && ns == riders) {   /* where every rider waits at the gate (Alive's 12-slot gates: build_activity_grid.c) */
            int all = 1;
            for (int j = 0; j < ns; j++) all &= sl[j].vehicle != 0 && F32(sl[j].vehicle, 0xc4) != 0.0f;
            if (all) {
                placed = 1;
                for (int j = 0; j < ns; j++)
                    printf("frame %d: gate slot %2d %s at (%.1f, %.1f, %.1f)\n", f, sl[j].slot, sl[j].local ? "player" : "AI    ",
                           F32(sl[j].vehicle, 0xc0), F32(sl[j].vehicle, 0xc4), F32(sl[j].vehicle, 0xc8));
            }
        }
        for (int i = 0; i < n; i++) {
            const int k = r[i].slot;
            if (k < 0 || k >= MAXS || r[i].local) continue;
            struct rider* q = &rd[k];
            q->seen = 1;
            if (r[i].laps_completed > q->laps && r[i].laps_completed <= MAXLAPS) {
                q->laptime[r[i].laps_completed - 1] = r[i].last_lap;
                q->laps = r[i].laps_completed;
            }
            if (r[i].race_time > 0.0f) started = 1;
            if (r[i].finished) q->finished = 1;
            uint32_t v = 0;
            for (int j = 0; j < ns; j++) if (sl[j].slot == k) v = sl[j].vehicle;
            if (!v || !started || q->finished) continue;
            const int wrecked = U32(v, 0x280) != 0;
            if (wrecked && !q->wrecked_prev) q->crashes++;
            q->wrecked_prev = wrecked;
            if (wrecked) q->wrecked_s += dt;
            if (F32(v, 0x150) < 5.0f) {
                if (q->stall_s == 0.0f) { q->stall_x = F32(v, 0xc0); q->stall_z = F32(v, 0xc8); }
                q->stall_s += dt;
                if (q->stall_s > q->stall_max_s) {
                    q->stall_max_s = q->stall_s;
                    if (q->stall_s > 10.0f && (int)(q->stall_s * 60.0f) % 600 == 0)
                        printf("frame %d: slot %d stalled %.0f s near (%.0f, %.0f)\n", f, k, q->stall_s, q->stall_x, q->stall_z);
                }
            } else q->stall_s = 0.0f;
        }
        if (f % 60) continue;
        int ai = 0, done = 0;
        for (int i = 0; i < n; i++) if (!r[i].local) { ai++; done += r[i].finished != 0; }
        if (ai > 0 && done == ai) break;
    }
    n = rx_session_standings(s, r, MAXS);
    float times[MAXS], bestsum = 0.0f, stall_max = 0.0f; int fin = 0, bests = 0, crashes = 0, ais = 0;
    for (int i = 0; i < n; i++) {
        if (r[i].local) continue;
        const struct rider* q = &rd[r[i].slot];
        ais++;
        printf("slot %2d place %2d laps %d %s time %8.3f best %8.3f crashes %d wrecked %5.1f s stall %5.1f s laps:", r[i].slot, r[i].place,
               r[i].laps_completed, r[i].finished ? "finished" : "racing  ", r[i].race_time, r[i].best_lap < 1e30f ? r[i].best_lap : 0.0f,
               q->crashes, q->wrecked_s, q->stall_max_s);
        for (int l = 0; l < q->laps && l < MAXLAPS; l++) printf(" %.2f", q->laptime[l]);
        printf("\n");
        if (r[i].finished) times[fin++] = r[i].race_time;
        if (r[i].best_lap > 0.0f && r[i].best_lap < 1e30f) { bestsum += r[i].best_lap; bests++; }
        crashes += q->crashes;
        if (q->stall_max_s > stall_max) stall_max = q->stall_max_s;
    }
    qsort(times, (size_t)fin, sizeof times[0], cmpf);
    printf("RESULT %s %s laps %s difficulty %s: %d / %d finished in %.1f s, winner %.3f, median %.3f, mean best lap %.3f, crashes %.2f per rider, "
           "longest stall %.1f s\n", argv[2], argv[4], argv[7], argv[8], fin, ais, f / 60.0, fin ? times[0] : 0.0f, fin ? times[fin / 2] : 0.0f,
           bests ? bestsum / (float)bests : 0.0f, ais ? (float)crashes / (float)ais : 0.0f, stall_max);
    rx_session_destroy(s);
    return 0;
}
