/* practice_test.c (w-lb-practice) -- headless checks of PRACTICE, the LAP LOG and the local vehicle's physics class (core/plan/LEADERBOARDS.md
 * 2.1, 3.3, package C; game/practice.h, rx_api.h rx_session_set_practice / rx_session_lap_log / rx_session_local_phys_class).
 *
 *   practice_test <install root> practice [laps (10)]
 *       National NAT_Concord, one rider (AI 0), practice on; pad 0 rides the race-line autopilot of career_series_test.c. Once the gate lap
 *       is logged it scripts two bad laps: in the 3rd lap the RESET key (the keyboard's Reset column of rx_remap_defaults) 12 s in for 3
 *       frames (with the game's own debug gate of 0054e470 opened for them: an exhibition race takes the key only while the rider is down),
 *       in the 5th lap the stick hard right with the throttle held for 10 s, 10 s in (off the track). It runs until `laps` laps are logged and checks:
 *         - num_laps == 0x7fffffff and, once racing, the race state stays 2 (InProgress) and the session never finishes;
 *         - serials 1, 2, ... in order with no gap; flying 0 for the gate lap, 1 for every later one;
 *         - |exact - time| <= the frame (1/60 s, or 1/30 with PT_JITTER) for every lap, and sum(exact) = sum(time) within one frame + 1 ms
 *           (the last crossing's remainder);
 *         - the 3rd lap has resets >= 1; the 5th lap is flagged (off_track > 2 s, a penalty, or a reset);
 *         - rx_session_local_phys_class is a valid class and equals the class PHYSICS STUDIO's tracker names for the same event (the finish
 *           run below, armed);
 *         - mods == 0 on every lap (nothing non-original set).
 *       PT_POKE=1: after the gate lap the driver's lap counter (D+0xf1c) is set to 94, so the counter reaches its cap 97 within the run: the
 *       checks then also need laps_completed == 97 at the end while the serials go on (a test-only write of game memory; without it the cap
 *       is reached only after 97 real laps: `practice 100` rides them, about 10 minutes). PT_JITTER=1: every frame gets a random real dt in 1/60 .. 1/30 s (rx_session_advance_dt).
 *   practice_test <install root> finish [laps (2)]
 *       the same event without practice (and PHYSICS STUDIO armed with no config, to read its tracker's class): the race must finish after its
 *       laps, the log holds every counted crossing, and the tracker's class is printed (PT_CLASS_OUT=<file> writes it for the practice run).
 *   practice_test <install root> markers
 *       (w-lb-markers) SPAWN MARKERS in PRACTICE, the MX Bikes rule. The same practice event; about 2.3 laps:
 *         the gate lap: 85 s in (PT_MARK_AT) a marker is kept (rx_session_spawn_pose: the spot about 13 s before the line on NAT_Concord);
 *         lap 2 (the first flying lap): 20 s in, rx_session_spawn_at sends the rider back to the marker (answer 0; a second call while
 *         the respawn runs answers 3); the vehicle is at the marker once the reset ran; he rides on to the line;
 *         lap 3: 30 s in a marker is kept again (placing only); the lap is ridden to the line.
 *       Checks: lap 2 is logged ONCE, at the real crossing after the jump (no crossing at the jump itself), with markers 1, flying 0,
 *       resets 0 (the respawn is the marker's, not a reset) and a short time (20 s + the respawn + the ride from the marker); the lap
 *       clock restarts at that crossing; the game's own best lap does not take lap 2; lap 3 is a clean flying lap (markers 0, flying 1,
 *       resets 0, no penalty, off track <= 2 s, |exact - time| <= one frame, its time = the race clock between its crossings) and becomes
 *       the game's best lap; the rules never reset the rider after the jump (the driver was placed at the marker on the track).
 *       PT_INSTANT=1 (w-markerbtns): the respawn is rx_session_spawn_at_instant (the client's SPAWN MARKER): the same checks (the "again"
 *       call is skipped: nothing runs afterwards to refuse it).
 *   practice_test <install root> scale <type> <type id> <level> <location id> <map scale> <the track's default> <laps>
 *       (w-alivegates) BASE SCALE: a practice on any level (an Alive National at 1.1 / 1.1 and 1.2 / 1.1 ...): rx_session_map_scale_records
 *       is 1 exactly when the scale is the track's default, and the lap log's map scale bit (16) is clear on every lap there, set on every
 *       lap at any other scale (cmd_scale).
 * Exit 0 = PASS. Links against the product objects (tools/practice_test.sh). */
#include "rx_api.h"
#include "rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail;
static void check(int ok, const char* what) { printf("%-4s %s\n", ok ? "ok" : "FAIL", what); if (!ok) g_fail = 1; }

/* ---- the race-line autopilot (career_series_test.c 101-140) ------------------------------------------------------------------------- */
static float wrap_angle(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }
struct autopilot { float stuck_clock, reverse_clock; };
static void autopilot_pad(struct rx_session* s, struct autopilot* pilot, rx_pad* pad) {
    rx_bike_state bike; float line[64 * 3];
    const int points = rx_session_bike(s, &bike) == 0 ? rx_session_race_line(s, line, 64) : 0;
    if (points < 2) return;
    const float x = bike.position[0][0], z = bike.position[0][2];
    int nearest = 0; float best = 1e30f;
    for (int i = 0; i < points && i < 6; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z, d = dx * dx + dz * dz; if (d < best) { best = d; nearest = i; } }
    const float look = 18.0f + bike.speed * 0.6f;
    int target = nearest;
    for (int i = nearest; i < points; i++) { const float dx = line[3 * i] - x, dz = line[3 * i + 2] - z; target = i; if (dx * dx + dz * dz >= look * look) break; }
    const float tx = line[3 * target] - x, tz = line[3 * target + 2] - z;
    const float* q = bike.orientation[0];
    const float fx = 2.0f * (q[0] * q[2] + q[3] * q[1]), fz = 1.0f - 2.0f * (q[0] * q[0] + q[1] * q[1]);
    const float error = wrap_angle(atan2f(tx, tz) - atan2f(fx, fz));
    float steer = error * 2.2f; if (steer > 1.0f) steer = 1.0f; if (steer < -1.0f) steer = -1.0f;
    const float speed = bike.speed, turn = fabsf(error);
    const float desired = turn > 1.2f ? 8.0f : turn > 0.6f ? 14.0f : turn > 0.3f ? 20.0f : 30.0f;
    if (speed < 1.0f) pilot->stuck_clock += 1.0f / 60.0f; else pilot->stuck_clock = 0.0f;
    if (pilot->stuck_clock > 4.0f) { pilot->reverse_clock = 1.5f; pilot->stuck_clock = 0.0f; }
    if (pilot->reverse_clock > 0.0f) { pilot->reverse_clock -= 1.0f / 60.0f; pad->left_trigger = 255; pad->lx = (int16_t)(-steer * 32000.0f); }
    else {
        pad->lx = (int16_t)(steer * 32000.0f);
        if (speed < desired) pad->right_trigger = 255; else if (speed > desired + 6.0f) pad->left_trigger = 160; else pad->right_trigger = 120;
    }
}

/* the local human's race driver, as rx_session.c finds it (local_player: G = [0x00a946b8], the slots G+0x153ec; race_driver_for) */
static rx_addr local_driver(void) {
    const rx_addr g = U32(0, 0x00a946b8u);
    if (!g) return 0;
    for (uint32_t i = 0; i < U8(g, 0x1541c) && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) != 0 || U32(p, 4) == 0) continue;
        const rx_addr a = U32(p, 0x94), list = a ? U32(a, 0x268) : 0;
        for (uint32_t k = 0; list && k < U32(a, 0x26c) && k < 64; k++) if (U32(U32(list, 4u * k), 0x74) == p) return U32(list, 4u * k);
    }
    return 0;
}

static int load(struct rx_session* s, int practice, int laps) {
    rx_event_spec e; rx_session_event_default(&e);   /* National NAT_Concord */
    e.riders = 1; e.ai_difficulty = 0; e.duration = laps; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    if (rx_session_set_practice(s, practice) != 0) { printf("FAIL set_practice: %s\n", rx_session_last_error(s)); return 1; }
    if (rx_session_event_load(s, &e) != 0) { printf("FAIL load: %s\n", rx_session_last_error(s)); return 1; }
    /* set_practice after the load is refused (wrong state) */
    check(rx_session_set_practice(s, 1) == 2, "rx_session_set_practice after the load is refused (2)");
    check(rx_session_practice(s) == practice, "rx_session_practice reports the flag");
    return 0;
}

static void print_lap(const rx_lap_record* r) {
    printf("  lap #%u flying %d time %.4f exact %.4f (d %+.4f) penalty %.2f off %.2f race %.2f resets %u wrong %d mods %u\n", r->serial, r->flying,
           (double)r->time, (double)r->exact, (double)(r->exact - r->time), (double)r->penalty, (double)r->off_track, (double)r->race_time, r->resets,
           r->wrong_way, r->mods);
}

static int cmd_practice(const char* root, int want) {
    if (rx_session_sizeof("rx_lap_record") != (int32_t)sizeof(rx_lap_record)) { printf("FAIL rx_session_sizeof(rx_lap_record)\n"); return 1; }
    rx_session* s = rx_session_create(root);
    if (!s) { printf("FAIL create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (load(s, 1, 3)) return 1;
    const int poke = getenv("PT_POKE") && atoi(getenv("PT_POKE")), jitter = getenv("PT_JITTER") && atoi(getenv("PT_JITTER"));
    rx_remap_table remap; memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    int dik_of[256]; memset(dik_of, 0, sizeof dik_of);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    const int reset_key = dik_of[remap.keyboard[12] & 255] & 255;
    struct autopilot pilot = {0, 0};
    rx_lap_record laps[512]; int n = 0;
    uint32_t seen = 0;
    int racing_from = -1, left_racing = 0, finished = 0, poked = 0, phys_class = -2, manual_reset_frames = 0, unstick_frames = 0;
    unsigned seed = 7;
    double lap_start_time = 0; float max_dt = 0.0f;
    rx_race_state race; memset(&race, 0, sizeof race);
    const long budget = 60L * 60 * 4 * want + 20000;   /* 4 game minutes a lap at most */
    for (long f = 0; f < budget && n < want; f++) {
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        rx_pad pad; memset(&pad, 0, sizeof pad);
        const int racing = racing_from >= 0;
        const double in_lap = race.race_time - lap_start_time;   /* seconds into the open lap (the race clock) */
        const int lap_now = n + 1;
        if (racing) {
            if (lap_now == 5 && in_lap > 10.0 && in_lap < 20.0) { pad.lx = 32000; pad.right_trigger = 255; }   /* off the track */
            else autopilot_pad(s, &pilot, &pad);
            /* the 3rd lap: the RESET key (Player_HandleResetInput 0054e4f0 -> the manual reset 00550400) for 3 frames 12 s in. In an
             * exhibition race 0054e470 takes the key only in the crash timer's window (the original's Practice, mode 2, any time), so the
             * test opens the game's own debug gate [0x00aec048] (0054e470: allowed whenever it is set) for those frames: a test-only write */
            /* a long run's autopilot can get stuck for good: past 200 s in one lap the RESET key (the same gate) every 10 s puts it back */
            const int unstick = in_lap > 200.0 && fmod(in_lap, 10.0) < 0.06;
            if (unstick) unstick_frames++;
            if (lap_now == 3 && in_lap > 12.0 && in_lap < 12.06) { keys[reset_key] = 0x80; SU32(0, 0x00aec048u, 1); manual_reset_frames++; }
            else if (unstick) { keys[reset_key] = 0x80; SU32(0, 0x00aec048u, 1); }
            else if (U32(0, 0x00aec048u)) SU32(0, 0x00aec048u, 0);
        }
        rx_session_set_input(s, keys, &pad, 1);
        float dt = 0.0f;
        if (jitter) dt = (1.0f + (float)(rand_r(&seed) % 1000) / 1000.0f) / 60.0f;
        if ((jitter ? rx_session_advance_dt(s, dt) : rx_session_advance(s)) != 0) { printf("FAIL advance: %s\n", rx_session_last_error(s)); return 1; }
        const float fdt = rx_session_frame_dt(s);
        if (racing && fdt > max_dt) max_dt = fdt;
        rx_session_race(s, &race);
        const rx_session_state st = rx_session_get_state(s);
        if (racing_from < 0 && race.valid && race.state == 2) { racing_from = (int)f; printf("racing from frame %ld, num_laps 0x%x\n", f, (unsigned)race.num_laps); }
        if (racing && race.state != 2) left_racing++;
        if (st == RX_SESSION_FINISHED) finished++;
        if (phys_class == -2 && racing) phys_class = rx_session_local_phys_class(s);
        rx_lap_record got[8];
        const int k = rx_session_lap_log(s, seen, got, 8);
        if (k < 0) { printf("FAIL rx_session_lap_log -1\n"); return 1; }
        for (int i = 0; i < k && n < (int)(sizeof laps / sizeof laps[0]); i++) {
            laps[n++] = got[i]; seen = got[i].serial; lap_start_time = got[i].race_time;
            print_lap(&got[i]);
            printf("     laps_completed %d (frame %ld)\n", race.laps_completed, f);
        }
        if (poke && !poked && n >= 1) {   /* the counter near its cap (test only: see the header) */
            const rx_addr d = local_driver();
            if (d) { printf("PT_POKE: the lap counter D+0xf1c %d -> 94\n", I32(d, 0xf1c)); SU32(d, 0xf1c, 94); }
            poked = 1;
        }
    }
    printf("frames to the %d-th lap; the largest frame dt %.4f\n", n, (double)max_dt);
    check(n >= want, "the laps were logged within the frame budget");
    check(race.num_laps == 0x7fffffff, "num_laps == 0x7fffffff (the original's Practice rule)");
    check(left_racing == 0, "once racing the race state stayed 2");
    check(finished == 0, "the session never finished");
    int order = 1, flying = 1;
    for (int i = 0; i < n; i++) { if (laps[i].serial != (uint32_t)(i + 1)) order = 0; if (laps[i].flying != (i > 0)) flying = 0; }
    check(order, "serials 1 .. n in order, no gap");
    check(flying, "flying 0 for the gate lap, 1 for every later lap");
    const float frame = (jitter ? 1.0f / 30.0f : 1.0f / 60.0f) + 1e-4f;
    double sum_t = 0, sum_e = 0; int within = 1;
    for (int i = 0; i < n; i++) { sum_t += laps[i].time; sum_e += laps[i].exact; if (fabsf(laps[i].exact - laps[i].time) > frame) within = 0; }
    printf("sum time %.4f, sum exact %.4f (difference %.4f: the last crossing's remainder)\n", sum_t, sum_e, sum_t - sum_e);
    check(within, "|exact - time| within one frame on every lap");
    check(fabs(sum_t - sum_e) <= frame + 1e-3, "sum(exact) = sum(time) within one frame + 1 ms");
    int moved = 0; for (int i = 0; i < n; i++) if (laps[i].exact != laps[i].time) moved++;
    check(moved > 0, "the sub-frame correction moved the times (exact != time on some lap)");
    printf("the RESET key was held %d frames in the 3rd lap\n", manual_reset_frames);
    if (unstick_frames) printf("the autopilot was stuck: the RESET key put it back (%d frames)\n", unstick_frames);
    if (n >= 3) check(laps[2].resets >= 1, "the 3rd lap (the RESET key) has resets >= 1");
    if (n >= 5) check(laps[4].off_track > 2.0f || laps[4].penalty > 0.0f || laps[4].resets > 0, "the 5th lap (off the track) is flagged: off_track > 2 s, a penalty or a reset");
    int mods = 0; for (int i = 0; i < n; i++) mods |= (int)laps[i].mods;
    check(mods == 0, "mods 0 on every lap (nothing non-original set)");
    printf("rx_session_local_phys_class %d\n", phys_class);
    check(phys_class >= 0 && phys_class <= 8, "rx_session_local_phys_class is a class (0 .. 8)");
    const char* cls_file = getenv("PT_CLASS_IN");
    if (cls_file) {
        FILE* fp = fopen(cls_file, "r"); int armed = -9;
        if (fp) { if (fscanf(fp, "%d", &armed) != 1) armed = -9; fclose(fp); }
        printf("the PHYSICS STUDIO tracker's class of the same event: %d\n", armed);
        check(armed == phys_class, "the unarmed class equals the PHYSICS STUDIO tracker's (armed) class");
    }
    if (poke || n > 97) check(race.laps_completed == 97 && laps[n - 1].serial == (uint32_t)n && n > (poke ? 97 - 94 : 97), "laps_completed stopped at 97 while the serials went on");
    const int failed = g_fail;   /* (destroy restores the image's statics: this driver's too) */
    rx_session_destroy(s);
    printf("%s\n", failed ? "PRACTICE FAIL" : "PRACTICE PASS");
    return failed;
}

static int cmd_finish(const char* root, int want) {
    rx_session* s = rx_session_create(root);
    if (!s) { printf("FAIL create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_session_phys_arm_live(s, 1);   /* the tracker only (no config: nothing is applied) */
    if (load(s, 0, want)) return 1;
    struct autopilot pilot = {0, 0};
    rx_lap_record laps[64]; int n = 0; uint32_t seen = 0;
    rx_race_state race; memset(&race, 0, sizeof race);
    int racing = 0, done = 0, phys_class = -2;
    for (long f = 0; f < 60L * 60 * 4 * (want + 1) + 20000 && !done; f++) {
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        rx_pad pad; memset(&pad, 0, sizeof pad);
        if (racing) autopilot_pad(s, &pilot, &pad);
        rx_session_set_input(s, keys, &pad, 1);
        if (rx_session_advance(s) != 0) { printf("FAIL advance: %s\n", rx_session_last_error(s)); return 1; }
        rx_session_race(s, &race);
        if (race.valid && race.state == 2) racing = 1;
        if (racing && phys_class == -2) {
            phys_class = rx_session_local_phys_class(s);
            /* w-lb-markers: without practice a race refuses the SPAWN MARKER (2), leaving the rider alone */
            float pos[3], dir[3];
            if (rx_session_spawn_pose(s, pos, dir) == 0) check(rx_session_spawn_at(s, pos, dir) == 2, "without practice rx_session_spawn_at answers 2 (a race)");
        }
        rx_lap_record got[8];
        const int k = rx_session_lap_log(s, seen, got, 8);
        for (int i = 0; i < k && n < 64; i++) { laps[n++] = got[i]; seen = got[i].serial; print_lap(&got[i]); }
        if (rx_session_get_state(s) == RX_SESSION_FINISHED) { done = 1; printf("finished at frame %ld: laps_completed %d of %d\n", f, race.laps_completed, race.num_laps); }
    }
    check(done, "without practice the race finishes");
    check(race.num_laps == want, "num_laps is the event's");
    check(n >= want && n <= want + 1, "the log holds the race's counted crossings");
    /* the PHYSICS STUDIO tracker's class of the local vehicle (rx_session_phys_live_values: "L <k> <class id> <sim> <local>") */
    uint32_t need = 0; rx_session_phys_live_values(s, NULL, 0, &need);
    char* t = malloc(need ? need : 1); t[0] = 0; rx_session_phys_live_values(s, t, need, &need);
    static const char* ids[9] = {"mx125", "mx250", "mx250f", "mx450f", "atv", "utv", "buggy", "truck2wd", "truck2wdlite"};
    int armed = -1;
    for (char* line = strtok(t, "\n"); line; line = strtok(NULL, "\n")) {
        char cls[32]; int k2, local; unsigned sim;
        if (sscanf(line, "L\t%d\t%31s\t%x\t%d", &k2, cls, &sim, &local) == 4 && local) for (int c = 0; c < 9; c++) if (!strcmp(cls, ids[c])) armed = c;
    }
    free(t);
    printf("rx_session_local_phys_class %d, the PHYSICS STUDIO tracker's local class %d\n", phys_class, armed);
    check(armed >= 0 && armed == phys_class, "rx_session_local_phys_class equals the tracker's class (armed)");
    if (getenv("PT_CLASS_OUT")) { FILE* fp = fopen(getenv("PT_CLASS_OUT"), "w"); if (fp) { fprintf(fp, "%d\n", armed); fclose(fp); } }
    const int failed = g_fail;   /* (destroy restores the image's statics: this driver's too) */
    rx_session_destroy(s);
    printf("%s\n", failed ? "FINISH FAIL" : "FINISH PASS");
    return failed;
}

/* ---- SPAWN MARKERS in PRACTICE (w-lb-markers) ---------------------------------------------------------------------------------------- */
static int cmd_markers(const char* root) {
    if (rx_session_sizeof("rx_lap_record") != (int32_t)sizeof(rx_lap_record)) { printf("FAIL rx_session_sizeof(rx_lap_record)\n"); return 1; }
    rx_session* s = rx_session_create(root);
    if (!s) { printf("FAIL create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (load(s, 1, 3)) return 1;
    struct autopilot pilot = {0, 0};
    rx_lap_record laps[16]; int n = 0; uint32_t seen = 0;
    rx_race_state race; memset(&race, 0, sizeof race);
    int racing = 0, kept = 0, jumped = 0, placed_checked = 0, kept3 = 0, again = -9, jump_rc = -9;
    const int instant = getenv("PT_INSTANT") != NULL;
    float marker_pos[3] = {0, 0, 0}, marker_dir[3] = {0, 0, 1}, jump_in_lap = 0, best_after_2 = -1, lap_time_after_2 = -1, best_after_3 = -1;
    long jump_frame = -1, settled_frame = -1;
    double lap_start = 0;
    const double mark_at = getenv("PT_MARK_AT") ? atof(getenv("PT_MARK_AT")) : 85.0;   /* seconds into the gate lap the marker is kept */
    int resets_seen_after_jump = 0, was_resetting = 0;
    for (long f = 0; f < 60L * 60 * 12 && n < 3; f++) {
        uint8_t keys[256]; memset(keys, 0, sizeof keys);
        rx_pad pad; memset(&pad, 0, sizeof pad);
        const double in_lap = race.race_time - lap_start;
        if (racing && (jump_frame < 0 || race.reset_timer <= 0.0f)) autopilot_pad(s, &pilot, &pad);
        rx_session_set_input(s, keys, &pad, 1);
        if (rx_session_advance(s) != 0) { printf("FAIL advance: %s\n", rx_session_last_error(s)); return 1; }
        rx_session_race(s, &race);
        if (!racing && race.valid && race.state == 2) { racing = 1; printf("racing from frame %ld\n", f); }
        if (!racing) continue;
        /* the gate lap: keep a marker about 15 s before the line */
        if (n == 0 && !kept && in_lap >= mark_at) {
            if (rx_session_spawn_pose(s, marker_pos, marker_dir) == 0) {
                kept = 1;
                printf("marker kept in the gate lap %.1f s in (frame %ld): (%.1f, %.1f, %.1f) heading (%.2f, %.2f)\n", in_lap, f,
                       (double)marker_pos[0], (double)marker_pos[1], (double)marker_pos[2], (double)marker_dir[0], (double)marker_dir[2]);
            }
        }
        /* lap 2, 20 s in: back to the marker */
        if (n == 1 && kept && !jumped && in_lap >= 20.0) {
            /* PT_INSTANT=1 (w-markerbtns): the client's INSTANT respawn (rx_session_spawn_at_instant): nothing runs afterwards to refuse a second call */
            jump_rc = instant ? rx_session_spawn_at_instant(s, marker_pos, marker_dir) : rx_session_spawn_at(s, marker_pos, marker_dir);
            again = instant ? 3 : rx_session_spawn_at(s, marker_pos, marker_dir);
            jumped = 1; jump_frame = f; jump_in_lap = (float)in_lap;
            printf("lap 2, %.2f s in: rx_session_spawn_at -> %d (again -> %d: %s), lap clock %.3f\n", in_lap, jump_rc, again, rx_session_last_error(s), (double)race.lap_time);
        }
        if (jumped && settled_frame < 0 && race.reset_timer <= 0.0f && (f > jump_frame || instant)) {
            settled_frame = f;
            rx_bike_state b; memset(&b, 0, sizeof b); rx_session_bike(s, &b);
            const float dx = b.position[0][0] - marker_pos[0], dy = b.position[0][1] - marker_pos[1], dz = b.position[0][2] - marker_pos[2];
            printf("the respawn ran out at frame %ld (%.2f s): the vehicle %.2f ft from the marker (dy %.2f)\n", f, (f - jump_frame) / 60.0, (double)sqrtf(dx * dx + dz * dz), (double)dy);
            placed_checked = sqrtf(dx * dx + dz * dz) < 1.5f && fabsf(dy) < 3.0f;
        }
        /* resets after the respawn ran (a rule putting the rider back) */
        const int resetting = race.reset_timer > 0.0f;
        if (settled_frame >= 0 && resetting && !was_resetting) {
            resets_seen_after_jump++;
            const rx_addr d = local_driver(), p = d ? U32(d, 0x74) : 0;
            printf("a reset at frame %ld (%.2f s after the respawn ran): reason %u\n", f, (f - settled_frame) / 60.0, p ? U32(p, 0x1170) : 0);
        }
        was_resetting = resetting;
        /* lap 3, 30 s in: keep a marker again (placing only) */
        if (n == 2 && !kept3 && in_lap >= 30.0) { float p2[3], d2[3]; kept3 = rx_session_spawn_pose(s, p2, d2) == 0; printf("lap 3, 30 s in: a marker kept again (%d)\n", kept3); }
        rx_lap_record got[8];
        const int k = rx_session_lap_log(s, seen, got, 8);
        for (int i = 0; i < k && n < 16; i++) {
            laps[n++] = got[i]; seen = got[i].serial; lap_start = got[i].race_time;
            print_lap(&got[i]); printf("     markers %u (frame %ld)\n", got[i].markers, f);
        }
        if (k > 0 && n == 2) { best_after_2 = race.best_lap; lap_time_after_2 = race.lap_time; }
        if (k > 0 && n == 3) best_after_3 = race.best_lap;
    }
    check(n >= 3, "three laps were logged");
    check(kept, "a marker was kept in the gate lap");
    check(jump_rc == 0, "rx_session_spawn_at in PRACTICE answers 0 (any time while racing, not only while down)");
    check(again == 3, "a second call while the respawn runs answers 3");
    check(placed_checked, "the vehicle is at the marker once the respawn ran");
    if (n >= 3) {
        const rx_lap_record *g = &laps[0], *m = &laps[1], *c = &laps[2];
        const float frame = 1.0f / 60.0f + 1e-4f;
        check(g->markers == 0 && g->flying == 0, "the gate lap: markers 0 (keeping a marker does not invalidate), from the gate");
        check(m->serial == 2 && m->markers == 1, "lap 2 (the jump) has markers 1");
        check(m->flying == 0, "lap 2 is not a flying lap (D+0x10f0: the game's best lap never takes it)");
        check(m->resets == 0, "lap 2's respawn is the marker's: resets 0");
        check(m->time > jump_in_lap + 1.0f && m->time < jump_in_lap + 60.0f, "lap 2 ended at the real crossing after the jump (20 s + the respawn + the ride from the marker), not at the jump");
        check(lap_time_after_2 >= 0.0f && lap_time_after_2 <= 2.0f * frame, "the lap clock restarted at the crossing that ended lap 2");
        check(!(best_after_2 > 0.0f && best_after_2 < 1e30f), "the game's best lap did not take lap 2 (none yet after the gate lap and the marker lap)");
        check(c->serial == 3 && c->flying == 1 && c->markers == 0 && c->resets == 0, "lap 3 is a flying lap with no marker and no reset (a marker kept in it, not used)");
        check(c->penalty == 0.0f && c->off_track <= 2.0f && c->wrong_way == 0, "lap 3 is clean (no penalty, off track <= 2 s, never the wrong way)");
        check(fabsf(c->exact - c->time) <= frame, "lap 3: |exact - time| within one frame");
        check(fabsf(c->time - (c->race_time - m->race_time)) <= 2.0f * frame, "lap 3's time is the race clock between its crossings (timed from the line)");
        check(fabsf(best_after_3 - (c->time + c->penalty)) < 1e-3f, "lap 3 is the game's best lap");
        printf("lap 2 %.3f s (jump %.2f s in), lap 3 %.3f s exact %.4f; game best after lap 2 %g, after lap 3 %.3f\n", (double)m->time, (double)jump_in_lap,
               (double)c->time, (double)c->exact, (double)best_after_2, (double)best_after_3);
    }
    check(resets_seen_after_jump == 0, "no reset after the respawn ran (the rules never put the rider back)");
    const int failed = g_fail;
    rx_session_destroy(s);
    printf("%s\n", failed ? "MARKERS FAIL" : "MARKERS PASS");
    return failed;
}

/* (w-alivegates) BASE SCALE: a practice on any level at a MAP SCALE and the track's default scale (rx_session_set_map_scale_base): the
 * autopilot rides `want` laps; checks rx_session_map_scale_records (1 exactly when scale == base) and the lap log's map scale bit (16) on
 * every lap: clear at the track's default, set at any other scale. */
static int cmd_scale(const char* root, const char* type, int type_id, const char* level, int location, float scale, float base, int want) {
    rx_session* s = rx_session_create(root);
    if (!s) { printf("FAIL create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (base != 1.0f) check(rx_session_set_map_scale_base(s, base) == 0, "rx_session_set_map_scale_base before the load");
    if (scale != 1.0f) check(rx_session_set_map_scale(s, scale) == 0, "rx_session_set_map_scale before the load");
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", type); e.event_type_id = type_id;
    snprintf(e.location, sizeof e.location, "%s", level); e.location_id = location; e.track_id = 0;
    e.riders = 1; e.ai_difficulty = 0; e.duration = 3; snprintf(e.duration_type, sizeof e.duration_type, "Laps");
    if (rx_session_set_practice(s, 1) != 0) { printf("FAIL set_practice: %s\n", rx_session_last_error(s)); return 1; }
    if (rx_session_event_load(s, &e) != 0) { printf("FAIL load: %s\n", rx_session_last_error(s)); return 1; }
    check(rx_session_set_map_scale_base(s, 1.0f) == 2, "rx_session_set_map_scale_base after the load is refused (2)");
    const int keeps = rx_session_map_scale_records(s), expect = fabsf(scale - base) <= 1e-4f * base;
    printf("%s %s: map scale %.3f, the track's default %.3f, records %s\n", type, level, (double)rx_session_map_scale(s), (double)rx_session_map_scale_base(s),
           keeps ? "kept" : "off");
    check(keeps == expect, "rx_session_map_scale_records is 1 exactly at the track's default scale");
    struct autopilot pilot = {0, 0};
    rx_lap_record got[8]; uint32_t seen = 0; int n = 0, bit = 0, clear = 0;
    rx_race_state race; memset(&race, 0, sizeof race);
    for (long f = 0; f < 60L * 60 * 5 * want + 20000 && n < want; f++) {
        rx_pad pad; memset(&pad, 0, sizeof pad);
        if (race.valid && race.state == 2) autopilot_pad(s, &pilot, &pad);
        rx_session_set_input(s, NULL, &pad, 1);
        if (rx_session_advance(s) != 0) { printf("FAIL advance: %s\n", rx_session_last_error(s)); return 1; }
        rx_session_race(s, &race);
        const int k = rx_session_lap_log(s, seen, got, 8);
        for (int i = 0; i < k; i++) { print_lap(&got[i]); seen = got[i].serial; n++; if (got[i].mods & 16u) bit++; else clear++; }
    }
    check(n >= want, "the laps were logged");
    if (expect) check(bit == 0 && clear == n, "at the track's default no lap carries the map scale bit (16): its laps count");
    else check(bit == n, "at another scale every lap carries the map scale bit (16): not kept, not posted");
    rx_session_destroy(s);
    printf("%s\n", g_fail ? "FAIL" : "PASS");
    return g_fail;
}

int main(int argc, char** argv) {
    if (argc >= 10 && !strcmp(argv[2], "scale"))
        return cmd_scale(argv[1], argv[3], atoi(argv[4]), argv[5], atoi(argv[6]), (float)atof(argv[7]), (float)atof(argv[8]), atoi(argv[9]));
    if (argc >= 3 && !strcmp(argv[2], "practice")) return cmd_practice(argv[1], argc > 3 ? atoi(argv[3]) : 10);
    if (argc >= 3 && !strcmp(argv[2], "markers")) return cmd_markers(argv[1]);
    if (argc >= 3 && !strcmp(argv[2], "finish")) return cmd_finish(argv[1], argc > 3 ? atoi(argv[3]) : 2);
    fprintf(stderr, "usage: practice_test <install root> practice [laps] | finish [laps] | markers\n");
    return 2;
}
