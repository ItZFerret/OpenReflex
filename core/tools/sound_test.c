/* sound_test.c -- samples the sound managers' query (rx_session_sound, include/rx_sound.h) and the engine emitters' AIFX slots
 * (rx_session_vehicle_audio audio_slot) through a race.
 *   sound_test <librxcore.dylib> <install root> [race frames (900)] [level]
 * Loads the reference event (or the level given), holds the throttle (W), and prints the channel mix, every 60th frame the local rider's
 * wheels (zone, contact, slip, suspension force), every rider's manager slot, and counts the contacts / reactions / cues per kind.
 * Also (w-reverb2): counts the RX_SCF_SLIDE_B contacts with a slide_kind, and samples rx_session_first_person_view while the driving camera is
 * switched to the first-person preset (3) for frames 300 .. 600 of the race: prints each change of the flag.
 * Exit 0 = the query answered while racing, 1 = an API call failed or nothing was reported. Test only; public API calls only. */
#include "rx_api.h"
#include "rx_vehicle_audio.h"
#include "rx_sound.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#define LOAD(n) __typeof__(&n) f_##n = (__typeof__(&n))dlsym(lib, #n); if (!f_##n) { fprintf(stderr, "missing API %s\n", #n); return 2; }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: %s <librxcore> <install root> [race frames] [level]\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0); alarm(900);
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    LOAD(rx_session_create); LOAD(rx_session_destroy); LOAD(rx_session_last_error); LOAD(rx_session_event_default); LOAD(rx_session_event_load);
    LOAD(rx_session_advance_dt); LOAD(rx_session_get_state); LOAD(rx_session_set_input); LOAD(rx_session_vehicle_audio); LOAD(rx_session_sound);
    LOAD(rx_session_sizeof);
    if (f_rx_session_sizeof("rx_sound_state") != (int32_t)sizeof(rx_sound_state)) { fprintf(stderr, "sizeof mismatch\n"); return 1; }
    typedef int (*fp_fn)(rx_session*); typedef int (*cam_fn)(rx_session*, int32_t);
    fp_fn f_first_person = (fp_fn)dlsym(lib, "rx_session_first_person_view"); cam_fn f_set_cam = (cam_fn)dlsym(lib, "rx_session_set_driving_camera");
    if (!f_first_person) fprintf(stderr, "(no rx_session_first_person_view)\n");
    int fp_last = -1; uint32_t slide_b = 0, slide_b_kind[4] = {0};
    const int race_frames = argc > 3 ? atoi(argv[3]) : 900;
    rx_session* s = f_rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create: %s\n", f_rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; f_rx_session_event_default(&e);
    if (argc > 4) snprintf(e.location, sizeof e.location, "%s", argv[4]);
    printf("event %s %s riders %d\n", e.event_type, e.location, e.riders);
    if (f_rx_session_event_load(s, &e)) { fprintf(stderr, "event load: %s\n", f_rx_session_last_error(s)); f_rx_session_destroy(s); return 1; }
    uint32_t contacts = 0, reactions = 0, cues = 0; int racing = 0, reported = 0, mixed = 0, slots = 0;
    uint32_t by_class[8] = {0}; uint32_t zones[9] = {0}, hits = 0;
    static rx_sound_state v; static rx_vehicle_audio_state a;
    for (int i = 0; i < 12000 && racing < race_frames; i++) {
        const rx_session_state st = f_rx_session_get_state(s);
        uint8_t keys[256] = {0};
        if (st == RX_SESSION_RACING) { racing++; keys[0x11] = 0x80; }
        if (f_set_cam && (racing == 300 || racing == 600)) f_set_cam(s, racing == 300 ? 3 : 0);
        if (f_rx_session_set_input(s, keys, NULL, 0) || f_rx_session_advance_dt(s, 1.f / 60)) { fprintf(stderr, "advance: %s\n", f_rx_session_last_error(s)); f_rx_session_destroy(s); return 1; }
        memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_SOUND_VERSION;
        if (f_rx_session_sound(s, &v)) { fprintf(stderr, "sound query failed\n"); f_rx_session_destroy(s); return 1; }
        memset(&a, 0, sizeof a); a.size = sizeof a; a.version = RX_VEHICLE_AUDIO_VERSION;
        if (f_rx_session_vehicle_audio(s, &a)) { fprintf(stderr, "vehicle audio query failed\n"); f_rx_session_destroy(s); return 1; }
        for (uint32_t k = 0; k < RX_SOUND_CONTACTS; k++) {
            const rx_sound_contact* c = &v.contacts[k];
            if (c->serial <= contacts) continue;
            const int ci = c->class_b == RX_SC_TERRAIN ? 0 : c->class_b == RX_SC_COLLISION_ENTITY ? 1 : c->class_b == RX_SC_VEHICLE ? 2 : c->class_b == RX_SC_RIDER_ENTITY ? 3 :
                           c->class_b == RX_SC_PHYSICS_ENTITY ? 4 : c->class_b == RX_SC_FOREST ? 5 : c->class_b == RX_SC_WATER ? 6 : 7;
            by_class[ci]++;
            if (c->flags & RX_SCF_SLIDE_B) { slide_b++; slide_b_kind[c->slide_kind & 3]++; }
            if (c->serial % 97 == 0)
                printf("frame %d contact %u cls %x/%x snd %d/%d mat %d/%d flags %x str %.2f/%.2f zone %d slide %d %.1f pos %.0f %.0f %.0f\n", i, c->serial, c->class_a, c->class_b,
                       c->sound_a, c->sound_b, c->material_a, c->material_b, c->flags, c->strength_a, c->strength_b, c->zone, c->slide_kind, c->slide_speed,
                       c->position[0], c->position[1], c->position[2]);
        }
        if (f_first_person && st == RX_SESSION_RACING) {
            const int fp = f_first_person(s);
            if (fp != fp_last) { printf("frame %d racing %d first-person view %d\n", i, racing, fp); fp_last = fp; }
        }
        if (v.contact_serial > contacts) contacts = v.contact_serial;
        for (uint32_t k = 0; k < RX_SOUND_REACTIONS; k++) if (v.crowd.reactions[k].serial > reactions) printf("frame %d st %d crowd reaction %d\n", i, st, v.crowd.reactions[k].kind);
        if (v.crowd.reaction_serial > reactions) reactions = v.crowd.reaction_serial;
        for (uint32_t k = 0; k < RX_SOUND_CUES; k++) if (v.cues[k].serial > cues) printf("frame %d st %d cue '%s' index %d\n", i, st, v.cues[k].path, v.cues[k].index);
        if (v.cue_serial > cues) cues = v.cue_serial;
        if (v.valid && !mixed && v.channel_count > 0 && st >= RX_SESSION_COUNTDOWN) {
            printf("mix (%d channels):", v.channel_count); for (int k = 0; k < v.channel_count; k++) printf(" %.3f", v.channel_mix[k]); printf("\n"); mixed = 1;
        }
        {   /* the music player: every change of its serial */
            static uint32_t mserial; static int first = 1;
            if (v.music.valid && (first || v.music.serial != mserial)) {
                printf("frame %d st %d music mode %d flags %x kind %d delay %.1f req %d cur %d score %d/%d pos %d count %d trans %.0f track '%s' score '%s' playlist", i, st,
                       v.music.mode, v.music.flags, v.music.kind, v.music.delay, v.music.requested, v.music.current, v.music.score_requested, v.music.score_current,
                       v.music.position, v.music.count, v.music.transition, v.music.track, v.music.score);
                for (int k = 0; k < v.music.count && k < RX_SOUND_PLAYLIST; k++) printf(" %d", v.music.playlist[k]);
                printf("\n"); mserial = v.music.serial; first = 0;
            }
        }
        if (a.valid && !slots && st == RX_SESSION_RACING) {
            printf("audio slots:"); for (int r = 0; r < a.rider_count; r++) printf(" r%d(slot %d flags %x)=%d", r, a.riders[r].slot, a.riders[r].flags, a.riders[r].audio_slot); printf("\n"); slots = 1;
        }
        if (v.valid && v.rider_count > 0 && st == RX_SESSION_RACING) {
            const rx_sound_rider* r = &v.riders[0];
            for (int w = 0; w < r->wheel_count; w++) { const int z = r->wheels[w].zone; zones[z >= 0 && z < 8 ? z : 8]++; if (r->wheels[w].contact) hits++; }
            if (i % 60 == 0) {
                printf("frame %d r0 slot %d cls %d snd %d mat %d wheels %d:", i, r->slot, r->vehicle_class, r->sound_id, r->material, r->wheel_count);
                for (int w = 0; w < r->wheel_count; w++) {
                    const rx_sound_wheel* d = &r->wheels[w];
                    printf(" [zone %d id %d fl %x susp %.0f side %.1f roll %.1f ang %.1f d %.2f]", d->zone, d->contact_id, d->flags, d->suspension_force, d->side_slip2, d->roll_slip2, d->angular_velocity, d->depth);
                }
                printf(" crowd %u t %d s %d/%d m %.2f\n", v.crowd.valid, v.crowd.type, v.crowd.size, v.crowd.size_requested, v.crowd.momentum);
                reported++;
            }
        }
    }
    f_rx_session_destroy(s);
    printf("contacts %u (terrain %u, collision entities %u, vehicles %u, riders %u, physics %u, forest %u, water %u, other %u), reactions %u, cues %u\n",
           contacts, by_class[0], by_class[1], by_class[2], by_class[3], by_class[4], by_class[5], by_class[6], by_class[7], reactions, cues);
    printf("zones under the local wheels:"); for (int z = 0; z < 9; z++) printf(" %d:%u", z == 8 ? -1 : z, zones[z]); printf(" ; object contacts %u\n", hits);
    printf("slide-B contacts %u (slide_kind 0:%u 1:%u 2:%u 3:%u)\n", slide_b, slide_b_kind[0], slide_b_kind[1], slide_b_kind[2], slide_b_kind[3]);
    printf("reported %d racing samples\n", reported);
    return reported > 0 ? 0 : 1;
}
