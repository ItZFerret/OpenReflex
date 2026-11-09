/* spawn_marker_test.c -- w-spawnmarker: SPAWN MARKER through the PRODUCT session API (core/api/spawn_marker_session.inc).
 *   spawn_marker_test <install root> [event type [type id [level [location id]]]]
 * default: FreeRide 9 FR_Alpine 45. A level ride: rx_session_spawn_pose keeps a marker (the vehicle's frame body position and heading), the
 * rider rides on (and is crashed by the game's own wreck when it finds one), then rx_session_spawn_at sends him back through the game's
 * reset. The checks (exit 0 = all pass):
 *   pose      the kept heading is the body's forward (the bike pose's node 0 +Z axis, horizontal)
 *   gate      a Free Ride only: any other event type (a race, ...) answers 2 and leaves the player alone (run with "National 7 NAT_Concord 1")
 *   running   a second call while the reset runs answers 3 (nothing changes)
 *   placed    when the reset has run its course: the vehicle is at the marker (x, z within 1 ft, y within 3 ft: it settles on its wheels),
 *             facing the marker's heading (dot >= 0.98), standing (speed < 4 ft/s), not wrecked, reset timer 0, reason 1 (manual)
 *   clean     the game's other reset route (the player's save spot written with the same spot + the UseResetPoint key = "back to spot")
 *             from the same ride gives the same vehicle state (position within 0.5 ft, heading within 0.01) once it has run its course
 *   repeat    a second marker use after riding off again places him again (the reset's state does not stick)
 * Build: core/tools/spawn_marker_test.sh <out dir> */
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
typedef struct { float pos[3], fwd[2], speed; int wrecked; } pose_t;
static pose_t pose(void) {
    pose_t r; memset(&r, 0, sizeof r);
    rx_bike_state b; memset(&b, 0, sizeof b); rx_session_bike(s, &b);
    float m[18 * 16]; const int nn = rx_session_bike_pose(s, m, 18);
    r.pos[0] = b.position[0][0]; r.pos[1] = b.position[0][1]; r.pos[2] = b.position[0][2];
    float hx = nn > 0 ? m[8] : 0.f, hz = nn > 0 ? m[10] : 1.f, l = sqrtf(hx * hx + hz * hz);
    if (l < 1e-3f) { hx = 0; hz = 1; l = 1; }
    r.fwd[0] = hx / l; r.fwd[1] = hz / l; r.speed = b.speed;
    rx_onboard_state o; memset(&o, 0, sizeof o); rx_session_onboard_state(s, &o); r.wrecked = o.wrecked;
    return r;
}
/* ride: throttle, with a steer pulse so the rider ends somewhere else than it started */
static int ride(int frames, int steer) {
    uint8_t keys[256] = {0};
    for (int i = 0; i < frames; i++) {
        memset(keys, 0, sizeof keys);
        keys[KB(0)] = 0x80;
        if (steer && (i / 90) % 2 == 0) keys[KB(steer > 0 ? 2 : 3)] = 0x80;
        if (step(keys)) return 1;
    }
    return 0;
}
/* until the player's reset has run its course (the timer +0x116c > 0 while it runs); returns the frames it took, -1 never started */
static int finish_reset(rx_addr p, int max) {
    uint8_t keys[256] = {0};
    int started = 0;
    for (int i = 0; i < max; i++) {
        if (F32(p, 0x116c) > 0.f) started = 1;
        else if (started) return i;
        if (step(keys)) return -2;
    }
    return started ? max : -1;
}

static int load(const char* install, const char* type, int type_id, const char* level, int loc) {
    s = rx_session_create(install);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", type);
    e.event_type_id = type_id;
    snprintf(e.location, sizeof e.location, "%s", level);
    e.location_id = loc; e.track_id = 0; e.vehicle_class = -1; e.riders = 1; e.ai_difficulty = 0; e.duration = 0; e.duration_type[0] = 0; e.event_activation[0] = 0;
    if (strcmp(type, "FreeRide") != 0) { e.duration = 2; snprintf(e.duration_type, sizeof e.duration_type, "Laps"); }
    if (rx_session_event_load(s, &e) != 0) { printf("LOAD failed: %s\n", rx_session_last_error(s)); return 1; }
    int f = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && f++ < 3000) if (rx_session_advance_dt(s, 1.f / 60)) break;
    for (int i = 0; i < 3000 && rx_session_get_state(s) != RX_SESSION_RACING; i++) if (rx_session_advance_dt(s, 1.f / 60)) break;
    if (rx_session_get_state(s) != RX_SESSION_RACING) { printf("not racing (state %d): %s\n", (int)rx_session_get_state(s), rx_session_last_error(s)); return 1; }
    memset(&remap, 0, sizeof remap); rx_remap_defaults(&remap);
    for (int d = 255; d > 0; d--) if (remap.scan[d] > 0 && remap.scan[d] < 256) dik_of[remap.scan[d]] = d;
    return 0;
}

static float dist2d(const float a[3], const float b[3]) { return hypotf(a[0] - b[0], a[2] - b[2]); }

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: spawn_marker_test <install> [type [type id [level [location id]]]]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    const char* type = argc > 2 ? argv[2] : "FreeRide";
    const int type_id = argc > 3 ? atoi(argv[3]) : 9;
    const char* level = argc > 4 ? argv[4] : "FR_Alpine";
    const int loc = argc > 5 ? atoi(argv[5]) : 45;
    const int free_ride = strcmp(type, "FreeRide") == 0;
    if (load(argv[1], type, type_id, level, loc)) return 1;
    rx_addr p = local_player();
    if (!p) { printf("no player\n"); return 1; }
    printf("spawn_marker_test: %s %s (%s): racing\n", type, level, free_ride ? "a Free Ride" : "not a Free Ride");
    for (int i = 0; i < 120; i++) { uint8_t k[256] = {0}; if (step(k)) return 1; }   /* standing on the ground at the start */

    float mpos[3], mdir[3];
    CHECK(rx_session_spawn_pose(s, mpos, mdir) == 0, "pose");
    pose_t at = pose();
    printf("  marker %.1f %.1f %.1f heading %.3f %.3f (bike pose %.3f %.3f) speed %.1f\n", mpos[0], mpos[1], mpos[2], mdir[0], mdir[2], at.fwd[0], at.fwd[1], at.speed);
    CHECK(mdir[0] * at.fwd[0] + mdir[2] * at.fwd[1] > 0.99f, "pose: the kept heading is not the body's forward");
    if (ride(300, 0)) { printf("ride: %s\n", rx_session_last_error(s)); return 1; }   /* (a heading while moving: the pose again, in the air or not) */
    { float qp[3], qd[3]; pose_t mv = pose(); CHECK(rx_session_spawn_pose(s, qp, qd) == 0 && qd[0] * mv.fwd[0] + qd[2] * mv.fwd[1] > 0.99f, "pose: heading while riding"); }

    if (!free_ride) {
        const float save0[3] = {at.pos[0], at.pos[1], at.pos[2]}; (void)save0;
        const int rc = rx_session_spawn_at(s, mpos, mdir);
        CHECK(rc == 2, "gate: a non-Free-Ride answered %d", rc);
        CHECK(!(F32(p, 0x116c) > 0.f), "gate: the reset must not start");
        printf("%s\n", failures ? "FAILED" : "PASS (gate)");
        return failures ? 1 : 0;
    }

    /* ride on, then use the marker */
    if (ride(420, 1)) { printf("ride: %s\n", rx_session_last_error(s)); return 1; }
    pose_t away = pose();
    printf("  rode away to %.1f %.1f %.1f (%.0f ft from the marker) speed %.1f wrecked %d\n", away.pos[0], away.pos[1], away.pos[2], dist2d(away.pos, mpos), away.speed, away.wrecked);
    CHECK(dist2d(away.pos, mpos) > 40.f || away.speed > 8.f, "the ride must have moved");
    const int rc = rx_session_spawn_at(s, mpos, mdir);
    CHECK(rc == 0, "spawn_at answered %d (%s)", rc, rx_session_last_error(s));
    CHECK(F32(p, 0x116c) > 0.f, "the game's reset timer must be running");
    CHECK(rx_session_spawn_at(s, mpos, mdir) == 3, "running: a second call must be refused");
    const int took = finish_reset(p, 600);
    CHECK(took >= 0, "the reset never ran its course (%d)", took);
    if (took >= 0) { for (int i = 0; i < 2; i++) if (step((uint8_t[256]){0})) return 1; }
    pose_t here = pose();
    printf("  reset ran %d frames; now %.1f %.1f %.1f heading %.3f %.3f speed %.1f wrecked %d reason %u timer %.2f\n", took, here.pos[0], here.pos[1], here.pos[2], here.fwd[0], here.fwd[1], here.speed, here.wrecked, U32(p, 0x1170), F32(p, 0x116c));
    CHECK(dist2d(here.pos, mpos) < 1.0f, "placed: %.2f ft from the marker", dist2d(here.pos, mpos));
    CHECK(fabsf(here.pos[1] - mpos[1]) < 1.5f, "placed: height off by %.2f", here.pos[1] - mpos[1]);
    CHECK(here.fwd[0] * mdir[0] + here.fwd[1] * mdir[2] > 0.98f, "placed: heading %.3f %.3f vs %.3f %.3f", here.fwd[0], here.fwd[1], mdir[0], mdir[2]);
    CHECK(here.speed < 4.0f, "placed: speed %.1f", here.speed);
    CHECK(!here.wrecked, "placed: wrecked");
    CHECK(U32(p, 0x1170) == 1, "placed: reason %u", U32(p, 0x1170));
    CHECK(!(F32(p, 0x116c) > 0.f), "placed: the reset timer");
    CHECK(rx_session_get_state(s) == RX_SESSION_RACING, "state");

    /* repeat: ride off again, use it again */
    if (ride(420, 0)) { printf("ride: %s\n", rx_session_last_error(s)); return 1; }
    pose_t away2 = pose();
    CHECK(dist2d(away2.pos, mpos) > 30.f, "repeat: rode off %.0f ft", dist2d(away2.pos, mpos));
    CHECK(rx_session_spawn_at(s, mpos, mdir) == 0, "repeat: spawn_at");
    CHECK(finish_reset(p, 600) >= 0, "repeat: reset");
    for (int i = 0; i < 2; i++) step((uint8_t[256]){0});
    pose_t again = pose();
    CHECK(dist2d(again.pos, mpos) < 1.0f && again.fwd[0] * mdir[0] + again.fwd[1] * mdir[2] > 0.98f, "repeat: %.2f ft, heading %.3f %.3f", dist2d(again.pos, mpos), again.fwd[0], again.fwd[1]);

    /* the game's own back to spot with the same spot: the save spot (+0x1140 position, +0x1150 orientation = yaw quaternion whose +Z axis is
     * the heading, +0x1130 set) and the UseResetPoint key. The vehicle state afterwards must be the same (the reset is the same code) */
    if (ride(300, 0)) { printf("ride: %s\n", rx_session_last_error(s)); return 1; }
    {
        const float a = atan2f(mdir[0], mdir[2]);
        SF32(p, 0x1140, mpos[0]); SF32(p, 0x1144, mpos[1]); SF32(p, 0x1148, mpos[2]); SF32(p, 0x114c, 1.f);
        SF32(p, 0x1150, 0.f); SF32(p, 0x1154, sinf(a / 2)); SF32(p, 0x1158, 0.f); SF32(p, 0x115c, cosf(a / 2));
        SU32(p, 0x1130, 1);
        uint8_t keys[256] = {0};
        for (int i = 0; i < 3; i++) { keys[KB(13)] = 0x80; if (step(keys)) return 1; }
        CHECK(finish_reset(p, 600) >= 0, "back to spot: reset");
        for (int i = 0; i < 2; i++) step((uint8_t[256]){0});
        pose_t key = pose();
        printf("  back to spot (the key): %.2f %.2f %.2f heading %.3f %.3f speed %.1f; marker: %.2f %.2f %.2f heading %.3f %.3f speed %.1f\n", key.pos[0], key.pos[1], key.pos[2], key.fwd[0], key.fwd[1], key.speed,
               again.pos[0], again.pos[1], again.pos[2], again.fwd[0], again.fwd[1], again.speed);
        CHECK(dist2d(key.pos, again.pos) < 0.5f && fabsf(key.pos[1] - again.pos[1]) < 0.5f, "clean: the key's reset ends %.2f ft from the marker's", dist2d(key.pos, again.pos));
        CHECK(key.fwd[0] * again.fwd[0] + key.fwd[1] * again.fwd[1] > 0.999f, "clean: heading");
    }
    printf("%s\n", failures ? "FAILED" : "PASS");
    rx_session_destroy(s);
    return failures ? 1 : 0;
}
