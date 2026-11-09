/* activation_native_test.c -- the Free Ride activation areas run by the product library, no emulator (core/game/race_activation.c).
 *   activation_native_test <install root> <move|drive|plan> [activity (HillClimb)] [challenge frames (3000)]
 *   build: core/build.sh, then clang -std=c11 -O1 -Iinclude -Isrc -Igame -o <out> tools/activation_native_test.c build/obj/*.o -lz -lc++
 *
 * 1. The Race & Ride challenges Free Ride of FR_Alpine (FreeRide, EventLocationId 45, InCareerMode through
 *    rx_session_event_in_career_mode): the load, the activity's discovery mode (A + 0xff180), the level's StartActivity triggers.
 * 2. The player gets into the activity's area:
 *      move   the area's trigger is moved onto the bike (its own matrix setter, MXTrigger::vf16 00409b70): the game's trigger update
 *             (0040a8a0) then posts "VehicleEntered" and the UI manager's handler (005a3b60) marks the area; moved back, "VehicleExited"
 *             (0040a980 -> 005a3d00) clears it.
 *      drive  an autopilot rides there (pad throttle, steering toward the area from the bike's heading; env ROUTE="x,z;x,z;..." waypoints
 *             first), out of the area and back into it.
 *      plan   prints a ROUTE to the area over the collision terrain (Dijkstra on a coarse height grid, steep slopes refused) and stops.
 *    Each step is read through rx_session_activation (the API the product's host uses).
 * 3. The pause in the area (MXUIManager_OnPauseRequest 00598e40: EventTypeActivation = the area's activity, TrackId = its track):
 *    a new session with the Free Ride spec, event_activation and track_id from the snapshot; its activity class and states.
 * 4. EXIT (RealWorldSwitchToFreeRide): the level's Free Ride again.
 * Linked with the product library's objects (core/build.sh <dir>; the objects in <dir>/obj). Exit 0 = every step as expected. */
#include "rx_api.h"
#include "../game/rx_activation_api.h"
#include "../src/rx_mem.h"
#include "../src/rx_fn.h"
#include "../game/build_alloc.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <execinfo.h>

static const char* g_step = "start";
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "activation_native_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    void* bt[40];
    const int m = backtrace(bt, 40);
    backtrace_symbols_fd(bt, m, 2);
    _exit(128 + sig);
}

static int fail(const char* what, rx_session* s) {
    fprintf(stderr, "activation_native_test: FAIL %s%s%s\n", what, s ? ": " : "", s ? rx_session_last_error(s) : "");
    return 1;
}

/* the local player's activity (the game's player slots, as the API's local_player) */
static rx_addr local_activity(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return 0;
    const uint32_t n = U8(g, 0x1541c);
    for (uint32_t i = 0; i < n && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) == 0) return U32(p, 0x94);
    }
    return 0;
}
static void read_activation(rx_session* s, rx_activation_state* a) {
    memset(a, 0, sizeof *a);
    a->size = sizeof *a;
    a->version = RX_ACTIVATION_API_VERSION;
    if (rx_session_activation(s, a) != 0) memset(a, 0, sizeof *a);
}

/* a session of the challenges Free Ride (activation "" ) or of a challenge started from it */
static rx_session* start_session(const char* install, const char* activation, int32_t track) {
    rx_session* s = rx_session_create(install);
    if (!s) { fprintf(stderr, "activation_native_test: create: %s\n", rx_session_last_error(NULL)); return NULL; }
    if (rx_session_event_in_career_mode(s, 1) != 0) { fail("in_career_mode", s); return NULL; }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "FR_Alpine");
    e.location_id = 45;
    e.track_id = track;
    e.duration = 0;
    e.duration_type[0] = 0;
    e.riders = 1;
    e.ai_difficulty = 0;
    e.vehicle_class = -1;
    snprintf(e.event_activation, sizeof e.event_activation, "%s", activation);
    if (rx_session_event_load(s, &e) != 0) { fail("event load", s); return NULL; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 10000) {
        if (rx_session_advance(s) != 0) break;
        frames++;
    }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { fail("load frames", s); return NULL; }
    printf("activation_native_test: \"%s\" track %d loaded in %d frames\n", activation, track, frames);
    return s;
}

/* frames until the local activity is InProgress (A + 0x21c == 2) */
static int run_to_in_progress(rx_session* s, int max) {
    for (int i = 0; i < max; i++) {
        const rx_addr A = local_activity();
        if (A && U32(A, 0x21c) == 2) return i;
        if (rx_session_advance(s) != 0) return -1;
    }
    return -1;
}

static float bike_heading_error(const rx_bike_state* b, float tx, float tz) {
    const float vx = b->velocity[0], vz = b->velocity[2];
    const float dx = tx - b->position[0][0], dz = tz - b->position[0][2];
    const float want = atan2f(dx, dz), have = atan2f(vx, vz);
    float d = want - have;
    while (d > 3.14159265f) d -= 6.2831853f;
    while (d < -3.14159265f) d += 6.2831853f;
    return d;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: activation_native_test <install root> <move|drive> [activity] [challenge frames]\n"); return 2; }
    const char* install = argv[1];
    const int drive = strcmp(argv[2], "drive") == 0;
    const char* want = argc > 3 ? argv[3] : "HillClimb";
    const int challenge_frames = argc > 4 ? atoi(argv[4]) : 3000;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(1800);
    setvbuf(stdout, NULL, _IONBF, 0);

    /* ---- 1. the challenges Free Ride ---- */
    g_step = "free ride";
    rx_session* s = start_session(install, "", 0);
    if (!s) return 1;
    const int pre = run_to_in_progress(s, 20000);
    if (pre < 0) return fail("free ride never InProgress", s);
    const rx_addr A = local_activity();
    printf("activation_native_test: free ride activity %08x vtable %08x InProgress after %d frames, discovery mode %u, collectables %u found %u\n",
           A, U32(A, 0), pre, U32(A, 0xff180), U32(A, 0xff080), U32(A, 0xff104));
    if (U32(A, 0) != 0x0094329cu || U32(A, 0xff180) != 1) return fail("not the Free Ride in discovery mode", NULL);
    rx_addr trigger = 0;
    for (uint32_t k = 0; k < 0x18; k++) {
        const rx_addr t = U32(A, 0xff190 + 4 * k);
        if (!t) continue;
        char name[32] = {0};
        for (uint32_t i = 0; i < 31 && U8(t, 0x108 + i); i++) name[i] = (char)U8(t, 0x108 + i);
        printf("activation_native_test: area \"%s\" track %d at %.1f %.1f %.1f\n", name, I32(t, 0x104), F32(t, 0x60), F32(t, 0x64), F32(t, 0x68));
        if (strcmp(name, want) == 0 && !trigger) trigger = t;
    }
    if (!trigger) return fail("no StartActivity area of the activity", NULL);
    rx_activation_state a;
    read_activation(s, &a);
    printf("activation_native_test: before: valid %u active %u \"%s\" track %d\n", a.valid, a.active, a.activity, a.track_id);
    if (!a.valid || a.active) return fail("an area is marked before entering one", NULL);

    if (strcmp(argv[2], "plan") == 0) {
        /* a route over the collision terrain from the bike to the area: Dijkstra on a coarse grid, the cost the distance times a
         * slope penalty (slopes over 0.55 refused); prints every 8th node (the ROUTE for drive) */
        rx_terrain_info ti; memset(&ti, 0, sizeof ti);
        if (rx_session_terrain(s, &ti) != 0 || ti.samples == 0) return fail("terrain", s);
        rx_bike_state bs; rx_session_bike(s, &bs);
        const int step = 8, n = (int)ti.samples / step;
        float* h = malloc(sizeof(float) * (size_t)n * (size_t)n);
        float* row = malloc(sizeof(float) * ti.samples);
        for (int r = 0; r < n; r++) {
            rx_session_terrain_heights(s, row, 0, r * step, (int32_t)ti.samples, 1);
            for (int c = 0; c < n; c++) h[r * n + c] = row[c * step];
        }
        const float cell = ti.spacing * (float)step;
        #define CX(x) ((int)lroundf(((x) - ti.min_x) / cell))
        #define CR(z) ((int)lroundf((ti.max_z - (z)) / cell))
        const int sc = CX(bs.position[0][0]), sr = CR(bs.position[0][2]), gc = CX(F32(trigger, 0x60)), gr = CR(F32(trigger, 0x68));
        float* dist = malloc(sizeof(float) * (size_t)n * (size_t)n); int* prev = malloc(sizeof(int) * (size_t)n * (size_t)n); char* done = calloc((size_t)n * (size_t)n, 1);
        for (int i = 0; i < n * n; i++) { dist[i] = 1e30f; prev[i] = -1; }
        dist[sr * n + sc] = 0;
        for (;;) {
            int u = -1; float best = 1e30f;
            for (int i = 0; i < n * n; i++) if (!done[i] && dist[i] < best) { best = dist[i]; u = i; }
            if (u < 0 || u == gr * n + gc) break;
            done[u] = 1;
            const int ur = u / n, uc = u % n;
            for (int dr = -1; dr <= 1; dr++) for (int dc = -1; dc <= 1; dc++) {
                if (!dr && !dc) continue;
                const int vr = ur + dr, vc = uc + dc;
                if (vr < 0 || vc < 0 || vr >= n || vc >= n) continue;
                const float run = cell * sqrtf((float)(dr * dr + dc * dc));
                const float slope = fabsf(h[vr * n + vc] - h[u]) / run;
                if (slope > 0.55f) continue;
                const float c = run * (1.0f + 12.0f * slope * slope);
                if (dist[u] + c < dist[vr * n + vc]) { dist[vr * n + vc] = dist[u] + c; prev[vr * n + vc] = u; }
            }
        }
        int path[4096], np = 0;
        for (int v = gr * n + gc; v >= 0 && np < 4096; v = prev[v]) path[np++] = v;
        printf("activation_native_test: plan: %d nodes, cost %.0f\nROUTE=\"", np, dist[gr * n + gc]);
        for (int k = np - 1 - 8; k > 0; k -= 8) printf("%.0f,%.0f;", ti.min_x + (float)(path[k] % n) * cell, ti.max_z - (float)(path[k] / n) * cell);
        printf("\"\n");
        return 0;
    }

    /* ---- 2. into the area, and out again ---- */
    g_step = "into the area";
    const float tx = F32(trigger, 0x60), tz = F32(trigger, 0x68);
    rx_bike_state b;
    float saved[16];
    for (int i = 0; i < 16; i++) saved[i] = F32(trigger, 0x30 + 4 * i);
    const rx_addr matrix = rx_reflex_aligned_alloc(0x40);   /* the setter's source matrix (the game's heap) */
    int entered = -1;
    if (!drive) {
        if (rx_session_bike(s, &b) != 0) return fail("bike", s);
        for (int i = 0; i < 16; i++) SF32(matrix, 4 * i, saved[i]);
        SF32(matrix, 0x30, b.position[0][0]); SF32(matrix, 0x34, b.position[0][1]); SF32(matrix, 0x38, b.position[0][2]);
        uint32_t (*set_matrix)(rx_addr, rx_addr) = RX_FN(U32(U32(trigger, 0), 0x40), uint32_t (*)(rx_addr, rx_addr));   /* MXTrigger::vf16 */
        if (!set_matrix) return fail("the trigger's matrix setter is not in the core", NULL);
        set_matrix(trigger, matrix);
        printf("activation_native_test: the area moved onto the bike at %.1f %.1f %.1f\n", b.position[0][0], b.position[0][1], b.position[0][2]);
        for (int i = 0; i < 600 && entered < 0; i++) {
            if (rx_session_advance(s) != 0) return fail("advance", s);
            read_activation(s, &a);
            if (a.active) entered = i + 1;
        }
    } else {
        uint8_t keys[256];
        memset(keys, 0, sizeof keys);
        rx_pad pad;
        float best = 1e30f;
        /* ROUTE="x,z;x,z;...": waypoints before the area (an authored route); each is passed within 40 units */
        float route[32][2]; int nroute = 0, wp = 0;
        if (getenv("ROUTE")) {
            char buf[512]; snprintf(buf, sizeof buf, "%s", getenv("ROUTE"));
            for (char* t = strtok(buf, ";"); t && nroute < 32; t = strtok(NULL, ";")) if (sscanf(t, "%f,%f", &route[nroute][0], &route[nroute][1]) == 2) nroute++;
        }
        char last_area[24] = "";
        uint32_t last_active = 0;
        for (int i = 0; i < 60 * 240 && entered < 0; i++) {
            memset(&pad, 0, sizeof pad);
            if (rx_session_bike(s, &b) != 0) return fail("bike", s);
            const float dx = tx - b.position[0][0], dz = tz - b.position[0][2];
            const float dist = sqrtf(dx * dx + dz * dz);
            if (dist < best) best = dist;
            float gx = tx, gz = tz;
            if (wp < nroute) {
                const float wx = route[wp][0] - b.position[0][0], wz = route[wp][1] - b.position[0][2];
                if (wx * wx + wz * wz < 40.0f * 40.0f) { printf("activation_native_test: drive frame %d: waypoint %d\n", i, wp); wp++; }
                if (wp < nroute) { gx = route[wp][0]; gz = route[wp][1]; }
            }
            const float err = b.speed > 3.0f ? bike_heading_error(&b, gx, gz) : 0.0f;
            float steer = err * 2.5f;
            if (steer > 1.0f) steer = 1.0f;
            if (steer < -1.0f) steer = -1.0f;
            pad.lx = (int16_t)(steer * 32000.0f);
            pad.right_trigger = (uint8_t)(fabsf(err) > 1.2f ? 120 : 220);
            rx_session_set_input(s, keys, &pad, 1);
            if (rx_session_advance(s) != 0) return fail("advance", s);
            read_activation(s, &a);
            if (a.active != last_active || strcmp(a.activity, last_area) != 0) {
                printf("activation_native_test: drive frame %d: area %s \"%s\" track %d\n", i + 1, a.active ? "entered" : "left", a.activity, a.track_id);
                last_active = a.active;
                snprintf(last_area, sizeof last_area, "%s", a.activity);
            }
            if (a.active && strcmp(a.activity, want) == 0) entered = i + 1;
            if ((i % 600) == 0) printf("activation_native_test: drive frame %d at %.1f %.1f %.1f speed %.1f distance %.1f\n", i, b.position[0][0], b.position[0][1], b.position[0][2], b.speed, dist);
        }
        printf("activation_native_test: drive: closest %.1f\n", best);
        if (entered > 0) {
            /* on through the area (straight on, full throttle) until the game clears it */
            int on = -1;
            for (int i = 0; i < 60 * 30 && on < 0; i++) {
                memset(&pad, 0, sizeof pad);
                pad.right_trigger = 220;
                rx_session_set_input(s, keys, &pad, 1);
                if (rx_session_advance(s) != 0) return fail("advance", s);
                rx_activation_state x;
                read_activation(s, &x);
                if (!x.active) on = i + 1;
            }
            rx_activation_state x;
            read_activation(s, &x);
            printf("activation_native_test: ridden out of the area after %d frames: active %u \"%s\" track %d\n", on, x.active, x.activity, x.track_id);
            if (on < 0 || strcmp(x.activity, "FreeRide") != 0) return fail("the area was not cleared", NULL);
            /* back into it (the pause is pressed in the area) */
            int back = -1;
            for (int i = 0; i < 60 * 60 && back < 0; i++) {
                memset(&pad, 0, sizeof pad);
                if (rx_session_bike(s, &b) != 0) return fail("bike", s);
                const float err = b.speed > 3.0f ? bike_heading_error(&b, tx, tz) : 0.0f;
                float steer = err * 2.5f;
                if (steer > 1.0f) steer = 1.0f;
                if (steer < -1.0f) steer = -1.0f;
                pad.lx = (int16_t)(steer * 32000.0f);
                pad.right_trigger = (uint8_t)(fabsf(err) > 1.2f ? 90 : 180);
                rx_session_set_input(s, keys, &pad, 1);
                if (rx_session_advance(s) != 0) return fail("advance", s);
                read_activation(s, &x);
                if (x.active && strcmp(x.activity, want) == 0) back = i + 1;
            }
            printf("activation_native_test: back in the area after %d frames\n", back);
            if (back < 0) return fail("could not ride back into the area", NULL);
        }
        memset(&pad, 0, sizeof pad);
        rx_session_set_input(s, keys, &pad, 1);
    }
    read_activation(s, &a);
    printf("activation_native_test: in the area after %d frames: active %u \"%s\" track %d\n", entered, a.active, a.activity, a.track_id);
    if (entered < 0 || strcmp(a.activity, want) != 0) return fail("the area was not marked", NULL);
    const rx_activation_state in_area = a;

    g_step = "out of the area";
    int left = -1;
    if (!drive) {
        for (int i = 0; i < 16; i++) SF32(matrix, 4 * i, saved[i]);
        uint32_t (*set_matrix)(rx_addr, rx_addr) = RX_FN(U32(U32(trigger, 0), 0x40), uint32_t (*)(rx_addr, rx_addr));
        set_matrix(trigger, matrix);
        for (int i = 0; i < 600 && left < 0; i++) {
            if (rx_session_advance(s) != 0) return fail("advance", s);
            read_activation(s, &a);
            if (!a.active) left = i + 1;
        }
        printf("activation_native_test: out of the area after %d frames: active %u \"%s\" track %d\n", left, a.active, a.activity, a.track_id);
        if (left < 0 || strcmp(a.activity, "FreeRide") != 0) return fail("the area was not cleared", NULL);
    }
    rx_session_destroy(s);

    /* ---- 3. the pause in the area: the area's challenge ---- */
    g_step = "challenge";
    s = start_session(install, in_area.activity, in_area.track_id);
    if (!s) return 1;
    const int cpre = run_to_in_progress(s, 20000);
    const rx_addr C = local_activity();
    printf("activation_native_test: challenge activity %08x vtable %08x kind %u InProgress after %d frames\n", C, C ? U32(C, 0) : 0, C ? U32(C, 0xff070) : 0, cpre);
    if (!C || U32(C, 0) == 0x0094329cu || cpre < 0) return fail("the challenge did not start", NULL);
    uint32_t last = 0xffffffffu;
    uint8_t keys[256];
    memset(keys, 0, sizeof keys);
    keys[0xc8] = 0x80;   /* DIK_UP: throttle (the default keyboard map) */
    rx_session_set_input(s, keys, NULL, 0);
    for (int i = 0; i < challenge_frames; i++) {
        if (rx_session_advance(s) != 0) return fail("challenge advance", s);
        const uint32_t st = U32(C, 0x21c);
        if (st != last) { printf("activation_native_test: challenge frame %d state %u\n", i + 1, st); last = st; }
    }
    rx_session_destroy(s);

    /* ---- 4. EXIT: the level's Free Ride ---- */
    g_step = "exit";
    s = start_session(install, "", 0);
    if (!s) return 1;
    const int fpre = run_to_in_progress(s, 20000);
    const rx_addr F = local_activity();
    printf("activation_native_test: back in the Free Ride: activity vtable %08x InProgress after %d frames\n", F ? U32(F, 0) : 0, fpre);
    if (!F || U32(F, 0) != 0x0094329cu || fpre < 0) return fail("not back in the Free Ride", NULL);
    for (int i = 0; i < 300; i++) if (rx_session_advance(s) != 0) return fail("free ride advance", s);
    rx_session_destroy(s);
    printf("activation_native_test: PASS\n");
    return 0;
}
