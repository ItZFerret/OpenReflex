/* fmt_terrain.c -- see fmt_terrain.h. Reference: level_export.py (export_terrain, coarse_bounds, export_collision_world),
 * alpine_collision_world.py, terrain_material_export.py, terrain_deform_pack.py. */
#if defined(__clang__)
#  pragma STDC FP_CONTRACT OFF
#endif
#include "fmt_terrain.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

int fmt_terrain_params_read(fmt_level* lv, int subscene, fmt_terrain_params* p)
{
    memset(p, 0, sizeof *p);
    fmt_scene* sc = fmt_level_scene(lv);
    if (!sc) return fmt_fail("%s has no scene", fmt_level_name(lv));
    if (subscene < 0) subscene = fmt_scene_find_subscene(sc, fmt_level_base_subscene(lv));
    int n;
    if (!fmt_scene_entities(sc, subscene, &n)) return -1;
    const fmt_entity* e = fmt_scene_find_type(sc, subscene, "HFTerrainRSData", 0);
    if (!e) return fmt_fail("%s: the scene has no HFTerrain entity", fmt_level_name(lv));
    if (fmt_scene_find_type(sc, subscene, "HFTerrainRSData", 1)) return fmt_fail("%s: more than one HFTerrain entity", fmt_level_name(lv));
    p->entity_id = e->id;
    p->name = fmt_entity_string(e, "Name");
    if (fmt_entity_matrix(e, p->matrix) < 0) return -1;
    p->height_map = fmt_entity_asset(e, "HeightMapAsset");
    p->material = fmt_entity_asset(e, "MaterialAsset");
    p->softness_mask = fmt_entity_asset(e, "SoftnessMask");
    p->deformation = fmt_entity_asset(e, "TerrainDeformation");
    p->tracks = fmt_entity_asset(e, "TerrainTracks");
    p->generic_track = fmt_entity_asset(e, "GenericTrack");
    p->mx_track = fmt_entity_asset(e, "MXTrack");
    p->atv_track = fmt_entity_asset(e, "ATVTrack");
    p->utv_track = fmt_entity_asset(e, "UTVTrack");
    p->pro4_track = fmt_entity_asset(e, "Pro4Track");
    p->prolite_track = fmt_entity_asset(e, "ProLiteTrack");
    p->buggy_track = fmt_entity_asset(e, "BuggyTrack");
    p->friction_zones = fmt_entity_asset(e, "FrictionZones");
    p->grid_enum = fmt_entity_enum(e, "GridResolution", -1);
    p->edge_buffer = fmt_entity_float(e, "EdgeBuffer", 0.0f);
    const fmt_entity* sm = fmt_scene_find_type(sc, subscene, "ShadowMapRSData", 0);
    if (sm) p->volume_shadow_map = fmt_entity_asset(sm, "VolumeShadowMapAsset");
    if (!p->height_map || p->grid_enum < 0 || p->grid_enum > 4) return fmt_fail("%s: terrain entity lacks a height map or a grid resolution", fmt_level_name(lv));
    p->spacing = 1 << p->grid_enum;
    return 0;
}

int fmt_terrain_coarse_bounds(const float* heights, int size, float scale, float* out)
{
    if (size <= 0 || size % FMT_TERRAIN_COARSE_BLOCK) return fmt_fail("terrain: size %d is not a multiple of %d", size, FMT_TERRAIN_COARSE_BLOCK);
    int blocks = size / FMT_TERRAIN_COARSE_BLOCK;
    for (int cz = 0; cz < blocks; cz++) {
        int r0 = cz * FMT_TERRAIN_COARSE_BLOCK, r1 = r0 + FMT_TERRAIN_COARSE_BLOCK + 1 < size ? r0 + FMT_TERRAIN_COARSE_BLOCK + 1 : size;
        for (int cx = 0; cx < blocks; cx++) {
            int c0 = cx * FMT_TERRAIN_COARSE_BLOCK, c1 = c0 + FMT_TERRAIN_COARSE_BLOCK + 1 < size ? c0 + FMT_TERRAIN_COARSE_BLOCK + 1 : size;
            float mn = heights[(size_t)r0 * size + c0], mx = mn;
            for (int r = r0; r < r1; r++)
                for (int c = c0; c < c1; c++) {
                    float v = heights[(size_t)r * size + c];
                    if (v < mn) mn = v;
                    if (v > mx) mx = v;
                }
            out[2 * ((size_t)cz * blocks + cx)] = mn * scale;
            out[2 * ((size_t)cz * blocks + cx) + 1] = mx * scale;
        }
    }
    return 0;
}

void fmt_terrain_free(fmt_terrain* t)
{
    if (!t) return;
    free(t->heights); free(t->coarse);
    free(t);
}

fmt_terrain* fmt_terrain_load(fmt_level* lv)
{
    fmt_terrain* t = (fmt_terrain*)fmt_alloc(sizeof *t);
    if (!t) return NULL;
    if (fmt_terrain_params_read(lv, -1, &t->params) < 0) { free(t); return NULL; }
    fmt_buf raw = { NULL, 0 };
    fmt_texture tex;
    if (fmt_level_read_asset(lv, t->params.height_map, "texture", &raw) < 0) { free(t); return NULL; }
    if (fmt_texture_parse(raw.data, raw.size, &tex) < 0) goto fail;
    if (tex.format != FMT_TEX_R32F || tex.width != tex.height) { fmt_fail("terrain: the height map is not a square R32F texture"); goto fail; }
    t->size = tex.width;
    t->heights = (float*)fmt_alloc((size_t)t->size * t->size * sizeof(float));
    if (!t->heights || fmt_texture_read_r32f(&tex, 0, t->heights) < 0) goto fail;
    for (size_t i = 0; i < (size_t)t->size * t->size; i++) if (!isfinite(t->heights[i])) { fmt_fail("terrain: nonfinite height sample"); goto fail; }
    fmt_buf_free(&raw);
    t->coarse_blocks = t->size / FMT_TERRAIN_COARSE_BLOCK;
    t->coarse = (float*)fmt_alloc((size_t)t->coarse_blocks * t->coarse_blocks * 2 * sizeof(float));
    if (!t->coarse || fmt_terrain_coarse_bounds(t->heights, t->size, FMT_TERRAIN_HEIGHT_SCALE, t->coarse) < 0) { fmt_terrain_free(t); return NULL; }
    t->world = (float)((t->size - 1) * t->params.spacing);
    t->half = t->world / 2;
    return t;
fail:
    fmt_buf_free(&raw);
    fmt_terrain_free(t);
    return NULL;
}

float fmt_terrain_x(const fmt_terrain* t, int column) { return (float)(column * t->params.spacing) - t->half; }
float fmt_terrain_z(const fmt_terrain* t, int row) { return t->half - (float)(row * t->params.spacing); }
float fmt_terrain_y(const fmt_terrain* t, int column, int row) { return t->heights[(size_t)row * t->size + column] * FMT_TERRAIN_HEIGHT_SCALE; }

fmt_material* fmt_terrain_load_material(fmt_level* lv, const fmt_terrain_params* p)
{
    fmt_buf raw;
    if (!p->material || fmt_level_read_asset(lv, p->material, "material", &raw) < 0) { if (!p->material) fmt_fail("terrain has no material"); return NULL; }
    fmt_material* m = fmt_material_parse(raw.data, raw.size);
    fmt_buf_free(&raw);
    return m;
}

int fmt_terrain_load_softness(fmt_level* lv, const fmt_terrain_params* p, uint8_t** rgb, int* width, int* height)
{
    *rgb = NULL; *width = *height = 0;
    if (!p->softness_mask) return fmt_fail("terrain has no softness mask");
    fmt_buf raw;
    if (fmt_level_read_asset(lv, p->softness_mask, "texture", &raw) < 0) return -1;
    fmt_texture tex;
    uint8_t *rgba = NULL, *out = NULL;
    if (fmt_texture_parse(raw.data, raw.size, &tex) < 0) goto fail;
    if (tex.format != FMT_TEX_DXT1 && tex.format != FMT_TEX_DXT5) { fmt_fail("softness mask: unexpected format %s", fmt_texture_format_name(tex.format)); goto fail; }
    size_t n = (size_t)tex.width * tex.height;
    rgba = (uint8_t*)fmt_alloc(n * 4);
    out = (uint8_t*)fmt_alloc(n * 3);
    if (!rgba || !out || fmt_texture_decode_rgba8(&tex, 0, 1, rgba) < 0) goto fail;
    for (size_t i = 0; i < n; i++) { out[3 * i] = rgba[4 * i]; out[3 * i + 1] = rgba[4 * i + 1]; out[3 * i + 2] = rgba[4 * i + 2]; }
    free(rgba); fmt_buf_free(&raw);
    *rgb = out; *width = tex.width; *height = tex.height;
    return 0;
fail:
    free(rgba); free(out); fmt_buf_free(&raw);
    return -1;
}

void fmt_zones_free(fmt_zones* z)
{
    if (!z) return;
    for (int i = 0; i < 4; i++) { free(z->words[i]); z->words[i] = NULL; }
}

int fmt_zones_parse(const uint8_t* data, size_t size, fmt_zones* z)
{
    memset(z, 0, sizeof *z);
    fmt_bxml* d = fmt_bxml_parse(data, size);
    if (!d) return -1;
    int rc = -1;
    fmt_bxml_value v;
    int n = fmt_bxml_find_child(d, 0, "Interleaved");
    z->interleaved = n >= 0 && fmt_bxml_node_value(d, n, &v) == 0 && v.count >= 1 && fmt_bx_u32(&v, 0) != 0;
    for (int i = 0; i < 4; i++) {
        char name[32];
        int node;
        snprintf(name, sizeof name, "Width%d", i + 1);   node = fmt_bxml_find_child(d, 0, name);
        if (node < 0 || fmt_bxml_node_value(d, node, &v) < 0 || v.count < 1) { fmt_fail("zones: %s missing", name); goto done; }
        z->width[i] = (int)fmt_bx_u32(&v, 0);
        snprintf(name, sizeof name, "Height%d", i + 1);  node = fmt_bxml_find_child(d, 0, name);
        if (node < 0 || fmt_bxml_node_value(d, node, &v) < 0 || v.count < 1) { fmt_fail("zones: %s missing", name); goto done; }
        z->height[i] = (int)fmt_bx_u32(&v, 0);
        snprintf(name, sizeof name, "NumBits%d", i + 1); node = fmt_bxml_find_child(d, 0, name);
        if (node < 0 || fmt_bxml_node_value(d, node, &v) < 0 || v.count < 1) { fmt_fail("zones: %s missing", name); goto done; }
        z->bits[i] = (int)fmt_bx_u32(&v, 0);
        snprintf(name, sizeof name, "Data%d", i + 1);    node = fmt_bxml_find_child(d, 0, name);
        if (node < 0 || fmt_bxml_node_value(d, node, &v) < 0 || v.kind != FMT_BX_UINT) { fmt_fail("zones: %s missing", name); goto done; }
        z->nwords[i] = (int)v.count;
        z->words[i] = (uint32_t*)fmt_alloc(((size_t)v.count + 1) * 4);
        if (!z->words[i]) goto done;
        for (uint32_t k = 0; k < v.count; k++) z->words[i][k] = fmt_bx_u32(&v, k);
        if (z->bits[i] < 1 || z->bits[i] > 8 || z->width[i] < 1 || z->height[i] < 1 ||
            ((uint64_t)z->width[i] * z->height[i] * z->bits[i] + 31) / 32 > (uint64_t)z->nwords[i]) { fmt_fail("zones: map %d is inconsistent", i + 1); goto done; }
    }
    rc = 0;
done:
    fmt_bxml_free(d);
    if (rc < 0) fmt_zones_free(z);
    return rc;
}

int fmt_zones_load(fmt_level* lv, const fmt_terrain_params* p, fmt_zones* z)
{
    memset(z, 0, sizeof *z);
    if (!p->friction_zones) return fmt_fail("terrain has no friction zones");
    fmt_buf raw;
    if (fmt_level_read_asset(lv, p->friction_zones, "packdata", &raw) < 0) return -1;
    int rc = fmt_zones_parse(raw.data, raw.size, z);
    fmt_buf_free(&raw);
    return rc;
}

int fmt_zones_sample(const fmt_zones* z, int map, int x, int y)
{
    if (map < 0 || map > 3 || x < 0 || y < 0 || x >= z->width[map] || y >= z->height[map]) return -1;
    uint64_t bit = ((uint64_t)y * z->width[map] + x) * z->bits[map];
    uint64_t w = z->words[map][bit >> 5];
    if ((bit & 31) + z->bits[map] > 32 && (bit >> 5) + 1 < (uint64_t)z->nwords[map]) w |= (uint64_t)z->words[map][(bit >> 5) + 1] << 32;
    return (int)((w >> (bit & 31)) & ((1u << z->bits[map]) - 1));
}
