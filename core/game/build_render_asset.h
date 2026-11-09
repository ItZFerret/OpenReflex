/* build_render_asset.h -- construction unit "asset_render" (2g): the render asset types the race reads or other loaders look up.
 *
 * ShaderAsset ("shader"; XenonShaderAsset, vtable 0x00971a88, 0x894 bytes; ShaderAssetIO::vf3 00794780 -> XenonCore::vf15 00843b80 (the
 * allocation: ReflexAlignedAlloc(0x894), +8 / +0xc 0, the vtable) -> XenonShaderAsset::vf3 007b0fb0 / FUN_007b1040).
 * File: u32 version. Versions 1 and 3: u32 size, then `size` bytes (a program blob: +0x14 size, +0x10 a ReflexAlignedAlloc copy). Version 2
 * (every shader of the install): u32 count, then per program {u32 kind (0 vertex, 1 pixel; other kinds are skipped), u32 name length,
 * the name, u32 size, the SM3 token stream}. Any other first word is kept in +0x14 with +0x18 = 1. Fields:
 *   +0x10 blob (versions 1 / 3), +0x14 its size, +0x18 the version, +0x454 vertex shader count, +0x890 pixel shader count,
 *   vertex entries at +0x1c + 0x48 i (up to 15), pixel entries at +0x458 + 0x48 i (up to 15): {+0 name[0x40], +0x40 the
 *   IDirect3DVertexShader9 / PixelShader9 (device slot 91 / 106), +0x44 the ID3DXConstantTable of the stream (D3DXGetShaderConstantTable)}.
 * The stream is read into a temporary ReflexAlignedAlloc block (size / 4 + 1 words) that is freed after the two D3D calls.
 * Entries are looked up by name from the END (build_render.h rx_shader_find_vs / _ps: materials, the forest, the water). */
#pragma once
#include "rx_game.h"
#include "build_stream.h"
#ifdef __cplusplus
extern "C" {
#endif
rx_addr rx_shader_asset_io_create(rx_stream* s);   /* ShaderAssetIO::vf3 00794780 */
void    rx_shader_asset_destroy(rx_addr asset);               /* 007b13d0 (ECX = the asset) */
rx_addr rx_shader_asset_sdtor(rx_addr asset, uint32_t flags); /* XenonShaderAsset::sdtor 007b0f90 */
rx_addr rx_shader_asset_get_io(rx_addr asset);                /* XenonShaderAsset::vf2 00794720 */

/* TextureAsset ("texture"; XenonTextureAsset, vtable 0x009462d0, 0x24 bytes; TextureAssetIO::vf3 00794810 -> XenonTextureAsset_Create 007b2820).
 * File (core/src/fmt_texture.h): five u32 {0x77DDCC00, version 4, 0, images, total levels}, five u32 {flags, class, ...}, per image {levels, width,
 * height, D3D format}, per level {byte size, pitch}, then the levels' rows (face after face for a cube). Fields: +0x10 the array of D3D textures
 * (one per image; ReflexAlignedAlloc(4 * images)), +0x14 the image count, +0x1c 1 for a cube map (flags & 2).
 * Classes (version 4: header word 6): 0 a managed texture (device slot 23 / 25 CreateTexture / CreateCubeTexture, usage 0, pool MANAGED, the file's
 * level count; a cube's six faces are read level by level), 1 a render target (flags & 1; no data in the file; created through the GPU registry
 * FUN_007b0970 with usage 1, pool DEFAULT; a 0x500 x 0x2d0 target takes the display's size renderer + 0x3c + 8 / 0xc). Not ported (abort): the
 * texture-quality reduction of the top mips (renderer + 0x64 < 2 with [0x00a956fa] set: neither holds in the reference world), versions below 4,
 * the dynamic class 0x200 and flag 8 (no file has them). A level is read row by row into the locked rect (FUN_007b26d0): a level that does not fit
 * is skipped and its image regenerated with D3DXLoadSurfaceFromSurface (the reference boot's stand-in does not copy: nothing here either). */
rx_addr rx_texture_asset_io_create(rx_stream* s, rx_addr name);   /* TextureAssetIO::vf3 00794810 */
/* SurfaceAsset ("surface"; XenonSurfaceAsset, vtable 0x00971cdc, 0x1c bytes; SurfaceAssetIO::vf3 007947b0 -> XenonCore::vf11 00843ac0 -> vf6 007b20f0)
 * (build_render_asset_surface.c). File: 16-byte header {?, version, count, ?}, count 0x160-byte descriptors, then for each descriptor of kind 0 / 1
 * (XenonTriangleSurface::vf1 007b2330) the skin data (version >= 2: descriptor +0xfc bytes), the index data (+0x14 bytes) and the vertex streams
 * (+0xbc.. bytes each) into D3D buffers created through the device. Fields: +0x10 count, +0x14 the 0xcc-byte surface records (4-byte count in front),
 * +0x18 the descriptors. Surface record +4 = descriptor flags bit 1 (read by VehicleLoader_Assemble 0053ddc0). Kinds >= 2 (morph surfaces) are left
 * as constructed. The sdtor (package unload) is not ported. */
rx_addr rx_surface_asset_io_create(rx_stream* s);   /* SurfaceAssetIO::vf3 007947b0 */
rx_addr rx_surface_asset_get(rx_addr asset, uint32_t index);   /* XenonSurfaceAsset::vf4 00794f90 (registered; thiscall, RET 4) */
/* Registered virtuals (core signature (rx_addr this, stack args), EAX result): XenonTextureAsset::sdtor 007b2620 (vtable slot 0; flags; returns this)
 * and XenonTextureAsset::vf2 007b3410 (slot 2: the texture AssetIO). The asset system's Database::vf31 reaches them at refcount 0 through the asset's
 * vtable slot 2 and the AssetIO's slot 4 (rx_asset_io_destroy = AssetIO::vf4 007d3ee0, build_asset.c). Other types stay unregistered: a package
 * unload that takes one of them to 0 aborts naming the address (their sdtors are not ported). */
uint32_t rx_texture_asset_sdtor(rx_addr asset, uint32_t flags);
uint32_t rx_texture_asset_get_io(rx_addr asset);
#ifdef __cplusplus
}
#endif
