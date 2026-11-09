/* trick_hold_ride.c -- the trick hold / release case on the product core (w-fsfix; the original's side is the oracle hook
 * core/oracle/trick_hold_probe.cpp, rx_TrickPadHook, which drives the same script and prints the same line).
 *
 *   trick_hold_ride <install root>      env: FS=1 a Freestyle event (FMX_BuenaVista; DUR minutes, default 1; CLS class, default 24;
 *                                             RIDERS / AI), else Free Ride; LOC / LOCID the level (FMX_BuenaVista / 39)
 *                                             REL=<n> let go n frames after the gesture's last direction began (-1: hold to landing)
 *                                             RELMODE 0 both, 1 the stick only, 2 LB only; TRICKS (3) take-offs used; FRAMES (1000)
 *                                             RIDERS_TRACE=<n>: every n frames each player slot's type, position and speed
 *                                             GESTURE=<g>[,<g>...] the gestures drawn (default 323), the k-th take-off draws the k-th
 *                                             (cycling): '0' up, '1' right, '2' down, '3' left (the classifier's 4 directions)
 *                                             CHAIN=1 the next gesture as soon as the trick plays (+0xaf10 set) while still airborne: the
 *                                             next trick starts as the air trick 00622bc0, chained after the playing one
 *                                             DROP=<ft>: after 30 frames on the ground the rider is sent DROP feet up (the SPAWN MARKER's
 *                                             respawn, rx_session_spawn_at: the game's own reset) for a long fall (Free Ride only)
 *                                             AIRDELAY=<n> (3) airborne frames before a gesture starts
 *
 * After the activity starts racing: RT held; on each take-off that lasts more than 3 frames (the tracker K = player + 0xb58, K + 0x88)
 * pad LB (the trick modifier) is held and the right stick draws "323" (9OClockNac_L_Air: left 5 frames, centre 2, down 5, centre 2,
 * then left). One line per frame while the trick or its input is live: the gesture state K + 0x5c, the hold flag K + 0x18, the mode
 * K + 8, the rider's +0xaf10 / +0xaf30 / +0xaf34, the performance animator's time (+0xaf14 -> +0x98) and the clip end +0xaf20.
 * The first line also gives the activity's Duration (+0x2bc) and clock (+0x294). With GESTURE / CHAIN / DROP / POSE=1 also a "trick" line
 * per gesture and a "pose" line whenever the rider's scripted pose clips (+0xc928 current, +0xc92c next) or the bike simulation's pose
 * override (+0x2de0 on, +0x2de4 mode) change.
 *
 * w-pose446 (the chained-trick pose override, simulation vf33 00446cd0 from 00620a60): a trick whose clip has no bike keys (NoHander 010,
 * Superman 031, ...) and a chained one whose clip has them (BarHop 023, Ruler 302, ...):
 *   LOC=FMX_DoverB LOCID=-1 TRICKS=6 FRAMES=400 CHAIN=1 DROP=120 AIRDELAY=10 GESTURE=010,023,023,023,023,023
 * stopped with "unported game function 00446cd0" before the fix; now a "pose ... c92c=0 sim.2de0=1" line (the override started).
 *
 * Build: source tools/product_link.sh (C = core, O = out dir); rx_product_objs; rx_link_driver $O/trick_hold_ride tools/trick_hold_ride.c */
#include "rx_api.h"
#include "../game/rx_game.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct rx_session* S;
static uint8_t keys[256];
static rx_pad pad[4];
static int step(void) { rx_session_set_input(S, keys, pad, 1); return rx_session_advance_dt(S, 1.0f / 60.0f); }
static int envi(const char* n, int d) { const char* e = getenv(n); return e && *e ? atoi(e) : d; }
/* the local human player (api/rx_session.c local_player) or 0 */
static rx_addr local_player(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541c);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4 * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) != 0) continue;
        return U32(p, 0x1ac) && U32(p, 0x1b4) ? p : 0;
    }
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: trick_hold_ride <install root>\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    S = rx_session_create(argv[1]);
    if (!S) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("FA")) rx_session_set_free_air(S, atoi(getenv("FA")));
    rx_event_spec e; rx_session_event_default(&e);
    const int fs = getenv("FS") != NULL;
    snprintf(e.event_type, sizeof e.event_type, "%s", fs ? "Freestyle" : "FreeRide");
    e.event_type_id = fs ? 7 : 9;
    snprintf(e.location, sizeof e.location, "%s", getenv("LOC") ? getenv("LOC") : "FMX_BuenaVista");
    e.location_id = envi("LOCID", 39);
    e.track_id = 0; e.event_activation[0] = 0;
    e.duration = fs ? envi("DUR", 1) : 0;
    snprintf(e.duration_type, sizeof e.duration_type, "%s", fs ? "Minutes" : "");
    e.vehicle_class = fs ? envi("CLS", 24) : -1;
    e.riders = fs ? envi("RIDERS", 1) : 1;
    e.ai_difficulty = fs ? envi("AI", 0) : 0;
    if (rx_session_event_load(S, &e)) { fprintf(stderr, "load: %s\n", rx_session_last_error(S)); return 1; }
    int lf = 0;
    while (rx_session_get_state(S) != RX_SESSION_RACING && lf < 10000) { if (step()) break; lf++; }
    {
        const rx_addr g = U32(0, 0x00a946b8), A = U32(g + 0x7460, 0x94);
        printf("racing after %d frames: activity type %u duration %.3f clock %.3f\n", lf, U32(A, 0x23c), (double)F32(A, 0x2bc), (double)F32(A, 0x294));
    }
    const int rel = envi("REL", -1), relmode = envi("RELMODE", 0), frames = envi("FRAMES", 1000), max_tricks = envi("TRICKS", 3);
    int phase = 0, pf = 0, air_frames = 0, tricks = 0;
    const char* gestures = getenv("GESTURE") && *getenv("GESTURE") ? getenv("GESTURE") : "323";
    char gesture[16] = "323";
    uint32_t pose_seen[4] = {~0u, ~0u, ~0u, ~0u};
    const int chain = envi("CHAIN", 0), drop = envi("DROP", 0), air_delay = envi("AIRDELAY", 3);
    const int extra = getenv("GESTURE") || chain || drop || envi("POSE", 0);   /* the w-pose446 lines (the oracle hook's script prints none) */
    int ground_frames = 0;
    for (int f = 0; f < frames; f++) {
        const rx_addr p = local_player();
        memset(pad, 0, sizeof pad);
        int air = 0;
        if (p) {
            const rx_addr K = p + 0xb58;
            air = U32(K, 0x88) != 0;
            pad[0].right_trigger = 255;
            air_frames = air ? air_frames + 1 : 0;
            ground_frames = air ? 0 : ground_frames + 1;
            if (drop > 0 && phase == 0 && ground_frames == 30) {   /* up into the air again: the game's own reset at a higher spot */
                float pos[3], dir[3];
                if (rx_session_spawn_pose(S, pos, dir) == 0) {
                    pos[1] += (float)drop;
                    const int rc = rx_session_spawn_at(S, pos, dir);
                    printf("drop f%d: (%.1f, %.1f, %.1f) rc %d\n", f, (double)pos[0], (double)pos[1], (double)pos[2], rc);
                }
            }
            if (phase == 0 && air && air_frames > air_delay && tricks < max_tricks) {
                phase = 1; pf = 0;
                const char* g = gestures;   /* the tricks-th gesture of the list, cycling */
                int n = 1;
                for (const char* c = gestures; *c; c++) n += *c == ',';
                for (int k = tricks % n; k > 0; k--) g = strchr(g, ',') + 1;
                size_t len = strcspn(g, ",");
                if (len == 0 || len >= sizeof gesture) len = 3, g = "323";
                memcpy(gesture, g, len); gesture[len] = 0;
                if (extra) printf("trick %d: gesture %s\n", tricks, gesture);
                tricks++;
            }
            if (phase >= 1) {
                /* each direction 5 frames then 2 centred; the last one held ("323": left 5, centre 2, down 5, centre 2, then left) */
                const int last = 7 * ((int)strlen(gesture) - 1), at = pf < last ? pf / 7 : (int)strlen(gesture) - 1;
                int lb = 1, rx = 0, ry = 0;
                if (pf >= last || pf % 7 < 5) {
                    const char d = gesture[at];
                    if (d == '0') ry = 30000; else if (d == '1') rx = 30000; else if (d == '2') ry = -30000; else if (d == '3') rx = -30000;
                }
                if (rel >= 0 && pf >= last + rel) { if (relmode == 0 || relmode == 1) { rx = 0; ry = 0; } if (relmode == 0 || relmode == 2) lb = 0; }
                if (lb) pad[0].buttons |= 0x0100;
                pad[0].rx = (int16_t)rx; pad[0].ry = (int16_t)ry;
                pf++;
                if (!air && pf > 20) phase = 0;
                if (chain && air && pf > last + 4 && U32(U32(p, 0x1b4), 0xaf10) != 0) phase = 0;   /* the trick plays: the next one chains on */
            }
        }
        if (step()) { fprintf(stderr, "advance: %s\n", rx_session_last_error(S)); return 1; }
        if (envi("RIDERS_TRACE", 0) > 0 && f % envi("RIDERS_TRACE", 0) == 0) {   /* the oracle hook's RX_TRICK_RIDERS line */
            const rx_addr g = U32(0, 0x00a946b8);
            printf("riders f%d:", f);
            for (uint32_t k = 0; k < 16; k++) {
                const rx_addr pk = g + 0x7460 + k * 0x12a0, v = U32(pk, 0x1ac);
                if (!v) continue;
                const rx_addr sim = U32(v, 0x234);
                printf(" [%u t%u pos %.1f %.1f %.1f v %.1f]", k, U32(pk, 0x10), sim ? (double)F32(sim, 0xf0) : 0.0, sim ? (double)F32(sim, 0xf4) : 0.0,
                       sim ? (double)F32(sim, 0xf8) : 0.0, (double)F32(v, 0x150));
            }
            printf("\n");
        }
        if (!p) continue;
        const rx_addr K = p + 0xb58, ra = U32(p, 0x1b4), anim = U32(ra, 0xaf14);
        {
            const rx_addr sim = U32(U32(p, 0x1ac), 0x234);
            const uint32_t now[4] = {U32(ra, 0xc928), U32(ra, 0xc92c), sim ? U32(sim, 0x2de0) : 0, sim ? U32(sim, 0x2de4) : 0};
            if (extra && memcmp(now, pose_seen, sizeof now) != 0) {
                printf("pose f%d c928=%08x c92c=%08x sim.2de0=%u 2de4=%u\n", f, now[0], now[1], now[2], now[3]);
                memcpy(pose_seen, now, sizeof now);
            }
        }
        if (phase >= 1 || U32(K, 8) != 0 || U32(ra, 0xaf10) != 0)
            printf("f%d pf%d air%d lb%d rs(%d,%d) gest%u flag%u mode%u trick=%s ra.st%u hold%u w%u anim98=%.3f end=%.3f\n", f, pf, air,
                   (pad[0].buttons & 0x100) != 0, pad[0].rx, pad[0].ry, U32(K, 0x5c), U32(K, 0x18), U32(K, 8), U32(K, 0x1c) ? "(t)" : "-",
                   U32(ra, 0xaf10), U32(ra, 0xaf30), U32(ra, 0xaf34), anim ? (double)F32(anim, 0x98) : -1.0, (double)F32(ra, 0xaf20));
    }
    (void)argc;
    return 0;
}
