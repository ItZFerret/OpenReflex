/* build_world_test.c -- test driver of construction units 3 (world) and 5 (terrain + collision) (core/notes/construction.md):
 * builds the unit's objects from the install into the core's arena, then writes the canonical dump + image (rx_canon.h) and an
 * anchor file for core/tools/rx_canon_cmp.py.
 *   build_world_test <install root> <out prefix> <level> <units> [orig anchors file] [asset script]
 *     units (comma separated, built in this order): mxworld (FUN_00765e20, before physworld), physworld (FUN_0075da90), terrain (the HFTerrainRSData entity, HFTerrainEntity::vf5),
 *     collision[:<subscene>,...] (every CollisionRSData entity of the Base subscene, or of the listed subscenes, in file order:
 *     CollisionEntity::vf5's collision part + vf28; needs physworld), finalize (FUN_00769030: the grids' extents and rebuild)
 * Setup (test scaffolding, not product code; each part belongs to another unit, marked STAND-IN):
 *   - the executable's .rdata (vtables read by virtual calls) and initialized .data (the raw sections of MXReflex.exe) and the runtime-initialized constants the constructors copy
 *     (0x00d61670 zero vector, 0x00d616a0 (0, 1, 0, 0)) -- unit 12 globals;
 *   - the Database: the asset script (core/tools/build_script_concord.txt) or, without one, EngineDependencies + the level database
 *     and its Shared / <level>Base packages -- unit 1 (build_asset.h);
 *   - XenonCore at [0x00a95844] = [0x00a95960] (+8 Database, +0x14 the vertex declaration cache, +0x20 the event scene, +0x1ec the
 *     rx_d3d device), MXGame [0x00a946b8] (+0x5a54 an MXWorld with +0x20018 = 0), the scene (its +0x1a18 + 0x10c8 light list with
 *     capacity 0x40, +0x2a68 entity array with capacity 0x100), the entities' base fields (Entity::vf5) -- units 12 / 3 / 4;
 *   - texture assets for the HeightMapAsset (TextureAsset 0x24: +0x10 -> holder -> an rx_d3d R32F texture with the map's level 0)
 *     -- unit 2g.
 * Anchors: "A:<orig>=<core>" for the objects whose original address the orig anchors file gives ("<name> <hex>" lines: physworld,
 * terrain = the HFTerrainEntity). */
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
#include "../game/build_collis.h"
#include "rx_canon.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_canon_root roots[4096]; static int nroots;
static void root(rx_addr a) { roots[nroots].addr = a; roots[nroots].key = (uint32_t)nroots; nroots++; }

typedef struct { char name[64]; uint32_t addr; } orig_anchor;
static orig_anchor oa[256]; static int noa;
static uint32_t orig_of(const char* name) { for (int i = 0; i < noa; i++) if (!strcmp(oa[i].name, name)) return oa[i].addr; return 0; }
static FILE* anchors;
static rx_addr g_world;
static void anchor(const char* name, rx_addr core) { if (orig_of(name)) fprintf(anchors, "A:%08x=%08x\n", orig_of(name), core); }

/* STAND-IN unit 12: the exe's .rdata and initialized .data */
static int load_data(fmt_install* in) {
    fmt_buf b = {0};
    if (fmt_vfs_read_file(fmt_install_vfs(in), "MXReflex.exe", &b) != 0) { fprintf(stderr, "MXReflex.exe: %s\n", fmt_error()); return 0; }
    const uint8_t* p = b.data; uint32_t pe = (uint32_t)p[0x3c] | (uint32_t)p[0x3d] << 8;
    uint32_t nsec = (uint32_t)p[pe + 6] | (uint32_t)p[pe + 7] << 8, opt = (uint32_t)p[pe + 0x14] | (uint32_t)p[pe + 0x15] << 8;
    for (uint32_t s = 0; s < nsec; s++) {
        const uint8_t* h = p + pe + 0x18 + opt + 0x28 * s;
        if (memcmp(h, ".data", 5) && memcmp(h, ".rdata", 6)) continue;   /* .rdata: the vtables the virtual calls read */
        uint32_t va, raw, off; memcpy(&va, h + 12, 4); memcpy(&raw, h + 16, 4); memcpy(&off, h + 20, 4);
        for (uint32_t k = 0; k < raw; k++) rx_wr8(0x00400000 + va + k, p[off + k]);
    }
    fmt_buf_free(&b);
    SF32(0, 0x00d616a4, 1.0f);   /* runtime vector (0, 1, 0, 0) */
    for (uint32_t k = 0; k < 16; k += 4) SF32(0, 0x00d61650 + k, 1.0f);   /* runtime vector (1, 1, 1, 1) */
    return 1;
}

static rx_addr collis_loader(rx_stream* st, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_collision_asset_io_create(st); }

/* STAND-IN unit 1: the asset loads */
static void load_assets(fmt_install* in, rx_addr db, const char* level, const char* script) {
    /* the loaders these units consume: collis (unit 2c, build_collis.h); RX_WT_ALL_LOADERS=1: every core loader (the other loaders of
     * build_load.c are other units' work in progress) */
    if (getenv("RX_WT_ALL_LOADERS")) rx_asset_register_core_loaders();
    else rx_asset_set_loader(0x00974f38, collis_loader);
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

/* STAND-IN unit 4: Entity::vf5's base fields the loaders read (scene, name) */
static void entity_base(rx_addr e, rx_addr scene, const char* name) {
    SU32(e, 0x10, scene);
    uint32_t n = (uint32_t)strlen(name);
    rx_addr s = rx_reflex_aligned_alloc(n + 1);
    for (uint32_t k = 0; k <= n; k++) SU8(s, k, (uint8_t)name[k]);
    SU32(e, 0x18, s);
}

int main(int argc, char** argv) {
    if (argc < 5) { fprintf(stderr, "usage: build_world_test <install> <out prefix> <level> <units> [orig anchors] [asset script]\n"); return 2; }
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
    rx_addr scene = rx_reflex_aligned_alloc(0x2d20), decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(core, 0, 0x009763a4);
    SU32(0, 0x00a95844, core); SU32(0, 0x00a95960, core); SU32(0, 0x00a946b8, game);
    SU32(core, 0x20, scene); SU32(game, 0x5a54, mxw); SU32(mxw, 0x20018, 0);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(core, 0x14, decls);
    rx_addr dev = rx_d3d_device_create();
    SU32(core, 0x1ec, dev);
    SU32(scene, 0x1a18 + 0x10c8, rx_reflex_aligned_alloc(0x40 * 0x20)); SU32(scene, 0x1a18 + 0x10cc, 0);
    SU32(scene, 0x1a18 + 0x10d0, 0x40); SU32(scene, 0x1a18 + 0x10d4, 0x40);
    SU32(scene, 0x2a68, rx_reflex_aligned_alloc(0x100 * 4)); SU32(scene, 0x2a6c, 0); SU32(scene, 0x2a70, 0x100); SU32(scene, 0x2a74, 0x100);
    rx_addr db = rx_database_create();
    SU32(core, 8, db);
    load_assets(in, db, argv[3], argc > 6 ? argv[6] : NULL);
    char path[1024];
    snprintf(path, sizeof path, "%s-anchors.txt", argv[2]);
    anchors = fopen(path, "w");
    char units[256]; snprintf(units, sizeof units, "%s", argv[4]);
    for (char* u = strtok(units, ","); u; u = strtok(NULL, ",")) {
        if (!strcmp(u, "physworld")) {
            rx_addr w = g_world = rx_physics_world_create(scene);
            SU32(mxw, 0x20134, w);
            root(w); anchor("physworld", w);
            printf("physics world %08x\n", w);
        } else if (!strcmp(u, "terrain")) {
            fmt_scene* sc = fmt_level_scene(lv);
            int sub = fmt_scene_find_subscene(sc, fmt_level_base_subscene(lv)), ne = 0;
            fmt_scene_entities(sc, sub, &ne);
            const fmt_entity* rs = fmt_scene_find_type(sc, sub, "HFTerrainRSData", 0);
            if (!rs) { fprintf(stderr, "no HFTerrainRSData\n"); return 1; }
            fmt_terrain* t = fmt_terrain_load(lv);
            if (!t) { fprintf(stderr, "terrain: %s\n", fmt_error()); return 1; }
            rx_addr hm = rx_rs_asset_handle(rs, "HeightMapAsset");
            if (!U32(hm, 0x44)) SU32(hm, 0x44, texture_asset_r32f(dev, t->heights, (uint32_t)t->size, (uint32_t)t->size));
            rx_addr e = rx_reflex_aligned_alloc(0x45a0);
            rx_hfterrain_entity_construct_temp(e);
            entity_base(e, scene, t->params.name ? t->params.name : "");
            rx_hfterrain_entity_load(e, rs);
            fmt_terrain_free(t);
            root(e); anchor("terrain", e);
            printf("terrain entity %08x (surface %08x)\n", e, e + 0x90);
        } else if (!strncmp(u, "collision", 9)) {
            fmt_scene* sc = fmt_level_scene(lv);
            char subs[1024]; snprintf(subs, sizeof subs, "%s", u[9] == ':' ? u + 10 : fmt_level_base_subscene(lv));
            int built = 0;
            for (char* sn = subs, *next; sn && *sn; sn = next) {
                next = strchr(sn, '+'); if (next) *next++ = 0;
                int sub = fmt_scene_find_subscene(sc, sn), ne = 0;
                if (sub < 0) { fprintf(stderr, "no subscene %s\n", sn); return 1; }
                const fmt_entity* es = fmt_scene_entities(sc, sub, &ne);
                for (int k = 0; k < ne; k++) {
                    if (!es[k].type || strcmp(es[k].type, "CollisionRSData")) continue;
                    rx_addr e = rx_reflex_aligned_alloc(0x1a0);
                    rx_collision_entity_construct_temp(e);
                    entity_base(e, scene, "");
                    float m[16];
                    if (fmt_entity_matrix(&es[k], m) == 0) for (int j = 0; j < 16; j++) SF32(e, 0x30 + 4u * (uint32_t)j, m[j]);
                    rx_collision_entity_load(e, &es[k]);
                    root(e); built++;
                    if (built == 1) anchor("collision0", e);
                }
            }
            printf("collision entities %d\n", built);
        } else if (!strcmp(u, "mxworld")) {
            /* FUN_00759f00's first step: ReflexAlignedAlloc(0x22150) + the constructor on the config (game + 0x39c0) */
            if (!U32(0, 0x00d6925c)) rx_command_registry_init();   /* STAND-IN unit 12: the static initializer */
            rx_addr cfg = game + 0x39c0;
            rx_addr w = rx_reflex_aligned_alloc(0x22150);
            rx_mxworld_construct(w, cfg);
            SU32(game, 0x5a54, w);
            mxw = w;
            root(w); anchor("mxworld", w);
            printf("MXWorld %08x\n", w);
        } else if (!strcmp(u, "finalize")) {
            rx_physics_world_finalize(g_world, 0);
        } else { fprintf(stderr, "unknown unit %s\n", u); return 2; }
    }
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
