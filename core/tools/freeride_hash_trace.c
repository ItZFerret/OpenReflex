/* freeride_hash_trace.c -- w-freerideall: a deterministic Free Ride ride through the PRODUCT session API with a hash trace, to prove a
 * change leaves a level's Free Ride byte-identical (build it against two cores and diff the output).
 *   freeride_hash_trace <install root> <level> <location id> <frames> <seed> [profile cells file]
 * The ride: the stop_audit_run fuzz pad (rx_remap_defaults keyboard bindings: throttle most of the time, brake, steering, the trick
 * modifier and the reflex keys at random every 15-55 frames) plus taps of SetResetPoint / Reset / UseResetPoint (columns 11 / 12 / 13,
 * 3 frames every 90, in turn). Output: one line per frame with an FNV-1a 64 of the local vehicle state (rx_session_bike), every 100
 * frames a hash of the game's whole data image (0x00400000 .. 0x00e10000: the executable's sections, the globals), and a final line
 * with the running hash of every line. Static link with the product objects (it reads guest memory through rx_mem.h):
 *   C=core O=<out>; source core/tools/product_link.sh; rx_product_objs; rx_link_driver <out exe> core/tools/freeride_hash_trace.c
 * (w-frnowalls) env FRH_EVENT / FRH_BORDERS (below; FRH_BORDERS only when rx_api.h has
 * rx_session_set_free_ride_borders). */
#include "rx_api.h"
#include "rx_mem.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv(uint64_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

int main(int argc, char** argv) {
    if (argc < 6) { fprintf(stderr, "usage: freeride_hash_trace <install> <level> <location id> <frames> <seed> [profile]\n"); return 2; }
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (argc > 6) {
        FILE* pf = fopen(argv[6], "rb");
        if (!pf) return 1;
        fseek(pf, 0, SEEK_END); long pb = ftell(pf); rewind(pf);
        rx_profile_cell* cells = malloc((size_t)pb);
        if (!cells || fread(cells, 1, (size_t)pb, pf) != (size_t)pb) return 1;
        fclose(pf);
        if (rx_session_profile_import(s, cells, (int)(pb / (long)sizeof *cells)) != 0) { fprintf(stderr, "profile: %s\n", rx_session_last_error(s)); return 1; }
        free(cells);
    }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "%s", argv[2]);
    e.location_id = atoi(argv[3]);
    e.track_id = 0; e.vehicle_class = -1; e.duration = 0; e.duration_type[0] = 0; e.riders = 1; e.ai_difficulty = 0; e.event_activation[0] = 0;
    /* env FRH_EVENT="<type>,<type id>,<duration>,<duration type>,<riders>,<ai>" (w-frnowalls): another event than Free Ride (a race, to show
     * an option leaves it unchanged); env FRH_BORDERS=<mode>: rx_session_set_free_ride_borders before the load (only with a core that has it) */
    if (getenv("FRH_EVENT")) {
        char t[32] = "", dt[16] = ""; int id = 0, dur = 0, riders = 1, ai = 0;
        if (sscanf(getenv("FRH_EVENT"), "%31[^,],%d,%d,%15[^,],%d,%d", t, &id, &dur, dt, &riders, &ai) >= 2) {
            snprintf(e.event_type, sizeof e.event_type, "%s", t); e.event_type_id = id; e.duration = dur;
            snprintf(e.duration_type, sizeof e.duration_type, "%s", dt); e.riders = riders; e.ai_difficulty = ai;
        }
    }
#ifdef RX_FREE_RIDE_BORDERS_OPEN
    if (getenv("FRH_BORDERS") && rx_session_set_free_ride_borders(s, atoi(getenv("FRH_BORDERS"))) != 0) { fprintf(stderr, "borders: %s\n", rx_session_last_error(s)); return 1; }
#endif
    const int frames = atoi(argv[4]);
    unsigned seed = (unsigned)atoi(argv[5]);
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f < 3000) { if (rx_session_advance(s) != 0) break; f++; }
    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    int dik_of[256]; memset(dik_of, 0, sizeof dik_of);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
#define KB(col) (dik_of[remap.keyboard[(col)] & 255] & 255)
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    uint64_t all = 0xcbf29ce484222325ull;
    int hold = 0;
    static const int pulse[3] = {11, 12, 13};
    for (int i = 0; i < frames && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
        if (hold-- <= 0) {
            hold = 15 + (int)(rand_r(&seed) % 40);
            memset(keys, 0, sizeof keys);
            if (rand_r(&seed) % 10 != 0) keys[KB(0)] = 0x80;
            if (rand_r(&seed) % 12 == 0) keys[KB(1)] = 0x80;
            { unsigned r = rand_r(&seed) % 4; if (r == 0) keys[KB(2)] = 0x80; else if (r == 1) keys[KB(3)] = 0x80; }
            if (rand_r(&seed) % 2 == 0) keys[KB(4)] = 0x80;
            for (int k = 6; k < 10; k++) if (rand_r(&seed) % 4 == 0) keys[KB(k)] = 0x80;
        }
        for (int k = 0; k < 3; k++) keys[KB(pulse[k])] = 0;
        if (i % 90 < 3) keys[KB(pulse[(i / 90) % 3])] = 0x80;
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s) != 0) break;
        f++;
        rx_bike_state b; memset(&b, 0, sizeof b);
        const int brc = rx_session_bike(s, &b);
        char line[160];
        snprintf(line, sizeof line, "F %d bike %d %016llx pos %.3f %.3f %.3f", f, brc, (unsigned long long)fnv(0xcbf29ce484222325ull, &b, sizeof b),
                 b.position[0][0], b.position[0][1], b.position[0][2]);
        if (f % 100 == 0) {
            const uint64_t g = fnv(0xcbf29ce484222325ull, rx_base + 0x00400000u, 0x00e10000u - 0x00400000u);
            snprintf(line + strlen(line), sizeof line - strlen(line), " image %016llx", (unsigned long long)g);
        }
        all = fnv(all, line, strlen(line));
        puts(line);
    }
    printf("END frames %d state %d trace %016llx\n", f, (int)rx_session_get_state(s), (unsigned long long)all);
    rx_session_destroy(s);
    return 0;
}
