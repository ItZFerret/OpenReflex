/* spawn_marker_session.inc -- SPAWN MARKER (port addition, w-spawnmarker; after Open Unleashed's SPAWN MARKER), included by rx_session.c
 * (it uses session_ok, session_error and local_player).
 *
 * rx_session_spawn_at sends the local human's vehicle through the GAME'S OWN reset to a spot the host chose, exactly as the reset key
 * does (Player_HandleResetInput 0054e4f0 -> Player_ResetManual 00550400 -> Player_ResetTo 005501f0): the spot goes to the player
 * (+0x1180 position, +0x1190 forward direction), the reset timer starts and the white reset fade runs; MXPlayer_Update 0054af90 finishes
 * it when the timer runs out (Player_Reset 0054fff0: the trick controls, the wreck, the crash recovery, the input groups; the vehicle
 * placed at the spot upright with its velocity gone: 00460020; the game's own ghost time after a reset). Nothing of the original's reset
 * is changed or bypassed; only the spot is ours. The player's own save spot (+0x1130 .. +0x1150, the Y / B keys' "back to spot") is
 * not touched.
 *
 * A refusal never ends the session (last_error says why). Where the game's own reset key works, this works (0054e470: the activity racing, the Free Ride's not-paused flag) and nowhere else:
 * a human's Free Ride of an event world (the same gate as FREE RIDE WITHOUT BORDERS: no race, no challenge world, no MotoSkills, no
 * test ride). The reset timer must not already be running.
 *
 * PRACTICE (w-lb-markers; game/practice.h): also the local human's PRACTICE session while it races (activity state 2), any time (the
 * exhibition race's reset gate 0054e470, which takes the RESET key only while the rider is down, is the RESET key's alone); the respawn
 * then invalidates the lap in progress and the timing restarts from the finish line (rx_lap_log_marker; the driver is placed on the track
 * at the marker after the reset: rx_practice_marker_settle in practice_session.inc).
 *
 * INSTANT (PORT ADDITION, w-markerbtns): rx_session_spawn_at_instant is the client's SPAWN MARKER respawn: the same gate and the same reset
 * steps, run at once instead of after the reset key's 0.5 s white fade (see the function). rx_session_spawn_at stays the reset key's path
 * (the oracle: core/tools/spawn_marker_test.c compares the two). */
#include "../game/race.h"
#include "../game/player_reset_gate.h"
#include "../game/practice.h"
#include "../game/loop_player.h"
static rx_addr lap_log_driver(void);   /* practice_session.inc */

/* a refusal says why in last_error WITHOUT putting the session into the error state (session_error does: these are ordinary answers) */
static void spawn_note(struct rx_session* s, const char* msg) { snprintf(s->error, sizeof s->error, "%s", msg); }

/* the gate both respawns share (the reset key's conditions; see the header): 0 go (*player, *practice_driver, the unit heading d), else the
 * answer (1 / 2 / 3, last_error says why) */
static int spawn_gate(struct rx_session* s, const float pos[3], const float dir[3], rx_addr* player, rx_addr* practice, float d[3]) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR) return 1;
    if (!pos || !dir || !isfinite(pos[0]) || !isfinite(pos[1]) || !isfinite(pos[2]) || !isfinite(dir[0]) || !isfinite(dir[1]) || !isfinite(dir[2])) {
        spawn_note(s, "spawn marker: a position and a direction are needed");
        return 2;
    }
    d[0] = dir[0]; d[1] = 0.0f; d[2] = dir[2];
    const float len = sqrtf(d[0] * d[0] + d[2] * d[2]);
    if (len < 1e-4f) { spawn_note(s, "spawn marker: no horizontal direction"); return 2; }
    d[0] /= len; d[2] /= len;
    const rx_addr p = local_player();
    if (!p || U32(p, 0x1ac) == 0) { spawn_note(s, "spawn marker: no vehicle"); return 2; }
    const rx_addr drv = U32(p, 0x94);
    /* PRACTICE (w-lb-markers): the local human's driver of a practice lap race */
    const rx_addr practice_driver = rx_practice_applies(drv) ? lap_log_driver() : 0;
    if (!practice_driver && !rx_frb_free_ride_human(p)) { spawn_note(s, "spawn marker: only in a Free Ride or PRACTICE"); return 2; }
    /* Player_HandleResetInput's conditions for the reset key: a controller that is not paused or already resetting, armed (+0x1134), the
     * driver not finished (+0xfe8), and 0054e470 (the activity racing; the Free Ride's flag at [0xa93fcc] + 0x298 clear) */
    if (I32(p, 0x1f0) < 0 || U32(drv, 0xfe8) != 0 || U8(p, 0x21c) != 0 || U32(p, 0x1134) == 0) { spawn_note(s, "spawn marker: not now"); return 3; }
    if (practice_driver) {   /* PRACTICE: while racing (InProgress), the driver not finished */
        if (U32(drv, 0x21c) != 2 || U32(practice_driver, 0x70) != 0 || U32(0, 0x00a942b4u) == 2) { spawn_note(s, "spawn marker: not now"); return 3; }
    } else if (rx_player_reset_input_allowed(p) == 0 || U32(0, 0x00a942b4u) == 2) { spawn_note(s, "spawn marker: not now"); return 3; }
    if (F32(p, 0x116c) > 0.0f) { spawn_note(s, "spawn marker: a reset is already running"); return 3; }
    *player = p; *practice = practice_driver;
    return 0;
}

int rx_session_spawn_at(struct rx_session* s, const float pos[3], const float dir[3]) {
    rx_addr p = 0, practice_driver = 0; float d[3];
    const int rc = spawn_gate(s, pos, dir, &p, &practice_driver, d);
    if (rc) return rc;
    const rx_addr sp0 = rx_sp(), S = (sp0 - 0x40u) & ~0xfu;
    rx_set_sp(S);
    SF32(S, 0x20, pos[0]); SF32(S, 0x24, pos[1]); SF32(S, 0x28, pos[2]); SF32(S, 0x2c, 1.0f);   /* the position (a vec4) */
    SF32(S, 0x10, d[0]);   SF32(S, 0x14, 0.0f);   SF32(S, 0x18, d[2]);   SF32(S, 0x1c, 0.0f);   /* the forward direction */
    rx_player_reset_to(p, S + 0x20, S + 0x10);
    SU32(p, 0x1170, 1);   /* the reset reason: manual, as the key sets it */
    rx_set_sp(sp0);
    if (practice_driver && F32(p, 0x116c) > 0.0f) rx_lap_log_marker(practice_driver);   /* PRACTICE: the lap is the marker's (w-lb-markers) */
    return 0;
}

/* PORT ADDITION (w-markerbtns): the INSTANT respawn. The same gate and the same steps as rx_session_spawn_at, without the wait: where the
 * reset key's path holds the rider PlayerReset\FadeOutTime (MXRegistry.xml 0.5 s, [0x9c5424]) behind the white viewport fade before
 * MXPlayer_Update 0054af90 runs Player_Reset at the end of the timer, this runs Player_ResetTo's steps for the spot and then Player_Reset at
 * once, exactly as Player_ResetTo 005501f0 itself does for a player without a viewport ("others finish at once"): the spot to +0x1180 /
 * +0x1190, the fade hold (0054b720: the vehicle's slot 0x80 off, the rider's wreck bits cleared), the reset timer started, the reason (manual),
 * then Player_Reset 0054fff0 (flag 0): the timer cleared, the trick controls, the respawn fade (the game's ghost time, InvulnerableTime), the
 * wreck, the crash ended, the input groups back, the vehicle placed upright at the spot with its velocity gone (00460020). Only the white
 * viewport fade (00550280) and the 0.5 s wait are left out. The RESET key, the track resets and rx_session_spawn_at keep the game's timing.
 * PRACTICE: the lap is the marker's as with rx_session_spawn_at (rx_lap_log_marker before Player_Reset: the timer runs at that moment). */
int rx_session_spawn_at_instant(struct rx_session* s, const float pos[3], const float dir[3]) {
    rx_addr p = 0, practice_driver = 0; float d[3];
    const int rc = spawn_gate(s, pos, dir, &p, &practice_driver, d);
    if (rc) return rc;
    const rx_addr act = U32(p, 0x94);   /* Player_ResetTo's own gate: the activity running (state 2 / 3) */
    if (!act || (U32(act, 0x21c) != 2 && U32(act, 0x21c) != 3)) { spawn_note(s, "spawn marker: not now"); return 3; }
    const rx_addr sp0 = rx_sp(), S = (sp0 - 0x40u) & ~0xfu;
    rx_set_sp(S);
    SF32(p, 0x1180, pos[0]); SF32(p, 0x1184, pos[1]); SF32(p, 0x1188, pos[2]);   /* Player_ResetTo: the point (12 bytes each) */
    SF32(p, 0x1190, d[0]);   SF32(p, 0x1194, 0.0f);   SF32(p, 0x1198, d[2]);
    rx_player_set_fade_hold(p);                       /* 0054b720, as Player_ResetTo begins the reset */
    SF32(p, 0x116c, F32(0, 0x9c5424));                /* the reset timer, as Player_ResetTo starts it (Player_Reset clears it) */
    SU32(p, 0x1170, 1);                               /* the reset reason: manual, as the key sets it */
    if (practice_driver) rx_lap_log_marker(practice_driver);   /* PRACTICE: the lap is the marker's (w-lb-markers) */
    rx_player_reset(p, 0);                            /* 0054fff0: what the end of the timer runs */
    rx_set_sp(sp0);
    return 0;
}

int rx_session_spawn_pose(struct rx_session* s, float pos[3], float dir[3]) {
    if (!session_ok(s) || s->state == RX_SESSION_ERROR || !pos || !dir) return 1;
    const rx_addr p = local_player();
    const rx_addr v = p ? U32(p, 0x1ac) : 0;
    if (!v) return 2;
    rx_bike_state b; memset(&b, 0, sizeof b);
    if (rx_session_bike(s, &b) != 0) return 2;
    pos[0] = b.position[0][0]; pos[1] = b.position[0][1]; pos[2] = b.position[0][2];
    const float x = b.orientation[0][0], y = b.orientation[0][1], z = b.orientation[0][2], w = b.orientation[0][3];
    const float fx = 2.0f * (x * z + w * y), fz = 1.0f - 2.0f * (x * x + y * y);   /* the body's +Z axis (the heading, as the host's q * forward) */
    const float l = sqrtf(fx * fx + fz * fz);
    dir[0] = l > 1e-4f ? fx / l : 0.0f; dir[1] = 0.0f; dir[2] = l > 1e-4f ? fz / l : 1.0f;
    return isfinite(pos[0] + pos[1] + pos[2]) ? 0 : 2;
}
