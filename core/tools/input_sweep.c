/* input_sweep.c -- the input value pipeline of the product core, stage by stage (agent w-input, 2026-10-28).
 *   input_sweep <install root> [out.tsv]       env: RX_HAND (ControllerHandedness 0/1, default: the game's 1), RX_LOCATION (FR_Alpine),
 *                                                   RX_MOUSE_SENS (MouseSensitivity, default the game's 50)
 *
 * Loads a Free Ride, rides the local MX bike to a standstill and then feeds the session the way the Unity client does
 * (rx_session_set_input / _set_mouse) one control at a time, recording after the frame that consumed it:
 *   the game's pad record (post dead zone: rx_pad_axis 008156e0, triggers 00816400), the vehicle input block S+0x1c.. (what the physics
 *   reads: 0044c170 samples it into the control block) and the processed control block C = [S+0x5c30].
 * One line per sample:  <group> <label> <input...> | pad LX LY RX RY LT RT | block thr rev fbrk rbrk stx sty rfx rfy | C thr rawX rawY rfx rfy ang
 * Link: the product objects in core/build/obj, see input_sweep.sh. */
#include "rx_api.h"
#include "rx_ui_settings.h"
#include "../game/rx_game.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct rx_session* g_s;
static FILE* g_out;
static rx_addr g_S;

static rx_addr im(void) { return U32(U32(0, 0x00a95844), 0x34); }
static rx_addr pad0(void) { return im() + 4; }
static rx_addr mouse(void) { return im() + 0x3234; }
static int env_int(const char* n, int d) { const char* e = getenv(n); return e && *e ? atoi(e) : d; }

static uint8_t g_keys[256];
static rx_pad g_pad[4];
static int g_pad_mask = 1;

static int step(void) {
    rx_session_set_input(g_s, g_keys, g_pad, (uint32_t)g_pad_mask);
    return rx_session_advance_dt(g_s, 1.0f / 60.0f);
}
static void steps(int n) { for (int i = 0; i < n; i++) if (step() != 0) { fprintf(stderr, "advance failed: %s\n", rx_session_last_error(g_s)); exit(1); } }
static void neutral(void) { memset(g_keys, 0, sizeof g_keys); memset(g_pad, 0, sizeof g_pad); }

static void record(const char* group, const char* label, const char* input) {
    const rx_addr p = pad0(), C = U32(g_S, 0x5c30);
    fprintf(g_out, "%s\t%s\t%s\t|\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t|\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t|\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\t%.7g\n",
            group, label, input,
            (double)F32(p, 0x2b0), (double)F32(p, 0x2b4), (double)F32(p, 0x2a8), (double)F32(p, 0x2ac), (double)F32(p, 0x1c0), (double)F32(p, 0x1d8),
            (double)F32(g_S, 0x1c), (double)F32(g_S, 0x20), (double)F32(g_S, 0x24), (double)F32(g_S, 0x28),
            (double)F32(g_S, 0x2c), (double)F32(g_S, 0x30), (double)F32(g_S, 0x34), (double)F32(g_S, 0x38),
            (double)F32(C, 0x00), (double)F32(C, 0x68), (double)F32(C, 0x6c), (double)F32(C, 0x70), (double)F32(C, 0x74), (double)F32(C, 0x10));
}

/* transitions: per-frame values through every stage, including the lean outputs and the steering filter */
static void record_t(const char* group, const char* label, const char* input) {
    const rx_addr p = pad0(), C = U32(g_S, 0x5c30);
    fprintf(g_out, "%s\t%s\t%s\tpad %.7g %.7g %.7g %.7g\tblk stx %.7g sty %.7g rfx %.7g rfy %.7g\tC raw %.7g %.7g %.7g %.7g ang %.7g lean30 %.7g %.7g %.7g %.7g lean40 %.7g %.7g %.7g %.7g\tfilt steer %.7g reflexy %.7g\n",
            group, label, input,
            (double)F32(p, 0x2b0), (double)F32(p, 0x2b4), (double)F32(p, 0x2a8), (double)F32(p, 0x2ac),
            (double)F32(g_S, 0x2c), (double)F32(g_S, 0x30), (double)F32(g_S, 0x34), (double)F32(g_S, 0x38),
            (double)F32(C, 0x68), (double)F32(C, 0x6c), (double)F32(C, 0x70), (double)F32(C, 0x74), (double)F32(C, 0x10),
            (double)F32(C, 0x30), (double)F32(C, 0x34), (double)F32(C, 0x38), (double)F32(C, 0x3c),
            (double)F32(C, 0x40), (double)F32(C, 0x44), (double)F32(C, 0x48), (double)F32(C, 0x4c),
            (double)F32(g_S, 0x5c5c), (double)F32(g_S, 0x5c98));
}
static void set_axis(int which, int v) {
    switch (which) { case 0: g_pad[0].lx = (int16_t)v; break; case 1: g_pad[0].ly = (int16_t)v; break; case 2: g_pad[0].rx = (int16_t)v; break; default: g_pad[0].ry = (int16_t)v; break; }
}
/* hold `from` for 30 frames, then `to` for 30 frames; one line per frame around the switch */
static void transition(const char* name, int which, int from, int to) {
    char in[48]; snprintf(in, sizeof in, "%d->%d", from, to);
    neutral(); steps(30);
    set_axis(which, from); steps(30);
    record_t(name, "pre", in);
    set_axis(which, to);
    for (int f = 1; f <= 30; f++) { steps(1); char lab[16]; snprintf(lab, sizeof lab, "f%d", f); record_t(name, lab, in); }
}

/* at speed (throttle held): a lean/steer key or mouse or stick held, then released or reversed, one line per frame */
static void accelerate(void) { neutral(); g_pad[0].right_trigger = 255; steps(150); }
static void moving_case(const char* name, int kind, int a, int b) {
    /* kind 0 key a held 40 frames then key b (or none = 0) held 40 frames; kind 1 mouse dx a x20 frames then dx b x20 frames; kind 2 RX stick a then b */
    accelerate();
    rx_bike_state bs; memset(&bs, 0, sizeof bs); rx_session_bike(g_s, &bs);
    char in[64], lab[16];
    snprintf(in, sizeof in, "%s speed %.1f", name, (double)bs.speed);
    for (int ph = 0; ph < 2; ph++) {
        int v = ph == 0 ? a : b;
        for (int f = 1; f <= (ph == 0 ? 40 : 40); f++) {
            memset(g_keys, 0, sizeof g_keys); g_pad[0].right_trigger = 255; g_pad[0].rx = 0;
            if (kind == 0 && v) g_keys[v] = 0x80;
            if (kind == 1) rx_session_set_mouse(g_s, v, 0, 0, 0);
            if (kind == 2) g_pad[0].rx = (int16_t)v;
            steps(1);
            if (ph == 1 && f <= 14) { snprintf(lab, sizeof lab, "p2_f%d", f); record_t("MOVING", lab, in); }
            if (ph == 0 && f > 38) { snprintf(lab, sizeof lab, "p1_f%d", f); record_t("MOVING", lab, in); }
        }
    }
}

static int16_t clamp16(double v) { long r = lround(v); if (r > 32767) r = 32767; if (r < -32768) r = -32768; return (int16_t)r; }

static void settle(void) { neutral(); steps(40); }

/* one axis of a stick, hold n frames, record after the first and after the last frame */
static void axis_sweep(const char* name, int which, const int* values, int count, int hold) {
    for (int i = 0; i < count; i++) {
        neutral(); steps(25);
        int16_t v = (int16_t)values[i];
        switch (which) { case 0: g_pad[0].lx = v; break; case 1: g_pad[0].ly = v; break; case 2: g_pad[0].rx = v; break; default: g_pad[0].ry = v; break; }
        char in[32]; snprintf(in, sizeof in, "%d", v);
        steps(1); record(name, "f1", in);
        steps(hold - 1); record(name, "held", in);
    }
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: input_sweep <install root> [out.tsv]\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    g_out = argc > 2 ? fopen(argv[2], "w") : stdout;
    if (!g_out) return 2;
    g_s = rx_session_create(argv[1]);
    if (!g_s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    snprintf(spec.event_type, sizeof spec.event_type, "FreeRide");
    snprintf(spec.location, sizeof spec.location, "%s", getenv("RX_LOCATION") ? getenv("RX_LOCATION") : "FR_Alpine");
    spec.event_type_id = 9; spec.event_activation[0] = 0; spec.track_id = 0; spec.duration = 0; spec.duration_type[0] = 0; spec.riders = 1; spec.ai_difficulty = 0; spec.vehicle_class = -1;
    spec.location_id = 45;
    if (getenv("RX_HAND")) {
        int32_t before = -1;
        int rc = rx_session_ui_setting(g_s, "ControllerHandedness", env_int("RX_HAND", 1), 1, &before);
        fprintf(stderr, "ControllerHandedness before %d set %d rc %d\n", before, env_int("RX_HAND", 1), rc);
    }
    if (getenv("RX_MOUSE_SENS")) rx_session_ui_setting(g_s, "MouseSensitivity", env_int("RX_MOUSE_SENS", 50), 1, NULL);
    if (rx_session_event_load(g_s, &spec) != 0) { fprintf(stderr, "event_load: %s\n", rx_session_last_error(g_s)); return 1; }
    int lf = 0;
    while (rx_session_get_state(g_s) != RX_SESSION_RACING && lf < 10000) { neutral(); if (step() != 0) break; lf++; }
    if (rx_session_get_state(g_s) != RX_SESSION_RACING) { fprintf(stderr, "no racing state after %d frames: %s\n", lf, rx_session_last_error(g_s)); return 1; }
    rx_vehicle_info vi; memset(&vi, 0, sizeof vi);
    if (rx_session_vehicle_info(g_s, -1, &vi) != 0) { fprintf(stderr, "vehicle_info: %s\n", rx_session_last_error(g_s)); return 1; }
    g_S = vi.sim;
    {
        int32_t hand = -1, preset = -1, motion = -1, sens = -1;
        rx_session_ui_setting(g_s, "ControllerHandedness", 0, 0, &hand);
        rx_session_ui_setting(g_s, "ControllerPreset", 0, 0, &preset);
        rx_session_ui_setting(g_s, "ControllerMotion", 0, 0, &motion);
        rx_session_ui_setting(g_s, "MouseSensitivity", 0, 0, &sens);
        fprintf(g_out, "#meta\tload_frames %d\tvehicle type %d bodies %d\thandedness %d preset %d motion %d mouse_sensitivity %d (S=%.4g)\tstick_throttle_flag %u brake_hold %u\n",
                lf, vi.type, vi.bodies, hand, preset, motion, sens, (double)F32(0, 0x009cb7b0), U32(g_S, 0x44), U32(g_S, 0x54));
    }
    /* clear the brake-hold latch (a throttle sample), then stand still on the brakes */
    neutral(); steps(5);
    g_pad[0].right_trigger = 255; steps(8);
    g_pad[0].right_trigger = 0; g_pad[0].left_trigger = 255; steps(300);
    fprintf(g_out, "#meta\tafter_setup speed %.3f brake_hold %u\n", (double)vi.speed, U32(g_S, 0x54));
    settle();

    if (getenv("RX_DUMP")) {   /* the action objects behind the four sticks (bind tables) */
        const rx_addr blk = U32(0, 0x009c52c4u);
        const struct { const char* n; uint32_t off; } acts[] = { {"SteeringStickX", 0x19c4}, {"SteeringStickY", 0x1bc4}, {"RiderReflexStickX", 0x1f20}, {"RiderReflexStickY", 0x21d0}, {"Throttle", 0x348}, {"StickThrottleBrake", 0x127c} };
        for (unsigned a = 0; a < sizeof acts / sizeof *acts; a++) {
            fprintf(g_out, "#act %s @%08x vt %08x:", acts[a].n, blk + acts[a].off, U32(blk + acts[a].off, 0));
            for (uint32_t o = 4; o < 0xb8; o += 4) fprintf(g_out, " %x:%08x", o, U32(blk + acts[a].off, o));
            fprintf(g_out, "\n");
        }
        for (unsigned a = 0; a < 2; a++) for (int c = 0; c < 2; c++) {
            rx_addr ch = U32(blk + acts[a].off, 0xa4 + 4 * c);
            fprintf(g_out, "#child %s[%d] @%08x vt %08x:", acts[a].n, c, ch, U32(ch, 0));
            for (uint32_t o = 4; o < 0x60; o += 4) fprintf(g_out, " %x:%08x", o, U32(ch, o));
            fprintf(g_out, "\n");
        }
        {   /* the lag filters {value, time constant, factor} the input passes on the way: steering, brakes, reflex y, engine, air roll / pitch / yaw */
            const struct { const char* n; uint32_t off; } fl[] = { {"steer 5c5c", 0x5c5c}, {"front brake 5c34", 0x5c34}, {"rear brake 5c48", 0x5c48}, {"pitch 5c70", 0x5c70}, {"steer correction 5c84", 0x5c84},
                {"reflex y 5c98", 0x5c98}, {"engine 5cac", 0x5cac}, {"air roll 8b78", 0x8b78}, {"air pitch 8b8c", 0x8b8c}, {"air yaw 8ba0", 0x8ba0} };
            for (unsigned i = 0; i < sizeof fl / sizeof *fl; i++)
                fprintf(g_out, "#filter %s value %.6g time_constant %.6g factor %.6g\n", fl[i].n, (double)F32(g_S, fl[i].off), (double)F32(g_S, fl[i].off + 4), (double)F32(g_S, fl[i].off + 8));
            fprintf(g_out, "#params steer_rate_limit 100/s; brake release brake_time S+5cc4 %.6g; reflex coeffs S+5594 %.6g S+55dc %.6g S+5624 %.6g; air S+5680 %.6g %.6g %.6g\n", (double)F32(g_S, 0x5cc4), (double)F32(g_S, 0x5594), (double)F32(g_S, 0x55dc), (double)F32(g_S, 0x5624), (double)F32(g_S, 0x5680), (double)F32(g_S, 0x5684), (double)F32(g_S, 0x5688));
        }
        g_pad[0].ly = 32000; steps(3);
        fprintf(g_out, "#ly_with_LY=32000 pad_ly %.6f blk_sty %.6f\n", (double)F32(pad0(), 0x2b4), (double)F32(g_S, 0x30));
        fprintf(g_out, "#done\n"); if (g_out != stdout) fclose(g_out); rx_session_destroy(g_s); return 0;
    }
    if (getenv("RX_ONLY_MOVING")) {
        moving_case("LEFT_key_release", 0, 0xcb, 0);
        moving_case("LEFT_to_RIGHT_key", 0, 0xcb, 0xcd);
        moving_case("UP_key_release", 0, 0xc8, 0);
        moving_case("mouse_dx20_to_stop", 1, 20, 0);
        moving_case("mouse_dx20_to_dx-20", 1, 20, -20);
        moving_case("mouse_dx-20_to_dx20", 1, -20, 20);
        moving_case("RX_full_to_0", 2, 32767, 0);
        moving_case("RX_full_to_-full", 2, 32767, -32768);
        fprintf(g_out, "#done\n"); if (g_out != stdout) fclose(g_out); rx_session_destroy(g_s); return 0;
    }
    /* ---------------- sticks: per axis, fine around the dead zone, coarse elsewhere ---------------- */
    int vals[400], n = 0;
    for (int v = -32768; v <= 32767; v += 2048) vals[n++] = v;
    vals[n++] = 32767;
    for (int v = -8300; v <= 8300; v += 100) vals[n++] = v;
    int extra[] = { 7863, 7864, 7865, -7863, -7864, -7865, -7866, 1, -1, 100, -100, 32000, -32000, 16384, -16384, 24000, -24000, 30000, -30000 };
    for (unsigned i = 0; i < sizeof extra / sizeof *extra; i++) vals[n++] = extra[i];
    const char* names[4] = { "LX", "LY", "RX", "RY" };
    for (int a = 0; a < 4; a++) axis_sweep(names[a], a, vals, n, 20);

    /* ---------------- release and reversal, every stick axis, per frame ---------------- */
    {
        const char* nm[4] = { "LX", "LY", "RX", "RY" };
        for (int a = 0; a < 4; a++) {
            transition(nm[a], a, 32767, 0);  /* release */
            transition(nm[a], a, -32768, 0);
            transition(nm[a], a, 32767, -32768);  /* reversal */
            transition(nm[a], a, -32768, 32767);
            transition(nm[a], a, 16384, -16384);  /* half reversal */
            transition(nm[a], a, 20000, 9000);    /* partial return */
        }
        /* analog release written as a physical stick would travel: 32767 -> 0 over 3 frames, then -> -32768 over 3 frames */
        const char* nm2[2] = { "RX", "RY" };
        for (int a = 0; a < 2; a++) {
            neutral(); steps(30);
            int which = a + 2; char in[32]; snprintf(in, sizeof in, "ramp 32767->-32768 in 6 frames");
            set_axis(which, 32767); steps(30); record_t(nm2[a], "ramp_pre", in);
            for (int f = 1; f <= 6; f++) { set_axis(which, 32767 - (int)(f * 65535.0 / 6.0)); steps(1); char lab[16]; snprintf(lab, sizeof lab, "f%d", f); record_t(nm2[a], lab, in); }
            for (int f = 7; f <= 24; f++) { steps(1); char lab[16]; snprintf(lab, sizeof lab, "f%d", f); record_t(nm2[a], lab, in); }
        }
    }

    /* ---------------- radial sweeps ---------------- */
    const double mags[] = { 0.1, 0.25, 0.3, 0.5, 0.75, 1.0 };
    for (int stick = 0; stick < 2; stick++)
        for (unsigned m = 0; m < sizeof mags / sizeof *mags; m++)
            for (int deg = 0; deg < 360; deg += 15) {
                neutral(); steps(25);
                double x = cos(deg * 3.14159265358979 / 180.0) * mags[m], y = sin(deg * 3.14159265358979 / 180.0) * mags[m];
                int16_t ix = clamp16(x * 32767.0), iy = clamp16(y * 32767.0);
                if (stick == 0) { g_pad[0].lx = ix; g_pad[0].ly = iy; } else { g_pad[0].rx = ix; g_pad[0].ry = iy; }
                char in[64]; snprintf(in, sizeof in, "r%.2f_a%d(%d,%d)", mags[m], deg, ix, iy);
                steps(20); record(stick == 0 ? "LS_radial" : "RS_radial", "held", in);
            }

    /* ---------------- triggers (every byte), with the bike held by neither brake nor throttle first ---------------- */
    for (int side = 0; side < 2; side++)
        for (int b = 0; b <= 255; b++) {
            neutral(); steps(25);
            if (side == 0) g_pad[0].left_trigger = (uint8_t)b; else g_pad[0].right_trigger = (uint8_t)b;
            char in[16]; snprintf(in, sizeof in, "%d", b);
            steps(1); record(side == 0 ? "LT" : "RT", "f1", in);
            if (b % 8 == 0 || b == 255 || (b >= 28 && b <= 33)) { steps(9); record(side == 0 ? "LT" : "RT", "held", in); }
            if (side == 1 && b == 255) { steps(120); }
        }
    /* the bike has been given gas: stop it, so the keyboard section starts at standstill */
    neutral(); g_pad[0].left_trigger = 255; steps(400); settle();

    /* ---------------- keyboard: the game's key ids by DirectInput scan code ---------------- */
    struct { const char* name; int dik; } keys[] = { {"W", 0x11}, {"S", 0x1f}, {"A", 0x1e}, {"D", 0x20}, {"UP", 0xc8}, {"DOWN", 0xd0}, {"LEFT", 0xcb}, {"RIGHT", 0xcd}, {"SPACE", 0x39}, {"LSHIFT", 0x2a} };
    for (unsigned k = 0; k < sizeof keys / sizeof *keys; k++) {
        neutral(); steps(40);
        char lab[32];
        g_keys[keys[k].dik] = 0x80;
        for (int f = 1; f <= 90; f++) { steps(1); if (f <= 12 || f % 6 == 0) { snprintf(lab, sizeof lab, "down_f%d", f); record_t("KEY", lab, keys[k].name); } }
        g_keys[keys[k].dik] = 0;
        for (int f = 1; f <= 120; f++) { steps(1); if (f <= 12 || f % 6 == 0) { snprintf(lab, sizeof lab, "up_f%d", f); record_t("KEY", lab, keys[k].name); } }
        if (keys[k].dik == 0x11) { neutral(); g_pad[0].left_trigger = 255; steps(300); settle(); }
    }

    /* ---------------- mouse: constant counts per frame for 90 frames, then none ---------------- */
    const int mdx[] = { 4, 16, 40, 100 };
    for (unsigned i = 0; i < sizeof mdx / sizeof *mdx; i++) {
        for (int axis = 0; axis < 2; axis++) {
            neutral(); steps(120);
            char lab[32], in[32]; snprintf(in, sizeof in, "%s%+d/frame", axis ? "dy" : "dx", mdx[i]);
            for (int f = 1; f <= 60; f++) {
                rx_session_set_mouse(g_s, axis ? 0 : mdx[i], axis ? mdx[i] : 0, 0, 0);
                steps(1);
                if (f <= 8 || f % 6 == 0) { snprintf(lab, sizeof lab, "move_f%d", f); record_t("MOUSE", lab, in); fprintf(g_out, "#mouse_axes\tX %.7g\tY %.7g\n", (double)F32(mouse(), 0x10), (double)F32(mouse(), 0x28)); }
            }
            for (int f = 1; f <= 90; f++) { steps(1); if (f <= 8 || f % 10 == 0) { snprintf(lab, sizeof lab, "stop_f%d", f); record_t("MOUSE", lab, in); } }
        }
    }
    fprintf(g_out, "#done\n");
    if (g_out != stdout) fclose(g_out);
    rx_session_destroy(g_s);
    return 0;
}
