/* rng_frames.c -- the PRODUCT's random number streams at every frame end (agent w-rng; development only, not in librxcore).
 *
 *   rng_frames <install root> <frames> <out file> [spec [riders [ai]]]
 *     spec   type,typeid,location,locid,track,class,durtype,dur,activity (the baton boot's REFLEX_EVENT_SPEC format;
 *            default the session's reference event, National NAT_Concord 12 riders AI 95, 3 laps)
 *   env    RNG_SEED_SETUP=<hex>  both generators set to <hex> before the event load (the oracle boot's REFLEX_SEED_SETUP)
 *          RNG_SEED_PRE=<hex>    the CRT _holdrand set to <hex> at the start of every frame whose raw activity state is 0 (Pre and the
 *                                load frames: the oracle world test's REFLEX_SEED), so both sides enter the countdown on the same stream
 *          RNG_RESEED=1          from the countdown on, both generators are set at the start of every frame to a value of the frame's
 *                                index since the countdown (k): CRT k * 0x9e3779b9 + 1, game LCG k * 0x85ebca6b + 7 (the oracle probe
 *                                does the same, core/oracle/rng_probe.cpp): every frame's draw count is then measured from a known state
 *          RNG_HOLD_W=1          W from the first frame instead of the gate drop
 * Per frame one line (after the frame):
 *   F <session frame> st <raw activity state A+0x21c> k <frames since the countdown entry, -1 before> rs <CRT _holdrand> gs <g_GameRandSeed>
 *     gc <g_GameRandCount> [n <CRT draws since the frame start> m <LCG draws since the frame start>]   (n / m with RNG_RESEED)
 * The input rule is the oracle world test's (W held from the first frame of race state >= 2). core/tools/rng_cmp.py compares the file
 * with the oracle probe's (RX_RNG_PROBE) after aligning both on the countdown entry. */
#include "rx_api.h"
#include "rx_rng.h"
#include "crt.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef RX_RNG_TRACED
/* the traced build (a patched copy of the core, scratch only: every g_GameRandCount write and every crt_rand call reports the C function
 * that makes it): one "D <frame> <kind> <function> <count>" line per draw; frame -1 = the event load */
static FILE* g_out; static long g_frame = -1;
void rx_rng_note(const char* kind, const char* fn, uint32_t v) { if (g_out) fprintf(g_out, "D %ld %s %s %u\n", g_frame, kind, fn, v); }
#endif

static uint32_t holdrand(void) { const rx_addr ptd = crt_ptd(); return ptd ? U32(ptd, 0x14) : 0; }
static void set_holdrand(uint32_t v) { const rx_addr ptd = crt_ptd(); if (ptd) SU32(ptd, 0x14, v); }

/* steps of an LCG x -> x * a + c from `from` to `to` (bounded) */
static int steps(uint32_t from, uint32_t to, uint32_t a, uint32_t c) {
    for (int n = 0; n < 4000000; n++) { if (from == to) return n; from = from * a + c; }
    return -1;
}

static int raw_state(rx_session* s) {
    rx_race_state r;
    memset(&r, 0, sizeof r);
    if (rx_session_race(s, &r) != 0 || !r.valid) return -1;
    return r.state;
}

static int parse_spec(const char* sp, rx_event_spec* e) {
    char buf[256]; snprintf(buf, sizeof buf, "%s", sp);
    char* f[9]; int n = 0; char* p = buf;
    for (;;) { f[n++] = p; char* c = strchr(p, ','); if (!c || n == 9) break; *c = 0; p = c + 1; }
    if (n != 9) return -1;
    snprintf(e->event_type, sizeof e->event_type, "%s", f[0]); e->event_type_id = atoi(f[1]);
    snprintf(e->location, sizeof e->location, "%s", f[2]); e->location_id = atoi(f[3]); e->track_id = atoi(f[4]);
    e->vehicle_class = atoi(f[5]); snprintf(e->duration_type, sizeof e->duration_type, "%s", f[6]); e->duration = atoi(f[7]);
    return 0;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <install root> <frames> <out file> [spec [riders [ai]]]\n", argv[0]); return 2; }
    const int frames = atoi(argv[2]);
    FILE* out = fopen(argv[3], "w");
    if (!out) { perror(argv[3]); return 1; }
#ifdef RX_RNG_TRACED
    g_out = out;
#endif
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec spec;
    rx_session_event_default(&spec);
    if (argc > 4 && parse_spec(argv[4], &spec) != 0) { fprintf(stderr, "bad spec %s\n", argv[4]); return 2; }
    if (argc > 5) spec.riders = atoi(argv[5]);
    if (argc > 6) spec.ai_difficulty = atoi(argv[6]);
    if (getenv("RNG_SEED_SETUP")) {
        const uint32_t seed = (uint32_t)strtoul(getenv("RNG_SEED_SETUP"), NULL, 16);
        set_holdrand(seed); SU32(0, 0x009cb860, seed); SU32(0, 0x009cb864, 0);
    }
    if (rx_session_event_load(s, &spec) != 0) { fprintf(stderr, "event load failed: %s\n", rx_session_last_error(s)); return 1; }
    const char* pre = getenv("RNG_SEED_PRE");
    const uint32_t pre_seed = pre ? (uint32_t)strtoul(pre, NULL, 16) : 0;
    const int reseed = getenv("RNG_RESEED") != NULL;
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    int k = -1, prev = -1;
    for (int i = 0; i < frames; i++) {
        const int st0 = raw_state(s);
        if (pre && st0 == 0) set_holdrand(pre_seed);
        uint32_t r0 = 0, g0 = 0;
        int reseeded = 0;
        if (reseed && k >= 0) {
            reseeded = 1;
            r0 = (uint32_t)k * 0x9e3779b9u + 1u; g0 = (uint32_t)k * 0x85ebca6bu + 7u;
            set_holdrand(r0); SU32(0, 0x009cb860, g0);
        }
#ifdef RX_RNG_TRACED
        g_frame = i;
#endif
        keys[0x11] = (st0 >= 2 || getenv("RNG_HOLD_W")) ? 0x80 : 0;
        rx_session_set_input(s, keys, NULL, 0);
        if (rx_session_advance_dt(s, rx_session_step_dt(s)) != 0) {
            fprintf(out, "STOP %llu %s\n", (unsigned long long)rx_session_frames(s), rx_session_last_error(s));
            break;
        }
        const int st = raw_state(s);
        if (k < 0 && st == 1 && prev == 0) k = 0;   /* the countdown entry: the first frame ending in state 1 after one ending in 0 */
        prev = st;
        fprintf(out, "F %llu st %d k %d rs %08x gs %08x gc %u", (unsigned long long)rx_session_frames(s), st, k, holdrand(),
                U32(0, 0x009cb860), U32(0, 0x009cb864));
        if (reseeded)
            fprintf(out, " n %d m %d", steps(r0, holdrand(), 0x343fdu, 0x269ec3u), steps(g0, U32(0, 0x009cb860), 0x41c64e6du, 0x3039u));
        fprintf(out, "\n");
        if (k >= 0) k++;
    }
    {   /* the streams' export (include/rx_rng.h) */
        rx_rng_state r; memset(&r, 0, sizeof r); r.size = sizeof r; r.version = RX_RNG_VERSION;
        if (rx_session_rng(s, &r) == 0)
            fprintf(out, "RNG crt_main %08x crt_loader %08x game %08x/%u ocean %u seed %08x draws %u\n", r.crt_main, r.crt_loader, r.game_seed,
                    r.game_count, r.ocean_valid, r.ocean_seed, r.ocean_draws);
    }
    if (getenv("RNG_DUMP")) {   /* development: guest words at the end ("addr,addr,...", hex) */
        const char* d = getenv("RNG_DUMP");
        while (*d) { char* e; const rx_addr a = (rx_addr)strtoul(d, &e, 16); if (e == d) break;
                     fprintf(out, "DUMP %08x %08x %g\n", a, U32(0, a), (double)F32(0, a)); d = *e ? e + 1 : e; }
    }
    fclose(out);
    rx_session_destroy(s);
    return 0;
}
