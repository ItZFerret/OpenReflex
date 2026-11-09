/* build_scene_test.c -- test driver of construction unit 4 (scene; core/notes/construction.md section 12): loads the subscenes of a level
 * through the entity factory into the core's arena, then writes the canonical dump + image (rx_canon.h) and an anchor file for
 * core/tools/build_cmp.py / rx_canon_cmp.py.
 *   build_scene_test <install root> <out prefix> <level> <subscene[+subscene...]> [orig anchors file] [asset script]
 * The orig anchors file ("<name> <hex>" lines) gives the original's address of the scene manager ("scene"), of the physics world ("physworld").
 * Setup (test scaffolding, not product code; each part belongs to another unit, marked STAND-IN):
 *   - the executable's .rdata and initialized .data (unit 12), the runtime vector constants;
 *   - the Database: the asset script (core/tools/build_script_concord.txt) or EngineDependencies + the level database and its Shared /
 *     <level>Base packages (unit 1), every core asset loader registered (units 2b-2g);
 *   - XenonCore at [0x00a95844] = [0x00a95960] (+8 Database, +0x14 the vertex declaration cache, +0x20 the event scene, +0x1ec the rx_d3d
 *     device), MXGame [0x00a946b8], an MXWorld (+0x5a54) -- units 12 / 3;
 *   - the physics world (unit 3) and the height-map texture asset of the terrain (unit 2g). */
#include "../src/fmt_db.h"
#include "../src/fmt_scene.h"
#include "../src/fmt_terrain.h"
#include "../src/rx_heap.h"
#include "../src/rx_d3d.h"
#include "../game/build_alloc.h"
#include "../game/build_asset.h"
#include "../game/build_world.h"
#include "../game/build_terrain.h"
#include "../game/build_render.h"
#include "../game/build_collision.h"
#include "../game/build_mxworld.h"
#include "../game/build_load.h"
#include "../game/build_scene.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <unistd.h>
#include <execinfo.h>
static void on_abort(int sig) { void* bt[40]; int n = backtrace(bt, 40); (void)sig; backtrace_symbols_fd(bt, n, 2); _exit(134); }

static rx_canon_root roots[4096]; static int nroots;
static void root(rx_addr a) { roots[nroots].addr = a; roots[nroots].key = (uint32_t)nroots; nroots++; }

typedef struct { char name[64]; uint32_t addr; } orig_anchor;
static orig_anchor oa[256]; static int noa;
static uint32_t orig_of(const char* name) { for (int i = 0; i < noa; i++) if (!strcmp(oa[i].name, name)) return oa[i].addr; return 0; }
static FILE* anchors;
static void anchor(const char* name, rx_addr core) { if (orig_of(name)) fprintf(anchors, "A:%08x=%08x\n", orig_of(name), core); }

/* STAND-IN unit 12: the exe's .rdata and initialized .data */
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

/* STAND-IN unit 1: the asset loads */
static void load_assets(fmt_install* in, rx_addr db, const char* level, const char* script) {
    rx_asset_register_core_loaders();
    rx_database_register_all(db);
    if (script) {
        FILE* sc = fopen(script, "r"); char line[1024];
        if (!sc) { fprintf(stderr, "script %s\n", script); exit(1); }
        while (fgets(line, sizeof line, sc)) {
            char a[256], b[256], c[64], d[64];
            if (line[0] == '#' || line[0] == '\n' || !strncmp(line, "register", 8)) continue;
            if (!strncmp(line, "cacheon", 7)) rx_database_cache_enable(db);
            else if (sscanf(line, "cacheadd %255s %255s", a, b) == 2) rx_database_cache_add(db, a, b);
            else if (sscanf(line, "db %255s %255s %63s %63s", a, b, c, d) == 4) rx_database_load(db, in, a, b, (uint32_t)strtoul(c, NULL, 16), (uint32_t)strtoul(d, NULL, 16));
            else if (sscanf(line, "pkg %255s %255s %63s %63s", a, b, c, d) == 4) rx_database_load_package(db, in, a, b, 0, (uint32_t)strtoul(d, NULL, 16));
            else if (sscanf(line, "new %255s %255s", a, b) == 2) rx_database_add_asset(db, b, 0, rx_asset_tag_lo(a), rx_asset_tag_hi(a));
            else if (sscanf(line, "unload %255s %255s", a, b) == 2) rx_database_unload_package(db, a, b);
            else if (sscanf(line, "requests %63s", c) == 1) for (unsigned long k = strtoul(c, NULL, 10); k; k--) rx_database_request(db);
        }
        fclose(sc);
    } else {
        char base[256];
        rx_database_load(db, in, "EngineDependencies", "Database\\", 0, 0);
        rx_database_load_package(db, in, "EngineDependencies", "EngineDependencies", 0, 0);
        rx_database_load(db, in, level, "Database\\", 0, 0);
        rx_database_load_package(db, in, level, "Shared", 0, 0);
        snprintf(base, sizeof base, "%sBase", level);
        rx_database_load_package(db, in, level, base, 0, 0);
    }
}

/* STAND-IN unit 2g: a TextureAsset with an R32F texture holding raw floats */
static rx_addr texture_asset_r32f(rx_addr dev, const float* data, uint32_t w, uint32_t h) {
    rx_addr a = rx_reflex_aligned_alloc(0x24), holder = rx_reflex_aligned_alloc(4);
    SU32(a, 0, 0x009462d0); SU32(a, 0x10, holder);
    rx_d3d_create_texture(dev, w, h, 1, 0, RX_D3DFMT_R32F, RX_D3DPOOL_MANAGED, holder);
    uint32_t pitch; rx_addr bits = rx_d3d_bits(U32(holder, 0), 0, &pitch);
    for (uint32_t y = 0; y < h; y++) for (uint32_t x = 0; x < w; x++) SF32(bits + y * pitch, 4 * x, data[y * w + x]);
    return a;
}

/* STAND-IN unit 12 (Game::Game 00882080 -> SceneManager FUN_007d14e0 / FUN_007df410): the scene manager's lists */
static void srv_list(rx_addr base, uint32_t off, uint32_t cap, uint32_t grow) {   /* {data, count, capacity, growth, max} */
    SU32(base, off, rx_reflex_aligned_alloc(cap * 4)); SU32(base, off + 4, 0); SU32(base, off + 8, cap); SU32(base, off + 0xc, grow); SU32(base, off + 0x10, 0x7fffffff);
}
static void srv_vec(rx_addr v, uint32_t reserve) {   /* a checked std::vector<T*> (proxy block, reserve) */
    rx_addr px = rx_reflex_aligned_alloc(4); SU32(px, 0, v); SU32(v, 0, px);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    if (reserve) { rx_addr b = rx_reflex_aligned_alloc(reserve * 4); SU32(v, 0xc, b); SU32(v, 0x10, b); SU32(v, 0x14, b + reserve * 4); }
}
static void srv_namemap(rx_addr t) {   /* FUN_00843480: 50 buckets */
    rx_htab_init_50(t);
}
/* STAND-IN FUN_007df410 (the lights / render container at scene + 0x1a18), data part: the list headers the light entities and the
 * instanced masters append to. Not built: the critical sections (+0x1114, +0x112c), the events and the four render worker threads, the
 * grid FUN_007dfba0 (+0x10f0, built by the event scene only) */
static void srv_el1360(rx_addr el) {   /* Fn_007d1360 */
    SU32(el, 0x10, 0); SF32(el, 0x14, 0.0f);
    SU32(el, 0, U32(0, 0x00a95d50)); SU32(el, 4, U32(0, 0x00a95d54)); SU32(el, 8, U32(0, 0x00a95d58));
}
static void srv_elist(rx_addr base, uint32_t off, uint32_t n, uint32_t stride, uint32_t grow) {   /* 007f9de0 / 007f9f20 */
    rx_addr d = rx_reflex_aligned_alloc(n * stride);
    for (uint32_t k = 0; k < n; k++) srv_el1360(d + stride * k);
    SU32(base, off, d); SU32(base, off + 4, 0); SU32(base, off + 8, n); SU32(base, off + 0xc, grow); SU32(base, off + 0x10, 0x7fffffff);
}
static void srv_lights(rx_addr L) {
    srv_list(L, 0x1000, 0x32, 0x32); srv_list(L, 0x1014, 4, 4); srv_list(L, 0x1028, 0x32, 0x32);
    srv_list(L, 0x103c, 0x400, 0x400); srv_list(L, 0x1050, 0x100, 0x100); srv_list(L, 0x1064, 0x400, 0x400);
    srv_list(L, 0x1078, 0x400, 0x400); srv_list(L, 0x108c, 0x400, 0x400); srv_list(L, 0x10a0, 0x400, 0x400);
    srv_elist(L, 0x10b4, 100, 0x20, 100); srv_elist(L, 0x10c8, 0x40, 0x20, 0x40); srv_elist(L, 0x10dc, 100, 0x40, 100);
}
static rx_addr rx_scene_manager_standin(void) {
    rx_addr s = rx_reflex_aligned_alloc(0x2d20);
    for (uint32_t k = 0; k < 0x2d20; k += 4) SU32(s, k, 0);
    uint32_t cap = 10000, grow = 50;   /* the event scene: FUN_007d14e0(.., 1, 10000, 0x32) */
    srv_namemap(s + 0x19d4); srv_namemap(s + 0x19f8);
    srv_list(s, 0x2bfc, 0x10, 0x10);
    srv_list(s, 0x2c28, grow, grow);
    srv_vec(s + 0x2c10, grow); srv_vec(s + 0x2c3c, grow); srv_vec(s + 0x2c54, grow); srv_vec(s + 0x2c6c, 10); srv_vec(s + 0x2c84, 0xc8);
    SU32(s, 0x2cc4, 1); SU32(s, 0x19f4, 1); SU32(s, 0x2cb0, 1); SU32(s, 0x2cb4, 1); SF32(s, 0x2d14, 1.0f);
    for (uint32_t k = 0; k < 64; k += 4) SU32(s, 0x2cd0 + k, U32(0, 0x009cb510 + k));
    SU32(s, 0x19d0, 1);   /* STAND-IN: the scene's render pass count (InstancedAnimationEntity::vf5 divides the master's +0x320 by it) */
    srv_lights(s + 0x1a18);
    (void)cap;
    return s;
}

/* the asset loader registers the table loaders of unit 12a (build_tables*, not linked here: the scene loads no tables) */

/* the asset loader registers the table loader of unit 12a (build_tables*, not linked: its string compare needs the CRT's locale data;
 * the scene's packages are loaded with the tables left unloaded) */
__attribute__((weak)) void rx_tbl_register_loader(void) {}
__attribute__((weak)) void rx_tbl_row_count(void) { fprintf(stderr, "rx_tbl_row_count: tables unit not linked\n"); abort(); }

/* the rider unit's constructors reach two vehicle-unit allocators the scene never runs: weak stand-ins so the vehicle files need not be linked */
__attribute__((weak)) void rx_veh_map_head_alloc(void) { fprintf(stderr, "rx_veh_map_head_alloc: vehicle unit not linked\n"); abort(); }
__attribute__((weak)) void rx_veh_ranged3_alloc(void) { fprintf(stderr, "rx_veh_ranged3_alloc: vehicle unit not linked\n"); abort(); }

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: build_scene_test <install> <out prefix> <level> <subscenes> [orig anchors] [asset script]\n"); return 2; }
    if (getenv("RX_BT")) signal(SIGABRT, on_abort);
    if (!rx_mem_init()) { fprintf(stderr, "arena reservation failed\n"); return 1; }
    if (argc > 5) {
        FILE* f = fopen(argv[5], "r");
        if (f) { while (noa < 256 && fscanf(f, "%63s %x", oa[noa].name, &oa[noa].addr) == 2) noa++; fclose(f); }
    }
    fmt_install* in = fmt_install_open(argv[1]);
    if (!in) { fprintf(stderr, "install: %s\n", fmt_error()); return 1; }
    fmt_level* lv = fmt_level_open(in, argv[3]);
    if (!lv) { fprintf(stderr, "level %s: %s\n", argv[3], fmt_error()); return 1; }
    if (!load_data(in)) return 1;
    rx_phys_props_set_install(in);
    /* STAND-INs: XenonCore, MXGame, MXWorld, the scene, the device */
    rx_addr core = rx_reflex_aligned_alloc(0x1f0), game = rx_reflex_aligned_alloc(0x16000), mxw = rx_reflex_aligned_alloc(0x22150);
    rx_addr decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(core, 0, 0x009763a4);
    SU32(0, 0x00a95844, core); SU32(0, 0x00a95960, core); SU32(0, 0x00a946b8, game);
    SU32(game, 0x5a54, mxw); SU32(mxw, 0x20018, 0);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(decls, 0x50, 0);   /* the renderer reports no refraction capability (water asset, FUN_007edb00) */
    SU32(core, 0x14, decls);
    SU32(core, 0x3c + 8, 0x500); SU32(core, 0x3c + 0xc, 0x2d0); SU32(core, 0x3c + 0x28, 2);   /* the display: 1280 x 720, texture quality 2 (the texture loader's mip reduction) */
    SU32(core, 0x2c, 0x004046a0);   /* the entity factory registered by Engine::Startup (XenonCore vf1) */
    rx_addr dev = rx_d3d_device_create();
    SU32(core, 0x1ec, dev);
    rx_addr scene = rx_scene_manager_standin();
    SU32(core, 0x20, scene); SU32(core, 0x1c, 0); SU32(core, 4, 1);   /* the current scene: slot 1 = the event scene */
    rx_addr db = rx_database_create();
    SU32(core, 8, db);
    load_assets(in, db, argv[3], argc > 6 ? argv[6] : NULL);
    if (getenv("RX_SCENE_KEY0")) SU32(0, 0x00a95838, (uint32_t)strtoul(getenv("RX_SCENE_KEY0"), NULL, 0));
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    anchors = fopen(path, "w");
    /* the physics world the collision entities add to (FUN_0075da90) */
    rx_addr pw = rx_physics_world_create(scene);
    SU32(mxw, 0x20134, pw);
    root(pw); anchor("physworld", pw);
    /* the height map of the terrain entity: a texture asset holding the level's R32F map */
    fmt_scene* sc = fmt_level_scene(lv);
    {
        int sub0 = fmt_scene_find_subscene(sc, fmt_level_base_subscene(lv)), ne0 = 0;
        fmt_scene_entities(sc, sub0, &ne0);
        const fmt_entity* rs = fmt_scene_find_type(sc, sub0, "HFTerrainRSData", 0);
        if (rs) {
            fmt_terrain* t = fmt_terrain_load(lv);
            if (!t) { fprintf(stderr, "terrain: %s\n", fmt_error()); return 1; }
            rx_addr hm = rx_rs_asset_handle(rs, "HeightMapAsset");
            if (!U32(hm, 0x44)) SU32(hm, 0x44, texture_asset_r32f(dev, t->heights, (uint32_t)t->size, (uint32_t)t->size));
            fmt_terrain_free(t);
        }
    }
    char subs[1024]; snprintf(subs, sizeof subs, "%s", argv[4]);
    for (char* sn = subs, *next; sn && *sn; sn = next) {
        next = strchr(sn, '+'); if (next) *next++ = 0;
        int sub = fmt_scene_find_subscene(sc, sn);
        if (sub < 0) { fprintf(stderr, "no subscene %s\n", sn); return 1; }
        rx_addr arr = rx_scene_load_subscene(scene, sc, sub, sn);
        printf("subscene %s: entity array %08x\n", sn, arr);
    }
    rx_physics_world_install_forest(pw);
    if (getenv("RX_PROBE")) fprintf(stderr, "probe before finalize %08x\n", U32(0, (uint32_t)strtoul(getenv("RX_PROBE"), 0, 16)));
    rx_physics_world_finalize(pw, 0);
    if (getenv("RX_PROBE")) fprintf(stderr, "probe after finalize %08x\n", U32(0, (uint32_t)strtoul(getenv("RX_PROBE"), 0, 16)));
    root(scene); anchor("scene", scene);
    printf("scene %08x physics world %08x\n", scene, pw);
    root(U32(0, 0x00a95844));
    fclose(anchors);
    char info[256] = "";
    snprintf(path, sizeof path, "%s-canon.bin", argv[2]);
    if (!rx_canon_write(path, roots, nroots, getenv("RX_CANON_CLASSES"), info, sizeof info)) return 1;
    snprintf(path, sizeof path, "%s-world.bin", argv[2]);
    if (!rx_canon_image(path)) return 1;
    printf("heap blocks %d; %s\n", rx_heap_blocks(), info);
    return 0;
}
