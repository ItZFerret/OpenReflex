/* champ_weekend_test.c -- RESTORED CONTENT test: a whole championship weekend (National, Supercross, Waypoint, Freestyle) (include/rx_champ_weekend.h) through the
 * product session API, no emulator, no Unity.
 *   champ_weekend_test <install root> [nat|sx|wp|fs (nat)] [location (NAT_Concord / SX_Springfield / FR_Gibson / FMX_BuenaVista)] [max frames per stage (40000)] [AI difficulty (95)]
 * For each stage: a session, the discipline's exhibition event at the location (rx_session_event_default + the type and the location),
 * rx_session_champ_stage_apply, the exhibition replay presenter (rx_session_replay_begin_event: this driver acts as its renderer, every
 * request applied and painted, as stop_audit_run STOP_AUDIT_REPLAY=1 / Unity's ReplayWorker), frames with the race-line autopilot on pad 0 (career_series_test's) while racing, until the activity
 * reaches Post and STAGE_POST_FRAMES more, then rx_session_champ_stage_record and the session destroyed.
 * Prints the activity states with their frames, each stage's places and the weekend's standings. REFLEX_TEST_LAPS=<n> shortens the
 * races (the original's debug lap override). Exit 0 = every stage ran and was recorded; 1 = an API call failed or a stage did not end
 * in the frame budget; 128 + signal = a fault outside the session guard.
 * TIMED RACES (version 4): CW_TIMED=<minutes>+<laps> runs every National / Supercross stage on that clock (the practices + 0); without
 * it every stage is a 3-lap race (minutes 0: the version 3 weekend). A timed stage prints the clock's expiry and the flag, and at its end
 * every driver's laps / finished and whether the places follow the laps (a check, exit 1 when it fails).
 * FIELD SIZE: CW_RIDERS=<2..32> loads every stage's event with that many riders (rx_event_spec.riders, the exhibition path the game's
 * FIELD SIZE takes: OpenSetup.LaunchRiders -> OriginalWorldRuntime.OpenEventSpec); unset: the event's default 12. Each stage then prints
 * its field ("FIELD <stage>: n riders") and a Supercross weekend the heat's split ("SPLIT").
 *   build (static: the waypoint gate steering reads core state): bash core/build.sh, then product_link.sh's rx_link_driver <exe>
 *   core/tools/champ_weekend_test.c (or clang with -Icore/include -Icore/src -Icore/game -Icore/api and the objects of core/build/objs.list). */
#include "rx_api.h"
#include "rx_champ_weekend.h"
#include "rx_replay_api.h"
#include "rx_podium_api.h"
/* the gate steering reads the waypoint activity's gates from core state: the driver links the product's objects (product_link.sh) */
#include "../game/build_activity_util.h"
#include "../game/rx_waypoint_api.h"
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define STAGE_POST_FRAMES 240

static const char* g_step = "start";
uint32_t rx_grid_active_cell(uint32_t row);   /* build_activity_grid.c (TEST ONLY: CW_PROBE_ROWS) */
static void on_signal(int sig) {
    char buf[200];
    const int n = snprintf(buf, sizeof buf, "champ_weekend_test: signal %d during: %s\n", sig, g_step);
    if (n > 0) (void)write(2, buf, (size_t)n);
    _exit(128 + sig);
}

/* ---- the race-line autopilot (career_series_test.c's: pad 0 left stick steering, right trigger throttle, left trigger brake) ---- */
static float wrap_angle(float angle) {
    while (angle > 3.14159265f) angle -= 6.2831853f;
    while (angle < -3.14159265f) angle += 6.2831853f;
    return angle;
}
struct autopilot { float stuck_clock, reverse_clock, detour_clock; int detours, reset_frames; float anchor[3]; float anchor_clock; };
static int g_gate_pilot;   /* this frame steers at a waypoint gate (no race line) */
static int g_tricks;   /* Freestyle: no race line; ride about and circle the right stick (career_series_test's CST_TRICKS) */
/* TEST ONLY, the waypoint race's next gate for the local rider, read from core state (no API has it): the local slot's vehicle V, its
 * player P = V+0x190, P's activity A = P+0x94 and driver (A+0x268 with D+0x74 == P); the gate after the last one passed, as
 * rx_waypoint_driver_distance_to_next (00517d20) picks it (D+0x10d4 passed, D+0x10d8 count; A+0xff078 the gates, A+0xff074 their
 * count); its position from its world matrix (the entity's vf15 0x3c, row 3). Returns 0 when there is none. */
static int waypoint_next_gate(struct rx_session* s, float out[3]) {
    rx_slot sl[16];
    const int n = rx_session_slots(s, sl, 16);
    rx_addr v = 0;
    for (int i = 0; i < n; i++) if (sl[i].local) v = sl[i].vehicle;
    if (!v) return 0;
    const rx_addr p = U32(v, 0x190), a = p ? U32(p, 0x94) : 0;
    if (!a || U32(a, 0xff078) == 0 || I32(a, 0xff074) <= 0) return 0;
    rx_addr d = 0;
    for (int32_t i = 0; i < I32(a, 0x26c); i++) { const rx_addr x = U32(U32(a, 0x268), 4u * (uint32_t)i); if (x && U32(x, 0x74) == p) d = x; }
    if (!d || U32(d, 0) != 0x0093ffe4u) return 0;   /* a WaypointDriver */
    int32_t index = I32(d, 0x10d4);
    const int32_t last = I32(d, 0x10d8) - 1;
    rx_addr gate = U32(U32(a, 0xff078), 0);
    if (index >= 0 && index < last) { index++; if (index < I32(a, 0xff074)) gate = U32(U32(a, 0xff078), 4u * (uint32_t)index); }
    if (!gate) return 0;
    ACT_FRAME(F, 0x50, 0);
    uint32_t arg = F;
    act_vcall(gate, 0x3c, 1, &arg);
    out[0] = F32(F, 0x30); out[1] = F32(F, 0x34); out[2] = F32(F, 0x38);
    ACT_END(F);
    return 1;
}

/* TEST ONLY: a target that goes around the course's obstacles: an AI rider (the game's own BehaviorRoamToWaypointGate drives it) on the
 * same leg (its last passed gate D+0x10d4 equal to ours) and at least 25 ft away, the nearest; else the next gate itself. */
static int waypoint_target(struct rx_session* s, const float me[3], float out[3]) {
    if (!waypoint_next_gate(s, out)) return 0;
    rx_slot sl[16];
    const int n = rx_session_slots(s, sl, 16);
    rx_addr v = 0;
    for (int i = 0; i < n; i++) if (sl[i].local) v = sl[i].vehicle;
    const rx_addr p = v ? U32(v, 0x190) : 0, a = p ? U32(p, 0x94) : 0;
    if (!a) return 1;
    int32_t mine = -2;
    for (int32_t i = 0; i < I32(a, 0x26c); i++) { const rx_addr d = U32(U32(a, 0x268), 4u * (uint32_t)i); if (d && U32(d, 0x74) == p) mine = I32(d, 0x10d4); }
    float best = 1e30f;
    for (int i = 0; i < n; i++) {
        if (sl[i].local || sl[i].type != 1) continue;
        const rx_addr ap = U32(sl[i].vehicle, 0x190);
        int32_t theirs = -3;
        for (int32_t k = 0; k < I32(a, 0x26c); k++) { const rx_addr d = U32(U32(a, 0x268), 4u * (uint32_t)k); if (d && U32(d, 0x74) == ap) theirs = I32(d, 0x10d4); }
        if (theirs != mine) continue;
        rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
        if (rx_session_vehicle_info(s, sl[i].slot, &vi) != 0) continue;
        const float dx = vi.render_position[0] - me[0], dz = vi.render_position[2] - me[2], dd = dx * dx + dz * dz;
        /* only a rider nearer the gate than we are (it shows the way, it does not lead back) */
        const float gx = out[0] - me[0], gz = out[2] - me[2], ox = out[0] - vi.render_position[0], oz = out[2] - vi.render_position[2];
        if (dd < 25.0f * 25.0f || ox * ox + oz * oz >= gx * gx + gz * gz) continue;
        if (dd < best) { best = dd; out[0] = vi.render_position[0]; out[1] = vi.render_position[1]; out[2] = vi.render_position[2]; }
    }
    return 1;
}

static void autopilot_input(struct rx_session* s, struct autopilot* pilot, int racing) {
    uint8_t keys[256];
    rx_pad pad;
    memset(keys, 0, sizeof keys);
    memset(&pad, 0, sizeof pad);
    rx_bike_state bike;
    float line[64 * 3];
    int points = racing && rx_session_bike(s, &bike) == 0 ? rx_session_race_line(s, line, 64) : 0;
    g_gate_pilot = 0;
    if (racing && points < 2 && !g_tricks) {   /* a waypoint race: steer at the next gate (a two-point "line": here, the gate) */
        float g[3];
        const float me[3] = {bike.position[0][0], bike.position[0][1], bike.position[0][2]};
        if (rx_session_bike(s, &bike) == 0 && waypoint_target(s, me, g)) {
            line[0] = bike.position[0][0]; line[1] = bike.position[0][1]; line[2] = bike.position[0][2];
            line[3] = g[0]; line[4] = g[1]; line[5] = g[2];
            points = 2;
            g_gate_pilot = 1;
        }
    }
    if (points >= 2) {
        const float x = bike.position[0][0], z = bike.position[0][2];
        int nearest = 0;
        float best = 1e30f;
        for (int i = 0; i < points && i < 6; i++) {
            const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z, d = dx * dx + dz * dz;
            if (d < best) { best = d; nearest = i; }
        }
        const float look = 18.0f + bike.speed * 0.6f;
        int target = nearest;
        for (int i = nearest; i < points; i++) {
            const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z;
            target = i;
            if (dx * dx + dz * dz >= look * look) break;
        }
        const float tx = line[3 * target] - x, tz = line[3 * target + 2] - z;
        const float* q = bike.orientation[0];
        const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
        const float error = wrap_angle(atan2f(tx, tz) - atan2f(fx, fz));
        float steer = error * 2.2f;
        if (steer > 1.0f) steer = 1.0f;
        if (steer < -1.0f) steer = -1.0f;
        const float turn = fabsf(error);
        static float scale = 0.0f;   /* CW_PILOT_SCALE: the target speeds scaled (default 1), as career_series_test's CST_PILOT_SCALE */
        if (scale == 0.0f) scale = getenv("CW_PILOT_SCALE") ? (float)atof(getenv("CW_PILOT_SCALE")) : 1.0f;
        const float desired = scale * (turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f);
        if (bike.speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f;
        else pilot->stuck_clock = 0.0f;
        if (g_gate_pilot) {   /* off the race line the speed can read above walking pace while the bike goes nowhere: by distance */
            pilot->anchor_clock += 1.0f / 60.0f;
            if (pilot->anchor_clock >= 4.0f) {
                const float dx = x - pilot->anchor[0], dz = z - pilot->anchor[2];
                if (dx * dx + dz * dz < 16.0f) pilot->stuck_clock = 5.0f;
                pilot->anchor[0] = x; pilot->anchor[2] = z; pilot->anchor_clock = 0.0f;
            }
        }
        /* stuck: back off (longer off the race line: a waypoint course has trees and rocks), then a detour to one side, the side
         * alternating from one time to the next */
        if (pilot->stuck_clock > 4.0f) {
            pilot->reverse_clock = g_gate_pilot ? 2.5f : 1.5f; pilot->stuck_clock = 0.0f;
            if (g_gate_pilot) { pilot->detour_clock = 2.0f; pilot->detours++; if ((pilot->detours % 3) == 0) pilot->reset_frames = 20; }
        }
        if (pilot->reverse_clock > 0.0f) {
            pilot->reverse_clock -= 1.0f / 60.0f;
            pad.left_trigger = 255;
            pad.lx = (int16_t)(-steer * 32000.0f);
        } else if (pilot->detour_clock > 0.0f) {
            pilot->detour_clock -= 1.0f / 60.0f;
            pad.right_trigger = 200;
            pad.lx = (int16_t)((pilot->detours & 1) ? 32000 : -32000);
        } else {
            pad.lx = (int16_t)(steer * 32000.0f);
            if (bike.speed < desired) pad.right_trigger = 255;
            else if (bike.speed > desired + 6.0f) pad.left_trigger = 160;
            else pad.right_trigger = 120;
        }
    }
    if (racing && points < 2 && g_tricks) {   /* career_series_test's CST_TRICKS: full throttle, a slow weave, the right stick circling */
        static unsigned drive, tick;
        pad.right_trigger = 255;
        pad.lx = (int16_t)(sinf((float)(drive++ % 600) / 600.0f * 6.2831853f) * 9000.0f);
        const float turn = (float)(tick++ % 45) / 45.0f * 6.2831853f;
        pad.rx = (int16_t)(sinf(turn) * 30000.0f);
        pad.ry = (int16_t)(cosf(turn) * 30000.0f);
    }
    if (pilot->reset_frames > 0) {   /* every third time stuck: the game's own Reset key (the default remap's column 12), as a player would */
        static rx_remap_table remap; static int have;
        if (!have) { have = rx_remap_defaults(&remap) == 0 ? 1 : -1; }
        if (have == 1) keys[remap.keyboard[12] & 255] = 0x80;
        pilot->reset_frames--;
    }
    rx_session_set_input(s, keys, &pad, 1);
}

static const char* state_name(int st) {
    static const char* names[] = {"Pre", "Countdown", "InProgress", "WindDown", "Post", "Inactive"};
    return st >= 0 && st <= 5 ? names[st] : "?";
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: champ_weekend_test <install root> [nat|sx] [location] [max frames per stage] [AI difficulty]\n"); return 2; }
    const int sx = argc > 2 && strcmp(argv[2], "sx") == 0, wp = argc > 2 && strcmp(argv[2], "wp") == 0, fs = argc > 2 && strcmp(argv[2], "fs") == 0;
    g_tricks = fs;
    const char* location = argc > 3 ? argv[3] : sx ? "SX_Springfield" : wp ? "FR_Gibson" : fs ? "FMX_BuenaVista" : "NAT_Concord";
    const int max_frames = argc > 4 ? atoi(argv[4]) : 40000;
    const int ai = argc > 5 ? atoi(argv[5]) : 95;
    signal(SIGABRT, on_signal); signal(SIGSEGV, on_signal); signal(SIGBUS, on_signal); signal(SIGALRM, on_signal);
    alarm(3600);
    setvbuf(stdout, NULL, _IONBF, 0);

    rx_champ_weekend w;
    if (rx_champ_weekend_begin(&w, sx ? RX_CHAMP_SUPERCROSS : wp ? RX_CHAMP_WAYPOINT : fs ? RX_CHAMP_FREESTYLE : RX_CHAMP_NATIONAL) != 0) { fprintf(stderr, "champ_weekend_test: begin failed\n"); return 1; }
    {   /* the stages' timing: CW_TIMED=<m>+<n>, else laps only (3) */
        const char* tm = getenv("CW_TIMED");
        int m = 0, n = 3;
        if (tm && sscanf(tm, "%d+%d", &m, &n) < 1) { fprintf(stderr, "champ_weekend_test: CW_TIMED=<minutes>+<laps>\n"); return 2; }
        for (int k = 0; k < w.stage_count; k++)
            (void)rx_champ_weekend_set_timing(&w, k, m, m > 0 && strcmp(w.stages[k].label, "PRACTICE") == 0 ? 0 : (m > 0 ? n : 3));   /* (Waypoint / Freestyle: not timeable, kept) */
    }
    /* FIELD SIZE: the heat's split (rx_champ_weekend_split) and its field, 0 before the heat. Locals, not statics: each new session restores
     * the image's __DATA as the first session found it (src/rx_reset.c), and linked statically that image holds this driver's statics too */
    int g_split[3] = {0, 0, 0}, g_split_field = 0;
    printf("champ_weekend_test: %s weekend at %s, AI %d, %d stages\n", sx ? "Supercross" : wp ? "Waypoint" : fs ? "Freestyle" : "National", location, ai, w.stage_count);
    while (!w.finished) {
        const int stage = w.stage;
        rx_champ_stage* st = &w.stages[stage];
        g_step = "session create";
        rx_session* s = rx_session_create(argv[1]);
        if (!s) { fprintf(stderr, "champ_weekend_test: rx_session_create failed: %s\n", rx_session_last_error(NULL)); return 1; }
        rx_event_spec spec;
        rx_session_event_default(&spec);
        snprintf(spec.location, sizeof spec.location, "%s", location);
        spec.ai_difficulty = ai;
        if (getenv("CW_RIDERS")) spec.riders = atoi(getenv("CW_RIDERS"));   /* FIELD SIZE (2 .. 32) */
        if (sx) {   /* the Supercross exhibition event (EventType "Supercross": EventTypeId 8, SX_Springfield 78 by default) */
            snprintf(spec.event_type, sizeof spec.event_type, "Supercross");
            spec.event_type_id = getenv("CW_TYPE_ID") ? atoi(getenv("CW_TYPE_ID")) : 8;   /* EventType "Supercross" */
            spec.location_id = getenv("CW_LOCATION_ID") ? atoi(getenv("CW_LOCATION_ID")) : 78;   /* SX_Springfield (EventLocation) */
        }
        if (fs) {   /* the Freestyle exhibition event (EventTypeId 7, FMX_BuenaVista 39, a session of CW_MINUTES (2) minutes, the career FMX series' length) */
            snprintf(spec.event_type, sizeof spec.event_type, "Freestyle");
            spec.event_type_id = 7;
            spec.location_id = getenv("CW_LOCATION_ID") ? atoi(getenv("CW_LOCATION_ID")) : 39;
            spec.duration = getenv("CW_MINUTES") ? atoi(getenv("CW_MINUTES")) : 2;
            snprintf(spec.duration_type, sizeof spec.duration_type, "Minutes");
        }
        if (wp) {   /* the Waypoint exhibition event, as champ_native_test's (EventTypeId 14, FR_Gibson 47, 1 round) */
            snprintf(spec.event_type, sizeof spec.event_type, "WaypointRace");
            spec.event_type_id = 14;
            spec.location_id = getenv("CW_LOCATION_ID") ? atoi(getenv("CW_LOCATION_ID")) : 47;
            spec.duration = 1;
            spec.track_id = getenv("CW_TRACK") ? atoi(getenv("CW_TRACK")) : 0;
        }
        uint64_t replay = 0;
        if (rx_session_replay_begin_event(s, &replay) != 0) { fprintf(stderr, "champ_weekend_test: replay begin_event failed\n"); return 1; }
        g_step = "event load";
        if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "champ_weekend_test: event load: %s\n", rx_session_last_error(s)); return 1; }
        g_step = "stage apply";
        if (getenv("CW_NO_APPLY") && atoi(getenv("CW_NO_APPLY"))) {   /* TEST ONLY: the same event as a plain single race (the collider probe's reference) */
            printf("STAGE %d: NOT APPLIED (a single race of the event)\n", stage);
        } else if (rx_session_champ_stage_apply(s, &w) != 0) { fprintf(stderr, "champ_weekend_test: stage apply: %s\n", rx_session_last_error(s)); return 1; }
        printf("STAGE %d %s: activity %s type 0x%x laps %d minutes %d\n", stage, st->label, st->activity, (unsigned)st->activity_type, st->laps, w.stage_minutes[stage]);
        if (getenv("CW_PROBE_ROWS")) {   /* TEST ONLY: the Player List's Active / StartZone per row after the apply (the grid's input) */
            const uint32_t rows = rx_tbl_row_count(ACT_PLAYER_LIST);
            printf("  ROWS   %s after the apply: %u rows:", st->label, rows);
            for (uint32_t r = 0; r < rows; r++) printf(" %u:%u/%d", r, rx_grid_active_cell(r), (int)act_cell(ACT_PLAYER_LIST, 0x0093df00, r));
            printf("\n");
        }
        int clock_expired = 0, clock_flag = 0;
        g_step = "stage frames";
        struct autopilot pilot; memset(&pilot, 0, sizeof pilot);
        int last_state = -1, post_left = -1, frame = 0, local_finish = -1;
        int intro_frames = 0, intro_first = -1; unsigned intro_steps = 0;   /* the race intro scenario (rx_session_podium scenario 3) */
        rx_race_state race;
        for (; frame < max_frames; frame++) {
            memset(&race, 0, sizeof race);
            const int have = rx_session_race(s, &race) == 0 && race.valid;
            autopilot_input(s, &pilot, have && race.state == 2);
            if (rx_session_advance(s) != 0) { fprintf(stderr, "champ_weekend_test: stage %d advance %d: %s\n", stage, frame + 1, rx_session_last_error(s)); return 1; }
            {   /* the replay presenter's renderer: every request of the frame applied, then painted */
                rx_replay_view v; memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_REPLAY_API_VERSION;
                if (rx_session_replay_view(s, replay, &v) != 0) { fprintf(stderr, "champ_weekend_test: replay view failed\n"); return 1; }
                for (;;) {
                    rx_replay_request r; memset(&r, 0, sizeof r); r.size = sizeof r; r.version = RX_REPLAY_API_VERSION;
                    if (rx_session_replay_next(s, replay, &r) != 0) break;
                    if (rx_session_replay_ack(s, replay, &r, RX_REPLAY_APPLIED) != 0 || rx_session_replay_ack(s, replay, &r, RX_REPLAY_PAINTED) != 0) {
                        fprintf(stderr, "champ_weekend_test: replay ack failed\n"); return 1;
                    }
                }
            }
            {
                rx_podium_info pi; rx_podium_rider pr[16]; rx_podium_slot ps[16];
                static float pm[16 * 69 * 16];
                memset(&pi, 0, sizeof pi);
                if (rx_session_podium(s, &pi, pr, 16, pm, 16 * 69, ps, 16) == 0 && pi.scenario == 3) {
                    intro_frames++; if (intro_first < 0) intro_first = frame + 1;
                    if (pi.step >= 0 && pi.step < 32) intro_steps |= 1u << pi.step;
                }
            }
            memset(&race, 0, sizeof race);
            if (rx_session_race(s, &race) != 0 || !race.valid) continue;
            if (race.state != last_state) {
                printf("STAGE %d frame %6d activity %-10s laps %d/%d place %d time %.2f\n", stage, frame + 1, state_name(race.state), race.laps_completed,
                       race.num_laps, race.place, race.race_time);
                if (race.state == 1 && getenv("CW_PROBE_ROWS")) {   /* TEST ONLY: the same at the countdown (after the grid) */
                    const uint32_t rows = rx_tbl_row_count(ACT_PLAYER_LIST);
                    printf("  ROWS   %s at the countdown: %u rows:", st->label, rows);
                    for (uint32_t r = 0; r < rows; r++) printf(" %u:%u/%d", r, rx_grid_active_cell(r), (int)act_cell(ACT_PLAYER_LIST, 0x0093df00, r));
                    printf("\n");
                }
                if (race.state == 2 && getenv("CW_RIDERS")) {   /* FIELD SIZE: the start grid as ridden: the stage's riders' positions at GO (none stacked) */
                    rx_driver_result dr[64]; const int nd = rx_session_standings(s, dr, 64);
                    float pos[64][3]; int ok = 0, close = 0, ca = -1, cb = -1; float best = 1e30f;
                    for (int q = 0; q < nd; q++) {
                        rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
                        if (rx_session_vehicle_info(s, dr[q].slot, &vi) != 0) { pos[q][0] = 1e9f; continue; }
                        pos[q][0] = vi.render_position[0]; pos[q][1] = vi.render_position[1]; pos[q][2] = vi.render_position[2]; ok++;
                    }
                    for (int q = 0; q < nd; q++) for (int o = q + 1; o < nd; o++) {
                        const float dx = pos[q][0] - pos[o][0], dz = pos[q][2] - pos[o][2], d = sqrtf(dx * dx + dz * dz);
                        if (d < best) { best = d; ca = dr[q].slot; cb = dr[o].slot; }
                        if (d < 2.5f) close++;
                    }
                    printf("  START  %s: %d riders at GO (%d placed), closest two %.1f ft apart (slots %d, %d), %d pair(s) under 2.5 ft%s\n", st->label, nd, ok,
                           (double)best, ca, cb, close, close ? "  STACKED" : "");
                }
                last_state = race.state;
            }
            if (getenv("CW_DEBUG") && frame % 600 == 0) {
                rx_bike_state b; float line[64 * 3];
                const int okb = rx_session_bike(s, &b) == 0;
                const int pts = rx_session_race_line(s, line, 64);
                rx_driver_result dr[16]; const int nd = rx_session_standings(s, dr, 16);
                printf("DEBUG frame %d bike %d speed %.2f pos %.1f %.1f %.1f line %d drivers %d", frame + 1, okb, okb ? (double)b.speed : 0.0,
                       okb ? (double)b.position[0][0] : 0.0, okb ? (double)b.position[0][1] : 0.0, okb ? (double)b.position[0][2] : 0.0, pts, nd);
                for (int q = 0; q < nd; q++) printf(" [%d p%d l%d]", dr[q].slot, dr[q].place, dr[q].laps_completed);
                float gpos[3] = {0, 0, 0};
                const int hasg = waypoint_next_gate(s, gpos);
                rx_waypoint_state ws; memset(&ws, 0, sizeof ws); ws.size = sizeof ws; ws.version = RX_WAYPOINT_API_VERSION;
                (void)rx_session_waypoint(s, &ws);
                printf(" gate %d/%d next %d (%.0f %.0f %.0f)\n", ws.gate_index, ws.gate_count, hasg, (double)gpos[0], (double)gpos[1], (double)gpos[2]);
            }
            {   /* TIMED RACES: the clock's expiry and the flag */
                rx_champ_clock ck; memset(&ck, 0, sizeof ck); ck.size = sizeof ck;
                if (rx_session_champ_clock(s, &ck) == 0 && ck.timed) {
                    if (ck.expired && !clock_expired) {
                        clock_expired = 1;
                        rx_driver_result dr[64]; const int nd = rx_session_standings(s, dr, 64);
                        int lead = -1; for (int q = 0; q < nd; q++) if (dr[q].place == 1) lead = q;
                        printf("STAGE %d frame %6d CLOCK %.0f s out at %.2f: lap total %d (the leader's laps %d + 1 + %d)\n", stage, frame + 1, (double)ck.seconds,
                               (double)ck.expired_at, ck.num_laps, lead >= 0 ? dr[lead].laps_completed : -1, ck.extra_laps);
                    }
                    if (ck.flag && !clock_flag) { clock_flag = 1; printf("STAGE %d frame %6d FLAG at %.2f: the leader took the checkered flag (lap total %d)\n", stage, frame + 1, (double)ck.flag_at, ck.num_laps); }
                }
            }
            if (frame + 1 == 60) {   /* the collider probe: the activity area's subscene and the physics world's object vectors */
                rx_slot sl[16]; const int ns = rx_session_slots(s, sl, 16); rx_addr v = 0;
                for (int q = 0; q < ns; q++) if (sl[q].local) v = sl[q].vehicle;
                const rx_addr p = v ? U32(v, 0x190) : 0, a = p ? U32(p, 0x94) : 0, wld = a ? U32(a, 0x234) : 0, pw = wld ? U32(wld, 0x20134) : 0;
                if (pw) {
                    const uint32_t dyn = (U32(pw, 0x1954 + 0x10) - U32(pw, 0x1954 + 0xc)) / 0xe0, keyed = (U32(pw, 0x1994 + 0x10) - U32(pw, 0x1994 + 0xc)) / 0xe0,
                                   stat = (U32(pw, 0x19b4 + 0x10) - U32(pw, 0x19b4 + 0xc)) / 0x40;
                    printf("STAGE %d COLLIDERS: activity type 0x%x area subscene %s; physics world dynamic %u keyed %u static %u\n", stage, U32(a, 0x23c),
                           U32(a, 0xec8) ? "loaded" : "NOT LOADED", dyn, keyed, stat);
                }
            }
            if (local_finish < 0 && race.state == 2 && race.num_laps > 0 && race.laps_completed >= race.num_laps) {
                local_finish = frame + 1;
                printf("STAGE %d frame %6d the local rider finished: place %d time %.2f\n", stage, frame + 1, race.place, race.race_time);
            }
            /* the record waits for Post (state 4): a Freestyle's AI scores are set there (MXActivityFreestyle vf41 -> 005639a0) */
            if (post_left < 0 && race.state == 4) post_left = STAGE_POST_FRAMES;
            if (post_left > 0 && --post_left == 0 && rx_session_get_state(s) == RX_SESSION_FINISHED) break;
        }
        if (frame >= max_frames && getenv("CW_SMOKE") && atoi(getenv("CW_SMOKE"))) {
            /* TEST ONLY (CW_SMOKE=1): a stage the autopilot cannot finish (Waypoint: no race line in the API) is run for the frame budget
             * only, its drivers' standings printed, and the next stage taken without a record */
            rx_driver_result dr[16]; const int nd = rx_session_standings(s, dr, 16);
            printf("  SMOKE %s: %d frames, no fault; activity %s; standings", st->label, frame, state_name(last_state));
            for (int q = 0; q < nd; q++) printf(" [slot %d place %d laps %d]", dr[q].slot, dr[q].place, dr[q].laps_completed);
            printf("\n  INTRO SCENARIO %s: %d frames from frame %d\n", st->label, intro_frames, intro_first);
            rx_session_destroy(s);
            if (++w.stage >= w.stage_count) w.finished = 1;
            continue;
        }
        if (frame >= max_frames) {
            printf("  INTRO SCENARIO %s: %d frames from frame %d\n", st->label, intro_frames, intro_first);
            fprintf(stderr, "champ_weekend_test: stage %d did not end in %d frames (activity %s, laps %d/%d, lap progress %.3f, place %d)\n", stage, max_frames,
                    state_name(last_state), race.laps_completed, race.num_laps, (double)race.lap_progress, race.place);
            return 1;
        }
        printf("  INTRO SCENARIO %s: %d frames from frame %d, steps", st->label, intro_frames, intro_first);
        for (int k = 0; k < 32; k++) if (intro_steps & (1u << k)) printf(" %d", k);
        printf("\n");
        if (w.stage_minutes[stage] > 0) {   /* TIMED RACES: every driver finished, the places follow the laps */
            rx_driver_result dr[64]; const int nd = rx_session_standings(s, dr, 64);
            int unfinished = 0, order_ok = 1, prev = 1 << 30;
            printf("  TIMED %s: clock %s, flag %s; drivers", st->label, clock_expired ? "out" : "NOT OUT", clock_flag ? "out" : "NOT OUT");
            for (int pl = 1; pl <= nd; pl++) for (int q = 0; q < nd; q++) if (dr[q].place == pl) {
                printf(" [p%d slot %d laps %d %s %.2f]", pl, dr[q].slot, dr[q].laps_completed, dr[q].finished ? "fin" : "RACING", (double)dr[q].race_time);
                if (!dr[q].finished) unfinished++;
            }
            printf("\n");
            g_step = "stage record";
            if (rx_session_champ_stage_record(s, &w) != 0) { fprintf(stderr, "champ_weekend_test: stage record: %s\n", rx_session_last_error(s)); return 1; }
            for (int pl = 1; pl <= RX_CHAMP_MAX_RIDERS; pl++) for (int i = 0; i < w.rider_count; i++) if (w.riders[i].place[stage] == pl) {
                if (w.rider_laps[i][stage] > prev) order_ok = 0;
                prev = w.rider_laps[i][stage];
            }
            printf("  TIMED CHECK %s: %d of %d drivers not finished, places by laps %s\n", st->label, unfinished, nd, order_ok ? "ok" : "WRONG");
            if (!clock_expired || !clock_flag || !order_ok || unfinished) { fprintf(stderr, "champ_weekend_test: timed stage check failed\n"); return 1; }
        } else {
        g_step = "stage record";
        if (rx_session_champ_stage_record(s, &w) != 0) { fprintf(stderr, "champ_weekend_test: stage record: %s\n", rx_session_last_error(s)); return 1; }
        }
        printf("STAGE %d %s ended after %d frames (game frames %llu)\n", stage, st->label, frame + 1, (unsigned long long)rx_session_frames(s));
        printf("  GRID   %s:", st->label);   /* the start slots the riders took (P+0x1dc), front first */
        for (int g = 1; g <= 32; g++)
            for (int i = 0; i < w.rider_count; i++) if (w.riders[i].gate[stage] == g) printf(" %d=%s%s", g, w.riders[i].name, w.riders[i].local ? "*" : "");
        printf("\n  RESULT %s:", st->label);
        for (int pl = 1; pl <= 32; pl++)
            for (int i = 0; i < w.rider_count; i++)
                if (w.riders[i].place[stage] == pl) {
                    printf(" %d=%s%s(%.2f)", pl, w.riders[i].name, w.riders[i].local ? "*" : "", (double)(w.judged ? w.riders[i].score[stage] : w.riders[i].best_lap[stage]));
                    if (w.stage_minutes[stage] > 0) printf("[%dL]", w.rider_laps[i][stage]);
                }
        printf("\n");
        if (getenv("CW_RIDERS")) {   /* FIELD SIZE: the stage's field; a Supercross heat's split (rx_champ_weekend_split), checked against the LCQ / MAIN fields */
            int field = 0;
            for (int i = 0; i < w.rider_count; i++) if (w.riders[i].place[stage] > 0) field++;
            printf("  FIELD  %s: %d riders (event riders %d, weekend riders %d)\n", st->label, field, (int)spec.riders, (int)w.rider_count);
            if (sx && strcmp(st->label, "HEAT") == 0 && rx_champ_weekend_split(field, &g_split[0], &g_split[1], &g_split[2]) == 0) {
                g_split_field = field;
                printf("  SPLIT  HEAT of %d: 1-%d to the MAIN, %d-%d to the LCQ (%d riders, its top %d to the MAIN: a MAIN of %d), %d out\n", field, g_split[0],
                       g_split[0] + 1, g_split[1], g_split[1] - g_split[0], g_split[2], g_split[0] + g_split[2], field - g_split[1]);
            }
            if (sx && g_split_field > 0 && (strcmp(st->label, "LCQ") == 0 || strcmp(st->label, "MAIN") == 0)) {
                const int want = strcmp(st->label, "LCQ") == 0 ? g_split[1] - g_split[0] : g_split[0] + g_split[2];
                printf("  SPLIT CHECK %s: %d riders, the split says %d: %s\n", st->label, field, want, field == want ? "ok" : "WRONG");
                if (field != want) { fprintf(stderr, "champ_weekend_test: the %s field does not follow the split\n", st->label); return 1; }
            }
        }
        for (int k = stage + 1; k < w.stage && k < w.stage_count; k++) if (w.stages[k].decided) {
            printf("  DECIDED %s (not raced: the local rider is not in it):", w.stages[k].label);
            int decided = 0;
            for (int pl = 1; pl <= 32; pl++) for (int i = 0; i < w.rider_count; i++) if (w.riders[i].place[k] == pl) { printf(" %d=%s", pl, w.riders[i].name); decided++; }
            printf("\n");
            if (getenv("CW_RIDERS") && g_split_field > 0 && w.stages[k].field == RX_CHAMP_FIELD_LCQ) {
                printf("  SPLIT CHECK %s (decided): %d riders, the split says %d: %s\n", w.stages[k].label, decided, g_split[1] - g_split[0], decided == g_split[1] - g_split[0] ? "ok" : "WRONG");
                if (decided != g_split[1] - g_split[0]) { fprintf(stderr, "champ_weekend_test: the decided LCQ does not follow the split\n"); return 1; }
            }
        }
        if (w.eliminated) printf("  ELIMINATED: the local rider is not in the main event\n");
        /* TEST ONLY (CW_TEST_PROMOTE=1, like career_cli's WIN=1): the autopilot cannot qualify on its own, so after a raced LCQ that
         * eliminated it, the local rider's LCQ place is swapped with the LCQ winner's in the weekend (host memory) and the main event
         * is run, to drive the MAIN stage and its grid. Never in the product. */
        if (w.eliminated && getenv("CW_TEST_PROMOTE") && atoi(getenv("CW_TEST_PROMOTE"))) {
            int lcq = -1, main_stage = -1, local = -1, winner = -1;
            for (int k = 0; k < w.stage_count; k++) { if (w.stages[k].field == RX_CHAMP_FIELD_LCQ) lcq = k; if (w.stages[k].field == RX_CHAMP_FIELD_MAIN) main_stage = k; }
            for (int i = 0; i < w.rider_count; i++) { if (w.riders[i].local) local = i; if (lcq >= 0 && w.riders[i].place[lcq] == 1) winner = i; }
            if (lcq >= 0 && main_stage >= 0 && local >= 0 && winner >= 0 && (w.stages[lcq].raced || w.stages[lcq].decided)) {   /* (decided: the local rider was out after the heat; its LCQ place 0 goes to the winner) */
                const int t = w.riders[local].place[lcq];
                w.riders[local].place[lcq] = 1; w.riders[winner].place[lcq] = t;
                w.eliminated = 0; w.finished = 0; w.stage = main_stage;
                printf("  TEST PROMOTE: %s takes LCQ place 1 (was %d), %s place %d; the MAIN is run\n", w.riders[local].name, t, w.riders[winner].name, t);
            }
        }
        g_step = "session destroy";
        rx_session_destroy(s);
    }
    printf("WEEKEND RESULT (overall):\n");
    for (int i = 0; i < w.rider_count; i++) {
        const rx_champ_rider* r = &w.riders[w.overall[i]];
        if (w.judged) printf("  %2d. %-14s%s total score %8.2f  places", i + 1, r->name, r->local ? " (you)" : "      ", (double)r->total_score);
        else printf("  %2d. %-14s%s points %3d  places", i + 1, r->name, r->local ? " (you)" : "      ", r->points);
        for (int k = 0; k < w.stage_count; k++) printf(" %s=%d(gate %d)", w.stages[k].label, r->place[k], r->gate[k]);
        printf("\n");
    }
    return 0;
}
