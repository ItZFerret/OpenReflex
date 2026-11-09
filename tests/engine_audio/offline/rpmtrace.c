/* rpmtrace.c -- records every rider's engine telemetry (rx_session_vehicle_audio: rpm / max / idle / throttle / gear / speed / air)
 * every frame of a race, through the product session API the Unity client uses. Scratch tool for w-enginepacks.
 *   rpmtrace <librxcore> <install root ("game files/Archive")> <race frames> <pad: hold | lap:<seed> | fuzz:<seed>> [level] > trace.csv
 * Default event (12 riders); rider 0 is the local human: hold = throttle (W) held, lap = held and lifted 0.25 .. 0.9 s every 2 .. 5 s,
 * fuzz = random throttle / brake / steering / reset as stop_audit_run's fuzz pad. Build:
 *   clang -std=c11 -O1 -I core/include rpmtrace.c -o rpmtrace   (the library is dlopened) */
#include "rx_api.h"
#include "rx_vehicle_audio.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n = (__typeof__(&n))dlsym(lib, #n); if (!f_##n) { fprintf(stderr, "missing API %s\n", #n); return 2; }
int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage\n"); return 2; }
    alarm(900);
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    LOAD(rx_session_create); LOAD(rx_session_destroy); LOAD(rx_session_last_error); LOAD(rx_session_event_default); LOAD(rx_session_event_load);
    LOAD(rx_session_advance_dt); LOAD(rx_session_get_state); LOAD(rx_session_set_input); LOAD(rx_session_vehicle_audio); LOAD(rx_remap_defaults);
    const int race_frames = atoi(argv[3]);
    int fuzz = strncmp(argv[4], "fuzz:", 5) == 0, lap = strncmp(argv[4], "lap:", 4) == 0; int lift = 0, next_lift = 150; unsigned seed = fuzz ? (unsigned)atoi(argv[4] + 5) : lap ? (unsigned)atoi(argv[4] + 4) : 1;
    rx_remap_table remap; memset(&remap, 0, sizeof remap); f_rx_remap_defaults(&remap);
    /* keyboard[] holds native input ids; scan[DIK] = native id: the DIK of each action */
    for (int a = 0; a < 19; a++) { int nat = remap.keyboard[a]; remap.keyboard[a] = 0; for (int k = 1; k < 256; k++) if (remap.scan[k] == nat) { remap.keyboard[a] = k; break; } }
    fprintf(stderr, "throttle DIK %d\n", remap.keyboard[0]);
    rx_session* s = f_rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create: %s\n", f_rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; f_rx_session_event_default(&e);
    if (argc > 5) snprintf(e.location, sizeof e.location, "%s", argv[5]);
    fprintf(stderr, "event %s %s riders %d\n", e.event_type, e.location, e.riders);
    if (f_rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", f_rx_session_last_error(s)); return 1; }
    printf("frame,rider,slot,flags,cls,size,rpm,max,idle,thr,gear,speed,air,clutch,load\n");
    int racing = 0, hold = 0; uint8_t keys[256] = {0};
    for (int i = 0; i < 20000 && racing < race_frames; i++) {
        const rx_session_state st = f_rx_session_get_state(s);
        if (st == RX_SESSION_RACING) {
            racing++;
            if (fuzz) {
                if (hold-- <= 0) {
                    hold = 15 + (int)(rand_r(&seed) % 40); memset(keys, 0, sizeof keys);
                    if (rand_r(&seed) % 10 != 0) keys[remap.keyboard[0] & 255] = 0x80;
                    if (rand_r(&seed) % 12 == 0) keys[remap.keyboard[1] & 255] = 0x80;
                    { unsigned r = rand_r(&seed) % 4; if (r == 0) keys[remap.keyboard[2] & 255] = 0x80; else if (r == 1) keys[remap.keyboard[3] & 255] = 0x80; }
                    if (rand_r(&seed) % 60 == 0) keys[remap.keyboard[12] & 255] = 0x80;
                }
            } else if (lap) {   /* throttle held, lifted 0.25 .. 0.9 s every 2 .. 5 s (landings, corners) */
                memset(keys, 0, sizeof keys);
                if (lift > 0) lift--; else { keys[remap.keyboard[0] & 255] = 0x80; if (--next_lift <= 0) { lift = 15 + (int)(rand_r(&seed) % 40); next_lift = 120 + (int)(rand_r(&seed) % 180); } }
            } else { memset(keys, 0, sizeof keys); keys[remap.keyboard[0] & 255] = 0x80; }
        }
        if (f_rx_session_set_input(s, keys, NULL, 0) || f_rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", f_rx_session_last_error(s)); break; }
        static rx_vehicle_audio_state v;
        memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_VEHICLE_AUDIO_VERSION;
        if (f_rx_session_vehicle_audio(s, &v)) { fprintf(stderr, "query failed\n"); break; }
        if (st != RX_SESSION_RACING || !v.valid) continue;
        for (int r = 0; r < v.rider_count; r++) {
            const rx_vehicle_audio_rider* d = &v.riders[r];
            printf("%d,%d,%d,%u,%d,%d,%.1f,%.1f,%.1f,%.3f,%d,%.2f,%u,%u,%.3f\n", racing, r, d->slot, d->flags, d->vehicle_class, d->engine_size,
                   d->rpm, d->max_rpm, d->idle_rpm, d->throttle, d->gear, d->speed, d->airborne, d->clutch, d->load);
        }
    }
    f_rx_session_destroy(s);
    fprintf(stderr, "racing frames %d\n", racing);
    return 0;
}
