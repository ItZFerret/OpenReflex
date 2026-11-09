/* lap_hash_trace.c (w-lb-practice) -- a lap race ridden to its finish through the PRODUCT session API with a hash trace, to prove a change
 * leaves the races byte-identical (build it against two cores and diff the output; freeride_hash_trace.c's hashes, with the race-line
 * autopilot of career_series_test.c on pad 0 instead of the fuzz keys, so the local rider crosses the line and finishes).
 *   lap_hash_trace <install root> <type> <type id> <level> <location id> <riders> <ai> <laps> <frames>
 * Output: one line per frame with an FNV-1a 64 of the local vehicle state (rx_session_bike) and the race state (rx_session_race), every
 * 100 frames a hash of the game's whole data image (0x00400000 .. 0x00e10000), and a final line with the running hash of every line. Only
 * calls every core has (no PRACTICE / lap log calls), so the same source builds against main. Static link with the product objects:
 *   C=core O=<out>; source core/tools/product_link.sh; rx_product_objs; rx_link_driver <out exe> core/tools/lap_hash_trace.c */
#include "rx_api.h"
#include "rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint64_t fnv(uint64_t h, const void* p, size_t n) {
    const uint8_t* b = (const uint8_t*)p;
    for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; }
    return h;
}

/* ---- the race-line autopilot (career_series_test.c 101-140) ------------------------------------------------------------------------- */
static float wrap_angle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
struct autopilot { float stuck_clock, reverse_clock; };
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
    const float desired = turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f;
    if (speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f; else pilot->stuck_clock = 0.0f;
    if (pilot->stuck_clock > 4.0f) { pilot->reverse_clock = 1.5f; pilot->stuck_clock = 0.0f; }
    if (pilot->reverse_clock > 0.0f) { pilot->reverse_clock -= 1.0f / 60.0f; pad->left_trigger = 255; pad->lx = (int16_t)(-steer * 32000.0f); }
    else {
        pad->lx = (int16_t)(steer * 32000.0f);
        if (speed < desired) pad->right_trigger = 255; else if (speed > desired + 6.0f) pad->left_trigger = 160; else pad->right_trigger = 120;
    }
}

int main(int argc, char** argv) {
    if (argc < 10) { fprintf(stderr, "usage: lap_hash_trace <install> <type> <type id> <level> <location id> <riders> <ai> <laps> <frames>\n"); return 2; }
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.riders = atoi(argv[6]); e.ai_difficulty = atoi(argv[7]); e.duration = atoi(argv[8]); snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    const int frames = atoi(argv[9]);
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    struct autopilot pilot = {0, 0};
    uint64_t all = 0xcbf29ce484222325ull;
    int f = 0, racing = 0;
    for (int i = 0; i < frames && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        rx_pad pad; memset(&pad, 0, sizeof pad);
        if (racing) autopilot_pad(s, &pilot, &pad);
        rx_session_set_input(s, keys, &pad, 1);
        if (rx_session_advance(s) != 0) break;
        f++;
        rx_bike_state b; memset(&b, 0, sizeof b);
        const int brc = rx_session_bike(s, &b);
        rx_race_state r; memset(&r, 0, sizeof r);
        rx_session_race(s, &r);
        if (r.valid && r.state == 2) racing = 1;
        char line[200];
        snprintf(line, sizeof line, "F %d bike %d %016llx race %016llx st %d laps %d/%d pos %.3f %.3f %.3f", f, brc, (unsigned long long)fnv(0xcbf29ce484222325ull, &b, sizeof b),
                 (unsigned long long)fnv(0xcbf29ce484222325ull, &r, sizeof r), r.state, r.laps_completed, r.num_laps, b.position[0][0], b.position[0][1], b.position[0][2]);
        if (f % 100 == 0) {
            const uint64_t g = fnv(0xcbf29ce484222325ull, rx_base + 0x00400000u, 0x00e10000u - 0x00400000u);
            snprintf(line + strlen(line), sizeof line - strlen(line), " image %016llx", (unsigned long long)g);
        }
        all = fnv(all, line, strlen(line));
        puts(line);
        if (rx_session_get_state(s) == RX_SESSION_FINISHED && racing) { printf("FINISHED at frame %d\n", f); break; }
    }
    printf("END frames %d state %d trace %016llx\n", f, (int)rx_session_get_state(s), (unsigned long long)all);
    rx_session_destroy(s);
    return 0;
}
