/* terrain_tiles_trace.c -- w-terrtiles: a per-frame hash trace of the exported terrain and tyre-track arrays, run against any librxcore.
 *
 *   terrain_tiles_trace <librxcore.dylib> <install root (the Archive dir or "game files")> <out trace prefix> [frames] [location id name] [riders]
 *
 * It loads a National (default NAT_Renne, 12 riders, AI 95), runs `frames` race frames with corebench's pad script and, every frame:
 *   terrain  rx_session_terrain; when deform_version changed, the heights + deformation of the single dirty rectangle are hashed
 *            (<prefix>.terrain: frame version rectangle hash). That file is the same for a core without rx_session_terrain_tiles (the
 *            baseline) and one with it, so `cmp` of the two traces is the before / after proof.
 *   tiles    (cores that have rx_session_terrain_tiles) the tile rectangles are applied to a mirror of the heights + deformation, the
 *            single rectangle to another one: they must be equal every frame, and equal to the full export every 25 frames; the
 *            rectangle count, the tiles' texels and the single rectangle's texels are summed (<prefix>.tiles).
 *   tracks   the tyre-track exports in a different order of calls each frame (count / triangles / ribbons) with different
 *            max_triangles; what comes back (info, triangles, ribbons) is hashed (<prefix>.tracks), the same for any order and any core.
 * Exit 0 when every check held.
 */
#include "rx_api.h"
#include <dlfcn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

static uint64_t fnv(uint64_t h, const void* p, size_t n) { const uint8_t* b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }
#define SYM(name) __typeof__(&name) f_##name = (__typeof__(&name))dlsym(lib, #name); if (!f_##name) { fprintf(stderr, "missing %s\n", #name); return 2; }
static void pad(uint8_t keys[256], const rx_remap_table* r, int frame) {
    memset(keys, 0, 256);
    keys[r->keyboard[0] & 255] = 0x80;
    const int phase = frame % 240;
    if (phase >= 120 && phase < 150) keys[r->keyboard[2] & 255] = 0x80;
    if (phase >= 200 && phase < 220) keys[r->keyboard[3] & 255] = 0x80;
}
static double now_ms(void) { struct timespec t; clock_gettime(CLOCK_MONOTONIC, &t); return t.tv_sec * 1e3 + t.tv_nsec / 1e6; }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: terrain_tiles_trace <lib> <install> <out prefix> [frames] [NAT_Renne:54] [riders]\n"); return 2; }
    void* lib = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    if (!lib) { fprintf(stderr, "%s\n", dlerror()); return 2; }
    SYM(rx_session_create) SYM(rx_session_destroy)
    SYM(rx_session_last_error) SYM(rx_session_event_default)
    SYM(rx_session_event_load) SYM(rx_session_advance)
    SYM(rx_session_get_state) SYM(rx_session_set_input)
    SYM(rx_session_terrain) SYM(rx_session_terrain_heights)
    SYM(rx_session_terrain_deform) SYM(rx_session_tracks)
    SYM(rx_session_track_ribbons) SYM(rx_remap_defaults)
    #define rx_session_create f_rx_session_create
    #define rx_session_destroy f_rx_session_destroy
    #define rx_session_last_error f_rx_session_last_error
    #define rx_session_event_default f_rx_session_event_default
    #define rx_session_event_load f_rx_session_event_load
    #define rx_session_advance f_rx_session_advance
    #define rx_session_get_state f_rx_session_get_state
    #define rx_session_set_input f_rx_session_set_input
    #define rx_session_terrain f_rx_session_terrain
    #define rx_session_terrain_heights f_rx_session_terrain_heights
    #define rx_session_terrain_deform f_rx_session_terrain_deform
    #define rx_session_tracks f_rx_session_tracks
    #define rx_session_track_ribbons f_rx_session_track_ribbons
    #define rx_remap_defaults f_rx_remap_defaults
    int (*tiles_fn)(struct rx_session*, rx_terrain_rect*, int32_t) = (int (*)(struct rx_session*, rx_terrain_rect*, int32_t))dlsym(lib, "rx_session_terrain_tiles");
    const int frames = argc > 4 ? atoi(argv[4]) : 3000;
    const char* loc = "NAT_Renne"; int loc_id = 54;
    if (argc > 5) { loc = argv[5]; loc_id = argc > 6 ? atoi(argv[6]) : 54; }
    const int riders = argc > 7 ? atoi(argv[7]) : 12;
    char p[4400];
    snprintf(p, sizeof p, "%s.terrain", argv[3]); FILE* ft = fopen(p, "w");
    snprintf(p, sizeof p, "%s.tiles", argv[3]); FILE* fti = tiles_fn ? fopen(p, "w") : NULL;
    snprintf(p, sizeof p, "%s.tracks", argv[3]); FILE* fk = fopen(p, "w");
    struct rx_session* s = rx_session_create(argv[2]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "National"); e.event_type_id = 10;
    snprintf(e.location, sizeof e.location, "%s", loc); e.location_id = loc_id;
    e.track_id = 0; e.vehicle_class = 2; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = 95; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    for (int i = 0; rx_session_get_state(s) == RX_SESSION_LOADING && i < 3000; i++) if (rx_session_advance(s)) break;
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { fprintf(stderr, "load error: %s\n", rx_session_last_error(s)); return 1; }
    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    uint8_t keys[256];

    rx_terrain_info ti; memset(&ti, 0, sizeof ti);
    uint32_t last_version = 0, n = 0;
    float *mh1 = NULL, *md1 = NULL, *mh2 = NULL, *md2 = NULL, *fh = NULL, *fd = NULL, *th = NULL, *td = NULL;   /* single-rect mirror, tile mirror, full export, scratch */
    static float tris[200000 * 18];
    static rx_track_ribbon rib[256];
    static float tris_small[1000 * 18], tris_big[100000 * 18];
    static float fetch_prev[65536 * 18], fetch_now[65536 * 18];
    uint64_t fetches = 0, fetch_verts = 0, fetch_changed = 0, fetch_range = 0, fetch_identical = 0; int fetch_prev_tris = 0;
    uint64_t total_single = 0, total_tiles = 0, changed_frames = 0, max_rects = 0, rect_hist[17] = {0};
    int bad = 0; double t_terr = 0, t_tiles = 0, t_tracks = 0;
    for (int f = 1; f <= frames; f++) {
        pad(keys, &remap, f);
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s)) { fprintf(stderr, "advance failed at %d: %s\n", f, rx_session_last_error(s)); return 1; }
        /* ---- terrain: the single rectangle (the baseline's answer) */
        double t0 = now_ms();
        if (rx_session_terrain(s, &ti)) continue;
        t_terr += now_ms() - t0;
        if (!mh1) {
            n = ti.samples; size_t N = (size_t)n * n;
            mh1 = calloc(N, 4); md1 = calloc(N, 4); mh2 = calloc(N, 4); md2 = calloc(N, 4); fh = calloc(N, 4); fd = calloc(N, 4); th = calloc(N, 4); td = calloc(N, 4);
            last_version = ti.deform_version - 1;   /* the first call: everything */
        }
        if (ti.deform_version != last_version) {
            last_version = ti.deform_version;
            int x0 = ti.dirty_x0 < 0 ? 0 : ti.dirty_x0, z0 = ti.dirty_z0 < 0 ? 0 : ti.dirty_z0, x1 = ti.dirty_x1 > (int)n ? (int)n : ti.dirty_x1, z1 = ti.dirty_z1 > (int)n ? (int)n : ti.dirty_z1;
            uint64_t h = 0xcbf29ce484222325ull;
            if (x1 > x0 && z1 > z0) {
                const int w = x1 - x0, hh = z1 - z0;
                rx_session_terrain_heights(s, th, x0, z0, w, hh); rx_session_terrain_deform(s, td, x0, z0, w, hh);
                h = fnv(h, th, (size_t)w * hh * 4); h = fnv(h, td, (size_t)w * hh * 4);
                for (int r = 0; r < hh; r++) { memcpy(&mh1[(size_t)(z0 + r) * n + x0], &th[(size_t)r * w], (size_t)w * 4); memcpy(&md1[(size_t)(z0 + r) * n + x0], &td[(size_t)r * w], (size_t)w * 4); }
                total_single += (uint64_t)w * hh;
            }
            fprintf(ft, "%d %u %d %d %d %d %016llx\n", f, ti.deform_version, ti.dirty_x0, ti.dirty_z0, ti.dirty_x1, ti.dirty_z1, (unsigned long long)h);
            ++changed_frames;
            /* ---- terrain: the tile rectangles of the same frame */
            if (tiles_fn) {
                t0 = now_ms();
                rx_terrain_rect rc[RX_TERRAIN_MAX_RECTS];
                int nr = tiles_fn(s, rc, RX_TERRAIN_MAX_RECTS);
                t_tiles += now_ms() - t0;
                if (nr < 1 || nr > RX_TERRAIN_MAX_RECTS) { fprintf(stderr, "frame %d: %d rectangles\n", f, nr); bad = 1; nr = 0; }
                uint64_t texels = 0;
                for (int i = 0; i < nr; i++) {
                    const int w = rc[i].x1 - rc[i].x0, hh = rc[i].z1 - rc[i].z0;
                    if (w <= 0 || hh <= 0 || rc[i].x0 % 32 || rc[i].z0 % 32 || (rc[i].x1 % 32 && rc[i].x1 != (int)n) || (rc[i].z1 % 32 && rc[i].z1 != (int)n)) { fprintf(stderr, "frame %d: rect %d malformed\n", f, i); bad = 1; continue; }
                    rx_session_terrain_heights(s, th, rc[i].x0, rc[i].z0, w, hh); rx_session_terrain_deform(s, td, rc[i].x0, rc[i].z0, w, hh);
                    for (int r = 0; r < hh; r++) { memcpy(&mh2[(size_t)(rc[i].z0 + r) * n + rc[i].x0], &th[(size_t)r * w], (size_t)w * 4); memcpy(&md2[(size_t)(rc[i].z0 + r) * n + rc[i].x0], &td[(size_t)r * w], (size_t)w * 4); }
                    texels += (uint64_t)w * hh;
                    fprintf(fti, "%d r%d %d %d %d %d\n", f, i, rc[i].x0, rc[i].z0, rc[i].x1, rc[i].z1);
                }
                total_tiles += texels; if ((uint64_t)nr > max_rects) max_rects = (uint64_t)nr; rect_hist[nr]++;
                
                /* the repeated call in the same frame gives the same answer */
                rx_terrain_rect again[RX_TERRAIN_MAX_RECTS]; int na = tiles_fn(s, again, RX_TERRAIN_MAX_RECTS);
                if (na != nr || memcmp(again, rc, sizeof(rx_terrain_rect) * (size_t)nr)) { fprintf(stderr, "frame %d: tiles not stable in a frame\n", f); bad = 1; }
                if (memcmp(mh1, mh2, (size_t)n * n * 4) || memcmp(md1, md2, (size_t)n * n * 4)) { fprintf(stderr, "frame %d: tile mirror differs from the single-rectangle mirror\n", f); bad = 1; }
                fprintf(fti, "%d tiles %d texels %llu single %d\n", f, nr, (unsigned long long)texels, (x1 - x0) * (z1 - z0));
            }
        } else if (tiles_fn) {
            rx_terrain_rect rc[RX_TERRAIN_MAX_RECTS];
            if (tiles_fn(s, rc, RX_TERRAIN_MAX_RECTS) != 0) { fprintf(stderr, "frame %d: tiles reported while nothing changed\n", f); bad = 1; }
        }
        if (f % 25 == 0 && mh1) {   /* the full export against both mirrors */
            rx_session_terrain_heights(s, fh, 0, 0, (int)n, (int)n); rx_session_terrain_deform(s, fd, 0, 0, (int)n, (int)n);
            if (memcmp(fh, mh1, (size_t)n * n * 4) || memcmp(fd, md1, (size_t)n * n * 4)) { fprintf(stderr, "frame %d: single-rectangle mirror differs from the full export\n", f); bad = 1; }
            if (tiles_fn && (memcmp(fh, mh2, (size_t)n * n * 4) || memcmp(fd, md2, (size_t)n * n * 4))) { fprintf(stderr, "frame %d: tile mirror differs from the full export\n", f); bad = 1; }
            uint64_t h = fnv(fnv(0xcbf29ce484222325ull, fh, (size_t)n * n * 4), fd, (size_t)n * n * 4);
            fprintf(ft, "%d full %016llx\n", f, (unsigned long long)h);
        }
        /* ---- tracks: a different call order and request size each frame; the answers must not depend on either */
        if (f % 2 == 0) {
            t0 = now_ms();
            uint32_t want = f % 7 == 0 ? 1000 : f % 11 == 0 ? 100000 : f % 13 == 0 ? 0 : 65536;
            float* buf = want == 1000 ? tris_small : want == 100000 ? tris_big : tris;
            rx_track_info info_a, info_b, info_c; int nw = 0, nr = 0;
            const int order = f % 3;
            if (order == 0) { nw = rx_session_tracks(s, want ? buf : NULL, want, &info_a); nr = rx_session_track_ribbons(s, rib, 256, &info_b); rx_session_tracks(s, NULL, 0, &info_c); }
            else if (order == 1) { rx_session_tracks(s, NULL, 0, &info_c); nw = rx_session_tracks(s, want ? buf : NULL, want, &info_a); nr = rx_session_track_ribbons(s, rib, 256, &info_b); }
            else { nr = rx_session_track_ribbons(s, rib, 256, &info_b); rx_session_tracks(s, NULL, 0, &info_c); nw = rx_session_tracks(s, want ? buf : NULL, want, &info_a); }
            t_tracks += now_ms() - t0;
            uint64_t h = 0xcbf29ce484222325ull;
            const int32_t head[6] = { (int32_t)info_a.version, info_a.triangles, info_a.vertices, info_a.strips, info_a.chunks, nw };
            h = fnv(h, head, sizeof head);
            if (memcmp(&info_a, &info_b, sizeof info_a) && (info_a.triangles != info_b.triangles || info_a.strips != info_b.strips)) { fprintf(stderr, "frame %d: tracks info differs between calls\n", f); bad = 1; }
            if (info_c.triangles != info_a.triangles || info_c.chunks != info_a.chunks) { fprintf(stderr, "frame %d: tracks count differs between calls\n", f); bad = 1; }
            if (nw > 0) h = fnv(h, buf, (size_t)nw * 18 * 4);
            if (nr > 0) h = fnv(h, rib, (size_t)nr * sizeof rib[0]);
            fprintf(fk, "%d want %u tri %d strips %d chunks %d written %d ribbons %d %016llx\n", f, want, info_a.triangles, info_a.strips, info_a.chunks, nw, nr, (unsigned long long)h);
        }
        /* the Unity OriginalTracks.Apply diff: every 4th frame, how many of the vertices of the track list differ from the last fetch */
        if (f % 4 == 0) {
            rx_track_info ki; const int nt = rx_session_tracks(s, fetch_now, 65536, &ki);
            const int nv = nt * 3; int lo = nv, hi = 0; uint64_t changed = 0;
            for (int v = 0; v < nv; v++) {
                if (v >= fetch_prev_tris * 3 || memcmp(&fetch_now[v * 6], &fetch_prev[v * 6], 24)) { changed++; if (v < lo) lo = v; hi = v + 1; }
            }
            ++fetches; fetch_verts += (uint64_t)nv; fetch_changed += changed; fetch_range += hi > lo ? (uint64_t)(hi - lo) : 0; if (!changed && nt == fetch_prev_tris) ++fetch_identical;
            memcpy(fetch_prev, fetch_now, (size_t)nt * 18 * 4); fetch_prev_tris = nt;
        }
        if (f % 1000 == 0) { rx_track_info ki; rx_session_tracks(s, NULL, 0, &ki); fprintf(stderr, "frame %d, tracks triangles %d, chunks %d\n", f, ki.triangles, ki.chunks); }
    }
    fprintf(stderr, "frames %d, terrain changes %llu, single-rectangle texels %llu%s", frames, (unsigned long long)changed_frames, (unsigned long long)total_single, tiles_fn ? "" : "\n");
    if (tiles_fn) {
        fprintf(stderr, ", tile texels %llu (%.1f%% of the single rectangles), max rectangles %llu\n", (unsigned long long)total_tiles, total_single ? 100.0 * (double)total_tiles / (double)total_single : 0.0, (unsigned long long)max_rects);
        fprintf(stderr, "rectangles per changed frame:"); for (int i = 0; i <= 16; i++) if (rect_hist[i]) fprintf(stderr, " %d:%llu", i, (unsigned long long)rect_hist[i]); fprintf(stderr, "\n");
    }
    fprintf(stderr, "track list fetches (every 4th frame): %llu, mean vertices %.0f, mean changed vs the last fetch %.0f (%.1f%%), mean upload range %.0f (%.1f%%), identical fetches %llu\n",
            (unsigned long long)fetches, fetches ? (double)fetch_verts / fetches : 0.0, fetches ? (double)fetch_changed / fetches : 0.0, fetch_verts ? 100.0 * (double)fetch_changed / fetch_verts : 0.0,
            fetches ? (double)fetch_range / fetches : 0.0, fetch_verts ? 100.0 * (double)fetch_range / fetch_verts : 0.0, (unsigned long long)fetch_identical);
    fprintf(stderr, "ms per call: rx_session_terrain %.4f, tiles %.4f (per changed frame), tracks (3 calls, every 2nd frame) %.4f\n", t_terr / frames, changed_frames ? t_tiles / (double)changed_frames : 0.0, t_tracks / (frames / 2.0));
    fclose(ft); if (fti) fclose(fti); fclose(fk);
    rx_session_destroy(s);
    fprintf(stderr, bad ? "FAIL\n" : "PASS\n");
    return bad;
}
