/* race_snake_wall.c -- XenonSnakeWallVisibleMesh (vtable 0x009447c4, IRenderable at +0x90 0x0094483c, IRenderEvent at +0x98 0x0094487c;
 * 0x11c+ bytes, embedded in MXActivitySnake +0xff100 and MXActivityInfection +0xff0c0): the shrinking boundary wall of the Snake and
 * Infection party modes, hand-transliterated from the original (each function's address is in its comment; frames and the stack
 * depth of each call are the original's).
 *
 * A render object. Following the core's precedent for render-side objects (TerrainTracks, the water and forest assets: construction
 * notes 9.6 / 11), the core builds it with the original's code: its fields, its CPU vertex array (+0xf4: the ring of wall quads
 * 0057fc00 writes, 12 vertices of 8 floats per segment: position, normal, u, v) and its D3D resources through the core's memory-backed
 * D3D layer (rx_d3d: the shaders of "SnakeShader", the vertex declaration, the falloff texture 006a1c10, two vertex buffers); nothing
 * is drawn. The render event (vf0_o98 0057f130: the copy into the vertex buffer and the draw) and the IRenderable slots are the
 * renderer's and are not ported (the product has no renderer; Unity draws the wall from rx_session_party_trails' boundary: the centre
 * +0xb0, the radius +0xc0, the height +0xb4 / +0x11c).
 *
 * Layout: +0 .. +0x8c Entity, +0x90 IRenderable (+0x94 = 0xf0), +0x98 IRenderEvent, +0xa0 the segment capacity, +0xa4 the segments of
 * the ring, +0xb0 the centre (vec3; +0xb4 becomes the wall's mid height), +0xc0 the radius, +0xd0 the colour (vec3), +0xe0 1.0,
 * +0xe4 / +0xe8 the vertex / pixel shader ("SnakeWallVS" / "SnakeWallPS"), +0xec the vertex declaration, +0xf0 the texture
 * (a TextureAsset), +0xf4 the vertex array (capacity * 0x180 bytes), +0xf8 / +0xfc two vertex buffers of +0x100 bytes, +0x104 a
 * CRITICAL_SECTION, +0x11c the wall's half height. */
#include "race_snake.h"
#include "race_extern.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_render.h"
#include "build_scene_entity.h"
#include "anim.h"
#include "race_beh1_extern.h"
#include "phys_crt.h"
#include "world_veh.h"
#include "crt.h"
#include "../src/rx_d3d.h"
#include "../src/rx_os.h"
#include <string.h>

#define VT_WALL       0x009447c4u
#define VT_WALL_O90   0x0094483cu
#define VT_WALL_O98   0x0094487cu
#define VT_ENTITY     0x0095a2e4u
#define VT_RENDERABLE 0x0093708cu
#define VT_RENDEREVT  0x009370ccu
#define VT_TEXASSET   0x009462d0u   /* TextureAsset::vftable */
#define IAT_ENTER_CS  0x0091600cu
#define IAT_LEAVE_CS  0x009160f8u
#define IAT_INIT_CS   0x00916014u
#define IAT_DELETE_CS 0x00916010u
#define CRT_HEAP_CS   0x00d61704u   /* the CRT heap lock, taken while [0xda67d8] */

static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }
/* the CRT heap lock around _aligned_malloc / _aligned_free (`cmp byte [0xda67d8], 0; je; push 0xd61704; call [Enter]`) */
static inline void heap_lock(void) { if (U8(0, 0x00da67d8) != 0) os_cs(IAT_ENTER_CS, CRT_HEAP_CS); }
static inline void heap_unlock(void) { if (U8(0, 0x00da67d8) != 0) os_cs(IAT_LEAVE_CS, CRT_HEAP_CS); }
/* fld dword / fstp dword: the bits, a signalling NaN quieted */
static inline uint32_t x87_bits(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? b | 0x400000u : b; }
static inline float x87_copy(float v) { return rx_bf(x87_bits(rx_fb(v))); }
static inline void fcopy(rx_addr d, uint32_t od, rx_addr s, uint32_t os) { SU32(d, od, x87_bits(U32(s, os))); }
static inline uint64_t dbits(double d) { uint64_t b; memcpy(&b, &d, 8); return b; }
static inline double dbits_d(uint64_t b) { double d; memcpy(&d, &b, 8); return d; }

/* ======================================================================================================================== */
/* render-side asset helpers                                                                                                 */

/* 00554e50 FloatImage_Alloc (ESI = image, EAX = width, ECX = height) -> image: {w, h, _aligned_malloc(w * h * 16, 16)} under the CRT
 * heap lock. Frame: push edi. */
rx_addr rx_float_image_alloc(rx_addr img, uint32_t w, uint32_t h) {
    ACT_FRAME(F, 0, 1);
    SU32(img, 0, w);
    SU32(img, 4, h);
    const uint64_t bytes = (uint64_t)(w * h) * 16u;
    const uint32_t size = (bytes >> 32) ? 0xffffffffu : (uint32_t)bytes;
    heap_lock();
    rx_addr p;
    ACT_CALLR(p, 2, crt_aligned_malloc(size, 0x10));
    heap_unlock();
    SU32(img, 8, p);
    ACT_END(F);
    return img;
}
RX_FUNCTION(0x00554e50, rx_float_image_alloc)

/* 007b3380 TextureAsset_CreateA8R8G8B8 (ESI = asset; stdcall width, height; RET 8): +0x1c = 0, +0x14 = 1, a 4-byte holder
 * (_aligned_malloc(4, 16)) at +0x10 holding the device's CreateTexture(w, h, all levels, usage 0, the format [0x9b7604], MANAGED).
 * Frame: push ecx / ebx / edi (the texture out pointer at F + 8). */
void rx_texture_asset_create_argb(rx_addr asset, uint32_t w, uint32_t h) {
    ACT_FRAME(F, 0, 3);
    const rx_addr dev = U32(U32(0, 0x00a95960), 0x1ec);
    SU32(asset, 0x1c, 0);
    SU32(asset, 0x14, 1);
    heap_lock();
    rx_addr holder;
    ACT_CALLR(holder, 2, crt_aligned_malloc(4, 0x10));
    heap_unlock();
    SU32(asset, 0x10, holder);
    const uint32_t a[9] = {dev, w, h, 0, 0, U32(0, 0x009b7604), 1, F + 8, 0};
    rx_set_sp(F - 0x24);
    (void)rx_d3d_call(dev, 23, 9, a);                               /* CreateTexture (device slot 23, vtable + 0x5c) */
    rx_set_sp(F);
    SU32(U32(asset, 0x10), 0, U32(F, 8));
    ACT_END(F);
}
RX_FUNCTION(0x007b3380, rx_texture_asset_create_argb)

/* 006a1c10 (cdecl image, bits; bits 0x10 / 0x20, else 0) -> a new TextureAsset (0x24 bytes, vtable 0x9462d0) with an A8R8G8B8 texture
 * of the image's size and every mip level the image's halvings give: level 0 the image, each next level a box filter of the previous
 * one (the original's (p[-1] + p + q + p) * 0.25 of two rows: p the 2x-th texel of row 2y + 1, q of row 2y + 2); every texel
 * clamped to [0, 1], times 255, floored (the CRT's inlined floor) and packed ARGB into the level's locked bits (a temporary row of
 * int4 texels, _aligned_malloc'd per level). The downsampled levels' images are freed (the caller frees level 0's).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x84; push ebx / esi / edi. */
rx_addr rx_texture_from_float_image(rx_addr img, uint32_t bits) {
    ACT_FRAME_EBP(F, 16, 0x84, 3);
    if (bits != 0x10 && bits != 0x20) { ACT_END(F); return 0; }
    uint32_t levels = 0;
    for (uint32_t w = U32(img, 0), h = U32(img, 4); w != 0 && h != 0; w >>= 1, h >>= 1) levels++;
    SU32(F, 0x28, levels);
    rx_addr asset;
    ACT_CALLR(asset, 1, rx_reflex_aligned_alloc(0x24));
    if (asset != 0) {
        for (uint32_t k = 8; k <= 0x20; k += 4) SU32(asset, k, 0);
        SU32(asset, 0, VT_TEXASSET);
    }
    SU32(F, 0x20, asset);
    ACT_CALL(2, rx_texture_asset_create_argb(asset, U32(img, 0), U32(img, 4)));
    rx_addr src = U32(img, 8);                                         /* F + 0x24: the level's image */
    uint32_t h = U32(img, 4), w = U32(img, 0);                         /* F + 0x10 / EDI */
    SU32(F, 0x24, src);
    SU32(F, 0x10, h);
    for (uint32_t k = 0; k < 16; k += 4) SF32(F + 0x50, k, 0.25f /* 0x92ff84 */);
    SU32(F, 0x2c, 0);
    if (levels != 0) {
        for (uint32_t k = 0; k < 16; k += 4) SF32(F + 0x40, k, 255.0f /* 0x92e104 */);
        for (uint32_t level = 0; level < levels; level++) {
            SU32(F, 0x2c, level);
            if (level != 0) {                                          /* the box filter into a new image of half the size */
                const uint32_t nh = h >> 1, nw = w >> 1;
                SU32(F, 0x1c, nh);
                SU32(F, 0x10, nw);
                const uint64_t bytes = (uint64_t)(nh * nw) * 16u;
                const uint32_t size = (bytes >> 32) ? 0xffffffffu : (uint32_t)bytes;
                SU32(F, 0x18, size);
                heap_lock();
                rx_addr dst;
                ACT_CALLR(dst, 2, crt_aligned_malloc(size, 0x10));
                heap_unlock();
                SU32(F, 0x14, dst);
                ACT_CALL(3, (void)crt_memset(dst, 0, (nh * nw) << 4));
                if (nh - 1 > 1) {
                    rx_addr p = src + (w << 5) + 0x30, q = src + (w * 3 << 4) + 0x20, o = dst + (nw << 4) + 0x10;
                    for (uint32_t y = nh - 2; y != 0; y--) {
                        if (nw - 1 > 1) {
                            rx_addr pp = p, qq = q, oo = o;
                            for (uint32_t x = nw - 2; x != 0; x--) {
                                for (uint32_t c = 0; c < 16; c += 4) {
                                    const float a = F32(pp, c), b = F32(pp - 0x10, c), d = F32(qq, c);
                                    float s = rx_adds(b, a);
                                    s = rx_adds(s, d);
                                    s = rx_adds(s, a);
                                    SF32(oo, c, rx_muls(s, F32(F + 0x50, c)));
                                }
                                qq += 0x20; pp += 0x20; oo += 0x10;
                            }
                        }
                        p += w << 5; q += w << 5; o += nw << 4;
                    }
                }
                if (level > 1) {                                       /* the previous downsampled image */
                    heap_lock();
                    ACT_CALL(1, crt_aligned_free(src));
                    heap_unlock();
                }
                src = dst;
                SU32(F, 0x24, src);
                w = nw;
                h = nh;
                SU32(F, 0x10, nh);
            }
            const rx_addr tex = U32(U32(asset, 0x10), 0);
            {   const uint32_t a[5] = {tex, level, F + 0x38, 0, 0};
                rx_set_sp(F - 0x14);
                (void)rx_d3d_call(tex, 19, 5, a);                      /* LockRect (texture slot 19, vtable + 0x4c): pitch F + 0x38, bits F + 0x3c */
                rx_set_sp(F); }
            const uint64_t rb = (uint64_t)w * 16u;
            const uint32_t rsize = (rb >> 32) ? 0xffffffffu : (uint32_t)rb;
            heap_lock();
            rx_addr row;
            ACT_CALLR(row, 2, crt_aligned_malloc(rsize, 0x10));
            heap_unlock();
            SU32(F, 0xc, row);
            if (h != 0) {
                const uint32_t pitch = U32(F, 0x38);
                rx_addr out = U32(F, 0x3c), in = src;
                SU32(F, 0x30, pitch);
                SU32(F, 0x18, w << 4);
                for (uint32_t y = h; y != 0; y--) {
                    SU32(F, 0x14, out);
                    SU32(F, 0x1c, y);
                    for (uint32_t x = 0; x < w; x++) {
                        for (uint32_t c = 0; c < 16; c += 4) {
                            float v = rx_minss(F32(in + 16 * x, c), F32(0, 0x00d61690 + c));
                            v = rx_maxss(v, F32(0, 0x00d61660 + c));
                            v = rx_muls(v, F32(F + 0x40, c));
                            SF32(F + 0x60, c, v);
                        }
                        for (uint32_t c = 0; c < 16; c += 4) {
                            const double d = (double)F32(F + 0x60, c);
                            /* the inlined floor: round to an integer (x + 2^52 - 2^52, the sign kept), minus 1 where that rounded up */
                            const double sgn = dbits_d(dbits(d) & 0x8000000000000000ull);
                            const double big = dbits_d(0x4330000000000000ull /* 0x977640 */ | (dbits(d) & 0x8000000000000000ull /* 0x977648 */));
                            double t = (d + big) - big;
                            if (!(t - d <= sgn)) t = t - 1.0 /* 0x977638 */;
                            SU32(F + 0x70, c, (uint32_t)rx_cvtt((float)t));
                        }
                        for (uint32_t c = 0; c < 16; c += 4) SU32(F + 0x60, c, U32(F + 0x70, c));
                        const rx_addr dst = row + 16 * x;
                        for (uint32_t c = 0; c < 16; c += 4) SU32(dst & ~15u, c, U32(F + 0x60, c));
                    }
                    for (uint32_t x = 0; x < w; x++) {
                        const rx_addr t = row + 16 * x;
                        SU32(out, 4 * x, ((U32(t, 0xc) << 8 | U32(t, 0)) << 8 | U32(t, 4)) << 8 | U32(t, 8));
                    }
                    out += pitch;
                    in += w << 4;
                }
            }
            heap_lock();
            ACT_CALL(1, crt_aligned_free(row));
            heap_unlock();
            {   const uint32_t a[2] = {tex, level};
                rx_set_sp(F - 8);
                (void)rx_d3d_call(tex, 20, 2, a);                      /* UnlockRect (texture slot 20, vtable + 0x50) */
                rx_set_sp(F); }
        }
    }
    if (levels > 1) {
        heap_lock();
        ACT_CALL(1, crt_aligned_free(U32(F, 0x24)));
        heap_unlock();
    }
    ACT_END(F);
    return asset;
}
RX_FUNCTION(0x006a1c10, rx_texture_from_float_image)

/* 007b12a0 / 007b1330 (stdcall shader asset, name, out shader, out constant table; RET 0x10): the last vertex (pixel) shader of the
 * XenonShaderAsset's table (+0x454 count, records of 0x48 bytes: +0x1c the name, +0x5c the shader, +0x60 its constant table; the pixel
 * shaders at +0x890 / +0x458 / +0x498 / +0x49c) whose name matches (_stricmp), AddRef'ed into *out (and its table into *out2). */
static void shader_lookup(rx_addr asset, rx_addr name, rx_addr out, rx_addr out2, uint32_t count_off, uint32_t name_off, uint32_t obj_off) {
    ACT_FRAME(F, 0, 3);
    SU32(out, 0, 0);
    int32_t i = I32(asset, count_off) - 1;
    if (i >= 0) {
        rx_set_sp(F - 4);   /* push edi */
        for (; i >= 0; i--) {
            int32_t d;
            ACT_CALLR(d, 2, crt_stricmp(name, asset + name_off + 0x48u * (uint32_t)i));
            if (d == 0) break;
        }
        if (i < 0) { ACT_END(F); return; }
        const rx_addr rec = asset + 0x48u * (uint32_t)i;
        const rx_addr sh = U32(rec, obj_off);
        { const uint32_t a[1] = {sh}; (void)rx_d3d_call(sh, 1, 1, a); }   /* AddRef */
        SU32(out, 0, U32(rec, obj_off));
        if (out2 != 0) {
            const rx_addr ct = U32(rec, obj_off + 4);
            if (ct != 0) { const uint32_t a[1] = {ct}; (void)rx_d3d_call(ct, 1, 1, a); }
            SU32(out2, 0, U32(rec, obj_off + 4));
        }
    }
    ACT_END(F);
}
void rx_shader_asset_vertex_shader(rx_addr asset, rx_addr name, rx_addr out, rx_addr out2) { shader_lookup(asset, name, out, out2, 0x454, 0x1c, 0x5c); }
RX_FUNCTION(0x007b12a0, rx_shader_asset_vertex_shader)
void rx_shader_asset_pixel_shader(rx_addr asset, rx_addr name, rx_addr out, rx_addr out2) { shader_lookup(asset, name, out, out2, 0x890, 0x458, 0x498); }
RX_FUNCTION(0x007b1330, rx_shader_asset_pixel_shader)

/* ======================================================================================================================== */
/* XenonSnakeWallVisibleMesh                                                                                                 */

/* 0057efb0 (stdcall mesh; RET 4): the "SnakeShader" asset of the engine's database (vf27 slot 0x6c, tag 'shader'), its loading flag
 * (+0x4c) held while "SnakeWallVS" (+0xe4) and "SnakeWallPS" (+0xe8) are taken from its XenonShaderAsset (+0x44).
 * Frame: push ebx / esi / edi. */
void rx_snake_wall_shaders(rx_addr m) {
    ACT_FRAME(F, 0, 3);
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    const uint32_t a[4] = {0x009426c4 /* "SnakeShader" */, 0x64616873u, 0x7265u, 1};
    const rx_addr h = act_vcall(db, 0x6c, 4, a);
    const rx_addr asset = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    ACT_CALL(4, rx_shader_asset_vertex_shader(asset, 0x009426d0 /* "SnakeWallVS" */, m + 0xe4, 0));
    ACT_CALL(4, rx_shader_asset_pixel_shader(asset, 0x009426dc /* "SnakeWallPS" */, m + 0xe8, 0));
    SU32(h, 0x4c, 0);
    ACT_END(F);
}
RX_FUNCTION(0x0057efb0, rx_snake_wall_shaders)

/* 0057ebb0 (stdcall mesh, colour, segments; RET 0xc) -> mesh: the Entity base (identity matrix 0x9cb510), the class's fields, the
 * critical section, the shaders (0057efb0), the vertex declaration (007afd60: elements 0x9c5248, the renderer's cache), a 256 x 256
 * float image of the wall's falloff (row r: 1 - |fmod(r + 8, [0x9ddfdc] (16)) * [0x9c4754] (0.125) - 1|, clamped at 0, to the 4th power p;
 * texel = min(p * colour + colour * 0.2 + p, 1), alpha 1) made into the texture (006a1c10, 32 bits), the vertex array of
 * segments * 12 vertices of 32 bytes (+0xf4), two vertex buffers (CreateVertexBuffer slot 26: +0x100 bytes, usage 0x208), +0x11c = 1,
 * the image freed. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xa4; push ebx / esi / edi (the image F + 0x24). */
rx_addr rx_snake_wall_construct(rx_addr m, rx_addr color, uint32_t segments) {
    ACT_FRAME_EBP(F, 16, 0xa4, 3);
    SU32(m, 0, VT_ENTITY);
    SF32(m, 0x24, 0.0f); SF32(m, 0x20, 0.0f);
    SU32(m, 0x18, 0); SU32(m, 0x10, 0); SU32(m, 0x70, 0);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(m + 0x30, k, U32(0, 0x009cb510 + k));
    SF32(m, 0x78, 0.0f);
    SU32(m, 0x1c, 0); SU32(m, 0x7c, 0); SU32(m, 0x74, 0); SU32(m, 0x28, 0); SU32(m, 0x2c, 0);
    SU32(m, 0x90, VT_RENDERABLE);
    SU32(m, 0x94, 0xf0);
    SU32(m, 0x98, VT_RENDEREVT);
    SU32(m, 0, VT_WALL);
    SU32(m, 0x90, VT_WALL_O90);
    SU32(m, 0x98, VT_WALL_O98);
    SU32(m, 0xa0, segments);
    SU32(m, 0xa4, 0);
    SF32(m, 0xb0, 0.0f); SF32(m, 0xb4, 0.0f); SF32(m, 0xb8, 0.0f);
    SF32(m, 0xc0, 1.0f /* 0x931f24 */);
    fcopy(m, 0xd0, color, 0); fcopy(m, 0xd4, color, 4); fcopy(m, 0xd8, color, 8);
    SF32(m, 0xe0, 1.0f);
    rx_set_sp(F - 4);
    os_cs(IAT_INIT_CS, m + 0x104);
    rx_set_sp(F);
    ACT_CALL(1, rx_snake_wall_shaders(m));
    rx_addr decl;
    ACT_CALLR(decl, 0, rx_vertex_decl_get(0x009c5248, U32(U32(0, 0x00a95960), 0x14)));
    SU32(m, 0xec, decl);
    ACT_CALL(0, (void)rx_float_image_alloc(F + 0x24, 0x100, 0x100));
    /* the colour (movaps from the caller's vector: its 4th lane is whatever the caller's frame holds) and 0.2 of it */
    for (uint32_t k = 0; k < 16; k += 4) {
        SU32(F + 0x80, k, U32(color, k));
        SF32(F + 0x50, k, rx_muls(rx_bf(0x3e4ccccdu) /* 0.2, 0x92e388 */, F32(color, k)));
    }
    const uint32_t mis = (F + 0x1c) & 0xf;
    SU32(F, 0x38, 0x10 - mis);
    SU32(F, 0x30, F + 0x80 - mis);
    SU32(F, 0x14, 0);
    SF32(F, 0x20, 0.0f);
    SF32(F, 0x1c, 1.0f);
    SU32(F, 0x18, 0);
    for (uint32_t r = 0; r < 0x100; r++) {
        SU32(F, 0x14, r);
        float fm;
        {   const float a = rx_adds(F32(F, 0x20), 8.0f /* 0x9322b8 */);
            const float b = x87_copy(F32(0, 0x009ddfdc));
            rx_set_sp(F - 8);
            SF32(F - 8, 4, b); SF32(F - 8, 0, a);
            ACT_CALLR(fm, 0, rx_world_float_fmod(a, b));
            rx_set_sp(F); }
        /* fmul [0x9c4754]; fsub 1.0; fstp (53-bit precision: the double operations, rounded once to float) */
        const float v = (float)((double)fm * (double)F32(0, 0x009c4754) - 1.0);
        SF32(F, 0x34, v);
        float av;
        ACT_CALLR(av, 0, rx_world_float_abs(v));
        SF32(F, 0x3c, rx_subs(1.0f, av));
        for (uint32_t c = 0; c < 0x100; c++) {
            float x = F32(F, 0x3c);
            if (0.0f > x) x = 0.0f;
            const float x2 = rx_muls(x, x), x4 = rx_muls(x2, x2);
            for (uint32_t k = 0; k < 16; k += 4) SF32(F + 0x90, k, x4);
            /* the inlined unaligned 16-byte load of {1.0, ...} at F + 0x1c: memcpy of its first (16 - mis) bytes, memset of the rest */
            rx_set_sp(F - 0xc);
            ACT_CALL(3, (void)crt_memcpy(F + 0x70, F + 0x1c, 0x10 - mis));
            ACT_CALL(3, (void)crt_memset(U32(F, 0x30), 0, mis));
            rx_set_sp(F);
            for (uint32_t k = 0; k < 16; k += 4) SU32(F + 0x60, k, U32(F + 0x70, k));
            for (uint32_t k = 0; k < 16; k += 4) {
                float s = rx_muls(x4, F32(F + 0x80, k));
                s = rx_adds(s, F32(F + 0x50, k));
                s = rx_adds(s, x4);
                SF32(F + 0x40, k, rx_minss(s, F32(0, 0x00d61650 + k)));
            }
            SU32(F, 0x4c, U32(F, 0x60));
            if ((int32_t)c >= 0 && (int32_t)r >= 0 && (int32_t)c < I32(F, 0x24) && (int32_t)r < I32(F, 0x28)) {
                const rx_addr t = U32(F, 0x2c) + ((U32(F, 0x18) + c) << 4);
                for (uint32_t k = 0; k < 16; k += 4) SU32(t, k, U32(F + 0x40, k));
            }
        }
        SU32(F, 0x18, U32(F, 0x18) + U32(F, 0x24));
        SF32(F, 0x20, rx_adds(F32(F, 0x20), 1.0f));
    }
    SU32(F, 0x14, 0x100);
    rx_addr tex;
    ACT_CALLR(tex, 2, rx_texture_from_float_image(F + 0x24, 0x20));
    SU32(m, 0xf0, tex);
    SU32(m, 0x100, (segments * 12u) << 5);
    const uint64_t bytes = (uint64_t)(segments * 12u) * 0x20u;
    rx_addr verts;
    rx_set_sp(F - 0xc);
    ACT_CALLR(verts, 1, rx_reflex_aligned_alloc((bytes >> 32) ? 0xffffffffu : (uint32_t)bytes));
    rx_set_sp(F);
    SU32(m, 0xf4, verts);
    const rx_addr dev = U32(U32(0, 0x00a95960), 0x1ec);
    {   const uint32_t a[7] = {dev, U32(m, 0x100), 0x208, 0, 0, m + 0xf8, 0};
        rx_set_sp(F - 0x1c);
        (void)rx_d3d_call(dev, 26, 7, a);                              /* CreateVertexBuffer */
        rx_set_sp(F); }
    {   const uint32_t a[7] = {dev, U32(m, 0x100), 0x208, 0, 0, m + 0xfc, 0};
        rx_set_sp(F - 0x1c);
        (void)rx_d3d_call(dev, 26, 7, a);
        rx_set_sp(F); }
    SF32(m, 0x11c, 1.0f);
    heap_lock();
    ACT_CALL(1, crt_aligned_free(U32(F, 0x2c)));
    heap_unlock();
    ACT_END(F);
    return m;
}
RX_FUNCTION(0x0057ebb0, rx_snake_wall_construct)

/* 0057f020 (ESI = mesh): the destructor's body: the shaders and the declaration released, the texture asset's vf0 (1), the vertex
 * array freed, both vertex buffers released, the critical section deleted, the Entity base's audio registries ([0xa95824] /
 * [0xa95828] vf1 with the entity; null in the product). */
void rx_snake_wall_destroy(rx_addr m) {
    SU32(m, 0, VT_WALL);
    SU32(m, 0x90, VT_WALL_O90);
    SU32(m, 0x98, VT_WALL_O98);
    for (uint32_t k = 0xe4; k <= 0xec; k += 4) { const rx_addr o = U32(m, k); const uint32_t a[1] = {o}; ACT_CALL(1, (void)rx_d3d_call(o, 2, 1, a)); }
    const rx_addr tex = U32(m, 0xf0);
    if (tex != 0) { const uint32_t a[1] = {1}; (void)act_vcall(tex, 0, 1, a); }
    ACT_CALL(1, rx_reflex_aligned_free(U32(m, 0xf4)));
    for (uint32_t k = 0xf8; k <= 0xfc; k += 4) {
        const rx_addr o = U32(m, k);
        if (o != 0) { const uint32_t a[1] = {o}; ACT_CALL(1, (void)rx_d3d_call(o, 2, 1, a)); SU32(m, k, 0); }
    }
    rx_set_sp(rx_sp() - 4);
    os_cs(IAT_DELETE_CS, m + 0x104);
    rx_set_sp(rx_sp() + 4);
    SU32(m, 0x90, VT_RENDERABLE);
    SU32(m, 0, VT_ENTITY);
    for (rx_addr g = 0x00a95824; g <= 0x00a95828; g += 4) {
        const rx_addr r = U32(0, g);
        if (r != 0) { const uint32_t a[1] = {m}; (void)act_vcall(r, 4, 1, a); }
    }
}
RX_FUNCTION(0x0057f020, rx_snake_wall_destroy)

/* 0057ef90 slot 0 (thiscall; flags; RET 4) -> mesh: 0057f020, freed with flags & 1. Frame: push esi. */
rx_addr rx_snake_wall_sdtor(rx_addr m, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_snake_wall_destroy(m));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(m));
    ACT_END(F);
    return m;
}
RX_FUNCTION(0x0057ef90, rx_snake_wall_sdtor)
/* 00592b70 IRenderable slot 0 (ECX = mesh + 0x90): the adjustor into 0057ef90 */
rx_addr rx_snake_wall_sdtor_o90(rx_addr r, uint32_t flags) { return rx_snake_wall_sdtor(r - 0x90, flags); }
RX_FUNCTION(0x00592b70, rx_snake_wall_sdtor_o90)

/* 006a0e20 (EAX = the entity, ESI = the scene): the entity described ({the scene, no name}: 0080ddf0), its id from the scene's counter
 * (+0x19f4 under the scene's lock), added to the scene (007d2f70). Frame: sub esp, 0xc; push ebx / edi (the record at F + 8). */
void rx_snake_mesh_scene_add(rx_addr e, rx_addr scene) {
    ACT_FRAME(F, 0xc, 2);
    SU32(F, 0xc, 0);
    SU32(F, 0x10, 0);
    SU32(F, 8, scene);
    ACT_CALL(2, rx_entity_describe(e, F + 8));
    os_cs(IAT_ENTER_CS, scene + 4);
    const uint32_t id = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, id + 1);
    SU32(e, 0x14, id);
    os_cs(IAT_LEAVE_CS, scene + 4);
    ACT_CALL(1, rx_scene_manager_add_entity(e, scene));
    ACT_END(F);
}
RX_FUNCTION(0x006a0e20, rx_snake_mesh_scene_add)

/* ======================================================================================================================== */
/* the ring                                                                                                                  */

/* 0055fba0 (xmm1 = angle; stdcall x, z; ECX = out x, EDX = out z; RET 0 (the caller pops)): (x, z) rotated by the angle with the
 * game's inlined sine / cosine: out x = cos * x + sin * z, out z = cos * z - sin * x. */
void rx_rotate_xz(float angle, float x, float z, rx_addr out_x, rx_addr out_z) {
    const float sign = b1_sign(angle);
    const float t = b1_trig_reduce(rx_muls(sign, angle));
    const float t2 = rx_muls(t, t), t4 = rx_muls(t2, t2);
    const float c = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
    const float s = rx_muls(rx_muls(b1_sin_poly(t2, t4), t), sign);
    SF32(out_x, 0, rx_adds(rx_muls(c, x), rx_muls(s, z)));
    SF32(out_z, 0, rx_subs(rx_muls(c, z), rx_muls(s, x)));
}
RX_FUNCTION(0x0055fba0, rx_rotate_xz)

/* the face normal of the triangle (v0, v1, v2) (e1 = v0 - v2, e2 = v0 - v1), unit length unless the cross product is exactly 0 (then the
 * normal is not written), copied to the three vertices; t = F + toff for the three temporaries, len at F + loff */
static void tri_normal(rx_addr F, rx_addr v0, rx_addr v1, rx_addr v2, uint32_t toff, uint32_t loff, int first) {
    const float x0 = F32(v0, 0), y0 = F32(v0, 4), z0 = F32(v0, 8);
    float nx, ny, nz;
    if (first) {   /* 0057fe21: ax = x0 - x2, az = z0 - z2, ay = y0 - y2; bx = x0 - x1, bz = z0 - z1, by = y0 - y1 */
        const float ax = rx_subs(x0, F32(v2, 0)), bx = rx_subs(x0, F32(v1, 0)), az = rx_subs(z0, F32(v2, 8)), bz = rx_subs(z0, F32(v1, 8));
        const float by = rx_subs(y0, F32(v1, 4)), ay = rx_subs(y0, F32(v2, 4));
        nx = rx_subs(rx_muls(az, by), rx_muls(ay, bz));
        nz = rx_subs(rx_muls(ay, bx), rx_muls(by, ax));
        ny = rx_subs(rx_muls(bz, ax), rx_muls(az, bx));
    } else {       /* 0057ffd4: cx = x0 - x2, dx = x0 - x1, dy = y0 - y1, cy = y0 - y2, dz = z0 - z1, cz = z0 - z2 */
        const float cx = rx_subs(x0, F32(v2, 0)), dx = rx_subs(x0, F32(v1, 0)), dy = rx_subs(y0, F32(v1, 4)), cy = rx_subs(y0, F32(v2, 4));
        const float dz = rx_subs(z0, F32(v1, 8)), cz = rx_subs(z0, F32(v2, 8));
        nx = rx_subs(rx_muls(cz, dy), rx_muls(cy, dz));
        nz = rx_subs(rx_muls(cy, dx), rx_muls(dy, cx));
        ny = rx_subs(rx_muls(dz, cx), rx_muls(cz, dx));
    }
    SF32(F, toff, nx); SF32(F, toff + 4, ny); SF32(F, toff + 8, nz);
    const float len2 = rx_adds(rx_adds(rx_muls(nz, nz), rx_muls(ny, ny)), rx_muls(nx, nx));
    SF32(F, loff, len2);
    if (!(len2 == 0.0f)) {
        float len;
        ACT_CALLR(len, 1, phys_sqrtf(x87_copy(len2)));
        const float inv = rx_divs(1.0f, len);
        SF32(v0, 0xc, rx_muls(nx, inv));
        SF32(v0, 0x10, rx_muls(ny, inv));
        SF32(v0, 0x14, rx_muls(nz, inv));
    }
    for (uint32_t k = 0xc; k <= 0x14; k += 4) { fcopy(v1, k, v0, k); fcopy(v2, k, v0, k); }
}
/* a back-face vertex: the position and uv of src, its normal times -1 */
static void back_vertex(rx_addr d, rx_addr s) {
    for (uint32_t k = 0; k <= 8; k += 4) fcopy(d, k, s, k);
    fcopy(d, 0x18, s, 0x18); fcopy(d, 0x1c, s, 0x1c);
    for (uint32_t k = 0xc; k <= 0x14; k += 4) { fcopy(d, k, s, k); SF32(d, k, rx_muls(F32(d, k), rx_bf(0xbf800000u) /* -1, 0x9cd9e8 */)); }
}

/* 0057fc00 (EDI = mesh, EAX = the centre; stdcall radius, width; RET 8) -> EAX 1 when the ring was built, else 0: the centre and radius
 * stored; n = trunc(2 r pi / width) segments (x87, 53-bit, truncation); when n fits the capacity (+0xa0), under the mesh's lock, the
 * vertex array gets per segment i (the unit circle points p_i, p_i+1 by repeated rotation of (1, 0) by 2 pi / n (0055fba0)) two
 * quads from y -1 to 1 (u = the angle i 2 pi / n (x87), v 0 / 1) with their face normals and the same six vertices facing back; +0xa4 =
 * n. The wall's height from the scene's terrain (0x9e30d4 registry, 00402dc0: +0xdc the lowest, +0xe0 * 1.5 the highest; else the
 * centre's y for both): +0xb4 = the middle, +0x11c = half the span, each 35 beyond. Frame: push ebp; mov ebp, esp; and esp, -8;
 * sub esp, 0x68; push ebx / esi. */
uint32_t rx_snake_wall_build_ring(rx_addr m, rx_addr centre, float radius, float width) {
    ACT_FRAME_EBP(F, 8, 0x68, 2);
    fcopy(m, 0xb0, centre, 0); fcopy(m, 0xb4, centre, 4); fcopy(m, 0xb8, centre, 8);
    SF32(m, 0xc0, radius);
    uint32_t ret = 0;
    SF32(F, 0x24, F32(m, 0xb4));
    SF32(F, 0x28, F32(m, 0xb4));
    /* fld r; fadd st, st; fmul [pi]; fdiv width; fistp (truncation): the 53-bit operations */
    const double q = ((double)radius + (double)radius) * (double)rx_bf(0x40490fdbu) /* pi, 0x977498 */ / (double)width;
    uint32_t n;
    {   const double tq = q < 0 ? -__builtin_floor(-q) : __builtin_floor(q);
        n = (tq >= -9.2233720368547758e18 && tq < 9.2233720368547758e18) ? (uint32_t)(int64_t)tq : 0u; }
    SU32(F, 0x20, n);
    const float nf = (float)((int32_t)n < 0 ? (double)(int32_t)n + 4294967296.0 : (double)(int32_t)n);
    SF32(F, 0x18, nf);
    const float step = rx_divs(rx_bf(0x40c90fdbu) /* 2 pi, 0x97749c */, nf);
    SF32(F, 0x10, 1.0f);
    SF32(F, 0x14, 0.0f);
    SF32(F, 0x38, step);
    rx_set_sp(F - 8);
    SF32(F - 8, 0, 1.0f); SF32(F - 8, 4, 0.0f);
    ACT_CALL(0, rx_rotate_xz(step, 1.0f, 0.0f, F + 0xc, F + 8));
    rx_set_sp(F);
    if (n <= U32(m, 0xa0)) {
        rx_set_sp(F - 4);
        os_cs(IAT_ENTER_CS, m + 0x104);
        rx_set_sp(F);
        const rx_addr verts = U32(m, 0xf4);
        if (!(U8(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
        rx_addr terrain;
        ACT_CALLR(terrain, 0, rx_world_find_by_controller(0x009e30d4));
        SF32(F, 0x24, F32(terrain, 0xdc));
        SF32(F, 0x28, rx_muls(F32(terrain, 0xe0), rx_bf(0x3fc00000u) /* 1.5, 0x9322a4 */));
        for (uint32_t i = 0; i < n; i++) {
            const rx_addr B = verts + 0x180u * i;
            const double twopi = (double)rx_bf(0x40c90fdbu);
            const double fi = (double)i + ((int32_t)i < 0 ? 4294967296.0 : 0.0);
            const double fi1 = (double)(i + 1) + ((int32_t)(i + 1) < 0 ? 4294967296.0 : 0.0);
            const float a0 = (float)(fi * twopi / (double)nf);
            const float a1 = (float)(fi1 * twopi / (double)nf);
            SF32(F, 0x30, a0);
            SF32(F, 0x1c, a1);
            const float cx = F32(F, 0x10), cz = F32(F, 0x14), nx = F32(F, 0xc), nz = F32(F, 8);
            /* the front quad: v0 (p_i, -1), v1 (p_i, 1), v2 (p_i+1, 1); v3 (p_i, -1), v4 (p_i+1, 1), v5 (p_i+1, -1) */
            SF32(B, 0, cx); SF32(B, 8, cz); SF32(B, 4, rx_bf(0xbf800000u)); SF32(B, 0x1c, 0.0f);
            SF32(B, 0x18, a0);
            SF32(B, 0x20, cx); SF32(B, 0x28, cz); SF32(B, 0x24, 1.0f);
            SF32(B, 0x38, a0); SF32(B, 0x3c, 1.0f);
            SF32(B, 0x40, nx); SF32(B, 0x48, nz); SF32(B, 0x44, 1.0f);
            SF32(B, 0x58, a1); SF32(B, 0x5c, 1.0f);
            tri_normal(F, B, B + 0x20, B + 0x40, 0x40, 0x2c, 1);
            SF32(B, 0x60, cx); SF32(B, 0x68, cz); SF32(B, 0x64, rx_bf(0xbf800000u));
            SF32(B, 0x78, a0); SF32(B, 0x7c, 0.0f);
            SF32(B, 0x80, nx); SF32(B, 0x84, 1.0f); SF32(B, 0x88, nz);
            SF32(B, 0x9c, 1.0f); SF32(B, 0x98, a1);
            SF32(B, 0xa0, nx); SF32(B, 0xa8, nz); SF32(B, 0xa4, rx_bf(0xbf800000u));
            SF32(B, 0xbc, 0.0f); SF32(B, 0xb8, a1);
            tri_normal(F, B + 0x60, B + 0x80, B + 0xa0, 0x4c, 0x34, 0);
            /* the back faces: (v0, v2, v1), (v3, v5, v4) */
            back_vertex(B + 0xc0, B);
            back_vertex(B + 0xe0, B + 0x40);
            back_vertex(B + 0x100, B + 0x20);
            back_vertex(B + 0x120, B + 0x60);
            back_vertex(B + 0x140, B + 0xa0);
            back_vertex(B + 0x160, B + 0x80);
            /* the next point: p_i+1 rotated */
            const float px = x87_copy(nx), pz = x87_copy(nz);
            rx_set_sp(F - 8);
            SF32(F - 8, 4, pz); SF32(F - 8, 0, px);
            SF32(F, 0x10, nx);
            SF32(F, 0x14, nz);
            ACT_CALL(0, rx_rotate_xz(F32(F, 0x38), px, pz, F + 0xc, F + 8));
            rx_set_sp(F);
        }
        SU32(m, 0xa4, n);
        rx_set_sp(F - 4);
        os_cs(IAT_LEAVE_CS, m + 0x104);
        rx_set_sp(F);
        ret = 1;
    }
    const float k35 = 35.0f /* 0x9774d4 */;
    const float lo = rx_subs(F32(F, 0x24), k35);
    const float span = rx_subs(rx_adds(F32(F, 0x28), k35), lo);
    const float half = rx_muls(rx_bf(rx_fb(span) & 0x7fffffffu), 0.5f /* 0x9322cc */);
    SF32(m, 0xb4, rx_adds(half, lo));
    SF32(m, 0x11c, half);
    ACT_END(F);
    return ret;
}
RX_FUNCTION(0x0057fc00, rx_snake_wall_build_ring)
