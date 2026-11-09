/* build_test.c -- construction unit test driver (core/notes/construction.md): builds units from the install into the core's arena,
 * then writes the arena's canonical dump + image (rx_canon.h) and an anchor file for core/tools/rx_canon_cmp.py.
 *   build_test <install root> <out prefix> anim [database (Animations)]
 *     every "anim" asset of the database through AnimAssetIO::vf3 (build_anim.h), in package order; anchors H:<AnimAssetHandle
 *     vtable>:<asset name>=<core address> (the original's handle of that name, its +0x44 asset)
 *   build_test <install root> <out prefix> assets <script>
 *     the asset system (build_asset.h): the Database, the 36 AssetIO registrations, then the script's database / package loads in
 *     order (core/tools/build_script_concord.txt: the reference boot's operations); every asset through Database::vf30 with the
 *     core's loaders (anim). Anchor A:<the original's Database 0x10867c70>=<core Database>; <prefix>-assets.txt: the core's asset
 *     loads (name, type, serial) for comparison with the original's trace.
 *   build_test <install root> <out prefix> pool
 *     the small-object pool (build_pool.h: FUN_00744180's memory part) at its original address 0x00d94938; anchor G:00d94a38.
 * Output: <prefix>-canon.bin (+ .addr), <prefix>-world.bin, <prefix>-anchors.txt. */
#include "../src/fmt_db.h"
#include "../src/rx_heap.h"
#include "../game/build_anim.h"
#include "../game/build_asset.h"
#include "../game/build_load.h"
#include "../game/build_pool.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* the script's Database::vf23 calls, run at their serial (between two assets of a package load, or in script order) */
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
static FILE* g_assets;
static void asset_line(const char* l) { fprintf(g_assets, "%s\n", l); }

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: build_test <install> <out prefix> anim [database]\n"); return 2; }
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    FILE* anchors = fopen(path, "w");
    int built = 0, failed = 0, shorts = 0;
    rx_canon_root* roots = NULL; int nroots = 0, cap = 0;
    if (!strcmp(argv[3], "anim")) {
        const char* dbn = argc > 4 ? argv[4] : "Animations";
        const fmt_database* db = fmt_install_db(in, dbn);
        if (!db) { fprintf(stderr, "database %s: %s\n", dbn, fmt_error()); return 1; }
        for (int p = 0; p < db->npackages; p++)
            for (int k = 0; k < db->packages[p].nassets; k++) {
                const fmt_asset* a = &db->packages[p].assets[k];
                if (strcmp(a->type, "anim")) continue;
                fmt_buf b = {0};
                if (fmt_install_read(in, a, &b) != 0) { fprintf(stderr, "read %s: %s\n", a->name, fmt_error()); failed++; continue; }
                rx_stream s; rx_stream_init(&s, b.data, b.size);
                rx_addr asset = rx_anim_asset_io_create(&s);
                if (s.short_read) { shorts++; fprintf(stderr, "short read: %s (%zu bytes)\n", a->name, b.size); }
                else if (s.pos != s.size) fprintf(stderr, "%s: %zu of %zu bytes read\n", a->name, s.pos, s.size);
                fmt_buf_free(&b);
                if (nroots == cap) { cap = cap ? cap * 2 : 1024; roots = (rx_canon_root*)realloc(roots, sizeof *roots * (size_t)cap); }
                roots[nroots].addr = asset; roots[nroots].key = (uint32_t)nroots; nroots++;
                fprintf(anchors, "H:975e94:%s=%08x\n", a->name, asset);
                built++;
            }
    } else if (!strcmp(argv[3], "assets") && argc > 4) {
        FILE* sc = fopen(argv[4], "r");
        if (!sc) { fprintf(stderr, "script %s\n", argv[4]); return 1; }
        snprintf(path, sizeof path, "%s-assets.txt", argv[2]);
        g_assets = fopen(path, "w");
        rx_load_test_scaffold();             /* the renderer stand-in the GPU-resource loaders (terrain, tree, ...) call */
        rx_asset_trace(asset_line);
        rx_asset_between_loads(between);
        rx_asset_register_core_loaders();
        {   char l2[1024]; new_op o;
            while (fgets(l2, sizeof l2, sc)) if (sscanf(l2, "new %15s %127s %u", o.type, o.name, &o.serial) == 3 && g_nnew < 256) { o.done = 0; g_new[g_nnew++] = o; }
            rewind(sc); }
        rx_addr db = rx_database_create();
        char line[1024];
        int dbs = 0, pkgs = 0, assets = 0;
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
        fclose(sc); fclose(g_assets);
        roots = (rx_canon_root*)malloc(sizeof *roots); roots[0].addr = db; roots[0].key = 0; nroots = 1;
        fprintf(anchors, "A:10867c70=%08x\n", db);
        built = assets;
        printf("databases %d, packages %d, assets %d, serial %u\n", dbs, pkgs, assets, U32(db, 0xe8));
    } else if (!strcmp(argv[3], "pool")) {
        rx_lua_memory_init(RX_LUA_SYS);
        fprintf(anchors, "G:00d94a38\n");
        built = 1;
    } else { fprintf(stderr, "unknown unit %s\n", argv[3]); return 2; }
    fclose(anchors);
    char info[256] = "";
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    const char* classes = getenv("RX_CANON_CLASSES");
    if (!rx_canon_write(path, roots, nroots, classes, info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    printf("built %d (failed %d, short reads %d); heap blocks %d; %s\n", built, failed, shorts, rx_heap_blocks(), info);
    return 0;
}
