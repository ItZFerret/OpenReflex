/* bike_parity_probe.c -- the PRODUCT's bike under the same scripted pad as the oracle's whip / turn cases
 * (core/oracle/bike_whip_reverse_probe.cpp): one line per frame of the local MX simulation's state, in the oracle trace's format,
 * so builds/physparity/whip/rear.py / show.py read both. Static link with the product objects (core/build.sh's obj dir):
 *   clang -std=c11 -O2 -Icore/include -Icore/src -Icore/game -Icore/api -c core/tools/bike_parity_probe.c -o probe.o
 *   clang++ -o bike_parity_probe probe.o core/build/obj/<every object> -lz
 * usage: bike_parity_probe <game files> <frames> <trace out> [player snapshot (RPS2 file, imported read-only)]
 * env: PROBE_MODE=turn|whip (as RX_WHIP_MODE), RX_TURN_STEER / RX_TURN_RT / RX_TURN_BRAKE, RX_WHIP_JUMP / T1 / T2 / RAMP_MS / STICKS,
 *      PROBE_EQUIP=<owner>,<id> (w-parity4w: the local's vehicle equipped before the load as the oracle's REFLEX_GARAGE does it),
 *      PROBE_PROFILE=<rx_profile_cell file> (w-damagetick: imported before the load, the player's own vehicle; PAIR_KPOKE: gridpair_line.inc pair_kpoke),
 *      PROBE_EVENT="type,typeid,location,locid,class,riders,ai" (default: the reference National NAT_Concord, 12 riders, AI 95),
 *      PROBE_SIMDUMP=<file> (w-surfaces: at the first frame with every slot's vehicle, each MX slot's spec identity and the raw words of its
 *      simulation S, in the format of builds/surfaces/oracle_sims.py, for the per-model parameter comparison with the original's world;
 *      the first frame >= PROBE_SIMDUMP_FRAME with the activity up (COUNTDOWN: the world image's state, Pre on the gate;
 *      PROBE_SIMDUMP_ANY=1: whatever the state, for w-gridpair's paired dumps);
 *      PROBE_PEEK=addr[,addr..]: guest words printed at the dump),
 *      PROBE_PAIR=<file> (w-gridpair: every frame's core/tools/gridpair_line.inc line, the product side of the paired ride against the
 *      oracle's rx_PairFrameTrace; core/tools/gridpair.sh runs both, gridpair_cmp.py compares; PROBE_RESEED=1: the oracle's
 *      RX_PAIR_RESEED, both generators set at every event Pre frame start and every frame start after the countdown entry
 *      (gridpair_line.inc pair_next_seed), a test hook; PROBE_CLOCK_AT=frame:word: the engine clock word set after that frame, the
 *      oracle's clock at the matching frame (its front end ran ~5 s before the event load), a test hook; RX_WHIP_QUANT=<fps>: the
 *      script's clock rounded to whole frames, as the oracle hook's; PROBE_POKE=frame:addr:word[,...] guest words written after that
 *      frame; PROBE_CLOCK0=<ticks> the host counter's start value; PROBE_STACK_POISON=<word>; PROBE_STEP_TRACE / PROBE_STEP_SLOT with
 *      the gridpair_stepbuild.sh scratch build; PROBE_PAIR_SIMDUMP[_SLOT], PROBE_PAIR_TERRAIN: all w-gridpair test hooks;
 *      PROBE_PAIR_BOUNDS=frame[,frame...] (gridpair_line.inc pair_bounds_dump), PROBE_RENDER_SYNC=1 (after each frame, per vehicle,
 *      the render sync job's vf14_o94 / vf6_o94: the render copy of the model and its bounds): w-hfterrain test hooks),
 *      PROBE_FREE_AIR=<mode> (w-freeair: rx_session_set_free_air after the load, 1 no trick hold, 2 no auto-level, 3 both),
 *      PROBE_MODE=fuzz (w-freeair: from the race start both sticks and the triggers at random, a new pick every 6..48 frames, PROBE_SEED),
 *      PROBE_AIR_HELPERS=<auto-level>,<snap> (w-airsliders: rx_session_set_air_helpers after the load, 0 .. 1 each; 1,1 = the original, 0,0 = FREER AIR 3),
 *      PROBE_LOOSE=<0..1> (w-freeair2: rx_session_set_free_air_looseness after the load), PROBE_LOOSE_CURVE=damp,takeoff,lag,gain (its
 *      factors at 100 %, a test hook),
 *      PROBE_FREE_AIR_AT=frame:mode[,frame:mode...] (w-freeair: rx_session_set_free_air before those frames, a live change),
 *      PROBE_AIR_PARAMS=1 (w-freeair: the bike's air damping constants printed once racing),
 *      PROBE_DT (seconds per frame, default 1/60), PROBE_MAP_SCALE (rx_session_set_map_scale before the load), PROBE_NODEFORM=1 (the terrain's deformation gain held at 0: ruts off; a test
 *      switch, not original behaviour).
 *      PROBE_PHYS=<file> (w-physstudio): a PHYSICS STUDIO config (rx_session_phys_set text) set before the load (weak: links
 *      against a core without it too).
 *      PROBE_WEATHER_GRIP=<factor> (weather grip): rx_session_set_weather_grip after the load (0.85 rain / 0.7 snow at HEAVY).
 * Development tool only: reads guest memory through rx_mem.h between frames, writes nothing but PROBE_NODEFORM's gain. */
#include "rx_api.h"
#include "rx_mem.h"
#include "crt.h"
#include "free_air.h"
#include "build_activity.h"
#include "build_vehicle_select.h"
#include "race_activation.h"
#include "../src/rx_d3d.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define PAIR_RD(a) U32(0, (rx_addr)(a))
void rx_vehicle_bound_centre(rx_addr vehicle, rx_addr centre, rx_addr radius);   /* 00533510 (race_beh2_seperate.c) */
#define PAIR_CENTRE(v, out) do { const rx_addr sp_ = rx_sp(); rx_set_sp(sp_ - 0x4000u); rx_vehicle_bound_centre((v), (out), (out) + 0xc); rx_set_sp(sp_); } while (0)
#include "gridpair_line.inc"
#undef PAIR_RD

static void kpoke_write(uint32_t a, uint32_t w) { SU32(0, (rx_addr)a, w); }
static uint32_t fuzz_state;   /* w-freeair PROBE_MODE=fuzz: the pad's generator (PROBE_SEED) */
static uint32_t fuzz_next(void) { fuzz_state = fuzz_state * 1664525u + 1013904223u; return fuzz_state >> 8; }
static double envd(const char* n, double d) { const char* v = getenv(n); return v && *v ? atof(v) : d; }
static rx_addr local_sim(void) {
    const rx_addr g = U32(0, 0x00a946b8);
    if (!g) return 0;
    const uint32_t count = U8(g, 0x1541c);
    for (uint32_t i = 0; i < count && i < 8; i++) {
        const uint32_t slot = U32(g, 0x153ec + 4u * i);
        if (slot >= 16) continue;
        const rx_addr p = g + 0x7460 + slot * 0x12a0;
        if (U32(p, 0x10) != 0) continue;
        const rx_addr v = U32(p, 0x1ac);
        const rx_addr s = v ? U32(v, 0x234) : 0;
        return s && U32(s, 0) == 0x0093c8a4u ? s : 0;
    }
    return 0;
}
static void trace_line(FILE* f, unsigned long long n, rx_addr S, float dt) {
    const rx_addr C = U32(S, 0x5c30);
    fprintf(f, "%llu dt %08x air %u first %u c", n, rx_fb(dt), U32(S, 0x8d98), U32(S, 0x8d9c));
    for (uint32_t o = 0x68; o <= 0x74; o += 4) fprintf(f, " %08x", U32(C, o));
    fprintf(f, " w"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0x740 + 4 * k));
    fprintf(f, " q"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", U32(S, 0x730 + 4 * k));
    fprintf(f, " flt"); for (uint32_t k = 0; k < 15; k++) fprintf(f, " %08x", U32(S, 0x8b78 + 4 * k));
    fprintf(f, " fade %08x %08x t %08x", U32(S, 0x8bd4), U32(S, 0x8bd8), U32(S, 0x8b74));
    fprintf(f, " hist"); for (uint32_t k = 0; k < 6; k++) fprintf(f, " %08x", U32(S, 0x8bbc + 4 * k));
    fprintf(f, " to"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", U32(S, 0x8c10 + 4 * k));
    fprintf(f, " g %u %u %u %u", U32(S, 0x8d40), U32(S, 0x8d44), U32(S, 0x8d58), U32(S, 0x8d5c));
    fprintf(f, " l"); static const uint32_t L[6] = {0x8c60, 0x8c64, 0x8c68, 0x8cd0, 0x8cd4, 0x8cd8}; for (int k = 0; k < 6; k++) fprintf(f, " %u", U32(S, L[k]));
    fprintf(f, " pos"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0xf0 + 4 * k));
    fprintf(f, " v"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0x100 + 4 * k));
    static const uint32_t W[2] = {0x8790, 0x88e0};
    for (int w = 0; w < 2; w++) {
        fprintf(f, " %s %u %u %u", w ? "front" : "rear", U32(S, W[w] + 0x100), U32(S, W[w] + 0xc0), U32(S, W[w] + 0xc4));
        for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, W[w] + 0x80 + 4 * k));
    }
    fprintf(f, " grip %08x %08x off %u\n", U32(S, 0x88a0), U32(S, 0x88a4), U32(S, 0x2374));
}

/* w-gridpair: the per-step trace of one simulation in the oracle's rx_WhipStepTrace format (RX_WHIP_TRACE_WIDE fields included), from a
 * scratch build whose bike_integrate_step calls gridpair_step_hook at its entry (core/tools/gridpair_stepbuild.sh); the plain product
 * link leaves the weak hook unset. PROBE_STEP_TRACE=<file> PROBE_STEP_SLOT=<player slot> */
static FILE* g_step_out; static int g_step_slot = -1; static unsigned long long g_steps;
void gridpair_step_hook(rx_addr S, float dt) {
    if (!g_step_out) return;
    const rx_addr g = U32(0, 0x00a946b8), v = g ? U32(g + 0x7460 + (uint32_t)g_step_slot * 0x12a0, 0x1ac) : 0;
    if (!v || S != U32(v, 0x234)) return;
    FILE* f = g_step_out; const rx_addr C = U32(S, 0x5c30);
    if (pair_fw(S)) {   /* w-parity4w: a four-wheel sim (gridpair_line.inc pair_fw): the oracle's rx_WhipStepTrace four-wheel line */
        const pair_fw_class* c = pair_fw(S);
        fprintf(f, "%llu dt %08x fw %08x in", g_steps++, rx_fb(dt), c->vt);
        for (uint32_t o = 0x1c; o < 0x5c; o += 4) fprintf(f, " %08x", U32(S, o));
        fprintf(f, " B"); for (uint32_t o = 0xf0; o < 0xf0 + c->bodies * 0xa0; o += 4) fprintf(f, " %08x", U32(S, o));
        fprintf(f, " Q"); for (uint32_t o = 0x730; o < 0x730 + c->bodies * 0xa0; o += 4) fprintf(f, " %08x", U32(S, o));
        for (int w = 0; w < 4; w++) { fprintf(f, " W%d", w); for (uint32_t o = 0; o < 0x150; o += 4) fprintf(f, " %08x", U32(S, c->wheel[w] + o)); }
        fprintf(f, " spin"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", U32(S, c->spin + 4 * k));
        fprintf(f, " X"); for (uint32_t o = 0x2ec0; o < 0x2ef0; o += 4) fprintf(f, " %08x", U32(S, o));
        fprintf(f, " off %u", U32(S, 0x2374));
        if (c->vt == 0x0093d054u) {   /* the ATVs' wheel-query triangle buffer 0xad6720 (vehicle_atv_contact_dispatch.c) and its count S+0x9130 */
            const uint32_t n = U32(S, 0x9130);
            fprintf(f, " T %u", n);
            for (uint32_t o = 0; n <= 200 && o < n * 48; o += 4) fprintf(f, " %08x", U32(0x00ad6720u, o));
        } else {   /* the family's wheel-query triangle buffer (fourwheel_terrain_query.c: UTV 0xad3fb0 / buggy 0xad7550 /
                                       * truck 0xad3230, 48 bytes each) and its count (S+0x8070, buggy +0x8060): the last wheel's query */
            const rx_addr tb = c->vt == 0x0093d43cu ? 0x00ad3fb0u : c->vt == 0x0093d24cu ? 0x00ad7550u : 0x00ad3230u;
            const uint32_t n = U32(S, c->vt == 0x0093d24cu ? 0x8060 : 0x8070);
            fprintf(f, " T %u", n);
            for (uint32_t o = 0; n <= 200 && o < n * 48; o += 4) fprintf(f, " %08x", U32(tb, o));
        }
        fprintf(f, "\n");
        return;
    }
    fprintf(f, "%llu dt %08x air %u first %u c", g_steps++, rx_fb(dt), U32(S, 0x8d98), U32(S, 0x8d9c));
    for (uint32_t o = 0x68; o <= 0x74; o += 4) fprintf(f, " %08x", U32(C, o));
    fprintf(f, " w"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0x740 + 4 * k));
    fprintf(f, " q"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", U32(S, 0x730 + 4 * k));
    fprintf(f, " flt"); for (uint32_t k = 0; k < 15; k++) fprintf(f, " %08x", U32(S, 0x8b78 + 4 * k));
    fprintf(f, " fade %08x %08x t %08x", U32(S, 0x8bd4), U32(S, 0x8bd8), U32(S, 0x8b74));
    fprintf(f, " hist"); for (uint32_t k = 0; k < 6; k++) fprintf(f, " %08x", U32(S, 0x8bbc + 4 * k));
    fprintf(f, " to"); for (uint32_t k = 0; k < 4; k++) fprintf(f, " %08x", U32(S, 0x8c10 + 4 * k));
    fprintf(f, " g %u %u %u %u", U32(S, 0x8d40), U32(S, 0x8d44), U32(S, 0x8d58), U32(S, 0x8d5c));
    fprintf(f, " l"); static const uint32_t L[6] = {0x8c60, 0x8c64, 0x8c68, 0x8cd0, 0x8cd4, 0x8cd8}; for (int k = 0; k < 6; k++) fprintf(f, " %u", U32(S, L[k]));
    fprintf(f, " pos"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0xf0 + 4 * k));
    fprintf(f, " v"); for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, 0x100 + 4 * k));
    static const uint32_t W[2] = {0x8790, 0x88e0};
    for (int w = 0; w < 2; w++) {
        fprintf(f, " %s %u %u %u", w ? "front" : "rear", U32(S, W[w] + 0x100), U32(S, W[w] + 0xc0), U32(S, W[w] + 0xc4));
        for (uint32_t k = 0; k < 3; k++) fprintf(f, " %08x", U32(S, W[w] + 0x80 + 4 * k));
    }
    fprintf(f, " grip %08x %08x", U32(S, 0x88a0), U32(S, 0x88a4));
    fprintf(f, " in"); for (uint32_t o = 0x1c; o < 0x5c; o += 4) fprintf(f, " %08x", U32(S, o));
    fprintf(f, " Wr"); for (uint32_t o = 0; o < 0x150; o += 4) fprintf(f, " %08x", U32(S, 0x8790 + o));
    fprintf(f, " Wf"); for (uint32_t o = 0; o < 0x150; o += 4) fprintf(f, " %08x", U32(S, 0x88e0 + o));
    fprintf(f, " B"); for (uint32_t o = 0xf0; o < 0xf0 + 4 * 0xa0; o += 4) fprintf(f, " %08x", U32(S, o));
    fprintf(f, " X"); for (uint32_t o = 0x2ec0; o < 0x2ef0; o += 4) fprintf(f, " %08x", U32(S, o));   /* w-parity: the contact results taken */
    fprintf(f, "\n");
}

/* PROBE_SIMDUMP: every MX slot (S vtable 0x93c8a4): the vehicle spec of P+0x1b0 (MXGame +0x154d8 / +0x154dc: +4 VehicleId, +8 the model
 * record, +0x98 the owner, +0x9c its vehicle id, +0xa0 .. +0xbc the tuning, +0xc the parts by slot) and S's 0x8fc0 bytes */
static int sim_dump(const char* path) {
    const rx_addr G = U32(0, 0x00a946b8);
    if (!G) return 0;
    int n = 0, pending = 0;
    for (uint32_t k = 0; k < 16; k++) {
        const rx_addr P = G + 0x7460 + k * 0x12a0, V = U32(P, 0x1ac);
        if (V && U32(V, 0x234) && U32(U32(V, 0x234), 0) == 0x0093c8a4u) n++;
        else if (U32(P, 0x1b0) && !V) pending++;
    }
    if (!n || pending) return 0;
    FILE* f = fopen(path, "w"); if (!f) return 0;
    const uint32_t vn = U32(G, 0x154dc); const rx_addr va = U32(G, 0x154d8);
    for (uint32_t k = 0; k < 16; k++) {
        const rx_addr P = G + 0x7460 + k * 0x12a0, V = U32(P, 0x1ac);
        const rx_addr S = V ? U32(V, 0x234) : 0;
        if (!S || U32(S, 0) != 0x0093c8a4u) continue;
        rx_addr e = 0;
        for (uint32_t i = 0; i < vn && i < 64; i++) { const rx_addr x = U32(va, 4 * i); if (x && U32(x, 4) == U32(P, 0x1b0)) { e = x; break; } }
        fprintf(f, "slot %u type %d model %d owner %d ovid %d tuning", k, I32(P, 0x10), e && U32(e, 8) ? I32(U32(e, 8), 0) : -1, e ? I32(e, 0x98) : -1, e ? I32(e, 0x9c) : -1);
        for (uint32_t o = 0xa0; e && o < 0xc0; o += 4) fprintf(f, " %d", I32(e, o));
        fprintf(f, " parts");
        for (uint32_t j = 0; e && j < 17; j++) { const rx_addr pt = U32(e, 0xc + 4 * j); fprintf(f, " %d", pt ? I32(pt, 0x40) : -1); }
        fprintf(f, " S %08x\nwords", S);
        for (uint32_t o = 0; o < 0x8fc0; o += 4) fprintf(f, " %08x", U32(S, o));
        fprintf(f, "\n");
    }
    for (const char* q = getenv("PROBE_PEEK"); q && *q; ) {   /* PROBE_PEEK=addr[,addr...]: guest words at the dump (hex) */
        char* end; const unsigned long a = strtoul(q, &end, 16);
        if (end == q) break;
        fprintf(stderr, "peek %08lx %08x\n", a, U32(0, (rx_addr)a));
        q = *end == ',' ? end + 1 : end;
    }
    fclose(f);
    return 1;
}

int probe_frame_now;   /* the frame being run (1-based, the pair line's number): for scratch hooks linked into a probe (w-parity4w) */
__attribute__((weak)) int rx_session_phys_set(struct rx_session* s, const char* text, char* report, uint32_t capacity);
int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: %s <game files> <frames> <trace out> [snapshot]\n", argv[0]); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (getenv("PROBE_CLOCK0")) {   /* test hook (w-gridpair): the host performance counter's start value (10 MHz ticks), the oracle's at
                                     * the matching point: the game's millisecond clock (timeGetTime) then has the original's phase */
        extern void rx_host_clock_set(uint64_t ticks);
        rx_host_clock_set(strtoull(getenv("PROBE_CLOCK0"), NULL, 0));
    }
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    if (getenv("PROBE_PROFILE")) {   /* w-damagetick: a garage profile (rx_profile_cell rows, as stop_audit_run's STOP_AUDIT_PROFILE): the player's own vehicle */
        FILE* pf = fopen(getenv("PROBE_PROFILE"), "rb"); if (!pf) return 2;
        fseek(pf, 0, SEEK_END); long pb = ftell(pf); rewind(pf);
        rx_profile_cell* cells = malloc((size_t)pb);
        if (!cells || fread(cells, 1, (size_t)pb, pf) != (size_t)pb) return 2;
        fclose(pf);
        if (rx_session_profile_import(s, cells, (int)(pb / (long)sizeof *cells))) { fprintf(stderr, "profile import: %s\n", rx_session_last_error(s)); return 1; }
        free(cells);
    }
    if (argc > 4) {
        FILE* pf = fopen(argv[4], "rb"); if (!pf) return 2;
        fseek(pf, 0, SEEK_END); long n = ftell(pf); rewind(pf);
        unsigned char* b = malloc((size_t)n); if (!b || fread(b, 1, (size_t)n, pf) != (size_t)n) return 2; fclose(pf);
        if (n < 88 || rx_session_player_import(s, b + 88, (uint32_t)(n - 88))) { fprintf(stderr, "import: %s\n", rx_session_last_error(s)); return 1; }
        free(b);
    }
    rx_event_spec e; rx_session_event_default(&e);
    if (getenv("PROBE_EVENT")) {
        char buf[256]; snprintf(buf, sizeof buf, "%s", getenv("PROBE_EVENT"));
        char* f[7] = {0}; int k = 0; for (char* t = strtok(buf, ","); t && k < 7; t = strtok(NULL, ",")) f[k++] = t;
        if (k != 7) return 2;
        snprintf(e.event_type, sizeof e.event_type, "%s", f[0]); e.event_type_id = atoi(f[1]);
        snprintf(e.location, sizeof e.location, "%s", f[2]); e.location_id = atoi(f[3]);
        e.vehicle_class = atoi(f[4]); e.riders = atoi(f[5]); e.ai_difficulty = atoi(f[6]);
        if (!strcmp(e.event_type, "FreeRide")) { e.duration = 0; e.duration_type[0] = 0; e.track_id = 0; }
    }
    if (getenv("PROBE_MAP_SCALE") && rx_session_set_map_scale(s, (float)envd("PROBE_MAP_SCALE", 1))) { fprintf(stderr, "map scale: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("PROBE_PHYS")) {   /* w-physstudio: the config of the ride */
        if (!rx_session_phys_set) { fprintf(stderr, "PROBE_PHYS: this core has no PHYSICS STUDIO\n"); return 2; }
        FILE* cf = fopen(getenv("PROBE_PHYS"), "rb"); if (!cf) return 2;
        static char text[1 << 20], rep[4096]; size_t cn = fread(text, 1, sizeof text - 1, cf); fclose(cf); text[cn] = 0;
        fprintf(stderr, "PROBE_PHYS: %d value(s): %s", rx_session_phys_set(s, text, rep, sizeof rep), rep);
    }
    if (getenv("PROBE_EQUIP")) {   /* w-parity4w (test hook): "owner,id" = the oracle's REFLEX_GARAGE=-1,0,0,<owner>,<id>,-1 (boot.cpp: after
                                    * EnsureLocalPlayers, MXTableHelper:EquipVehicle 004e7930 on the controller-0 Player List row, before
                                    * SetupEvent), e.g. 4,44 an AI rider's ATV (AIVehicle 44), 4,45 the UTV: the local rides that vehicle */
        int owner = -1, id = -1; if (sscanf(getenv("PROBE_EQUIP"), "%d,%d", &owner, &id) != 2) return 2;
        rx_ensure_local_players(0, 0);
        const rx_addr sp = rx_sp();
        rx_set_sp(sp - 8); const uint32_t row = U32(rx_player_table_row(0), 0); rx_set_sp(sp);
        rx_set_sp(sp - 12); rx_table_equip_vehicle((uint32_t)owner, row, (uint32_t)id); rx_set_sp(sp);
        fprintf(stderr, "PROBE_EQUIP: EquipVehicle(row %u, vehicle %d, owner %d)\n", row, id, owner);
    }
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    if (getenv("PROBE_FREE_AIR") && rx_session_set_free_air(s, (int32_t)envd("PROBE_FREE_AIR", 0))) { fprintf(stderr, "free air: %s\n", rx_session_last_error(s)); return 1; }   /* w-freeair */
    if (getenv("PROBE_LOOSE_CURVE")) {   /* w-freeair2: AIR LOOSENESS's factors at 100 % (free_air.h rx_fa_curve: damp,takeoff,lag,gain), a test hook */
        rx_fa_curve* c = &rx_fa_curve_value;
        if (sscanf(getenv("PROBE_LOOSE_CURVE"), "%f,%f,%f,%f", &c->damp, &c->takeoff, &c->lag, &c->gain) != 4) return 2;
    }
    if (getenv("PROBE_AIR_HELPERS")) {   /* w-airsliders: "level,snap" = rx_session_set_air_helpers after the load (0 .. 1 each) */
        float lv = 1, sn = 1; if (sscanf(getenv("PROBE_AIR_HELPERS"), "%f,%f", &lv, &sn) != 2) return 2;
        if (rx_session_set_air_helpers(s, lv, sn)) { fprintf(stderr, "air helpers: %s\n", rx_session_last_error(s)); return 1; }
    }
    if (getenv("PROBE_LOOSE") && rx_session_set_free_air_looseness(s, (float)envd("PROBE_LOOSE", 0))) { fprintf(stderr, "looseness: %s\n", rx_session_last_error(s)); return 1; }   /* w-freeair2 */
    if (getenv("PROBE_WEATHER_GRIP") && rx_session_set_weather_grip(s, (float)envd("PROBE_WEATHER_GRIP", 1))) { fprintf(stderr, "weather grip: %s\n", rx_session_last_error(s)); return 1; }   /* weather grip (2026-11-07) */
    FILE* tf = fopen(argv[3], "w"); if (!tf) return 2;
    const int frames = atoi(argv[2]);
    const float dt = (float)envd("PROBE_DT", 1.0 / 60.0);
    const char* mode = getenv("PROBE_MODE");
    const int turn = mode && !strcmp(mode, "turn");
    const int fuzz = mode && !strcmp(mode, "fuzz");
    fuzz_state = (uint32_t)envd("PROBE_SEED", 1);
    int racing = 0, state = 0, jumps = 0; double start = 0, t0 = 0, ground_since = -1;
    unsigned long long n = 0;
    int dumped = getenv("PROBE_SIMDUMP") == NULL;
    FILE* pair = getenv("PROBE_PAIR") ? fopen(getenv("PROBE_PAIR"), "w") : NULL;
    if (getenv("PROBE_STEP_TRACE")) { g_step_out = fopen(getenv("PROBE_STEP_TRACE"), "w"); g_step_slot = (int)envd("PROBE_STEP_SLOT", 0); }
    for (int i = 0; i < frames; i++) {
        if (!dumped && (getenv("PROBE_SIMDUMP_ANY") || rx_session_get_state(s) >= RX_SESSION_COUNTDOWN) && i >= (int)envd("PROBE_SIMDUMP_FRAME", 0) && (dumped = sim_dump(getenv("PROBE_SIMDUMP"))))
            fprintf(stderr, "sim dump at frame %d (game time %.4f)\n", i, rx_session_game_time(s));
        const rx_addr S = local_sim();
        double clock = rx_session_game_time(s);
        if (getenv("RX_WHIP_QUANT") && envd("RX_WHIP_QUANT", 0) > 0) { const double fps = envd("RX_WHIP_QUANT", 60); clock = (double)(float)(floor(clock * fps + 0.5) / fps); }
        const rx_addr G = U32(0, 0x00a946b8), A = G ? U32(G + 0x7460, 0x94) : 0;   /* the activity's race state, as boot_main reads it */
        const int race = A ? I32(A, 0x21c) : -1;
        if (!racing && race >= 2 && rx_session_get_state(s) == RX_SESSION_RACING) { racing = 1; start = clock; fprintf(stderr, "racing from frame %d\n", i); }
        if (racing == 1 && S && getenv("PROBE_AIR_PARAMS")) {   /* w-freeair: the air model's constants of this bike */
            racing = 2;
            fprintf(stderr, "air params: AngularRateDamping S+5680 %g %g %g, OnTakeoffDamping S+56e0 %g %g %g, idle fade rate [009d789c] %g\n",
                    F32(S, 0x5680), F32(S, 0x5684), F32(S, 0x5688), F32(S, 0x56e0), F32(S, 0x56e4), F32(S, 0x56e8), F32(0, 0x009d789c));
        }
        rx_pad pad; memset(&pad, 0, sizeof pad);
        double lat = 0; int rt = racing ? 255 : 0, lt = 0;
        if (racing && S && !turn && state < 2) {   /* the whip case: arm after 0.5 s grounded, whip on the RX_WHIP_JUMP-th take-off */
            if (U32(S, 0x8d98)) {
                if (state == 1) { ++jumps; if (jumps == (int)envd("RX_WHIP_JUMP", 1)) { state = 2; t0 = clock; fprintf(stderr, "take-off %d at %.6f (whip)\n", jumps, clock); } else state = 0; }
                ground_since = -1;
            } else { if (ground_since < 0) ground_since = clock; if (clock - ground_since >= 0.5) state = 1; }
        }
        if (state == 2) {
            const double fq = envd("RX_WHIP_QUANT", 0), t = fq > 0 ? floor((clock - t0) * fq + 0.5) / fq : clock - t0, T1 = envd("RX_WHIP_T1", 0.25), T2 = envd("RX_WHIP_T2", 0.55), ramp = envd("RX_WHIP_RAMP_MS", 0) / 1000.0, side = envd("RX_WHIP_SIDE", 1);
            if (t < T1) lat = side; else if (t < T2) lat = ramp > 0 && t < T1 + ramp ? side * (1 - 2 * (t - T1) / ramp) : -side; else state = 3;
        }
        if (turn && racing) {
            const double fq = envd("RX_WHIP_QUANT", 0), t = fmod(fq > 0 ? floor((clock - start) * fq + 0.5) / fq : clock - start, 7.0), k = envd("RX_TURN_STEER", 0.5);
            lat = t < 2 ? k : t < 3.5 ? 0 : t < 5.5 ? -k : 0;
            rt = (int)envd("RX_TURN_RT", 255);
            if (envd("RX_TURN_BRAKE", 0) != 0 && ((t >= 3.0 && t < 3.5) || t >= 6.5)) { rt = 0; lt = 255; }
        }
        int fuzzed = 0;
        if (fuzz && racing) {   /* w-freeair PROBE_MODE=fuzz: both sticks and the triggers at random, a new pick every 6..48 frames */
            static int hold_left; static double fl[4]; static int frt, flt;
            if (--hold_left <= 0) {
                hold_left = 6 + (int)(fuzz_next() % 43u);
                for (int k = 0; k < 4; k++) fl[k] = fuzz_next() % 3u == 0 ? 0.0 : ((double)(fuzz_next() % 2001u) - 1000.0) / 1000.0;
                if (fuzz_next() % 4u == 0) fl[0] = fl[0] < 0 ? -1.0 : 1.0;   /* full whips now and then */
                frt = fuzz_next() % 10u < 8u ? 255 : (int)(fuzz_next() % 256u); flt = fuzz_next() % 12u == 0 ? 255 : 0;
            }
            pad.lx = (int16_t)lround(fl[0] * 32767); pad.ly = (int16_t)lround(fl[1] * 32767);
            pad.rx = (int16_t)lround(fl[2] * 32767); pad.ry = (int16_t)lround(fl[3] * 32767);
            pad.right_trigger = (uint8_t)frt; pad.left_trigger = (uint8_t)flt;
            fuzzed = 1;
        }
        const char* sticks = getenv("RX_WHIP_STICKS");
        const int left = turn || !sticks || !strcmp(sticks, "both") || !strcmp(sticks, "left");
        const int right = !turn && (!sticks || !strcmp(sticks, "both") || !strcmp(sticks, "right"));
        const int16_t v = (int16_t)lround(lat * 32767);
        if (!fuzzed) {
            pad.right_trigger = (uint8_t)rt; pad.left_trigger = (uint8_t)lt;
            pad.lx = left ? v : 0; pad.rx = right ? v : 0;
        }
        if (getenv("PROBE_NODEFORM")) {   /* test switch: the active terrain's deformation gain (+0xbc) held at 0 */
            if (U32(0, 0x009e30e0) & 1) {   /* api/rx_session.c active_terrain */
                const rx_addr scene = U32(0, 0x00a95844);
                for (uint32_t k = 0; scene && k < U32(0, 0x009e30dc); k++) {
                    const rx_addr t = U32(0, 0x009e30d4 + 4u * k);
                    if (t && U32(scene, 0x1c + 4u * U32(scene, 4)) == U32(t, 0x44a8)) SF32(t, 0xbc, 0.0f);
                }
            }
            if (getenv("PROBE_NODEFORM_SURF")) {   /* w-parity4w: also every listed terrain's deformation surface (terrain +0x240) back to its first
                * seen contents before each frame. The activity's vf18 (rx_world_set_terrain_wetness, at Pre) sets the gain back to 2 inside a
                * frame, so the product's CPU stamps of that frame landed and moved the vertex heights under the wheels (1 ulp on NAT_Bedford),
                * where the oracle's surface never changes (its null device draws nothing: no ruts on the original's side either way) */
                static struct { rx_addr obj; uint8_t* copy; uint32_t bytes; } snap[8];
                for (uint32_t k = 0; k < U32(0, 0x009e30dc) && k < 4; k++) {
                    const rx_addr t = U32(0, 0x009e30d4 + 4u * k), obj = t ? U32(t, 0x240) : 0;
                    uint32_t pitch = 0; const rx_addr bits = obj ? rx_d3d_bits(obj, 0, &pitch) : 0;
                    if (!bits) continue;
                    const uint32_t bytes = pitch * U32(obj, 0x10);
                    int j = 0; while (j < 8 && snap[j].obj && snap[j].obj != obj) j++;
                    if (j == 8) continue;
                    if (!snap[j].obj) { snap[j].obj = obj; snap[j].bytes = bytes; snap[j].copy = malloc(bytes); for (uint32_t o = 0; o < bytes; o += 4) memcpy(snap[j].copy + o, &(uint32_t){U32(bits, o)}, 4); }
                    else for (uint32_t o = 0; o < snap[j].bytes && o < bytes; o += 4) { uint32_t w; memcpy(&w, snap[j].copy + o, 4); if (U32(bits, o) != w) SU32(bits, o, w); }
                }
            }
        }
        if (getenv("PROBE_STACK_POISON")) {   /* test hook (w-gridpair): the 128 KB of guest stack below the main thread's SP filled with a word
                                               * before every frame: a run that changes with it reads dead stack somewhere */
            const uint32_t w = (uint32_t)strtoul(getenv("PROBE_STACK_POISON"), NULL, 16); const rx_addr sp = rx_sp();
            for (rx_addr a = sp - 0x20000u; a < sp; a += 4) SU32(0, a, w);
        }
        for (const char* fa = getenv("PROBE_FREE_AIR_AT"); fa && *fa; ) {   /* w-freeair: "frame:mode[,...]" = rx_session_set_free_air before that frame (a live change) */
            char* e; const long fr = strtol(fa, &e, 10); if (e == fa || *e != ':') break;
            const long m = strtol(e + 1, &e, 10);
            if (fr == i && rx_session_set_free_air(s, (int32_t)m)) { fprintf(stderr, "free air: %s\n", rx_session_last_error(s)); return 1; }
            fa = *e == ',' ? e + 1 : e;
        }
        probe_frame_now = i + 1;
        if (rx_session_set_input(s, NULL, &pad, 1) || rx_session_advance_dt(s, dt)) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); return 1; }
        if (getenv("PROBE_RENDER_SYNC")) {   /* test hook (w-hfterrain): the gameplay part of the original's render sync per vehicle (the
                                           * per-object job Render_SyncWorkerJob 007e0c50, which the product does not run): RavenVehicle
                                           * vf14_o94 004302c0 (vf61, then the model's node set +0x10 copied over its render copy +0x50)
                                           * and vf6_o94 004302f0 (the copy's subtree bounds, which the AI's BehaviorSeperate reads).
                                           * Superseded (w-rendersync): the product's render_sync now runs the job itself (loop_engine.c
                                           * rx_render_sync_data); with this hook the vehicles are synced twice per frame */
            extern void rx_world_vehicle_o94_copy_nodes(rx_addr self);
            extern void rx_vehicle_root_bounds(rx_addr v, rx_addr a0, rx_addr a1, rx_addr a2);
            const rx_addr G = U32(0, 0x00a946b8), sp = rx_sp(), out = (sp - 0x200u) & ~15u;
            rx_set_sp(sp - 0x4000u);
            for (uint32_t k = 0; G && k < 16; k++) {
                const rx_addr V = U32(G + 0x7460 + k * 0x12a0, 0x1ac);
                if (!V) continue;
                rx_world_vehicle_o94_copy_nodes(V + 0x94);
                rx_vehicle_root_bounds(V + 0x94, out, out + 0x10, out + 0x20);
            }
            rx_set_sp(sp);
        }
        {   /* test hook (w-gridpair): PROBE_POKE="frame:addr:word[,...]" (hex addr / word), written after that frame; w-parity5: also
             * PROBE_POKE_FILE=<file> of the same entries (a full-length ride's per-frame pokes exceed the environment's size), both parsed
             * once, applied in their order */
            static struct probe_poke { unsigned long fr; uint32_t a, w; }* pokes; static size_t npokes; static int parsed;
            if (!parsed) {
                parsed = 1;
                char* buf = NULL;
                const char* fn = getenv("PROBE_POKE_FILE");
                FILE* pf = fn && *fn ? fopen(fn, "rb") : NULL;
                if (pf) {
                    fseek(pf, 0, SEEK_END); const long n = ftell(pf); fseek(pf, 0, SEEK_SET);
                    buf = (char*)malloc((size_t)n + 1);
                    if (buf) { buf[fread(buf, 1, (size_t)n, pf)] = 0; }
                    fclose(pf);
                }
                const char* srcs[2] = {getenv("PROBE_POKE"), buf};
                for (int si = 0; si < 2; si++)
                    for (const char* pk = srcs[si]; pk && *pk; ) {
                        while (*pk == ',' || *pk == '\n' || *pk == ' ') pk++;
                        if (!*pk) break;
                        char* e; const unsigned long fr = strtoul(pk, &e, 10); if (e == pk || *e != ':') break;
                        const unsigned long a = strtoul(e + 1, &e, 16); if (*e != ':') break;
                        const unsigned long w = strtoul(e + 1, &e, 16);
                        struct probe_poke* np = (struct probe_poke*)realloc(pokes, (npokes + 1) * sizeof *pokes);
                        if (!np) break;
                        pokes = np; pokes[npokes].fr = fr; pokes[npokes].a = (uint32_t)a; pokes[npokes].w = (uint32_t)w; npokes++;
                        pk = e;
                    }
                free(buf);
            }
            for (size_t k = 0; k < npokes; k++)
                if (pokes[k].fr == (unsigned long)(i + 1)) SU32(0, (rx_addr)pokes[k].a, pokes[k].w);
        }
        for (const char* ps = getenv("PROBE_PART_SOFT"); ps && *ps; ) {   /* w-parity4w (test alignment): "frame:slot.part[,...]": after that frame
             * the slot's damage part is "soft" as the original's uninitialised entity byte +0x15d made it (0041cd90 -> 007697e0: the part
             * shape's +0x10 = 4 | 0x10 instead of the default 1 | 0x10; the entity's +0x15d = 1 so a re-made body keeps it) */
            char* e; const unsigned long fr = strtoul(ps, &e, 10); if (e == ps || *e != ':') break;
            const unsigned long sl = strtoul(e + 1, &e, 10); if (*e != '.') break;
            const unsigned long pt = strtoul(e + 1, &e, 10);
            if (fr == (unsigned long)(i + 1)) {
                const rx_addr G = U32(0, 0x00a946b8), V = G ? U32(G + 0x7460 + (uint32_t)sl * 0x12a0, 0x1ac) : 0;
                for (uint32_t k = 0; V && k < 12; k++) {
                    const rx_addr E = 0x00db1410u + 0x10 + 0x410 * k;
                    if (U32(E, 0x3fc) != V || pt >= U32(E, 0)) continue;
                    const rx_addr ent = U32(E, 0x2c + 4 * (uint32_t)pt), sh = U32(E, 0x4c + 8 * (uint32_t)pt + 4);
                    if (ent) SU8(ent, 0x15d, 1);
                    if (sh) SU32(sh, 0x10, 0x14);
                }
            }
            ps = *e == ',' ? e + 1 : e;
        }
        for (const char* vc = getenv("PROBE_VCTL"); vc && *vc; ) {   /* w-parity4w (test alignment): "frame:slot:word[,...]": after that frame the
             * slot's vehicle control record word V+0x129c (+0xc, the AI's reverse before its driver's first write: uninitialised heap in the
             * original, 0 in the product's arena) = word */
            char* e; const unsigned long fr = strtoul(vc, &e, 10); if (e == vc || *e != ':') break;
            const unsigned long sl = strtoul(e + 1, &e, 10); if (*e != ':') break;
            const unsigned long w = strtoul(e + 1, &e, 16);
            const rx_addr G = U32(0, 0x00a946b8), V = G && sl < 16 ? U32(G + 0x7460 + (uint32_t)sl * 0x12a0, 0x1ac) : 0;
            if (fr == (unsigned long)(i + 1) && V) SU32(V, 0x129c, (uint32_t)w);
            vc = *e == ',' ? e + 1 : e;
        }
        if (getenv("PROBE_CLOCK_AT")) {   /* test hook (w-gridpair): "frame:word" = the engine clock (+0x3c, hex float word) set after that frame */
            unsigned long at = 0, w = 0;
            if (sscanf(getenv("PROBE_CLOCK_AT"), "%lu:%lx", &at, &w) == 2 && (unsigned long)(i + 1) == at) SU32(0, 0x00d728d0 + 0x3c, (uint32_t)w);
        }
        if (pair) {
            const rx_addr ptd = crt_ptd();
            pair_line(pair, (unsigned long)(i + 1), ptd ? U32(ptd, 0x14) : 0u);
            pair_kpoke((unsigned long)(i + 1), kpoke_write);   /* w-damagetick PAIR_KPOKE */
            if (getenv("PROBE_PAIR_TERRAIN") && strtoul(getenv("PROBE_PAIR_TERRAIN"), NULL, 10) == (unsigned long)(i + 1)) pair_terrain_dump(pair, (unsigned long)(i + 1));
            for (const char* sd = getenv("PROBE_PAIR_SIMDUMP"); sd && *sd; ) {   /* frame[,frame...]: the local sim's dump after it */
                char* e; const unsigned long fr = strtoul(sd, &e, 10); if (e == sd) break;
                if (fr == (unsigned long)(i + 1)) pair_simdump(pair, fr, getenv("PROBE_PAIR_SIMDUMP_SLOT") ? atoi(getenv("PROBE_PAIR_SIMDUMP_SLOT")) : -1);
                sd = *e == ',' ? e + 1 : e;
            }
            for (const char* bd = getenv("PROBE_PAIR_BOUNDS"); bd && *bd; ) {   /* w-hfterrain: frame[,frame...] -> pair_bounds_dump */
                char* e; const unsigned long fr = strtoul(bd, &e, 10); if (e == bd) break;
                if (fr == (unsigned long)(i + 1)) pair_bounds_dump(pair, fr, (rx_sp() - 0x200u) & ~15u);
                bd = *e == ',' ? e + 1 : e;
            }
            uint32_t crt, lcg;
            if (getenv("PROBE_RESEED") && pair_next_seed(pair_state(), &crt, &lcg)) { if (ptd) SU32(ptd, 0x14, crt); SU32(0, 0x009cb860, lcg); }
        }
        const rx_addr S2 = local_sim();
        if (S2 && racing) trace_line(tf, n++, S2, dt);
    }
    fclose(tf);
    if (pair) fclose(pair);
    if (g_step_out) fclose(g_step_out);
    printf("frames %d traced %llu\n", frames, n);
    rx_session_destroy(s);
    return 0;
}
