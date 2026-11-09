/* practice.c -- PORT ADDITION (not original game code): PRACTICE and the LAP LOG (practice.h, core/plan/LEADERBOARDS.md). */
#include "practice.h"
#include "race.h"   /* w-lb-markers: the driver's place on the track after a marker respawn (rx_practice_marker_settle) */
#include <string.h>

uint32_t rx_practice_value;   /* restored to 0 with the core's statics at destroy */

void rx_practice_set(int on) { rx_practice_value = on ? 1u : 0u; }

int rx_practice_lap_type(uint32_t type) { return type == 0 || type == 1 || type == 0x11 || type == 0x12 || type == 0x13; }

int rx_practice_applies(rx_addr A) {
    return rx_practice_value && A && U32(A, 0x238) == 1 && rx_practice_lap_type(U32(A, 0x23c));
}

/* ---- the lap log ------------------------------------------------------------------------------------------------------------------ */

rx_addr rx_lap_log_driver;    /* the local human's race driver, set by the session layer before each frame */
float   rx_lap_log_dt;        /* its lap clock step in this frame (RaceDriver::vf7), 0 when the clock did not run */

static rx_lap_note g_ring[RX_LAP_LOG_RING];
static uint32_t g_serial;                       /* the last serial given (the session's laps so far) */
/* the open lap's accumulators */
static float    g_off_track, g_carry;
static uint32_t g_resets, g_mods;
static int32_t  g_wrong_way;
static int      g_resetting;                   /* the last frame's P+0x116c > 0 (the rising edge counts a reset) */
/* the vehicle's position at the end of the last frame (the sub-frame crossing's move) */
static float    g_prev[3];
static int      g_prev_ok;
/* SPAWN MARKERS in practice (practice.h): the open lap's rides back to the marker; a respawn to settle (1 while its reset runs, then the
 * frames left to place the driver) */
static uint32_t g_markers;
static int      g_marker_pending, g_marker_settled;

void rx_lap_log_track(rx_addr driver) {
    if (driver != rx_lap_log_driver) { g_prev_ok = 0; g_resetting = 0; g_marker_pending = 0; }
    rx_lap_log_driver = driver;
    rx_lap_log_dt = 0.0f;
}

static void open_lap(void) { g_off_track = 0.0f; g_resets = 0; g_mods = 0; g_wrong_way = 0; g_markers = 0; }

void rx_lap_log_frame(int racing, int on_track, int wrong_way, int resetting, uint32_t mods, const float pos[3]) {
    if (!racing) {   /* before the start (or a restart back to the gate): the first lap opens at the start, from nothing */
        open_lap(); g_carry = 0.0f; g_resetting = 0; g_prev_ok = 0; g_marker_pending = 0;
        return;
    }
    if (!on_track) g_off_track += rx_lap_log_dt;
    if (resetting && !g_resetting) g_resets++;
    g_resetting = resetting;
    if (wrong_way) g_wrong_way = 1;
    g_mods |= mods;
    if (pos) { memcpy(g_prev, pos, sizeof g_prev); g_prev_ok = 1; } else g_prev_ok = 0;
}

void rx_lap_log_note(rx_addr d, float penalty) {
    if (!d || d != rx_lap_log_driver) return;
    rx_lap_note* r = &g_ring[g_serial % RX_LAP_LOG_RING];
    memset(r, 0, sizeof *r);
    r->serial = ++g_serial;
    r->flying = U32(d, 0x10f0) == 0;
    r->time = F32(d, 0xf20);
    r->penalty = penalty;
    r->off_track = g_off_track;
    r->race_time = F32(d, 0x134);
    r->resets = g_resets;
    r->wrong_way = g_wrong_way;
    r->mods = g_mods;
    r->markers = g_markers;
    /* the sub-frame crossing (practice.h): the frame's move, else the finish rule's own D+0x80 -> V+0xc0 */
    float over = 0.0f;
    const rx_addr v = U32(U32(d, 0x74), 0x1ac), track = U32(d, 0x10);
    if (v && track && rx_lap_log_dt > 0.0f) {
        const float cur[3] = {F32(v, 0xc0), F32(v, 0xc4), F32(v, 0xc8)};
        float f = g_prev_ok ? rx_race_line_fraction(track + 0x70, cur, g_prev) : -1.0f;
        if (f < 0.0f) {
            const float last[3] = {F32(d, 0x80), F32(d, 0x84), F32(d, 0x88)};
            f = rx_race_line_fraction(track + 0x70, cur, last);
        }
        if (f >= 0.0f) over = (1.0f - f) * rx_lap_log_dt;
    }
    r->exact = r->time - over + g_carry;
    g_carry = over;
    open_lap();
}

int rx_lap_log_read(uint32_t after, rx_lap_note* out, int max) {
    if (!out || max <= 0) return 0;
    uint32_t first = after + 1;
    if (g_serial > RX_LAP_LOG_RING && first <= g_serial - RX_LAP_LOG_RING) first = g_serial - RX_LAP_LOG_RING + 1;   /* older ones are gone */
    int n = 0;
    for (uint32_t s = first; s <= g_serial && n < max; s++) out[n++] = g_ring[(s - 1) % RX_LAP_LOG_RING];
    return n;
}

/* ---- SPAWN MARKERS in practice (w-lb-markers; practice.h) ---------------------------------------------------------------------------- */

void rx_lap_log_marker(rx_addr d) {
    if (!d || d != rx_lap_log_driver) return;
    g_markers++;
    g_resetting = 1;              /* the respawn's reset (P+0x116c > 0 from now on) is the marker's, not a reset of the lap */
    g_marker_pending = 1; g_marker_settled = 0;
    SU32(d, 0x10f0, 1);           /* the lap did not begin at the line (as the gate lap: RaceDriver::vf3 0050f090): no "Best Lap" from it */
}

/* the driver's place on the track is the vehicle's, as RaceDriver::vf3 0050f090 places a Free Ride driver (the nearest node of the whole
 * track by 00538440, the vehicle's direction +0x130); the segment is the one the vehicle is in (the node before the nearest when the
 * vehicle is behind it); the last position is the vehicle's; the track-advance rule's section follows (as RuleTrackAdvance::vf5 0051d400
 * does at a section change); the off-track rule's excursion ends (RuleOfftrackTimePenalty: on track, no excursion, the clock now) */
static void marker_place(rx_addr d, rx_addr v) {
    const rx_addr sp0 = rx_sp(), S = (sp0 - 0x40u) & ~0xfu;
    rx_set_sp(S);
    const rx_addr track = U32(d, 0x10);
    rx_track_nearest_node_all(track, v + 0x130, 0, v + 0xc0, S);
    if (U32(S, 4) == 0) rx_track_nearest_node_all(track, 0, 0, v + 0xc0, S);
    rx_addr sec = U32(S, 0), node = U32(S, 4);
    rx_set_sp(sp0);
    if (!sec || !node) return;
    const float t = (F32(v, 0xc0) - F32(node, 0)) * F32(node, 0x50) + (F32(v, 0xc4) - F32(node, 4)) * F32(node, 0x54) + (F32(v, 0xc8) - F32(node, 8)) * F32(node, 0x58);
    if (t < 0.0f && U32(node, 0x60) != 0) node = U32(node, 0x60);
    SU32(d, 0x14, sec); SU32(d, 0x18, node); SU32(d, 0x1c, U32(node, 0x64));
    if (U32(node, 0x64) == 0) {   /* the section's last node: the next section's start (as 0050f090) */
        const rx_addr first = U32(U32(sec, 8), 0);
        if (first) { SU32(d, 0x14, first); SU32(d, 0x18, U32(first, 0)); SU32(d, 0x1c, U32(U32(first, 0), 0x64)); }
    }
    SU32(d, 0x90, U32(d, 0x1c));
    SF32(d, 0x80, F32(v, 0xc0)); SF32(d, 0x84, F32(v, 0xc4)); SF32(d, 0x88, F32(v, 0xc8));
    const rx_addr adv = rx_driver_track_advance_rule(d);
    if (adv) {
        const rx_addr s2 = U32(d, 0x14);
        SU32(adv, 0x10, s2);
        SF32(adv, 0xc, (U32(s2, 0x14) != 0 && I32(U32(U32(s2, 0xc), 0), 0x10) > 1) ? 0.0f : rx_bf(0x7f7fffffu));
    }
    const rx_addr pen = rx_driver_offtrack_penalty_rule(d);
    if (pen) {
        SU32(pen, 0x10, 1); SU32(pen, 0x14, 0); SU32(pen, 0x18, 0);
        SF32(pen, 0xc, 0.0f); SF32(pen, 0x20, 0.0f); SF32(pen, 0x54, 0.0f); SF32(pen, 0xd4, 0.0f);
        SF32(pen, 0x28, F32(d, 0x130));
    }
}

int rx_practice_marker_settle(rx_addr d) {
    if (!g_marker_pending || !d || d != rx_lap_log_driver) return 0;
    const rx_addr p = U32(d, 0x74), v = p ? U32(p, 0x1ac) : 0;
    if (!v || F32(p, 0x116c) > 0.0f) return 1;   /* the respawn's reset still runs (the vehicle is placed when it runs out) */
    marker_place(d, v);
    if (++g_marker_settled >= 2) g_marker_pending = 0;   /* that frame and the next (the vehicle's position follows its placement) */
    return g_marker_pending;
}
