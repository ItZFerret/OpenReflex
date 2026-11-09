/* build_terrain_asset.h -- construction unit "asset_terrain" (2e): TerrainAsset (XenonTerrainAsset) and TerrainCollisionAsset objects.
 *
 * TerrainAsset (vtable 0x009624e4, 0xf0 bytes), AssetIO::vf3 006d8900 + the loader FUN_006d9a90 ("terrain" assets of the supercross levels;
 * file: 0x194-byte header, then n = columns * rows patch tables, the index tables, dc patch records and their D3D buffers):
 *   +0x10 n patch count (+0x14 columns * +0x18 rows), +0x14 / +0x18 header words 4 / 5, +0x1c n patch records (0x60 each, in the block),
 *   +0x20 the index tables, +0x30 / +0x34 / +0x38 / +0x40 / +0x44 / +0x48 header floats 6..11, +0x50 / +0x54 = 2 * [+0x40] / columns and
 *   2 * [+0x48] / rows (floats), +0x58 level count (header word 12), +0x5c + 8j {header words 13 + 2j, 14 + 2j} per level,
 *   +0xdc dc draw-patch count (header word 3), +0xe0 the draw-patch records (0x94 each), +0xe4 the vertex declaration (007afd60),
 *   +0xe8 the block (= +0x20). A patch record (+0x1c): +0..+0x3f per level the pointer to its index table, +0x40 / +0x44 / +0x48 and
 *   +0x50 / +0x54 / +0x58 bounds (x87 dword copies). A draw-patch record: +0 the D3D index buffer (INDEX32, managed, usage 8), +4 / +0xc
 *   header words, +8 its byte length, +0x10 the vertex buffer count, +0x14 + 4j the D3D vertex buffers, +0x54 + 4j their lengths.
 *
 * TerrainCollisionAsset (vtable 0x0096251c, 0x2080 bytes), AssetIO::vf3 006d8bd0 + FUN_006d8b10 / FUN_0041d090 (binary form, magic
 * 0x00bada55): a critical section at +0x10, then the collision mesh S = asset + 0x10:
 *   S+0x18..S+0x2018 0x400 zeroed 8-byte entries, S+0x201c the data block (faces, vertices, indices, grid table), S+0x2020 the
 *   faces (0x10 each: 3 vertex pointers, u16 material at +0xc), S+0x2024 face count, S+0x2028 the vertices (0x20 each),
 *   S+0x202c vertex count, S+0x2030 / 34 / 38 / 40 / 44 / 48 header floats, S+0x2050 the index words, S+0x2054 the grid spacing (float),
 *   S+0x2058 / S+0x205c grid columns / rows, S+0x2060 the grid (one pointer per column; each cell a pointer to a counted index list). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_terrain_asset_io_create(rx_stream* s);             /* TerrainAssetIO::vf3 006d8900 */
rx_addr rx_terrain_collision_asset_io_create(rx_stream* s);   /* TerrainCollisionAssetIO::vf3 006d8bd0 */
#ifdef __cplusplus
}
#endif
