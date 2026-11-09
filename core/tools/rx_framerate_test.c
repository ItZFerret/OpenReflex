/* rx_framerate_test.c -- frame-rate dependence of the product session (w-latency, 2026-10-28).
 *   rx_framerate_test <install root> <mode> <out.csv> [location] [seconds] [offset]
 *     mode: "dt:<seconds>"            one rx_session_advance_dt(dt) per host frame (1/144, 1/60, 1/40, 1/30 ...)
 *           "merged:<n>"              the C# bridge's merged job: one advance_dt(n / 60) per host frame (n = 2, 3)
 *           "jitter:<lo>:<hi>"        a host whose frame time alternates lo, hi (seconds): a 30-40 fps player
 *           "fixed60:<host dt>"       the parity fix: host frames of <host dt>, the game fed whole 1/60 s frames from an accumulator
 *                                     (each one advance_dt(1/60) with that host frame's input), at most 6 per host frame
 * The load and the countdown run at 1/60 s in every mode (same grid slot); the mode applies from the first RACING frame.
 * Loads a one-rider event (default National NAT_Concord), waits for RACING, then drives pad 0 with a script keyed to the GAME's
 * clock since the first RACING frame (so every mode gets the same input at the same game time, quantised to its frames):
 * RX_FR_SCRIPT=whip: RT 255, the same left turn, then over the big jump (both wheels off after 6.2 s = take-off T,
 *   as the host saw it) a full lateral whip RX +32767 [T+0.05, T+0.05+H), reversed RX -32767 [T+0.05+H, T+0.05+2H), released
 *   (H = RX_FR_WHIP_HOLD, 0.3 s; RX_FR_WHIP_AMP the amplitude; RX_FR_TURN=0 no left turn; RX_FR_AFTER the take-off search start). Default script:
 *   RT 255 throughout except the turns (RT 200): left LX -16384 [3, 5), right LX +12000 [7.6, 9.6); rider lean RY +32767
 *   [5.6, 6.6) (into / over the big jump at about 6.0-7.5 s on NAT_Concord), RX +32767 [6.6, 6.9), RY -32767 [6.9, 7.4); later
 *   RY +32767 [10, 11), RX -32767 [11.5, 12.5). Optional [offset]: seconds added to the script clock (ensembles).
 * RX_FR_SWITCH=<s>: every mode runs 1/60 s frames until <s> of script time (the state at the switch is the same in every mode);
 * RX_FR_TAKEOFF=<s>: the whip's take-off time fixed instead of detected; the ensemble offset then shifts only the whip's edges.
 * Per host frame the CSV has the game clock, the engine dt, the vehicle scheduler's steps / lead (S+b4 / S+c0), body 0 / rear wheel
 * (body 3) raw positions and velocities, body 0 orientation and angular velocity (S+730 / S+740), the input block (S+1c..), contacts.
 * Read-only: the harness only calls the public API and reads guest memory. Link: the product objects + this file (rx_framerate_test.sh). */
#include "rx_api.h"
#include "../src/rx_mem.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static double g_offset;   /* seconds added to the script's clock (ensemble runs: the same ride with the input edges shifted) */
static double g_takeoff = NAN;   /* whip: the game time (script clock) of the first host frame with both wheels off after 6.2 s */
static double g_switch;          /* RX_FR_SWITCH: script seconds before which every mode runs 1/60 s frames (the same take-off state) */
static double g_ramp, g_poll_hz, g_poll_phase;   /* RX_FR_WHIP_RAMP / RX_FR_POLL_HZ / RX_FR_POLL_PHASE (see script_input) */
static double g_hold = 0.30;      /* RX_FR_WHIP_HOLD: seconds each whip direction is held */
static int g_whip_amp = 32767;   /* RX_FR_WHIP_AMP: the whip's stick amplitude */
static double g_after = 6.2;      /* RX_FR_AFTER: the whip's take-off is the first both-wheels-off frame after this script time */
static int g_turn = 1;           /* RX_FR_TURN=0: the whip ride without the left turn (straight from the gate) */
static int g_whip;        /* RX_FR_SCRIPT=whip: the whip reversal over the big jump instead of the lean pulses */
static double script_input(double t, rx_pad* p) {
    memset(p, 0, sizeof *p);
    if (t < 0) return 0;
    t += g_offset;
    p->right_trigger = 255;
    if (g_whip) {   /* full lateral whip once airborne, reversed mid-air, released (keyed to the take-off the host saw) */
        const double tr = t - g_offset;   /* the ride before the jump does not move with the ensemble offset */
        if (g_turn && tr >= 3.0 && tr < 5.0) { p->right_trigger = 200; p->lx = -16384; }
        if (!isnan(g_takeoff)) {
            /* RX_FR_POLL_HZ: the stick as a device polled at that rate on its own clock (phase RX_FR_POLL_PHASE) would report it: the
             * value of the last poll at or before this frame (Unity's Input System polls XInput at 60 Hz by default) */
            const double ts = g_poll_hz > 0 ? floor((tr - g_poll_phase) * g_poll_hz) / g_poll_hz + g_poll_phase : tr;
            const double u = ts - g_offset - g_takeoff;   /* the ensemble offset shifts only the whip's edges (earlier by -offset) */
            /* RX_FR_WHIP_RAMP: a thumb's travel time (s) from centre to full and full to full (twice that) instead of a step */
            const double e1 = 0.05, e2 = 0.05 + g_hold, e3 = 0.05 + 2 * g_hold, k = g_ramp > 0 ? 1.0 / g_ramp : 1e9;
            double v = 0;
            if (u >= e1 && u < e2) v = fmin(1.0, (u - e1) * k);
            else if (u >= e2 && u < e3) v = fmax(-1.0, 1.0 - (u - e2) * k);
            else if (u >= e3) v = fmin(0.0, -1.0 + (u - e3) * k);
            p->rx = (int16_t)lrint(v * g_whip_amp);
        }
        return 1;
    }
    if (t >= 3.0 && t < 5.0) { p->right_trigger = 200; p->lx = -16384; }          /* left turn */
    if (t >= 5.6 && t < 6.6) p->ry = 32767;                                          /* lean back into / over the big jump (6.0-7.5 s) */
    if (t >= 6.9 && t < 7.4) p->ry = -32767;                                         /* lean forward in the air */
    if (t >= 6.6 && t < 6.9) p->rx = 32767;                                          /* lean right in the air (whip) */
    if (t >= 7.6 && t < 9.6) { p->right_trigger = 200; p->lx = 12000; }             /* right turn */
    if (t >= 10.0 && t < 11.0) p->ry = 32767;
    if (t >= 11.5 && t < 12.5) p->rx = -32767;
    return 1;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: rx_framerate_test <install> <mode> <out.csv> [location] [seconds] [offset]\n"); return 2; }
    const char* mode = argv[2];
    const char* loc = argc > 4 ? argv[4] : "NAT_Concord";
    const double seconds = argc > 5 ? atof(argv[5]) : 14.5;
    g_offset = argc > 6 ? atof(argv[6]) : 0.0;
    g_whip = getenv("RX_FR_SCRIPT") && !strcmp(getenv("RX_FR_SCRIPT"), "whip");
    if (getenv("RX_FR_AFTER")) g_after = atof(getenv("RX_FR_AFTER"));
    if (getenv("RX_FR_TURN")) g_turn = atoi(getenv("RX_FR_TURN"));
    if (getenv("RX_FR_WHIP_RAMP")) g_ramp = atof(getenv("RX_FR_WHIP_RAMP"));
    if (getenv("RX_FR_POLL_HZ")) g_poll_hz = atof(getenv("RX_FR_POLL_HZ"));
    if (getenv("RX_FR_POLL_PHASE")) g_poll_phase = atof(getenv("RX_FR_POLL_PHASE"));
    if (getenv("RX_FR_WHIP_HOLD")) g_hold = atof(getenv("RX_FR_WHIP_HOLD"));
    if (getenv("RX_FR_WHIP_AMP")) g_whip_amp = atoi(getenv("RX_FR_WHIP_AMP"));
    g_switch = getenv("RX_FR_SWITCH") ? atof(getenv("RX_FR_SWITCH")) : 0.0;
    if (getenv("RX_FR_TAKEOFF")) g_takeoff = atof(getenv("RX_FR_TAKEOFF"));   /* a fixed take-off time instead of the detected one */
    double host_dt = 1.0 / 60.0, jit_lo = 0, jit_hi = 0; int merged = 0, fixed60 = 0;
    if (!strncmp(mode, "dt:", 3)) host_dt = atof(mode + 3);
    else if (!strncmp(mode, "merged:", 7)) { merged = atoi(mode + 7); host_dt = merged / 60.0; }
    else if (!strncmp(mode, "jitter:", 7)) { if (sscanf(mode + 7, "%lf:%lf", &jit_lo, &jit_hi) != 2) return 2; }
    else if (!strncmp(mode, "fixed60:", 8)) { fixed60 = 1; host_dt = atof(mode + 8); }
    else { fprintf(stderr, "bad mode %s\n", mode); return 2; }

    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec; rx_session_event_default(&spec);
    snprintf(spec.location, sizeof spec.location, "%s", loc);
    spec.riders = 1;
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    FILE* out = fopen(argv[3], "w");
    if (!out) return 1;
    fprintf(out, "host,t,gameTime,engineDt,steps,lead,state,inT,inBr,inSx,inLx,inLy,"
                 "p0x,p0y,p0z,v0x,v0y,v0z,q0x,q0y,q0z,q0w,w0x,w0y,w0z,p3x,p3y,p3z,v3x,v3y,v3z,front,rear,speed\n");
    uint8_t keys[256] = {0};
    rx_pad pads[4]; memset(pads, 0, sizeof pads);
    double t0 = NAN, acc = 0; long host = 0; int racing_frames = 0;
    for (long guard = 0; guard < 200000; guard++) {
        /* the load and the countdown at 1/60 s in every mode (the grid slot comes from the game's clock-seeded random draw: the same
         * gate for every mode); the mode's host frames from the first RACING frame on */
        const float gt = rx_session_game_time(s);
        const double t = isnan(t0) ? -1 : (double)gt - t0;
        double dt = isnan(t0) || (g_switch > 0 && t < g_switch) ? 1.0 / 60.0 : jit_lo > 0 ? (host & 1 ? jit_hi : jit_lo) : host_dt;
        script_input(t, &pads[0]);
        rx_session_set_input(s, keys, pads, 1u);
        int code = 0, runs = 1; float pass = (float)dt;
        if (fixed60 && !isnan(t0) && !(g_switch > 0 && t < g_switch)) { acc += dt; runs = (int)(acc / (1.0 / 60.0)); if (runs > 6) { runs = 6; acc = 0; } else acc -= runs * (1.0 / 60.0); pass = 1.0f / 60.0f; }
        for (int r = 0; r < runs && code == 0; r++) code = rx_session_advance_dt(s, pass);
        if (code) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); return 1; }
        host++;
        const rx_session_state st = rx_session_get_state(s);
        if (st == RX_SESSION_RACING && isnan(t0)) t0 = rx_session_game_time(s);
        if (isnan(t0)) continue;
        rx_vehicle_info vi; rx_onboard_state ob; memset(&ob, 0, sizeof ob);
        if (rx_session_vehicle_info(s, -1, &vi) != 0) continue;
        rx_session_onboard_state(s, &ob);
        const rx_addr S = vi.sim;
        const double tt = (double)rx_session_game_time(s) - t0;
        fprintf(out, "%ld,%.6f,%.6f,%.7f,%d,%.7f,%d,%.4f,%.4f,%.4f,%.4f,%.4f", host, tt, (double)rx_session_game_time(s),
                (double)rx_session_engine_dt(s), (int)I32(S, 0xb4), (double)F32(S, 0xc0), (int)st,
                (double)F32(S, 0x1c), (double)F32(S, 0x28), (double)F32(S, 0x2c), (double)F32(S, 0x34), (double)F32(S, 0x38));
        for (int k = 0; k < 3; k++) fprintf(out, ",%.6f", (double)F32(S, 0xf0 + 4 * k));
        for (int k = 0; k < 3; k++) fprintf(out, ",%.6f", (double)F32(S, 0x100 + 4 * k));
        for (int k = 0; k < 4; k++) fprintf(out, ",%.7f", (double)F32(S, 0x730 + 4 * k));
        for (int k = 0; k < 3; k++) fprintf(out, ",%.6f", (double)F32(S, 0x740 + 4 * k));
        for (int k = 0; k < 3; k++) fprintf(out, ",%.6f", (double)F32(S, 0xf0 + 0xa0 * 3 + 4 * k));
        for (int k = 0; k < 3; k++) fprintf(out, ",%.6f", (double)F32(S, 0x100 + 0xa0 * 3 + 4 * k));
        fprintf(out, ",%d,%d,%.5f\n", ob.front_grounded, ob.rear_grounded, (double)vi.speed);
        if (g_whip && isnan(g_takeoff) && tt > g_after && ob.contacts_valid && !ob.front_grounded && !ob.rear_grounded) g_takeoff = tt;
        racing_frames++;
        if (tt > seconds) break;
    }
    fclose(out);
    printf("%s %s: %d racing host frames, %ld host frames total\n", mode, loc, racing_frames, host);
    rx_session_destroy(s);
    return 0;
}
