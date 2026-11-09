/* build_render_asset.c -- see build_render_asset.h. Hand transliteration of ShaderAssetIO::vf3 00794780, XenonCore::vf15 00843b80,
 * XenonShaderAsset::vf3 007b0fb0 and FUN_007b1040 from the disassembly; the destruction (XenonShaderAsset::sdtor 007b0f90, its destructor
 * 007b13d0, vf2 00794720: a package unload, w-heat). */
#include "build_render_asset.h"
#include "build_alloc.h"
#include "build_render.h"
#include "../src/rx_d3d.h"
#include "build_activity_util.h"
#include "loop_platform.h"
#include <stdio.h>
#include <stdlib.h>

/* FUN_007b1040 (ESI = asset, EDI = stream): the version 2 programs */
static void shader_programs(rx_addr a, rx_stream* s) {
    rx_addr dev = rx_renderer_device();
    SU32(a, 0x454, 0); SU32(a, 0x890, 0);
    int32_t count = (int32_t)rx_stream_u32(s);
    for (int32_t i = 0; i < count; i++) {
        uint32_t kind = rx_stream_u32(s);
        if (kind != 0 && kind != 1) continue;           /* (the original reads nothing for another kind) */
        uint32_t cnt_off = kind ? 0x890 : 0x454, ent_off = kind ? 0x458 : 0x1c;
        uint32_t len = rx_stream_u32(s);
        if (len > 0x40) { fprintf(stderr, "rxcore: shader asset: program name of %u bytes\n", len); abort(); }
        rx_addr e = a + ent_off + 0x48 * U32(a, cnt_off);
        rx_stream_read(s, e, len);
        uint32_t size = rx_stream_u32(s);
        uint32_t words = (size >> 2) + 1;               /* mul 4 with the overflow clamp */
        rx_addr blk = rx_reflex_aligned_alloc(words > 0x3fffffffu ? 0xffffffffu : words * 4);
        rx_stream_read(s, blk, size);
        uint32_t cr[3] = {dev, blk, e + 0x40};
        rx_d3d_call(dev, kind ? 106 : 91, 3, cr);      /* CreatePixelShader / CreateVertexShader */
        rx_d3dx_get_shader_constant_table(blk, e + 0x44);
        rx_addr ct = U32(e, 0x44);
        uint32_t g[1] = {ct};
        rx_d3d_call(ct, 3, 1, g);                       /* GetBufferPointer, GetBufferSize: results unused */
        rx_d3d_call(ct, 4, 1, g);
        rx_reflex_aligned_free(blk);
        SU32(a, cnt_off, U32(a, cnt_off) + 1);
    }
}

/* XenonShaderAsset::vf3 007b0fb0 */
static void shader_load(rx_addr a, rx_stream* s) {
    SU32(a, 0x10, 0); SU32(a, 0x454, 0); SU32(a, 0x890, 0);
    uint32_t version = rx_stream_u32(s);
    SU32(a, 0x18, version);
    if (version == 1 || version == 3) {
        uint32_t size = rx_stream_u32(s);
        SU32(a, 0x14, size);
        rx_addr blob = rx_reflex_aligned_alloc(size);
        SU32(a, 0x10, blob);
        rx_stream_read(s, blob, size);
    } else if (version == 2) {
        shader_programs(a, s);
    } else {
        SU32(a, 0x14, version);
        SU32(a, 0x18, 1);
    }
}

/* ShaderAssetIO::vf3 00794780 */
rx_addr rx_shader_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x894);                    /* XenonCore::vf15 00843b80 */
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x00971a88); }
    shader_load(a, s);
    return a;
}

/* ---- the destruction (an asset whose last reference goes: Database vf31 0084af30; a Heat round's re-created riders, w-heat) ---------- */

#define SHADER_ASSET_VT 0x00971a88u   /* XenonShaderAsset::vftable */
#define ASSET_BASE_VT   0x0093eb78u   /* the base asset's vftable */
#define ALLOC_LOCK      0x00d61704u   /* the allocator's critical section (when [0x00da67d8] is set) */

/* the D3D objects of `count` program entries from `first` (0x48 bytes each): the constant table (+0x44), then the shader (+0x40),
 * each released (IUnknown::Release, slot 2) and cleared */
static void shader_release_programs(rx_addr first, uint32_t count) {
    for (uint32_t i = 0; i < count; i++) {
        const rx_addr entry = first + 0x48u * i;
        if (U32(entry, 0x44) != 0) {
            const uint32_t a[1] = {U32(entry, 0x44)};
            rx_d3d_call(U32(entry, 0x44), 2, 1, a);   /* the original's push + call: the import pushes its own argument */
            SU32(entry, 0x44, 0);
        }
        if (U32(entry, 0x40) != 0) {
            const uint32_t a[1] = {U32(entry, 0x40)};
            rx_d3d_call(U32(entry, 0x40), 2, 1, a);
            SU32(entry, 0x40, 0);
        }
    }
}

/* XenonShaderAsset's destructor 007b13d0 (ECX = the asset; push ebx / ebp / esi / edi): the vertex programs (+0x1c, count +0x454) and
 * the pixel programs (+0x458, count +0x890) released, the blob (+0x10) freed (_aligned_free inline, under the allocator lock taken
 * twice when [0x00da67d8] is set), the base vtable */
void rx_shader_asset_destroy(rx_addr asset) {
    ACT_FRAME(F, 0, 4);
    SU32(asset, 0, SHADER_ASSET_VT);
    shader_release_programs(asset + 0x1c, (uint32_t)(I32(asset, 0x454) > 0 ? I32(asset, 0x454) : 0));
    shader_release_programs(asset + 0x458, (uint32_t)(I32(asset, 0x890) > 0 ? I32(asset, 0x890) : 0));
    const rx_addr blob = U32(asset, 0x10);
    if (U8(0, 0x00da67d8) != 0) {
        loop_enter_cs(ALLOC_LOCK);   /* EnterCriticalSection (imports push their own arguments) */
        if (U8(0, 0x00da67d8) != 0) loop_enter_cs(ALLOC_LOCK);
    }
    rx_crt_aligned_free(blob);                                       /* _aligned_free [0x00916350] */
    if (U8(0, 0x00da67d8) != 0) {
        loop_leave_cs(ALLOC_LOCK);
        if (U8(0, 0x00da67d8) != 0) loop_leave_cs(ALLOC_LOCK);
    }
    SU32(asset, 0, ASSET_BASE_VT);
    ACT_END(F);
}
RX_FUNCTION(0x007b13d0, rx_shader_asset_destroy)

/* XenonShaderAsset::sdtor 007b0f90 (thiscall; flags; RET 4; push esi): the destructor, then (flags bit 0) ReflexAlignedFree */
rx_addr rx_shader_asset_sdtor(rx_addr asset, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_shader_asset_destroy(asset));
    if (flags & 1) {
        SU32(F, -4, asset);                                           /* push esi */
        ACT_CALL(1, rx_reflex_aligned_free(asset));
    }
    ACT_END(F);
    return asset;
}
RX_FUNCTION(0x007b0f90, rx_shader_asset_sdtor)

/* XenonShaderAsset::vf2 00794720: the shader AssetIO (the static object 0x00da67e0, its vtable 0x00971bec written on the first call:
 * guard 0x00da67f4, then _atexit of its destructor 00914070; the startup registration has made it) */
rx_addr rx_shader_asset_get_io(rx_addr asset) {
    (void)asset;
    if (!(U8(0, 0x00da67f4) & 1)) act_unreached(0x0079472d, "the shader AssetIO made on first use (its _atexit registration)");
    return 0x00da67e0;
}
RX_FUNCTION(0x00794720, rx_shader_asset_get_io)
