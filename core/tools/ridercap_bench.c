/* ridercap_bench.c (w-ridercap) -- the core's cost per rider, headless, through the PRODUCT session API (core/plan/RIDER_CAP.md).
 * Not shipped with the game.
 *   ridercap_bench <install root> <nat|renne|sx> <riders> [frames]
 *     nat = National NAT_Concord, renne = National NAT_Renne, sx = Supercross SX_Kingston; MX 250, 3 laps, AI 95
 *     riders: EventRiders (the player + AI); frames: racing frames timed from GO (default 1800 = 30 s)
 * The event runs untimed through its Pre and Countdown (the session leaves LOADING after a few frames, but the activity stays in Pre
 * for ~830 frames and counts down ~350 more on NAT_Concord) to GO (the activity's InProgress), then `frames` frames are timed. The
 * player holds the throttle with corebench's pad script (a short steer every 4 s). Reported per frame of rx_session_advance: the main
 * thread's CPU ms and the wall ms (RX_JOB_THREADS as set; RX_JOB_THREADS=0 puts all of the core's work on this thread), split into
 * the first 300 frames from GO (the gate drop and the pack's first 5 s: everyone within a few bike lengths) and the rest; the slowest
 * frame; how many riders the core built (player slots with a vehicle, standings rows); a hash of every slot's bike pose and the game
 * RNG count (the same build and riders always print the same hash, in every RX_JOB_THREADS mode).
 * One RESULT line. Up to 32 riders (w-rc-core: riders 13..32 in game/rider_cap.h side storage; before it the core stopped at the load).
 * Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/ridercap_bench.c */
#include "rx_api.h"
#include "rx_rng.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAXS 64
#define START_FRAMES 300

static double wall_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static double cpu_ms(void) { struct timespec t; clock_gettime(CLOCK_THREAD_CPUTIME_ID, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }
static uint64_t fnv(uint64_t h, const void* p, size_t n) { const uint8_t* b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
static void pad(uint8_t keys[256], const rx_remap_table* r, int frame) {   /* corebench's pad script */
    memset(keys, 0, 256);
    keys[r->keyboard[0] & 255] = 0x80;
    const int phase = frame % 240;
    if (phase >= 120 && phase < 150) keys[r->keyboard[2] & 255] = 0x80;
    if (phase >= 200 && phase < 220) keys[r->keyboard[3] & 255] = 0x80;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: ridercap_bench <install root> <nat|renne|sx> <riders> [frames]\n"); return 2; }
    const int riders = atoi(argv[3]);
    const int frames = argc > 4 ? atoi(argv[4]) : 1800;
    if (frames <= START_FRAMES) { fprintf(stderr, "frames must be over %d\n", START_FRAMES); return 2; }
    rx_event_spec e; rx_session_event_default(&e);
    if (strcmp(argv[2], "nat") == 0) { snprintf(e.event_type, sizeof e.event_type, "National"); e.event_type_id = 10; snprintf(e.location, sizeof e.location, "NAT_Concord"); e.location_id = 50; }
    else if (strcmp(argv[2], "renne") == 0) { snprintf(e.event_type, sizeof e.event_type, "National"); e.event_type_id = 10; snprintf(e.location, sizeof e.location, "NAT_Renne"); e.location_id = 54; }
    else if (strcmp(argv[2], "sx") == 0) { snprintf(e.event_type, sizeof e.event_type, "Supercross"); e.event_type_id = 8; snprintf(e.location, sizeof e.location, "SX_Kingston"); e.location_id = 74; }
    else { fprintf(stderr, "track: nat, renne or sx\n"); return 2; }
    e.track_id = 0; e.vehicle_class = 2; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = 95; e.event_activation[0] = 0;
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT riders %d event load failed: %s\n", riders, rx_session_last_error(s)); return 1; }
    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    uint8_t keys[256];
    static float pose[64 * 16];
    rx_slot sl[MAXS]; rx_driver_result st[MAXS];
    uint64_t hash = 0xcbf29ce484222325ull;
    double c_start = 0, c_rest = 0, w_start = 0, w_rest = 0, wmax = 0;
    int f = 0, go = 0, nslots = 0, nstand = 0;
    for (;;) {   /* untimed: the load, Pre and the countdown, to GO */
        pad(keys, &remap, f);
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT riders %d error before GO, frame %d: %s\n", riders, f, rx_session_last_error(s)); return 1; }
        f++;
        rx_race_state rs;
        if (rx_session_race(s, &rs) == 0 && rs.valid && rs.state == 2) { go = f; break; }
        if (f > 6000) { printf("RESULT riders %d no GO in 6000 frames\n", riders); return 1; }
    }
    for (int i = 0; i < frames; i++, f++) {
        pad(keys, &remap, f);
        rx_session_set_input(s, keys, NULL, 0);
        const double c = cpu_ms(), t = wall_ms();
        const int rc = rx_session_advance(s);
        const double dw = wall_ms() - t, dc = cpu_ms() - c;
        if (i < START_FRAMES) { w_start += dw; c_start += dc; } else { w_rest += dw; c_rest += dc; }
        if (dw > wmax) wmax = dw;
        if (rc != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT riders %d error at frame %d: %s\n", riders, f, rx_session_last_error(s)); return 1; }
        const int n = rx_session_slots(s, sl, MAXS);
        if (n > nslots) nslots = n;
        for (int k = 0; k < n; k++) { const int m = rx_session_bike_pose_slot(s, sl[k].slot, pose, 64); if (m > 0) hash = fnv(hash, pose, (size_t)m * 16 * sizeof(float)); }
        const int m = rx_session_standings(s, st, MAXS);
        if (m > nstand) nstand = m;
    }
    rx_rng_state g; memset(&g, 0, sizeof g); g.size = sizeof g; if (rx_session_rng(s, &g) == 0) hash = fnv(hash, &g.game_count, sizeof g.game_count);
    const int rest = frames - START_FRAMES;
    printf("RESULT riders %d slots %d standings %d go %d start_cpu %.3f start_wall %.3f race_cpu %.3f race_wall %.3f max_ms %.3f hash %016llx\n",
           riders, nslots, nstand, go, c_start / START_FRAMES, w_start / START_FRAMES, c_rest / rest, w_rest / rest, wmax, (unsigned long long)hash);
    rx_session_destroy(s);
    return 0;
}
