/* build_render_check.c -- consumption test of the render-side asset loaders (core/notes/construction.md section 9.6; unit 2f / 2g).
 *   build_render_check <install root> <level database> [package ...]
 * Loads EngineDependencies and the level's database with all its packages (or the listed ones) through the Database with every core loader
 * registered, the loaders of the types below wrapped so that each one must consume its asset's stream exactly (no short read, position at the
 * end), and prints the number built / failed per type. The forest needs the level's tree, material and shader assets, so a level is
 * loaded as a whole (one process per level: the arena does not free).
 * STAND-INS (test scaffolding): the exe's .rdata / .data, XenonCore [0x00a95844] = [0x00a95960] with the rx_d3d device. */
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

typedef struct { const char* type; uint32_t io_vt; rx_addr (*create)(rx_stream*); int built, bad; } wrapped;
static rx_addr w_forest(rx_stream* s) { return rx_forest_asset_io_create(s); }
static rx_addr w_shader(rx_stream* s) { return rx_shader_asset_io_create(s); }
static rx_addr g_wname;
static rx_addr w_texture(rx_stream* s) { return rx_texture_asset_io_create(s, g_wname); }
#ifdef HAVE_WATER
static rx_addr w_water(rx_stream* s) { return rx_water_asset_io_create(s); }
#endif
static wrapped W[] = {
    {"forest", 0x0093a0c0, w_forest, 0, 0}, {"shader", 0x00971bec, w_shader, 0, 0}, {"texture", 0x00971c24, w_texture, 0, 0},
#ifdef HAVE_WATER
    {"water", 0x00972a88, w_water, 0, 0},
#endif
};
#define NW ((int)(sizeof W / sizeof W[0]))
static rx_addr run_wrapped(int k, rx_stream* s, rx_addr name) {
    g_wname = name;
    rx_addr a = W[k].create(s);
    W[k].built++;
    if (s->short_read || s->pos != s->size) {
        char nm[0x41]; rx_ld_gets(name, nm, sizeof nm);
        fprintf(stderr, "%s %s: %zu of %zu bytes read%s\n", W[k].type, nm, s->pos, s->size, s->short_read ? " (short read)" : "");
        W[k].bad++;
    }
    return a;
}
static rx_addr wf0(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_wrapped(0, s, n); }
static rx_addr wf1(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_wrapped(1, s, n); }
static rx_addr wf2(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_wrapped(2, s, n); }
static rx_addr wf3(rx_stream* s, rx_addr n, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return run_wrapped(3, s, n); }
static const rx_asset_loader WF[4] = {wf0, wf1, wf2, wf3};

/* the tables unit is not linked into the tool: the weak stand-ins build_scene_test.c uses */
__attribute__((weak)) void rx_tbl_register_loader(void) {}
__attribute__((weak)) void rx_tbl_row_count(void) { fprintf(stderr, "rx_tbl_row_count: tables unit not linked\n"); abort(); }
__attribute__((weak)) void rx_veh_map_head_alloc(void) { fprintf(stderr, "rx_veh_map_head_alloc: vehicle unit not linked\n"); abort(); }
__attribute__((weak)) void rx_veh_ranged3_alloc(void) { fprintf(stderr, "rx_veh_ranged3_alloc: vehicle unit not linked\n"); abort(); }

int main(int argc, char** argv) {
    if (argc < 3) { fprintf(stderr, "usage: build_render_check <install> <level database> [package ...]\n"); return 2; }
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    if (!load_data(in)) return 1;
    rx_addr core = rx_reflex_aligned_alloc(0x1f0), decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(core, 0, 0x009763a4);
    SU32(0, 0x00a95844, core); SU32(0, 0x00a95960, core);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(decls, 0x50, 0);   /* no refraction capability */
    SU32(core, 0x14, decls);
    SU32(core, 0x3c + 8, 0x500); SU32(core, 0x3c + 0xc, 0x2d0); SU32(core, 0x3c + 0x28, 2);   /* the display: 1280 x 720, texture quality 2 (no mip reduction) */
    SU32(core, 0x1ec, rx_d3d_device_create());
    rx_addr db = rx_database_create();
    SU32(core, 8, db);
    rx_asset_register_core_loaders();
    rx_database_register_all(db);
    for (int k = 0; k < NW; k++) rx_asset_set_loader(W[k].io_vt, WF[k]);
    rx_database_load(db, in, "EngineDependencies", "Database\\", 0, 0);
    rx_database_load_package(db, in, "EngineDependencies", "EngineDependencies", 0, 0);
    if (!rx_database_load(db, in, argv[2], "Database\\", 0, 0)) { fprintf(stderr, "database %s not loaded\n", argv[2]); return 1; }
    const fmt_database* d = fmt_install_db(in, argv[2]);
    if (!d) { fprintf(stderr, "database %s: %s\n", argv[2], fmt_error()); return 1; }
    for (int p = 0; p < d->npackages; p++) {
        int want = argc <= 3;
        for (int k = 3; k < argc; k++) if (!strcmp(argv[k], d->packages[p].name)) want = 1;
        if (want) rx_database_load_package(db, in, argv[2], d->packages[p].name, 0, 0);
    }
    int bad = 0;
    for (int k = 0; k < NW; k++) { printf("%s: %d built, %d failed\n", W[k].type, W[k].built, W[k].bad); bad += W[k].bad; }
    printf("heap blocks %d\n", rx_heap_blocks());
    return bad ? 1 : 0;
}
