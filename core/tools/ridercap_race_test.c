/* ridercap_race_test.c (w-rc-core) -- a race of up to 32 riders to the finish, headless, through the PRODUCT session API, with the checks of
 * the rider cap (core/plan/RIDER_CAP.md "Done"): every computer rider finishes, the standings hold every rider once with places 1 .. n,
 * the player slots / standings / highlight recorder past the original's 12 (game/rider_cap.h side storage) are filled and consistent.
 * Not shipped with the game.
 *   ridercap_race_test <install root> <type> <type id> <level> <location id> <class> <laps> <riders> [frames]
 *     e.g. ridercap_race_test "game files" National 10 NAT_Concord 50 2 3 32
 * The player stays at the gate (no input); the race runs until every computer rider finished (or `frames`, default 30 min of game time);
 * then the player's laps are set done (the oracle's finish stimulus RX_RNG_FINISH_STIM, as stop_audit_run's STOP_AUDIT_FINISH: D +0xf1c =
 * A +0xff070) so the race ends by its own rule and reaches Post (the results page's table, the end-of-race replay of the highlight
 * clips: the exhibition replay presenter is installed, every request applied and painted, as Unity does), then up to 60 s more. Prints a line per rider and one RESULT line:
 *   finished x / y, places ok|BAD (finishers in race-time order ahead of the others, places within 1 .. n), slots ok|BAD, clips <saved clips> (side entries <n>), ring <riders with recorded samples> / <riders>,
 *   places-list at 5 s <off below 12> / <off at 12+> (the recorder's places list against the riders' places 5 s into the race: 0 / 0),
 *   results <rows> (populated <n>), hash <h> (the standings' slots, places, times and the game LCG count: the same
 *   build and arguments print the same hash every run)
 * RX_MAP_SCALE / RX_MAP_SCALE_BASE as alive_race_test. RX_HEAP_REDZONE=<bytes> (rx_heap.h): the heap's blocks checked for writes past
 * their end every 600 frames and at the end ("redzone <n>" in the RESULT line; addresses differ from a run without it). Static link with the product objects:
 *   C=$PWD/core O=<out>; source core/tools/product_link.sh; rx_product_objs && rx_link_driver <exe> core/tools/ridercap_race_test.c */
#include "rx_api.h"
#include "rx_results.h"
#include "rx_rng.h"
#include "rx_replay_api.h"
#include "rx_mem.h"
#include "rider_cap.h"
#include "rx_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAXS 40
#define CLIP_SLOT(i) (0x00d323c0u + 0x35d0u * (uint32_t)(i))

static uint64_t fnv(uint64_t h, const void* p, size_t n) { const uint8_t* b = p; for (size_t i = 0; i < n; i++) { h ^= b[i]; h *= 0x100000001b3ull; } return h; }

int main(int argc, char** argv) {
    setvbuf(stdout, NULL, _IOLBF, 0);
    if (argc < 9) { fprintf(stderr, "usage: ridercap_race_test <root> <type> <type id> <level> <location id> <class> <laps> <riders> [frames]\n"); return 2; }
    const int riders = atoi(argv[8]);
    const int frames = argc > 9 ? atoi(argv[9]) : 30 * 60 * 60;
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("RX_MAP_SCALE_BASE") && rx_session_set_map_scale_base(s, (float)atof(getenv("RX_MAP_SCALE_BASE"))) != 0) { printf("RESULT base: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("RX_MAP_SCALE") && rx_session_set_map_scale(s, (float)atof(getenv("RX_MAP_SCALE"))) != 0) { printf("RESULT scale: %s\n", rx_session_last_error(s)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]);
    e.track_id = 0; e.vehicle_class = atoi(argv[6]); e.duration = atoi(argv[7]); snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    e.riders = riders; e.ai_difficulty = 95; e.event_activation[0] = 0;
    uint64_t gen = 0;
    if (rx_session_replay_begin_event(s, &gen) != 0) { printf("RESULT replay presenter: %s\n", rx_session_last_error(s)); return 1; }
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT event load failed: %s\n", rx_session_last_error(s)); return 1; }
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    rx_driver_result r[MAXS]; rx_slot sl[MAXS];
    int f = 0, n = 0, ns = 0, post = -1, maxslots = 0, plist5_low = -1, plist5_high = -1;
    for (; f < frames + 3600; f++) {
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance(s) != 0 || rx_session_get_state(s) == RX_SESSION_ERROR) { printf("RESULT error at frame %d: %s\n", f, rx_session_last_error(s)); return 1; }
        for (int guard = 0; guard < 64; guard++) {   /* the replay presenter */
            rx_replay_request rq; memset(&rq, 0, sizeof rq); rq.size = sizeof rq; rq.version = RX_REPLAY_API_VERSION;
            if (rx_session_replay_next(s, gen, &rq) != 0) break;
            if (rx_session_replay_ack(s, gen, &rq, RX_REPLAY_APPLIED) || rx_session_replay_ack(s, gen, &rq, RX_REPLAY_PAINTED)) break;
        }
        ns = rx_session_slots(s, sl, MAXS);
        if (ns > maxslots) maxslots = ns;
        rx_race_state rs;
        if (rx_session_race(s, &rs) == 0 && rs.valid && rs.state == 4 && post < 0) { post = f; printf("frame %d: the race's Post\n", f); }
        /* the highlight recorder's places list (0xa7f3a8 + 4 i, by driver index; rider_cap.h side table past 12): noted for every
         * rider until 7 s of race clock, then only the local human's (006ab5b0): compared with the places at 5 s */
        if (rs.valid && rs.state == 2 && plist5_low < 0) {
            const rx_addr a = U32(0, 0x00a942b8);
            if (a && F32(a, 0x294) > 5.0f && F32(a, 0x294) < 6.5f) {
                plist5_low = plist5_high = 0;
                for (int i = 0; i < I32(a, 0x26c) && i < (int)RX_RC_MAX; i++)
                    if (U32(rx_rc_tab(0x00a7f3a8u, 4, (uint32_t)i), 0) != U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x78)) { if (i < 12) plist5_low++; else plist5_high++; }
            }
        }
        if (f % 600 == 0 && rx_heap_redzone_check(0) > 0) { printf("frame %d: heap blocks written past their end:\n", f); rx_heap_redzone_check(8); }
        if (f % 60) continue;
        n = rx_session_standings(s, r, MAXS);
        int ai = 0, done = 0;
        for (int i = 0; i < n; i++) if (!r[i].local) { ai++; done += r[i].finished != 0; }
        static int all_done = -1;
        if (ai > 0 && done == ai && all_done < 0) {
            all_done = f; printf("frame %d: every computer rider finished\n", f);
            const rx_addr A = U32(0, 0x00a942b8);   /* the race activity (the clip recorder's): the local human finishes */
            for (int32_t k = 0; A && k < I32(A, 0x26c); k++) {
                const rx_addr D = U32(U32(A, 0x268), 4u * (uint32_t)k), P = D ? U32(D, 0x74) : 0;
                if (P && U32(P, 0x10) == 0 && U32(P, 4) != 0) { SU32(D, 0xf1c, U32(A, 0xff070)); printf("frame %d: the player's laps set done\n", f); break; }
            }
        }
        if (all_done >= 0 && (post >= 0 || f - all_done > 3600)) break;
        if (all_done < 0 && f >= frames) break;
    }
    n = rx_session_standings(s, r, MAXS);
    int fin = 0, ais = 0, places_ok = n == riders, seen_place[MAXS + 1] = {0}, seen_slot[MAXS] = {0};
    uint64_t h = 0xcbf29ce484222325ull;
    for (int i = 0; i < n; i++) {
        printf("rank %2d slot %2d place %2d %s laps %d %s time %8.3f best %8.3f\n", i, r[i].slot, r[i].place, r[i].local ? "player" : "AI    ",
               r[i].laps_completed, r[i].finished ? "finished" : "racing  ", r[i].race_time, r[i].best_lap < 1e30f ? r[i].best_lap : 0.0f);
        if (!r[i].local) { ais++; fin += r[i].finished != 0; }
        if (r[i].slot < 0 || r[i].slot >= MAXS || seen_slot[r[i].slot]++) places_ok = 0;
        /* the standings order: finishers by race time (the places +0x78 may repeat after the race: outside InProgress the race's
         * comparator never answers 1, rx_api.h rx_session_standings), every rider's place within 1 .. n */
        if (r[i].place < 1 || r[i].place > n) places_ok = 0;
        if (r[i].finished && i > 0 && r[i - 1].finished && r[i].race_time < r[i - 1].race_time) places_ok = 0;
        if (r[i].finished && i > 0 && !r[i - 1].finished) places_ok = 0;
        (void)seen_place;
        h = fnv(h, &r[i].slot, 4); h = fnv(h, &r[i].place, 4); h = fnv(h, &r[i].finished, 4); h = fnv(h, &r[i].race_time, 4);
    }
    rx_rng_state rng; memset(&rng, 0, sizeof rng);
    if (rx_session_rng(s, &rng) == 0) h = fnv(h, &rng, sizeof rng);
    /* the highlight recorder: clips saved, their field snapshots past 12, the ring's samples of every rider, the places list */
    int clips = 0, side_entries = 0, ring = 0, plist_ok = 1;
    for (int i = 0; i < 12; i++) {
        if (U32(CLIP_SLOT(i), 8) == 0) continue;
        clips++;
        for (uint32_t k = RX_RC_ORIG; k < (uint32_t)riders && k < RX_RC_MAX; k++) { const rx_addr en = rx_clip_entry(CLIP_SLOT(i) + 0xc, k, 0); if (en && U32(en, 0)) side_entries++; }
    }
    for (uint32_t k = 0; k < (uint32_t)riders && k < RX_RC_MAX; k++) {
        int any = 0;
        for (uint32_t smp = 0; smp < RX_RC_CLIP_SAMPLES && !any; smp++) {
            const rx_addr rec = rx_clip_record(rx_clip_index(0, k, smp));
            for (uint32_t o = 0; o < 0x10 && !any; o += 4) any = U32(rec, o) != 0;
        }
        ring += any;
    }
    int plist_low = 0, plist_high = 0;   /* (information) the places list against the places after the race: the computer riders' entries
                                          * are their places at 7 s (006ab5b0 notes only the local human's after that), so most differ */
    {
        const rx_addr a = U32(0, 0x00a942b8);
        if (a) for (int i = 0; i < I32(a, 0x26c) && i < (int)RX_RC_MAX; i++) {
            const uint32_t want = U32(U32(U32(a, 0x268), 4u * (uint32_t)i), 0x78);
            if (U32(rx_rc_tab(0x00a7f3a8u, 4, (uint32_t)i), 0) != want) { if (i < 12) plist_low++; else plist_high++; }
        }
        (void)plist_ok;
    }
    rx_results_page page; memset(&page, 0, sizeof page); page.size = sizeof page; page.version = RX_RESULTS_VERSION;
    int rows = -1, populated = -1, rows_ok = 1;
    if (rx_session_results_page(s, &page) == 0) {
        rows = page.rows; populated = (int)page.populated;
        int seen[MAXS] = {0};
        for (int i = 0; i < page.rows; i++) if (page.row[i].slot < 0 || page.row[i].slot >= MAXS || seen[page.row[i].slot]++) rows_ok = 0;
        if (page.populated && page.rows != riders) rows_ok = 0;
    }
    printf("RESULT %s %s riders %d slots %d: finished %d / %d in %.1f s, places %s, slots %s, clips %d (side entries %d), ring %d / %d, "
           "places-list at 5 s %d / %d off (at the end %d / %d), results %d (populated %d, %s), post %s, redzone %d, hash %016llx\n",
           argv[2], argv[4], riders, maxslots, fin, ais, f / 60.0, places_ok ? "ok" : "BAD", maxslots == riders ? "ok" : "BAD", clips, side_entries,
           ring, riders, plist5_low, plist5_high, plist_low, plist_high, rows, populated, rows_ok ? "ok" : "BAD", post >= 0 ? "yes" : "no", rx_heap_redzone_check(8), (unsigned long long)h);
    rx_session_destroy(s);
    return 0;
}
