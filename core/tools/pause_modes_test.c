/* pause_modes_test.c -- the read-only exports the pause / results pages of every offline mode read (w-pausemodes): rx_session_event_ui_state (every
 * activity class the shell offers: type, state, Post sub-state), rx_session_race_hud (the Free Ride's Hit Discovery counts) and
 * rx_session_mode_results (the Waypoint / Freestyle / Section Race standings), sampled while the event runs and through its natural end.
 *   pause_modes_test <install root> <type> <typeId> <level> <locationId> <trackId> <vehicleClass> <duration> <durationType> <riders> <ai>
 *                    <activation|-> <frames> [W (hold the throttle)] [env PM_CAREER=1: InCareerMode]
 * prints "STATE" lines each time (event type, state, post, known) changes and every <PM_EVERY> frames (default 600) a "SAMPLE" line, and a final
 * "RESULT ok|fail". Link: clang -std=c11 -O1 -I core/include tools/pause_modes_test.c -L build -lrxcore -Wl,-rpath,$PWD/build */
#include "rx_api.h"
#include "rx_race_hud.h"
#include "rx_mode_results.h"
#include "../game/rx_activation_api.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(int argc, char** argv) {
    if (argc < 14) { fprintf(stderr, "usage: see the file header\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT fail create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("PM_CAREER") && rx_session_event_in_career_mode(s, 1) != 0) { printf("RESULT fail in_career_mode\n"); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]); e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]); e.location_id = atoi(argv[5]); e.track_id = atoi(argv[6]);
    e.vehicle_class = atoi(argv[7]); e.duration = atoi(argv[8]); snprintf(e.duration_type, sizeof e.duration_type, "%s", argv[9]);
    e.riders = atoi(argv[10]); e.ai_difficulty = atoi(argv[11]); e.event_activation[0] = 0;
    if (strcmp(argv[12], "-") != 0) snprintf(e.event_activation, sizeof e.event_activation, "%s", argv[12]);
    const int frames = atoi(argv[13]); const int every = getenv("PM_EVERY") ? atoi(getenv("PM_EVERY")) : 600;
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    if (argc > 14 && strcmp(argv[14], "W") == 0) keys[17] = 0x80;
    if (rx_session_event_load(s, &e) != 0) { printf("RESULT fail load: %s\n", rx_session_last_error(s)); return 1; }
    int n = 0, last_key = -1, bad = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && n < 4000) { if (rx_session_advance(s) != 0) break; n++; }
    rx_session_set_input(s, keys, NULL, 0);
    for (int i = 0; i < frames && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
        if (rx_session_advance(s) != 0) break;
        rx_event_ui_state u; memset(&u, 0, sizeof u);
        rx_session_event_ui_state(s, &u);
        rx_race_hud_state h; memset(&h, 0, sizeof h); h.size = sizeof h; h.version = RX_RACE_HUD_VERSION;
        rx_session_race_hud(s, &h);
        rx_mode_results m; memset(&m, 0, sizeof m); m.size = sizeof m; m.version = RX_MODE_RESULTS_VERSION;
        if (rx_session_mode_results(s, &m) != 0) { printf("RESULT fail mode_results\n"); return 1; }
        rx_race_state rs; memset(&rs, 0, sizeof rs); (void)rx_session_race(s, &rs);
        const int key = (int)(u.known * 131 + u.activity_type * 17 + u.race_state * 5 + u.post_state * 3 + u.rules_hold);
        const int changed = key != last_key;
        if (changed) {
            printf("STATE frame %d known %x type %d state %d rules %d post %d | hud type %d | discovery %d/%d events %u | race valid %d laps %d/%d riders %d place %d\n", i, u.known, u.activity_type, u.race_state,
                   u.rules_hold, u.post_state, h.activity_type, h.discovery_found, h.discovery_total, h.discovery_events, rs.valid, rs.laps_completed, rs.num_laps, rs.riders, rs.place);
            last_key = key;
        }
        if (u.known && (u.known & RX_EVENT_UI_ACTIVITY_TYPE) && h.valid && h.activity_type != u.activity_type) { printf("MISMATCH hud type %d vs event ui %d\n", h.activity_type, u.activity_type); bad++; }
        if (changed || (every > 0 && i % every == 0) || (u.race_state >= 3 && i % 200 == 0)) {
            printf("SAMPLE frame %d state %d post %d mode_results valid %u type %d rows %u:", i, u.race_state, u.post_state, m.valid, m.activity_type, m.count);
            for (uint32_t k = 0; k < m.count && k < 4; k++)
                printf(" [slot %d%s place %d fin %d t %.2f score %.2f best %.2f/%s secs %d best_sec %d %.2f]", m.rows[k].slot, m.rows[k].local ? "*" : "", m.rows[k].place, m.rows[k].finished,
                       (double)m.rows[k].total_time, (double)m.rows[k].score, (double)m.rows[k].best_trick_score, m.rows[k].best_trick, m.rows[k].sections_won, m.rows[k].best_section,
                       (double)m.rows[k].best_section_time);
            printf("\n");
        }
    }
    printf("RESULT %s frames %d state %d\n", bad ? "fail" : "ok", frames, rx_session_get_state(s));
    return bad ? 1 : 0;
}
