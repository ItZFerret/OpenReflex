/* Regression: the bike ABI is four body vec4s, not four contiguous body-0 matrices.
 * Compare the independent vehicle export bit-for-bit, and guard the output bounds.
 * clang -std=c11 -Icore/include core/tools/rx_api_bike_pose_test.c -o <test>
 * RX_FULL_FRAME=1 <test> <librxcore.dylib> <install> [frames=2400]
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define LOAD(name) __typeof__(&name) fn_##name = (__typeof__(&name))dlsym(lib, #name); \
    if (!fn_##name) { fprintf(stderr, "missing %s\n", #name); return 2; }
int main(int argc, char **argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s library install [frames]\n", argv[0]); return 2; }
    void *lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    LOAD(rx_session_create); LOAD(rx_session_destroy); LOAD(rx_session_event_default);
    LOAD(rx_session_event_load); LOAD(rx_session_advance_dt); LOAD(rx_session_last_error);
    LOAD(rx_session_bike); LOAD(rx_session_vehicle_info); LOAD(rx_session_set_input);
    struct rx_session *s = fn_rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create failed\n"); return 1; }
    rx_event_spec spec; fn_rx_session_event_default(&spec);
    int bad = fn_rx_session_event_load(s, &spec) != 0, checked = 0;
    const int frames = argc > 3 ? atoi(argv[3]) : 2400;
    for (int frame = 0; !bad && frame < frames; ++frame) {
        uint8_t keys[256] = {0}; keys[0x11] = 0x80;
        fn_rx_session_set_input(s, keys, NULL, 0);
        if (fn_rx_session_advance_dt(s, 1.0f / 60.0f) != 0) {
            fprintf(stderr, "advance: %s\n", fn_rx_session_last_error(s)); bad = 1; break;
        }
        struct { uint64_t before; rx_bike_state bike; uint64_t after; } guard;
        memset(&guard, 0xa5, sizeof guard);
        const uint64_t canary = UINT64_C(0xa5a5a5a5a5a5a5a5);
        rx_vehicle_info vehicle;
        int rc = fn_rx_session_bike(s, &guard.bike);
        if (guard.before != canary || guard.after != canary) {
            fprintf(stderr, "FAIL frame %d: bike getter overwrote output boundary\n", frame); bad = 1; break;
        }
        if (rc || fn_rx_session_vehicle_info(s, -1, &vehicle) || vehicle.bodies != 4) continue;
        for (int body = 0; body < 4; ++body) {
            if (memcmp(guard.bike.render_position[body], vehicle.render_position + 4 * body, 16) ||
                memcmp(guard.bike.render_orientation[body], vehicle.render_orientation + 4 * body, 16)) {
                fprintf(stderr, "FAIL frame %d body %d: bike pose differs from vehicle export\n", frame, body);
                bad = 1; break;
            }
        }
        if (!bad) checked++;
    }
    fn_rx_session_destroy(s);
    printf("%s: %d frames, four body poses and output canaries\n", !bad && checked ? "PASS" : "FAIL", checked);
    return bad || !checked ? 1 : 0;
}
