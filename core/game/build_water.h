/* build_water.h -- construction unit "asset_water": the WaterAsset ("water"; XenonWaterAsset, vtable 0x00972a50, 0x4b0 bytes;
 * WaterAssetIO::vf3 007e3ec0 -> FUN_007ede50 (the surface) -> XenonWaterAsset_Load 007f0320).
 *
 * File (version 5): a 0x8c-byte header {u32 magic 0x77770360, u32 version, name}, the surface matrix (16 floats, D3D row vectors), the
 * extents (x, z), three u32 flags {flow map, description map, description holds heights}, then the flow map {u32 w, h; w * h * 4 bytes of
 * G16R16F} and the description map {u32 w, h; with heights: floats scale, offset; w * h * 4 bytes A8R8G8B8 as stored / without: w * h * 4
 * source bytes of which bytes 2 and 1 become B and G, R = A = 0}. Versions below 5 end after the extents (nothing else is built).
 * The surface is the PlatformWaterSurface at asset + 0x10 (a water patch of the physics world's water set, world_node_water.c); its fields
 * (offsets from the surface):
 *   +0x18 the wake object [0xa95790], +0x1c the flow texture (device slot 23, G16R16F), +0x20 0, +0x24 flow width, +0x28 height,
 *   +0x2c width * 4, +0x30 0x70 (the format), +0x34 the description texture (A8R8G8B8, managed), +0x38 / +0x3c the CPU copy of its texels
 *   (ReflexAlignedAlloc(w * h * 4), rows copied by FUN_007ee700), +0x40 width, +0x44 height, +0x48 pitch (w * 4), +0x4c 0x15 (the format),
 *   +0x54 the mesh (0xf0 bytes), +0x490 / +0x494 the height scale / offset (gRiverHeightValue), +0x498 1 once added to the water set, +0x49c the
 *   surface matrix' y translation. The patch fields the physics reads: mesh +0x14 / +0x18 the extents, +0x70 the matrix copy (+0x80 its
 *   second row = the plane normal, +0xa0 its translation = the plane point), +0xb0 / +0xb4 / +0xb8 / +0xbc the box (min x, min z, max x,
 *   max z), +0xc0 the valid-cell mask (FUN_007e5330: +0xc4 size, +0xc8 / +0xcc columns / rows, +0xd0 / +0xd4 cells per mask), +0x20 5.0
 *   (or scale / 2 with heights).
 * The water set (FUN_007ee540, world_node_water.c rx_world_water_set_height): the std::vector of surfaces at 0x00d72ab0 (+0xc begin, +0x10 end,
 * +0x14 capacity; created by the first wake setup or surface with its guard bit [0x00d72aa8] & 1), [0x00d72aac] the wake.
 * Shared render objects, made by the first surface: the ocean [0x00a9578c] (0x14080 bytes, 128-aligned, its spectrum is NOT built: it needs the
 * CRT rand stream of the original's run and only the renderer reads it), the wake [0x00a95790] (0x30f0 bytes: three 512^2 and six 256^2 R32F
 * targets, the Water_Wake shader entries, two vertex declarations; FUN_007eba00 + FUN_007ebd10) and the refraction object [0x00a95794] (0x18). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_water_asset_io_create(rx_stream* s);   /* WaterAssetIO::vf3 007e3ec0 */
/* Ocean_InitSpectrum's draws (w-rng): 1 once an ocean spectrum was drawn this session; seed = the level loader thread's CRT rand() state
 * before its first draw (the renderer's spectrum generator starts there), draws = the draws it made. */
int rx_water_ocean_seed(uint32_t* seed, uint32_t* draws);
#ifdef __cplusplus
}
#endif
