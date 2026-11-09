/* build_terrain.h -- construction unit "terrain" (core/notes/construction.md, unit 5): the level's height-field terrain from its
 * HFTerrainRSData scene entity: HFTerrainEntity::vf5 007c8db0 and what it builds (HFTerrainSurface_InitDeformAndTracks 007c4890:
 * grid dimensions, the 16 MB base height grid copied from the HeightMapAsset texture (007cc880), the coarse bounds (007c4d10), the
 * DeformablePatch with its deformation surface (007cacc0), the three TerrainTracks rings (007cb9a0 / 007cbbf0)), the registration
 * in the terrain list 0x009e30d4 (007c4cc0) and in the scene.
 *
 * HFTerrainEntity (vtable 0x00971d6c, 0x45a0 bytes): Entity base (+0x10 the scene, +0x18 the name, +0x30 the matrix ...), +0x90 the
 * embedded HFTerrainSurface, +0x4538 the scene, +0x453c the FrictionZones handle (packdata), +0x4580 EdgeBuffer, +0x4590 0.
 * HFTerrainSurface (vtable 0x00971cfc; offsets from the surface = entity + 0x90): the object the race calls `terrain`
 *   +0x14 the HeightMapAsset handle, +0x1c the base heights (width x height raw R32F samples of the HeightMapAsset; height = raw * 2048),
 *   +0x44 the HFT_Helper shader handle (+0x48..+0x5c / +8 / +0x10 its shaders), +0xa0..+0xb4 the six track texture handles,
 *   +0xb8 SoftnessMask, +0xbc deformation gain 2.0, +0xc0 HFTerrainRenderEvent, +0xc4 = surface, +0xc8 the material handle,
 *   +0xcc HFTerrainTextureRenderEvent, +0xd0 = surface + 0xf0, +0xd4 the coarse bounds (cells x (min, max) * 2048) and +0xd8 the
 *   coarse row length, +0xdc / +0xe0 the terrain's min / max height, +0xe4 the entity, +0xe8 the clip-map mesh (render: not built),
 *   +0xf0 the HFProjectedTexture (0x140), +0x230 the DeformablePatch (+0x10 = +0x240 the R32F SYSTEMMEM deformation surface,
 *   +8 / +0xc its two R32F render targets, +0x20 / +0x24 their surfaces, +0x28 SoftnessMask, +0x424c the vertex declaration,
 *   +0x4250 its stride 0x14, +0x4254..+0x4260 the deform shaders, +0x4264 TerrainDeformation, +0x4268 the surface), +0x44a0 /
 *   +0x44a4 the HFT_Deform_Default / HFT_Track_Default shader handles, +0x44ac 0, +0x44b0 -1, +0x44b4 1 = registered,
 *   +0x44bc width (row stride), +0x44c0 height, +0x44c8 / +0x44cc the same as floats, +0x44d0 / +0x44d4 world size
 *   ((cells * spacing) - spacing), +0x44d8..+0x44e4 min / max x, z (-/+ size / 2), +0x44e8 spacing (1 << GridResolution),
 *   +0x44ec 2 * spacing, +0x44f0 [0x977e98].
 * TerrainTracks (vtable 0x00971d50, 0x38 bytes; [0x00a95774] / [0x00a95778] / [0x00a9577c], rings of 30 / 10 / 5 points): +4 / +8
 *   the ring size, +0xc 0x18 strips, +0x10 index buffer, +0x14 vertex buffer, +0x18 the vertex declaration, +0x1c stride 8, +0x20
 *   0xfe, +0x24 the strips (0x18 x 0xc0: +0 points (ring x 0xc: vertex byte offset, index offset, index byte offset), +0xc ring
 *   x 4 (first word 1), +0x20 (0, 0, 1) the direction [0x9cb4c0], +0x68 -1, +0x94 / +0xa0 / +0xa4 / +0xac 1 ...), +0x30 / +0x34
 *   the track shaders.
 *
 * Render-only steps (Unity draws; construction.md: render-only state is not core) are NOT built and leave their fields 0: the
 * clip-map mesh WaterMesh_Build 007e4280 (+0xe8 and the mesh extents), the shared projected-texture render target of
 * PlatformHFProjectedTexture_Create 007c9160 (its globals 0x00d62780.. / [0x00a95788]), the terrain material's effect parameters
 * (PlatformHFTerrainSurface::vf15 007ccb50: +0x20..+0x40, +0x80..). Shader lookups run (build_render.h) and find nothing while the
 * shader assets are not built.
 *
 * RSData: the property loaders read the entity's fields from fmt_scene (core/src/fmt_scene.h), the original's XMLReader cursor over
 * the same scene chunk; asset fields become the Database's handles (Database::vf27 = rx_database_find_handle). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
struct fmt_entity;

rx_addr rx_hfterrain_surface_construct(rx_addr surface);          /* 007c4630 (EDI = surface) */
rx_addr rx_deformable_patch_construct(rx_addr patch);             /* 007caa10 (ESI = patch) */
rx_addr rx_hfprojected_texture_construct(rx_addr p);              /* 007ba010 (ESI = p) */
void    rx_hfterrain_surface_init(rx_addr surface, int32_t width, int32_t height, int32_t spacing, rx_addr world_size,
                                  rx_addr heightmap_handle, const struct fmt_entity* rs);   /* 007c4890 (EAX = height, EDX = spacing; RET 0x10) */
void    rx_hfterrain_copy_heights(rx_addr surface, rx_addr heightmap_handle);   /* 007cc880 (ESI = surface, EAX = handle) */
void    rx_hfterrain_coarse_bounds(rx_addr surface);              /* 007c4d10 (ESI = surface) */
void    rx_hfterrain_register(rx_addr surface);                   /* 007c4cc0 (EDX = surface) */
void    rx_deformable_patch_create(rx_addr patch, uint32_t w, uint32_t h, rx_addr shader, const struct fmt_entity* rs);   /* 007cacc0 */
rx_addr rx_terrain_tracks_construct(rx_addr tracks, uint32_t ring);   /* 007cb9a0 (ESI = tracks, EAX = ring) */
void    rx_terrain_tracks_create_buffers(rx_addr tracks, rx_addr shader);   /* 007cbbf0 (EAX = tracks; RET 4) */
/* HFTerrainEntity::vf5 007c8db0 after Entity::vf5 (the scene unit's): the terrain from the entity's RSData */
void    rx_hfterrain_entity_load(rx_addr entity, const struct fmt_entity* rs);
/* TEMP until scene unit: the HFTerrainEntity constructor 007c8cf0 (Entity base fields + the surface) */
rx_addr rx_hfterrain_entity_construct_temp(rx_addr entity);

/* RSData helpers shared by the property loaders of units 3 / 5 (the original's FUN_0080ec70 / 0080e180 / 0080e4d0 / 0080e850):
 * an asset field's handle (Database::vf27 of (name, type)); a field's presence; an EnumValue; a Float (0 when absent) */
rx_addr rx_rs_asset_handle(const struct fmt_entity* rs, const char* field);
int     rx_rs_has(const struct fmt_entity* rs, const char* field);
int32_t rx_rs_enum(const struct fmt_entity* rs, const char* field);
float   rx_rs_float(const struct fmt_entity* rs, const char* field);
/* Database::vf27 (name, tag, create = 1): the handle of an asset by type and name (asset-system API) */
rx_addr rx_asset_handle(const char* type, const char* name);

#ifdef __cplusplus
}
#endif
