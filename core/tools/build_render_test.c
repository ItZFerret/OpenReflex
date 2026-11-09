/* build_render_test.c -- test driver of the render-side asset loaders (core/notes/construction.md section 9.6): the assets unit of
 * build_test.c (the Database and the reference boot's load script) with what the loaders of the render types need -- the executable's
 * .rdata / .data, XenonCore [0x00a95844] = [0x00a95960] with the rx_d3d device -- then the canonical dump + image + anchors.
 *   build_render_test <install root> <out prefix> <script> [skip-tbldata]
 * Anchor A:10867c70=<core Database> (the original's Database of the reference world). STAND-INS: the PE data, the engine object. */
#include "../src/fmt_db.h"
#include "../src/rx_heap.h"
#include "../src/rx_d3d.h"
#include "../game/build_alloc.h"
#include "../game/build_asset.h"
#include "../game/build_load.h"
#include "../game/build_world.h"
#include "../game/build_forest.h"
#include "../game/build_render_asset.h"
#if __has_include("../game/build_water.h")
#include "../game/build_water.h"
#define HAVE_WATER 1
#endif
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
static void on_abort(int sig) { void* bt[40]; int n = backtrace(bt, 40); (void)sig; backtrace_symbols_fd(bt, n, 2); _exit(134); }

static int load_data(fmt_install* in) {
    fmt_buf b = {0};
    if (fmt_vfs_read_file(fmt_install_vfs(in), "MXReflex.exe", &b) != 0) { fprintf(stderr, "MXReflex.exe: %s\n", fmt_error()); return 0; }
    const uint8_t* p = b.data; uint32_t pe = (uint32_t)p[0x3c] | (uint32_t)p[0x3d] << 8;
    uint32_t nsec = (uint32_t)p[pe + 6] | (uint32_t)p[pe + 7] << 8, opt = (uint32_t)p[pe + 0x14] | (uint32_t)p[pe + 0x15] << 8;
    for (uint32_t s = 0; s < nsec; s++) {
        const uint8_t* h = p + pe + 0x18 + opt + 0x28 * s;
        if (memcmp(h, ".data", 5) && memcmp(h, ".rdata", 6)) continue;
        uint32_t va, raw, off; memcpy(&va, h + 12, 4); memcpy(&raw, h + 16, 4); memcpy(&off, h + 20, 4);
        for (uint32_t k = 0; k < raw; k++) rx_wr8(0x00400000 + va + k, p[off + k]);
    }
    fmt_buf_free(&b);
    rx_load_runtime_constants();
    return 1;
}

/* the render-side loaders, wrapped: each asset gets an anchor H:<handle vtable>:<name>=<asset> (the original's handle of that name, its +0x44
 * asset, is paired with it) so a level other than the reference one (no Database anchor) can be compared asset by asset */
typedef struct { const char* type; uint32_t io_vt, h_vt; rx_addr (*create)(rx_stream*); } wtype;
static rx_addr w_forest(rx_stream* s) { return rx_forest_asset_io_create(s); }
static rx_addr w_shader(rx_stream* s) { return rx_shader_asset_io_create(s); }
#ifdef HAVE_WATER
static rx_addr w_water(rx_stream* s) { return rx_water_asset_io_create(s); }
#endif
static const wtype WT[] = {
    {"forest", 0x0093a0c0, 0x0095b56c, w_forest}, {"shader", 0x00971bec, 0x00971aa4, w_shader},
#ifdef HAVE_WATER
    {"water", 0x00972a88, 0x00972a60, w_water},
#endif
};
#define NWT ((int)(sizeof WT / sizeof WT[0]))
static FILE* g_anch;
static int g_wcount[8];
static rx_addr run_w(int k, rx_stream* s, rx_addr name) {
    rx_addr a = WT[k].create(s);
    char nm[0x41]; rx_ld_gets(name, nm, sizeof nm);
    if (g_anch) fprintf(g_anch, "H:%x:%s=%08x\n", WT[k].h_vt, nm, a);
    g_wcount[k]++;
    if (s->short_read || s->pos != s->size) fprintf(stderr, "%s %s: %zu of %zu bytes read%s\n", WT[k].type, nm, s->pos, s->size, s->short_read ? " (short read)" : "");
    return a;
}
static rx_addr wt0(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_w(0, s, n); }
static rx_addr wt1(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_w(1, s, n); }
static rx_addr wt2(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_w(2, s, n); }
static const rx_asset_loader WTF[3] = {wt0, wt1, wt2};

typedef struct { char type[16], name[128]; uint32_t serial; int done; } new_op;
static new_op g_new[256]; static int g_nnew, g_next_new;
static void run_new(rx_addr db, new_op* o) {
    if (o->done) return;
    if (U32(db, 0xe8) != o->serial) fprintf(stderr, "new %s at serial %u, expected %u\n", o->name, U32(db, 0xe8), o->serial);
    rx_database_add_asset(db, o->name, 0, rx_asset_tag_lo(o->type), rx_asset_tag_hi(o->type)); o->done = 1;
}
static void between(rx_addr db) {
    while (g_next_new < g_nnew && (g_new[g_next_new].done || g_new[g_next_new].serial == U32(db, 0xe8))) run_new(db, &g_new[g_next_new++]);
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: build_render_test <install> <out prefix> <script> [skip-tbldata]\n"); return 2; }
    if (getenv("RX_BT")) signal(SIGABRT, on_abort);
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (!load_data(in)) return 1;
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    FILE* anchors = fopen(path, "w");
    g_anch = anchors;
    FILE* sc = fopen(argv[3], "r");
    if (!sc) { fprintf(stderr, "script %s\n", argv[3]); return 1; }
    rx_addr core = rx_reflex_aligned_alloc(0x1f0), decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(core, 0, 0x009763a4);
    SU32(0, 0x00a95844, core); SU32(0, 0x00a95960, core);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(decls, 0x50, 0);   /* no refraction capability */
    SU32(core, 0x14, decls);
    SU32(core, 0x3c + 8, 0x500); SU32(core, 0x3c + 0xc, 0x2d0); SU32(core, 0x3c + 0x28, 2);   /* the display: 1280 x 720, texture quality 2 (no mip reduction) */
    SU32(core, 0x1ec, rx_d3d_device_create());
    rx_asset_between_loads(between);
    rx_asset_register_core_loaders();
    for (int k = 0; k < NWT; k++) rx_asset_set_loader(WT[k].io_vt, WTF[k]);
    if (argc > 4 && !strcmp(argv[4], "skip-tbldata")) rx_asset_set_loader(0x0093a184, NULL);
    {   char l2[1024]; new_op o;
        while (fgets(l2, sizeof l2, sc)) if (sscanf(l2, "new %15s %127s %u", o.type, o.name, &o.serial) == 3 && g_nnew < 256) { o.done = 0; g_new[g_nnew++] = o; }
        rewind(sc); }
    rx_addr db = rx_database_create();
    SU32(core, 8, db);
    char line[1024];
    int dbs = 0, pkgs = 0, assets = 0, failed = 0;
    while (fgets(line, sizeof line, sc)) {
        char a[256], b[256], c[64], d[64];
        if (line[0] == '#' || line[0] == '\n') continue;
        if (!strncmp(line, "register", 8)) rx_database_register_all(db);
        else if (!strncmp(line, "cacheon", 7)) rx_database_cache_enable(db);
        else if (sscanf(line, "cacheadd %255s %255s", a, b) == 2) rx_database_cache_add(db, a, b);
        else if (sscanf(line, "db %255s %255s %63s %63s", a, b, c, d) == 4) {
            if (!rx_database_load(db, in, a, b, (uint32_t)strtoul(c, NULL, 16), (uint32_t)strtoul(d, NULL, 16))) { fprintf(stderr, "database %s: not loaded\n", a); failed++; }
            dbs++;
        } else if (sscanf(line, "pkg %255s %255s %63s %63s", a, b, c, d) == 4) {
            int n = rx_database_load_package(db, in, a, b, 0, (uint32_t)strtoul(d, NULL, 16));
            if (n < 0) { fprintf(stderr, "package %s/%s: unknown\n", a, b); failed++; } else assets += n;
            pkgs++;
        } else if (sscanf(line, "new %255s %255s", a, b) == 2) {
            for (int k = 0; k < g_nnew; k++) if (!g_new[k].done && !strcmp(g_new[k].name, b)) { run_new(db, &g_new[k]); break; }
            while (g_next_new < g_nnew && g_new[g_next_new].done) g_next_new++;
        }
        else if (sscanf(line, "unload %255s %255s", a, b) == 2) rx_database_unload_package(db, a, b);
        else if (sscanf(line, "requests %63s", c) == 1) for (unsigned long k = strtoul(c, NULL, 10); k; k--) rx_database_request(db);
    }
    fclose(sc);
    rx_canon_root root = {db, 0};
    if (!getenv("RX_NO_DB_ANCHOR")) fprintf(anchors, "A:10867c70=%08x\n", db);
    fclose(anchors);
    printf("databases %d, packages %d, assets %d, serial %u, failed %d\n", dbs, pkgs, assets, U32(db, 0xe8), failed);
    for (int k = 0; k < NWT; k++) printf("%s: %d built\n", WT[k].type, g_wcount[k]);
    char info[256] = "";
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    if (!rx_canon_write(path, &root, 1, getenv("RX_CANON_CLASSES"), info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    printf("heap blocks %d; %s\n", rx_heap_blocks(), info);
    return 0;
}
