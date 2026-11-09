/* stop_audit_run.c -- runs one event through the PRODUCT session API and reports whether it stops (an unported function, an
 * act_unreached abort, a fault). The dynamic half of core/plan/STOP_AUDIT.md: the matrix of event types x locations x vehicle
 * classes (stop_audit_matrix.py) calls this once per combination.
 *   stop_audit_run <install root> <type> <typeId> <level> <locationId> <trackId> <vehicleClass> <duration> <durationType> <riders> <ai>
 *                  <activation|-> <load frames max> <race frames> [keys-held scan codes, comma separated | fuzz:<seed>]
 * env STOP_AUDIT_IN_CAREER_MODE=1 sets the UI property InCareerMode before the load (the Race & Ride challenges Free Ride).
 * env STOP_AUDIT_PROFILE=<profile cells file> imports a garage profile first (the player's vehicle: ATV / UTV / buggy / truck).
 * fuzz:<seed> presses the keyboard bindings of the game's default remap table (rx_remap_defaults: throttle held most of the time,
 * steering / trick modifier / reflex keys / camera / reset at random, every 20-80 frames) to drive crashes, resets and tricks.
 * env STOP_AUDIT_PULSE=<columns>:<period> (w-vtunported) taps the default keyboard bindings of those remap columns (rx_remap_defaults order:
 * 11 SetResetPoint = the Free Ride "save spot", 12 Reset, 13 UseResetPoint ...) for 3 frames every <period> race frames, on top of the
 * fuzz or held keys: e.g. STOP_AUDIT_PULSE=11,13:90 saves a spot and goes back to it all through a Free Ride.
 * env STOP_AUDIT_REPLAY=1 installs the exhibition replay presenter before the load (rx_session_replay_begin_event, as Unity's
 * ReplayWorker) and acts as its renderer: after every frame each copied request is applied and painted (ACK 1 then 2) in order,
 * and one REPLAY line summarises what the session asked for (viewport / fade requests, frames with the replay camera active).
 * With the development build (stop_audit_trace.sh) it also prints the activity state / Post sub-state and the viewport fields.
 * env STOP_AUDIT_LIFE=<n> runs the whole life of the event (development hooks needed): the activity's own state is watched; <n> frames
 * after it first enters RACING (state 2) it is forced into WindDown (state 3; n < 0 = never forced: a natural finish), and the run goes on
 * until STOP_AUDIT_POST_EXTRA (default 900) frames after Post (state 4) was entered (or the race frame cap). One LIFE line summarises racing / post / forced frames.
 * The post-race presentation (rx_session_podium) is watched every frame: one PODIUM line per change of the playing post scenario / step / drawn
 * podium riders / the local rider's and vehicle's drawn flags, and a PODIUM summary (the steps seen, frames each scenario played: intro 3 / finish 1 / results 2).
 * env STOP_AUDIT_TEARDOWN=1 runs the live activity's teardown (vf3, vf6(0), vf0(1) of the activity: rxtrace_teardown) before the session
 * is destroyed. After rx_session_destroy the driver prints ENDED (a stop in the session end shows as no ENDED line).
 * env STOP_AUDIT_FREE_PLAYERS=1 (with STOP_AUDIT_TEARDOWN=1) then frees every player slot as the next event's player sync does (rxtrace_players_free:
 * 00552c60 -> MXPlayer vf0 -> the vehicle's teardown, its damage parts destroyed), with TEARDOWN-DMG lines before / after; STOP_AUDIT_DMG_ACTIVATE=1
 * activates every other part first (0047a630, rxtrace_dmg_activate); STOP_AUDIT_DRAIN=1 then ends that frame as the original's Engine::Frame does
 * (rxtrace_frame_tail: the counters and Database vf33, the end-of-frame removal of the queued handles), with a third TEARDOWN-DMG line.
 * env STOP_AUDIT_FINISH=<n> (with STOP_AUDIT_LIFE=-1): <n> frames after RACING the local human finishes (the oracle's finish stimulus; w-wpfill).
 * env STOP_AUDIT_RESULTS=1: RESULTS lines: the Post results sub-state's first frame and the results page table each time it is filled (w-resultsrng).
 * env STOP_AUDIT_COUNTERS=<n>: COUNTERS lines (the engine's frame counters, the pending removal lists, the model handles) after frames.
 * env STOP_AUDIT_POSE=<n> (w-freerideall): POSE lines for the local vehicle (rx_session_bike: body 0's position, the heading of the model's
 * root node, speed, the wheels on the ground, wrecked) at the first race frame (SPAWN) and every <n> frames, the activity state as it changes,
 * RESETS (a jump of the body by more than 6 ft in one frame) and one POSE summary (frames with a wheel down, airborne / wrecked frames, the
 * lowest / highest y, NaN frames, the resets).
 * env STOP_AUDIT_RESTART=<n>[,<n>...]: the pause menu's RESTART (rx_session_restart_activity = App.RestartActivity) at those race frames (one line each:
 * RESTART frame <n> -> <rc>, the activity state before and after it); the run then goes on, so a path of the restart that is not ported shows as a stop.
 * Prints one line:  RESULT <ok|stop> state=<n> frames=<n> game_time=<s> [error="..."]
 * Links against librxcore.dylib: clang -std=c11 -O1 -I core/include stop_audit_run.c -L<dir> -lrxcore -Wl,-rpath,<dir> */
#include "rx_api.h"
#include "rx_replay_api.h"
#include "rx_podium_api.h"
#include "../game/rx_activation_api.h"
#include "../game/rx_challenge_api.h"
#include "rx_results.h"
#include "rx_rng.h"
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dlfcn.h>
#include <math.h>

static int dirdump_done;
/* env STOP_AUDIT_RESULTS=1 (w-resultsrng): the frame the activity's Post sub-state 2 (the results) is first seen, and every change of the
 * results page table (rx_session_results_page: the core's App.PopulateResultsTable where the original's page starts) with its rows and
 * the game LCG's draw count (rx_session_rng). */
/* the standings as rx_session_standings writes them (the activity's order: a results page numbers its rows by it) with each driver's place
 * +0x78 (w-resultsplace: after InProgress the places repeat, the order does not) */
static void results_standings(struct rx_session* s, const char* when, int frame) {
    rx_driver_result d[16]; const int n = rx_session_standings(s, d, 16);
    for (int i = 0; i < n; i++)
        fprintf(stderr, "RESULTS standings %s frame %d: %2d slot %2d%s place %2d laps %d %s time %8.3f best %8.3f\n", when, frame, i + 1, d[i].slot,
                d[i].local ? " (local)" : "        ", d[i].place, d[i].laps_completed, d[i].finished ? "finished" : "racing  ", (double)d[i].race_time, (double)d[i].best_lap);
}
static void results_trace(struct rx_session* s, int frame) {
    static uint32_t last_pop = 0; static int post2 = -1;
    rx_event_ui_state u; memset(&u, 0, sizeof u);
    if (rx_session_event_ui_state(s, &u) == 0 && u.race_state == 4 && u.post_state == 2 && post2 < 0) {
        post2 = frame; fprintf(stderr, "RESULTS post2 at frame %d\n", frame);
        results_standings(s, "post2", frame);
    }
    rx_rng_state g; memset(&g, 0, sizeof g); g.size = sizeof g; g.version = RX_RNG_VERSION;
    (void)rx_session_rng(s, &g);
    rx_results_page p; memset(&p, 0, sizeof p); p.size = sizeof p; p.version = RX_RESULTS_VERSION;
    if (rx_session_results_page(s, &p) != 0 || p.populated == last_pop) return;
    last_pop = p.populated;
    fprintf(stderr, "RESULTS page populated %u at frame %d (post2 + %d) type %d columns %d rows %d lcg seed %08x count %u titles [%s] [%s] [%s]\n", p.populated, frame,
            post2 >= 0 ? frame - post2 : -1, p.activity_type, p.columns, p.rows, g.game_seed, g.game_count, p.title[0], p.title[1], p.title[2]);
    for (int i = 0; i < p.rows; i++)
        fprintf(stderr, "RESULTS row %2d slot %2d%s %-20s %-12s %-12s %-12s sort %g %g %g\n", p.row[i].index, p.row[i].slot, p.row[i].local ? " (local)" : "", p.row[i].name,
                p.row[i].cell[0], p.row[i].cell[1], p.row[i].cell[2], (double)p.row[i].sort[0], (double)p.row[i].sort[1], (double)p.row[i].sort[2]);
    results_standings(s, "page", frame);
}
/* STOP_AUDIT_POSE (w-freerideall): the local vehicle's pose, its ground contact and the activity state through the run */
static struct { int on, period, frames, ground, air, wrecked, nan, resets, last_state, have_last, reset_at, reset_ok, reset_pending; float miny, maxy, maxd, last[3], spawn[3]; } pz;
static void pose_trace(struct rx_session* s, int frame, int spawn) {
    rx_bike_state b; memset(&b, 0, sizeof b);
    rx_onboard_state o; memset(&o, 0, sizeof o);
    rx_event_ui_state u; memset(&u, 0, sizeof u);
    if (rx_session_event_ui_state(s, &u) == 0 && u.race_state != pz.last_state) {
        fprintf(stderr, "POSE state %d (activity type %d) at frame %d\n", u.race_state, u.activity_type, frame);
        pz.last_state = u.race_state;
    }
    if (rx_session_bike(s, &b) != 0) return;
    (void)rx_session_onboard_state(s, &o);
    float m[18 * 16]; const int nn = rx_session_bike_pose(s, m, 18);
    const float x = b.position[0][0], y = b.position[0][1], z = b.position[0][2];
    const float hx = nn > 0 ? m[8] : 0.f, hz = nn > 0 ? m[10] : 0.f;   /* the root node's third row (its forward axis), xz */
    const int grounded = o.contacts_valid && (o.front_grounded || o.rear_grounded);
    pz.frames++;
    if (x != x || y != y || z != z) { pz.nan++; return; }
    if (grounded) pz.ground++; else pz.air++;
    if (o.wrecked) pz.wrecked++;
    if (spawn) { pz.spawn[0] = x; pz.spawn[1] = y; pz.spawn[2] = z; }
    { const float sx = x - pz.spawn[0], sz = z - pz.spawn[2], d = sx * sx + sz * sz; if (d > pz.maxd) pz.maxd = d; }
    if (pz.reset_pending && grounded && !o.wrecked) { pz.reset_ok++; pz.reset_pending = 0; }   /* back on its wheels after a reset */
    if (pz.reset_pending && frame - pz.reset_at > 180) pz.reset_pending = 0;
    if (pz.frames == 1 || y < pz.miny) pz.miny = y;
    if (pz.frames == 1 || y > pz.maxy) pz.maxy = y;
    if (pz.have_last) {
        const float dx = x - pz.last[0], dy = y - pz.last[1], dz = z - pz.last[2];
        if (dx * dx + dy * dy + dz * dz > 36.f) { pz.resets++; pz.reset_at = frame; pz.reset_pending = 1; fprintf(stderr, "POSE reset at frame %d: %.1f %.1f %.1f -> %.1f %.1f %.1f\n", frame, pz.last[0], pz.last[1], pz.last[2], x, y, z); }
    }
    pz.last[0] = x; pz.last[1] = y; pz.last[2] = z; pz.have_last = 1;
    if (spawn || (pz.period > 0 && frame % pz.period == 0))
        fprintf(stderr, "POSE %s frame %d pos %.2f %.2f %.2f heading %.3f %.3f speed %.1f wheels %d%d%s%s\n", spawn ? "SPAWN" : "at", frame, x, y, z, hx, hz,
                b.speed, o.front_grounded, o.rear_grounded, o.contacts_valid ? "" : " (no contacts)", o.wrecked ? " wrecked" : "");
}
static void on_alarm(int sig) { (void)sig; const char m[] = "RESULT stop state=-1 frames=0 game_time=0 error=\"watchdog\"\n"; (void)write(1, m, sizeof m - 1); _exit(3); }

/* development build (w-frametail): the handles of a type's table (rxtrace_type_handles), -1 without the hook */
static int type_handles(const char* type) {
    int (*f)(const char*) = (int (*)(const char*))dlsym(RTLD_DEFAULT, "rxtrace_type_handles");
    return f ? f(type) : -1;
}
/* development build: the damage manager 0xdb1410 (12 entries of 0x410 bytes: +0x3fc the vehicle, +0 the part count, +0x2c the part entities, +0x50 the
 * part shapes) and the event world's keyed objects (the parts' bodies are records there) and the database's removal lists (+0x64 / +0x7c) */
static void dmg_report(const char* when, uint32_t (*peek)(uint32_t)) {
    const uint32_t g = peek(0x00a946b8), lvl = g ? peek(g + 0x5a54) : 0, pw = lvl ? peek(lvl + 0x20134) : 0, db = peek(peek(0x00a95844) + 8);
    unsigned entries = 0, parts = 0, in_event = 0, broken = 0, active = 0;
    for (uint32_t k = 0; k < 12; k++) {
        const uint32_t e = 0x00db1410u + 0x10 + 0x410 * k;
        if (peek(e + 0x3fc)) entries++;
        for (uint32_t i = 0; i < peek(e) && i < 8; i++) {
            const uint32_t ent = peek(e + 0x2c + 4 * i);
            if (!ent) continue;
            parts++;
            if (peek(e + 0xc + 4 * i) != 0) broken++;                                 /* the part's state (1 / 2 / 3: loose, hanging, off) */
            if ((peek(e + 0x3d0 + (i & ~3u)) >> (8 * (i & 3))) & 0xff) active++;       /* the part's active flag (a body of its own in the scene) */
            if (peek(ent + 0x158) && peek(peek(ent + 0x158) + 4) == pw) in_event++;   /* the part's body is a record of the event world */
        }
    }
    fprintf(stderr, "TEARDOWN-DMG %s: entries with a vehicle %u, part entities %u (bodies in the event world %u, broken %u, active %u), keyed records %u, database removals queued %u / %u, model handles %d\n", when, entries, parts, in_event, broken, active,
            pw ? (peek(pw + 0x19a4) - peek(pw + 0x19a0)) / 0xe0 : 0, (peek(db + 0x74) - peek(db + 0x70)) / 4, (peek(db + 0x8c) - peek(db + 0x88)) / 4, type_handles("model"));
}

/* env STOP_AUDIT_COUNTERS=<n> (development build): after each of the first n frames, every 300th and every frame whose pending lists or model
 * handle count changed: the engine's render / update counters ([[0xa95844] + 0x10] + 0x54 / + 0x58), the Database's two pending removal
 * lists (+0x64 / +0x7c) and the "model" handles (w-frametail: Engine::Frame's tail in the product's frame) */
static void counters_trace(int frames, uint32_t (*peek)(uint32_t)) {
    static int every = -2; static uint32_t last = 0xffffffffu; static int last_models = -2;
    if (every == -2) every = getenv("STOP_AUDIT_COUNTERS") ? atoi(getenv("STOP_AUDIT_COUNTERS")) : -1;
    if (every < 0 || !peek) return;
    const uint32_t ctx = peek(0x00a95844), eng = ctx ? peek(ctx + 0x10) : 0, db = ctx ? peek(ctx + 8) : 0;
    if (!eng || !db) return;
    const uint32_t p0 = (peek(db + 0x74) - peek(db + 0x70)) / 4, p1 = (peek(db + 0x8c) - peek(db + 0x88)) / 4;
    const int models = type_handles("model");
    if (frames <= every || frames % 300 == 0 || (p0 | (p1 << 16)) != last || models != last_models)
        fprintf(stderr, "COUNTERS frame %d render %u update %u pending %u / %u model handles %d\n", frames, peek(eng + 0x54), peek(eng + 0x58), p0, p1, models);
    last = p0 | (p1 << 16); last_models = models;
}

int main(int argc, char** argv) {
    if (argc < 15) { fprintf(stderr, "usage: see the file header\n"); return 2; }
    signal(SIGALRM, on_alarm);
    alarm(900);
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("RESULT stop state=-1 frames=0 game_time=0 error=\"create: %s\"\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("STOP_AUDIT_PROFILE")) {   /* a garage profile (rx_profile_cell rows, e.g. builds/sol_core_features/vehicle-profiles/atv.bin): the player's own vehicle */
        FILE* pf = fopen(getenv("STOP_AUDIT_PROFILE"), "rb");
        if (!pf) { printf("RESULT stop state=-1 frames=0 game_time=0 error=\"profile file\"\n"); return 1; }
        fseek(pf, 0, SEEK_END); long pb = ftell(pf); rewind(pf);
        rx_profile_cell* cells = malloc((size_t)pb);
        if (!cells || fread(cells, 1, (size_t)pb, pf) != (size_t)pb) return 1;
        fclose(pf);
        if (rx_session_profile_import(s, cells, (int)(pb / (long)sizeof *cells)) != 0) { printf("RESULT stop state=-1 frames=0 game_time=0 error=\"profile import: %s\"\n", rx_session_last_error(s)); return 1; }
        free(cells);
    }
    /* STOP_AUDIT_IN_CAREER_MODE=1: the event runs with the UI property InCareerMode (the Race & Ride challenges Free Ride and its
     * challenges: rx_session_event_in_career_mode, as Unity's OwnActivationArea worlds) */
    if (getenv("STOP_AUDIT_IN_CAREER_MODE") && strcmp(getenv("STOP_AUDIT_IN_CAREER_MODE"), "1") == 0 && rx_session_event_in_career_mode(s, 1) != 0) {
        printf("RESULT stop state=-1 frames=0 game_time=0 error=\"in_career_mode: %s\"\n", rx_session_last_error(s)); return 1;
    }
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]);
    e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]);
    e.location_id = atoi(argv[5]);
    e.track_id = atoi(argv[6]);
    e.vehicle_class = atoi(argv[7]);
    e.duration = atoi(argv[8]);
    snprintf(e.duration_type, sizeof e.duration_type, "%s", argv[9]);
    e.riders = atoi(argv[10]);
    e.ai_difficulty = atoi(argv[11]);
    e.event_activation[0] = 0;
    if (strcmp(argv[12], "-") != 0) snprintf(e.event_activation, sizeof e.event_activation, "%s", argv[12]);
    int load_max = atoi(argv[13]), race = atoi(argv[14]);
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    if (argc > 15) { char* p = strdup(argv[15]); for (char* t = strtok(p, ","); t; t = strtok(NULL, ",")) keys[atoi(t) & 255] = 0x80; }
    int fuzz = 0; unsigned seed = 1; rx_remap_table remap; memset(&remap, 0, sizeof remap);
    /* a remap column's keyboard binding is a native key id; rx_session_set_input takes DIK scan codes: scan[DIK] = id (rx_remap_defaults).
     * (Before w-vtunported the fuzz pressed DIK = key id: other keys than the columns it names.) */
    int dik_of[256]; memset(dik_of, 0, sizeof dik_of);
#define KB(col) (dik_of[remap.keyboard[(col)] & 255] & 255)
    if (argc > 15 && strncmp(argv[15], "fuzz:", 5) == 0) { fuzz = 1; seed = (unsigned)atoi(argv[15] + 5); rx_remap_defaults(&remap); }
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    int pulse_cols[8], npulse = 0, pulse_period = 0, pulses = 0;     /* STOP_AUDIT_PULSE=11,13:90 */
    if (getenv("STOP_AUDIT_PULSE")) {
        char pl[128]; snprintf(pl, sizeof pl, "%s", getenv("STOP_AUDIT_PULSE"));
        char* colon = strchr(pl, ':');
        if (colon) { *colon = 0; pulse_period = atoi(colon + 1); }
        for (char* tk = strtok(pl, ","); tk && npulse < 8; tk = strtok(NULL, ",")) pulse_cols[npulse++] = atoi(tk);
        if (!fuzz) { rx_remap_defaults(&remap); for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d; }
        if (pulse_period <= 0) npulse = 0;
    }
    const char* trick = getenv("RXTRACE_TRICK");   /* development build only: start this aerial trick (e.g. 9OClockNac_L_Air) whenever the local rider is airborne */
    int (*try_trick)(const char*) = trick ? (int (*)(const char*))dlsym(RTLD_DEFAULT, "rxtrace_try_trick") : NULL;
    int tricks = 0;
    int camlist_buf[16], ncam = 0; const int* camlist = NULL;   /* STOP_AUDIT_CAMERAS=0,1,2,3: the driving-camera presets cycled every 400 race frames */
    if (getenv("STOP_AUDIT_CAMERAS")) { char cl[128]; snprintf(cl, sizeof cl, "%s", getenv("STOP_AUDIT_CAMERAS")); for (char* tk = strtok(cl, ","); tk && ncam < 16; tk = strtok(NULL, ",")) camlist_buf[ncam++] = atoi(tk); camlist = camlist_buf; }
    int (*set_state)(int) = getenv("RXTRACE_STATES") ? (int (*)(int))dlsym(RTLD_DEFAULT, "rxtrace_set_state") : NULL;   /* "1500:3,1800:4": at race frame N the activity enters state S (development build only) */
    int st_frame[16], st_state[16], nst = 0;
    if (set_state) { char sl[128]; snprintf(sl, sizeof sl, "%s", getenv("RXTRACE_STATES")); for (char* tk = strtok(sl, ","); tk && nst < 16; tk = strtok(NULL, ",")) { sscanf(tk, "%d:%d", &st_frame[nst], &st_state[nst]); nst++; } }
    /* the exhibition replay presenter (STOP_AUDIT_REPLAY=1): before the load, as the Unity worker does */
    const int replay = getenv("STOP_AUDIT_REPLAY") && strcmp(getenv("STOP_AUDIT_REPLAY"), "1") == 0;
    uint64_t replay_generation = 0;
    unsigned replay_viewports = 0, replay_fades_in = 0, replay_fades_out = 0, replay_active_frames = 0, replay_camera_frames = 0;
    int replay_first_active = -1, last_view = -1;
    if (replay) {
        const int brc = rx_session_replay_begin_event(s, &replay_generation);
        fprintf(stderr, "REPLAY begin_event -> %d generation %llu\n", brc, (unsigned long long)replay_generation);
        if (brc != 0) { printf("RESULT stop state=-1 frames=0 game_time=0 error=\"replay begin_event %d\"\n", brc); return 1; }
    }
    int (*set_state_life)(int) = (int (*)(int))dlsym(RTLD_DEFAULT, "rxtrace_set_state");
    uint32_t (*peek)(uint32_t) = (uint32_t (*)(uint32_t))dlsym(RTLD_DEFAULT, "rxtrace_peek");          /* development build only */
    uint32_t (*activity)(void) = (uint32_t (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_activity");
    int last_activity_state = -2, last_post = -2, last_replay_mode = -2;
    int podium_last = -1, podium_intro_frames = 0, podium_finish_frames = 0, podium_results_frames = 0, podium_drawn_max = 0; uint32_t podium_steps = 0;
    int last_area_active = -1, activations_seen = 0; char last_area[24] = "";   /* the activation-area snapshot a host polls (STOP_AUDIT_IN_CAREER_MODE) */
    const int life = getenv("STOP_AUDIT_LIFE") != NULL, life_force_after = life ? atoi(getenv("STOP_AUDIT_LIFE")) : 0;
    const int life_post_extra = getenv("STOP_AUDIT_POST_EXTRA") ? atoi(getenv("STOP_AUDIT_POST_EXTRA")) : 900;   /* frames run after Post was entered */
    int life_racing = -1, life_post = -1, life_forced = -1, life_wind = -1;
    int rc = rx_session_event_load(s, &e);
    int frames = 0;
    if (rc == 0) {
        while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < load_max) { if (rx_session_advance(s) != 0) break; frames++; counters_trace(frames, peek); }
        if (getenv("RXTRACE_POKE")) {   /* "d5a9a4=0,..." game-memory words written once the load is done (development build only) */
            void (*poke)(unsigned, unsigned) = (void (*)(unsigned, unsigned))dlsym(RTLD_DEFAULT, "rxtrace_poke");
            char pl[256]; snprintf(pl, sizeof pl, "%s", getenv("RXTRACE_POKE"));
            for (char* tk = strtok(pl, ","); tk && poke; tk = strtok(NULL, ",")) { unsigned a, v; if (sscanf(tk, "%x=%x", &a, &v) == 2) poke(a, v); }
        }
        if (argc > 15 && !fuzz) rx_session_set_input(s, keys, NULL, 0);
        int hold = 0;
        if (getenv("STOP_AUDIT_POSE")) { pz.on = 1; pz.period = atoi(getenv("STOP_AUDIT_POSE")); pz.last_state = -9; }
        for (int i = 0; i < race && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
            if (life && life_post >= 0 && frames >= life_post + life_post_extra) break;
            if (fuzz && hold-- <= 0) {
                hold = 15 + (int)(rand_r(&seed) % 40);
                memset(keys, 0, sizeof keys);
                if (rand_r(&seed) % 10 != 0) keys[KB(0)] = 0x80;                 /* throttle */
                if (rand_r(&seed) % 12 == 0) keys[KB(1)] = 0x80;                 /* brake */
                { unsigned r = rand_r(&seed) % 4; if (r == 0) keys[KB(2)] = 0x80; else if (r == 1) keys[KB(3)] = 0x80; }
                { int trick = rand_r(&seed) % 2 == 0;
                  if (trick) keys[KB(4)] = 0x80;                                  /* trick modifier */
                  int any = 0;
                  for (int k = 6; k < 10; k++) if (rand_r(&seed) % 4 == 0) { keys[KB(k)] = 0x80; any = 1; }   /* reflex left / right / up / down */
                  if (trick && !any) keys[KB(6 + (int)(rand_r(&seed) % 4))] = 0x80; }
                if (rand_r(&seed) % 40 == 0) keys[KB(10)] = 0x80;                /* camera */
                if (rand_r(&seed) % 60 == 0) keys[KB(12)] = 0x80;                /* reset */
                rx_session_set_input(s, keys, NULL, 0);
            }
            if (npulse) {          /* the tapped columns: down for 3 frames of every period, the column's own key (one at a time, in turn) */
                const int ph = i % pulse_period, col = pulse_cols[(i / pulse_period) % npulse];
                for (int k = 0; k < npulse; k++) keys[KB(pulse_cols[k])] = 0;
                if (ph < 3) { keys[KB(col)] = 0x80; if (ph == 0) pulses++; }
                rx_session_set_input(s, keys, NULL, 0);
            }
            if (rx_session_advance(s) != 0) break;
            frames++;
            counters_trace(frames, peek);
            if (getenv("STOP_AUDIT_RESTART")) {
                char rl[128]; snprintf(rl, sizeof rl, "%s", getenv("STOP_AUDIT_RESTART"));
                for (char* tk = strtok(rl, ","); tk; tk = strtok(NULL, ","))
                    if (atoi(tk) == i + 1) {
                        rx_event_ui_state ru; memset(&ru, 0, sizeof ru);
                        const int before = rx_session_event_ui_state(s, &ru) == 0 ? ru.race_state : -1;
                        const int rrc = rx_session_restart_activity(s);
                        const int urc = rx_session_event_ui_state(s, &ru);
                        fprintf(stderr, "RESTART frame %d -> %d (state %d, after %d)\n", i + 1, rrc, before, urc == 0 ? ru.race_state : -1);
                    }
            }
            if (pz.on) pose_trace(s, frames, i == 0);
            if (getenv("STOP_AUDIT_RESULTS")) results_trace(s, frames);
            if (getenv("STOP_AUDIT_IN_CAREER_MODE")) {   /* the host's per-frame poll of the activation areas (Unity OwnActivationArea) */
                rx_activation_state as; memset(&as, 0, sizeof as); as.size = sizeof as; as.version = RX_ACTIVATION_API_VERSION;
                if (rx_session_activation(s, &as) == 0 && ((int)as.active != last_area_active || strcmp(as.activity, last_area) != 0)) {
                    fprintf(stderr, "ACTIVATION area %s active %u track %d required_class %d at frame %d\n", as.activity, as.active, as.track_id, as.required_class, frames);
                    last_area_active = (int)as.active; snprintf(last_area, sizeof last_area, "%s", as.activity); activations_seen++;
                }
            }
            if (replay) {   /* the renderer's side: every copied request of this frame applied, then painted, in order */
                rx_replay_view v; memset(&v, 0, sizeof v); v.size = sizeof v; v.version = RX_REPLAY_API_VERSION;
                const int vrc = rx_session_replay_view(s, replay_generation, &v);
                if (vrc != 0) { fprintf(stderr, "REPLAY view -> %d at frame %d\n", vrc, frames); break; }
                if (v.active) { replay_active_frames++; if (v.camera_valid) replay_camera_frames++; if (replay_first_active < 0) replay_first_active = frames; }
                if ((int)(v.active * 2 + v.camera_valid) != last_view) {
                    fprintf(stderr, "REPLAY view active %u camera %u at frame %d\n", v.active, v.camera_valid, frames);
                    last_view = (int)(v.active * 2 + v.camera_valid);
                }
                for (uint32_t q = 0; q < v.pending_requests; q++) {
                    rx_replay_request r; memset(&r, 0, sizeof r); r.size = sizeof r; r.version = RX_REPLAY_API_VERSION;
                    if (rx_session_replay_peek(s, replay_generation, q, &r) != 0) { fprintf(stderr, "REPLAY peek failed\n"); break; }
                    if (r.operation == RX_REPLAY_VIEWPORT) replay_viewports++;
                    else if (r.fade_kind == 2) replay_fades_in++;
                    else replay_fades_out++;
                    fprintf(stderr, "REPLAY request id %llu frame %llu op %u count %u fade %u at frame %d\n", (unsigned long long)r.id,
                            (unsigned long long)r.frame, r.operation, r.viewport_count, r.fade_kind, frames);
                }
                for (;;) {
                    rx_replay_request r; memset(&r, 0, sizeof r); r.size = sizeof r; r.version = RX_REPLAY_API_VERSION;
                    if (rx_session_replay_next(s, replay_generation, &r) != 0) break;
                    const int a1 = rx_session_replay_ack(s, replay_generation, &r, RX_REPLAY_APPLIED);
                    const int a2 = rx_session_replay_ack(s, replay_generation, &r, RX_REPLAY_PAINTED);
                    if (a1 != 0 || a2 != 0) { fprintf(stderr, "REPLAY ack %d / %d\n", a1, a2); break; }
                }
            }
            {   /* the post-race presentation (public API) */
                rx_podium_info pi; rx_podium_rider pr[16]; rx_podium_slot ps[16];
                static float pm[16 * 69 * 16];
                if (rx_session_podium(s, &pi, pr, 16, pm, 16 * 69, ps, 16) == 0) {
                    int drawn = 0, local_rider = 1, local_vehicle = 1;
                    for (int q = 0; q < pi.riders; q++) if (pr[q].visible && pr[q].placed && pr[q].bones == 69) drawn++;
                    rx_slot sl[16]; const int ns = rx_session_slots(s, sl, 16);
                    for (int q = 0; q < ns; q++) if (sl[q].local) for (int k = 0; k < pi.slots; k++) if (ps[k].slot == sl[q].slot) { local_rider = ps[k].rider_drawn; local_vehicle = ps[k].vehicle_drawn; }
                    const int key = pi.scenario * 100000 + pi.step * 1000 + drawn * 100 + local_rider * 10 + local_vehicle;
                    if (key != podium_last) {
                        rx_camera cam; const int crc = rx_session_camera(s, &cam);
                        fprintf(stderr, "PODIUM scenario %d step %d post %d riders %d drawn %d local rider %d vehicle %d camera %d (%.1f %.1f %.1f fov %.3f) at frame %d\n",
                                pi.scenario, pi.step, pi.post_state, pi.riders, drawn, local_rider, local_vehicle, crc, cam.position[0], cam.position[1], cam.position[2], cam.fov, frames);
                        podium_last = key;
                        if (getenv("STOP_AUDIT_PODIUM_DETAIL"))
                            for (int q = 0; q < pi.riders; q++)
                                fprintf(stderr, "PODIUM   rider %08x slot %d visible %d placed %d bones %d root %.1f %.1f %.1f draw %.0f mud %.3f color %.2f %.2f %.2f\n", pr[q].object, pr[q].slot, pr[q].visible, pr[q].placed,
                                        pr[q].bones, pr[q].root[12], pr[q].root[13], pr[q].root[14], pr[q].draw_distance, pr[q].mud_amount[0], pr[q].mud_color[0], pr[q].mud_color[1], pr[q].mud_color[2]);
                        if (getenv("STOP_AUDIT_PODIUM_DETAIL"))
                            for (int q = 0; q < pi.slots; q++)
                                fprintf(stderr, "PODIUM   live slot %d rider drawn %d vehicle drawn %d\n", ps[q].slot, ps[q].rider_drawn, ps[q].vehicle_drawn);
                    }
                    if (getenv("STOP_AUDIT_SCENARIO_ACTORS") && frames % atoi(getenv("STOP_AUDIT_SCENARIO_ACTORS")) == 0) {
                        /* the scenario actors (ScenarioAnimEntity, ambient kind 1: the Free Ride arrival's hovercraft / sky crane / box car)
                         * drawn this frame: model, visible, entity position, first bone */
                        static rx_ambient_entity ae[1024]; static float am[16 * 8192]; rx_ambient_info ai;
                        const int na = rx_session_ambient_entities(s, ae, 1024, am, 8192, &ai);
                        for (int q = 0; q < na; q++) if (ae[q].kind == 1 && (ae[q].flags & 1u)) {
                            const float* b = ae[q].bones > 0 && ae[q].first_matrix + ae[q].bones <= ai.matrices ? am + 16 * ae[q].first_matrix : NULL;
                            uint8_t nf[256]; const int nn = rx_session_ambient_node_flags(s, ae[q].object_id, nf, 256); int hidden = 0;
                            for (int k = 0; k < nn && k < 256; k++) hidden += (nf[k] & 0x10) != 0;
                            fprintf(stderr, "ACTOR frame %d scenario %d step %d %s #%d bones %d hidden nodes %d at %.1f %.1f %.1f bone0 %.1f %.1f %.1f\n", frames, pi.scenario, pi.step, ae[q].model,
                                    ae[q].entity_id, ae[q].bones, hidden, ae[q].matrix[12], ae[q].matrix[13], ae[q].matrix[14], b ? b[12] : 0.f, b ? b[13] : 0.f, b ? b[14] : 0.f);
                        }
                    }
                    if (getenv("STOP_AUDIT_SCENARIO_ACTORS")) {   /* the scenario scripts' sound effects (v3) */
                        rx_scenario_sound snd[16]; const int nsnd = rx_session_scenario_sounds(s, snd, 16);
                        for (int q = 0; q < nsnd; q++) fprintf(stderr, "SOUND frame %d scenario %d step %d %s%s\n", frames, pi.scenario, pi.step, snd[q].event, snd[q].has_position ? " (3D)" : "");
                    }
                    if (getenv("STOP_AUDIT_SCENARIO_ACTORS")) {
                        /* the static scenery models whose drawn flag / instance matrix changed since the first frame they were seen (v3) */
                        static rx_scenario_model sm[4096]; static struct { int32_t id, drawn; float x, y, z; } first[4096]; static int nfirst;
                        const int nm = rx_session_scenario_models(s, sm, 4096);
                        for (int q = 0; q < nm && q < 4096; q++) {
                            int k = 0; while (k < nfirst && first[k].id != sm[q].entity_id) k++;
                            if (k == nfirst) { if (nfirst == 4096) continue; first[k].id = sm[q].entity_id; first[k].drawn = sm[q].drawn; first[k].x = sm[q].matrix[12]; first[k].y = sm[q].matrix[13]; first[k].z = sm[q].matrix[14]; nfirst++; continue; }
                            if (first[k].drawn != sm[q].drawn || first[k].x != sm[q].matrix[12] || first[k].y != sm[q].matrix[13] || first[k].z != sm[q].matrix[14]) {
                                fprintf(stderr, "MODEL frame %d scenario %d step %d #%d drawn %d -> %d at %.1f %.1f %.1f -> %.1f %.1f %.1f\n", frames, pi.scenario, pi.step, sm[q].entity_id,
                                        first[k].drawn, sm[q].drawn, first[k].x, first[k].y, first[k].z, sm[q].matrix[12], sm[q].matrix[13], sm[q].matrix[14]);
                                first[k].drawn = sm[q].drawn; first[k].x = sm[q].matrix[12]; first[k].y = sm[q].matrix[13]; first[k].z = sm[q].matrix[14];
                            }
                        }
                    }
                    if (pi.scenario == 3) podium_intro_frames++;
                    if (pi.scenario == 1) podium_finish_frames++;
                    if (pi.scenario == 2) podium_results_frames++;
                    if (pi.scenario && pi.step > 0 && pi.step < 16) podium_steps |= 1u << pi.step;
                    if (drawn > podium_drawn_max) podium_drawn_max = drawn;
                }
            }
            if (peek && activity) {   /* development build: the activity state / Post sub-state / replay mode as they change */
                const uint32_t a = activity();
                const int ast = a ? (int)peek(a + 0x21c) : -1;
                const int post = a ? (int)peek(a + (peek(a + 0x23c) == 6 ? 0x13e618u : 0x13e640u)) : -1;
                const int mode = (int)peek(0x00a942b4);
                /* STOP_AUDIT_FINISH_CLIP=1 (development): when the activity enters WindDown, the first recorded clip slot becomes the finish
                 * clip (type 9), as a natural finish records it, so the finish scenario's replay step 7 plays after a forced WindDown */
                if (getenv("STOP_AUDIT_FINISH_CLIP") && ast == 3 && last_activity_state != 3) {
                    void (*poke)(unsigned, unsigned) = (void (*)(unsigned, unsigned))dlsym(RTLD_DEFAULT, "rxtrace_poke");
                    for (uint32_t k = 0; k < 12 && poke; k++) {
                        const uint32_t slot = 0x00d323c8u + 0x35d0u * k;
                        if (peek(slot) != 0) { fprintf(stderr, "FINISH_CLIP slot %u type %u -> 9 at frame %d\n", k, peek(slot), frames); poke(slot, 9); break; }
                    }
                }
                /* STOP_AUDIT_CAMLOG=1 (development): the post scenario's animated camera (world + 0x218f0: +0x2b0 posed, Camera +0x90) */
                if (getenv("STOP_AUDIT_CAMLOG") && ast == 4 && frames % 30 == 0 && a) {
                    const uint32_t w = peek(a + 0x234), cam = w + 0x218f0u + 0x90u;
                    union { uint32_t u; float f; } c[8];
                    const uint32_t off[8] = {0x10, 0x14, 0x18, 0x40, 0x44, 0x48, 0x50, 0};
                    for (int q = 0; q < 7; q++) c[q].u = peek(cam + off[q]);
                    rx_camera rc; const int crc = rx_session_camera(s, &rc);
                    { const uint32_t ctl = peek(w + 0x21de0u), pl = ctl ? peek(ctl + 0x190u) : 0;
                      fprintf(stderr, "CAMLOG frame %d replay: controller player %08x flag21c %u directors %08x tv %u timer %08x view %08x\n", frames, pl, pl ? (peek(pl + 0x21cu) & 0xffu) : 0u,
                              peek(w + 0x21dd8u), peek(w + 0x21de4u) & 0xffu, peek(w + 0x21e00u), peek(peek(peek(w + 0x20138u) + 0x19b8u))); }
                    {   /* the current TV director (a replay camera entity, loop_cam_replay.c): its class (vtable), its Camera's position and the view's */
                        const uint32_t dir = peek(w + 0x21dfcu), view = peek(peek(w + 0x20138u) + 0x19b8u), vcam = view ? peek(view) : 0;
                        union { uint32_t u; float f; } d3[3], v3[3];
                        for (int q = 0; q < 3; q++) { d3[q].u = dir ? peek(dir + 0x90u + 0x10u + 4u * (uint32_t)q) : 0; v3[q].u = vcam ? peek(vcam + 0x10u + 4u * (uint32_t)q) : 0; }
                        if (frames % 300 == 0) {   /* the director list (world +0x21dd8, links +0x310): the replay camera classes the scene factory built */
                            int nt = 0, nz = 0, ns = 0, na = 0, nother = 0, n = 0;
                            for (uint32_t d = peek(w + 0x21dd8u); d && n < 256; d = peek(d + 0x310u), n++) {
                                const uint32_t vt = peek(d);
                                if (vt == 0x0095ab04u) nt++; else if (vt == 0x0095abacu) nz++; else if (vt == 0x0095ac54u) ns++; else if (vt == 0x0095acfcu) na++; else nother++;
                            }
                            fprintf(stderr, "CAMLOG frame %d director list: %d entries, tower %d zip line %d spline %d animated %d other %d\n", frames, n, nt, nz, ns, na, nother);
                        }
                        fprintf(stderr, "CAMLOG frame %d director class %08x camera %.1f %.1f %.1f | view camera %.1f %.1f %.1f | session camera %d (%.1f %.1f %.1f)\n", frames, dir ? peek(dir) : 0,
                                d3[0].f, d3[1].f, d3[2].f, v3[0].f, v3[1].f, v3[2].f, crc, rc.position[0], rc.position[1], rc.position[2]);
                    }
                    fprintf(stderr, "CAMLOG frame %d controller %08x director %08x post %d posed %u anim pos %.1f %.1f %.1f fwd %.2f %.2f %.2f fov %.3f | session %d pos %.1f %.1f %.1f fwd %.2f %.2f %.2f\n",
                            frames, peek(w + 0x21de0u), peek(w + 0x21dfcu), post, peek(w + 0x218f0u + 0x2b0u) & 0xffu, c[0].f, c[1].f, c[2].f, c[3].f, c[4].f, c[5].f, c[6].f,
                            crc, rc.position[0], rc.position[1], rc.position[2], rc.forward[0], rc.forward[1], rc.forward[2]);
                }
                /* STOP_AUDIT_DIRDUMP=<file> (development): the replay cameras of the world's director list (+0x21dd8, links +0x310) once the race
                 * runs, as words: "DIR <class vtable> <entity id> <size> <w0 w1 ...>" and for each activation list "AREA <count> <words>"
                 * (core/tools/cam_cmp.py compares them with the original's world image) */
                if (getenv("STOP_AUDIT_DIRDUMP") && ast == 2 && !dirdump_done && a) {
                    dirdump_done = 1;
                    FILE* df = fopen(getenv("STOP_AUDIT_DIRDUMP"), "w");
                    const uint32_t w = peek(a + 0x234);
                    int n = 0;
                    for (uint32_t d = peek(w + 0x21dd8u); df && d && n < 256; d = peek(d + 0x310u), n++) {
                        const uint32_t vt = peek(d), size = vt == 0x0095ab04u ? 0x430u : vt == 0x0095ac54u ? 0x4b0u : 0x450u;
                        fprintf(df, "DIR %08x %u %u", vt, peek(d + 0x14u), size);
                        for (uint32_t o = 0; o < size; o += 4) fprintf(df, " %08x", peek(d + o));
                        fprintf(df, "\n");
                        const uint32_t lo = vt == 0x0095acfcu ? 0x3e4u : 0x3e0u, cnt = peek(d + lo + 4u), data = peek(d + lo);
                        fprintf(df, "AREA %u", cnt);
                        for (uint32_t o = 0; o < cnt * 4u && data; o++) fprintf(df, " %08x", peek(data + 4u * o));
                        fprintf(df, "\n");
                    }
                    if (df) fclose(df);
                    fprintf(stderr, "DIRDUMP %d directors at frame %d\n", n, frames);
                }
                if (life) {
                    if (ast == 2 && life_racing < 0) life_racing = frames;
                    if (ast == 3 && life_wind < 0) life_wind = frames;
                    if (ast == 4 && life_post < 0) life_post = frames;
                    if (life_racing >= 0 && life_force_after >= 0 && life_forced < 0 && ast == 2 && frames >= life_racing + life_force_after && set_state_life) {
                        life_forced = frames; set_state_life(3);
                    }
                    /* STOP_AUDIT_FINISH=<n> (w-wpfill): <n> frames after RACING the local human finishes as the oracle's RX_RNG_FINISH_STIM
                     * does (a Waypoint race: its laps done D+0xf10 = A+0xff090; a race: D+0xf1c = A+0xff070), so the activity's own
                     * end-of-race check ends the event (development build only) */
                    static int finish_done = 0;
                    void (*poke_f)(unsigned, unsigned) = (void (*)(unsigned, unsigned))dlsym(RTLD_DEFAULT, "rxtrace_poke");
                    uint32_t (*act_f)(void) = (uint32_t (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_activity");
                    if (getenv("STOP_AUDIT_FINISH") && !finish_done && life_racing >= 0 && ast == 2 && peek && poke_f && act_f &&
                        frames >= life_racing + atoi(getenv("STOP_AUDIT_FINISH"))) {
                        const uint32_t A = act_f(), n = A ? peek(A + 0x26c) : 0, list = A ? peek(A + 0x268) : 0;
                        for (uint32_t k = 0; k < n && k < 16; k++) {
                            const uint32_t D = peek(list + 4 * k), P = D ? peek(D + 0x74) : 0;
                            if (!P || peek(P + 0x10) != 0) continue;
                            if (peek(A) == 0x00943c14) poke_f(D + 0xf10, peek(A + 0xff090)); else poke_f(D + 0xf1c, peek(A + 0xff070));
                            fprintf(stderr, "FINISH driver %08x at frame %d\n", D, frames);
                            break;
                        }
                        finish_done = 1;
                    }
                }
                if (ast != last_activity_state || post != last_post || mode != last_replay_mode) {
                    const uint32_t g = peek(0x00a946b8);
                    const uint32_t timing = peek(0x00a95844) ? peek(peek(0x00a95844) + 0x10) : 0;
                    fprintf(stderr, "ACTIVITY state %d post %d replay mode %d clip %d at frame %d (game viewports %u revision %u/%u split %u)\n", ast, post, mode,
                            (int)peek(0x009c4070), frames, g ? peek(g + 0x15b24) : 0, g ? peek(g + 0x15b2c) : 0, g ? peek(g + 0x14) : 0, timing ? peek(timing + 0x5c) : 0);
                    last_activity_state = ast; last_post = post; last_replay_mode = mode;
                }
            }
            if (peek && getenv("STOP_AUDIT_DMG") && frames % 300 == 0) {   /* development build: the vehicle damage manager 0xdb1410 (12 entries of 0x410 bytes): vehicle class, part states, part entities */
                for (uint32_t k = 0; k < 12; k++) {
                    const uint32_t e = 0x00db1410u + 0x10 + 0x410 * k, v = peek(e + 0x3fc);
                    if (!v) continue;
                    fprintf(stderr, "DMG frame %d entry %u class %u parts %u states", frames, k, peek(v + 0x194), peek(e));
                    for (uint32_t i = 0; i < peek(e) && i < 8; i++) fprintf(stderr, " %u", peek(e + 0xc + 4 * i));
                    fprintf(stderr, " entities");
                    for (uint32_t i = 0; i < peek(e) && i < 8; i++) fprintf(stderr, " %08x", peek(e + 0x2c + 4 * i));
                    fprintf(stderr, "\n");
                }
            }
            for (int q = 0; q < nst; q++) if (frames == st_frame[q]) { int r = set_state(st_state[q]); fprintf(stderr, "STATE %d requested at frame %d -> %d\n", st_state[q], frames, r); }
            if (camlist && frames % 400 == 0) { int pr = camlist[(frames / 400) % ncam]; int cr = rx_session_set_driving_camera(s, pr); fprintf(stderr, "CAMERA preset %d at frame %d -> %d\n", pr, frames, cr); }
            if (try_trick) {   /* a comma list: the next name at each start */
                char list[8192]; snprintf(list, sizeof list, "%s", trick);
                char* names[256]; int nn = 0;
                for (char* tk = strtok(list, ","); tk && nn < 256; tk = strtok(NULL, ",")) names[nn++] = tk;
                if (nn && try_trick(names[tricks % nn]) == 1) { fprintf(stderr, "TRICK %s started at frame %d\n", names[tricks % nn], frames); tricks++; }
            }
        }
    }
    if (getenv("RXTRACE_CENSUS") && getenv("RXTRACE_VTLIST")) {   /* development build of librxcore with rxtrace_census (see stop_audit_matrix.py --trace) */
        void (*census)(const char*, const char*) = (void (*)(const char*, const char*))dlsym(RTLD_DEFAULT, "rxtrace_census");
        if (census) census(getenv("RXTRACE_VTLIST"), getenv("RXTRACE_CENSUS"));
    }
    { rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
      if (rx_session_vehicle_info(s, -1, &vi) == 0) fprintf(stderr, "LOCAL vehicle factory type %d (0 MX, 1 ATV, 3 TrophyTruck, 5 Buggy, 7 UTV) bodies %d\n", vi.type, vi.bodies); }
    { char st[96] = ""; size_t o = 0; for (int q = 1; q < 16; q++) if (podium_steps & (1u << q)) o += (size_t)snprintf(st + o, sizeof st - o, "%s%d", o ? "," : "", q);
      fprintf(stderr, "PODIUM summary intro_frames %d finish_frames %d results_frames %d steps %s riders_drawn_max %d\n", podium_intro_frames, podium_finish_frames, podium_results_frames, o ? st : "-", podium_drawn_max); }
    if (life) fprintf(stderr, "LIFE racing=%d wind=%d post=%d forced=%d frames=%d\n", life_racing, life_wind, life_post, life_forced, frames);
    if (replay) {
        fprintf(stderr, "REPLAY summary viewports %u fade_in %u fade_out %u active_frames %u camera_frames %u first_active %d\n",
                replay_viewports, replay_fades_in, replay_fades_out, replay_active_frames, replay_camera_frames, replay_first_active);
        fprintf(stderr, "REPLAY end_event -> %d\n", rx_session_replay_end_event(s, replay_generation));
    }
    if (getenv("STOP_AUDIT_TEARDOWN") && strcmp(getenv("STOP_AUDIT_TEARDOWN"), "1") == 0) {
        int (*teardown)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_teardown");
        if (teardown) fprintf(stderr, "TEARDOWN -> %d\n", teardown()); else fprintf(stderr, "TEARDOWN hook missing\n");
        if (getenv("STOP_AUDIT_FREE_PLAYERS")) {   /* development build: the players leave their slots (the next event's player sync): their vehicles' teardown */
            int (*players_free)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_players_free");
            if (getenv("STOP_AUDIT_DMG_ACTIVATE")) {   /* every other part activated first (0047a630): the teardown's active-part path */
                int (*activate)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_dmg_activate");
                if (activate) fprintf(stderr, "DMG parts activated %d\n", activate());
            }
            if (peek) dmg_report("before", peek);
            if (players_free) fprintf(stderr, "PLAYERS freed %d\n", players_free()); else fprintf(stderr, "PLAYERS hook missing\n");
            if (peek) dmg_report("after", peek);
            if (getenv("STOP_AUDIT_DRAIN")) {   /* development build (w-frametail): the frame the player sync ran in ends (Engine::Frame's tail: vf33) */
                int (*tail)(void) = (int (*)(void))dlsym(RTLD_DEFAULT, "rxtrace_frame_tail");
                if (tail) fprintf(stderr, "DRAIN frame tail: %d handles left pending\n", tail()); else fprintf(stderr, "DRAIN hook missing\n");
                if (peek) dmg_report("after the frame tail", peek);
            }
        }
    }
    int st = (int)rx_session_get_state(s);
    if (pz.on) fprintf(stderr, "POSE summary frames %d ground %d air %d wrecked %d nan %d resets %d grounded_after_reset %d y %.1f .. %.1f farthest %.0f ft\n", pz.frames, pz.ground,
                       pz.air, pz.wrecked, pz.nan, pz.resets, pz.reset_ok, pz.miny, pz.maxy, sqrtf(pz.maxd));
    if (npulse) fprintf(stderr, "PULSE %d taps (columns %s, every %d frames)\n", pulses, getenv("STOP_AUDIT_PULSE"), pulse_period);
    if (rc != 0 || st == RX_SESSION_ERROR)
        printf("RESULT stop state=%d frames=%d game_time=%.3f error=\"%s\"\n", st, frames, rx_session_game_time(s), rx_session_last_error(s) ? rx_session_last_error(s) : "?");
    else printf("RESULT ok state=%d frames=%d game_time=%.3f\n", st, frames, rx_session_game_time(s));
    rx_session_destroy(s);
    printf("ENDED\n");
    return 0;
}
