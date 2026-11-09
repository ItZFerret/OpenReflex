/* vehicle_audio_test.c -- samples the vehicle sound layer query (rx_session_vehicle_audio, include/rx_vehicle_audio.h) through a race.
 *   vehicle_audio_test <librxcore.dylib> <install root> [race frames (900)] [level]
 * Loads the reference event (or the level given), holds the throttle (W) from the start, lifts it for a while, and prints every 30th
 * frame each rider's rpm / max / idle / throttle / gear / load / flags and every new cue (start line, Pre Exited, scenario sounds).
 * Exit 0 = the query answered with riders while racing, 1 = an API call failed or nothing was reported. Test only; every action is
 * a public API call. */
#include "rx_api.h"
#include "rx_vehicle_audio.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n = (__typeof__(&n))dlsym(lib, #n); if (!f_##n) { fprintf(stderr, "missing API %s\n", #n); return 2; }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <librxcore> <install root> [race frames] [level]\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0); alarm(600);
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    LOAD(rx_session_create); LOAD(rx_session_destroy); LOAD(rx_session_last_error); LOAD(rx_session_event_default); LOAD(rx_session_event_load);
    LOAD(rx_session_advance_dt); LOAD(rx_session_get_state); LOAD(rx_session_set_input); LOAD(rx_session_vehicle_audio);
    const int race_frames = argc > 3 ? atoi(argv[3]) : 900;
    rx_session* s = f_rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create: %s\n", f_rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; f_rx_session_event_default(&e);
    if (argc > 4) snprintf(e.location, sizeof e.location, "%s", argv[4]);
    printf("event %s %s riders %d\n", e.event_type, e.location, e.riders);
    if (f_rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", f_rx_session_last_error(s)); f_rx_session_destroy(s); return 1; }
    uint32_t seen = 0; int racing = 0, reported = 0;
    for (int i = 0; i < 12000 && racing < race_frames; i++) {
        const rx_session_state st = f_rx_session_get_state(s);
        uint8_t keys[256] = {0};
        if (st == RX_SESSION_RACING) { racing++; if (racing < 400 || racing > 520) keys[0x11] = 0x80; }
        if (f_rx_session_set_input(s, keys, NULL, 0) || f_rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", f_rx_session_last_error(s)); f_rx_session_destroy(s); return 1; }
        static rx_vehicle_audio_state v;
        memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_VEHICLE_AUDIO_VERSION;
        if (f_rx_session_vehicle_audio(s, &v)) { fprintf(stderr, "query failed\n"); f_rx_session_destroy(s); return 1; }
        for (uint32_t k = 0; k < RX_VEHICLE_AUDIO_CUES; k++) {
            const rx_vehicle_audio_cue* c = &v.cues[k];
            if (c->serial > seen) printf("frame %d state %d CUE %u kind %d '%s'\n", i, st, c->serial, c->kind, c->name);
        }
        if (v.cue_serial > seen) seen = v.cue_serial;
        if (v.valid && v.rider_count > 0 && st >= RX_SESSION_COUNTDOWN && (i % 30 == 0)) {
            for (int r = 0; r < v.rider_count; r++) {
                const rx_vehicle_audio_rider* d = &v.riders[r];
                if (r > 1 && r != v.rider_count - 1) continue;
                printf("frame %d st %d r%d slot %d cls %d kind %d pad %d size %d flags %x rpm %.0f/%.0f idle %.0f thr %.2f c2 %.2f gear %d load %.3f clutch %u air %u wheels %u%u%u%u spd %.1f pos %.1f %.1f %.1f wreck %u\n",
                       i, st, r, d->slot, d->vehicle_class, d->vehicle_kind, d->pad, d->engine_size, d->flags, d->rpm, d->max_rpm, d->idle_rpm,
                       d->throttle, d->control2, d->gear, d->load, d->clutch, d->airborne, d->wheel_air[0], d->wheel_air[1], d->wheel_air[2],
                       d->wheel_air[3], d->speed, d->position[0], d->position[1], d->position[2], d->wrecked);
            }
            if (st == RX_SESSION_RACING) reported++;
        }
    }
    f_rx_session_destroy(s);
    printf("reported %d racing samples, %u cues\n", reported, seen);
    return reported > 0 ? 0 : 1;
}
