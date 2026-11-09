/* jobthreads_hash.c (w-jobthreads) -- one event through the product session API with a fake clock, for the bit-exactness checks of
 * the parallel worker jobs (core/game/loop_jobs.h; core/tools/jobthreads_check.sh runs it per RX_JOB_THREADS mode and compares):
 *   HASH frame <n> all <h> image <h> heap <h> dll <h> stack <h> high <h>   the whole arena every JT_HASH race frames (0: never)
 *   G <n> <h>                                                             JT_POSE=1: every slot's exported rider / bike pose and the game LCG, per frame
 *   PERF frame <n> core <ms>/frame                                         every 300 race frames (rx_session_advance wall time)
 *   jobthreads_hash <install root> <type> <typeId> <level> <locationId> <trackId> <vehicleClass> <duration> <durationType> <riders> <ai>
 *                   <activation|-> <load frames max> <race frames> [keys-held scan codes, comma separated | fuzz:<seed> (as stop_audit_run)]
 * env JT_EXCL="lo-hi,..." hex ranges left out of every hash, JT_MAP=1 the touched 64 KB chunks of 0x70000000..0x80000000 at the end,
 * JT_DUMP=<race frame>:<file> every non-zero page (u32 address + 4 KB) at that frame (0 = after the load), then exit.
 * JT_PHYS=<file> (w-physstudio): a PHYSICS STUDIO config (rx_session_phys_set text) set before the load ("" = an empty config);
 * JT_PHYS_ARM=1 rx_session_phys_arm_live before the load; JT_PHYS_LIVE=<period>:<file>[,<file>...] (w-physstudio2): every <period> race
 * frames the next file's config set and applied live (rx_session_phys_apply_live), a LIVE line each time. Both are weak: the driver also links against a core without them (main).
 * JT_WEATHER_GRIP=<factor> (weather grip, 2026-11-07): rx_session_set_weather_grip before the load (weak, like the PHYSICS STUDIO calls);
 * JT_STEER=<frames>: with keys held, A (0x1e) and D (0x20) held in turn for that many frames each (a slalom); JT_DIST=1: the local rider's
 * (slot 0) bike root path length in the race, a DIST line every 300 frames and at the end, plus its mean speed.
 * Links against the product objects (rx_base): tools/product_link.sh. */
#include "rx_api.h"
#include "rx_mem.h"
#include "rx_os.h"
#include "rx_rng.h"
#include <mach/mach.h>
#include <mach/mach_vm.h>
#include <mach/vm_page_size.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define NREG 5
static const struct { uint32_t lo, hi; const char* name; } k_regions[NREG] = {
    {0x00000000u, 0x0fffffffu, "image"}, {0x10000000u, 0x6fffffffu, "heap"}, {0x70000000u, 0x7fdfffffu, "dll"},
    {0x7fe00000u, 0x7fffffffu, "stack"}, {0x80000000u, 0xffffffffu, "high"},
};
static uint32_t g_excl[16][2]; static int g_nexcl;
static uint64_t mix(uint64_t h, uint64_t v) { h ^= v; h *= 0x100000001b3ull; h ^= h >> 29; return h; }
static int excluded(uint64_t a) { for (int i = 0; i < g_nexcl; i++) if (a >= g_excl[i][0] && a <= g_excl[i][1]) return 1; return 0; }
static void hash_arena(uint64_t out[NREG + 1], int map) {
    memset(out, 0, sizeof(uint64_t) * (NREG + 1));
    const uint64_t page = (uint64_t)vm_page_size, chunk = 256ull << 20;
    static int* disp; if (!disp) disp = (int*)malloc(sizeof(int) * ((256 << 20) / 4096));
    uint8_t touched[4096] = {0};
    for (uint64_t base = 0; base < 0x100000000ull; base += chunk) {
        mach_vm_size_t count = (mach_vm_size_t)(chunk / page);
        if (mach_vm_page_range_query(mach_task_self(), (mach_vm_address_t)(uintptr_t)(rx_base + base), chunk,
                                     (mach_vm_address_t)(uintptr_t)disp, &count) != KERN_SUCCESS) { fprintf(stderr, "query failed\n"); exit(1); }
        for (mach_vm_size_t pg = 0; pg < count; pg++) {
            if (!(disp[pg] & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_PAGED_OUT))) continue;
            const uint64_t a = base + pg * page;
            const uint64_t* w = (const uint64_t*)(rx_base + a);
            uint64_t any = 0;
            for (uint64_t k = 0; k < page / 8; k++) any |= w[k];
            if (!any) continue;
            if (a >= 0x70000000u && a < 0x80000000u) touched[(a - 0x70000000u) >> 16] = 1;
            if (excluded(a)) continue;
            int reg = 0;
            while (reg < NREG - 1 && a > k_regions[reg].hi) reg++;
            uint64_t h = mix(0xcbf29ce484222325ull, a);
            for (uint64_t k = 0; k < page / 8; k++) h = mix(h, w[k]);
            out[1 + reg] = mix(out[1 + reg], h);
            out[0] = mix(out[0], h);
        }
    }
    if (map) for (int i = 0; i < 4096; i++) if (touched[i]) fprintf(stderr, "MAP %08x\n", 0x70000000u + ((uint32_t)i << 16));
}
/* JT_DUMP=<race frame>:<file>: every non-zero page (address, 4 KB) at that frame (0 = after the load) */
static void dump_arena(const char* path) {
    FILE* f = fopen(path, "wb"); if (!f) return;
    const uint64_t page = 4096, chunk = 256ull << 20;
    static int* disp; if (!disp) disp = (int*)malloc(sizeof(int) * ((256 << 20) / 4096));
    for (uint64_t base = 0; base < 0x100000000ull; base += chunk) {
        mach_vm_size_t count = (mach_vm_size_t)(chunk / (uint64_t)vm_page_size);
        mach_vm_page_range_query(mach_task_self(), (mach_vm_address_t)(uintptr_t)(rx_base + base), chunk, (mach_vm_address_t)(uintptr_t)disp, &count);
        for (mach_vm_size_t pg = 0; pg < count; pg++) {
            if (!(disp[pg] & (VM_PAGE_QUERY_PAGE_PRESENT | VM_PAGE_QUERY_PAGE_PAGED_OUT))) continue;
            for (uint64_t a = base + pg * vm_page_size; a < base + (pg + 1) * vm_page_size; a += page) {
                const uint64_t* w = (const uint64_t*)(rx_base + a); uint64_t any = 0;
                for (int k = 0; k < 512; k++) any |= w[k];
                if (!any) continue;
                const uint32_t a32 = (uint32_t)a; fwrite(&a32, 4, 1, f); fwrite(rx_base + a, 1, page, f);
            }
        }
    }
    fclose(f);
}
static int g_dump_frame = -1; static char g_dump_path[512];
/* JT_POSE=1: one "G <frame> <hash>" line per race frame: every slot's rider bones and bike nodes (the exported poses) and the game LCG */
static uint64_t pose_digest(rx_session* s) {
    static float buf[256 * 16];
    uint64_t h = 0xcbf29ce484222325ull;
    for (int sl = 0; sl < 12; sl++) {
        int n = rx_session_rider_pose_slot(s, sl, buf, 256);
        for (int k = 0; n > 0 && k < n * 16; k++) { uint32_t b; memcpy(&b, &buf[k], 4); h = mix(h, b); }
        n = rx_session_bike_pose_slot(s, sl, buf, 256);
        for (int k = 0; n > 0 && k < n * 16; k++) { uint32_t b; memcpy(&b, &buf[k], 4); h = mix(h, b); }
    }
    rx_rng_state g; memset(&g, 0, sizeof g); g.size = sizeof g; g.version = RX_RNG_VERSION;
    if (rx_session_rng(s, &g) == 0) { h = mix(h, g.game_seed); h = mix(h, g.game_count); }
    return h;
}
static uint32_t g_ms;
static uint32_t fake_time_ms(void) { return g_ms += 16; }

__attribute__((weak)) int rx_session_phys_set(struct rx_session* s, const char* text, char* report, uint32_t capacity);
__attribute__((weak)) int rx_session_phys_arm_live(struct rx_session* s, int32_t on);
__attribute__((weak)) int rx_session_phys_apply_live(struct rx_session* s);
__attribute__((weak)) int rx_session_phys_active(const struct rx_session* s);
__attribute__((weak)) int rx_session_set_weather_grip(struct rx_session* s, float factor);
static double g_dist; static float g_last[3]; static int g_has_last;
static void dist_frame(rx_session* s) {   /* JT_DIST: slot 0's bike root (node 0's matrix, row vectors: translation at 12..14) */
    static float buf[256 * 16];
    if (rx_session_bike_pose_slot(s, 0, buf, 256) <= 0) return;
    if (g_has_last) { double dx = buf[12] - g_last[0], dy = buf[13] - g_last[1], dz = buf[14] - g_last[2]; double d = dx * dx + dy * dy + dz * dz; if (d < 100.0 * 100.0) g_dist += __builtin_sqrt(d); }
    g_last[0] = buf[12]; g_last[1] = buf[13]; g_last[2] = buf[14]; g_has_last = 1;
}
static int g_live_period, g_live_n; static char* g_live_text[16];
static void phys_live_setup(void) {   /* JT_PHYS_LIVE */
    const char* v = getenv("JT_PHYS_LIVE");
    if (!v || !rx_session_phys_set || !rx_session_phys_apply_live) return;
    g_live_period = atoi(v);
    const char* c = strchr(v, ':');
    char buf[4096]; snprintf(buf, sizeof buf, "%s", c ? c + 1 : "");
    for (char* t = strtok(buf, ","); t && g_live_n < 16; t = strtok(NULL, ",")) {
        FILE* f = fopen(t, "rb"); if (!f) continue;
        fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
        char* b = malloc((size_t)n + 1); if (!b) { fclose(f); continue; }
        b[fread(b, 1, (size_t)n, f)] = 0; fclose(f); g_live_text[g_live_n++] = b;
    }
}
static void phys_live_frame(rx_session* s, int i) {
    if (g_live_period <= 0 || g_live_n == 0 || (i + 1) % g_live_period) return;
    static char rep[4096]; static int k;
    const int n = rx_session_phys_set(s, g_live_text[k % g_live_n], rep, sizeof rep);
    const int v = rx_session_phys_apply_live(s);
    printf("LIVE frame %d config %d: %d value(s), %d vehicle(s), active %d\n", i + 1, k % g_live_n, n, v, rx_session_phys_active ? rx_session_phys_active(s) : -1);
    k++;
}
static void phys_config(rx_session* s) {   /* JT_PHYS / JT_PHYS_ARM (w-physstudio) */
    if (getenv("JT_PHYS_ARM") && rx_session_phys_arm_live) printf("PHYS arm %d\n", rx_session_phys_arm_live(s, 1));
    const char* path = getenv("JT_PHYS");
    if (!path) return;
    if (!rx_session_phys_set) { printf("PHYS this core has no rx_session_phys_set\n"); return; }
    static char text[1 << 20], report[8192];
    size_t n = 0;
    if (*path) { FILE* f = fopen(path, "rb"); if (f) { n = fread(text, 1, sizeof text - 1, f); fclose(f); } }
    text[n] = 0;
    const int r = rx_session_phys_set(s, text, report, sizeof report);
    printf("PHYS set %d: %s", r, report);
}
int main(int argc, char** argv) {
    if (argc < 15) { fprintf(stderr, "usage: see the file header\n"); return 2; }
    setvbuf(stdout, NULL, _IONBF, 0);
    if (getenv("JT_EXCL")) {
        char buf[512]; snprintf(buf, sizeof buf, "%s", getenv("JT_EXCL"));
        for (char* t = strtok(buf, ","); t && g_nexcl < 16; t = strtok(NULL, ",")) {
            unsigned lo, hi; if (sscanf(t, "%x-%x", &lo, &hi) == 2) { g_excl[g_nexcl][0] = lo; g_excl[g_nexcl][1] = hi; g_nexcl++; }
        }
    }
    const int every = getenv("JT_HASH") ? atoi(getenv("JT_HASH")) : 0;
    rx_session* s = rx_session_create(argv[1]);
    if (!s) { printf("create failed: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_os_set_time_source(fake_time_ms);
    rx_event_spec e;
    rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[2]);
    e.event_type_id = atoi(argv[3]);
    snprintf(e.location, sizeof e.location, "%s", argv[4]);
    e.location_id = atoi(argv[5]);
    e.track_id = atoi(argv[6]);
    e.vehicle_class = atoi(argv[7]);
    e.duration = atoi(argv[8]);
    snprintf(e.duration_type, sizeof e.duration_type, "%s", argv[9]);
    e.riders = atoi(argv[10]);
    e.ai_difficulty = atoi(argv[11]);
    e.event_activation[0] = 0;
    if (strcmp(argv[12], "-") != 0) snprintf(e.event_activation, sizeof e.event_activation, "%s", argv[12]);
    const int load_max = atoi(argv[13]), race = atoi(argv[14]);
    uint8_t keys[256]; memset(keys, 0, sizeof keys);
    int fuzz = 0; unsigned seed = 1; rx_remap_table remap; memset(&remap, 0, sizeof remap);
    if (argc > 15 && strncmp(argv[15], "fuzz:", 5) == 0) { fuzz = 1; seed = (unsigned)atoi(argv[15] + 5); rx_remap_defaults(&remap); }   /* stop_audit_run's fuzz */
    else if (argc > 15) { char* p = strdup(argv[15]); for (char* t = strtok(p, ","); t; t = strtok(NULL, ",")) keys[atoi(t) & 255] = 0x80; }
    phys_config(s);
    phys_live_setup();
    if (getenv("JT_WEATHER_GRIP")) {
        if (!rx_session_set_weather_grip) printf("WEATHER GRIP this core has no rx_session_set_weather_grip\n");
        else printf("WEATHER GRIP %s set %d\n", getenv("JT_WEATHER_GRIP"), rx_session_set_weather_grip(s, (float)atof(getenv("JT_WEATHER_GRIP"))));
    }
    const int steer = getenv("JT_STEER") ? atoi(getenv("JT_STEER")) : 0, track = getenv("JT_DIST") != NULL;
    if (rx_session_event_load(s, &e) != 0) { printf("load failed: %s\n", rx_session_last_error(s)); return 1; }
    int frames = 0;
    while (rx_session_get_state(s) == RX_SESSION_LOADING && frames < load_max) { if (rx_session_advance(s) != 0) break; frames++; }
    printf("LOADED frames %d state %d\n", frames, rx_session_get_state(s));
    if (getenv("JT_DUMP")) { sscanf(getenv("JT_DUMP"), "%d:%511s", &g_dump_frame, g_dump_path); if (g_dump_frame == 0) { dump_arena(g_dump_path); return 0; } }
    if (argc > 15 && !fuzz) rx_session_set_input(s, keys, NULL, 0);
    int hold = 0;
    uint64_t h[NREG + 1];
    double acc = 0, total = 0; int n = 0, done = 0;
    for (int i = 0; i < race && rx_session_get_state(s) != RX_SESSION_ERROR; i++) {
        if (fuzz && hold-- <= 0) {   /* as stop_audit_run's fuzz: throttle mostly, steering / tricks / reflex / camera / reset at random */
            hold = 15 + (int)(rand_r(&seed) % 40);
            memset(keys, 0, sizeof keys);
            if (rand_r(&seed) % 10 != 0) keys[remap.keyboard[0] & 255] = 0x80;
            if (rand_r(&seed) % 12 == 0) keys[remap.keyboard[1] & 255] = 0x80;
            { unsigned r = rand_r(&seed) % 4; if (r == 0) keys[remap.keyboard[2] & 255] = 0x80; else if (r == 1) keys[remap.keyboard[3] & 255] = 0x80; }
            { int trick = rand_r(&seed) % 2 == 0;
              if (trick) keys[remap.keyboard[4] & 255] = 0x80;
              int any = 0;
              for (int k = 6; k < 10; k++) if (rand_r(&seed) % 4 == 0) { keys[remap.keyboard[k] & 255] = 0x80; any = 1; }
              if (trick && !any) keys[remap.keyboard[6 + (int)(rand_r(&seed) % 4)] & 255] = 0x80; }
            if (rand_r(&seed) % 40 == 0) keys[remap.keyboard[10] & 255] = 0x80;
            if (rand_r(&seed) % 60 == 0) keys[remap.keyboard[12] & 255] = 0x80;
            rx_session_set_input(s, keys, NULL, 0);
        }
        if (steer > 0 && !fuzz && argc > 15 && i % steer == 0) {   /* JT_STEER: A / D in turn */
            const int left = (i / steer) % 2 == 0;
            keys[0x1e] = left ? 0x80 : 0; keys[0x20] = left ? 0 : 0x80;
            rx_session_set_input(s, keys, NULL, 0);
        }
        struct timespec t0, t1; clock_gettime(CLOCK_MONOTONIC, &t0);
        const int adv = rx_session_advance(s);
        clock_gettime(CLOCK_MONOTONIC, &t1);
        const double ms = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
        acc += ms; total += ms; n++; done++;
        if (adv != 0) { printf("advance error at %d: %s\n", i, rx_session_last_error(s)); break; }
        phys_live_frame(s, i);
        if (track) { dist_frame(s); if ((i + 1) % 300 == 0) printf("DIST frame %d %.3f\n", i + 1, g_dist); }
        if (getenv("JT_POSE")) printf("G %d %016llx\n", i + 1, (unsigned long long)pose_digest(s));
        if (i + 1 == g_dump_frame) { dump_arena(g_dump_path); return 0; }
        if (n == 300) { printf("PERF frame %d core %.3f ms/frame\n", i + 1, acc / n); acc = 0; n = 0; }
        if (every > 0 && (i + 1) % every == 0) {
            hash_arena(h, 0);
            printf("HASH frame %d all %016llx", i + 1, (unsigned long long)h[0]);
            for (int r = 0; r < NREG; r++) printf(" %s %016llx", k_regions[r].name, (unsigned long long)h[1 + r]);
            printf("\n");
        }
    }
    if (track) printf("DIST final frames %d %.3f mean %.3f per s\n", done, g_dist, done ? g_dist / (done / 60.0) : 0.0);
    hash_arena(h, getenv("JT_MAP") != NULL);
    printf("FINAL frames %d state %d mean %.3f ms/frame all %016llx", done, rx_session_get_state(s), done ? total / done : 0, (unsigned long long)h[0]);
    for (int r = 0; r < NREG; r++) printf(" %s %016llx", k_regions[r].name, (unsigned long long)h[1 + r]);
    printf("\n");
    return 0;
}
