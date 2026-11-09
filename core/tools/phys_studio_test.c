/* phys_studio_test.c (w-physstudio) -- checks of the PHYSICS STUDIO core (game/phys_studio.c, rx_api.h rx_phys_* / rx_session_phys_*):
 *   phys_studio_test <install> catalog                      the catalog without a session: counts per class and group, the hash
 *   phys_studio_test <install> parse                        garbage / hostile config texts through rx_session_phys_set (no crash,
 *                                                           refused lines reported, values clamped), then a get / set round trip
 *   phys_studio_test <install> verify <type> <typeId> <level> <locationId> <class> <riders> [config file] [live config file]
 *       loads the event with the config (none: an empty one), runs to the race, and compares every parameter component of the local
 *       vehicle(s) with the catalog: an overridden one must hold the config's bits, every other one the file's bits as the catalog read
 *       them. With a live config: rx_session_phys_set + rx_session_phys_apply_live 120 frames into the race, then the same check.
 * Links against the product objects (tools/phys_studio_test.sh). */
#include "rx_api.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char* slurp(const char* path) {
    FILE* f = fopen(path, "rb"); if (!f) return NULL;
    fseek(f, 0, SEEK_END); long n = ftell(f); rewind(f);
    char* b = malloc((size_t)n + 1); if (!b) return NULL;
    if (fread(b, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(b); return NULL; }
    fclose(f); b[n] = 0; return b;
}
static char* catalog(const char* root) {
    uint32_t need = 0;
    rx_phys_catalog(root, NULL, 0, &need);
    char* b = malloc(need ? need : 1);
    if (!b || rx_phys_catalog(root, b, need, &need) != 0) { fprintf(stderr, "catalog failed\n"); exit(1); }
    return b;
}
static int fields(char* line, char** f, int max) {
    int n = 0;
    for (char* q = line; n < max;) { f[n++] = q; char* t = strchr(q, '\t'); if (!t) break; *t = 0; q = t + 1; }
    return n;
}

/* the catalog's V lines: class id, parameter name, component -> default bits */
typedef struct { char cls[16], name[64]; int comp; float def, lo, hi; } vrec;
static vrec* g_v; static int g_nv; static char g_cls[16][16];
static void index_catalog(const char* text) {
    char* t = strdup(text); g_v = calloc(4096, sizeof *g_v);
    char pname[16][128][64];
    for (char* line = strtok(t, "\n"); line; line = strtok(NULL, "\n")) {
        char* f[10]; int n = fields(line, f, 10);
        if (f[0][0] == 'C' && n >= 6) snprintf(g_cls[atoi(f[1])], 16, "%s", f[2]);
        else if (f[0][0] == 'P' && n >= 7) snprintf(pname[atoi(f[1])][atoi(f[2])], 64, "%s", f[3]);
        else if (f[0][0] == 'V' && n >= 9 && g_nv < 4096) {
            vrec* v = &g_v[g_nv++];
            snprintf(v->cls, 16, "%s", g_cls[atoi(f[1])]); snprintf(v->name, 64, "%s", pname[atoi(f[1])][atoi(f[2])]);
            v->comp = atoi(f[3]); v->def = strtof(f[5], NULL); v->lo = strtof(f[6], NULL); v->hi = strtof(f[7], NULL);
        }
    }
    free(t);
}

static int cmd_catalog(const char* root) {
    char* c = catalog(root);
    char* t = strdup(c);
    int classes = 0, params = 0, values = 0, grip = 0, drag = 0; char hash[16] = "";
    char groups[16][32]; int gcount[16][16]; int ng = 0; memset(gcount, 0, sizeof gcount);
    int cur = -1; char cname[16][16];
    for (char* line = strtok(t, "\n"); line; line = strtok(NULL, "\n")) {
        char* f[10]; int n = fields(line, f, 10);
        if (f[0][0] == 'H') snprintf(hash, sizeof hash, "%s", f[1]);
        else if (f[0][0] == 'C' && n >= 6) { classes++; cur = atoi(f[1]); snprintf(cname[cur], 16, "%s", f[2]); }
        else if (f[0][0] == 'P' && n >= 7) {
            params++;
            int g = 0; for (; g < ng; g++) if (!strcmp(groups[g], f[4])) break;
            if (g == ng && ng < 16) snprintf(groups[ng++], 32, "%s", f[4]);
            if (g < 16 && cur >= 0) gcount[g][cur]++;
        }
        else if (f[0][0] == 'V') values++;
        else if (f[0][0] == 'G') grip++;
        else if (f[0][0] == 'D') drag++;
    }
    printf("catalog hash %s: %d classes, %d parameters, %d values, %d grip + %d drag materials, %zu bytes of text\n", hash, classes, params,
           values, grip, drag, strlen(c));
    printf("%-18s", "group");
    for (int k = 0; k < classes; k++) printf(" %12s", cname[k]);
    printf("\n");
    for (int g = 0; g < ng; g++) { printf("%-18s", groups[g]); for (int k = 0; k < classes; k++) printf(" %12d", gcount[g][k]); printf("\n"); }
    if (getenv("PS_CATALOG_OUT")) { FILE* f = fopen(getenv("PS_CATALOG_OUT"), "w"); if (f) { fputs(c, f); fclose(f); } }
    return 0;
}

static int cmd_parse(const char* root) {
    char* c = catalog(root); index_catalog(c);
    rx_session* s = rx_session_create(root);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    static char rep[1 << 16];
    struct { const char* what; const char* text; int want; } cases[] = {
        {"empty", "", 0},
        {"comment only", "# nothing\n\n", 0},
        {"one value", "v\tmx450f\tRearTire_StaticMuSideSlip\t2\t1.5\n", 1},
        {"unknown class", "v\tmx999\tRearTire_Radius\t0\t1\n", 0},
        {"unknown param", "v\tmx450f\tRearTire_Foo\t0\t1\n", 0},
        {"bad component", "v\tmx450f\tRearTire_Radius\t7\t1\n", 0},
        {"negative component", "v\tmx450f\tRearTire_Radius\t-1\t1\n", 0},
        {"nan", "v\tmx450f\tRearTire_Radius\t0\tnan\n", 0},
        {"inf", "v\tmx450f\tRearTire_Radius\t0\tinf\n", 0},
        {"text value", "v\tmx450f\tRearTire_Radius\t0\tabc\n", 0},
        {"trailing junk", "v\tmx450f\tRearTire_Radius\t0\t1.0xyz\n", 0},
        {"huge (clamped)", "v\tmx450f\tMassProps_MainBodyMass\t0\t1e30\n", 1},
        {"tiny (clamped)", "v\tmx450f\tMassProps_MainBodyMass\t0\t-1e30\n", 1},
        {"grip", "g\t4\t1.2\no\tgrip_global\t1\n", 1},
        {"grip bad material", "g\t9\t1\n", 0},
        {"drag", "d\t3\t5\n", 1},
        {"crlf", "v\tatv\tVehicleSim_Gravity\t1\t-20\r\n", 1},
        {"missing fields", "v\tmx450f\n", 0},
        {"too many fields", "v\tmx450f\tRearTire_Radius\t0\t1\t2\t3\n", 0},
    };
    int fails = 0;
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        const int r = rx_session_phys_set(s, cases[i].text, rep, sizeof rep);
        const int ok = r == cases[i].want;
        fails += !ok;
        char* nl; while ((nl = strchr(rep, '\n'))) *nl = ' ';
        printf("%-4s %-20s -> %d (want %d): %s\n", ok ? "ok" : "FAIL", cases[i].what, r, cases[i].want, rep);
    }
    /* random bytes and long lines */
    unsigned seed = 12345; static char junk[1 << 18];
    for (int round = 0; round < 200; round++) {
        size_t n = (size_t)(rand_r(&seed) % (sizeof junk - 1));
        for (size_t k = 0; k < n; k++) { int r = rand_r(&seed) % 8; junk[k] = r == 0 ? '\t' : r == 1 ? '\n' : (char)(rand_r(&seed) % 255 + 1); }
        junk[n] = 0;
        if (round % 3 == 0) memcpy(junk, "v\tmx450f\t", 9);
        (void)rx_session_phys_set(s, junk, rep, sizeof rep);
    }
    printf("ok   200 random texts (up to 256 KB) without a crash\n");
    /* every value at its min, then max: set, read back, equal to the clamp */
    for (int pass = 0; pass < 2; pass++) {
        static char text[1 << 20]; size_t len = 0;
        for (int i = 0; i < g_nv; i++) len += (size_t)snprintf(text + len, sizeof text - len, "v\t%s\t%s\t%d\t%.9g\n", g_v[i].cls, g_v[i].name, g_v[i].comp,
                                                               (double)(pass ? g_v[i].hi + 10.0f * (fabsf(g_v[i].hi) + 1.0f) : g_v[i].lo - 10.0f * (fabsf(g_v[i].lo) + 1.0f)));
        const int r = rx_session_phys_set(s, text, rep, sizeof rep);
        uint32_t need = 0; rx_session_phys_get(s, NULL, 0, &need);
        char* got = malloc(need); rx_session_phys_get(s, got, need, &need);
        int lines = 0, wrong = 0;
        for (char* line = strtok(got, "\n"); line; line = strtok(NULL, "\n")) {
            char* f[6]; if (fields(line, f, 6) != 5) continue; lines++;
            for (int i = 0; i < g_nv; i++) if (!strcmp(g_v[i].cls, f[1]) && !strcmp(g_v[i].name, f[2]) && g_v[i].comp == atoi(f[3])) {
                const float want = pass ? g_v[i].hi : g_v[i].lo;
                if (strtof(f[4], NULL) != want && wrong++ < 3) printf("     %s %s %s: got %s want %.9g\n", f[1], f[2], f[3], f[4], (double)want);
                break;
            }
        }
        free(got);
        fails += r != g_nv || lines != g_nv || wrong;
        printf("%-4s every value past its %s: %d set, %d read back, %d not clamped to the bound\n", r == g_nv && lines == g_nv && !wrong ? "ok" : "FAIL",
               pass ? "max" : "min", r, lines, wrong);
    }
    /* round trip: get -> set -> get */
    {
        const char* cfg = "v\tmx250f\tEngine_MaximumSpeed\t0\t1234.5\nv\tmx450f\tRearTire_StaticMuSideSlip\t1\t0.987654321\ng\t2\t0.55\nd\t0\t0.1\no\tgrip_global\t1\n";
        rx_session_phys_set(s, cfg, rep, sizeof rep);
        static char a[1 << 16], b[1 << 16]; uint32_t need;
        rx_session_phys_get(s, a, sizeof a, &need);
        rx_session_phys_set(s, a, rep, sizeof rep);
        rx_session_phys_get(s, b, sizeof b, &need);
        const int same = strcmp(a, b) == 0;
        fails += !same;
        printf("%-4s get -> set -> get round trip (%zu bytes)\n", same ? "ok" : "FAIL", strlen(a));
    }
    rx_session_phys_set(s, "", rep, sizeof rep);
    printf("active after an empty config: %d\n", rx_session_phys_active(s));
    rx_session_destroy(s);
    printf("%s\n", fails ? "PARSE FAIL" : "PARSE PASS");
    return fails != 0;
}

static int check_live(rx_session* s, const char* given, const char* when) {
    uint32_t need = 0;
    /* the config as the core holds it (clamped to the bounds): what the vehicle must hold */
    char* cfg = NULL;
    if (given) { rx_session_phys_get(s, NULL, 0, &need); cfg = malloc(need); rx_session_phys_get(s, cfg, need, &need); }
    rx_session_phys_live_values(s, NULL, 0, &need);
    char* t = malloc(need); rx_session_phys_live_values(s, t, need, &need);
    int skip = 0, vehicles = 0, checked = 0, bad_file = 0, bad_cfg = 0, overridden = 0; char cls[16] = "";
    for (char* line = strtok(t, "\n"); line; line = strtok(NULL, "\n")) {
        char* f[8]; int n = fields(line, f, 8);
        if (f[0][0] == 'L' && n >= 3) { skip = strncmp(when, "after", 5) == 0 && n >= 5 && atoi(f[4]) == 0 && given && !strstr(given, "o\tall_vehicles\t1");   /* (after a reset to no config: every vehicle back on the file) */ if (skip) continue; vehicles++; snprintf(cls, sizeof cls, "%s", f[2]); printf("  local vehicle %s class %s sim %s\n", f[1], f[2], n > 3 ? f[3] : "?"); continue; }
        if (f[0][0] != 'l' || n < 6 || skip) continue;   /* (a live change reaches the local vehicles, every tracked one with all_vehicles) */
        const uint32_t now = (uint32_t)strtoul(f[4], NULL, 16), file = (uint32_t)strtoul(f[5], NULL, 16);
        /* the config's value for this component, if any */
        char key[160]; snprintf(key, sizeof key, "v\t%s\t%s\t%s\t", cls, f[2], f[3]);
        const char* hit = cfg ? strstr(cfg, key) : NULL;
        checked++;
        if (hit) {
            overridden++;
            float want = strtof(hit + strlen(key), NULL);
            uint32_t wb; memcpy(&wb, &want, 4);
            if (now != wb) { if (bad_cfg++ < 5) printf("  CONFIG MISMATCH %s %s comp %s: now %08x want %08x\n", cls, f[2], f[3], now, wb); }
        } else if (now != file) { if (bad_file++ < 8) printf("  FILE MISMATCH %s %s comp %s: now %08x catalog %08x\n", cls, f[2], f[3], now, file); }
    }
    free(t); free(cfg);
    printf("%s: %d vehicle(s), %d components checked, %d from the config, %d differ from the catalog, %d differ from the config\n",
           when, vehicles, checked, overridden, bad_file, bad_cfg);
    return vehicles > 0 && checked > 0 && !bad_file && !bad_cfg ? 0 : 1;
}

static int cmd_verify(int argc, char** argv) {
    const char* root = argv[1];
    char* cat = catalog(root); (void)cat;
    rx_session* s = rx_session_create(root);
    if (!s) { fprintf(stderr, "create: %s\n", rx_session_last_error(NULL)); return 1; }
    rx_event_spec e; rx_session_event_default(&e);
    snprintf(e.event_type, sizeof e.event_type, "%s", argv[3]); e.event_type_id = atoi(argv[4]);
    snprintf(e.location, sizeof e.location, "%s", argv[5]); e.location_id = atoi(argv[6]);
    e.vehicle_class = atoi(argv[7]); e.riders = atoi(argv[8]);
    char* cfg = argc > 9 && strcmp(argv[9], "-") ? slurp(argv[9]) : NULL;
    char* live = argc > 10 ? slurp(argv[10]) : NULL;
    static char rep[1 << 16];
    rx_session_phys_arm_live(s, 1);
    printf("config: %d value(s): %s", rx_session_phys_set(s, cfg ? cfg : "", rep, sizeof rep), rep);
    if (rx_session_event_load(s, &e)) { fprintf(stderr, "load: %s\n", rx_session_last_error(s)); return 1; }
    int i = 0, racing = -1;
    for (; i < 6000; i++) {
        if (rx_session_advance(s)) { fprintf(stderr, "advance: %s\n", rx_session_last_error(s)); return 1; }
        if (rx_session_get_state(s) == RX_SESSION_RACING && racing < 0) racing = i;
        if (racing >= 0 && i - racing >= 10) break;
    }
    int fail = check_live(s, cfg, "at the race");
    printf("guard: rx_session_phys_active %d (3: a vehicle diverged and got the file's values back)\n", rx_session_phys_active(s));
    if (live) {
        for (int k = 0; k < 120; k++) rx_session_advance(s);
        printf("live config: %d value(s): %s", rx_session_phys_set(s, live, rep, sizeof rep), rep);
        printf("live apply: %d vehicle(s)\n", rx_session_phys_apply_live(s));
        for (int k = 0; k < 60; k++) if (rx_session_advance(s)) { fprintf(stderr, "advance after live: %s\n", rx_session_last_error(s)); return 1; }
        fail |= check_live(s, live, "after the live change");
        rx_session_phys_set(s, "", rep, sizeof rep);
        printf("live apply of no config (back to the file): %d vehicle(s)\n", rx_session_phys_apply_live(s));
        for (int k = 0; k < 60; k++) if (rx_session_advance(s)) { fprintf(stderr, "advance after reset: %s\n", rx_session_last_error(s)); return 1; }
        fail |= check_live(s, NULL, "after the live reset");
    }
    rx_session_destroy(s);
    printf("%s\n", fail ? "VERIFY FAIL" : "VERIFY PASS");
    return fail;
}

int main(int argc, char** argv) {
    if (argc >= 3 && !strcmp(argv[2], "catalog")) return cmd_catalog(argv[1]);
    if (argc >= 3 && !strcmp(argv[2], "parse")) return cmd_parse(argv[1]);
    if (argc >= 9 && !strcmp(argv[2], "verify")) return cmd_verify(argc, argv);
    fprintf(stderr, "usage: phys_studio_test <install> catalog | parse | verify <type> <typeId> <level> <locationId> <class> <riders> [config] [live config]\n");
    return 2;
}
