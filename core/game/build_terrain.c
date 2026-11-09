#include "../src/fmt_modtrack.h"
/* build_terrain.c -- see build_terrain.h. Hand transliteration from the disassembly of 007c8db0, 007c4890, 007cc880, 007c4d10,
 * 007c4cc0, 007cacc0, 007caa10, 007c4630, 007cc700, 007ba010, 007cb9a0, 007cbbf0, 007e0e90 / 007f9e70 / 007fa340, 00544cc0 (and the
 * TEMP 007c8cf0). Single-precision SSE arithmetic in the original's order; x87 fld / fstp dword copies quiet a signalling NaN. */
#include "build_terrain.h"
#include "build_alloc.h"
#include "build_asset.h"
#include "build_render.h"
#include "build_world.h"
#include "map_scale.h"
#include "../src/rx_d3d.h"
#include "../src/fmt_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static uint32_t x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static void copy16(rx_addr dst, rx_addr src) { for (uint32_t k = 0; k < 64; k += 4) SU32(dst, k, U32(src, k)); }
static uint32_t vcall(rx_addr obj, uint32_t slot, int n, const uint32_t* a) { return rx_d3d_call(obj, slot, n, a); }

/* ---------------------------------------------------------------------------------------------------------------------------
 * RSData and handles */

rx_addr rx_asset_handle(const char* type, const char* name) {
    rx_addr db = U32(U32(0, 0x00a95844), 8);
    char t[9]; strncpy(t, type, 8); t[8] = 0;   /* strncpy(tag, type, 8) of 0080ec70 */
    rx_addr h = db ? rx_database_find_handle(db, t, name) : 0;
    if (!h) { fprintf(stderr, "rxcore: no %s handle %s (Database::vf27 creation not ported)\n", t, name); abort(); }
    return h;
}
rx_addr rx_rs_asset_handle(const struct fmt_entity* rs, const char* field) {
    const char* n = fmt_entity_asset(rs, field), * t = fmt_entity_asset_type(rs, field);
    if (!n || !t) { fprintf(stderr, "rxcore: entity %u: no asset field %s\n", rs->id, field); abort(); }
    return rx_asset_handle(t, n);
}
int rx_rs_has(const struct fmt_entity* rs, const char* field) { return fmt_entity_field_child(rs, field, 0) >= 0; }
int32_t rx_rs_enum(const struct fmt_entity* rs, const char* field) { return fmt_entity_enum(rs, field, 0); }
float rx_rs_float(const struct fmt_entity* rs, const char* field) {
    return fmt_entity_field_tagged(rs, field, "Float") >= 0 ? fmt_entity_float(rs, field, 0.0f) : 0.0f;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * constructors */

/* 007cc700 (EAX = surface): PlatformHFTerrainSurface's fields */
static void platform_surface_construct(rx_addr s) {
    SU32(s, 4, 0xf0);
    SU32(s, 0, 0x00971dfc);   /* PlatformHFTerrainSurface::vftable */
    SU32(s, 0x14, 0); SU32(s, 0x18, 0); SU32(s, 0x1c, 0);
    for (uint32_t k = 0xa0; k < 0xb8; k += 4) SU32(s, k, 0);
    SU32(s, 0xb8, 0);
    SU32(s, 0x24, 0); SU32(s, 0x2c, 0); SU32(s, 0x28, 0); SU32(s, 0x30, 0);
    SU32(s, 8, 0); SU32(s, 0xc, 0); SU32(s, 0x10, 0);
    for (uint32_t k = 0x48; k <= 0x5c; k += 4) SU32(s, k, 0);
    SF32(s, 0xbc, 2.0f /* 0x92e330 */);
}

/* 007c9160 (ESI = p): the PlatformHFProjectedTexture part. The first instance also creates the shared 0x300 x 0x600 render target
 * and its vertex declaration (globals 0x00d62780.., [0x00a95788]): render-only, not built. */
static void platform_projected_texture_create(rx_addr p) {
    SU32(p, 0, 0x00971de0);   /* PlatformHFProjectedTexture::vftable */
    SU32(0, 0x00a95784, U32(0, 0x00a95784) + 1);
    SU32(p, 8, 0); SU32(p, 0xc, 0); SU32(p, 0x10, 0);
}

/* 007ba010 */
rx_addr rx_hfprojected_texture_construct(rx_addr p) {
    platform_projected_texture_create(p);
    SU32(p, 0, 0x00971d40);   /* HFProjectedTexture::vftable */
    /* five 4x4 matrices from +0x20, each set to identity in the original's store order (zeros, then the diagonal) */
    static const uint16_t zero1[] = {0x24, 0x28, 0x2c, 0x30, 0x38, 0x3c, 0x40, 0x44, 0x4c, 0x50, 0x54, 0x58};
    for (unsigned k = 0; k < sizeof zero1 / sizeof *zero1; k++) SF32(p, zero1[k], 0.0f);
    SF32(p, 0x20, 1.0f); SF32(p, 0x34, 1.0f); SF32(p, 0x48, 1.0f); SF32(p, 0x5c, 1.0f);
    for (uint32_t m = 0x60; m <= 0xe0; m += 0x40) {
        static const uint16_t zero[] = {0x04, 0x08, 0x0c, 0x10, 0x18, 0x1c, 0x20, 0x24, 0x2c, 0x30, 0x34, 0x38};
        for (unsigned k = 0; k < sizeof zero / sizeof *zero; k++) SF32(p, m + zero[k], 0.0f);
        SF32(p, m, 1.0f); SF32(p, m + 0x14, 1.0f); SF32(p, m + 0x28, 1.0f); SF32(p, m + 0x3c, 1.0f);
    }
    for (uint32_t k = 0x120; k < 0x140; k += 4) SU32(p, k, 0);
    return p;
}

/* 007caa10 */
rx_addr rx_deformable_patch_construct(rx_addr d) {
    SU32(d, 0, 0x00971de8);   /* PlatformDeformablePatch::vftable */
    SU32(d, 0x14, 0); SU32(d, 0x18, 0); SU32(d, 0x1c, 0xffffffff);
    for (uint32_t k = 0; k < 2; k++) SU32(d + 0x30 + 0x208 * k, 0x200, 0);      /* Fn_007cda40 x 2 */
    for (uint32_t k = 0; k < 2; k++) SU32(d + 0x448 + 0x1e04 * k, 0x1e00, 0);   /* Fn_007cda80 x 2 */
    const float m1 = F32(0, 0x009cd9e8);   /* -1.0, a writable global */
    SU32(d, 0x10, 0); SU32(d, 0x20, 0); SU32(d, 0x24, 0); SU32(d, 0x41d0, 0); SU32(d, 0x440, 0); SU32(d, 0x444, 0);
    /* four corner vectors (x, y, u, v): (-1, -1, 0, 0) ... in the original's order */
    static const int8_t v[30] = {-1, -1, 0, 0, 0, -1, 1, 0, 0, 0, 1, -1, 0, 0, 0, 1, -1, 0, 0, 0, -1, 1, 0, 0, 0, 1, 1, 0, 0, 0};
    for (uint32_t k = 0; k < 30; k++) SF32(d, 0x41d4 + 4 * k, v[k] < 0 ? m1 : v[k] > 0 ? 1.0f : 0.0f);
    return d;
}

/* 007c4630 */
rx_addr rx_hfterrain_surface_construct(rx_addr s) {
    platform_surface_construct(s);
    SU32(s, 0, 0x00971cfc);   /* HFTerrainSurface::vftable */
    SU32(s, 0xc0, 0x00971d58); SU32(s, 0xc4, 0); SU32(s, 0xc8, 0);
    SU32(s, 0xcc, 0x00971d64); SU32(s, 0xd0, 0); SU32(s, 0xd4, 0); SU32(s, 0xe4, 0); SU32(s, 0xe8, 0);
    rx_hfprojected_texture_construct(s + 0xf0);
    rx_addr d = s + 0x230;
    rx_deformable_patch_construct(d);
    SU32(d, 0x426c, 0); SU32(d, 0x28, 0);
    SU32(d, 0, 0x00971d48);   /* DeformablePatch::vftable */
    SU32(s, 0x44ac, 0); SU32(s, 0x44b4, 0); SU32(s, 0x44b8, 0); SU32(s, 0x44bc, 0); SU32(s, 0x44c0, 0);
    SU32(s, 0x44b0, 0xffffffff);
    for (uint32_t k = 0x44c8; k <= 0x44e4; k += 4) SF32(s, k, 0.0f);
    SF32(s, 0x44e8, 1.0f); SF32(s, 0x44ec, 1.0f);
    SF32(s, 0x44f0, -30.0f /* 0x977e98 */);
    SU32(s, 0xc4, s);
    return s;
}

/* TEMP until scene unit: 007c8cf0 (stdcall entity; RET 4) */
rx_addr rx_hfterrain_entity_construct_temp(rx_addr e) {
    SU32(e, 0, 0x0095a2e4);   /* Entity::vftable */
    SU32(e, 0x18, 0); SU32(e, 0x10, 0);
    SF32(e, 0x24, 0.0f); SF32(e, 0x20, 0.0f);
    SU32(e, 0x70, 0);
    copy16(e + 0x30, 0x009cb510);   /* the identity matrix in .data */
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SF32(e, 0x78, 0.0f); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0);
    SU32(e, 0, 0x00971d6c);   /* HFTerrainEntity::vftable */
    rx_hfterrain_surface_construct(e + 0x90);
    SU32(e, 0x4590, 0);
    SU32(e, 0x70, 1);
    return e;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the height grid */

/* a texture asset's D3D texture: asset + 0x10 points at the holder of the IDirect3DTexture9 */
static rx_addr asset_texture(rx_addr asset) { return U32(U32(asset, 0x10), 0); }

/* GetType 3 (texture) or 5 (cube): GetLevelDesc(0) -> (width, height); 1 when read */
static int texture_size(rx_addr tex, uint32_t* w, uint32_t* h) {
    uint32_t a[1] = {tex};
    uint32_t type = vcall(tex, 10, 1, a);
    if (type != 3 && type != 5) return 0;
    RX_FRAME_ENTER(F, 0x20);
    uint32_t b[3] = {tex, 0, F};
    vcall(tex, 17, 3, b);
    *w = U32(F, 0x18); *h = U32(F, 0x1c);
    RX_FRAME_LEAVE(F, 0x20);
    return 1;
}

/* 007cc880 */
void rx_hfterrain_copy_heights(rx_addr s, rx_addr handle) {
    SU32(s, 0x14, handle);
    rx_addr asset = U32(handle, 0x44);
    SU32(handle, 0x4c, 1);
    uint32_t w = 0, h = 0;
    if (!texture_size(asset_texture(asset), &w, &h)) { fprintf(stderr, "rxcore: height map is not a texture\n"); abort(); }
    SU32(s, 0x1c, rx_reflex_aligned_alloc(rx_mul_sat(w * h, 4)));
    rx_addr tex = asset_texture(asset);
    RX_FRAME_ENTER(F, 8);
    uint32_t a[5] = {tex, 0, F, 0, 0x10};   /* LockRect(0, &rect, NULL, D3DLOCK_READONLY) */
    vcall(tex, 19, 5, a);
    rx_addr src = U32(F, 4); uint32_t pitch = U32(F, 0);
    RX_FRAME_LEAVE(F, 8);
    for (int32_t row = 0, off = 0; row < (int32_t)h; row++, off += (int32_t)(w * 4), src += pitch)
        for (uint32_t k = 0; k < w * 4; k += 4) SU32(U32(s, 0x1c) + (uint32_t)off, k, U32(src, k));   /* memcpy per row */
    {   /* MAP SCALE (port): the base heights (units of 2048 ft) scaled with the cell, so slopes keep their angle; the deformation's
         * gain (+0xbc) stays: ruts keep their real depth (nothing at 1) */
        const float ms = rx_ms_load();
        if (ms != 1.0f) for (uint32_t i = 0; i < w * h; i++) rx_ms_f(U32(s, 0x1c), 4 * i, ms);
    }
    if (fmt_modtrack_active() && w == 2048 && h == 2048)
        for(uint32_t i=0;i<2048u*2048u;i++) SF32(U32(s,0x1c),4*i,fmt_modtrack_height(i)*rx_ms_load()/2048.0f);
    uint32_t u[2] = {tex, 0};
    vcall(tex, 20, 2, u);   /* UnlockRect(0) */
    SU32(U32(s, 0x14), 0x4c, 0);
    /* the HFT_Helper shader and its light / reflection / bake / shadow shaders */
    rx_addr hh = rx_asset_handle("shader", "HFT_Helper");
    SU32(s, 0x44, hh);
    rx_addr sh = U32(hh, 0x44);
    SU32(hh, 0x4c, 1);
    rx_shader_find_vs(sh, "vs_spherical_light", s + 0x48, 0);   /* the inline search of 007cc9bf (vertex shader table) */
    rx_shader_find_ps(sh, "ps_spherical_light", s + 0x4c, 0);
    rx_shader_find_vs(sh, "vs_spherical_refl", s + 0x50, 0);
    rx_shader_find_ps(sh, "ps_spherical_refl", s + 0x54, 0);
    rx_shader_find_vs(sh, "vs_hft_bake_texture", s + 0x08, 0);
    rx_shader_find_ps(sh, "ps_hft_bake_texture_ext", s + 0x10, 0);
    rx_shader_find_vs(sh, "vs_hfb_shadows", s + 0x58, 0);
    rx_shader_find_ps(sh, "ps_hfb_shadows", s + 0x5c, 0);
    SU32(U32(s, 0x44), 0x4c, 0);
}

/* 007c4d10: per 16 x 16 cell block the (min, max) of 2048 * height over its 17 x 17 samples (shared edges; both bounds use the
 * height count as the original does), and the terrain's overall min / max */
void rx_hfterrain_coarse_bounds(rx_addr s) {
    int32_t cw = ((int32_t)U32(s, 0x44bc) + 0xf) / 16, ch = ((int32_t)U32(s, 0x44c0) + 0xf) / 16;   /* cdq / and 0xf / sar 4 */
    SU32(s, 0xd4, rx_reflex_aligned_alloc(rx_mul_sat((uint32_t)(ch * cw), 8)));
    rx_addr base = U32(s, 0x1c);
    SU32(s, 0xd8, (uint32_t)cw);
    if (!base) return;
    const float lo0 = -3.40282347e+38f /* 0x977550 */, hi0 = 3.40282347e+38f /* 0x977494 */;   /* the max / min starts */
    SF32(s, 0xe0, lo0);
    SF32(s, 0xdc, hi0);
    int32_t cell = 0;
    for (int32_t by = 0; by < I32(s, 0x44c0); by += 16) {
        for (int32_t bx = 0; bx < I32(s, 0x44bc); bx += 16, cell++) {
            rx_addr c = U32(s, 0xd4) + 8u * (uint32_t)cell;
            SF32(c, 4, lo0);
            SF32(c, 0, hi0);
            for (int32_t y = by; y < (I32(s, 0x44c0) < by + 0x11 ? I32(s, 0x44c0) : by + 0x11); y++)
                for (int32_t x = bx; x < (I32(s, 0x44c0) < bx + 0x11 ? I32(s, 0x44c0) : bx + 0x11); x++) {
                    float v = F32(base, 4u * (uint32_t)(I32(s, 0x44bc) * y + x)) * 2048.0f /* 0x92e22c */;
                    float mx = F32(c, 4);
                    SF32(c, 4, mx > v ? mx : v);          /* comiss max, v; ja keep */
                    float mn = F32(c, 0);
                    SF32(c, 0, v > mn ? mn : v);          /* comiss v, min; jbe take v */
                }
            float e0 = F32(s, 0xe0), cm = F32(c, 4);
            SF32(s, 0xe0, e0 > cm ? e0 : cm);
            float d0 = F32(s, 0xdc), cn = F32(c, 0);
            SF32(s, 0xdc, cn > d0 ? d0 : cn);
        }
    }
}

/* 007c4cc0 */
void rx_hfterrain_register(rx_addr s) {
    SU32(s, 0x44b4, 1);
    uint32_t n;
    if (!(U32(0, 0x009e30e0) & 1)) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); n = 0; SU32(0, 0x009e30dc, 0); }
    else n = U32(0, 0x009e30dc);
    for (uint32_t i = 0; i < n; i++) if (U32(0, RX_TERRAIN_LIST + 4 * i) == s) return;
    SU32(0, RX_TERRAIN_LIST + 4 * n, s);
    SU32(0, 0x009e30dc, U32(0, 0x009e30dc) + 1);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the deformable patch */

/* 007cacc0 (EAX = patch; stack w, h, shader asset; RET 0xc) */
void rx_deformable_patch_create(rx_addr d, uint32_t w, uint32_t h, rx_addr shader, const struct fmt_entity* rs) {
    rx_addr dev = rx_renderer_device();
    const rx_addr file = 0x00971770;   /* the creator's source file name in .rdata */
    rx_gpu_create_texture(w, h, 1, 1, RX_D3DFMT_R32F, 0, d + 8, 0x007cac50, d, 0, file, 0x4ce);
    rx_gpu_create_texture(w, h, 1, 1, RX_D3DFMT_R32F, 0, d + 0xc, 0x007cac50, d, 1, file, 0x4cf);
    { uint32_t a[3] = {U32(d, 8), 0, d + 0x20}; vcall(U32(d, 8), 18, 3, a); }       /* GetSurfaceLevel(0) */
    { uint32_t a[3] = {U32(d, 0xc), 0, d + 0x24}; vcall(U32(d, 0xc), 18, 3, a); }
    { uint32_t a[7] = {dev, w, h, RX_D3DFMT_R32F, RX_D3DPOOL_SYSTEMMEM, d + 0x10, 0}; vcall(dev, 36, 7, a); }   /* the deformation surface */
    RX_FRAME_ENTER(F, 8);
    rx_addr old_rt = F, old_ds = F + 4;
    { uint32_t a[3] = {dev, 0, old_rt}; vcall(dev, 38, 3, a); }                     /* GetRenderTarget(0) */
    { uint32_t a[2] = {dev, old_ds}; vcall(dev, 40, 2, a); }                        /* GetDepthStencilSurface */
    { uint32_t a[2] = {dev, 0}; vcall(dev, 39, 2, a); }                             /* SetDepthStencilSurface(NULL) */
    for (uint32_t k = 0; k < 2; k++) {
        uint32_t a[3] = {dev, 0, U32(d, 0x20 + 4 * k)}; vcall(dev, 37, 3, a);       /* SetRenderTarget(0, rt k) */
        uint32_t c[7] = {dev, 0, 0, 1, 0x80808080u, rx_fb(1.0f), 0}; vcall(dev, 43, 7, c);   /* Clear(target, 0x80808080, 1.0, 0) */
    }
    { uint32_t a[3] = {dev, 0, U32(old_rt, 0)}; vcall(dev, 37, 3, a); }
    { uint32_t a[2] = {dev, U32(old_ds, 0)}; vcall(dev, 39, 2, a); }
    if (U32(old_rt, 0)) { uint32_t a[1] = {U32(old_rt, 0)}; vcall(U32(old_rt, 0), 2, 1, a); SU32(old_rt, 0, 0); }   /* Release */
    if (U32(old_ds, 0)) { uint32_t a[1] = {U32(old_ds, 0)}; vcall(U32(old_ds, 0), 2, 1, a); SU32(old_ds, 0, 0); }
    RX_FRAME_LEAVE(F, 8);
    { rx_addr src = U32(d, U32(d, 0x14) == 0 ? 0x24 : 0x20);
      uint32_t a[3] = {dev, src, U32(d, 0x10)}; vcall(dev, 32, 3, a); }             /* GetRenderTargetData -> the deformation surface */
    SU32(d, 0x4264, rx_rs_asset_handle(rs, "TerrainDeformation"));
    SU32(d, 0x28, rx_rs_asset_handle(rs, "SoftnessMask"));
    /* the vertex declaration {0, 0, FLOAT3, POSITION}, {0, 0xc, FLOAT2, TEXCOORD}, D3DDECL_END */
    RX_FRAME_ENTER(G, 24);
    SU16(G, 0, 0); SU16(G, 2, 0); SU8(G, 4, 2); SU8(G, 5, 0); SU8(G, 6, 0); SU8(G, 7, 0);
    SU16(G, 8, 0); SU16(G, 10, 0xc); SU8(G, 12, 1); SU8(G, 13, 0); SU8(G, 14, 5); SU8(G, 15, 0);
    SU16(G, 16, 0xff); SU16(G, 18, 0); SU8(G, 20, 0x11); SU8(G, 21, 0); SU8(G, 22, 0); SU8(G, 23, 0);
    SU32(d, 0x424c, rx_vertex_decl_get(G, U32(U32(0, RX_RENDERER), 0x14)));
    RX_FRAME_LEAVE(G, 24);
    SU32(d, 0x4250, 0x14);
    rx_shader_find_vs(shader, "vs_hft_deform", d + 0x4254, 0);
    rx_shader_find_ps(shader, "ps_hft_deform", d + 0x4258, 0);
    rx_shader_find_vs(shader, "vs_hft_deform_copy", d + 0x425c, 0);
    rx_shader_find_ps(shader, "ps_hft_deform_copy", d + 0x4260, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * terrain tracks */

/* 007cb9a0 */
rx_addr rx_terrain_tracks_construct(rx_addr t, uint32_t ring) {
    SU32(t, 4, ring); SU32(t, 8, ring);
    SU32(t, 0xc, 0x18);
    SU32(t, 0, 0x00971df0);   /* PlatformTerrainTracks::vftable */
    SU32(t, 0x10, 0); SU32(t, 0x14, 0); SU32(t, 0x18, 0); SU32(t, 0x1c, 0);
    SU32(t, 0x20, 0xfe);
    SU32(t, 0x28, 0); SU32(t, 0x2c, 0); SU32(t, 0x30, 0); SU32(t, 0x34, 0);
    rx_addr strips = rx_reflex_aligned_alloc(rx_mul_sat(0x18, 0xc0));
    SU32(t, 0x24, strips);
    for (uint32_t k = 0; k < U32(t, 0xc) * 0xc0; k += 4) SU32(strips, k, 0);
    for (int32_t i = 0; i < I32(t, 0xc); i++) {
        uint32_t o = 0xc0u * (uint32_t)i;
        SU32(U32(t, 0x24) + o, 0, rx_reflex_aligned_alloc(rx_mul_sat(U32(t, 8), 0xc)));
        SU32(U32(t, 0x24) + o, 0xc, rx_reflex_aligned_alloc(rx_mul_sat(U32(t, 8), 4)));
        SU32(U32(U32(t, 0x24) + o, 0xc), 0, 1);
        rx_addr r = U32(t, 0x24) + o;
        SU32(r, 0x68, 0xffffffff);
        SU32(r, 0x20, U32(0, 0x009cb4c0)); SU32(r, 0x24, U32(0, 0x009cb4c4)); SU32(r, 0x28, U32(0, 0x009cb4c8));
        SF32(r, 0x40, 0.0f);
        SU32(r, 0x94, 1); SU32(r, 0x9c, 0); SU32(r, 0x98, 0); SU32(r, 0xa0, 1); SU32(r, 0xa4, 1); SU32(r, 0xac, 1); SU32(r, 0xb4, 0);
    }
    return t;
}

/* 007cbbf0 */
void rx_terrain_tracks_create_buffers(rx_addr t, rx_addr shader) {
    RX_FRAME_ENTER(F, 0x40);
    /* the vertex declaration {0, 0, type 0xa, 0, 0, 0}, D3DDECL_END */
    rx_addr decl = F + 0x2c;
    SU16(decl, 0, 0); SU16(decl, 2, 0); SU8(decl, 4, 0xa); SU8(decl, 5, 0); SU8(decl, 6, 0); SU8(decl, 7, 0);
    SU16(decl, 8, 0xff); SU16(decl, 10, 0); SU8(decl, 12, 0x11); SU8(decl, 13, 0); SU8(decl, 14, 0); SU8(decl, 15, 0);
    SU32(t, 0x18, rx_vertex_decl_get(decl, U32(U32(0, RX_RENDERER), 0x14)));
    SU32(t, 0x1c, 8);
    rx_gpu_create_vb((U32(t, 4) * U32(t, 0xc)) << 11, 0x208, t + 0x14, 0x007cc6f0, t, 0x00971770, 0x76b);
    rx_addr vbp = F + 0x28, ibp = F + 0x30;
    { uint32_t a[5] = {U32(t, 0x14), 0, 0, vbp, 0x2000}; vcall(U32(t, 0x14), 11, 5, a); }   /* Lock(0, 0, &p, NOOVERWRITE) */
    rx_gpu_create_ib16((U32(t, 4) * U32(t, 0xc)) << 9, t + 0x10);
    { uint32_t a[5] = {U32(t, 0x10), 0, 0, ibp, 0x2000}; vcall(U32(t, 0x10), 11, 5, a); }
    uint32_t vidx = 0, voff = 0, iidx = 0, ioff = 0;
    for (int32_t s = 0; s < I32(t, 0xc); s++) {
        uint32_t so = 0xc0u * (uint32_t)s;
        for (int32_t j = 0; j < I32(t, 4); j++) {
            SU32(U32(U32(t, 0x24) + so, 0), 0xcu * (uint32_t)j, voff);
            rx_addr p = U32(vbp, 0) + vidx * 8;
            for (uint32_t k = 0; k < 0x800; k += 4) SU32(p, k, 0);
            vidx += 0x100; voff += U32(t, 0x1c) << 8;
        }
        for (int32_t j = 0; j < I32(t, 4); j++) {
            rx_addr pt = U32(U32(t, 0x24) + so, 0) + 0xcu * (uint32_t)j;
            SU32(pt, 4, iidx);
            SU32(pt, 8, ioff);
            rx_addr p = U32(ibp, 0) + iidx * 2;
            for (uint32_t k = 0; k < 0x200; k += 4) SU32(p, k, 0);
            ioff += 0x200; iidx += 0x100;
        }
    }
    { uint32_t a[1] = {U32(t, 0x10)}; vcall(U32(t, 0x10), 12, 1, a); }   /* Unlock */
    { uint32_t a[1] = {U32(t, 0x14)}; vcall(U32(t, 0x14), 12, 1, a); }
    RX_FRAME_LEAVE(F, 0x40);
    rx_shader_find_vs(shader, "vs_hft_tracks", t + 0x30, 0);   /* the inline search of 007cbdc1 */
    rx_shader_find_ps(shader, "ps_hft_tracks", t + 0x34, 0);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the surface init and the entity */

/* 007c4890 */
void rx_hfterrain_surface_init(rx_addr s, int32_t width, int32_t height, int32_t spacing, rx_addr world, rx_addr hm,
                               const struct fmt_entity* rs) {
    SU32(s, 0x44c0, (uint32_t)height);
    SU32(s, 0x44bc, (uint32_t)width);
    SF32(s, 0x44e8, (float)spacing);
    SF32(s, 0x44c8, (float)width);
    SF32(s, 0x44cc, (float)height);
    float sp = F32(s, 0x44e8);
    SF32(s, 0x44d4, (float)(height * spacing) - sp);
    SF32(s, 0x44d0, (float)(width * spacing) - sp);
    SF32(s, 0x44ec, F32(s, 0x44e8) * 2.0f /* 0x92e330 */);
    SF32(s, 0x44d8, F32(s, 0x44d0) * -0.5f /* 0x9774b0 */);
    SF32(s, 0x44dc, F32(s, 0x44d4) * -0.5f);
    SF32(s, 0x44e0, F32(s, 0x44d0) * 0.5f /* 0x9322cc */);
    SF32(s, 0x44e4, F32(s, 0x44d4) * 0.5f);
    {   /* MAP SCALE (port, map_scale.h): the cell and so the terrain's extent scale; the heights below too (nothing at 1) */
        const float k = rx_ms_load();
        if (k != 1.0f) for (uint32_t o = 0x44d0; o <= 0x44ec; o += 4) if (o != 0x44c8 && o != 0x44cc) rx_ms_f(s, o, k);
    }
    rx_hfterrain_copy_heights(s, hm);
    rx_hfterrain_coarse_bounds(s);
    /* WaterMesh_Build (the clip-map render mesh, 0xf0 bytes with 0x80 / 0x40 rings, at +0xe8) and the mesh extents from the coarse
     * bounds: render-only, not built (+0xe8 stays 0) */
    (void)world;
    SU32(s, 0xd0, s + 0xf0);
    SU32(s, 0xf4, s);
    rx_addr dh = rx_asset_handle("shader", "HFT_Deform_Default");
    SU32(s, 0x44a0, dh);
    rx_addr dsh = U32(dh, 0x44);
    SU32(dh, 0x4c, 1);
    SU32(s + 0x230, 0x4268, s);
    rx_deformable_patch_create(s + 0x230, U32(s, 0x44bc), U32(s, 0x44c0), dsh, rs);
    SU32(U32(s, 0x44a0), 0x4c, 0);
    rx_addr th = rx_asset_handle("shader", "HFT_Track_Default");
    SU32(s, 0x44a4, th);
    rx_addr tsh = U32(th, 0x44);
    SU32(th, 0x4c, 1);
    static const uint32_t slots[3] = {0x00a95774, 0x00a95778, 0x00a9577c}, rings[3] = {0x1e, 0xa, 5};
    for (int k = 0; k < 3; k++) {
        if (U32(0, slots[k])) continue;
        rx_addr t = rx_reflex_aligned_alloc(0x38);
        if (t) { rx_terrain_tracks_construct(t, rings[k]); SU32(t, 0, 0x00971d50); /* TerrainTracks::vftable */ }
        SU32(0, slots[k], t);
        rx_terrain_tracks_create_buffers(t, tsh);
    }
    static const char* const tracks[6] = {"GenericTrack", "MXTrack", "ATVTrack", "UTVTrack", "ProLiteTrack", "BuggyTrack"};
    for (int k = 0; k < 6; k++) SU32(s, 0xa0 + 4u * (uint32_t)k, rx_rs_asset_handle(rs, tracks[k]));
    rx_addr soft = rx_rs_asset_handle(rs, "SoftnessMask");
    SU32(0, 0x00a95780, U32(0, 0x00a95780) + 1);
    SU32(s, 0xb8, soft);
    SU32(U32(s, 0x44a4), 0x4c, 0);
}

/* 007f9e70 (EAX = list {data, count, capacity, growth}, ESI = record): a (direction, entity, 0, range) record of 0x20 bytes appended
 * unless the entity is in the list; grows by FUN_007fa340 (new elements made by Fn_007d1360) */
static void scene_light_list_add(rx_addr list, rx_addr rec) {
    for (int32_t i = 0; i < I32(list, 4); i++) if (U32(U32(list, 0) + 0x20u * (uint32_t)i, 0x10) == U32(rec, 0x10)) return;
    if (I32(list, 4) >= I32(list, 8)) {
        uint32_t n = U32(list, 0xc) + U32(list, 8);
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(n, 0x20));
        if (nb) for (uint32_t k = 0; k < n; k++) {   /* Fn_007d1360 */
            rx_addr e = nb + 0x20 * k;
            SU32(e, 0x10, 0); SF32(e, 0x14, 0.0f);
            SU32(e, 0, U32(0, 0x00a95d50)); SU32(e, 4, U32(0, 0x00a95d54)); SU32(e, 8, U32(0, 0x00a95d58));
            SF32(e, 0x18, 15000.0f /* 0x977564 */);
        }
        for (uint32_t k = 0; k < U32(list, 8) * 0x20; k += 4) SU32(nb, k, U32(U32(list, 0), k));
        rx_reflex_aligned_free(U32(list, 0));
        SU32(list, 0, nb);
        SU32(list, 8, n);
    }
    uint32_t c = U32(list, 4);
    rx_addr e = U32(list, 0) + 0x20 * c;
    SU32(list, 4, c + 1);
    SU32(e, 0, x87_copy(U32(rec, 0))); SU32(e, 4, x87_copy(U32(rec, 4))); SU32(e, 8, x87_copy(U32(rec, 8)));
    SU32(e, 0x10, U32(rec, 0x10));
    SU32(e, 0x14, x87_copy(U32(rec, 0x14))); SU32(e, 0x18, x87_copy(U32(rec, 0x18)));
}

/* 00544cc0 (ESI = array {data, count, capacity, growth}): append a pointer unless present */
static void ptr_array_add_unique(rx_addr a, uint32_t v) {
    for (int32_t i = 0; i < I32(a, 4); i++) if (U32(U32(a, 0), 4u * (uint32_t)i) == v) return;
    if (I32(a, 4) >= I32(a, 8)) {
        uint32_t n = U32(a, 0xc) + U32(a, 8);
        rx_addr nb = rx_reflex_aligned_alloc(rx_mul_sat(n, 4));
        for (uint32_t k = 0; k < U32(a, 8) * 4; k += 4) SU32(nb, k, U32(U32(a, 0), k));
        rx_reflex_aligned_free(U32(a, 0));
        SU32(a, 0, nb);
        SU32(a, 8, n);
    }
    SU32(U32(a, 0), 4 * U32(a, 4), v);
    SU32(a, 4, U32(a, 4) + 1);
}

/* 007c8db0 after Entity::vf5 */
void rx_hfterrain_entity_load(rx_addr e, const struct fmt_entity* rs) {
    rx_addr material = rx_rs_asset_handle(rs, "MaterialAsset");
    rx_addr hm = rx_rs_asset_handle(rs, "HeightMapAsset");
    rx_addr friction = rx_rs_has(rs, "FrictionZones") ? rx_rs_asset_handle(rs, "FrictionZones") : 0;
    rx_addr asset = U32(hm, 0x44);
    SU32(hm, 0x4c, 1);
    uint32_t w = 0, h = 0;
    texture_size(asset_texture(asset), &w, &h);
    int32_t grid = rx_rs_enum(rs, "GridResolution");
    int32_t spacing = fmt_modtrack_active() ? 2 : 1 << (grid & 0x1f);
    RX_FRAME_ENTER(F, 8);   /* the world size pair passed by address */
    SF32(F, 0, (float)(int32_t)w * (float)spacing);
    SF32(F, 4, (float)(int32_t)h * (float)spacing);
    rx_hfterrain_surface_init(e + 0x90, (int32_t)w, (int32_t)h, spacing, F, hm, rs);
    float ww = F32(F, 0), wh = F32(F, 4);
    RX_FRAME_LEAVE(F, 8);
    { const float k = rx_ms_load(); if (k != 1.0f) { ww *= k; wh *= k; } }   /* MAP SCALE (port): the texture projection over the scaled extent */
    SU32(hm, 0x4c, 0);
    if (rx_rs_has(rs, "EdgeBuffer")) SF32(e, 0x4580, rx_rs_float(rs, "EdgeBuffer"));
    rx_ms_f(e, 0x4580, rx_ms_load());   /* MAP SCALE (port): the edge margin (surface +0x44f0) is a world distance */
    SU32(e, 0x453c, fmt_modtrack_active() ? 0 : friction);
    /* surface vf15 007c4ca0: PlatformHFTerrainSurface::vf15 (the material's effect parameters: render-only, not built), then +0xc8 */
    SU32(e + 0x90, 0xc8, material);
    SU32(e, 0x4538, U32(e, 0x10));
    SU32(e, 0x174, e);
    rx_hfterrain_register(e + 0x90);
    /* the scene's terrain texture projection (scene + 0x2cd0, 16 floats) */
    float ix = 1.0f / (ww * 0.5f), iz = 1.0f / (wh * 0.5f);
    float m[16];
    m[0] = ix * 0.5f; m[1] = m[2] = m[3] = ix * 0.0f;
    m[4] = 0.0f; m[5] = 0.0f; m[6] = F32(0, 0x009cd9e8); m[7] = 0.0f;
    m[8] = iz * 0.0f; m[9] = iz * -0.5f /* 0x9774b0 */; m[10] = m[8]; m[11] = m[8];
    m[12] = 0.5f; m[13] = 0.5f; m[14] = 0.0f; m[15] = 1.0f;
    rx_addr scene = U32(e, 0x10);
    for (int k = 0; k < 16; k++) SF32(scene, 0x2cd0 + 4u * (uint32_t)k, m[k]);
    /* 007e0e90 (EAX = scene + 0x1a18): the entity in the scene's light-receiver list (critical section scene + 0x1a18 + 0x112c) */
    RX_FRAME_ENTER(G, 0x20);
    SF32(G, 0x14, 0.0f);
    SU32(G, 0, U32(0, 0x00a95d50)); SU32(G, 4, U32(0, 0x00a95d54)); SU32(G, 0x10, e); SU32(G, 8, U32(0, 0x00a95d58));
    SF32(G, 0x18, 15000.0f /* 0x977564 */);
    scene_light_list_add(U32(e, 0x10) + 0x1a18 + 0x10c8, G);
    RX_FRAME_LEAVE(G, 0x20);
    ptr_array_add_unique(U32(e, 0x10) + 0x2a68, e);
}
