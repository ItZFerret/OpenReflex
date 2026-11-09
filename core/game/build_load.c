/* build_load.c -- see build_load.h. */
#include "build_load.h"
#include "build_tables.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_collis.h"
#include "build_xml.h"
#include "build_world.h"
#include "../src/rx_d3d.h"
#include "build_model.h"
#include "build_perf.h"
#include "build_packdata.h"
#include "build_tdf.h"
#include "build_tree.h"
#include "build_terrain_asset.h"
#include "build_render_asset.h"
#include "build_forest.h"
#include "build_water.h"
#include "build_particle.h"
#include "build_partattr.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static rx_addr g_db;
void rx_load_set_database(rx_addr db) { g_db = db; }
rx_addr rx_load_database(void) {
    if (g_db) return g_db;
    rx_addr eng = U32(0, 0x00a95844);        /* the engine global, +8 = the Database */
    return eng ? U32(eng, 8) : 0;
}

uint32_t rx_ld_strlen(rx_addr s) { uint32_t n = 0; while (U8(s, n)) n++; return n; }
void rx_ld_gets(rx_addr s, char* out, uint32_t n) {
    uint32_t i = 0;
    for (; i + 1 < n && U8(s, i); i++) out[i] = (char)U8(s, i);
    out[i] = 0;
}

rx_addr rx_load_find_handle_c(const char* type, const char* name) {
    rx_addr db = rx_load_database();
    return db ? rx_database_find_handle(db, type, name) : 0;
}
rx_addr rx_load_find_handle(const char* type, rx_addr name) {
    char nm[0x100]; rx_ld_gets(name, nm, sizeof nm);
    return rx_load_find_handle_c(type, nm);
}

void rx_ld_vec_init(rx_addr v) {
    rx_addr p = rx_reflex_aligned_alloc(4);
    if (p) { SU32(p, 0, v); SU32(v, 0, p); } else SU32(v, 0, 0);
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
}
void rx_ld_vec_init_crt(rx_addr v) {
    rx_addr p = rx_crt_aligned_malloc(4, 4);
    if (p) { SU32(p, 0, v); SU32(v, 0, p); } else SU32(v, 0, 0);
}
void rx_ld_vec_resize_empty(rx_addr v, uint32_t n, uint32_t elem, int aligned) {
    if (!n) return;
    uint32_t bytes = rx_mul_sat(n, elem);
    /* vector<char> grows through allocator<char> (0x0091619c = operator new = msvcr90 malloc: a raw heap block); vector<T*> /
     * vector<uint> through the game's allocator (ReflexAlignedAlloc) */
    rx_addr b = aligned ? rx_reflex_aligned_alloc(bytes) : rx_crt_malloc(bytes);
    SU32(v, 0xc, b); SU32(v, 0x10, b + bytes); SU32(v, 0x14, b + bytes);
}
void rx_ld_vec_alloc(rx_addr v, uint32_t n, uint32_t elem) {
    SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    if (!n) return;
    uint32_t bytes = rx_mul_sat(n, elem);
    rx_addr b = rx_crt_aligned_malloc(bytes, 16);
    SU32(v, 0xc, b); SU32(v, 0x14, b + bytes); SU32(v, 0x10, b + bytes);
}

/* ---- the loaders by AssetIO vtable ---- */
static rx_addr collis_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_collision_asset_io_create(s); }

static rx_addr bxml_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; return rx_bxml_asset_io_create(s, lo, hi); }

static rx_addr terrain_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_terrain_asset_io_create(s); }
static rx_addr trncol_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_terrain_collision_asset_io_create(s); }

static rx_addr model_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_model_asset_io_create(s); }

static rx_addr perf_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_perf_asset_io_create(s); }

static rx_addr packdata_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_packdata_asset_io_create(s); }

static rx_addr tdf_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_tdf_asset_io_create(s); }

static rx_addr tree_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_tree_asset_io_create(s); }

static rx_addr shader_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_shader_asset_io_create(s); }

static rx_addr texture_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)lo; (void)hi; return rx_texture_asset_io_create(s, name); }

static rx_addr surface_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_surface_asset_io_create(s); }

static rx_addr water_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_water_asset_io_create(s); }

/* ParticleAssetIO::vf3 007e3a10 (build_particle.c) */
static rx_addr particle_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_particle_asset_create(s); }

/* ParticleAttribAssetIO::vf3 006940a0 (build_partattr.c) */
static rx_addr partattr_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_particle_attrib_asset_create(s); }

static rx_addr forest_vf3(rx_stream* s, rx_addr name, uint32_t lo, uint32_t hi) { (void)name; (void)lo; (void)hi; return rx_forest_asset_io_create(s); }

typedef struct { const char* type; uint32_t io_vt; rx_asset_loader fn; } loader_ent;
static const loader_ent g_loaders[] = {
    {"collis", 0x00974f38, collis_vf3},     /* CollisionAssetIO::vf3 00818ef0 (build_collis.c) */
    {"terrain", 0x0093a114, terrain_vf3},   /* TerrainAssetIO::vf3 006d8900 (build_terrain_asset.c) */
    {"trncol", 0x0093a0f8, trncol_vf3},     /* TerrainCollisionAssetIO::vf3 006d8bd0 */
    {"model",  0x009728c0, model_vf3},      /* ModelAssetIO::vf3 007d53c0 (build_model.c) */
    {"perfdat", 0x00976204, perf_vf3},      /* PerformanceAssetIO::vf3 0083ff90 (build_perf.c) */
    {"packdata", 0x00972bb8, packdata_vf3}, /* PackedDataAssetIO::vf3 007f8f30 (build_packdata.c) */
    {"tdf",    0x0093a1a0, tdf_vf3},        /* TDFAssetIO::vf3 0053a0c0 (build_tdf.c) */
    {"tree",   0x0093a0a4, tree_vf3},       /* TreeAssetIO::vf3 00671ee0 (build_tree.c) */
    {"bxml",   0x00975738, bxml_vf3},       /* BXMLAssetIO::vf3 008221f0 (build_xml.c) */
    {"surface", 0x00971c08, surface_vf3},   /* SurfaceAssetIO::vf3 007947b0 (build_render_asset_surface.c) */
    {"texture", 0x00971c24, texture_vf3},   /* TextureAssetIO::vf3 00794810 (build_render_asset_texture.c) */
    {"shader", 0x00971bec, shader_vf3},     /* ShaderAssetIO::vf3 00794780 (build_render_asset.c) */
    {"forest", 0x0093a0c0, forest_vf3},     /* ForestAssetIO::vf3 00661f10 (build_forest.c) */
    {"water",  0x00972a88, water_vf3},      /* WaterAssetIO::vf3 007e3ec0 (build_water.c) */
    {"particle", 0x00972970, particle_vf3},  /* ParticleAssetIO::vf3 007e3a10 (build_particle.c) */
    {"partattr", 0x0093a1d8, partattr_vf3},  /* ParticleAttribAssetIO::vf3 006940a0 (build_partattr.c) */
};
#define NLOADERS ((int)(sizeof g_loaders / sizeof g_loaders[0]))

/* The runtime-initialized vector constants the constructors copy (static initializers of the exe: [0x00d615c0 .. 0x00d616c0), values
 * read from the reference world image). Unit 12 (globals) owns the real initialization; this writes them only when still unset. */
void rx_load_runtime_constants(void) {
    if (U32(0, 0x00d61600) == 0x3f800000u) return;
    static const uint32_t one = 0x3f800000u;
    for (int k = 0; k < 4; k++) {
        SU32(0, 0x00d615d0 + 4 * k, 0x3f000000u);     /* 0.5 splat */
        SU32(0, 0x00d615e0 + 4 * k, 0x7f7fffffu);     /* FLT_MAX splat */
        SU32(0, 0x00d615f0 + 4 * k, 0xff7fffffu);     /* -FLT_MAX splat */
        SU32(0, 0x00d61600 + 20 * k, one);            /* the identity matrix 0x00d61600 */
        SU32(0, 0x00d61650 + 4 * k, one);             /* 1 splat */
        SU32(0, 0x00d61690 + 4 * k, one);             /* 1 splat */
        SU32(0, 0x00d616b0 + 4 * k, 0x7fffffffu);     /* abs mask */
    }
    SU32(0, 0x00d615c0, one);
    SU32(0, 0x00d61640 + 12, one);                    /* (0, 0, 0, 1) */
    SU32(0, 0x00d61680 + 8, one); SU32(0, 0x00d61680 + 12, one);   /* (0, 0, 1, 1) */
    SU32(0, 0x00d616a4, one);                         /* (0, 1, 0, 0) */
}

void rx_load_test_scaffold(void) {
    if (U32(0, 0x00a95960)) return;
    rx_addr core = rx_reflex_aligned_alloc(0x1f0), decls = rx_reflex_aligned_alloc(0x7f0);
    SU32(0, 0x00a95960, core);
    rx_vec_proxy_init16(decls + 0x18); SU32(decls, 0x24, 0); SU32(decls, 0x28, 0); SU32(decls, 0x2c, 0);
    SU32(decls, 0x50, 0);   /* the renderer's refraction capability (the water asset: FUN_007edb00): none on PC */
    SU32(core, 0x14, decls);
    SU32(core, 0x3c + 8, 0x500); SU32(core, 0x3c + 0xc, 0x2d0); SU32(core, 0x3c + 0x28, 2);   /* the display: 1280 x 720, texture quality 2 (no mip reduction) */
    SU32(core, 0x1ec, rx_d3d_device_create());
}

void rx_load_register(void) {
    rx_load_runtime_constants();
    for (int i = 0; i < NLOADERS; i++) rx_asset_set_loader(g_loaders[i].io_vt, g_loaders[i].fn);
    rx_tbl_register_loader();   /* tbldata (TableDataAssetIO 0x93a184): build_tables (unit 12a) */
}
rx_addr rx_load_run(const char* type, rx_stream* s, rx_addr name) {
    for (int i = 0; i < NLOADERS; i++)
        if (!strcmp(g_loaders[i].type, type)) return g_loaders[i].fn(s, name, rx_asset_tag_lo(type), rx_asset_tag_hi(type));
    return 0;
}
