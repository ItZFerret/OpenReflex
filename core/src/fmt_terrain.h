/* fmt_terrain.h -- the HFTerrain of a level: height map, collision bounds, masks and parameters.
 *
 * The level scene has one RSToolsNS_RSDataNS_HFTerrainRSData entity (in the Base subscene) with the fields Name, Matrix
 * (identity), HeightMapAsset (texture, R32F), MaterialAsset (material, shader HFTerrain_*), SoftnessMask (texture, DXT1 / DXT5),
 * GridResolution (Enum + EnumValue), EdgeBuffer, TerrainDeformation (material HFT_Deform_Default), TerrainTracks (material),
 * GenericTrack / MXTrack / ATVTrack / UTVTrack / Pro4Track / ProLiteTrack / BuggyTrack (tyre track textures) and
 * FrictionZones (packdata). The ShadowMapRSData entity names the VolumeShadowMapAsset.
 *
 * Geometry (alpine_export.py / level_export.py, verified against the original terrain object 007c8db0 / 007c4890):
 *     size = height map width (2048, square), spacing = 1 << GridResolution.EnumValue (Alpine 2, Concord 1),
 *     half = (size - 1) * spacing / 2, world extent (size - 1) * spacing, centred on the origin:
 *         x = column * spacing - half     z = half - row * spacing     y = raw * 2048       (row 0 = the +Z edge)
 *     deformation: every sample starts as the neutral 128/255 (bits 0x3F008081); gain 2.0 (Alpine, live verified).
 *     coarse bounds: (size / 16)^2 pairs (min, max) of raw * 2048 over the 17 x 17 samples of every 16-cell block, the blocks
 *     sharing their edge rows and columns (the last block ends at the grid edge), row-major by block row (z) then column (x)
 *     -- identical to the 128 x 128 table of the live Alpine terrain object, byte for byte.
 * Friction zones (packdata, BXML): Interleaved (bool) and for n = 1..4 Width<n>, Height<n>, NumBits<n>, NumWords<n>, Data<n>
 * (u32 words): four 1024 x 1024 maps of 3 / 2 / 3 / 3 bit codes. The code of sample i is assumed to be bits [i*NumBits ...)
 * of the little-endian bit stream of the words, row-major; the spatial coherence of the result (93% equal neighbours)
 * supports it, but the meaning of the codes, of Interleaved and the orientation are NOT verified. */
#ifndef FMT_TERRAIN_H
#define FMT_TERRAIN_H

#include "fmt_material.h"
#include "fmt_scene.h"
#include "fmt_texture.h"

#ifdef __cplusplus
extern "C" {
#endif

#define FMT_TERRAIN_HEIGHT_SCALE 2048.0f
#define FMT_TERRAIN_COARSE_BLOCK 16
#define FMT_TERRAIN_DEFORM_NEUTRAL_BITS 0x3F008081u      /* 128/255 as float32 */
#define FMT_TERRAIN_DEFORM_GAIN 2.0f

typedef struct fmt_terrain_params {
    uint32_t    entity_id;
    const char* name;
    float       matrix[16];
    const char *height_map, *material, *softness_mask, *deformation, *tracks;
    const char *generic_track, *mx_track, *atv_track, *utv_track, *pro4_track, *prolite_track, *buggy_track;
    const char* friction_zones;               /* packdata asset */
    const char* volume_shadow_map;            /* ShadowMapRSData, may be NULL */
    int         grid_enum;                    /* GridResolution EnumValue */
    int         spacing;                      /* 1 << grid_enum */
    float       edge_buffer;
} fmt_terrain_params;

/* Reads the parameters from the level's scene (Base subscene unless subscene >= 0 names another). Pointers live as long as the scene. */
int fmt_terrain_params_read(fmt_level* lv, int subscene, fmt_terrain_params* out);

typedef struct fmt_terrain {
    fmt_terrain_params params;
    int      size;                             /* samples per side */
    float*   heights;                          /* size * size raw R32F samples (not multiplied), row 0 first */
    int      coarse_blocks;                    /* size / 16 */
    float*   coarse;                           /* coarse_blocks^2 * 2: (min, max) of raw * 2048 */
    float    world;                            /* (size - 1) * spacing */
    float    half;                             /* world / 2 */
} fmt_terrain;

fmt_terrain* fmt_terrain_load(fmt_level* lv);   /* params + height map + coarse bounds */
void         fmt_terrain_free(fmt_terrain* t);
/* Sample position of grid (column, row) in world units (x, z), and the height y = raw * 2048. */
float        fmt_terrain_x(const fmt_terrain* t, int column);
float        fmt_terrain_z(const fmt_terrain* t, int row);
float        fmt_terrain_y(const fmt_terrain* t, int column, int row);
/* The coarse table for any raw height grid (size divisible by 16); out has (size/16)^2 * 2 floats. */
int          fmt_terrain_coarse_bounds(const float* heights, int size, float scale, float* out);

/* The terrain material (nine textures for HFTerrain_4d_cl_s_xtra: world map, detail base / red / green / blue, detail mask,
 * chrominance-luminance map, spherical light map, spherical reflection map; parameters usr_dirt_tint_color, usr_detail_tile,
 * usr_bump_scalar). */
fmt_material* fmt_terrain_load_material(fmt_level* lv, const fmt_terrain_params* p);

/* Softness mask as RGB8, row-major, row 0 = the texture's top = +Z. (DXT1 with the rounded palette of the deformation port;
 * DXT5 uses the colour block only.) *rgb is malloc'ed; free() it. */
int          fmt_terrain_load_softness(fmt_level* lv, const fmt_terrain_params* p, uint8_t** rgb, int* width, int* height);

/* Friction zones (see above). */
typedef struct fmt_zones {
    int      interleaved;
    int      width[4], height[4], bits[4];
    uint32_t* words[4]; int nwords[4];
} fmt_zones;
int          fmt_zones_parse(const uint8_t* packdata, size_t size, fmt_zones* out);
void         fmt_zones_free(fmt_zones* z);
int          fmt_zones_load(fmt_level* lv, const fmt_terrain_params* p, fmt_zones* out);
/* The code of map `map` (0..3) at (x, y), row-major under the assumption above; -1 out of range. */
int          fmt_zones_sample(const fmt_zones* z, int map, int x, int y);

#ifdef __cplusplus
}
#endif
#endif
