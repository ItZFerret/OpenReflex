/* champ_collision_probe.c -- RESTORED CONTENT probe (w-champweekend): track-object collision in a championship stage vs a single race.
 *   champ_collision_probe <install root> <race|stage> [nat|sx (nat)] [location] [stage index (0)] [frames (2400)]
 * race: the discipline's exhibition event as a plain single race; stage: the same event with rx_session_champ_stage_apply of the
 * weekend's stage <index> (laps only). The local rider rides straight off the gate at a part throttle (no steering: on NAT_Concord and
 * SX_Springfield it runs into the first turn's banners / tuff blocks). Prints:
 *   ACTIVITY  the activity's class (vtable, its 72 slots), type, mode, area, the area loaded flag (+0xec8);
 *   GRID      per object vector of the physics world (world +0x20134: keyed +0x1994, static +0x19b4, dynamic +0x1954) its records, how
 *             many of them the broadphase grid holds (a cell list or the overflow) and the grid's extent: Activity::vf5 (00556c30)
 *             finalizes the world (00769030: the extent of every box, the cells rebuilt); without it the grids keep the zero extent
 *             of their construction and hold only what their first cell takes (the vehicles' contact queries miss the rest);
 *   HIT       the local vehicle's contact events in the world (world +0, 64 bytes, count +0x1900; as mp_collide_test reads them) with
 *             anything but a vehicle, by kind: x a static object (+0x19c0 records, owner entity +0x30), k a keyed object (+0x19a0,
 *             +0x34), d a generic dynamic one (+0x1960, +0x34), s a surface id (the bike's own impact, bike_response.c); with the
 *             owner entity's Name (+0x18);
 *   SUMMARY   the frames with a track-object contact (x / k / d) and with a surface impact.
 * TEST ONLY: reads core state (rx_mem), writes nothing. Build: product_link.sh's rx_product_objs + rx_link_driver (as build_event_test.sh). */
#include "rx_api.h"
#include "rx_champ_weekend.h"
#include "../game/build_activity_util.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static void on_signal(int sig) { char b[80]; int n = snprintf(b, sizeof b, "champ_collision_probe: signal %d\n", sig); if (n > 0) (void)write(2, b, (size_t)n); _exit(128 + sig); }

/* the hit objects by kind and name: a generic dynamic object (world +0x1960, id +0x34), a keyed object (+0x19a0, id +0x34), a static
 * object (+0x19c0, owner +0x30), else a surface id (the bike's impact on a terrain / static surface, bike_response.c) */
#define MAX_HITS 64
static struct { char kind; char name[48]; int frames; } hits[MAX_HITS];
static int nhits;
static void entity_name(rx_addr e, char* out, size_t cap) {
    out[0] = 0;
    const rx_addr n = e ? U32(e, 0x18) : 0;
    if (!n) { snprintf(out, cap, "(entity %08x)", e); return; }
    size_t i = 0; for (; i + 1 < cap && U8(n, (uint32_t)i); i++) out[i] = (char)U8(n, (uint32_t)i);
    out[i] = 0;
}
static char note_hit(rx_addr W, uint32_t other) {
    char kind = 's', name[48];
    snprintf(name, sizeof name, "surface %u", other);
    for (rx_addr r = U32(W, 0x1960); r < U32(W, 0x1964); r += 0xe0) if (U32(r, 0x34) == other) { kind = 'd'; entity_name(other, name, sizeof name); }
    for (rx_addr r = U32(W, 0x19a0); r < U32(W, 0x19a4); r += 0xe0) if (U32(r, 0x34) == other) { kind = 'k'; entity_name(other, name, sizeof name); }
    for (rx_addr r = U32(W, 0x19c0); r < U32(W, 0x19c4); r += 0x40) if (U32(r, 0x30) == other && other) { kind = 'x'; entity_name(other, name, sizeof name); }
    for (int i = 0; i < nhits; i++) if (hits[i].kind == kind && strcmp(hits[i].name, name) == 0) { hits[i].frames++; return kind; }
    if (nhits < MAX_HITS) { hits[nhits].kind = kind; snprintf(hits[nhits].name, sizeof hits[nhits].name, "%s", name); hits[nhits].frames = 1; nhits++; }
    return kind;
}

/* a broadphase grid's coverage (phys_coll_grid.c): its extent (+0x430 / +0x440), the records of the vector (begin +0xc, end +0x10,
 * the grid object +0x18 -> +0x30) whose index is in a cell list (+0x470, +0x474 counts, +0x414 per cell, +0x410 cells) or the
 * overflow (+0x47c, +0x478 count) */
static void grid_report(const char* label, rx_addr pw, uint32_t vec_off, uint32_t elem) {
    const rx_addr v = pw + vec_off;
    const uint32_t n = (U32(v, 0x10) - U32(v, 0xc)) / elem;
    if (!U32(v, 0x18)) { printf("  GRID %-7s %4u records, no grid\n", label, n); return; }
    const rx_addr g = U32(U32(v, 0x18), 0x30);
    static unsigned char seen[65536];
    memset(seen, 0, sizeof seen);
    const int32_t cells = I32(g, 0x410), cap = I32(g, 0x414);
    for (int32_t c = 0; c < cells; c++) for (int32_t k = 0; k < I32(U32(g, 0x474), 4u * (uint32_t)c) && k < cap; k++) {
        const uint32_t id = U32(U32(g, 0x470), 4u * (uint32_t)(cap * c + k)); if (id < 65536) seen[id] = 1;
    }
    for (int32_t k = 0; k < I32(g, 0x478); k++) { const uint32_t id = U32(U32(g, 0x47c), 4u * (uint32_t)k); if (id < 65536) seen[id] = 1; }
    uint32_t in = 0; for (uint32_t i = 0; i < n && i < 65536; i++) in += seen[i];
    printf("  GRID %-7s %4u records, %4u in the grid (overflow %d), extent x %.0f .. %.0f z %.0f .. %.0f\n", label, n, in, I32(g, 0x478),
           (double)F32(g, 0x430), (double)F32(g, 0x440), (double)F32(g, 0x438), (double)F32(g, 0x448));
}

static rx_addr local_vehicle(rx_session* s) {
    rx_slot sl[32]; const int n = rx_session_slots(s, sl, 32);
    for (int i = 0; i < n; i++) if (sl[i].local) return sl[i].vehicle;
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: champ_collision_probe <install> <race|stage> [nat|sx] [location] [stage] [frames]\n"); return 2; }
    const int stage_mode = strcmp(argv[2], "stage") == 0;
    const int sx = argc > 3 && strcmp(argv[3], "sx") == 0;
    const char* location = argc > 4 ? argv[4] : sx ? "SX_Springfield" : "NAT_Concord";
    const int stage = argc > 5 ? atoi(argv[5]) : 0;
    const int frames = argc > 6 ? atoi(argv[6]) : 2400;
    signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGABRT, on_signal);
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_champ_weekend w;
    if (rx_champ_weekend_begin(&w, sx ? RX_CHAMP_SUPERCROSS : RX_CHAMP_NATIONAL) != 0) return 1;
    for (int k = 0; k < w.stage_count; k++) (void)rx_champ_weekend_set_timing(&w, k, 0, 3);
    w.stage = stage;
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    snprintf(spec.location, sizeof spec.location, "%s", location);
    if (sx) { snprintf(spec.event_type, sizeof spec.event_type, "Supercross"); spec.event_type_id = 8; spec.location_id = 78; }
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "event load: %s\n", rx_session_last_error(s)); return 1; }
    if (stage_mode && rx_session_champ_stage_apply(s, &w) != 0) { fprintf(stderr, "apply: %s\n", rx_session_last_error(s)); return 1; }
    printf("PROBE %s %s stage %d (%s)\n", location, stage_mode ? "STAGE" : "RACE", stage, stage_mode ? w.stages[stage].label : "-");
    int last_state = -1, total_contacts = 0, prop_frames = 0, object_frames = 0, surface_frames = 0, first_object = -1;
    for (int f = 1; f <= frames; f++) {
        rx_pad pad; uint8_t keys[256]; memset(&pad, 0, sizeof pad); memset(keys, 0, sizeof keys);
        rx_race_state race; memset(&race, 0, sizeof race);
        const int have = rx_session_race(s, &race) == 0 && race.valid;
        if (have && race.state == 2) pad.right_trigger = 160;
        rx_session_set_input(s, keys, &pad, 1);
        if (rx_session_advance(s) != 0) { fprintf(stderr, "advance %d: %s\n", f, rx_session_last_error(s)); return 1; }
        const rx_addr V = local_vehicle(s);
        if (!V) continue;
        const rx_addr P = U32(V, 0x190), A = P ? U32(P, 0x94) : 0;
        const rx_addr h = U32(V + 0x90, 0xec), W = h ? U32(h, 4) : 0, R = W ? U32(W, 0x1960) + U32(h, 8) * 0xe0 : 0;
        if (f == 30 && A) {
            printf("ACTIVITY %08x vt %08x type 0x%x mode %u area %u ec8 %u\n  slots", A, U32(A, 0), U32(A, 0x23c), U32(A, 0x238), U32(A, 0x220), U32(A, 0xec8));
            for (int i = 0; i < 72; i++) printf(" %d:%08x", i, U32(U32(A, 0), 4u * (uint32_t)i));
            printf("\n");
            const rx_addr pw = U32(U32(A, 0x234), 0x20134);
            grid_report("keyed", pw, 0x1994, 0xe0);
            grid_report("static", pw, 0x19b4, 0x40);
            grid_report("dynamic", pw, 0x1954, 0xe0);
        }
        if (rx_session_race(s, &race) == 0 && race.valid && race.state != last_state) {
            printf("frame %5d state %d ghost bits %08x\n", f, race.state, R ? U32(U32(R, 0x2c), 0x14) : 0);
            last_state = race.state;
        }
        if (!W || !R) continue;
        const uint32_t me = U32(R, 0x34);
        int n = 0, object = 0, surface = 0; float dv = 0;
        for (uint32_t e = 0; e < U32(W, 0x1900) && e < 100; e++) {
            const rx_addr ev = W + e * 64; const uint32_t x = U32(ev, 0), y = U32(ev, 4);
            if (x != me && y != me) continue;
            const uint32_t other = x == me ? y : x;
            int vehicle = 0;   /* the other body a vehicle's? */
            rx_slot sl[32]; const int ns = rx_session_slots(s, sl, 32);
            for (int q = 0; q < ns; q++) {
                const rx_addr hv = sl[q].vehicle ? U32(sl[q].vehicle + 0x90, 0xec) : 0;
                if (hv && U32(U32(U32(hv, 4), 0x1960) + U32(hv, 8) * 0xe0, 0x34) == other) vehicle = 1;
            }
            if (vehicle) continue;
            n++; dv += x == me ? F32(ev, 0x24) : F32(ev, 0x28);
            if (note_hit(W, other) == 's') surface = 1; else object = 1;
        }
        object_frames += object; surface_frames += surface;
        if (object && first_object < 0) first_object = f;
        if (n) { total_contacts += n; prop_frames++; if (prop_frames <= 10) printf("frame %5d prop contacts %d dv %.2f\n", f, n, (double)dv); }
        if (f % 300 == 0) {
            rx_bike_state b; if (rx_session_bike(s, &b) == 0)
                printf("frame %5d pos %.1f %.1f %.1f speed %.1f ghost %08x events %u\n", f, (double)b.position[0][0], (double)b.position[0][1], (double)b.position[0][2], (double)b.speed, U32(U32(R, 0x2c), 0x14), U32(W, 0x1900));
        }
    }
    printf("SUMMARY %s %s: track-object contact on %d frames (first %d), surface impacts on %d frames (%d events)\n", location,
           stage_mode ? w.stages[stage].label : "RACE", object_frames, first_object, surface_frames, total_contacts);
    for (int i = 0; i < nhits; i++) printf("  HIT %c %-40s %d\n", hits[i].kind, hits[i].name, hits[i].frames);
    rx_session_destroy(s);
    return 0;
}
