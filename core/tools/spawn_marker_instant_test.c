/* spawn_marker_instant_test.c -- w-markerbtns: the INSTANT marker respawn (rx_session_spawn_at_instant) against the reset key's path
 * (rx_session_spawn_at, the oracle) and the RESET key itself, through the PRODUCT session API.
 *   spawn_marker_instant_test <install root> [level [location id]]      (default FR_Alpine 45, a Free Ride)
 * Every run is a fresh session of the same Free Ride with the same scripted ride (the core is deterministic): stand 2 s, keep the marker
 * (rx_session_spawn_pose), ride away 7 s with a steer pulse, then the respawn is requested between two frames (request frame 0). Runs:
 *   slow      rx_session_spawn_at (the reset key's path: Player_ResetTo, the white fade, Player_Reset when FadeOutTime runs out)
 *   instant   rx_session_spawn_at_instant
 *   key       the RESET key (the remap table's keyboard Reset column) pressed for 3 frames instead: the normal reset's timer
 * and for slow / instant once more with the throttle held from the request on. The checks (exit 0 = all pass):
 *   timing    instant: placed at the marker when the call returns (0 frames); slow: after FadeOutTime (about 30 frames at 60 Hz); the RESET key's
 *             timer runs the same number of frames as the slow path (the original's timing, unchanged)
 *   placed    instant: at the marker (x, z within 1 ft, y within 1.5 ft), facing its heading, standing, not wrecked, reason 1, timer 0
 *   same      the placement (all four bodies' positions, the velocity, the respawn fade state and clock, the crash timer, the reset reason,
 *             the wreck) is the slow run's; the reset state stays the same frame for frame over 4 s idle + 2 s throttle. A further instant
 *             run requested after the frames the slow path rides on during its hold (the same history: the drivetrain and wheel spin no reset
 *             clears) follows the slow run all along (positions within 0.01 ft, orientations, velocity, speed, gear); the instant run requested
 *             at the same moment as the slow one is printed (its drivetrain is the moment of the request's, not 0.5 s later's)
 *   control   throttle held from the request: frames until the rider moves (> 1 ft/s) at the marker, slow vs instant
 * Build: core/tools/spawn_marker_instant_test.sh <out dir> */
#include "rx_api.h"
#include "rx_mem.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_session* s;
static int dik_of[256];
static rx_remap_table remap;
#define KB(col) (dik_of[remap.keyboard[(col)] & 255] & 255)
static int failures;
#define CHECK(c, ...) do { if (c) printf("  ok   %s\n", #c); else { failures++; printf("  FAIL %s  ", #c); printf(__VA_ARGS__); printf("\n"); } } while (0)

static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8u);
    for (uint32_t i = 0; i < U8(g, 0x1541c) && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot < 16 && U32(g + 0x7460 + slot * 0x12a0, 0x10) == 0) return g + 0x7460 + slot * 0x12a0;
    }
    return 0;
}
static int step(const uint8_t* keys) {
    if (rx_session_set_input(s, keys, NULL, 0) != 0) return 1;
    return rx_session_advance_dt(s, 1.f / 60);
}
static int idle(int n) { uint8_t k[256] = {0}; for (int i = 0; i < n; i++) if (step(k)) return 1; return 0; }
static int ride(int frames, int steer) {
    uint8_t keys[256];
    for (int i = 0; i < frames; i++) {
        memset(keys, 0, sizeof keys);
        keys[KB(0)] = 0x80;
        if (steer && (i / 90) % 2 == 0) keys[KB(steer > 0 ? 2 : 3)] = 0x80;
        if (step(keys)) return 1;
    }
    return 0;
}

typedef struct {
    rx_bike_state b;
    float heading[2];
    int wrecked;
    uint32_t fade_state, reason;
    float fade_clock, fade_len, reset_timer, crash_timer;
} snap_t;
static snap_t snap(rx_addr p) {
    snap_t r; memset(&r, 0, sizeof r);
    rx_session_bike(s, &r.b);
    float m[18 * 16]; const int nn = rx_session_bike_pose(s, m, 18);
    float hx = nn > 0 ? m[8] : 0.f, hz = nn > 0 ? m[10] : 1.f, l = sqrtf(hx * hx + hz * hz);
    if (l < 1e-3f) { hx = 0; hz = 1; l = 1; }
    r.heading[0] = hx / l; r.heading[1] = hz / l;
    rx_onboard_state o; memset(&o, 0, sizeof o); rx_session_onboard_state(s, &o); r.wrecked = o.wrecked;
    r.fade_state = U32(p, 0x11a0); r.fade_clock = F32(p, 0x11a8); r.fade_len = F32(p, 0x11a4);
    r.reset_timer = F32(p, 0x116c); r.crash_timer = F32(p, 0x1168); r.reason = U32(p, 0x1170);
    return r;
}
static float dist2d(const float* a, const float* b) { return hypotf(a[0] - b[0], a[2] - b[2]); }

static int load(const char* install, const char* level, int loc) {
    s = rx_session_create(install);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", "FreeRide");
    e.event_type_id = 9;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = loc; e.track_id = 0; e.vehicle_class = -1; e.riders = 1; e.ai_difficulty = 0; e.duration = 0; e.duration_type[0] = 0; e.event_activation[0] = 0;
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f++ < 3000) if (rx_session_advance_dt(s, 1.f / 60)) break;
    for (int i = 0; i < 3000 && rx_session_get_state(s) != RX_SESSION_RACING; i++) if (rx_session_advance_dt(s, 1.f / 60)) break;
    if (rx_session_get_state(s) != RX_SESSION_RACING) { printf("not racing (state %d): %s\n", (int)rx_session_get_state(s), rx_session_last_error(s)); return 1; }
    memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    memset(dik_of, 0, sizeof dik_of);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    return 0;
}

enum { SLOW, INSTANT, KEY };
#define POST 360   /* frames recorded from the placement: 240 idle + 120 throttle */
typedef struct {
    int rc, placed_at;      /* frames stepped after the request until the vehicle was at the marker with the timer done (instant: 0) */
    int timer_frames;       /* frames the reset timer (+0x116c) ran */
    int moving_at;          /* throttle held from the request: frames until > 1 ft/s at the marker */
    float marker[3], dir[3];
    snap_t at[POST];        /* at[0] = the state at placement (instant: right after the call; slow: after the frame that placed it) */
} run_t;

static int run(const char* install, const char* level, int loc, int mode, int throttle, run_t* out, int delay) {
    memset(out, 0, sizeof *out); out->placed_at = out->moving_at = out->timer_frames = -1;
    if (load(install, level, loc)) return 1;
    rx_addr p = local_player();
    if (!p) { printf("no player\n"); return 1; }
    if (idle(120)) return 1;
    if (rx_session_spawn_pose(s, out->marker, out->dir) != 0) { printf("pose\n"); return 1; }
    if (ride(300, 0) || ride(420, 1)) return 1;   /* (spawn_marker_test's ride) */
    if (!throttle) { snap_t a = snap(p); printf("  [run %d] marker %.1f %.1f %.1f; rode to %.1f %.1f %.1f speed %.1f wrecked %d (%.0f ft away)\n", mode, out->marker[0], out->marker[1], out->marker[2],
                                 a.b.position[0][0], a.b.position[0][1], a.b.position[0][2], a.b.speed, a.wrecked, dist2d(a.b.position[0], out->marker)); }
    if (idle(delay)) return 1;   /* (the "same history" run: the frames the slow path rides on during its hold, with no input as there) */
    uint8_t keys[256];
    int frame = 0;
    if (mode == KEY) {   /* the timer's frames: from the step that started it (it already counts down in that frame's MXPlayer_Update) to the step that ended it */
        int first = -1, last = -1;
        for (int i = 0; i < 600 && last < 0; i++) {
            memset(keys, 0, sizeof keys);
            if (i < 3) keys[KB(12)] = 0x80;
            if (step(keys)) return 1;
            const int running = F32(p, 0x116c) > 0.f;
            if (running && first < 0) first = i;
            else if (!running && first >= 0) last = i;
        }
        out->timer_frames = first >= 0 && last >= 0 ? last - first + 1 : -1;
        out->rc = 0;
        rx_session_destroy(s); s = NULL;
        return 0;
    }
    out->rc = mode == INSTANT ? rx_session_spawn_at_instant(s, out->marker, out->dir) : rx_session_spawn_at(s, out->marker, out->dir);
    if (out->rc != 0) { printf("  spawn rc %d: %s\n", out->rc, rx_session_last_error(s)); rx_session_destroy(s); s = NULL; return 0; }
    int timer_start = F32(p, 0x116c) > 0.f ? 0 : -1;
    for (int i = 0; i < 600; i++) {
        snap_t now = snap(p);
        const int placed = !(now.reset_timer > 0.f) && dist2d(now.b.position[0], out->marker) < 1.0f;
        if (throttle && out->moving_at < 0 && placed && now.b.speed > 1.0f) out->moving_at = frame;
        if (!throttle && placed && out->placed_at < 0) { out->placed_at = frame; break; }
        if (throttle && out->moving_at >= 0) break;
        memset(keys, 0, sizeof keys);
        if (throttle) keys[KB(0)] = 0x80;
        if (step(keys)) return 1;
        frame++;
        if (getenv("SMI_TRACE") && frame < 50) { snap_t a = snap(p); printf("    f%d timer %.3f fade %u pos %.2f %.2f %.2f d %.2f speed %.2f state %d\n", frame, a.reset_timer, a.fade_state, a.b.position[0][0], a.b.position[0][1], a.b.position[0][2], dist2d(a.b.position[0], out->marker), a.b.speed, (int)rx_session_get_state(s)); }
        if (timer_start >= 0 && !(F32(p, 0x116c) > 0.f) && out->timer_frames < 0) out->timer_frames = frame - timer_start;
    }
    if (out->timer_frames < 0 && timer_start >= 0 && !(F32(p, 0x116c) > 0.f)) out->timer_frames = 0;
    if (!throttle && out->placed_at >= 0) {
        for (int k = 0; k < POST; k++) {
            out->at[k] = snap(p);
            memset(keys, 0, sizeof keys);
            if (k >= 240) keys[KB(0)] = 0x80;
            if (step(keys)) return 1;
        }
    }
    rx_session_destroy(s); s = NULL;
    return 0;
}

/* the runs' results: host memory the session reset does not restore (rx_reset.h copies this image's statics back at every destroy) */
static run_t* R;

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: spawn_marker_instant_test <install> [level [location id]]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    const char* level = argc > 2 ? argv[2] : "FR_Alpine";
    const int loc = argc > 3 ? atoi(argv[3]) : 45;
    printf("spawn_marker_instant_test: FreeRide %s\n", level);
    R = calloc(6, sizeof *R);
    if (!R) return 1;
    if (run(argv[1], level, loc, SLOW, 0, &R[0], 0)) return 1;
    printf("  slow:     rc %d, placed %d frames after the request (timer %d frames)\n", R[0].rc, R[0].placed_at, R[0].timer_frames);
    if (run(argv[1], level, loc, INSTANT, 0, &R[1], 0)) return 1;
    printf("  instant:  rc %d, placed %d frames after the request (timer %d frames)\n", R[1].rc, R[1].placed_at, R[1].timer_frames);
    if (run(argv[1], level, loc, KEY, 0, &R[2], 0)) return 1;
    printf("  RESET key: the reset timer ran %d frames from the press\n", R[2].timer_frames);
    if (run(argv[1], level, loc, SLOW, 1, &R[3], 0)) return 1;
    if (run(argv[1], level, loc, INSTANT, 1, &R[4], 0)) return 1;
    if (run(argv[1], level, loc, INSTANT, 0, &R[5], R[0].placed_at)) return 1;

    CHECK(R[0].rc == 0 && R[1].rc == 0, "rc slow %d instant %d", R[0].rc, R[1].rc);
    CHECK(R[1].placed_at == 0, "timing: instant placed after %d frames", R[1].placed_at);
    CHECK(R[0].placed_at >= 25 && R[0].placed_at <= 35, "timing: slow placed after %d frames (FadeOutTime 0.5 s)", R[0].placed_at);
    CHECK(R[2].timer_frames == R[0].timer_frames, "timing: RESET key %d frames vs the slow marker path %d", R[2].timer_frames, R[0].timer_frames);
    /* control: full throttle from the request; the bike's own acceleration to 1 ft/s after the placement is the same for both */
    printf("  throttle from the request: placed + moving (> 1 ft/s) after %d frames (slow; %d after its placement) / %d frames (instant)\n", R[3].moving_at, R[3].moving_at - R[0].placed_at, R[4].moving_at);
    CHECK(R[4].moving_at >= 0 && R[3].moving_at >= 0 && abs(R[4].moving_at - (R[3].moving_at - R[0].placed_at)) <= 1, "control: instant %d frames vs slow %d after its placement", R[4].moving_at, R[3].moving_at - R[0].placed_at);
    const snap_t* i0 = &R[1].at[0];
    CHECK(dist2d(i0->b.position[0], R[1].marker) < 1.0f && fabsf(i0->b.position[0][1] - R[1].marker[1]) < 1.5f, "placed: %.2f ft", dist2d(i0->b.position[0], R[1].marker));
    CHECK(i0->heading[0] * R[1].dir[0] + i0->heading[1] * R[1].dir[2] > 0.98f, "placed: heading");
    CHECK(i0->b.speed < 4.0f && !i0->wrecked && i0->reason == 1 && !(i0->reset_timer > 0.f), "placed: speed %.2f wrecked %d reason %u timer %.2f", i0->b.speed, i0->wrecked, i0->reason, i0->reset_timer);

    /* the same state from the placement on. Both paths place the vehicle before a frame's physics step (the slow path: MXPlayer_Update at the
     * end of the timer, after which that frame's step runs; the instant path: between two frames), so the vehicle records line up frame for
     * frame (slow[k] = instant[k], k = 0 the placement). The respawn fade clock of the slow path already ran that frame's tick in the same
     * MXPlayer_Update (Player_UpdateRespawnFade after Player_Reset); the instant path's first tick is the next frame's: the clocks line up as
     * slow[k] = instant[k + 1] (the instant ghost time is one frame, 1/60 s of its 3 s, later).
     * Two instant runs: requested at the same moment as the slow one (what the player gets), and requested after the frames the slow path
     * rides on during its hold (the same history: the drivetrain, the wheels' spin and the ride's other state that no reset clears, the
     * original's either, are then the same at the placement). The placement itself must match in both; the same-history run all along. */
    const snap_t* s0 = &R[0].at[0];
    for (int v = 1; v <= 5; v += 4) {
        const run_t* I = &R[v];
        float dpos = 0, dori = 0, dwheel = 0, dvel = 0, dspd = 0, dfade = 0; int dgear = 0, dstate = 0, dwreck = 0, dreason = 0, worst = -1, first = -1;
        for (int k = 0; k + 1 < POST; k++) {
            const snap_t *a = &R[0].at[k], *b = &I->at[k], *bf = &I->at[k + 1];
            for (int n = 0; n < 4; n++) for (int c = 0; c < 3; c++) { float d = fabsf(a->b.position[n][c] - b->b.position[n][c]); if (d > 1e-4f && first < 0) first = k; if (d > dpos) { dpos = d; worst = k; } }
            for (int n = 0; n < 4; n++) {   /* q and -q are the same rotation */
                float dp = 0, dm = 0;
                for (int c = 0; c < 4; c++) { dp = fmaxf(dp, fabsf(a->b.orientation[n][c] - b->b.orientation[n][c])); dm = fmaxf(dm, fabsf(a->b.orientation[n][c] + b->b.orientation[n][c])); }
                if (n < 2) { if (fminf(dp, dm) > dori) dori = fminf(dp, dm); }   /* bodies 0 / 1: the frame and the rider's body */
                else if (fminf(dp, dm) > dwheel) dwheel = fminf(dp, dm);         /* 2 / 3: the wheels (their spin angle is the ride's history) */
            }
            for (int c = 0; c < 3; c++) { float d = fabsf(a->b.velocity[c] - b->b.velocity[c]); if (d > dvel) dvel = d; }
            if (fabsf(a->b.speed - b->b.speed) > dspd) dspd = fabsf(a->b.speed - b->b.speed);
            if (fabsf(a->fade_clock - bf->fade_clock) > dfade) dfade = fabsf(a->fade_clock - bf->fade_clock);
            dgear += k > 0 && a->b.gear != b->b.gear;   /* (record 0: the instant path's gear is read before a frame has run since the placement) */
            dstate += a->fade_state != bf->fade_state; dwreck += a->wrecked != b->wrecked; dreason += a->reason != b->reason;
            if (getenv("SMI_TRACE") && (k < 4 || k == 239 || k == 240 || k == 300)) printf("    +%d slow pos %.4f %.4f %.4f spd %.3f gear %d fade %u %.3f | instant pos %.4f %.4f %.4f spd %.3f gear %d fade %u %.3f (next %.3f)\n", k,
                a->b.position[0][0], a->b.position[0][1], a->b.position[0][2], a->b.speed, a->b.gear, a->fade_state, a->fade_clock,
                b->b.position[0][0], b->b.position[0][1], b->b.position[0][2], b->b.speed, b->b.gear, b->fade_state, b->fade_clock, bf->fade_clock);
        }
        const snap_t* ip = &I->at[0];
        const char* name = v == 1 ? "instant (requested with the slow one)" : "instant (requested after the slow hold's frames: the same history)";
        printf("  %s\n    at placement  slow: pos %.3f %.3f %.3f speed %.3f fade %u clock %.3f/%.3f crash %.3f reason %u wrecked %d\n", name,
               s0->b.position[0][0], s0->b.position[0][1], s0->b.position[0][2], s0->b.speed, s0->fade_state, s0->fade_clock, s0->fade_len, s0->crash_timer, s0->reason, s0->wrecked);
        printf("               instant: pos %.3f %.3f %.3f speed %.3f fade %u clock %.3f/%.3f crash %.3f reason %u wrecked %d\n",
               ip->b.position[0][0], ip->b.position[0][1], ip->b.position[0][2], ip->b.speed, ip->fade_state, ip->fade_clock, ip->fade_len, ip->crash_timer, ip->reason, ip->wrecked);
        printf("    %d frames from the placement (4 s idle, 2 s throttle), max |slow - instant|: position %.5f ft (frame %d; first > 0.0001 ft at %d), orientation %.6f (wheels %.6f),"
               " velocity %.5f, speed %.5f, fade clock %.5f; frames that differ: gear %d, fade state %d, wreck %d, reason %d\n", POST - 1, dpos, worst, first, dori, dwheel, dvel, dspd, dfade, dgear, dstate, dwreck, dreason);
        int placement_same = 1;
        for (int n = 0; n < 4; n++) for (int c = 0; c < 3; c++) if (fabsf(s0->b.position[n][c] - ip->b.position[n][c]) > 1e-4f) placement_same = 0;
        for (int c = 0; c < 3; c++) if (fabsf(s0->b.velocity[c] - ip->b.velocity[c]) > 1e-4f) placement_same = 0;
        CHECK(placement_same && s0->fade_state == ip->fade_state && s0->wrecked == ip->wrecked && s0->reason == ip->reason && s0->crash_timer == ip->crash_timer && fabsf(ip->fade_clock - s0->fade_len) < 1e-6f,
              "same placement (%s)", name);
        CHECK(dstate == 0 && dwreck == 0 && dreason == 0, "same reset state all along (%s): fade state %d / wreck %d / reason %d frames differ", name, dstate, dwreck, dreason);
        if (v == 5) {
            CHECK(dpos < 0.01f, "same history: position differs by %.4f ft", dpos);
            CHECK(dori < 1e-3f && dvel < 0.01f && dspd < 0.01f && dfade < 2e-3f && dgear == 0, "same history: orientation %.5f velocity %.4f speed %.4f fade clock %.5f gear %d", dori, dvel, dspd, dfade, dgear);
        }
    }
    printf("%s\n", failures ? "FAILED" : "PASS");
    return failures ? 1 : 0;
}
