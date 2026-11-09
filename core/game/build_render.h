/* build_render.h -- the engine's render-resource bookkeeping that gameplay constructors go through (core/notes/construction.md,
 * "D3D resource layer"): the GPU resource registry, the vertex declaration cache and the shader-asset lookups. The D3D objects
 * themselves are core/src/rx_d3d.h's; this is the game's own code above them (transliterated from the disassembly).
 *
 * GPU resource registry ([0x00a95744], 0x1c bytes, made on first use by FUN_007b0920): +0 container proxy, +0x14 the head node of a
 * std::list of resource records (node 0xc: next, previous, record), +0x18 the list size. A record (0x48 bytes, zeroed) describes how
 * to (re)create one D3D resource after a device reset: +0 kind (0 texture, 1 render target, 2 offscreen surface, 3 cube?, 4 index
 * buffer, 5 vertex buffer), +4 where the object goes, +8 width / length, +0xc height / FVF, +0x10.. kind fields, +0x20 levels,
 * +0x24 usage, +0x28 format, +0x2c pool, +0x30 byte 1 = created, +0x34 the release callback, +0x38 its context, +0x40 / +0x44 the
 * creator's file name and line.
 * Renderer: [0x00a95960] (the platform renderer object, owned by the render unit): +0x14 the vertex declaration cache (its std::vector
 * of IDirect3DVertexDeclaration9* at +0x18), +0x1ec the IDirect3DDevice9.
 * XenonShaderAsset (asset_render unit): +0x454 vertex shader count, +0x1c entries of 0x48 bytes {name[0x40], shader, constant table};
 * +0x890 pixel shader count, +0x458 entries of the same shape. */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif

#define RX_GPU_REGISTRY 0x00a95744u
#define RX_RENDERER     0x00a95960u
static inline rx_addr rx_renderer_device(void) { return U32(U32(0, RX_RENDERER), 0x1ec); }

rx_addr  rx_gpu_registry_init(void);                                       /* 007b0920: [0x00a95744] once */
uint32_t rx_gpu_create(rx_addr rec);                                       /* 007b0780 (ESI = record): the device call; HRESULT */
/* 007b0970 (cdecl): a texture through the registry: CreateTexture(w, h, levels, usage, format, pool, out) + the record */
uint32_t rx_gpu_create_texture(uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool, rx_addr out,
                               rx_addr callback, rx_addr context, uint32_t a9, rx_addr file, uint32_t line);
uint32_t rx_gpu_create_vb(uint32_t length, uint32_t usage, rx_addr out, rx_addr callback, rx_addr context, rx_addr file, uint32_t line);
                                                                           /* 007b0cd0 (cdecl) */
uint32_t rx_gpu_create_ib16(uint32_t length, rx_addr out);                /* 007b0c10 (cdecl): 0x208 usage, INDEX16, pool 0 */
rx_addr  rx_vertex_decl_get(rx_addr elements, rx_addr cache);              /* 007afd60 (EAX = elements, EDX = cache): AddRef'ed */
void     rx_shader_find_vs(rx_addr asset, const char* name, rx_addr out, rx_addr out2);   /* 007b12a0 (stdcall; RET 0x10) */
void     rx_shader_find_ps(rx_addr asset, const char* name, rx_addr out, rx_addr out2);   /* 007b1330 (stdcall; RET 0x10) */

#ifdef __cplusplus
}
#endif
