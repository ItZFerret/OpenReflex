/* build_render_asset_surface.c -- see build_render_asset.h. Hand transliteration of SurfaceAssetIO::vf3 007947b0, XenonCore::vf11 00843ac0
 * (the allocation), XenonSurfaceAsset::vf6 007b20f0 (the file), Fn_007b2250 (the per-surface constructor) and XenonTriangleSurface::vf1
 * 007b2330 from the disassembly. The D3D calls go through rx_d3d (device slots 26 / 27, buffer Lock 11 / Unlock 12) as the texture loader's. */
#include "build_render_asset.h"
#include "build_alloc.h"
#include "build_render.h"
#include "../src/rx_d3d.h"
#include "fx.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>

#define SURFACE_ASSET_VT   0x00971cdcu   /* XenonSurfaceAsset::vftable */
#define TRIANGLE_SURFACE_VT 0x00971ca4u  /* XenonTriangleSurface::vftable */

static uint32_t dcall(rx_addr obj, uint32_t slot, int n, const uint32_t* a) { return rx_d3d_call(obj, slot, n, a); }

/* Fn_007b2250 (ECX = the 0xcc-byte surface record): the constructor of the array's elements (the other words are not touched) */
static void triangle_surface_ctor(rx_addr t) {
    SU32(t, 4, 0); SU32(t, 0, TRIANGLE_SURFACE_VT); SU32(t, 8, 0); SU32(t, 0xc, 0); SU32(t, 0x10, 0); SU32(t, 0x18, 0); SU32(t, 0x1c, 0);
    SU32(t, 0xa8, 0); SU32(t, 0xa4, 0); SU32(t, 0xac, 0); SU32(t, 0x14, 5); SU32(t, 0xc4, 0); SU32(t, 0xc8, 0);
    SU32(t, 0xb0, 0); SU32(t, 0xb8, 0); SU32(t, 0xbc, 0); SU32(t, 0xb4, 0); SU32(t, 0x20, 0);
}

/* the buffer's data from the stream: Lock(0, 0), read `size` bytes, Unlock */
static void fill_buffer(rx_stream* s, rx_addr buf, uint32_t size) {
    RX_FRAME_ENTER(F, 0x10);
    SU32(F, 0, 0);
    { uint32_t a[5] = {buf, 0, 0, F, 0}; dcall(buf, 11, 5, a); }       /* Lock */
    rx_stream_read(s, U32(F, 0), size);
    { uint32_t a[1] = {buf}; dcall(buf, 12, 1, a); }                    /* Unlock */
    RX_FRAME_LEAVE(F, 0x10);
}

/* XenonTriangleSurface::vf1 007b2330 (ECX = the surface record; stack: the file version, the stream, the 0x160-byte descriptor; RET 0xc).
 * Descriptor: +0 kind, +8 flags (bit 1: the value kept in +4; bit 3: 32-bit indices), +0xa u16, +0xc / +0x10 (to +8 / +0xc), +0x14 the index
 * data size, +0x18 the vertex element list (to the declaration cache), +0xb8 the vertex stream count, +0xbc.. the streams' byte sizes,
 * +0xfc the skin data size (version >= 2). Record: +4 the flag above, +8 / +0xc / +0x10 the descriptor's, +0x14 4 or 5, +0x18 the index
 * buffer, +0x1c the declaration, +0x24.. the vertex buffers (+0x64.. their vertex counts), +0xa4 the descriptor, +0xc4 / +0xc8 the skin data. */
static void triangle_surface_init(rx_addr t, uint32_t version, rx_stream* s, rx_addr d) {
    const rx_addr dev = rx_renderer_device();
    SU32(t, 0x14, (U32(d, 0) != 1) + 4u);
    SU32(t, 8, U32(d, 0xc));
    SU32(t, 0xc, U32(d, 0x10));
    SU32(t, 0x10, U32(d, 0xb8));
    SU32(t, 0xa4, d);
    if (U16(d, 0xa) == 0 && (U8(d, 8) & 2)) SU16(d, 0xa, 4);
    if ((int32_t)version < 2) SU32(d, 0xfc, 0);
    const uint32_t skin = U32(d, 0xfc);
    SU32(t, 0xc8, skin);
    if (skin) {
        rx_addr p = rx_reflex_aligned_alloc(skin);
        SU32(t, 0xc4, p);
        rx_stream_read(s, p, skin);
    }
    SU32(t, 0x1c, rx_vertex_decl_get(d + 0x18, U32(U32(0, RX_RENDERER), 0x14)));
    const uint32_t fmt = (U8(d, 8) & 8) ? 0x66u : 0x65u;
    { uint32_t a[7] = {dev, U32(d, 0x14), 8, fmt, 1, t + 0x18, 0}; dcall(dev, 27, 7, a); }   /* CreateIndexBuffer */
    fill_buffer(s, U32(t, 0x18), U32(d, 0x14));
    for (int32_t i = 0; i < (int32_t)U32(t, 0x10); i++) {
        const rx_addr vb = t + 0x24 + 4u * (uint32_t)i;
        const uint32_t len = U32(d, 0xbc + 4u * (uint32_t)i);
        SU32(vb, 0x40, (uint32_t)((int32_t)len / (int32_t)U32(d, 0x10)));
        { uint32_t a[7] = {dev, len, 8, 0, 1, vb, 0}; dcall(dev, 26, 7, a); }                /* CreateVertexBuffer */
        fill_buffer(s, U32(vb, 0), len);
    }
    SU32(t, 4, (U16(U32(t, 0xa4), 8) >> 1) & 1u);
}

/* XenonSurfaceAsset::vf6 007b20f0 (ECX = the asset; stack: the stream): header {?, version, count, ?} (16 bytes), count 0x160-byte descriptors,
 * the 0xcc-byte surface array (+0x14; a 4-byte count in front of it), and for each descriptor of kind 0 / 1 the surface's vf1 */
static void surface_asset_read(rx_addr a, rx_stream* s) {
    uint32_t h[4];
    rx_stream_read_host(s, h, 0x10);
    const uint32_t version = h[1], count = h[2];
    SU32(a, 0x10, count);
    rx_addr desc = 0;
    if (count) desc = rx_reflex_aligned_alloc(rx_mul_sat(count, 0x160));
    SU32(a, 0x18, desc);
    rx_stream_read(s, desc, U32(a, 0x10) * 0x160);
    rx_addr arr = 0;
    if (count) {
        uint32_t bytes = rx_mul_sat(count, 0xcc);
        bytes = bytes + 4 < bytes ? 0xffffffffu : bytes + 4;
        rx_addr p = rx_reflex_aligned_alloc(bytes);
        if (p) {
            SU32(p, 0, count);
            for (uint32_t i = 0; i < count; i++) triangle_surface_ctor(p + 4 + 0xccu * i);
            arr = p + 4;
        }
    }
    SU32(a, 0x14, arr);
    for (int32_t i = 0; i < (int32_t)U32(a, 0x10); i++) {
        rx_addr d = U32(a, 0x18) + 0x160u * (uint32_t)i;
        if (U32(d, 0) <= 1) triangle_surface_init(U32(a, 0x14) + 0xccu * (uint32_t)i, version, s, d);
    }
}

/* SurfaceAssetIO::vf3 007947b0: XenonCore::vf11 (0x1c bytes, vtable, the rest 0), then its vf6 with the stream */
rx_addr rx_surface_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x1c);
    if (a) {
        SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, SURFACE_ASSET_VT); SU32(a, 0x10, 0); SU32(a, 0x14, 0); SU32(a, 0x18, 0);
    }
    surface_asset_read(a, s);
    return a;
}

/* XenonSurfaceAsset::vf4 00794f90 (thiscall: index; RET 4): the address of surface record `index` (0xcc bytes each) */
rx_addr rx_surface_asset_get(rx_addr asset, uint32_t index) { return index * 0xccu + U32(asset, 0x14); }
RX_FUNCTION(0x00794f90, rx_surface_asset_get)

/* ---- the destruction (an asset whose last reference goes: Database vf31 0084af30, e.g. a removed rider's / vehicle's model) ------
 * The package unload runs these on the database thread, whose stack is never reused for anything else below the loader's wait:
 * the frames are the original's (pushes, arguments, return addresses, ReflexAlignedFree's / _aligned_free's / free's frames:
 * build_alloc.c rx_reflex_aligned_free_at), so its dead stack stays as the original leaves it. Each is entered with the stack
 * pointer at its return slot; the callee-saved registers are the fx register model (fx_util.h; the adapters bind it). */

static crt_regs4 surf_regs(uint32_t esi) { const crt_regs4 r = {fx_reg(FX_EBX), fx_reg(FX_EBP), esi, fx_reg(FX_EDI)}; return r; }
/* `push p; call ReflexAlignedFree` with the return slot e (the argument at e + 4) */
static void surf_free(rx_addr e, rx_addr p, rx_addr ret, uint32_t esi) {
    SU32(e, 4, p); SU32(e, 0, ret);
    rx_reflex_aligned_free_at(e, surf_regs(esi), p);
}
/* a COM Release (slot 2) as `push obj; call edx` makes it: the argument at e + 4, the return address at e */
static void surf_release(rx_addr e, rx_addr obj, rx_addr ret) {
    const rx_addr sp = rx_sp();
    rx_set_sp(e + 8);
    uint32_t a[1] = {obj};
    dcall(obj, 2, 1, a);
    SU32(e, 4, obj); SU32(e, 0, ret);
    rx_set_sp(sp);
}

/* XenonTriangleSurface's destructor 007b22b0 (ECX = the record): the skin data (+0xc4) freed, the index buffer (+0x18), the
 * declaration (+0x1c) and the +0x10 vertex buffers (+0x24 ..) released (IUnknown::Release, slot 2) and cleared; the base vtable
 * 0x00971a58. Frame: push ebx; push esi (ESI = the record); EBX = 0 and push edi (EDI = the slot) for the vertex buffers. */
void rx_triangle_surface_destroy(rx_addr t) {
    const rx_addr E = rx_sp();
    const fx_saved in = fx_save();
    SU32(E, -4, in.ebx); SU32(E, -8, in.esi);
    rx_set_sp(E - 8);
    fx_set_reg(FX_ESI, t);
    SU32(t, 0, TRIANGLE_SURFACE_VT);
    surf_free(E - 0x10, U32(t, 0xc4), 0x007b22c6, t);
    if (U32(t, 0x18)) { surf_release(E - 0x10, U32(t, 0x18), 0x007b22d8); SU32(t, 0x18, 0); }
    if (U32(t, 0x1c)) { surf_release(E - 0x10, U32(t, 0x1c), 0x007b22ee); SU32(t, 0x1c, 0); }
    fx_set_reg(FX_EBX, 0);
    if (I32(t, 0x10) > 0) {
        SU32(E, -0xc, in.edi);
        for (int32_t i = 0; i < I32(t, 0x10); i++) {
            const rx_addr slot = t + 0x24 + 4u * (uint32_t)i;
            fx_set_reg(FX_EDI, slot);
            if (!U32(slot, 0)) continue;
            surf_release(E - 0x14, U32(slot, 0), 0x007b230e);
            SU32(slot, 0, 0);
        }
    }
    SU32(t, 0, 0x00971a58);
    fx_restore(in);
    rx_set_sp(E);
}
RX_FUNCTION(0x007b22b0, rx_triangle_surface_destroy)

/* XenonTriangleSurface::sdtor 007b21f0 (thiscall; flags; RET 4): flag 2 = the array form (the count at -4: every element's destructor
 * through the vector destructor iterator 00402500, last first; the block from -4 freed with flag 1), else the one record.
 * Frame: push ebx (BL = the flags); push esi (ESI = the record); the array form: push edi (EDI = the block), the iterator's four
 * arguments. */
rx_addr rx_triangle_surface_sdtor(rx_addr t, uint32_t flags) {
    const rx_addr E = rx_sp();
    const fx_saved in = fx_save();
    SU32(E, -4, in.ebx); SU32(E, -8, in.esi);
    fx_set_reg(FX_EBX, (in.ebx & ~0xffu) | (flags & 0xffu)); fx_set_reg(FX_ESI, t);
    rx_addr r = t;
    if (flags & 2) {
        const rx_addr block = t - 4;
        const int32_t count = I32(block, 0);
        SU32(E, -0xc, in.edi);
        fx_set_reg(FX_EDI, block);
        SU32(E, -0x10, 0x007b22b0); SU32(E, -0x14, (uint32_t)count); SU32(E, -0x18, 0xcc); SU32(E, -0x1c, t); SU32(E, -0x20, 0x007b2215);
        rx_set_sp(E - 0x20);
        rx_fx_vector_dtor_iterator(t, 0xcc, count, 0x007b22b0);
        rx_set_sp(E - 0xc);
        if (flags & 1) surf_free(E - 0x14, block, 0x007b2220, t);
        r = block;
    } else {
        SU32(E, -0xc, 0x007b2230);
        rx_set_sp(E - 0xc);
        rx_triangle_surface_destroy(t);
        rx_set_sp(E - 8);
        if (flags & 1) surf_free(E - 0x10, t, 0x007b223b, t);
    }
    fx_restore(in);
    rx_set_sp(E);
    return r;
}
RX_FUNCTION(0x007b21f0, rx_triangle_surface_sdtor)

/* XenonSurfaceAsset's destructor 007b2060 (ECX = the asset): the surface array (+0x14: its count at -4) deleted through its first
 * element's vf0 with flag 3 (or, empty, the block freed), the descriptors (+0x18) freed; the base vtable 0x0093eb78.
 * Frame: push esi (ESI = the asset). */
void rx_surface_asset_destroy(rx_addr a) {
    const rx_addr E = rx_sp();
    const fx_saved in = fx_save();
    const rx_addr surfaces = U32(a, 0x14);
    SU32(E, -4, in.esi);
    rx_set_sp(E - 4);
    fx_set_reg(FX_ESI, a);
    SU32(a, 0, SURFACE_ASSET_VT);
    rx_addr ret = 0x007b20a7;
    if (surfaces) {
        if (U32(surfaces, -4) != 0) {
            const rx_addr fn = U32(U32(surfaces, 0), 0);
            rx_addr (*sdtor)(rx_addr, uint32_t) = RX_FN(fn, rx_addr (*)(rx_addr, uint32_t));
            if (!sdtor) { fprintf(stderr, "rxcore: XenonSurfaceAsset: the surface destructor %08x is not ported\n", fn); abort(); }
            SU32(E, -8, 3); SU32(E, -0xc, 0x007b2081);
            rx_set_sp(E - 0xc);
            sdtor(surfaces, 3);
            rx_set_sp(E - 4);
            ret = 0x007b208a;
        } else {
            surf_free(E - 0xc, surfaces - 4, 0x007b209b, a);
        }
    }
    surf_free(E - 0xc, U32(a, 0x18), ret, a);
    SU32(a, 0, 0x0093eb78);
    fx_restore(in);
    rx_set_sp(E);
}
RX_FUNCTION(0x007b2060, rx_surface_asset_destroy)

/* XenonSurfaceAsset::sdtor 007b2040 (thiscall; flags; RET 4). Frame: push esi (ESI = the asset). */
rx_addr rx_surface_asset_sdtor(rx_addr a, uint32_t flags) {
    const rx_addr E = rx_sp();
    const fx_saved in = fx_save();
    SU32(E, -4, in.esi); SU32(E, -8, 0x007b2048);
    fx_set_reg(FX_ESI, a);
    rx_set_sp(E - 8);
    rx_surface_asset_destroy(a);
    rx_set_sp(E - 4);
    if (flags & 1) surf_free(E - 0xc, a, 0x007b2055, a);
    fx_restore(in);
    rx_set_sp(E);
    return a;
}
RX_FUNCTION(0x007b2040, rx_surface_asset_sdtor)

/* XenonSurfaceAsset::vf2 007b20c0: the surface AssetIO (the static object 0x00da67f0, its vtable written on the first call: guard
 * 0x00da67e4) */
rx_addr rx_surface_asset_get_io(rx_addr a) {
    (void)a;
    if (!(U8(0, 0x00da67e4) & 1)) {
        SU32(0, 0x00da67e4, U32(0, 0x00da67e4) | 1);
        SU32(0, 0x00da67f0, 0x00971c08);
    }
    return 0x00da67f0;
}
RX_FUNCTION(0x007b20c0, rx_surface_asset_get_io)
