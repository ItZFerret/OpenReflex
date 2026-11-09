/* champ_weekend_session.inc -- RESTORED CONTENT: the championship weekend's session API (include/rx_champ_weekend.h), included by
 * rx_session.c. Host policy only (the stage lists, the fields, the grids, the standings); the game side is game/restored_champ_weekend.c
 * and the verified championship classes (game/race_champ.c). */
#include "rx_champ_weekend.h"
#include "../game/restored_champ_weekend.h"

/* The Heat format's qualification, as the original's MXSeriesEventHeat::vf13 (005adde0, game/career_heat.c) decides it for a heat of 12:
 * heat places 1-5 go to the main event, 6-11 to the LCQ, 12 and below are out; the LCQ's first two join the main event (a main of 7).
 * FIELD SIZE (PORT ADDITION, the rider cap: a weekend of 2 .. 32 riders): the split scales with the heat's field N (the riders the heat
 * placed, every rider of the weekend) by the 12-rider proportions, in integer arithmetic so that N = 12 is the original's 5 / 11 / 2 exactly:
 *   to the main    H = (5 N + 6) / 12   (5 of 12, rounded; at least 1)
 *   out            O = (N + 6) / 12     (1 of 12, rounded): the heat's last O places go home; the LCQ is heat H+1 .. N-O
 *   LCQ to main    T = (L + 1) / 3      (L = N - O - H, the LCQ's field; 2 of 6, rounded; at least 1). An LCQ whose riders would all go
 *                                       through (T >= L: only at N = 2) is not run: its riders go straight to the main (H += L, L = 0)
 * The table (rx_champ_weekend.h has it too) N: H / LCQ / T -> main:  2: 2/0/0 -> 2,  6: 3/2/1 -> 4,  8: 3/4/1 -> 4,  10: 4/5/2 -> 6,
 *   12: 5/6/2 -> 7 (the original's),  16: 7/8/3 -> 10,  20: 8/10/3 -> 11,  24: 10/12/4 -> 14,  28: 12/14/5 -> 17,  32: 13/16/5 -> 18.
 * At 24 the heat sends what the original's 24-rider Heat format (MXSeriesEventHeat: two heats of 12, 1-5 of each to the main, 6-11 to the
 * LCQ) sends: 10 to the main, 12 to the LCQ, 2 out. That format runs one heat per session with the other heat decided by the table order
 * (rx_career_heat.h); a weekend runs every stage on track with the whole field, so one heat of N keeps every rider racing its heat. The
 * LCQ keeps its one-in-three transfer (the original's 24-rider main is the 12 seats' cap: 2 of 12 would leave an LCQ of 16 at N = 32 with
 * 2 places). The main at 32 is 18: the 16-slot gate and 2 behind it (grid_rows.c). */
static void champ_split(int32_t field, int32_t* to_main, int32_t* lcq_last, int32_t* lcq_to_main) {
    int32_t h = (5 * field + 6) / 12;
    if (h < 1) h = 1;
    if (h > field) h = field;
    const int32_t last = field - (field + 6) / 12;
    int32_t l = last - h, t = (l + 1) / 3;
    if (l < 0) l = 0;
    if (l > 0 && t < 1) t = 1;
    if (t >= l) { h += l; l = 0; t = 0; }   /* (everyone would go through: no LCQ) */
    *to_main = h; *lcq_last = h + l; *lcq_to_main = t;
}
int rx_champ_weekend_split(int32_t field, int32_t* to_main, int32_t* lcq_last, int32_t* lcq_to_main) {
    if (field < 1 || field > RX_CHAMP_MAX_RIDERS || !to_main || !lcq_last || !lcq_to_main) return 1;
    champ_split(field, to_main, lcq_last, lcq_to_main);
    return 0;
}
/* Every race of the career's Heat and Moto formats is 3 laps (MXSeriesEventSingle set_type 005b04a0: +0xb4 = 3; the practice is
 * SetupActivity(..., 3, ...) in vf10 005ad960): the weekend uses the same. */
#define CHAMP_LAPS 3
/* TIMED RACES (PORT ADDITION, version 4): each stage's default clock (minutes) and laps after it (game/restored_champ_weekend.c). A real
 * pro National moto is 30:00 + 2 and a Supercross main 20:00 + 1; the weekend scales them for an exhibition field (the owner's example:
 * a 15:00 + 1 moto). The practices are timed only (+ 0: the leader's next crossing after the clock is the flag). */
static const int32_t champ_default_minutes[][RX_CHAMP_MAX_STAGES] = { {10, 5, 15, 15}, {6, 4, 3, 10}, {0}, {0} };
static const int32_t champ_default_extra[][RX_CHAMP_MAX_STAGES]   = { { 0, 1,  1,  1}, {0, 1, 1,  1}, {0}, {0} };

struct champ_stage_def {
    const char* label;
    rx_addr name;           /* the original's ActivityName literal (the exe's string at this address) */
    const char* text;       /* the same name as host text (the weekend can be begun before any session exists) */
    int32_t scored, field, grid, grid_from;
};
/* National: the original names no National LCQ class (only Supercross / Endurocross / Minimoto have one). RaceChampIntro (0x0092d99c,
 * 0x1f) is not a stage: it runs with the restored cast but the PC data has no intro script for it (Scenario_IntroScriptName 00747940
 * names the series "X" for 0x1f), so it shows no presentation and then races like a heat; whether to restore it is the owner's call. */
static const struct champ_stage_def champ_national[] = {
    {"PRACTICE", 0x0092d958u, "NationalChampPractice",   0, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_EVENT,    -1},
    {"HEAT",     0x0092d944u, "NationalChampHeat",       0, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_BEST_LAP,  0},
    {"MOTO 1",   0x0092d92cu, "NationalChampMainEvent",  1, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_PLACE,     1},
    {"MOTO 2",   0x0092d914u, "NationalChampMainEvent2", 1, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_PLACE,     2},
};
/* Supercross: practice, the heat (the whole field), the LCQ (heat 6-11) and the main event (heat 1-5 + LCQ 1-2) */
static const struct champ_stage_def champ_supercross[] = {
    {"PRACTICE", 0x0092d8fcu, "SupercrossChampPractice",  0, RX_CHAMP_FIELD_ALL,  RX_CHAMP_GRID_EVENT,    -1},
    {"HEAT",     0x0092d8e8u, "SupercrossChampHeat",      0, RX_CHAMP_FIELD_ALL,  RX_CHAMP_GRID_BEST_LAP,  0},
    {"LCQ",      0x0092d8b8u, "SupercrossChampLCQ",       0, RX_CHAMP_FIELD_LCQ,  RX_CHAMP_GRID_PLACE,     1},
    {"MAIN",     0x0092d8ccu, "SupercrossChampMainEvent", 1, RX_CHAMP_FIELD_MAIN, RX_CHAMP_GRID_MAIN,      1},
};
/* Waypoint: the intro (0x21: its intro script is "WP<n>_<track>_Intro", which the PC data has), the practice, the main event. The laps
 * are the event's own (a waypoint race is gate to gate). */
static const struct champ_stage_def champ_waypoint[] = {
    {"INTRO",    0x0092d970u, "WaypointRaceChampIntro",     0, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_EVENT, -1},
    {"PRACTICE", 0x0092d808u, "WaypointRaceChampPractice",  0, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_PLACE,  0},
    {"MAIN",     0x0092d7ecu, "WaypointRaceChampMainEvent", 1, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_PLACE,  1},
};
/* Freestyle: the practice and the two main events (0x3a / 0x3b / 0x3c: the factory's MXActivityFreestyleChampPractice vtable 0x9444cc and
 * MainEvent 0x9445cc, build_activity_race.c). Judged: the standings are the mains' TotalScores. FreestyleChampIntro (0x0092d988, 0x20)
 * runs with the restored cast and its "FMX_<track>_Intro" script exists (it plays 499 frames), but that is the same intro every
 * Freestyle stage plays, and the intro class then runs a whole session in which the local rider's tricks score nothing (0.00, the AI
 * are scored): as a stage it adds no presentation of its own and a session that does not count, so it is left out (the owner's call). */
static const struct champ_stage_def champ_freestyle[] = {
    {"PRACTICE", 0x0092d71cu, "FreestyleChampPractice",   0, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_EVENT, -1},
    {"MAIN 1",   0x0092d704u, "FreestyleChampMainEvent",  1, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_EVENT, -1},
    {"MAIN 2",   0x0092d6e8u, "FreestyleChampMainEvent2", 1, RX_CHAMP_FIELD_ALL, RX_CHAMP_GRID_EVENT, -1},
};
static const struct { const struct champ_stage_def* stages; int32_t count; uint32_t base_type; const char* event_type; int32_t laps, judged, timeable; } champ_disciplines[] = {
    {champ_national, 4, 0, "National", CHAMP_LAPS, 0, 1},
    {champ_supercross, 4, 1, "Supercross", CHAMP_LAPS, 0, 1},
    {champ_waypoint, 3, 6, "WaypointRace", 0, 0, 0},
    {champ_freestyle, 3, 5, "Freestyle", 0, 1, 0},
};
#define CHAMP_DISCIPLINES ((int32_t)(sizeof champ_disciplines / sizeof champ_disciplines[0]))

static int champ_weekend_ok(const rx_champ_weekend* w) {
    return w && w->size == sizeof *w && w->version == RX_CHAMP_WEEKEND_VERSION && w->discipline >= 0 && w->discipline < CHAMP_DISCIPLINES &&
           w->stage_count == champ_disciplines[w->discipline].count && w->stage >= 0 && w->stage <= w->stage_count &&
           w->rider_count >= 0 && w->rider_count <= RX_CHAMP_MAX_RIDERS;
}

int rx_champ_weekend_begin(rx_champ_weekend* w, int32_t discipline) {
    if (!w || discipline < 0 || discipline >= CHAMP_DISCIPLINES) return 1;
    memset(w, 0, sizeof *w);
    w->size = sizeof *w;
    w->version = RX_CHAMP_WEEKEND_VERSION;
    w->discipline = discipline;
    w->series_point_id = 1;
    w->judged = champ_disciplines[discipline].judged;
    w->stage_count = champ_disciplines[discipline].count;
    for (int32_t i = 0; i < w->stage_count; i++) {
        const struct champ_stage_def* d = &champ_disciplines[discipline].stages[i];
        rx_champ_stage* st = &w->stages[i];
        snprintf(st->label, sizeof st->label, "%s", d->label);
        snprintf(st->activity, sizeof st->activity, "%s", d->text);
        st->activity_type = -1;
        st->laps = champ_disciplines[discipline].laps;
        if (champ_disciplines[discipline].timeable && champ_default_minutes[discipline][i] > 0) {
            w->stage_minutes[i] = champ_default_minutes[discipline][i];
            st->laps = champ_default_extra[discipline][i];
        }
        st->scored = d->scored;
        st->field = d->field;
        st->grid = d->grid;
        st->grid_from = d->grid_from;
    }
    for (int32_t i = 0; i < RX_CHAMP_MAX_RIDERS; i++) w->overall[i] = -1;
    return 0;
}

int rx_champ_weekend_set_timing(rx_champ_weekend* w, int32_t stage, int32_t minutes, int32_t laps) {
    if (!champ_weekend_ok(w) || stage < 0 || stage >= w->stage_count || w->stages[stage].raced || w->stages[stage].decided) return 1;
    if (minutes < 0 || minutes > RX_CHAMP_MAX_MINUTES || laps < 0 || laps > RX_CHAMP_MAX_LAPS) return 1;
    if (!champ_disciplines[w->discipline].timeable) return 1;
    if (minutes == 0 && laps < 1) return 1;   /* a lap race needs a lap */
    w->stage_minutes[stage] = minutes;
    w->stages[stage].laps = laps;
    return 0;
}

/* the stage of a field rule's source: the HEAT and the LCQ of the weekend (by label) */
static int32_t champ_stage_of(const rx_champ_weekend* w, const char* label) {
    for (int32_t i = 0; i < w->stage_count; i++) if (strcmp(w->stages[i].label, label) == 0) return i;
    return -1;
}

/* the split of the weekend's heat (champ_split of the riders it placed; nothing placed yet: a field of 0, nobody qualifies) */
static void champ_heat_split(const rx_champ_weekend* w, int32_t heat, int32_t* to_main, int32_t* lcq_last, int32_t* lcq_to_main) {
    int32_t field = 0;
    if (heat >= 0) for (int32_t i = 0; i < w->rider_count; i++) if (w->riders[i].place[heat] >= 1) field++;
    if (field == 0) { *to_main = *lcq_last = *lcq_to_main = 0; return; }
    champ_split(field, to_main, lcq_last, lcq_to_main);
}

/* whether rider r rides stage s (from the places recorded so far) */
static int champ_in_field(const rx_champ_weekend* w, int32_t s, int32_t r) {
    const rx_champ_rider* x = &w->riders[r];
    const int32_t heat = champ_stage_of(w, "HEAT"), lcq = champ_stage_of(w, "LCQ");
    int32_t to_main, lcq_last, lcq_to_main;
    switch (w->stages[s].field) {
    case RX_CHAMP_FIELD_LCQ:
        champ_heat_split(w, heat, &to_main, &lcq_last, &lcq_to_main);
        return heat >= 0 && x->place[heat] > to_main && x->place[heat] <= lcq_last;
    case RX_CHAMP_FIELD_MAIN:
        champ_heat_split(w, heat, &to_main, &lcq_last, &lcq_to_main);
        return (heat >= 0 && x->place[heat] >= 1 && x->place[heat] <= to_main) ||
               (lcq >= 0 && x->place[lcq] >= 1 && x->place[lcq] <= lcq_to_main);
    default:
        return 1;
    }
}

/* a sort key of rider r for stage s's grid (smaller starts further forward) */
static int64_t champ_grid_key(const rx_champ_weekend* w, int32_t s, int32_t r) {
    const rx_champ_stage* st = &w->stages[s];
    const rx_champ_rider* x = &w->riders[r];
    const int32_t from = st->grid_from;
    const int64_t unplaced = 1000;
    switch (st->grid) {
    case RX_CHAMP_GRID_BEST_LAP: {   /* the source's best lap (milliseconds); riders without one behind, by place */
        const float lap = from >= 0 ? x->best_lap[from] : 0.0f;
        if (lap > 0.0f) return (int64_t)(lap * 1000.0f);
        return 100000000 + (from >= 0 && x->place[from] ? x->place[from] : unplaced);
    }
    case RX_CHAMP_GRID_PLACE:
        return from >= 0 && x->place[from] ? x->place[from] : unplaced;
    case RX_CHAMP_GRID_MAIN: {       /* the heat's direct qualifiers by heat place, then the LCQ's by LCQ place */
        const int32_t heat = champ_stage_of(w, "HEAT"), lcq = champ_stage_of(w, "LCQ");
        int32_t to_main, lcq_last, lcq_to_main;
        champ_heat_split(w, heat, &to_main, &lcq_last, &lcq_to_main);
        if (heat >= 0 && x->place[heat] >= 1 && x->place[heat] <= to_main) return x->place[heat];
        if (lcq >= 0 && x->place[lcq] >= 1) return 100 + x->place[lcq];
        return unplaced;
    }
    default:
        return 0;
    }
}

struct champ_call { struct rx_session* s; rx_champ_weekend* w; int rc, retire; };

static int32_t champ_rider_by_key(const rx_champ_weekend* w, const char* key) {
    for (int32_t i = 0; i < w->rider_count; i++) if (strcmp(w->riders[i].key, key) == 0) return i;
    return -1;
}

static void champ_apply_run(void* p) {
    struct champ_call* c = (struct champ_call*)p;
    rx_champ_weekend* w = c->w;
    const int32_t s = w->stage;
    rx_champ_stage* st = &w->stages[s];
    const uint32_t type = rx_restored_champ_type_of(champ_disciplines[w->discipline].stages[s].name);
    if (type == 0x3e) { session_error(c->s, "championship stage: the activity name has no type"); c->rc = 2; return; }
    /* the field and the grid of the stage, per Player List row (the riders by their PlayerName) */
    enum { ROWS = 64 };
    int32_t in_field[ROWS], zone[ROWS], rider_of_row[ROWS];
    const uint32_t rows = rx_restored_champ_player_rows();
    if (rows > ROWS) { session_error(c->s, "championship stage: %u Player List rows", rows); c->rc = 2; return; }
    for (uint32_t r = 0; r < rows; r++) {
        char name[32];
        rx_restored_champ_player_name(r, name, sizeof name);
        rider_of_row[r] = champ_rider_by_key(w, name);
        in_field[r] = rider_of_row[r] < 0 ? -1 : champ_in_field(w, s, rider_of_row[r]);   /* -1: a row no stage has raced yet */
        zone[r] = -1;
    }
    if (st->grid != RX_CHAMP_GRID_EVENT) {
        int32_t order[ROWS], n = 0;
        for (uint32_t r = 0; r < rows; r++) if (rider_of_row[r] >= 0 && in_field[r] == 1) order[n++] = (int32_t)r;
        for (int32_t i = 1; i < n; i++) {   /* stable insertion sort by the grid key */
            const int32_t k = order[i];
            const int64_t key = champ_grid_key(w, s, rider_of_row[k]);
            int32_t j = i - 1;
            while (j >= 0 && champ_grid_key(w, s, rider_of_row[order[j]]) > key) { order[j + 1] = order[j]; j--; }
            order[j + 1] = k;
        }
        for (int32_t i = 0; i < n; i++) zone[order[i]] = i;
    } else if (rows > RX_ORIGINAL_RIDERS) {
        /* FIELD SIZE: the event's own grid (the first stage) of a field past the original's 12. The championship types skip the grid
         * pass (0075f890 returns for 0x22 .. 0x3d: the series' gate pick writes StartZone), so every StartZone is -1 and the join
         * (0054cd20, build_activity_driver.c) gives each rider the first free gate slot, and a rider past the gate's slots slot 0, on top
         * of the first rider: at 24 riders 9 of them began the practice there. The riders in the Player List's order (the order those
         * first free slots follow) as the zones: grid_rows.c lines up the ones past the gate behind it (w-secondrow). A field of 12 or
         * fewer keeps its zones as they are (the 12-rider weekend unchanged). */
        int32_t z = 0;
        for (uint32_t r = 0; r < rows; r++) if (in_field[r] != 0) zone[r] = z++;
    }
    const int32_t minutes = champ_disciplines[w->discipline].timeable ? w->stage_minutes[s] : 0;
    if (rx_restored_champ_apply(champ_disciplines[w->discipline].base_type, type, minutes > 0 ? RX_CHAMP_TIMED_LAPS : st->laps, in_field, zone, rows) != 0) {
        session_error(c->s, "championship stage: the event has no %s activity row", champ_disciplines[w->discipline].event_type);
        c->rc = 2;
        return;
    }
    if (minutes > 0) rx_restored_champ_timed_arm((float)minutes * 60.0f, st->laps);   /* TIMED RACES (PORT ADDITION) */
    st->activity_type = (int32_t)type;
    c->rc = 0;
}

int rx_session_champ_stage_apply(struct rx_session* s, rx_champ_weekend* w) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || !champ_weekend_ok(w)) return 1;
    if (w->finished || w->stage >= w->stage_count) { session_error(s, "championship weekend: every stage was run"); return 2; }
    if (!s->spec_loaded || s->career_phase || s->frames != 0) { session_error(s, "championship stage: apply after rx_session_event_load and before the first advance"); return 2; }
    const char* want = champ_disciplines[w->discipline].event_type;
    if (strcmp(s->spec.event_type, want) != 0) { session_error(s, "championship stage: this weekend needs a %s event (EventType \"%s\")", want, s->spec.event_type); return 2; }
    struct champ_call call = {s, w, 1, 0};
    char captured[4096];
    const int sig = run_guarded(champ_apply_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, "rx_session_champ_stage_apply", sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return call.rc;
}

/* the weekend's rider of a Player List name (added when new) */
static int32_t champ_rider(rx_champ_weekend* w, const char* key, const char* plate, int local) {
    int32_t i = champ_rider_by_key(w, key);
    if (i >= 0) { if (local) w->riders[i].local = 1; return i; }
    if (w->rider_count >= RX_CHAMP_MAX_RIDERS) return -1;
    rx_champ_rider* r = &w->riders[w->rider_count];
    memset(r, 0, sizeof *r);
    snprintf(r->key, sizeof r->key, "%s", key);
    snprintf(r->name, sizeof r->name, "%s", plate[0] ? plate : key);
    r->local = local;
    return w->rider_count++;
}

/* the overall order: points (more first), then the places from the last stage back (a place before none, a better one first) */
static int champ_before(const rx_champ_weekend* w, int32_t a, int32_t b) {
    const rx_champ_rider* x = &w->riders[a];
    const rx_champ_rider* y = &w->riders[b];
    if (w->judged) { if (x->total_score != y->total_score) return x->total_score > y->total_score; }
    else if (x->points != y->points) return x->points > y->points;
    for (int32_t s = w->stage_count - 1; s >= 0; s--) {
        const int32_t px = x->place[s] ? x->place[s] : 1000, py = y->place[s] ? y->place[s] : 1000;
        if (px != py) return px < py;
    }
    return 0;
}
static void champ_overall(rx_champ_weekend* w) {
    int32_t order[RX_CHAMP_MAX_RIDERS];
    for (int32_t i = 0; i < w->rider_count; i++) order[i] = i;
    for (int32_t i = 1; i < w->rider_count; i++) {
        const int32_t k = order[i];
        int32_t j = i - 1;
        while (j >= 0 && champ_before(w, k, order[j])) { order[j + 1] = order[j]; j--; }
        order[j + 1] = k;
    }
    for (int32_t i = 0; i < RX_CHAMP_MAX_RIDERS; i++) w->overall[i] = i < w->rider_count ? order[i] : -1;
}

/* after a stage: the next stages the local rider does not ride. An LCQ without the local rider is decided by the heat's order (the
 * original decides it by the table order when no local human is in it: 005adde0); a main event without the local rider ends the
 * weekend for them ("Failed" in the original: the local rider is eliminated). */
static void champ_advance(rx_champ_weekend* w) {
    w->stage++;
    int32_t local = -1;
    for (int32_t i = 0; i < w->rider_count; i++) if (w->riders[i].local) local = i;
    while (w->stage < w->stage_count && local >= 0 && !champ_in_field(w, w->stage, local)) {
        rx_champ_stage* st = &w->stages[w->stage];
        const int32_t heat = champ_stage_of(w, "HEAT");
        if (st->field == RX_CHAMP_FIELD_LCQ && heat >= 0) {
            int32_t place = 1;
            for (int32_t hp = 1; hp <= RX_CHAMP_MAX_RIDERS; hp++)
                for (int32_t r = 0; r < w->rider_count; r++)
                    if (w->riders[r].place[heat] == hp && champ_in_field(w, w->stage, r)) w->riders[r].place[w->stage] = place++;
            st->decided = 1;
            w->stage++;
            continue;
        }
        w->eliminated = 1;
        w->stage = w->stage_count;
    }
    if (w->stage >= w->stage_count) w->finished = 1;
    champ_overall(w);
}

/* TIMED RACES: driver x before driver y in a timed stage's places (see champ_record_run) */
static int champ_timed_before(rx_addr a, rx_addr x, rx_addr y) {
    const int32_t lx = rx_restored_champ_timed_finish_laps(a, x), ly = rx_restored_champ_timed_finish_laps(a, y);
    if ((lx >= 0) != (ly >= 0)) return lx >= 0;
    if (lx < 0) return 0;   /* both on track: the game's order */
    if (lx != ly) return lx > ly;
    return F32(x, 0x134) < F32(y, 0x134);   /* the race time, stopped at the finish */
}

static void champ_record_run(void* p) {
    struct champ_call* c = (struct champ_call*)p;
    rx_champ_weekend* w = c->w;
    const int32_t stage = w->stage;
    rx_champ_stage* st = &w->stages[stage];
    const rx_addr local = local_player();
    const rx_addr a = local ? U32(local, 0x94) : 0;
    if (!a || I32(a, 0x23c) != st->activity_type) { session_error(c->s, "championship stage: the stage's activity is not running"); c->rc = 2; return; }
    /* the places as the original's post-event code takes them (MXSeriesEventHeat::vf13 005adde0): the activity's sorted driver order
     * (+0x2d0, one per place; the drivers' own +0x78 can tie for riders still on track) */
    const uint32_t n = U32(a, 0x26c);
    /* TIMED RACES: a timed stage's places: the finished riders (+0x70) by laps completed (more first), then by their race time at the
     * flag (+0x134, the crossing; the laps those riders had at their finish, as they ride on); then the riders still on track in the game's order (the original's end-of-race check gives the AI
     * behind the last human its projected laps and gap, 00566ee0). A lap race keeps the game's order as it is. */
    enum { MAXD = 64 };
    rx_addr order[MAXD];
    uint32_t count = 0;
    for (uint32_t i = 0; i < n && i < MAXD && i < RX_RC_MAX; i++) { const rx_addr d = rx_stand(a, i); if (d) order[count++] = d; }   /* (w-rc-core) standings 12..31: rider_cap.h */
    if (champ_disciplines[w->discipline].timeable && w->stage_minutes[stage] > 0)
        for (uint32_t i = 1; i < count; i++) {   /* stable insertion sort */
            const rx_addr k = order[i];
            uint32_t j = i;
            while (j > 0 && champ_timed_before(a, k, order[j - 1])) { order[j] = order[j - 1]; j--; }
            order[j] = k;
        }
    if (c->retire)   /* the local rider left the stage before its end (a quit / retire): last, the others in their order */
        for (uint32_t i = 0; i + 1 < count; i++)
            if (U32(order[i], 0x74) == local) { const rx_addr me = order[i]; for (uint32_t j = i; j + 1 < count; j++) order[j] = order[j + 1]; order[count - 1] = me; break; }
    for (uint32_t i = 0; i < count; i++) {
        const rx_addr d = order[i];
        const rx_addr player = U32(d, 0x74);
        char key[32], plate[16];
        uint32_t k = 0;
        for (; k + 1 < sizeof key && U8(player, 0x14 + k); k++) key[k] = (char)U8(player, 0x14 + k);
        key[k] = 0;
        for (k = 0; k + 1 < sizeof plate && U8(player, 0x200 + k); k++) plate[k] = (char)U8(player, 0x200 + k);
        plate[k] = 0;
        const int32_t r = champ_rider(w, key, plate, player == local);
        if (r < 0) continue;
        const int32_t place = (int32_t)i + 1;
        w->riders[r].place[stage] = place;
        const float best = F32(d, waypoint_driver_layout(d) ? 0x10b0 : 0x10c4);   /* FLT_MAX until a lap is timed (rx_session_standings' fields) */
        w->riders[r].best_lap[stage] = best > 0.0f && best < 1.0e30f ? best : 0.0f;
        w->riders[r].gate[stage] = I32(player, 0x1dc) + 1;   /* the start slot it took (0054cd20 / 0055b610) */
        {   /* laps completed (rx_session_standings' field); a timed stage's at the rider's finish (it rides on after the flag) */
            const int32_t at_finish = rx_restored_champ_timed_finish_laps(a, d);
            w->rider_laps[r][stage] = at_finish >= 0 ? at_finish : I32(d, waypoint_driver_layout(d) ? 0xf10 : 0xf1c);
        }
        if (w->judged) {   /* the judged TotalScore ("%3.2f" in the original's results table, 00614bf0 reads P+0xcec) */
            const float score = F32(player, 0xcec);
            w->riders[r].score[stage] = score;
            if (st->scored) w->riders[r].total_score += score;
        } else if (st->scored) w->riders[r].points += rx_restored_champ_points(w->series_point_id, place);
    }
    st->raced = 1;
    champ_advance(w);
    c->rc = 0;
}

static int champ_record(struct rx_session* s, rx_champ_weekend* w, int retire) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || !champ_weekend_ok(w)) return 1;
    if (w->finished || w->stage >= w->stage_count) { session_error(s, "championship weekend: every stage was recorded"); return 2; }
    if (w->stages[w->stage].activity_type < 0) { session_error(s, "championship stage: not applied in this session"); return 2; }
    if (!retire && s->state != RX_SESSION_FINISHED) {   /* ended: the local rider finished, or the activity is in WindDown / Post */
        const rx_addr p = local_player(), a = p ? U32(p, 0x94) : 0;
        if (!a || I32(a, 0x21c) < 3) { session_error(s, "championship stage: the stage has not ended"); return 2; }
    }
    struct champ_call call = {s, w, 1, retire};
    char captured[4096];
    const int sig = run_guarded(champ_record_run, &call, captured, sizeof captured);
    if (sig) {
        char msg[512];
        describe_fault(msg, sizeof msg, retire ? "rx_session_champ_stage_retire" : "rx_session_champ_stage_record", sig, captured);
        session_error(s, "%s", msg);
        return 1;
    }
    return call.rc;
}
int rx_session_champ_stage_record(struct rx_session* s, rx_champ_weekend* w) { return champ_record(s, w, 0); }
int rx_session_champ_stage_retire(struct rx_session* s, rx_champ_weekend* w) { return champ_record(s, w, 1); }

int rx_session_champ_clock(struct rx_session* s, rx_champ_clock* out) {
    if (!session_ok(s) || !out || out->size != sizeof *out) return 1;
    rx_champ_clock v;
    memset(&v, 0, sizeof v);
    v.size = sizeof v;
    const rx_addr p = local_player();
    struct rx_restored_champ_clock c;
    if (rx_restored_champ_timed_state(p ? U32(p, 0x94) : 0, &c)) {
        v.timed = 1; v.seconds = c.seconds; v.elapsed = c.elapsed; v.expired = c.expired; v.flag = c.flag;
        v.extra_laps = c.extra; v.num_laps = c.num_laps; v.expired_at = c.expired_at; v.flag_at = c.flag_at;
    }
    *out = v;
    return 0;
}

