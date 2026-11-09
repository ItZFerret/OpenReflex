/* ambient_census.c -- the ambient (NPC / scenery-animation) entities the PRODUCT session exports for one event: how many of each class
 * (rx_session_ambient_entities kind), how many are visible, which models. The check that a level's crowds, spectators, marshals, crew
 * and pit people exist in the core and reach the renderer.
 *   ambient_census <install root> <type> <typeId> <level> <locationId> <vehicleClass> <duration> <durationType> [frames after load]
 * e.g. ambient_census "game files" Supercross 8 SX_Kingston 74 1 2 Laps 60
 * env AMBIENT_CENSUS_MODELS=1 lists every distinct (kind, model) with its count; AMBIENT_CENSUS_BILLBOARD=1 lists the billboard crowd
 * sections (entity id, model, phase, empty-chair ratio, animated flag, active flashes); AMBIENT_CENSUS_FLASHES=N runs N more frames and, every
 * frame, compares rx_session_crowd_flashes (include/rx_crowd_api.h) with the active-flash counts of the ambient export (param[3]): frames
 * with flashes, the most at once, mismatches, the fields' ranges, and the records of the busiest frame.
 * Links against librxcore.dylib: clang -std=c11 -O1 -I core/include ambient_census.c -L<dir> -lrxcore -Wl,-rpath,<dir> */
#include "rx_api.h"
#include "rx_crowd_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static const char* k_kind[17] = {"SingleAnimLoop", "ScenarioAnim", "SplineAnimLoop", "CrewChief", "Instanced", "SingleLoopParticles", "SplineLoopParticles",
                                 "LandAnimal", "BillboardCrowd", "ScoreTowerFlagHolder", "TrackSideFlagHolder", "SplineStop", "SplineStopParticles",
                                 "Proximity", "Trigger", "AirAnimal", "ThirtySecondBoard"};

int main(int argc, char** argv) {
    if (argc < 9) { fprintf(stderr, "usage: see the file header\n"); return 2; }
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]);
    e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]);
    e.location_id = atoi(argv[5]);
    e.track_id = 0;
    e.vehicle_class = atoi(argv[6]);
    e.duration = atoi(argv[7]);
    snprintf(e.duration_type, sizeof e.duration_type, "%s", argv[8]);
    e.riders = 1;
    e.ai_difficulty = 0;
    e.event_activation[0] = 0;
    const int frames_after = argc > 9 ? atoi(argv[9]) : 30;
    if (rx_session_event_load(s, &e) != 0) { printf("load: %s\n", rx_session_last_error(s)); return 1; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < 20000) { if (rx_session_advance(s) != 0) break; frames++; }
    for (int i = 0; i < frames_after && rx_session_get_state(s) != RX_SESSION_ERROR; i++) if (rx_session_advance(s) != 0) break;
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("error: %s\n", rx_session_last_error(s)); return 1; }
    rx_ambient_info info;
    memset(&info, 0, sizeof info);
    (void)rx_session_ambient_entities(s, NULL, 0, NULL, 0, &info);
    const int cap = info.entities + 16, mcap = info.matrices + 16;
    rx_ambient_entity* ae = (rx_ambient_entity*)calloc((size_t)cap, sizeof *ae);
    float* am = (float*)calloc((size_t)mcap * 16, sizeof(float));
    const int n = rx_session_ambient_entities(s, ae, (uint32_t)cap, am, (uint32_t)mcap, &info);
    printf("%s %s: %d ambient entities, %d matrices, %d objects (fade_start %.2f lod_divisor %.2f), load %d frames\n", argv[4], argv[2], n, info.matrices,
           info.objects, info.fade_start, info.lod_divisor, frames);
    int count[17] = {0}, vis[17] = {0};
    for (int i = 0; i < n; i++) if (ae[i].kind >= 0 && ae[i].kind < 17) { count[ae[i].kind]++; vis[ae[i].kind] += (ae[i].flags & 1u) != 0; }
    for (int k = 0; k < 17; k++) if (count[k]) printf("  %-22s %5d (visible %d)\n", k_kind[k], count[k], vis[k]);
    if (getenv("AMBIENT_CENSUS_MODELS")) {
        for (int i = 0; i < n; i++) {
            int seen = 0;
            for (int j = 0; j < i && !seen; j++) seen = ae[j].kind == ae[i].kind && strcmp(ae[j].model, ae[i].model) == 0;
            if (seen) continue;
            int c = 0, v = 0;
            for (int j = i; j < n; j++) if (ae[j].kind == ae[i].kind && strcmp(ae[j].model, ae[i].model) == 0) { c++; v += (ae[j].flags & 1u) != 0; }
            printf("  model %-20s %-40s x%d (visible %d) lod %.0f bones %d\n", ae[i].kind < 17 ? k_kind[ae[i].kind] : "?", ae[i].model, c, v, ae[i].lod_distance, ae[i].bones);
        }
    }
    if (getenv("AMBIENT_CENSUS_BILLBOARD"))
        for (int i = 0; i < n; i++) if (ae[i].kind == 8)
            printf("  crowd #%d %s at %.1f %.1f %.1f phase %.3f empty %.3f nonanim %.0f flashes %.0f lod %.0f\n", ae[i].entity_id, ae[i].model,
                   ae[i].matrix[12], ae[i].matrix[13], ae[i].matrix[14], ae[i].param[0], ae[i].param[1], ae[i].param[2], ae[i].param[3], ae[i].lod_distance);
    if (getenv("AMBIENT_CENSUS_FLASHES")) {
        const int extra = atoi(getenv("AMBIENT_CENSUS_FLASHES"));
        static rx_crowd_flash fl[4096], busiest[4096];
        int with = 0, most = 0, mismatch = 0, bad = 0, nbusy = 0, frame_busy = -1, total = 0;
        float amax = 0, lmin = 1e9f, lmax = 0, rmin = 1e9f, rmax = -1e9f;
        for (int f = 0; f < extra && rx_session_get_state(s) != RX_SESSION_ERROR; f++) {
            if (rx_session_advance(s) != 0) break;
            const int na = rx_session_ambient_entities(s, ae, (uint32_t)cap, am, (uint32_t)mcap, &info);
            int sum = 0;
            for (int i = 0; i < na; i++) if (ae[i].kind == 8) sum += (int)ae[i].param[3];
            const int c = rx_session_crowd_flashes(s, NULL, 0);
            const int w = rx_session_crowd_flashes(s, fl, 4096);
            if (c != sum || w != (c < 4096 ? c : 4096)) mismatch++;
            if (c > 0) with++;
            total += c;
            if (c > most) { most = c; frame_busy = f; nbusy = w; memcpy(busiest, fl, (size_t)w * sizeof *fl); }
            for (int q = 0; q < w; q++) {
                if (fl[q].age < 0 || fl[q].age > fl[q].length + 0.1f || fl[q].length < 0.0099f || fl[q].length > 0.1101f || fl[q].seat >= 500u) bad++;
                if (fl[q].age > amax) amax = fl[q].age;
                if (fl[q].length < lmin) lmin = fl[q].length;
                if (fl[q].length > lmax) lmax = fl[q].length;
                if (fl[q].empty_chair_ratio < rmin) rmin = fl[q].empty_chair_ratio;
                if (fl[q].empty_chair_ratio > rmax) rmax = fl[q].empty_chair_ratio;
            }
        }
        printf("flashes over %d frames: frames with flashes %d, most at once %d (frame %d), total flash-frames %d, count mismatches vs ambient export %d, out-of-range records %d,"
               " age max %.3f, length %.3f .. %.3f, empty ratio %.3f .. %.3f\n", extra, with, most, frame_busy, total, mismatch, bad, amax, lmin, lmax, rmin, rmax);
        for (int q = 0; q < nbusy && q < 12; q++)
            printf("  flash section %08x #%d binding %d seat %u age %.4f length %.4f ratio %.3f\n", busiest[q].object_id, busiest[q].entity_id, busiest[q].binding, busiest[q].seat,
                   busiest[q].age, busiest[q].length, busiest[q].empty_chair_ratio);
    }
    rx_session_destroy(s);
    return 0;
}
