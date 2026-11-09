/* aitrace.c -- records the FULL engine-audio telemetry of every rider (rx_session_vehicle_audio, every field the original-engine emitter
 * reads, the start line, the cues) for every frame from the event load on (loading / countdown / racing), through the product session API
 * the Unity client uses. Input for engine_logic/ (the C# OriginalEngineAudio run offline) and field_mix.py. Scratch tool for w-aiengine.
 *   aitrace <librxcore> <install root> <total frames after the race starts> <pad: hold | lap:<seed>> [level] > trace.csv
 * With LANDTRACE_OUT=<file> it also writes the manager wheel groups (rx_session_sound: in air, suspension force, zone, contact id, slips) per frame,
 * "W,frame,rider,wheelcount,{inair,susp,zone,cid,side2,roll2,ang}x4" (w-landclank; read-only, the trace itself is unchanged).
 * Rider 0 is the local human. Default event (12 riders). A bike's REV one-shot ending is the sound engine's: the tracer reports it to the
 * core REV_FRAMES (75 = 1.25 s, the REV samples are 1.07 - 1.46 s) frames after the core entered / replayed the Rev (as the host does).
 * Build: clang -std=c11 -O1 -I core/include aitrace.c -o aitrace */
#include "rx_api.h"
#include "rx_vehicle_audio.h"
#include "rx_sound.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define REV_FRAMES 75
#define LOAD(n) __typeof__(&n) f_##n = (__typeof__(&n))dlsym(lib, #n); if (!f_##n) { fprintf(stderr, "missing API %s\n", #n); return 2; }
int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage\n"); return 2; }
    alarm(1200);
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    LOAD(rx_session_create); LOAD(rx_session_destroy); LOAD(rx_session_last_error); LOAD(rx_session_event_default); LOAD(rx_session_event_load);
    LOAD(rx_session_advance_dt); LOAD(rx_session_get_state); LOAD(rx_session_set_input); LOAD(rx_session_vehicle_audio); LOAD(rx_remap_defaults);
    LOAD(rx_session_vehicle_audio_finished);
    __typeof__(&rx_session_sound) f_rx_session_sound = (__typeof__(&rx_session_sound))dlsym(lib, "rx_session_sound");
    FILE* wout = getenv("LANDTRACE_OUT") && f_rx_session_sound ? fopen(getenv("LANDTRACE_OUT"), "w") : NULL;
    static rx_sound_state snd;
    const int race_frames = atoi(argv[3]);
    int lap = strncmp(argv[4], "lap:", 4) == 0; int lift = 0, next_lift = 150; unsigned seed = lap ? (unsigned)atoi(argv[4] + 4) : 1;
    rx_remap_table remap; memset(&remap, 0, sizeof remap); f_rx_remap_defaults(&remap);
    for (int a = 0; a < 19; a++) { int nat = remap.keyboard[a]; remap.keyboard[a] = 0; for (int k = 1; k < 256; k++) if (remap.scan[k] == nat) { remap.keyboard[a] = k; break; } }
    rx_session* s = f_rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create: %s\n", f_rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; f_rx_session_event_default(&e);
    if (argc > 5) snprintf(e.location, sizeof e.location, "%s", argv[5]);
    fprintf(stderr, "event %s %s riders %d\n", e.event_type, e.location, e.riders);
    if (f_rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", f_rx_session_last_error(s)); return 1; }
    printf("frame,phase,rider,slot,flags,cls,kind,pad,size,vehicle,px,py,pz,speed,rpm,max,idle,thr,ctl2,gear,load,clutch,air,belowhalf,water,uw,drowned,wrecked,wa0,wa1,wa2,wa3,aslot,sl_state,sl_serial,sl_target\n");
    int racing = 0, frame = 0; uint8_t keys[256] = {0}; unsigned last_cue = 0;
    unsigned serial_seen[16] = {0}; int finish_at[16]; for (int i = 0; i < 16; i++) finish_at[i] = -1;
    for (int i = 0; i < 40000 && racing < race_frames; i++) {
        const rx_session_state st = f_rx_session_get_state(s);
        memset(keys, 0, sizeof keys);
        if (st == RX_SESSION_RACING) {
            racing++;
            if (lap) { if (lift > 0) lift--; else { keys[remap.keyboard[0] & 255] = 0x80; if (--next_lift <= 0) { lift = 15 + (int)(rand_r(&seed) % 40); next_lift = 120 + (int)(rand_r(&seed) % 180); } } }
            else keys[remap.keyboard[0] & 255] = 0x80;
        }
        if (f_rx_session_set_input(s, keys, NULL, 0) || f_rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", f_rx_session_last_error(s)); break; }
        static rx_vehicle_audio_state v;
        memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_VEHICLE_AUDIO_VERSION;
        if (f_rx_session_vehicle_audio(s, &v)) { fprintf(stderr, "query failed\n"); break; }
        if (!v.valid) continue;
        frame++;
        if (wout) {
            memset(&snd, 0, sizeof snd); snd.size = sizeof snd; snd.version = RX_SOUND_VERSION;
            if (!f_rx_session_sound(s, &snd) && snd.valid)
                for (int r = 0; r < v.rider_count; r++) {
                    const rx_sound_rider* sr = NULL;
                    for (int k = 0; k < snd.rider_count; k++) if (snd.riders[k].vehicle == v.riders[r].vehicle) { sr = &snd.riders[k]; break; }
                    if (!sr) continue;
                    fprintf(wout, "W,%d,%d,%d", frame, r, sr->wheel_count);
                    for (int w = 0; w < 4; w++) { const rx_sound_wheel* wh = &sr->wheels[w]; fprintf(wout, ",%u,%.1f,%d,%d,%.1f,%.1f,%.1f", (wh->flags & RX_SW_AIR) ? 1u : 0u, wh->suspension_force, wh->zone, wh->contact_id, wh->side_slip2, wh->roll_slip2, wh->angular_velocity); }
                    fprintf(wout, "\n");
                }
        }
        for (int r = 0; r < v.rider_count; r++) {
            const rx_vehicle_audio_rider* d = &v.riders[r];
            printf("%d,%d,%d,%d,%u,%d,%d,%d,%d,%u,%.2f,%.2f,%.2f,%.2f,%.1f,%.1f,%.1f,%.3f,%.3f,%d,%.3f,%u,%u,%u,%.3f,%u,%u,%u,%u,%u,%u,%u,%d,%d,%u,%.1f\n", frame, (int)st, r, d->slot, d->flags,
                   d->vehicle_class, d->vehicle_kind, d->pad, d->engine_size, d->vehicle, d->position[0], d->position[1], d->position[2], d->speed, d->rpm, d->max_rpm,
                   d->idle_rpm, d->throttle, d->control2, d->gear, d->load, d->clutch, d->airborne, d->below_half, d->water, d->underwater, d->drowned, d->wrecked,
                   d->wheel_air[0], d->wheel_air[1], d->wheel_air[2], d->wheel_air[3], d->audio_slot, d->start_line_state, d->start_line_serial, d->start_line_target);
            /* the host's EVENT_FINISHED of a bike's Rev one-shot (the core replays / leaves the Rev) */
            if ((d->flags & RX_VA_START_LINE) && d->audio_slot >= 0 && d->audio_slot < 16) {
                int k = d->audio_slot;
                if (d->start_line_state == 2 && d->start_line_serial != serial_seen[k]) finish_at[k] = frame + REV_FRAMES;
                if (d->start_line_state != 2) finish_at[k] = -1;
                serial_seen[k] = d->start_line_serial;
            }
        }
        for (int c = 0; c < RX_VEHICLE_AUDIO_CUES; c++) if (v.cues[c].serial > last_cue) { /* in ring order: print then bump */ }
        { unsigned newest = last_cue; for (int c = 0; c < RX_VEHICLE_AUDIO_CUES; c++) if (v.cues[c].serial > last_cue) { printf("C,%d,%u,%d,%s\n", frame, v.cues[c].serial, v.cues[c].kind, v.cues[c].name); if (v.cues[c].serial > newest) newest = v.cues[c].serial; } last_cue = newest; }
        for (int k = 0; k < 16; k++) if (finish_at[k] >= 0 && frame >= finish_at[k]) { f_rx_session_vehicle_audio_finished(s, k); finish_at[k] = -1; printf("F,%d,%d\n", frame, k); }
    }
    if (wout) fclose(wout);
    f_rx_session_destroy(s);
    fprintf(stderr, "frames %d racing %d\n", frame, racing);
    return 0;
}
