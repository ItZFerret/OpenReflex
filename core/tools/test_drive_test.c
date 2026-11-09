/* test_drive_test.c -- the garage's TEST DRIVE through the session API (include/rx_test_drive.h): create, rx_session_test_drive_start, the
 * frames until the local slot spawned in the UI world, a ride with the right trigger held, rx_session_test_drive_end, destroy.
 *   test_drive_test <install root> [ride frames (600)]
 * Build (against the product library): clang -std=c11 -O2 -Icore/include -Icore/game core/tools/test_drive_test.c -L<dir> -lrxcore -o <out>
 * Exit 0 when the vehicle spawned, moved under throttle and the end ran without an error. */
#include "rx_test_drive.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fail(rx_session* s, const char* what) {
    fprintf(stderr, "test_drive_test: %s: %s\n", what, rx_session_last_error(s));
    if (s) rx_session_destroy(s);
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: test_drive_test <install root> [ride frames]\n"); return 2; }
    const int ride = argc > 2 ? atoi(argv[2]) : 600;
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) return fail(NULL, "create");
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (rx_session_test_drive_start(s, &spec) != 0) return fail(s, "test drive start");
    rx_test_drive_state td;
    int f = 0;
    for (; f < 3000; f++) {
        if (rx_session_advance(s) != 0) return fail(s, "advance (spawn)");
        memset(&td, 0, sizeof td); td.size = sizeof td; td.version = RX_TEST_DRIVE_API_VERSION;
        rx_session_test_drive(s, &td);
        if (td.spawned) break;
    }
    printf("test_drive_test: spawned after %d frames: state %d vehicle type %d enabled %d idle %d\n", f + 1, (int)rx_session_get_state(s),
           td.vehicle_type, td.enabled, td.players_idle);
    if (!td.spawned) return fail(s, "no spawn");
    {   /* what the Unity client reads at its startup: the slots, the local appearance, the terrain of the active (UI) scene */
        rx_slot sl[16]; memset(sl, 0, sizeof sl);
        const int n = rx_session_slots(s, sl, 16);
        rx_appearance ap; memset(&ap, 0, sizeof ap);
        const int ar = n > 0 ? rx_session_appearance(s, sl[0].slot, &ap) : -1;
        rx_terrain_info ti; memset(&ti, 0, sizeof ti);
        const int tr = rx_session_terrain(s, &ti);
        printf("test_drive_test: slots %d, appearance %d (vehicle owner %d id %d), terrain %d (%u samples, spacing %.2f)\n", n, ar, ap.vehicle_owner, ap.owner_vehicle_id,
               tr, ti.samples, ti.spacing);
        if (n < 1 || ar != 0 || tr != 0) return fail(s, "startup getters");
    }
    rx_vehicle_info vi; rx_bike_state b0, b1; rx_camera cam;
    memset(&b0, 0, sizeof b0); rx_session_bike(s, &b0);
    for (int i = 0; i < ride; i++) {
        rx_pad pads[4]; memset(pads, 0, sizeof pads);
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        if (i > 60) pads[0].right_trigger = 255;
        if (i > 200 && i < 260) pads[0].lx = 20000;
        rx_session_set_input(s, keys, pads, 1);
        if (rx_session_advance(s) != 0) return fail(s, "advance (ride)");
        if (i % 120 == 0 || i + 1 == ride) {
            memset(&vi, 0, sizeof vi); rx_session_vehicle_info(s, -1, &vi);
            memset(&b1, 0, sizeof b1); rx_session_bike(s, &b1);
            memset(&cam, 0, sizeof cam); const int cr = rx_session_camera(s, &cam);
            printf("frame %4d: state %d speed %6.2f gear %d rpm %7.1f chassis (%.2f %.2f %.2f) camera %d\n", i, (int)rx_session_get_state(s), vi.speed, vi.gear, vi.rpm,
                   b1.position[0][0], b1.position[0][1], b1.position[0][2], cr);
        }
    }
    memset(&b1, 0, sizeof b1); rx_session_bike(s, &b1);
    const float dx = b1.position[0][0] - b0.position[0][0], dz = b1.position[0][2] - b0.position[0][2];
    printf("test_drive_test: moved %.1f ft\n", sqrtf(dx * dx + dz * dz));
    if (rx_session_test_drive_end(s) != 0) return fail(s, "test drive end");
    for (int i = 0; i < 30; i++) if (rx_session_advance(s) != 0) return fail(s, "advance (after the end)");
    memset(&td, 0, sizeof td); td.size = sizeof td; td.version = RX_TEST_DRIVE_API_VERSION; rx_session_test_drive(s, &td);
    printf("test_drive_test: ended: active %d spawned %d enabled %d idle %d\n", td.active, td.spawned, td.enabled, td.players_idle);
    rx_session_destroy(s);
    printf("test_drive_test: PASS\n");
    return 0;
}
