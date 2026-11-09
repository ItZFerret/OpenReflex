/* career_series_test.c -- runs a MOTOCAREER series through the public career API of the product library, no emulator and no Unity.
 *
 *   career_series_test <install root> <player24 export> list
 *       prints the admission of every Series row (rx_session_career_series): owner, format, activity, class, supported / locked /
 *       eligible for the imported profile's equipped vehicle.
 *   career_series_test <install root> <player24 export> <series id> [max frames (60000)]
 *       plays the series the way the open front end's presenter does: configure, the replay presenter, load, the garage vehicle,
 *       initialize, the Moto / Heat practice choice (skip; CST_PRACTICE=1: ride the practice qualifier), start; then frames with a
 *       race-line autopilot on pad 0 while racing, and every career presentation request answered as a presenter that paints it
 *       would answer: rules popup (shown, accept, the RulesPopup wipe In / HideRaceRulesPopup / Out), fades, the SeriesEvent wipe,
 *       the hub (results page, Moto pages: gates and the inter-moto continue), Continue when the results allow it, CompleteEvent,
 *       the front-end route, CompleteSeries and the two durable checkpoints (the player export written to <export>.out-<n>.bin
 *       stands in for the host's save).
 *
 *   build: core/build.sh, then
 *     clang -std=c11 -O1 -Iinclude -Isrc -Igame -Iapi -o <out> tools/career_series_test.c build/obj/<all objects> -lz -lc++
 *   REFLEX_TEST_LAPS=<n> shortens the races (the original's own debug lap override, applied by rx_session_create).
 *
 *   The race HUD query (rx_session_race_hud, w-hud) is sampled every frame and its changes are printed ("HUD ..."); with CST_HUD_CHECK=1
 *   (and CST_TRICKS=1: the right stick circles, so Freestyle scores tricks) the series must also have produced what its HUD shows (5: the Player Eliminated message, 56 / 88: the gate counter, 52: the section
 *   leaders / split times, 22: the judge bars), else exit 1.
 *
 * CST_RESTART=<n>[,<n>...] (the pause menu's RESTART, rx_session_restart_activity = App.RestartActivity): when the local rider has raced <n>
 * frames, the activity restarts (one entry per restart, each counted from the last start of the race); the race must go back to its
 * start (state < 2), run again and the series must still complete with the same durable checkpoint pair (the restarts record nothing).
 *
 * CST_PAGE_TRACE=1 (Moto / Heat): the client hub page's release frames against the race state after every gate handoff (PAGE / PAGE_SUMMARY
 * lines; see page_trace_frame).
 *
 * CST_POST_SKIP=<n> (w-uibatch: the Event Outro page's Accept): <n> frames into every post-race finish scenario (Post sub-state 1) the
 * client's skip (rx_session_skip_scenario = App.SkipToNextScenario) is sent; POST lines give how long each Post 1 lasted and whether the
 * skip ended it (the next frame) or was ignored. Without it the POST lines give the scenarios' own length.
 *
 * Test only. It never writes game memory: every action is a public API call. Exit 0 = the series completed with a durable
 * checkpoint pair; 1 = an API call failed or the series did not complete in the frame budget; 128 + signal = the core aborted
 * (an unported path names its address on stderr). */
#include "rx_api.h"
#include "rx_ui_settings.h"
#include "rx_career_api.h"
#include "rx_career_transitions.h"
#include "rx_career_checkpoint.h"
#include "rx_career_completion.h"
#include "rx_career_vehicle.h"
#include "rx_career_supercross.h"
#include "rx_career_powercross.h"
#include "rx_replay_api.h"
#include "rx_career_moto.h"
#include "rx_career_series.h"
#include "rx_career_heat.h"
#include "rx_race_hud.h"
#include "rx_stunt_hud.h"
#include "rx_scene_pages_api.h"
#include "../game/rx_challenge_api.h"

#include <execinfo.h>
#include <math.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static const char* g_step = "start";
static char g_step_text[200];

static void step(const char* format, const char* detail) {
    snprintf(g_step_text, sizeof g_step_text, format, detail);
    g_step = g_step_text;
    printf("career_series_test: %s\n", g_step_text);
}

static void on_signal(int sig) {
    char text[300];
    const int length = snprintf(text, sizeof text, "career_series_test: signal %d during: %s\n", sig, g_step);
    if (length > 0) (void)write(2, text, (size_t)length);
    void* frames[40];
    const int count = backtrace(frames, 40);
    backtrace_symbols_fd(frames, count, 2);
    _exit(128 + sig);
}

static int fail(const char* what, int result, struct rx_session* s) {
    const char* error = s ? rx_session_last_error(s) : "";
    printf("career_series_test: FAIL %s (result %d)%s%s\n", what, result, error && *error ? ": " : "", error ? error : "");
    return 1;
}

static void* read_file(const char* path, uint32_t* size) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    const long length = ftell(f);
    fseek(f, 0, SEEK_SET);
    void* data = malloc((size_t)length);
    if (data && fread(data, 1, (size_t)length, f) != (size_t)length) { free(data); data = NULL; }
    fclose(f);
    *size = (uint32_t)length;
    return data;
}

/* Every presenter capability the API knows: this driver answers every request kind. */
static uint32_t all_capabilities(void) {
#ifdef RX_CAREER_ALL_PRESENTER_CAPABILITIES
    return RX_CAREER_ALL_PRESENTER_CAPABILITIES;
#else
    return RX_CAREER_POWERCROSS_ALL_CAPABILITIES;
#endif
}

/* ---- the race-line autopilot (pad 0: left stick steering, right trigger throttle, left trigger brake) ------------------------ */

static float wrap_angle(float angle) {
    while (angle > 3.14159265f) angle -= 6.2831853f;
    while (angle < -3.14159265f) angle += 6.2831853f;
    return angle;
}

struct autopilot {
    float stuck_clock;      /* seconds below walking speed while racing */
    float reverse_clock;    /* seconds left of a reverse manoeuvre */
};

static void autopilot_input(struct rx_session* s, struct autopilot* pilot, int racing) {
    uint8_t keys[256];
    rx_pad pad;
    memset(keys, 0, sizeof keys);
    memset(&pad, 0, sizeof pad);
    rx_bike_state bike;
    float line[64 * 3];
    const int points = racing && rx_session_bike(s, &bike) == 0 ? rx_session_race_line(s, line, 64) : 0;
    if (points >= 2) {
        const float x = bike.position[0][0];
        const float z = bike.position[0][2];
        /* the target: the first race-line node at least `look` units ahead along the line from the nearest of the first six */
        int nearest = 0;
        float best = 1e30f;
        for (int i = 0; i < points && i < 6; i++) {
            const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z;
            const float d = dx * dx + dz * dz;
            if (d < best) { best = d; nearest = i; }
        }
        const float look = 18.0f + bike.speed * 0.6f;
        int target = nearest;
        for (int i = nearest; i < points; i++) {
            const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z;
            target = i;
            if (dx * dx + dz * dz >= look * look) break;
        }
        const float tx = line[3 * target] - x;
        const float tz = line[3 * target + 2] - z;
        /* the heading: the frame body's forward axis (quaternion x, y, z, w rotating +Z) */
        const float* q = bike.orientation[0];
        const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]);
        const float fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
        const float error = wrap_angle(atan2f(tx, tz) - atan2f(fx, fz));
        float steer = error * 2.2f;
        if (steer > 1.0f) steer = 1.0f;
        if (steer < -1.0f) steer = -1.0f;
        const float speed = bike.speed;
        const float turn = fabsf(error);
        /* CST_PILOT_SCALE: the target speeds scaled (default 1) */
        static float scale = 0.0f;
        if (scale == 0.0f) scale = getenv("CST_PILOT_SCALE") ? (float)atof(getenv("CST_PILOT_SCALE")) : 1.0f;
        const float desired = scale * (turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f);
        if (speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f;
        else pilot->stuck_clock = 0.0f;
        if (pilot->stuck_clock > 4.0f) { pilot->reverse_clock = 1.5f; pilot->stuck_clock = 0.0f; }
        if (pilot->reverse_clock > 0.0f) {
            pilot->reverse_clock -= 1.0f / 60.0f;
            pad.left_trigger = 255;
            pad.lx = (int16_t)(-steer * 32000.0f);
        } else {
            pad.lx = (int16_t)(steer * 32000.0f);
            if (speed < desired) pad.right_trigger = 255;
            else if (speed > desired + 6.0f) pad.left_trigger = 160;
            else pad.right_trigger = 120;
        }
    }
    if (racing && points < 2 && getenv("CST_TRICKS") && atoi(getenv("CST_TRICKS"))) {   /* no race line (Freestyle): ride about */
        static unsigned drive;
        pad.right_trigger = 255;
        pad.lx = (int16_t)(sinf((float)(drive++ % 600) / 600.0f * 6.2831853f) * 9000.0f);
    }
    if (racing && getenv("CST_TRICKS") && atoi(getenv("CST_TRICKS"))) {   /* Freestyle: the right stick circles, the game's stunt recognizer (Ground / AerialStuntInputMappings) reads its directions */
        static unsigned tick;
        const float turn = (float)(tick++ % 45) / 45.0f * 6.2831853f;
        pad.rx = (int16_t)(sinf(turn) * 30000.0f);
        pad.ry = (int16_t)(cosf(turn) * 30000.0f);
    }
    rx_session_set_input(s, keys, &pad, 1);
}

/* ---- the series admission table -------------------------------------------------------------------------------------------- */

static int list_series(struct rx_session* s, uint64_t generation) {
    int admitted = 0;
    printf("id  owner  format        activity        class product supported locked eligible\n");
    for (uint32_t id = 0; id < 100; id++) {
        rx_career_series_info info;
        memset(&info, 0, sizeof info);
        info.size = sizeof info;
        info.version = RX_CAREER_API_VERSION;
        const int rc = rx_session_career_series(s, generation, id, &info);
        if (rc) return fail("series", rc, s);
        if (!info.authored) continue;
        admitted += info.supported != 0;
        printf("%2u  %-5s  %-12s  %-14s  %3d   %5d   %u        %u      %u\n", id, info.owner, info.format, info.activity,
               info.vehicle_class, info.product_id, info.supported, info.locked, info.eligible);
    }
    printf("career_series_test: %d series supported\n", admitted);
    return 0;
}

/* ---- the presenter ----------------------------------------------------------------------------------------------------------- */

struct presenter {
    uint64_t generation;
    uint64_t hub_id;               /* the last accepted hub request (the Moto page actions name it) */
    int results_shown;             /* a hub with results was acknowledged */
    int continue_sent;
    int checkpoints_committed;
    int moto;                      /* the series format is Moto */
    int heat;                      /* the series format is Heat */
    uint64_t loading_hub;          /* the hub whose AAA loading page was entered */
    int heat_rounds;               /* Heat rounds whose results were continued */
    const char* export_path;
};

static int save_checkpoint(struct rx_session* s, struct presenter* p, uint64_t id) {
    uint32_t required = 0;
    rx_session_player_export(s, NULL, 0, &required);
    void* data = malloc(required ? required : 1);
    uint32_t written = 0;
    int rc = rx_session_player_export(s, data, required, &written);
    if (rc) { free(data); return fail("player export (checkpoint)", rc, s); }
    char path[1024];
    snprintf(path, sizeof path, "%s.out-%d.bin", p->export_path, p->checkpoints_committed + 1);
    FILE* f = fopen(path, "wb");
    if (f) { fwrite(data, 1, required, f); fclose(f); }
    free(data);
    rc = rx_session_career_checkpoint_committed(s, p->generation, id);
    if (rc != RX_CAREER_OK && rc != RX_CAREER_REPEAT) return fail("checkpoint committed", rc, s);
    p->checkpoints_committed++;
    printf("career_series_test: CHECKPOINT %d committed (%u bytes, %s)\n", p->checkpoints_committed, required, path);
    return 0;
}

static int page_wipe(struct rx_session* s, struct presenter* p, const rx_career_request* r) {
    int rc = rx_session_career_wipe(s, p->generation, r->id, RX_CAREER_REQUEST_PAGE_WIPE, r->name, 1);
    if (rc) return fail("wipe In", rc, s);
    rx_career_transition_status t;
    memset(&t, 0, sizeof t);
    t.size = sizeof t;
    t.version = RX_CAREER_TRANSITIONS_VERSION;
    rc = rx_session_career_transitions(s, p->generation, &t);
    if (rc) return fail("transitions", rc, s);
    if (!strcmp(r->name, "RulesPopup") && !strcmp(t.required_ui_event, "HideRaceRulesPopup")) {
        rc = rx_session_career_rules_popup_ended(s, p->generation, t.rules_wipe_id);
        if (rc) return fail("rules popup ended", rc, s);
    }
    if (!strcmp(r->name, "AAALoading") && !strcmp(t.required_ui_event, "RoutedGotoHub")) {
        rc = rx_session_career_loading_hub_entered(s, p->generation, t.loading_wipe_id);
        if (rc) return fail("loading hub entered", rc, s);
    }
    rc = rx_session_career_wipe(s, p->generation, r->id, RX_CAREER_REQUEST_PAGE_WIPE, r->name, 2);
    if (rc) return fail("wipe Out", rc, s);
    return 0;
}

static int moto_status(struct rx_session* s, struct presenter* p, rx_career_moto_status* m) {
    memset(m, 0, sizeof *m);
    m->size = sizeof *m;
    m->version = RX_CAREER_MOTO_VERSION;
    return rx_session_career_moto_status(s, p->generation, m);
}

/* The Moto pages (OwnCareerPages.ShowMotoHub): gate selection, the first moto's continue, the next moto. */
static int moto_pages(struct rx_session* s, struct presenter* p, const rx_career_status* status) {
    if (!p->hub_id) return 0;
    rx_career_moto_status m;
    if (moto_status(s, p, &m)) return 0;
    int action = 0, gate = -1;
    if (m.post_complete) return 0;
    if (m.gate_active && m.device == 0 && m.gate_count > 0) { action = RX_MOTO_GATE_SELECT; gate = m.gates[0]; }
    else if (m.phase == 6 && m.next_phase == 7 && m.gate_count == 0 && !m.gate_active) {
        /* the first moto's results page: CONTINUE once the core allows it (it answers BAD_STATE until then) */
        if (!m.first_continue_submitted) { if (status->activity_state == 4 && status->activity_substate == 2) action = RX_MOTO_FIRST_CONTINUE; }
        else if (m.first_post_observed) action = RX_MOTO_NEXT;
    } else if (!m.gate_active && (m.next_phase == 6 || m.next_phase == 7) && status->activity_state != 2) action = RX_MOTO_GATE_BEGIN;
    if (!action) return 0;
    const int rc = rx_session_career_moto_action(s, p->generation, p->hub_id, (uint32_t)action, m.phase, m.sub_event, m.gate_cursor, gate);
    if (rc == RX_CAREER_BUSY) return 0;
    if (rc == RX_CAREER_BAD_STATE && action == RX_MOTO_FIRST_CONTINUE) return 0;   /* not yet allowed: the page waits */
    /* the gate pick: the local rider's turn in the pick order (the event's cursor + 0x34; vf9 orders the riders by SortValue: the
     * practice's best laps, or last after SKIP) and the gates still open (+ 0x3c) */
    printf("career_series_test: MOTO action %d (phase %u next %u gate %d, pick %u, %u gates open) -> %d\n", action, m.phase, m.next_phase, gate,
           m.gate_cursor + 1, m.gate_count, rc);
    if (rc) return fail("moto action", rc, s);
    return 0;
}

static int heat_status(struct rx_session* s, struct presenter* p, rx_career_heat_status* h) {
    memset(h, 0, sizeof *h);
    h->size = sizeof *h;
    h->version = RX_CAREER_HEAT_VERSION;
    return rx_session_career_heat_status(s, p->generation, h);
}

/* The Heat pages (rx_career_heat.h): the AAA loading page between the rounds, the gate selection of every round, the heat / LCQ
 * results' CONTINUE and the next round. */
static int heat_pages(struct rx_session* s, struct presenter* p, const rx_career_status* status) {
    if (!p->hub_id) return 0;
    rx_career_heat_status h;
    if (heat_status(s, p, &h)) return 0;
    if (h.activity_area_required && p->loading_hub != p->hub_id && status->pending_requests == 0) {
        const int rc = rx_session_career_loading_enter(s, p->generation);
        printf("career_series_test: HEAT loading page entered (phase %u next %u) -> %d\n", h.phase, h.next_phase, rc);
        if (rc) return fail("loading enter", rc, s);
        p->loading_hub = p->hub_id;
        return 0;
    }
    int action = 0, gate = -1;
    if (h.post_complete || h.activity_area_required) return 0;
    if (h.gate_active && h.device == 0 && h.gate_count > 0) {
        action = RX_HEAT_GATE_SELECT;
        gate = h.gates[0];
    } else if (!strcmp(h.event_status, "HeatOrLCQ") && (h.phase == 3 || h.phase == 4) && !h.gate_active &&
               !strcmp(h.routing_message, "GotoPostEvent")) {
        if (!h.round_continue_submitted) {
            if (status->activity_state == 4 && status->activity_substate == 2) action = RX_HEAT_ROUND_CONTINUE;
        } else if (h.round_post_observed) {
            action = RX_HEAT_NEXT;
        }
    } else if (!h.gate_active && h.gate_count > 0 && !strcmp(h.routing_message, "Goto GateSelection") &&
               (h.next_phase == 3 || h.next_phase == 4 || h.next_phase == 5) && status->activity_state != 2) {
        action = RX_HEAT_GATE_BEGIN;
    }
    if (!action) return 0;
    if (action == RX_HEAT_ROUND_CONTINUE) {
        printf("career_series_test: HEAT round %u results (%u riders):", h.phase, h.result_count);
        for (uint32_t i = 0; i < h.result_count; i++) printf(" %d:%s%s->%d", h.results[i].rank + 1, h.results[i].name,
                                                            h.results[i].player_type == 0 ? "*" : "", h.results[i].requested_state);
        printf("\n");
    }
    const int rc = rx_session_career_heat_action(s, p->generation, p->hub_id, (uint32_t)action, h.phase, h.next_phase, h.gate_cursor, gate);
    if (rc == RX_CAREER_BUSY) return 0;
    if (rc == RX_CAREER_BAD_STATE && action == RX_HEAT_ROUND_CONTINUE) return 0;   /* not yet allowed: the page waits */
    printf("career_series_test: HEAT action %d (phase %u next %u gate %d, \"%s\" / \"%s\") -> %d\n", action, h.phase, h.next_phase, gate,
           h.event_status, h.routing_message, rc);
    if (rc) return fail("heat action", rc, s);
    if (action == RX_HEAT_ROUND_CONTINUE) p->heat_rounds++;
    return 0;
}

static int serve_requests(struct rx_session* s, struct presenter* p, const rx_career_status* status) {
    for (int guard = 0; guard < 16; guard++) {
        rx_career_request r;
        memset(&r, 0, sizeof r);
        r.size = sizeof r;
        r.version = RX_CAREER_API_VERSION;
        int rc = rx_session_career_next(s, p->generation, &r);
        if (rc == RX_CAREER_EMPTY) return 0;
        if (rc) return fail("next", rc, s);
        printf("career_series_test: REQUEST %llu kind %u \"%s\"\n", (unsigned long long)r.id, r.kind, r.name);
        switch (r.kind) {
        case RX_CAREER_REQUEST_UI_EVENT:
            if (strcmp(r.name, "ShowRaceRulesPopup") != 0) return fail("unexpected UI event", 0, s);
            rc = rx_session_career_ack(s, p->generation, r.id, r.kind, 1);
            if (rc) return fail("rules shown", rc, s);
            rc = rx_session_career_rules_accept(s, p->generation, r.id, 0);
            if (rc) return fail("rules accept", rc, s);
            break;
        case RX_CAREER_REQUEST_PAGE_WIPE:
            if (page_wipe(s, p, &r)) return 1;
            break;
        case RX_CAREER_REQUEST_FADE_OUT:
            rc = rx_session_career_ack(s, p->generation, r.id, r.kind, 1);
            if (rc) return fail("fade", rc, s);
            break;
        case RX_CAREER_REQUEST_HUB:
            rc = rx_session_career_ack(s, p->generation, r.id, r.kind, 1);
            if (rc) return fail("hub", rc, s);
            p->hub_id = r.id;
            if (status->results_count > 0 && status->post_event_complete) p->results_shown = 1;
            break;
        case RX_CAREER_REQUEST_UI_ROUTE:
            rc = rx_session_career_ack(s, p->generation, r.id, r.kind, 1);
            if (rc) return fail("route", rc, s);
            break;
        case RX_CAREER_REQUEST_CHECKPOINT:
            if (save_checkpoint(s, p, r.id)) return 1;
            break;
        default:
            return fail("unknown request kind", (int)r.kind, s);
        }
    }
    return 0;
}

static int serve_replay(struct rx_session* s, struct presenter* p) {
    for (int guard = 0; guard < 64; guard++) {
        rx_replay_request r;
        memset(&r, 0, sizeof r);
        r.size = sizeof r;
        r.version = RX_REPLAY_API_VERSION;
        int rc = rx_session_replay_next(s, p->generation, &r);
        if (rc == RX_CAREER_EMPTY || rc == RX_CAREER_INACTIVE) return 0;
        if (rc) return fail("replay next", rc, s);
        rc = rx_session_replay_ack(s, p->generation, &r, RX_REPLAY_APPLIED);
        if (rc) return fail("replay applied", rc, s);
        rc = rx_session_replay_ack(s, p->generation, &r, RX_REPLAY_PAINTED);
        if (rc) return fail("replay painted", rc, s);
    }
    return 0;
}

static int serve_completion(struct rx_session* s, struct presenter* p, const rx_career_status* status, int* done) {
    rx_career_completion_status c;
    memset(&c, 0, sizeof c);
    c.size = sizeof c;
    c.version = RX_CAREER_COMPLETION_VERSION;
    int rc = rx_session_career_completion(s, p->generation, &c);
    if (rc) return fail("completion status", rc, s);
    /* Continue: the results page's button (and, for Moto, only after the second moto) */
    if (!p->continue_sent && status->continue_allowed && !status->continue_submitted && status->pending_requests == 0) {
        int final_moto = 1;
        if (p->moto) {
            rx_career_moto_status m;
            final_moto = moto_status(s, p, &m) == 0 && m.post_complete && m.phase == 7;
        }
        if (final_moto) {
            rc = rx_session_career_continue(s, p->generation);
            printf("career_series_test: CONTINUE -> %d (rank %d, %u results, status \"%s\")\n", rc, status->local_rank,
                   status->results_count, status->event_status);
            if (rc) return fail("continue", rc, s);
            p->continue_sent = 1;
            return 0;
        }
    }
    if (c.complete_event_allowed && !c.complete_event_state) {
        rc = rx_session_career_complete_event(s, p->generation, c.ticket);
        printf("career_series_test: COMPLETE_EVENT -> %d\n", rc);
        if (rc) return fail("complete event", rc, s);
        return 0;
    }
    if (c.frontend_ready_allowed && !c.frontend_ready) {
        rc = rx_session_career_frontend_ready(s, p->generation, c.ticket, c.frontend_request_id);
        printf("career_series_test: FRONTEND_READY \"%s\" -> %d\n", c.frontend_route, rc);
        if (rc) return fail("frontend ready", rc, s);
        return 0;
    }
    if (c.complete_series_allowed && !c.complete_series_state) {
        rc = rx_session_career_complete_series(s, p->generation, c.ticket);
        printf("career_series_test: COMPLETE_SERIES -> %d\n", rc);
        if (rc) return fail("complete series", rc, s);
        return 0;
    }
    if (c.durable) *done = 1;
    return 0;
}

static int read_status(struct rx_session* s, uint64_t generation, rx_career_status* status) {
    memset(status, 0, sizeof *status);
    status->size = sizeof *status;
    status->version = RX_CAREER_API_VERSION;
    return rx_session_career_status(s, generation, status);
}


/* ---- CST_PAGE_TRACE=1: the Moto / Heat hub page against the race (w-moto2prep) ----------------------------------------------------
 * Each frame whose (activity state, the event's phase / next / gate, pending requests, race state) differs from the last is printed
 * with the frame and the start board's timer, and the frames at which the client's hub page (OwnCareerPages.Tick: the Moto page and
 * TickHeat) is released after a gate handoff:
 *   running  the release before w-moto2prep: the handoff ended and the activity is InProgress (2): the page went at GO;
 *   ready    the release since: the handoff ended and the race's activity is in Pre / Countdown / InProgress (0..2).
 * A race whose rules popup shows has the page replaced by the popup before either (the REQUEST lines). */
struct page_trace { int on, race, running_at, ready_at, countdown_at, go_at; char last[220]; };
static void page_trace_frame(struct rx_session* s, struct presenter* p, struct page_trace* t, int frame) {
    if (!t->on || (!p->moto && !p->heat)) return;
    rx_career_status st;
    if (read_status(s, p->generation, &st)) return;
    uint32_t phase, next, gate_active, area = 0;
    if (p->moto) {
        rx_career_moto_status m;
        if (moto_status(s, p, &m)) return;
        phase = m.phase; next = m.next_phase; gate_active = m.gate_active;
    } else {
        rx_career_heat_status h;
        if (heat_status(s, p, &h)) return;
        phase = h.phase; next = h.next_phase; gate_active = h.gate_active; area = h.activity_area_required;
    }
    rx_race_state race;
    memset(&race, 0, sizeof race);
    rx_session_race(s, &race);
    /* OwnCareerPages: Moto (phase 6 / 7, next 8), Heat (next 8, no activity area to load) */
    const int handoff = (p->moto ? (phase == 6 || phase == 7) : !area) && next == 8 && !gate_active && st.activity_present == 1 &&
                        st.pending_requests == 0;
    const int running = handoff && st.activity_state == 2;
    const int ready = handoff && st.activity_state <= 2;
    char key[220];
    snprintf(key, sizeof key, "activity %u state %u sub %u | %s phase %u next %u gate %u area %u | pending %u | race %d | release running %d ready %d",
             st.activity_present, st.activity_state, st.activity_substate, p->moto ? "moto" : "heat", phase, next, gate_active, area,
             st.pending_requests, race.valid ? race.state : -1, running, ready);
    if (strcmp(key, t->last) != 0) {
        printf("career_series_test: PAGE frame %d: %s board %.3f\n", frame, key, race.start_timer);
        snprintf(t->last, sizeof t->last, "%s", key);
    }
    if (!handoff) { if (st.activity_state > 2) t->ready_at = t->running_at = t->countdown_at = t->go_at = 0; return; }
    if (ready && !t->ready_at) t->ready_at = frame;
    if (running && !t->running_at) t->running_at = frame;
    if (race.valid && race.state == 1 && !t->countdown_at) t->countdown_at = frame;
    if (race.valid && race.state == 2 && !t->go_at) {
        t->go_at = frame;
        t->race++;
        printf("career_series_test: PAGE_SUMMARY race %d (%s phase %u): release 'ready' frame %d, Countdown frame %d, GO frame %d, release 'running' frame %d; "
               "the world shows %d frames (%.2f s) before GO with 'ready', %d with 'running'\n", t->race, p->moto ? "moto" : "heat", phase,
               t->ready_at, t->countdown_at, t->go_at, t->running_at, t->go_at - t->ready_at, (t->go_at - t->ready_at) / 60.0,
               t->go_at - t->running_at);
    }
}

/* ---- the race HUD query (rx_session_race_hud): what the Eliminator / Outrun / Section Race / Freestyle components show ---------------- */
struct hud_seen {
    rx_race_hud_state last;
    uint32_t eliminated, gate, leaders, final_card;
    int32_t max_bars[4], sections_timed, best_set;
    rx_stunt_hud_state stunt_last;
    uint32_t stunts, warn_frames; int warned;
};

static void hud_sample(struct rx_session* s, struct hud_seen* h, int frame) {
    rx_race_hud_state v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    v.version = RX_RACE_HUD_VERSION;
    if (rx_session_race_hud(s, &v) != 0 || !v.valid) return;
    const rx_race_hud_state* o = &h->last;
    if (v.eliminated_events != o->eliminated_events) {
        printf("career_series_test: frame %d: HUD eliminated #%u: \"%s\" / Eliminated / \"%s\"! (slots %d <- %d)\n", frame, v.eliminated_events,
               v.eliminator_name, v.eliminated_name, v.eliminated_slot, v.eliminator_slot);
        h->eliminated = v.eliminated_events;
    }
    if (v.gate_events != o->gate_events) {
        printf("career_series_test: frame %d: HUD gate GATE: %d/%d (shown %u times)\n", frame, v.gate_number, v.gate_total, v.gate_events);
        h->gate = v.gate_events;
    }
    if (v.leader_events != o->leader_events || v.leader_set != o->leader_set) {
        printf("career_series_test: frame %d: HUD section leaders mask %x:", frame, v.leader_set);
        for (int i = 0; i < RX_RACE_HUD_SECTIONS; i++) if (v.leader_set & (1u << i)) printf(" #%d \"%s\"", i + 1, v.leader_names[i]);
        printf("\n");
        h->leaders = v.leader_events;
    }
    if (v.section_current != o->section_current && v.section_current >= 0)
        printf("career_series_test: frame %d: HUD in section %d of %d (best %.2f)\n", frame, v.section_current, v.section_count, v.section_best);
    if (v.section_current >= 0 && v.section_time > 0.0f && v.section_time < 1.0e30f) h->sections_timed++;
    if (v.section_current >= 0 && v.section_best > 0.0f && v.section_best < 1.0e30f) h->best_set++;
    for (int i = 0; i < 4; i++) {
        if (v.judge_bars[i] > h->max_bars[i]) h->max_bars[i] = v.judge_bars[i];
        if (v.judge_bars[i] != o->judge_bars[i]) printf("career_series_test: frame %d: HUD judge %d bars %d\n", frame, i + 1, v.judge_bars[i]);
    }
    if (v.final_events != o->final_events) {
        printf("career_series_test: frame %d: HUD final score card overall %.2f difficulty %.2f execution %.2f flow %.2f variety %.2f\n", frame,
               v.final_scores[0], v.final_scores[1], v.final_scores[2], v.final_scores[3], v.final_scores[4]);
        h->final_card = v.final_events;
    }
    if (v.activity_state != o->activity_state || v.activity_type != o->activity_type)
        printf("career_series_test: frame %d: HUD activity type %d mode %d state %d\n", frame, v.activity_type, v.activity_mode, v.activity_state);
    if (v.restart_events != o->restart_events) printf("career_series_test: frame %d: HUD restart %u\n", frame, v.restart_events);
    /* the event timer's WarningTime flash (MXUITimerComponent 005f7140 armed by "FreestyleEnd" 5 s before the Duration, 00562bd0): the Freestyle
     * activity's last 5 s with time left, in the modes that raise it (not 0, 2, 3) */
    if (v.activity_type == 5 && v.activity_mode != 0 && v.activity_mode != 2 && v.activity_mode != 3 && v.clock_counts_up == 0 &&
        v.clock >= v.clock_limit - 5.0f && v.clock_limit - v.clock > 0.0f) {
        if (!h->warned) printf("career_series_test: frame %d: HUD event timer WarningTime flash starts (clock %.2f of %.2f)\n", frame, v.clock, v.clock_limit);
        h->warned = 1; h->warn_frames++;
    }
    h->last = v;
    /* the stunt feedback overlay's trick names (rx_session_stunt_hud) */
    rx_stunt_hud_state t;
    memset(&t, 0, sizeof t);
    t.size = sizeof t;
    t.version = RX_STUNT_HUD_VERSION;
    if (rx_session_stunt_hud(s, &t) == 0 && t.valid) {
        for (int i = (int)(t.events - h->stunt_last.events < t.count ? t.events - h->stunt_last.events : t.count) - 1; i >= 0 && t.events >= h->stunt_last.events; i--)
            printf("career_series_test: frame %d: HUD trick #%u (%s): \"%s\"%s%s\n", frame, t.entries[i].number, t.entries[i].physics ? "physics" : "trick", t.entries[i].trick,
                   t.entries[i].trick[0] && t.entries[i].stunt[0] ? " + " : "", t.entries[i].stunt);
        if (t.events > h->stunts) h->stunts = t.events;
        if (t.restart_events != h->stunt_last.restart_events) printf("career_series_test: frame %d: HUD tricks cleared by the restart (%u)\n", frame, t.restart_events);
        h->stunt_last = t;
    }
}

/* 0 = the series' HUD showed what the series should have */
static int hud_check(const struct hud_seen* h, uint32_t series) {
    int bad = 0;
    switch (series) {
    case 5: case 32: case 40: case 67: case 84:
        if (!h->eliminated || !h->last.eliminated_name[0] || !h->last.eliminator_name[0]) bad = 1;
        break;
    case 9: case 56: case 64: case 83: case 88:
        if (!h->gate || h->last.gate_total <= 0 || h->last.gate_number < 0 || h->last.gate_number > h->last.gate_total) bad = 1;
        break;
    case 52:
        if (!h->sections_timed || !h->best_set) bad = 1;
        break;
    case 22:
        if (h->max_bars[0] + h->max_bars[1] + h->max_bars[2] + h->max_bars[3] <= 0) bad = 1;
        if (getenv("CST_TRICKS") && atoi(getenv("CST_TRICKS")) && (!h->stunts || !h->warned)) bad = 1;   /* names of the tricks ridden, and the timer's last 5 s */
        break;
    default: break;
    }
    printf("career_series_test: HUD %s series %u: eliminated %u gate %u (%d/%d) leaders %u timed %d best %d bars %d %d %d %d final %u tricks %u warning %u frames\n",
           bad ? "CHECK FAILED" : "check", series, h->eliminated, h->gate, h->last.gate_number, h->last.gate_total, h->leaders,
           h->sections_timed, h->best_set, h->max_bars[0], h->max_bars[1], h->max_bars[2], h->max_bars[3], h->final_card, h->stunts, h->warn_frames);
    return bad;
}

static void camera_setting_trace(struct rx_session* s, const char* at) {
    if (!getenv("CST_DRIVING_CAMERA")) return;
    int32_t v = -9; const int rc = rx_session_ui_setting(s, "DrivingCamera", 0, 0, &v);
    rx_camera_bindings cb; memset(&cb, 0, sizeof cb); rx_session_camera_bindings(s, &cb);
    printf("career_series_test: CAMERA after %s: DrivingCamera setting %d (rc %d), local player camera %d\n", at, v, rc, cb.driving_camera);
}

int main(int argc, char** argv) {
    if (argc < 4) {
        fprintf(stderr, "usage: career_series_test <install root> <player24 export> <list | series id> [max frames]\n");
        return 2;
    }
    signal(SIGABRT, on_signal);
    signal(SIGSEGV, on_signal);
    signal(SIGBUS, on_signal);
    signal(SIGALRM, on_signal);
    alarm(getenv("CST_WATCHDOG") ? (unsigned)atoi(getenv("CST_WATCHDOG")) : 3600u);
    setvbuf(stdout, NULL, _IONBF, 0);
    const int list = strcmp(argv[3], "list") == 0;
    const uint32_t series = list ? 0 : (uint32_t)atoi(argv[3]);
    const int max_frames = argc > 4 ? atoi(argv[4]) : 60000;

    step("create %s", argv[1]);
    struct rx_session* s = rx_session_create(argv[1]);
    if (!s) return fail("create", 0, NULL);
    uint32_t bytes = 0;
    void* profile = read_file(argv[2], &bytes);
    if (!profile) return fail("read export", 0, s);
    int rc = rx_session_player_import(s, profile, bytes);
    if (rc) return fail("player import", rc, s);
    /* CST_DRIVING_CAMERA=<0..3>: the saved camera written as the client's OpenReflexSimulation.ApplyUiSettings does (after the profile,
     * before the career configure, and again after it: see there); every race of the series must then ride it (CAMERA lines; a race on
     * another camera fails the run) */
    const int want_camera = getenv("CST_DRIVING_CAMERA") ? atoi(getenv("CST_DRIVING_CAMERA")) : -1;
    if (want_camera >= 0) {
        int32_t before = -9;
        rc = rx_session_ui_setting(s, "DrivingCamera", want_camera, 1, &before);
        printf("career_series_test: DrivingCamera %d written -> %d (was %d)\n", want_camera, rc, before);
        if (rc) return fail("DrivingCamera", rc, s);
    }
    int camera_races = 0, camera_wrong = 0, camera_last = -2;

    step("configure%s", "");
    rx_career_config config;
    memset(&config, 0, sizeof config);
    config.size = sizeof config;
    config.version = RX_CAREER_API_VERSION;
    config.controller = 0;
    config.presenter_capabilities = all_capabilities();
    uint64_t generation = 0;
    rc = rx_session_career_configure(s, &config, &generation);
    if (rc) return fail("configure", rc, s);
    camera_setting_trace(s, "configure");
    /* the configure re-defaults the primary device's UI settings row (rx_ui_set_primary_player_device_index(0): UISettings_Reset, the
     * original's profile route; the port's profile carries no PlayerSettings rows), so the client writes its settings again here
     * (OpenReflexSimulation's career start, w-camfix); CST_CAMERA_BEFORE_ONLY=1 leaves that out (the old client: every race on VISUAL) */
    if (want_camera >= 0 && !(getenv("CST_CAMERA_BEFORE_ONLY") && atoi(getenv("CST_CAMERA_BEFORE_ONLY")))) {
        int32_t before = -9;
        rc = rx_session_ui_setting(s, "DrivingCamera", want_camera, 1, &before);
        printf("career_series_test: DrivingCamera %d written again after the configure -> %d (was %d)\n", want_camera, rc, before);
        if (rc) return fail("DrivingCamera", rc, s);
    }
    if (list) return list_series(s, generation);

    struct presenter p;
    memset(&p, 0, sizeof p);
    p.generation = generation;
    p.export_path = argv[2];
    rc = rx_session_replay_begin(s, generation);
    if (rc) return fail("replay begin", rc, s);

    rx_career_series_info info;
    memset(&info, 0, sizeof info);
    info.size = sizeof info;
    info.version = RX_CAREER_API_VERSION;
    rc = rx_session_career_series(s, generation, series, &info);
    if (rc) return fail("series", rc, s);
    printf("career_series_test: SERIES %u owner %s format %s activity %s class %d supported %u locked %u eligible %u\n", series,
           info.owner, info.format, info.activity, info.vehicle_class, info.supported, info.locked, info.eligible);
    p.moto = strcmp(info.format, "Moto") == 0;
    p.heat = strcmp(info.format, "Heat") == 0;

    step("load%s", "");
    rc = rx_session_career_load(s, generation, series);
    if (rc) return fail("load", rc, s);
    camera_setting_trace(s, "load");
    rx_career_vehicle_identity vehicle;
    memset(&vehicle, 0, sizeof vehicle);
    vehicle.size = sizeof vehicle;
    vehicle.version = RX_CAREER_VEHICLE_VERSION;
    rc = rx_session_career_vehicle(s, generation, &vehicle);
    if (rc) return fail("vehicle", rc, s);
    printf("career_series_test: VEHICLE type %d model %d class %d\n", vehicle.vehicle_type, vehicle.model_id, vehicle.authored_class);
    rc = rx_session_career_select_vehicle(s, generation, (uint32_t)vehicle.vehicle_type);
    if (rc) return fail("select vehicle", rc, s);
    camera_setting_trace(s, "select vehicle");
    step("initialize%s", "");
    rc = rx_session_career_initialize(s, generation);
    if (rc) return fail("initialize", rc, s);
    camera_setting_trace(s, "initialize");
    if (p.moto || p.heat) {
        /* CareerPracticePopup: SKIP (CST_PRACTICE=1: PRACTICE) */
        rc = rx_session_career_moto_practice_choice(s, generation, getenv("CST_PRACTICE") && atoi(getenv("CST_PRACTICE")) ? 0 : 1);
        if (rc) return fail("practice choice", rc, s);
    }
    step("start%s", "");
    rc = rx_session_career_start(s, generation);
    if (rc) return fail("start", rc, s);
    camera_setting_trace(s, "start");

    struct autopilot pilot;
    memset(&pilot, 0, sizeof pilot);
    struct hud_seen hud;
    memset(&hud, 0, sizeof hud);
    static struct page_trace trace;
    trace.on = getenv("CST_PAGE_TRACE") && atoi(getenv("CST_PAGE_TRACE"));
    int done = 0, last_state = -1, last_laps = -1;
    char last_status[32] = "";
    int restart_at[16], restarts = 0, restarts_done = 0, racing_frames = 0;
    const int post_skip = getenv("CST_POST_SKIP") ? atoi(getenv("CST_POST_SKIP")) : -1;
    int post1_frames = 0, post1_skip_at = -1, post_last_sub = -1;
    if (getenv("CST_RESTART")) {
        char list_text[128];
        snprintf(list_text, sizeof list_text, "%s", getenv("CST_RESTART"));
        for (char* t = strtok(list_text, ","); t && restarts < 16; t = strtok(NULL, ",")) restart_at[restarts++] = atoi(t);
    }
    for (int frame = 0; frame < max_frames && !done; frame++) {
        rx_career_status status;
        rc = read_status(s, generation, &status);
        if (rc) return fail("status", rc, s);
        if (serve_requests(s, &p, &status)) return 1;
        if (serve_replay(s, &p)) return 1;
        rc = read_status(s, generation, &status);
        if (rc) return fail("status", rc, s);
        if (p.moto && moto_pages(s, &p, &status)) return 1;
        if (p.heat && heat_pages(s, &p, &status)) return 1;
        if (serve_completion(s, &p, &status, &done)) return 1;
        if (done) break;
        page_trace_frame(s, &p, &trace, frame);
        if (strcmp(status.event_status, last_status) != 0) {
            printf("career_series_test: frame %d: SeriesEventStatus \"%s\" (event phase %u next %u)\n", frame, status.event_status,
                   status.event_phase, status.event_next_phase);
            snprintf(last_status, sizeof last_status, "%s", status.event_status);
        }
        rx_race_state race;
        memset(&race, 0, sizeof race);
        rx_session_race(s, &race);
        if (race.valid && (race.state != last_state || race.laps_completed != last_laps)) {
            printf("career_series_test: frame %d: race state %d laps %d/%d place %d riders %d time %.1f\n", frame, race.state,
                   race.laps_completed, race.num_laps, race.place, race.riders, race.race_time);
            last_state = race.state;
            last_laps = race.laps_completed;
        }
        hud_sample(s, &hud, frame);
        if (want_camera >= 0 && race.valid && race.state >= 1 && race.state <= 3) {
            rx_camera_bindings cb; memset(&cb, 0, sizeof cb);
            const int cam = rx_session_camera_bindings(s, &cb) == 0 ? cb.driving_camera : -9;
            if (cam != camera_last) { camera_setting_trace(s, "a race state change"); printf("career_series_test: frame %d: CAMERA race state %d driving_camera %d (want %d)\n", frame, race.state, cam, want_camera); camera_last = cam; }
            if (race.state == 2 && racing_frames == 0) camera_races++;
            if (race.state == 2 && cam != want_camera) camera_wrong++;
        } else camera_last = -2;
        {   /* CST_POST_SKIP: the post-race finish scenario (Post 1), skipped <n> frames in */
            const int sub = status.activity_present == 1 && status.activity_state == 4 ? (int)status.activity_substate : -1;
            if (sub == 1) {
                post1_frames++;
                if (post_skip >= 0 && post1_frames == post_skip + 1 && post1_skip_at < 0) {
                    const int sr = rx_session_skip_scenario(s);
                    post1_skip_at = post1_frames;
                    printf("career_series_test: frame %d: POST skip sent %d frames into the finish scenario -> %d\n", frame, post_skip, sr);
                }
            } else if (post_last_sub == 1) {
                printf("career_series_test: frame %d: POST finish scenario lasted %d frames (%.2f s)%s\n", frame, post1_frames, post1_frames / 60.0,
                       post1_skip_at < 0 ? "" : post1_frames <= post1_skip_at + 2 ? ", ended by the skip" : ", the skip was IGNORED");
                post1_frames = 0; post1_skip_at = -1;
            }
            post_last_sub = sub;
        }
        racing_frames = race.valid && race.state == 2 ? racing_frames + 1 : 0;
        if (restarts_done < restarts && racing_frames == restart_at[restarts_done] && !status.records_finalized) {
            g_step = "restart";
            const int rr = rx_session_restart_activity(s);
            rx_career_status after;
            read_status(s, generation, &after);
            rx_session_race(s, &race);
            printf("career_series_test: frame %d: RESTART #%d after %d racing frames -> %d (race state %d laps %d/%d time %.1f; event \"%s\" phase %u results %u)\n", frame,
                   restarts_done + 1, racing_frames, rr, race.state, race.laps_completed, race.num_laps, race.race_time, after.event_status, after.event_phase,
                   after.results_count);
            if (rr) return fail("restart", rr, s);
            if (race.state >= 2) return fail("the restart did not put the race back to its start", race.state, s);
            restarts_done++;
            racing_frames = 0; last_state = -1;
        }
        if (status.records_finalized) continue;   /* terminal: no frames after the records were settled */
        autopilot_input(s, &pilot, race.valid && race.state == 2);
        g_step = "advance";
        rc = rx_session_advance(s);
        if (rc) {
            if (rx_session_get_state(s) == RX_SESSION_ERROR) return fail("advance", rc, s);
        }
    }
    if (!done) return fail("series did not complete in the frame budget", max_frames, s);
    rx_career_status status;
    if (hud_check(&hud, series) && getenv("CST_HUD_CHECK") && atoi(getenv("CST_HUD_CHECK"))) return 1;
    read_status(s, generation, &status);
    printf("career_series_test: PASS series %u: rank %d, reward %u + %u, %d checkpoints, %d restart(s)\n", series, status.local_rank,
           status.reward_base, status.reward_bonus, p.checkpoints_committed, restarts_done);
    if (restarts_done != restarts) return fail("a requested restart never fired", restarts_done, s);
    if (want_camera >= 0) {
        printf("career_series_test: CAMERA %d races, %d racing frames on another camera than %d\n", camera_races, camera_wrong, want_camera);
        if (camera_wrong || !camera_races) return fail("a race rode another camera than the saved DrivingCamera", camera_wrong, s);
    }
    rx_session_destroy(s);
    return 0;
}
