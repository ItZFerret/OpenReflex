/* build_render_asset_texture.c -- see build_render_asset.h. Hand transliteration of TextureAssetIO::vf3 00794810, XenonTextureAsset_Create 007b2820
 * and FUN_007b26d0 from the disassembly. */
#include "build_render_asset.h"
#include "build_alloc.h"
#include "build_render.h"
#include "../src/rx_d3d.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define FILE_STR2 0x00970ee0u   /* the engine source path of XenonTextureAsset.cpp (registry record) */

static void fatal(const char* what) { fprintf(stderr, "rxcore: texture asset: %s\n", what); abort(); }
static uint32_t dcall(rx_addr obj, uint32_t slot, int n, const uint32_t* a) { return rx_d3d_call(obj, slot, n, a); }

static int is_dxt(uint32_t f) { return f == 0x31545844 || f == 0x32545844 || f == 0x33545844 || f == 0x34545844 || f == 0x35545844; }

/* FUN_007b26d0 (ECX = the level's D3DSURFACE_DESC, EAX = the level's file entry {size, pitch}, stack: the stream, the locked rect): the level's
 * rows into the locked bits; 1, or 0 when the level does not fit (its bytes are skipped) */
static int read_level(rx_stream* s, rx_addr desc, rx_addr entry, rx_addr lr) {
    uint32_t size = U32(entry, 0), pitch = U32(entry, 4), fmt = U32(desc, 0), height = U32(desc, 0x1c);
    uint32_t dpitch = U32(lr, 0), rows = is_dxt(fmt) ? (height + 3) / 4 : height;
    uint32_t total = rows * dpitch;
    if ((int32_t)dpitch < (int32_t)pitch || (int32_t)size > (int32_t)total) { s->pos += size < s->size - s->pos ? size : s->size - s->pos; return 0; }
    uint32_t n = (int32_t)size < (int32_t)total ? size : total, row = (int32_t)pitch < (int32_t)dpitch ? pitch : dpitch;
    rx_addr dst = U32(lr, 4);
    uint32_t consumed = row, off = 0;
    while ((int32_t)consumed <= (int32_t)n) {
        if (off >= total) break;
        rx_stream_read(s, dst, row);
        consumed += row; dst += dpitch; off += dpitch;
    }
    if ((int32_t)(size - n) > 0) s->pos += size - n;
    return 1;
}

/* XenonTextureAsset_Create (ECX = the asset; stack: the stream, the name) */
static void texture_create(rx_addr a, rx_stream* s, rx_addr name) {
    rx_addr rend = U32(0, RX_RENDERER), dev = rx_renderer_device();
    RX_FRAME_ENTER(F, 0x100);
    rx_addr lr = F, desc = F + 0x10, tmp = F + 0x40;
    SU32(a, 0x1c, 0);
    uint32_t b1[5], b2[5] = {0, 0, 0, 0, 0};
    rx_stream_read_host(s, b1, 0x14);
    uint32_t version = b1[1], images = b1[3], total_levels = b1[4];
    if (version >= 2) rx_stream_read_host(s, b2, 0x14);
    uint32_t flags = b2[0];
    if (version < 4) fatal("a version below 4 (the original's cube fixups / class defaults) is not ported");
    if (flags & 8) fatal("flag 8 (per-level reads) is not ported");
    if (flags & 0x10) fatal("flag 0x10 (a trailing XML block) is not ported");
    uint32_t bytes = (total_levels + 2 * images) * 8;
    rx_addr tbl = rx_crt_aligned_malloc(bytes, 16);
    rx_addr lv = tbl + 16 * images;
    if (flags & 2) SU32(a, 0x1c, 1);
    rx_stream_read(s, tbl, bytes);
    SU32(a, 0x10, rx_crt_aligned_malloc(images * 4, 16));
    SU32(a, 0x14, images);
    uint32_t cls = b2[1];
    {   char nm[0x40]; uint32_t i = 0;
        for (; i + 1 < sizeof nm && U8(name, i); i++) nm[i] = (char)U8(name, i);
        nm[i] = 0;
        if (!strcmp(nm, "PX_LA_Bink_MAIN")) cls = 1;
    }
    uint32_t levels0 = U32(tbl, 0);
    int cube = U32(a, 0x1c) != 0;
    if (cls != 0 && cls != 1) fatal("a texture class other than 0 / 1 (0x200 dynamic) is not ported");
    for (uint32_t img = 0; img < images; img++) {
        rx_addr d = tbl + 16 * img;                        /* {levels, width, height, format} */
        uint32_t dlevels = U32(d, 0), w = U32(d, 4), h = U32(d, 8), fmt = U32(d, 12);
        rx_addr out = U32(a, 0x10) + 4 * img;
        if (cube) {
            SU32(out, 0, 0);
            uint32_t ar[8] = {dev, w, dlevels, cls, fmt, 1, out, 0};
            dcall(dev, 25, 8, ar);                           /* CreateCubeTexture(edge, levels, usage, format, MANAGED) */
        } else {
            uint32_t pool = ~cls & 1;
            if (fmt == 0x4e545844) fmt = 0x35545844;         /* 'DXTN' -> DXT5 */
            if (cls == 1) {
                if (w == 0x500 && h == 0x2d0) { w = U32(rend, 0x3c + 8); h = U32(rend, 0x3c + 0xc); }
                rx_gpu_create_texture(w, h, dlevels, 1, fmt, 0, out, 0, 0, 0, FILE_STR2, 0x529);
            } else {
                if (U8(0, 0x00a956fa) && U32(rend, 0x3c + 0x28) < 2) fatal("the texture-quality mip reduction is not ported");
                SU32(out, 0, 0);
                uint32_t ar[9] = {dev, w, h, levels0, cls, fmt, pool, out, 0};
                dcall(dev, 23, 9, ar);                       /* CreateTexture(width, height, levels, usage, format, pool) */
            }
        }
    }
    if ((flags & 9) == 0) {                                  /* the levels' rows */
        uint32_t run = 0;
        for (uint32_t img = 0; img < images; img++) {
            rx_addr tex = U32(U32(a, 0x10), 4 * img);
            uint32_t dlevels = U32(tbl + 16 * img, 0);
            if (!cube) {
                for (uint32_t l = 0; l < dlevels; l++, run++) {
                    { uint32_t g[3] = {tex, l, desc}; dcall(tex, 17, 3, g); }                    /* GetLevelDesc */
                    { uint32_t g[5] = {tex, l, lr, 0, 0}; dcall(tex, 19, 5, g); }                /* LockRect */
                    int ok = read_level(s, desc, lv + 8 * run, lr);
                    { uint32_t g[2] = {tex, l}; dcall(tex, 20, 2, g); }                          /* UnlockRect */
                    if (!ok && l > 0) { /* D3DXLoadSurfaceFromSurface(level l <- level 0): a no-op in the reference boot */ (void)tmp; }
                }
                { uint32_t g[2] = {tex, 0}; dcall(tex, 21, 2, g); }                              /* AddDirtyRect(NULL) */
            } else {
                for (uint32_t face = 0; face < 6; face++) {
                    for (uint32_t l = 0; l < dlevels; l++, run++) {
                        { uint32_t g[3] = {tex, l, desc}; dcall(tex, 17, 3, g); }
                        { uint32_t g[6] = {tex, face, l, lr, 0, 0}; dcall(tex, 19, 6, g); }       /* LockRect(face, level) */
                        read_level(s, desc, lv + 8 * run, lr);
                        { uint32_t g[3] = {tex, face, l}; dcall(tex, 20, 3, g); }
                    }
                    { uint32_t g[3] = {tex, face, 0}; dcall(tex, 21, 3, g); }                    /* AddDirtyRect(face, NULL) */
                }
            }
        }
    }
    rx_crt_aligned_free(tbl);
    RX_FRAME_LEAVE(F, 0x100);
}

/* TextureAssetIO::vf3 00794810 */
rx_addr rx_texture_asset_io_create(rx_stream* s, rx_addr name) {
    rx_addr a = rx_reflex_aligned_alloc(0x24);
    if (a) { for (uint32_t k = 8; k <= 0x20; k += 4) SU32(a, k, 0); SU32(a, 0, 0x009462d0); }
    texture_create(a, s, name);
    return a;
}

/* FUN_007b0ee0 (stack: a pointer): the GPU registry's record whose output slot is that pointer is freed and unlinked (a std::list erase) */
static void gpu_registry_forget(rx_addr slot) {
    rx_addr reg = U32(0, RX_GPU_REGISTRY);
    if (!reg) return;
    rx_addr head = U32(reg, 0x14);
    for (rx_addr n = U32(head, 0); n != head; n = U32(n, 0)) {
        rx_addr rec = U32(n, 8);
        if (U32(rec, 4) != slot) continue;
        rx_reflex_aligned_free(rec);
        SU32(U32(n, 4), 0, U32(n, 0)); SU32(U32(n, 0), 4, U32(n, 4));                  /* FUN_007cdb70: unlink, count - 1, free the node */
        SU32(reg, 0x18, U32(reg, 0x18) - 1);
        rx_reflex_aligned_free(n);
        return;
    }
}

/* XenonTextureAsset::sdtor 007b2620 (thiscall this, stack: flags; RET 4; returns this) with FUN_007b2640: every D3D texture of the array is forgotten
 * by the GPU registry and released, the array and the (trailing XML, +0x20: never built) block freed, the asset freed when flag 1 is set */
uint32_t rx_texture_asset_sdtor(rx_addr a, uint32_t flags) {
    SU32(a, 0, 0x009719f0);
    for (uint32_t i = 0; (int32_t)i < (int32_t)U32(a, 0x14); i++) {
        gpu_registry_forget(U32(a, 0x10) + 4 * i);
        rx_addr t = U32(U32(a, 0x10), 4 * i);
        uint32_t g[1] = {t};
        rx_d3d_call(t, 2, 1, g);                              /* Release */
    }
    rx_crt_aligned_free(U32(a, 0x10));
    if (U32(a, 0x20)) fatal("the trailing XML block (+0x20) is never built");
    SU32(a, 0, 0x0093eb78);
    if (flags & 1) rx_reflex_aligned_free(a);
    return a;
}
RX_FUNCTION(0x007b2620, rx_texture_asset_sdtor)

/* XenonTextureAsset::vf2 007b3410: the texture AssetIO (the static object 0x00da67e8, its vtable written on the first call: guard 0x00da67f8) */
uint32_t rx_texture_asset_get_io(rx_addr a) {
    (void)a;
    if (!(U8(0, 0x00da67f8) & 1)) { SU32(0, 0x00da67f8, U32(0, 0x00da67f8) | 1); SU32(0, 0x00da67e8, 0x00971c24); }
    return 0x00da67e8;
}
RX_FUNCTION(0x007b3410, rx_texture_asset_get_io)
