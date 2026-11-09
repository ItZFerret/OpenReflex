/* rx_input_clock_test.c -- product test of the session's input and clock path (core/notes/input-clock.md).
 *   rx_input_clock_test <install root>
 *
 * Creates a session and loads the reference event, then:
 *  A. (public API) rx_session_set_input / rx_session_set_mouse, one rx_session_advance_dt(1/60): the game's own records (the input manager's
 *     pad, keyboard and mouse records, Engine::UpdateTiming's fields) are compared with values computed from the documented mapping.
 *  B. (the frame head, internal) several frames of varying input and real time through the same functions the frame runs first
 *     (rx_host_clock_advance + rx_engine_frame_head): edges, held times, dead zone, the frame cap's busy wait (dt shorter than the cap),
 *     the long-frame clamp, the pace / alpha helpers.
 * The product session does not run past its first load frames yet (it stops at an unported game function; the test says where): B does not
 * need MXGame::Update. Exit 0 = every check passed (a stop of the session after A is reported, not a failure), 1 = a check failed. */
#include "rx_api.h"
#include "../game/rx_game.h"
#include "../game/loop_hostinput.h"
#include "../game/loop_engine.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail, g_checks;
#define CHECK(cond, ...) do { g_checks++; if (!(cond)) { g_fail++; printf("  FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)
static uint32_t fbits(float f) { uint32_t b; memcpy(&b, &f, 4); return b; }
#define CHECKF(got, want, what) CHECK(fbits(got) == fbits(want), "%s: %.9g (%08x), expected %.9g (%08x)", what, (double)(got), fbits(got), (double)(want), fbits(want))

#define ENGINE 0x00d728d0u
static rx_addr im(void) { return U32(U32(0, 0x00a95844), 0x34); }
static rx_addr pad0(void) { return im() + 4; }
static rx_addr kbd(void) { return im() + 0xb74; }
static rx_addr mouse(void) { return im() + 0x3234; }
static rx_addr key_rec(uint32_t dik) { return kbd() + 0x18u * (U8(0, 0x00a95848 + dik) - 0x23u); }

/* the game's stick axis (008156e0) outside the dead zone 0x1eb8, scaled by 1 / 24903 */
static float axis(int v) {
    const float k = 4.0155803e-05f;
    if (v > 0x1eb8) return (float)(v - 0x1eb8) * k;
    if (v < -0x1eb8) return ((float)(v + 0x1eb8) + 1.0f) * k;
    return 0.0f;
}

int main(int argc, char** argv) {
    if (argc < 2) { fprintf(stderr, "usage: rx_input_clock_test <install root>\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    const int loaded = rx_session_event_load(s, &spec) == 0;
    if (!loaded) printf("event load stops: %s\n  (the product's load chain, not the input / clock path: A is skipped, B runs on the created session)\n", rx_session_last_error(s));
    printf("session ready; input manager %08x, engine step %.9g s\n", im(), (double)rx_session_step_dt(s));
    CHECK(im() != 0, "no input manager");
    CHECKF(rx_session_step_dt(s), 0.016666668f, "frame cap (Engine::vf0(60.0), 0x3c888889)");
    CHECKF(F32(ENGINE, 0x48), 0.1f, "DeltaSecondsClamp (Registry, 0.1 s)");
    CHECK(U32(ENGINE, 0x84) != 0, "the FPS display's registry id");
    /* the constructor's ring of frame times (+0x24..+0x34, index +0x20) and their sum (+0x38): the first frames' mean is warmed up by them */
    const float ring_sum0 = F32(ENGINE, 0x38), ring_old0 = F32(ENGINE, 0x24 + 4 * U32(ENGINE, 0x20));
    CHECK(rx_session_alpha(s) == 0.0f, "alpha before pace");

    /* ---- A: the public path ---- */
    printf("A. set_input / set_mouse / advance_dt(1/60)%s\n", loaded ? "" : ": skipped");
    uint8_t keys[256] = {0};
    keys[0x11] = 0x80;   /* W */
    keys[0x1e] = 0x80;   /* A */
    keys[0xc8] = 0x01;   /* UP: a byte without the high bit is not down */
    rx_pad pads[4];
    memset(pads, 0, sizeof pads);
    pads[0].buttons = 0x1100;            /* A + left bumper */
    pads[0].left_trigger = 200; pads[0].right_trigger = 20;   /* RT below the threshold 30 */
    pads[0].lx = 20000; pads[0].ly = -12000; pads[0].rx = 4096; pads[0].ry = -9000;   /* RX inside the dead zone */
    CHECK(rx_session_set_input(s, keys, pads, 1) == 0, "set_input");
    if (loaded) CHECK(rx_session_set_mouse(s, 30, -10, 0, 1) == 0, "set_mouse: %s", rx_session_last_error(s));
    if (loaded) {
        CHECK(U32(mouse(), 0xfc) == 30 && I32(mouse(), 0x100) == -10, "mouse sums before the frame: %d %d", I32(mouse(), 0xfc), I32(mouse(), 0x100));
        CHECK(U32(mouse(), 0x118) == 1 && U32(mouse(), 0xf8) == 1, "mouse button sums %x held %x", U32(mouse(), 0x118), U32(mouse(), 0xf8));
    }

    const float dt_in = 1.0f / 60.0f;
    const int rc = loaded ? rx_session_advance_dt(s, dt_in) : 1;
    if (loaded) printf("  advance_dt -> %d, state %d%s%s\n", rc, (int)rx_session_get_state(s), rc ? ", " : "", rc ? rx_session_last_error(s) : "");
    CHECK(!loaded || rc == 0, "the first frame must run");
    if (loaded && rc == 0) {
        /* the clock: ticks = ceil(dt * 1e7) = 166667; the first dt measured from the startup stamp (0) */
        const float t0 = (float)(166667.0 / 10000000.0);
        CHECKF(F32(ENGINE, 0x24), t0, "frame time ring[0]");
        const float dt0 = ((ring_sum0 - ring_old0) + t0) * 0.2f;   /* UpdateTiming: sum = sum - ring[i] + t; dt = sum * 0.2 (the original's smoothing) */
        CHECKF(F32(ENGINE, 0x18), dt0, "dt after the 5-frame mean");
        CHECKF(rx_session_engine_dt(s), dt0, "rx_session_engine_dt");
        CHECKF(F32(ENGINE, 0x3c), dt0, "game time");
        CHECKF(rx_session_game_time(s), dt0, "rx_session_game_time");
        CHECK(rx_session_frame_dt(s) == dt_in, "frame_dt");

        /* the pad (record = manager + 4) */
        const rx_addr p = pad0();
        CHECK(U32(p, 0x29c) == 1 && U32(p, 0x2a0) == 1, "pad connected %u / this frame %u", U32(p, 0x29c), U32(p, 0x2a0));
        CHECK(U16(p, 0x244) == 0x1100 && U16(p, 0x246) == 0x1100 && U16(p, 0x260) == 0, "buttons %04x pressed %04x released %04x", U16(p, 0x244), U16(p, 0x246), U16(p, 0x260));
        CHECK(U32(p, 0x1b8) == 1, "left trigger held");
        CHECKF(F32(p, 0x1c0), (float)200 * 0.003921569f, "left trigger value");
        CHECK(U32(p, 0x1d0) == 0 && F32(p, 0x1d8) == 0.0f, "right trigger (20 < 30) is not held");
        CHECK(U32(p, 0x248) == 1 && U32(p, 0x24c) == 0, "trigger edges %u %u", U32(p, 0x248), U32(p, 0x24c));
        CHECKF(F32(p, 0x2b0), axis(20000), "left stick x");
        CHECKF(F32(p, 0x2b4), axis(-12000), "left stick y");
        CHECKF(F32(p, 0x2a8), axis(4096), "right stick x (dead zone)");
        CHECKF(F32(p, 0x2ac), axis(-9000), "right stick y");
        CHECK(U32(p, 0x258) == 1 && U32(p, 0x25c) == 1 && U32(p, 0x250) == 0 && U32(p, 0x254) == 1, "left-the-centre flags LX %u LY %u RX %u RY %u", U32(p, 0x258), U32(p, 0x25c), U32(p, 0x250), U32(p, 0x254));
        CHECK(U32(p + 0xc4, 0) == 2 && U32(p + 0xc4, 4) == 1, "button A (0x1000) record down + pressed");
        CHECK(U32(p + 0x124, 0) == 2 && U32(p + 0x124, 4) == 1, "left bumper (0x100) record down + pressed");
        CHECK(U32(p + 0x13c, 0) != 2, "right bumper record not down");
        CHECK(U32(im() + 4 + 0x2dc, 0x29c) == 0 && U32(im() + 4 + 0x2dc, 0x2a0) == 0, "pad 1 is not connected");

        /* the keyboard */
        const rx_addr kw = key_rec(0x11), ka = key_rec(0x1e), ku = key_rec(0xc8), ks = key_rec(0x1f);
        CHECK(kw == kbd() + 0x18 * (0x39 - 0x23), "W is the game's key id 0x39");
        /* a key record: +0 the edge this frame (2 pressed, 1 released), +4 the level (1 down), +0xc value */
        CHECK(U32(kw, 0) == 2 && U32(kw, 4) == 1 && F32(kw, 0xc) == 1.0f, "W down: edge %u level %u value %g", U32(kw, 0), U32(kw, 4), (double)F32(kw, 0xc));
        CHECK(U32(ka, 0) == 2 && U32(ka, 4) == 1, "A down");
        CHECK(U32(ku, 4) == 0 && U32(ks, 4) == 0, "UP (0x01 without the high bit) and S are up");
        CHECK(U32(kbd(), 0x990) == 1 && U32(kbd(), 0x994) == 1, "keyboard ok / a key down");

        /* the mouse: consumed by Mouse_Update (sums cleared), buttons and axes the game's */
        CHECK(U32(mouse(), 0xfc) == 0 && U32(mouse(), 0x100) == 0 && U32(mouse(), 0x118) == 0, "the frame consumed the mouse sums");
        CHECK(U32(mouse(), 0x34) == 2 && U32(mouse(), 0x38) == 1, "left button record down + pressed");
        {
            const float dt = F32(ENGINE, 0x18), k = 0.033333335f / dt, x = (float)30 * 0.00078125001f, sens = F32(0, 0x009cb7b0);
            float ax = k * x * sens;
            if (ax >= 1.0f) ax = 1.0f; else if (-1.0f >= ax) ax = -1.0f;
            CHECKF(F32(mouse(), 0x10), ax, "RiderReflexMouseX");
            CHECK(U8(mouse(), 0x128) == 1 && U8(mouse(), 0x132) == 1, "mouse moved flags");
        }
    }

    /* ---- the pace helpers (no game frame) ---- */
    printf("pace\n");
    {
        const float step = rx_session_step_dt(s);
        CHECK(rx_session_pace(s, 0.010f, 8) == 0, "10 ms: no step yet");
        CHECK(fabsf(rx_session_alpha(s) - 0.010f / step) < 1e-6f, "alpha %g", (double)rx_session_alpha(s));
        CHECK(rx_session_pace(s, 0.010f, 8) == 1, "20 ms: one step");
        CHECK(fabsf(rx_session_alpha(s) - (0.020f - step) / step) < 1e-5f, "alpha after a step %g", (double)rx_session_alpha(s));
        CHECK(rx_session_pace(s, 1.0f, 4) == 4, "1 s with at most 4 steps");
        CHECK(rx_session_alpha(s) >= 0.0f && rx_session_alpha(s) < 1.0f, "alpha range after a backlog %g", (double)rx_session_alpha(s));
        CHECK(rx_session_pace(s, -1.0f, 4) == 0 && rx_session_pace(s, NAN, 4) == 0, "negative / NaN time adds nothing");
    }

    if (loaded && rx_session_get_state(s) == RX_SESSION_ERROR) {
        printf("the session stopped after A: %s\n", rx_session_last_error(s));
        printf("B needs the session's game memory only (it does not run MXGame::Update): it runs on the stopped session\n");
    }

    /* ---- B: frames of the head only: varying input and real time (the game's own poll, no MXGame::Update) ---- */
    printf("B. frame head: edges, held times, the frame cap, the clamp\n");
    {
        const rx_addr p = pad0();
        /* a clean start: nothing held for one frame, then W / A and the pad go down: edges are fresh whatever A did */
        { uint8_t none[256] = {0}; rx_pad zero[4]; memset(zero, 0, sizeof zero); rx_session_set_input(s, none, zero, 1); rx_host_clock_advance(166667); rx_engine_frame_head(ENGINE); }
        rx_session_set_input(s, keys, pads, 1);
        rx_host_clock_advance(166667);
        rx_engine_frame_head(ENGINE);
        CHECK(U32(key_rec(0x11), 0) == 2 && U32(key_rec(0x11), 4) == 1, "W pressed: edge %u level %u", U32(key_rec(0x11), 0), U32(key_rec(0x11), 4));
        CHECK(U16(p, 0x246) == 0x1100 && U32(p, 0x258) == 1 && U32(p, 0x248) == 1, "pad edges after the clean start");
        /* the raw mouse path without a session call: motion -2, 5 (down), the middle button, a wheel notch; the frame consumes them */
        const uint32_t held_before = U32(mouse(), 0xf8);   /* the left button A pressed stays held */
        rx_host_mouse_post(-2, 5, 120, 0x10);
        CHECK(I32(mouse(), 0xfc) == -2 && I32(mouse(), 0x100) == 5 && U32(mouse(), 0x118) == 0x4 && (int16_t)U16(mouse(), 0x104) == 120, "raw mouse sums");
        rx_host_clock_advance(166667);
        rx_engine_frame_head(ENGINE);
        CHECK(U32(mouse(), 0xfc) == 0 && U32(mouse(), 0x118) == 0 && U32(mouse(), 0xf8) == (held_before | 0x4), "mouse sums consumed, middle button held (0x%x)", U32(mouse(), 0xf8));
        CHECK(U32(mouse(), 0xc4) == 2, "wheel up record down");
        /* the same input again: W stays down without a new press edge; the pad's buttons are held, not pressed again; the left
         * button record's down time grows by the game's dt */
        const float a_time = F32(p + 0xc4, 0x10);
        rx_host_clock_advance(166667);
        rx_engine_frame_head(ENGINE);
        const float dt2 = F32(ENGINE, 0x18);
        CHECK(U32(key_rec(0x11), 0) == 0 && U32(key_rec(0x11), 4) == 1, "W held: edge %u level %u", U32(key_rec(0x11), 0), U32(key_rec(0x11), 4));
        CHECK(U16(p, 0x246) == 0 && U16(p, 0x244) == 0x1100, "buttons held, no new press");
        CHECKF(F32(p + 0xc4, 0x10), a_time + dt2, "button A down time grew by the frame's dt");

        /* release W, release A button, centre the left stick; press D */
        keys[0x11] = 0; keys[0x20] = 0x80;
        pads[0].buttons = 0x0100; pads[0].lx = 100; pads[0].ly = 0;
        rx_session_set_input(s, keys, pads, 1);
        rx_host_clock_advance(166667);
        rx_engine_frame_head(ENGINE);
        CHECK(U32(key_rec(0x11), 0) == 1 && U32(key_rec(0x11), 4) == 0, "W released");
        CHECK(U32(key_rec(0x20), 0) == 2 && U32(key_rec(0x20), 4) == 1, "D pressed");
        CHECK(U16(p, 0x260) == 0x1000 && U16(p, 0x244) == 0x0100, "A released: word %04x held %04x", U16(p, 0x260), U16(p, 0x244));
        CHECK(U32(p, 0x274) == 1 && U32(p, 0x278) == 1, "left stick centred this frame (x %u y %u)", U32(p, 0x274), U32(p, 0x278));
        CHECKF(F32(p, 0x2b0), 0.0f, "left stick x in the dead zone");

        /* a pad that goes away */
        rx_session_set_input(s, keys, pads, 0);
        rx_host_clock_advance(166667);
        rx_engine_frame_head(ENGINE);
        CHECK(U32(p, 0x29c) == 0 && U32(p, 0x2a4) == 1, "pad 0 disconnected (lost this frame %u)", U32(p, 0x2a4));
        rx_session_set_input(s, keys, pads, 1);

        /* the frame cap: no real time passed -> the busy wait on the counter ends at the cap */
        const float t_before = F32(ENGINE, 0x3c);
        rx_host_clock_advance(0);
        rx_engine_frame_head(ENGINE);
        const float raw = F32(ENGINE, 0x24 + 4 * ((U32(ENGINE, 0x20) + 4) % 5));
        CHECK(raw >= 0.016666668f && raw < 0.0168f, "dt waited to the cap: %.9g", (double)raw);
        CHECK(F32(ENGINE, 0x3c) > t_before, "time advanced");

        /* a long frame: the game's DeltaSecondsClamp (0.1 s) after the mean of five */
        for (int i = 0; i < 5; i++) { rx_host_clock_advance(5000000); rx_engine_frame_head(ENGINE); }   /* 0.5 s each */
        CHECKF(F32(ENGINE, 0x18), 0.1f, "dt clamped");
    }

    printf("%d checks, %d failed\n", g_checks, g_fail);
    rx_session_destroy(s);
    return g_fail ? 1 : 0;
}
