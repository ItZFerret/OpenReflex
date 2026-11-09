/* grid_rows_test.c (w-secondrow) -- the start grid's rows behind the gate (core/game/grid_rows.h, core/plan/GRID_ROWS.md), headless, through
 * the PRODUCT session API: where the places behind the gate are, how every rider leaves the grid, and the race to the finish per row.
 *   grid_rows_test <install root> <type> <type id> <level> <location id> <class> <laps> <AI difficulty> [frames] [riders]
 *          RX_GRID_CAP=8 grid_rows_test "game files/Archive" National 10 NAT_Concord 50 2 3 95
 * Knobs (tools only):
 *   RX_GRID_CAP=<n>         read only n of the gate's slots (rx_grid_rows_test_cap): a 12-rider race with n < 12 has 12 - n riders behind it
 *   RX_MAP_SCALE=<s> / RX_MAP_SCALE_BASE=<b>   the race's MAP SCALE and its track's default (Alive: 1.1 / 1.3)
 *   RX_PLAYER_GO=1          the player rides too: full throttle, straight, from GO for 4 s (else it stays at its slot with no input)
 * Diagnostics: RX_GATE_OPEN=1 (an Alive gate's bar collision off), RX_TRACE_ZONE=<z> (that rider's vehicle every 10 frames; RX_TRACE_ALL=1
 * every frame around GO), RX_TRACE_GO=1 (every vehicle around GO), RX_NEAR_OBJECTS=1 / RX_NEAR_STATICS=1 (scene objects / static collision
 * records near the first place), RX_GROUND_MAP=1 (the first static obstacle behind the gate per 4 ft across; RX_GROUND_MAP_FULL=1 the whole
 * height map, RX_MAP_FROM / _TO / _STEP / _SIDE0 / _SIDES its range), RX_RAY_WALL=1 (flat rays forward from the first place).
 * Lines: the grid (slots, riders, pitch, row depth, the slots' height over their ground) and every place behind the gate (zone, row,
 * across, position, ground: terrain or static, its track section, static collision in its lane); every rider's zone, row, start position;
 * per rider at GO: its creep before GO (ft), its distance from the start at GO + 3 s and GO + 6 s, crashes in the first 20 s; the race:
 * place, laps, time, best lap, crashes, longest stall (as alive_race_test). RESULT lines per row and overall.
 * Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/grid_rows_test.c */
#include "rx_api.h"
#include "rx_mem.h"
#include "../game/grid_rows.h"
#include "../game/alive_gate.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXS 16
#define MAXLAPS 32

struct rider {
    int seen, laps, crashes, early_crashes, wrecked_prev, finished, zone, row, across, local, place;
    float laptime[MAXLAPS], stall_s, stall_max_s, x0, z0, creep, d3, d6, time, best;
};

static float hdist(uint32_t v, const struct rider* q) { const float dx = F32(v, 0xc0) - q->x0, dz = F32(v, 0xc8) - q->z0; return sqrtf(dx * dx + dz * dz); }

int main(int argc, char** argv) {
    if (argc < 9) {
        fprintf(stderr, "usage: grid_rows_test <install root> <type> <type id> <level> <location id> <class> <laps> <difficulty> [frames] [riders]\n");
        return 2;
    }
    const int frames = argc > 9 ? atoi(argv[9]) : 30 * 60 * 60;
    const int riders = argc > 10 ? atoi(argv[10]) : 12;
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("RX_MAP_SCALE_BASE") && rx_session_set_map_scale_base(s, (float)atof(getenv("RX_MAP_SCALE_BASE"))) != 0) { printf("RESULT base: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("RX_MAP_SCALE") && rx_session_set_map_scale(s, (float)atof(getenv("RX_MAP_SCALE"))) != 0) { printf("RESULT scale: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("RX_GRID_CAP")) rx_grid_rows_test_cap((uint32_t)atoi(getenv("RX_GRID_CAP")));
    const int player_go = getenv("RX_PLAYER_GO") != NULL;
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.track_id = 0; e.vehicle_class = atoi(argv[6]); e.duration = atoi(argv[7]); snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = atoi(argv[8]); e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT event load failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f < 3000) { if (rx_session_advance(s) != 0) break; f++; }
    if (rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT load error: %s\n", rx_session_last_error(s)); return 1; }
    printf("loaded after %d frames\n", f);
    if (getenv("RX_GATE_OPEN")) rx_alive_gate_set_collision(0);   /* (diagnostic: an Alive gate's bar collision off from the start) */

    rx_grid_info gi; rx_grid_place_info pl[RX_GRID_SLOTS];
    const uint32_t np = rx_grid_rows_info(&gi, pl, RX_GRID_SLOTS);
    printf("grid: %d slots, %d riders, %u places behind the gate, pitch %.2f ft, row depth %.2f ft, slots %.2f ft over their ground\n",
           gi.slots, gi.riders, np, gi.pitch, gi.depth, gi.clearance);
    int moved = 0, unclear = 0, offtrack = 0;
    static const char* const k_how[5] = {"preferred", "moved back", "moved forward", "beside", "NO CLEAR SPOT"};
    static const char* const k_why[4] = {"", " (uneven ground there)", " (an obstacle ahead there)", " (something in the way there)"};
    for (uint32_t k = 0; k < np; k++) {
        const rx_grid_place_info* q = &pl[k];
        printf("place zone %2d row %d across %2d at (%.1f, %.2f, %.1f) dir (%.2f, %.2f, %.2f) depth %.2f ft shift %+.2f ft: %s%s; ground %.2f (%s), track %s\n",
               q->zone, q->row, q->across, q->pos[0], q->pos[1], q->pos[2], q->dir[0], q->dir[1], q->dir[2], q->depth, q->shift,
               k_how[q->how < 0 || q->how > 4 ? 4 : q->how], q->how ? k_why[q->why < 0 || q->why > 3 ? 0 : q->why] : "", q->ground,
               q->on_static ? "static" : "terrain", q->found_section == 1 ? "same section" : q->found_section == 2 ? "OTHER section" : "no quad");
        moved += q->how >= 1 && q->how <= 3; unclear += q->how == 4; offtrack += q->found_section == 2;
    }

    if (getenv("RX_NEAR_OBJECTS") && np > 0) {   /* (diagnostic) the scene objects within 40 ft of the first place behind the gate */
        static rx_scene_object obj[8192];
        const int no = rx_session_scene_objects(s, obj, 8192);
        for (int i = 0; i < no; i++) {
            const float dx = obj[i].matrix[12] - pl[0].pos[0], dy = obj[i].matrix[13] - pl[0].pos[1], dz = obj[i].matrix[14] - pl[0].pos[2];
            if (dx * dx + dz * dz < 40.0f * 40.0f) printf("object %08x %-40s %-40s at (%.1f, %.1f, %.1f) dist %.1f\n", obj[i].vtable, obj[i].name, obj[i].model,
                obj[i].matrix[12], obj[i].matrix[13], obj[i].matrix[14], sqrtf(dx * dx + dy * dy + dz * dz));
        }
    }
    if (getenv("RX_NEAR_STATICS") && np > 0) {   /* (diagnostic) the static collision records whose box reaches the places behind the gate */
        const rx_addr A = U32(U32(U32(U32(0, 0x00a946b8), 0x7460 + 0x94), 0), 0) ? 0 : 0; (void)A;
        const rx_addr mxw = U32(U32(0, 0x00a946b8), 0x5a54), world = U32(mxw, 0x20134);
        float lo[3] = {1e9f, 1e9f, 1e9f}, hi[3] = {-1e9f, -1e9f, -1e9f};
        for (uint32_t k = 0; k < np; k++) for (int c = 0; c < 3; c++) { if (pl[k].pos[c] - 5 < lo[c]) lo[c] = pl[k].pos[c] - 5; if (pl[k].pos[c] + 5 > hi[c]) hi[c] = pl[k].pos[c] + 5; }
        const uint32_t nb = (U32(world, 0x19c4) - U32(world, 0x19c0)) >> 6;
        for (uint32_t b = 0; b < nb; b++) {
            const rx_addr body = U32(world, 0x19c0) + 0x40 * b;
            const float* mn = (const float*)0; (void)mn;
            float a0[3] = {F32(body, 0), F32(body, 4), F32(body, 8)}, a1[3] = {F32(body, 0x10), F32(body, 0x14), F32(body, 0x18)};
            int hit = 1;
            { const float dx = a0[0] - pl[0].pos[0], dz = a0[2] - pl[0].pos[2]; if (dx * dx + dz * dz > 40.0f * 40.0f) hit = 0; }
            if (!hit) continue;
            const rx_addr owner = U32(body, 0x30);
            char name[64] = "";
            if (owner && U32(owner, 0x18)) { for (int i = 0; i < 63; i++) { name[i] = (char)U8(U32(owner, 0x18), (uint32_t)i); if (!name[i]) break; } }
            printf("static %4u box (%.1f %.1f %.1f) .. (%.1f %.1f %.1f) on %u owner %08x %s shape %08x layer %08x\n", b, a0[0], a0[1], a0[2], a1[0], a1[1], a1[2],
                   U8(body, 0x38), owner, name, U32(body, 0x28), U32(body, 0x28) ? U32(U32(body, 0x28), 0x10) : 0);
        }
    }
    if (getenv("RX_GROUND_MAP") && np > 0) {   /* (diagnostic) the ground behind the gate: rows every 0.5 ft back from the slot line, columns across */
        const float* fw = gi.fwd; const float* ac = gi.across;
        /* the slot line: the first place minus its row's depth */
        const float bx = pl[0].pos[0] + fw[0] * pl[0].depth - ac[0] * pl[0].shift, bz = pl[0].pos[2] + fw[2] * pl[0].depth - ac[2] * pl[0].shift, by = pl[0].pos[1];
        for (float back = getenv("RX_MAP_FROM") ? (float)atof(getenv("RX_MAP_FROM")) : -6.0f; back <= (getenv("RX_MAP_TO") ? (float)atof(getenv("RX_MAP_TO")) : 30.0f); back += getenv("RX_MAP_STEP") ? (float)atof(getenv("RX_MAP_STEP")) : 0.5f) {
            if (!getenv("RX_GROUND_MAP_FULL")) break;
            printf("map back %5.1f:", back);
            const float s0 = getenv("RX_MAP_SIDE0") ? (float)atof(getenv("RX_MAP_SIDE0")) : -40.0f, s1 = -s0, ss = getenv("RX_MAP_SIDES") ? (float)atof(getenv("RX_MAP_SIDES")) : 4.0f;
            for (float side = s0; side <= s1; side += ss) {
                float h = 0; int32_t st = 0;
                const float x = bx - fw[0] * back + ac[0] * side, z = bz - fw[2] * back + ac[2] * side;
                if (rx_grid_ground_probe(gi.activity, x, by, z, &h, &st)) printf(" %6.2f%c", h - by, st ? 's' : ' '); else printf("    --  ");
            }
            printf("\n");
        }
        /* the summary: per column across the gate, the first static surface more than 0.5 ft above the slots' ground behind the slot line */
        printf("barrier (ft behind the slot line, per 4 ft across; - none within 30 ft):");
        for (float side = -40.0f; side <= 40.0f; side += 4.0f) {
            float found = -1.0f;
            for (float back = 0.0f; back <= 30.0f && found < 0.0f; back += 0.25f) {
                float h = 0; int32_t st = 0;
                const float x = bx - fw[0] * back + ac[0] * side, z = bz - fw[2] * back + ac[2] * side;
                if (rx_grid_ground_probe(gi.activity, x, by, z, &h, &st) && st && h > by - gi.clearance + 0.5f) found = back;
            }
            if (found < 0.0f) printf("    -"); else printf(" %4.1f", found);
        }
        printf("\n");
    }
    if (getenv("RX_RAY_WALL") && np > 0) {   /* (diagnostic) flat rays from the first place forward 20 ft, every half foot up, at lateral offsets */
        for (float side = -2.0f; side <= 2.01f; side += 1.0f) for (float up = 0.5f; up <= 7.01f; up += 0.5f) {
            const float o[3] = {pl[0].pos[0] + gi.across[0] * side, pl[0].pos[1] - gi.clearance + up, pl[0].pos[2] + gi.across[2] * side};
            const float e[3] = {o[0] + gi.fwd[0] * 20.0f, o[1], o[2] + gi.fwd[2] * 20.0f};
            float h[3];
            if (rx_grid_ray_probe(gi.activity, o, e, h)) printf("ray side %+.0f up %.1f: hit %.2f ft ahead\n", side, up, (h[0] - o[0]) * gi.fwd[0] + (h[2] - o[2]) * gi.fwd[2]);
            else printf("ray side %+.0f up %.1f: clear\n", side, up);
        }
    }
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    rx_driver_result r[MAXS]; rx_slot sl[MAXS];
    struct rider rd[MAXS]; memset(rd, 0, sizeof rd);
    const float dt = 1.0f / 60.0f;
    int started = 0, placed = 0, n = 0, go = -1;
    const rx_addr game = U32(0, 0x00a946b8);
    for (f = 0; f < frames; f++) {
        rx_pad pad; memset(&pad, 0, sizeof pad);
        if (player_go && go >= 0 && f - go < 240) pad.right_trigger = 255;   /* (pad 0's throttle) */
        rx_session_set_input(s, keys, &pad, player_go ? 1u : 0u);
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) {
            printf("RESULT error at frame %d: %s\n", f, rx_session_last_error(s));
            return 1;
        }
        n = rx_session_standings(s, r, MAXS);
        const int ns = rx_session_slots(s, sl, MAXS);
        if (!placed && ns >= riders) {
            int all = 1;
            for (int j = 0; j < riders; j++) all &= sl[j].vehicle != 0 && F32(sl[j].vehicle, 0xc4) != 0.0f;
            if (all) {
                placed = 1;
                for (int j = 0; j < riders; j++) {
                    struct rider* q = &rd[sl[j].slot];
                    const rx_addr P = game + 0x7460 + 0x12a0u * (uint32_t)sl[j].slot;
                    q->zone = I32(P, 0x1dc); q->local = sl[j].local;
                    uint32_t row = 0, across = 0;
                    if (rx_grid_place((uint32_t)gi.slots, (uint32_t)q->zone, &row, &across) == 0) { q->row = (int)row; q->across = (int)across; }
                    q->x0 = F32(sl[j].vehicle, 0xc0); q->z0 = F32(sl[j].vehicle, 0xc8);
                    const uint32_t S = U32(sl[j].vehicle, 0x234);
                    printf("frame %d: rider %2d %s zone %2d row %d across %2d at (%.1f, %.2f, %.1f) wheels (%.2f %.2f %.2f) r %.2f (%.2f %.2f %.2f) r %.2f\n", f, sl[j].slot, sl[j].local ? "player" : "AI    ",
                           q->zone, q->row, q->across, q->x0, F32(sl[j].vehicle, 0xc4), q->z0, F32(S, 0x230), F32(S, 0x234), F32(S, 0x238), F32(S, 0x88f0),
                           F32(S, 0x2d0), F32(S, 0x2d4), F32(S, 0x2d8), F32(S, 0x87a0));
                }
            }
        }
        for (int i = 0; i < n; i++) if (r[i].race_time > 0.0f && go < 0) go = f;
        if (getenv("RX_TRACE_GO") && placed && (go < 0 ? f >= 366 : f - go < 3))
            for (int j = 0; j < ns; j++) {
                const uint32_t v = sl[j].vehicle;
                printf("tracego f %d zone %2d pos (%.2f, %.2f, %.2f) speed %.1f v+0x180 %u v+0x200 (%.2f %.2f %.2f)\n", f, rd[sl[j].slot].zone, F32(v, 0xc0), F32(v, 0xc4), F32(v, 0xc8), F32(v, 0x150),
                       U32(v, 0x180), F32(v, 0x200), F32(v, 0x204), F32(v, 0x208));
            }
        if (getenv("RX_TRACE_ZONE") && placed && (getenv("RX_TRACE_ALL") ? (f > 355 && f < 380) : (f % 10 == 0)) && (go < 0 || f - go < 400))   /* RX_TRACE_ZONE=<z>: that rider's vehicle every 10 frames */
            for (int j = 0; j < ns; j++) {
                const struct rider* q = &rd[sl[j].slot];
                if (q->zone != atoi(getenv("RX_TRACE_ZONE"))) continue;
                const uint32_t v = sl[j].vehicle;
                const uint32_t S = U32(v, 0x234);
                printf("trace f %d (go %+d) zone %d pos (%.2f, %.2f, %.2f) speed %.1f wreck %u up (%.2f, %.2f, %.2f) sim (%.2f, %.2f, %.2f)\n", f, go < 0 ? 0 : f - go, q->zone,
                       F32(v, 0xc0), F32(v, 0xc4), F32(v, 0xc8), F32(v, 0x150), U32(v, 0x280), F32(v, 0x140), F32(v, 0x144), F32(v, 0x148),
                       S ? F32(S, 0x180) : 0.0f, S ? F32(S, 0x184) : 0.0f, S ? F32(S, 0x188) : 0.0f);
            }
        for (int j = 0; j < ns && placed; j++) {
            struct rider* q = &rd[sl[j].slot];
            if (go < 0) { const float d = hdist(sl[j].vehicle, q); if (d > q->creep) q->creep = d; }
            else if (f - go == 180) q->d3 = hdist(sl[j].vehicle, q);
            else if (f - go == 360) q->d6 = hdist(sl[j].vehicle, q);
        }
        for (int i = 0; i < n; i++) {
            const int k = r[i].slot;
            if (k < 0 || k >= MAXS) continue;
            struct rider* q = &rd[k];
            q->seen = 1;
            if (r[i].laps_completed > q->laps && r[i].laps_completed <= MAXLAPS) { q->laptime[r[i].laps_completed - 1] = r[i].last_lap; q->laps = r[i].laps_completed; }
            if (r[i].race_time > 0.0f) started = 1;
            if (r[i].finished) q->finished = 1;
            if (r[i].local) continue;
            uint32_t v = 0;
            for (int j = 0; j < ns; j++) if (sl[j].slot == k) v = sl[j].vehicle;
            if (!v || !started || q->finished) continue;
            const int wrecked = U32(v, 0x280) != 0;
            if (wrecked && !q->wrecked_prev) { q->crashes++; if (f - go < 1200) q->early_crashes++; }
            q->wrecked_prev = wrecked;
            if (F32(v, 0x150) < 5.0f) { q->stall_s += dt; if (q->stall_s > q->stall_max_s) q->stall_max_s = q->stall_s; }
            else q->stall_s = 0.0f;
        }
        if (f % 60) continue;
        int ai = 0, done = 0;
        for (int i = 0; i < n; i++) if (!r[i].local) { ai++; done += r[i].finished != 0; }
        if (ai > 0 && done == ai) break;
    }
    n = rx_session_standings(s, r, MAXS);
    struct { int riders, fin, crashes, early, slow; float stall, place, d6min, creepmax; } row[4];
    memset(row, 0, sizeof row);
    for (int k = 0; k < 4; k++) row[k].d6min = 1e9f;
    for (int i = 0; i < n; i++) {
        struct rider* q = &rd[r[i].slot];
        q->place = i + 1; q->time = r[i].race_time; q->best = r[i].best_lap < 1e30f ? r[i].best_lap : 0.0f;
        printf("rider %2d %s zone %2d row %d place %2d laps %d %s time %8.3f best %8.3f crashes %d (first 20 s %d) stall %5.1f s creep %.2f ft go+3s %.1f ft go+6s %.1f ft laps:",
               r[i].slot, r[i].local ? "player" : "AI    ", q->zone, q->row, i + 1, r[i].laps_completed, r[i].finished ? "finished" : "racing  ", q->time, q->best,
               q->crashes, q->early_crashes, q->stall_max_s, q->creep, q->d3, q->d6);
        for (int l = 0; l < q->laps && l < MAXLAPS; l++) printf(" %.2f", q->laptime[l]);
        printf("\n");
        if (r[i].local) continue;
        const int k = q->row >= 1 && q->row <= 3 ? q->row : 0;
        row[k].riders++; row[k].fin += r[i].finished != 0; row[k].crashes += q->crashes; row[k].early += q->early_crashes;
        if (q->stall_max_s > row[k].stall) row[k].stall = q->stall_max_s;
        row[k].place += (float)(i + 1);
        if (q->d6 < row[k].d6min) row[k].d6min = q->d6;
        if (q->creep > row[k].creepmax) row[k].creepmax = q->creep;
        row[k].slow += q->d6 < 30.0f;
    }
    int tot = 0, fin = 0, cr = 0; float stall = 0.0f;
    for (int k = 1; k < 4; k++) {
        if (!row[k].riders) continue;
        tot += row[k].riders; fin += row[k].fin; cr += row[k].crashes; if (row[k].stall > stall) stall = row[k].stall;
        printf("RESULT row %d: %d AI, %d finished, mean place %.1f, crashes %.2f per rider (first 20 s: %d), longest stall %.1f s, creep before GO <= %.2f ft, "
               "at GO + 6 s >= %.1f ft from the start (%d under 30 ft)\n", k, row[k].riders, row[k].fin, row[k].place / (float)row[k].riders,
               (float)row[k].crashes / (float)row[k].riders, row[k].early, row[k].stall, row[k].creepmax, row[k].d6min, row[k].slow);
    }
    printf("RESULT %s %s riders %d: %d / %d AI finished in %.1f s, crashes %.2f per rider, longest stall %.1f s; places behind the gate %u "
           "(moved for the ground %d, no clear spot %d, other track section %d)\n", argv[2], argv[4], riders, fin, tot, f / 60.0,
           tot ? (float)cr / (float)tot : 0.0f, stall, np, moved, unclear, offtrack);
    rx_session_destroy(s);
    return 0;
}
