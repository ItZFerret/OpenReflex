/* rx_d3d.c -- see rx_d3d.h. */
#include "rx_d3d.h"
#include "rx_fn.h"
#include "rx_heap.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef uint32_t (*d3d_method)(const uint32_t* a);

/* object fields */
enum { O_VT = 0x00, O_REFS = 0x04, O_KIND = 0x08, O_W = 0x0c, O_H = 0x10, O_LEVELS = 0x14, O_FMT = 0x18, O_USAGE = 0x1c, O_POOL = 0x20,
       O_LTAB = 0x24, O_CONT = 0x28, O_LEVEL = 0x2c, O_SIZE = 0x30, O_FVF = 0x34, O_DATA = 0x38 };
enum { L_BITS = 0x00, L_PITCH = 0x04, L_BYTES = 0x08, L_W = 0x0c, L_H = 0x10, L_SURF = 0x14, L_SIZE = 0x18 };
enum { D_RT = 0x10, D_DS = 0x20, D_BACK = 0x24, D_DEPTH = 0x28 };
enum { IF_DEVICE = 0, IF_TEXTURE = 1, IF_SURFACE = 2, IF_VB = 3, IF_IB = 4, IF_DECL = 5, IF_VS = 6, IF_PS = 7, IF_CT = 8, IF_CUBE = 9 };

#define D3DERR_NOTAVAILABLE 0x8876086cu

uint32_t rx_d3d_call(rx_addr obj, uint32_t slot, int n, const uint32_t* a) {
    rx_addr entry = U32(U32(obj, 0), 4 * slot);
    d3d_method f = (d3d_method)rx_fn(entry);
    if (f) return f(a);
    return rx_call_original(entry, RX_STDCALL, 0, n, a);
}

#ifndef RX_ORACLE
static uint32_t format_bits(uint32_t f) {   /* boot_devices.inc format_bits */
    switch (f) {
        case 20: return 24;
        case 21: case 22: case 31: case 32: case 33: case 34: case 35: case 63: case 64: case 67: case 71: case 75: case 77: case 79: case 114: return 32;
        case 23: case 24: case 25: case 26: case 29: case 30: case 51: case 60: case 80: case 81: case 111: case 70: case 73: return 16;
        case 28: case 50: case 41: case 52: return 8;
        case 36: case 113: case 115: case 110: return 64;
        case 116: return 128;
        default: return 32;
    }
}
static int format_dxt(uint32_t f, uint32_t* block) {
    if (f == 0x31545844u || f == 0x31495441u) { *block = 8; return 1; }
    if (f == 0x32545844u || f == 0x33545844u || f == 0x34545844u || f == 0x35545844u || f == 0x32495441u) { *block = 16; return 1; }
    return 0;
}
static void level_layout(uint32_t fmt, uint32_t w, uint32_t h, uint32_t* pitch, uint32_t* bytes) {
    uint32_t blk;
    if (format_dxt(fmt, &blk)) { *pitch = ((w + 3) / 4 ? (w + 3) / 4 : 1) * blk; *bytes = *pitch * ((h + 3) / 4 ? (h + 3) / 4 : 1); }
    else { uint32_t p = w * format_bits(fmt) / 8; if (!p) p = 1; *pitch = (p + 3) & ~3u; *bytes = *pitch * h; }
}
/* ---- GetRenderTargetData with a dirty rectangle (w-perfgrow) -------------------------------------------------------------------------
 * The terrain's deformation readback (HFTerrainSurface::vf14, every frame) copied the whole 2048 x 2048 R32F target (16 MB) into the physics
 * surface, and the host export (rx_session_terrain) compared that whole surface (16 MB, again) to find what changed: ~0.4 ms + ~0.4 ms of a
 * 4.5 ms core frame, for a frame that changes ~60-100 texels. The only writers of the targets are the deformation pass (game/fx_deform.c:
 * it names the rectangle it wrote, rx_d3d_track_note), Clear (any fill) and a LockRect that is not read-only; the only writer of the
 * physics surface is this copy and a LockRect that is not read-only. So a destination the deformation pass has announced is copied
 * rectangle by rectangle (the rectangle written since the last copy). Everything else - the first copy, a Clear, a writing lock, a created
 * resource, sizes that do not match - copies it all, as before. The rectangles copied, accumulated until the host export reads them
 * (rx_d3d_track_take), tell the export where the surface can have changed. RX_D3D_VERIFY=1 checks every copy against the full copy
 * (abort on a difference). Host bookkeeping only; nothing is written to the guest. */
typedef struct { rx_addr dst_bits; int valid; uint32_t epoch; int32_t cx0, cz0, cx1, cz1; int exp_valid; int32_t ex0, ez0, ex1, ez1; uint64_t full, part, bytes_part; } d3d_track;
#define D3D_TRACKS 4
static d3d_track g_tracks[D3D_TRACKS];
static uint32_t g_track_epoch = 1;
static int g_track_verify = -1;
static void rect_union(int32_t* x0, int32_t* z0, int32_t* x1, int32_t* z1, int32_t a0, int32_t b0, int32_t a1, int32_t b1) {
    if (a1 <= a0 || b1 <= b0) return;
    if (*x1 <= *x0 || *z1 <= *z0) { *x0 = a0; *z0 = b0; *x1 = a1; *z1 = b1; return; }
    if (a0 < *x0) *x0 = a0;
    if (b0 < *z0) *z0 = b0;
    if (a1 > *x1) *x1 = a1;
    if (b1 > *z1) *z1 = b1;
}
static d3d_track* track_find(rx_addr dst_bits, int create) {
    for (int i = 0; i < D3D_TRACKS; i++) if (g_tracks[i].dst_bits == dst_bits) return &g_tracks[i];
    if (!create) return NULL;
    for (int i = 0; i < D3D_TRACKS; i++) if (!g_tracks[i].dst_bits) { memset(&g_tracks[i], 0, sizeof g_tracks[i]); g_tracks[i].dst_bits = dst_bits; return &g_tracks[i]; }
    return NULL;
}
static void track_invalidate_all(void) { g_track_epoch++; }
/* the deformation pass wrote texels [x0, x1) x [z0, z1) of the render targets that feed the surface `dst_obj` */
void rx_d3d_track_note(rx_addr dst_obj, int32_t x0, int32_t z0, int32_t x1, int32_t z1) {
    uint32_t pitch = 0; const rx_addr bits = dst_obj ? rx_d3d_bits(dst_obj, 0, &pitch) : 0;
    if (!bits) return;
    d3d_track* t = track_find(bits, 1);
    if (t) rect_union(&t->cx0, &t->cz0, &t->cx1, &t->cz1, x0, z0, x1, z1);
}
/* the host export asks where the surface at `bits` can differ from what it saw at the last call: 1 and rect[4] = x0, z0, x1, z1 (empty
 * when x1 <= x0), or 0 = unknown (it must look at everything). Either way the next call reports what changed after this one. */
int rx_d3d_track_take(rx_addr bits, int32_t* rect) {
    d3d_track* t = track_find(bits, 0);
    if (!t || !t->valid) return 0;
    const int known = t->exp_valid;
    if (known) { rect[0] = t->ex0; rect[1] = t->ez0; rect[2] = t->ex1; rect[3] = t->ez1; }
    t->exp_valid = 1; t->ex0 = t->ez0 = t->ex1 = t->ez1 = 0;
    return known;
}
/* tools: the copies made with a dirty rectangle (part) and in full (full) for the destinations the deformation pass announced, the bytes the
 * partial copies moved, and the epoch (every Clear / writing lock / created resource bumps it) */
void rx_d3d_track_stats(uint64_t* part, uint64_t* full, uint64_t* bytes_part, uint32_t* epoch) {
    uint64_t p = 0, f = 0, b = 0;
    for (int i = 0; i < D3D_TRACKS; i++) { p += g_tracks[i].part; f += g_tracks[i].full; b += g_tracks[i].bytes_part; }
    *part = p; *full = f; *bytes_part = b; *epoch = g_track_epoch;
}
static void lock_note(rx_addr bits, uint32_t flags) {   /* a lock that may write (not D3DLOCK_READONLY 0x10) */
    if (flags & 0x10u) return;
    for (int i = 0; i < D3D_TRACKS; i++) if (g_tracks[i].dst_bits == bits) { g_tracks[i].valid = 0; g_tracks[i].exp_valid = 0; return; }
    track_invalidate_all();   /* possibly a render target of a track */
}

static rx_addr new_object(rx_addr vt, uint32_t kind) {
    rx_addr o = rx_heap_alloc(0x40, 16);
    SU32(o, O_VT, vt); SU32(o, O_REFS, 1); SU32(o, O_KIND, kind);
    return o;
}
/* a level table of n levels (bits allocated, at least 16 bytes each) */
static rx_addr new_levels(uint32_t n, uint32_t fmt, uint32_t w, uint32_t h) {
    track_invalidate_all();   /* a new resource may reuse the bits of a freed one */
    rx_addr t = rx_heap_alloc(L_SIZE * (n ? n : 1), 16);
    for (uint32_t l = 0; l < n; l++) {
        uint32_t lw = w >> l ? w >> l : 1, lh = h >> l ? h >> l : 1, pitch, bytes;
        level_layout(fmt, lw, lh, &pitch, &bytes);
        rx_addr e = t + L_SIZE * l;
        SU32(e, L_BITS, rx_heap_alloc(bytes > 16 ? bytes : 16, 16)); SU32(e, L_PITCH, pitch); SU32(e, L_BYTES, bytes);
        SU32(e, L_W, lw); SU32(e, L_H, lh); SU32(e, L_SURF, 0);
    }
    return t;
}
/* the level entry behind a texture level or a surface */
static rx_addr level_entry(rx_addr o, uint32_t level) {
    if (U32(o, O_KIND) == RX_D3D_SURFACE && U32(o, O_CONT)) { level = U32(o, O_LEVEL); o = U32(o, O_CONT); }
    else if (U32(o, O_KIND) == RX_D3D_SURFACE) level = 0;
    if (level >= (U32(o, O_KIND) == RX_D3D_CUBE ? U32(o, O_SIZE) : U32(o, O_LEVELS))) return 0;
    return U32(o, O_LTAB) + L_SIZE * level;
}
static rx_addr owner_of(rx_addr o) { return U32(o, O_KIND) == RX_D3D_SURFACE && U32(o, O_CONT) ? U32(o, O_CONT) : o; }

static void fatal(const char* what, uint32_t slot) { fprintf(stderr, "rxcore: rx_d3d: %s slot %u not supported\n", what, slot); abort(); }

/* ---- IUnknown / resources ---- */
static uint32_t m_query(const uint32_t* a) { SU32(a[2], 0, a[0]); SU32(a[0], O_REFS, U32(a[0], O_REFS) + 1); return 0; }
static uint32_t m_addref(const uint32_t* a) { SU32(a[0], O_REFS, U32(a[0], O_REFS) + 1); return U32(a[0], O_REFS); }
static uint32_t m_release(const uint32_t* a) { uint32_t n = U32(a[0], O_REFS); if (n) n--; SU32(a[0], O_REFS, n); return n; }   /* memory kept, as the boot's device */
static rx_addr g_device;
static uint32_t m_get_device(const uint32_t* a) { SU32(a[1], 0, g_device); return 0; }
static uint32_t m_ok(const uint32_t* a) { (void)a; return 0; }
static uint32_t m_get_type(const uint32_t* a) {
    switch (U32(a[0], O_KIND)) { case RX_D3D_SURFACE: return 1; case RX_D3D_TEXTURE: return 3; case RX_D3D_CUBE: return 5; case RX_D3D_VB: return 6; case RX_D3D_IB: return 7; }
    return 0;
}

/* D3DSURFACE_DESC {Format, Type, Usage, Pool, MultiSampleType, MultiSampleQuality, Width, Height} */
static void surface_desc(rx_addr o, uint32_t level, rx_addr out) {
    rx_addr e = level_entry(o, level), r = owner_of(o);
    SU32(out, 0, U32(r, O_FMT)); SU32(out, 4, 1); SU32(out, 8, U32(r, O_USAGE)); SU32(out, 12, U32(r, O_POOL));
    SU32(out, 16, 0); SU32(out, 20, 0); SU32(out, 24, e ? U32(e, L_W) : 0); SU32(out, 28, e ? U32(e, L_H) : 0);
}
/* D3DLOCKED_RECT {Pitch, pBits}; a rect moves pBits to its top-left texel */
static uint32_t lock_rect(rx_addr o, uint32_t level, rx_addr out, rx_addr rect, uint32_t flags) {
    rx_addr e = level_entry(o, level);
    if (!e) return D3DERR_NOTAVAILABLE;
    lock_note(U32(e, L_BITS), flags);
    rx_addr bits = U32(e, L_BITS); uint32_t pitch = U32(e, L_PITCH), blk, fmt = U32(owner_of(o), O_FMT);
    if (rect) {
        int32_t x = I32(rect, 0), y = I32(rect, 4);
        if (format_dxt(fmt, &blk)) bits += (uint32_t)(y / 4) * pitch + (uint32_t)(x / 4) * blk;
        else bits += (uint32_t)y * pitch + (uint32_t)x * format_bits(fmt) / 8;
    }
    SU32(out, 0, pitch); SU32(out, 4, bits);
    return 0;
}

/* ---- textures ---- */
static rx_addr surface_of(rx_addr tex, uint32_t level) {
    rx_addr e = level_entry(tex, level);
    if (!e) return 0;
    if (!U32(e, L_SURF)) {
        rx_addr s = new_object(RX_D3D_VT_SURFACE, RX_D3D_SURFACE);
        SU32(s, O_CONT, tex); SU32(s, O_LEVEL, level);
        SU32(e, L_SURF, s);
    } else SU32(U32(e, L_SURF), O_REFS, U32(U32(e, L_SURF), O_REFS) + 1);
    return U32(e, L_SURF);
}
static uint32_t t_level_count(const uint32_t* a) { return U32(a[0], O_LEVELS); }
static uint32_t t_get_level_desc(const uint32_t* a) { if (a[1] >= U32(a[0], O_LEVELS)) return D3DERR_NOTAVAILABLE; surface_desc(a[0], a[1], a[2]); return 0; }
static uint32_t t_get_surface_level(const uint32_t* a) { rx_addr s = surface_of(a[0], a[1]); SU32(a[2], 0, s); return s ? 0 : D3DERR_NOTAVAILABLE; }
static uint32_t t_lock_rect(const uint32_t* a) { return lock_rect(a[0], a[1], a[2], a[3], a[4]); }
static uint32_t t_autogen(const uint32_t* a) { (void)a; return 2; }

/* ---- surfaces ---- */
static uint32_t s_get_container(const uint32_t* a) { SU32(a[2], 0, U32(a[0], O_CONT) ? U32(a[0], O_CONT) : g_device); return 0; }
static uint32_t s_get_desc(const uint32_t* a) { surface_desc(a[0], 0, a[1]); return 0; }
static uint32_t s_lock_rect(const uint32_t* a) { return lock_rect(a[0], 0, a[1], a[2], a[3]); }

/* ---- buffers ---- */
static uint32_t b_lock(const uint32_t* a) { SU32(a[3], 0, U32(a[0], O_DATA) + a[1]); return 0; }
static uint32_t b_get_desc(const uint32_t* a) {   /* D3DVERTEXBUFFER_DESC {Format, Type, Usage, Pool, Size, FVF}; index: no FVF */
    int vb = U32(a[0], O_KIND) == RX_D3D_VB;
    SU32(a[1], 0, vb ? 100u : U32(a[0], O_FVF)); SU32(a[1], 4, vb ? 6u : 7u); SU32(a[1], 8, U32(a[0], O_USAGE)); SU32(a[1], 12, U32(a[0], O_POOL));
    SU32(a[1], 16, U32(a[0], O_SIZE)); if (vb) SU32(a[1], 20, U32(a[0], O_FVF));
    return 0;
}

/* ---- the device ---- */
static rx_addr new_texture(uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool) {
    uint32_t full = 1; for (uint32_t x = w > h ? w : h; x > 1; x >>= 1) full++;
    uint32_t n = levels == 0 || levels > full ? full : levels;
    if (usage & 0x400) n = levels == 0 ? full : levels;   /* AUTOGENMIPMAP */
    rx_addr t = new_object(RX_D3D_VT_TEXTURE, RX_D3D_TEXTURE);
    SU32(t, O_W, w); SU32(t, O_H, h); SU32(t, O_LEVELS, n); SU32(t, O_FMT, fmt); SU32(t, O_USAGE, usage); SU32(t, O_POOL, pool);
    SU32(t, O_LTAB, new_levels(n, fmt, w, h));
    return t;
}
static rx_addr new_cube(uint32_t edge, uint32_t levels, uint32_t usage, uint32_t fmt, uint32_t pool) {
    uint32_t full = 1; for (uint32_t x = edge; x > 1; x >>= 1) full++;
    uint32_t n = levels == 0 || levels > full ? full : levels;
    rx_addr t = new_object(RX_D3D_VT_CUBE, RX_D3D_CUBE);
    SU32(t, O_W, edge); SU32(t, O_H, edge); SU32(t, O_LEVELS, n); SU32(t, O_FMT, fmt); SU32(t, O_USAGE, usage); SU32(t, O_POOL, pool);
    SU32(t, O_SIZE, 6 * n);
    rx_addr tab = rx_heap_alloc(L_SIZE * 6 * n, 16);
    for (uint32_t f = 0; f < 6; f++) {
        rx_addr one = new_levels(n, fmt, edge, edge);                       /* a face's levels (own bits); the table entries are copied */
        for (uint32_t k = 0; k < L_SIZE * n; k += 4) SU32(tab, L_SIZE * n * f + k, U32(one, k));
    }
    SU32(t, O_LTAB, tab);
    return t;
}
static rx_addr new_surface(uint32_t w, uint32_t h, uint32_t fmt, uint32_t usage, uint32_t pool) {
    rx_addr s = new_object(RX_D3D_VT_SURFACE, RX_D3D_SURFACE);
    SU32(s, O_W, w); SU32(s, O_H, h); SU32(s, O_LEVELS, 1); SU32(s, O_FMT, fmt); SU32(s, O_USAGE, usage); SU32(s, O_POOL, pool);
    SU32(s, O_LTAB, new_levels(1, fmt, w, h));
    return s;
}
static uint32_t d_create_texture(const uint32_t* a) { SU32(a[7], 0, new_texture(a[1], a[2], a[3], a[4], a[5], a[6])); return 0; }
static uint32_t d_create_cube(const uint32_t* a) { SU32(a[6], 0, new_cube(a[1], a[2], a[3], a[4], a[5])); return 0; }   /* (dev, edge, levels, usage, fmt, pool, out, handle) */
static uint32_t cu_get_level_desc(const uint32_t* a) { if (a[1] >= U32(a[0], O_LEVELS)) return D3DERR_NOTAVAILABLE; surface_desc(a[0], a[1], a[2]); return 0; }
static uint32_t cu_get_surface(const uint32_t* a) {   /* GetCubeMapSurface(face, level, &surface) */
    if (a[1] >= 6 || a[2] >= U32(a[0], O_LEVELS)) return D3DERR_NOTAVAILABLE;
    rx_addr s = surface_of(a[0], a[1] * U32(a[0], O_LEVELS) + a[2]);
    SU32(a[3], 0, s); return s ? 0 : D3DERR_NOTAVAILABLE;
}
static uint32_t cu_lock_rect(const uint32_t* a) { return lock_rect(a[0], a[1] * U32(a[0], O_LEVELS) + a[2], a[3], a[4], a[5]); }   /* LockRect(face, level, &locked, rect, flags) */
static uint32_t d_create_buffer(const uint32_t* a, int vb) {
    rx_addr b = new_object(vb ? RX_D3D_VT_VB : RX_D3D_VT_IB, vb ? RX_D3D_VB : RX_D3D_IB);
    SU32(b, O_SIZE, a[1]); SU32(b, O_USAGE, a[2]); SU32(b, O_FVF, a[3]); SU32(b, O_POOL, a[4]);
    SU32(b, O_DATA, rx_heap_alloc(a[1] > 16 ? a[1] : 16, 16));
    SU32(a[5], 0, b);
    return 0;
}
static uint32_t d_create_vb(const uint32_t* a) { return d_create_buffer(a, 1); }
static uint32_t d_create_ib(const uint32_t* a) { return d_create_buffer(a, 0); }
static uint32_t d_create_rt(const uint32_t* a) { SU32(a[7], 0, new_surface(a[1], a[2], a[3], 1, 0)); return 0; }
static uint32_t d_create_ds(const uint32_t* a) { SU32(a[7], 0, new_surface(a[1], a[2], a[3], 2, 0)); return 0; }
static uint32_t d_create_offscreen(const uint32_t* a) { SU32(a[5], 0, new_surface(a[1], a[2], a[3], 0, a[4])); return 0; }
static uint32_t d_set_rt(const uint32_t* a) { if (a[1] < 4) SU32(a[0], D_RT + 4 * a[1], a[2]); return 0; }
static uint32_t d_get_rt(const uint32_t* a) {
    rx_addr rt = a[1] < 4 ? U32(a[0], D_RT + 4 * a[1]) : 0;
    if (!rt && a[1] == 0) rt = U32(a[0], D_BACK);
    SU32(a[2], 0, rt);
    if (rt) SU32(rt, O_REFS, U32(rt, O_REFS) + 1);
    return rt ? 0 : D3DERR_NOTAVAILABLE;
}
static uint32_t d_set_ds(const uint32_t* a) { SU32(a[0], D_DS, a[1]); return 0; }
static uint32_t d_get_ds(const uint32_t* a) {
    rx_addr d = U32(a[0], D_DS) ? U32(a[0], D_DS) : U32(a[0], D_DEPTH);
    SU32(a[1], 0, d);
    if (d) SU32(d, O_REFS, U32(d, O_REFS) + 1);
    return d ? 0 : D3DERR_NOTAVAILABLE;
}
/* Clear(count, rects, flags, color, z, stencil): the target bit with no rects fills render target 0: R32F with red / 255,
 * A8R8G8B8 / X8R8G8B8 with the color (the CPU-visible results the game reads back); other formats and z / stencil: nothing */
static uint32_t d_clear(const uint32_t* a) {
    rx_addr rt = U32(a[0], D_RT);
    if ((a[3] & 1) && a[1] == 0 && rt) {
        rx_addr e = level_entry(rt, 0), r = owner_of(rt);
        if (e) {
            uint32_t fmt = U32(r, O_FMT), v; int fill = 1;
            if (fmt == RX_D3DFMT_R32F) v = rx_fb((float)((a[4] >> 16) & 0xff) / 255.0f);
            else if (fmt == RX_D3DFMT_A8R8G8B8 || fmt == RX_D3DFMT_X8R8G8B8) v = a[4];
            else fill = 0;
            if (fill) { track_invalidate_all(); for (uint32_t k = 0; k < U32(e, L_BYTES); k += 4) SU32(U32(e, L_BITS), k, v); }
        }
    }
    return 0;
}
/* GetRenderTargetData(render target, system memory surface): the bytes, when format and pitch agree */
static uint32_t d_get_rt_data(const uint32_t* a) {
    rx_addr se = level_entry(a[1], 0), de = level_entry(a[2], 0);
    if (se && de && U32(owner_of(a[1]), O_FMT) == U32(owner_of(a[2]), O_FMT) && U32(se, L_PITCH) == U32(de, L_PITCH)) {
        uint32_t n = U32(se, L_BYTES) < U32(de, L_BYTES) ? U32(se, L_BYTES) : U32(de, L_BYTES);
#ifndef RX_ORACLE
        /* the same bytes as the word loop below (whole words: n rounded up to 4), in one copy: the terrain's deformation readback
         * (HFTerrainSurface::vf14 every frame) moved its grid word by word through the accessors (~15 % of the simulation thread) */
        const rx_addr sb = U32(se, L_BITS), db = U32(de, L_BITS);
        if (n && sb != db) {
            d3d_track* t = track_find(db, 0);   /* announced by the deformation pass: copy only what it wrote since the last copy */
            const uint32_t pitch = U32(se, L_PITCH), w = U32(de, L_W), h = U32(de, L_H);
            const int part = t && t->valid && t->epoch == g_track_epoch && U32(se, L_W) == w && U32(se, L_H) == h && 4u * w <= pitch && (uint64_t)pitch * h <= n;
            if (part) {
                int32_t x0 = t->cx0, z0 = t->cz0, x1 = t->cx1, z1 = t->cz1;
                if (x0 < 0) x0 = 0;
                if (z0 < 0) z0 = 0;
                if (x1 > (int32_t)w) x1 = (int32_t)w;
                if (z1 > (int32_t)h) z1 = (int32_t)h;
                if (x1 > x0 && z1 > z0) {
                    for (int32_t z = z0; z < z1; z++)
                        memmove(rx_base + db + (uint32_t)z * pitch + 4u * (uint32_t)x0, rx_base + sb + (uint32_t)z * pitch + 4u * (uint32_t)x0, 4u * (uint32_t)(x1 - x0));
                    t->bytes_part += 4ull * (uint64_t)(x1 - x0) * (uint64_t)(z1 - z0);
                    if (t->exp_valid) rect_union(&t->ex0, &t->ez0, &t->ex1, &t->ez1, x0, z0, x1, z1);
                }
                t->part++;
                if (g_track_verify < 0) { const char* v = getenv("RX_D3D_VERIFY"); g_track_verify = v && *v && *v != '0'; }
                if (g_track_verify && memcmp(rx_base + db, rx_base + sb, (n + 3u) & ~3u) != 0) {
                    fprintf(stderr, "rxcore: rx_d3d: GetRenderTargetData dirty-rectangle copy differs from the full copy (rect %d,%d - %d,%d)\n", x0, z0, x1, z1);
                    abort();
                }
            } else {
                memmove(rx_base + db, rx_base + sb, (n + 3u) & ~3u);
                if (t) { t->full++; t->exp_valid = 0; }   /* the host export must look at everything once */
            }
            if (t) { t->valid = 1; t->epoch = g_track_epoch; t->cx0 = t->cz0 = t->cx1 = t->cz1 = 0; }
        }
#else
        for (uint32_t k = 0; k < n; k += 4) SU32(U32(de, L_BITS), k, U32(U32(se, L_BITS), k));
#endif
    }
    return 0;
}

/* CreateVertexDeclaration(elements, out): the elements up to and including D3DDECL_END (stream 0xff), at most 64 */
static uint32_t d_create_decl(const uint32_t* a) {
    uint32_t n = 0;
    while (n < 64 && U16(a[1], 8 * n) != 0xff) n++;
    n++;
    rx_addr d = new_object(RX_D3D_VT_DECL, RX_D3D_DECL), e = rx_heap_alloc(8 * n, 16);
    for (uint32_t k = 0; k < 8 * n; k += 4) SU32(e, k, U32(a[1], k));
    SU32(d, O_SIZE, n); SU32(d, O_DATA, e);
    SU32(a[2], 0, d);
    return 0;
}
/* GetDeclaration(elements, &count): the elements (when the buffer is given) and their count */
static uint32_t c_get_decl(const uint32_t* a) {
    uint32_t n = U32(a[0], O_SIZE);
    if (a[1]) for (uint32_t k = 0; k < 8 * n; k += 4) SU32(a[1], k, U32(U32(a[0], O_DATA), k));
    SU32(a[2], 0, n);
    return 0;
}


/* ---- vertex / pixel shaders and the D3DX constant table ---- */
/* the token stream's length in words (boot_devices.inc shader_bytes): to and including the end token 0xffff, comment blocks
 * (0xfffe, length in bits 16..30) and the operands of SM2+ instructions (count in bits 24..27) skipped by their lengths */
static uint32_t shader_words(rx_addr p) {
    uint32_t n = 1;
    int sm2 = ((U32(p, 0) >> 8) & 0xff) >= 2;
    for (;;) {
        uint32_t t = U32(p, 4 * n);
        if (t == 0x0000ffffu) { n++; break; }
        if (n > (1u << 20)) { fprintf(stderr, "rxcore: rx_d3d: shader without end token\n"); abort(); }
        n += (t & 0xffff) == 0xfffe ? 1 + ((t >> 16) & 0x7fff) : sm2 ? 1 + ((t >> 24) & 15) : 1;
    }
    return n;
}
static uint32_t d_create_shader(const uint32_t* a, rx_addr vt, uint32_t kind) {
    uint32_t n = shader_words(a[1]);
    rx_addr s = new_object(vt, kind), copy = rx_heap_alloc(4 * n, 16);
    for (uint32_t k = 0; k < n; k++) SU32(copy, 4 * k, U32(a[1], 4 * k));
    SU32(s, O_SIZE, 4 * n); SU32(s, O_DATA, copy);
    SU32(a[2], 0, s);
    return 0;
}
static uint32_t d_create_vs(const uint32_t* a) { return d_create_shader(a, RX_D3D_VT_VS, RX_D3D_VS); }
static uint32_t d_create_ps(const uint32_t* a) { return d_create_shader(a, RX_D3D_VT_PS, RX_D3D_PS); }
/* GetFunction(data, &size): the stream (when data is given) and its byte length */
static uint32_t sh_get_function(const uint32_t* a) {
    uint32_t n = U32(a[0], O_SIZE);
    if (a[1]) for (uint32_t k = 0; k < n; k += 4) SU32(a[1], k, U32(U32(a[0], O_DATA), k));
    SU32(a[2], 0, n);
    return 0;
}

#define E_NOINTERFACE_ 0x80004002u
static uint32_t ct_query(const uint32_t* a) { (void)a; return E_NOINTERFACE_; }
static uint32_t ct_buffer_pointer(const uint32_t* a) { return U32(a[0], O_DATA); }
static uint32_t ct_buffer_size(const uint32_t* a) { return U32(a[0], O_SIZE); }
/* the blob: Size, Creator, Version, Constants, ConstantInfo, Flags, Target; D3DXSHADER_CONSTANTINFO (20 bytes): Name, RegisterSet,
 * RegisterIndex, RegisterCount, Reserved (u16 each), TypeInfo, DefaultValue; D3DXSHADER_TYPEINFO: Class, Type, Rows, Columns, Elements,
 * StructMembers (u16), StructMemberInfo */
static uint32_t ct_rd32(rx_addr t, uint32_t off) { return off + 4 <= U32(t, O_SIZE) ? U32(U32(t, O_DATA), off) : 0; }
static uint32_t ct_rd16(rx_addr t, uint32_t off) { return off + 2 <= U32(t, O_SIZE) ? U16(U32(t, O_DATA), off) : 0; }
static const char* ct_name(rx_addr t, uint32_t off, char* out, size_t n) {
    size_t i = 0;
    for (; i + 1 < n && off + i < U32(t, O_SIZE) && U8(U32(t, O_DATA), off + i); i++) out[i] = (char)U8(U32(t, O_DATA), off + i);
    out[i] = 0;
    return out;
}
static int64_t ct_index_of_name(rx_addr t, const char* name) {
    uint32_t count = ct_rd32(t, 12), info = ct_rd32(t, 16);
    for (uint32_t i = 0; i < count; i++) {
        char nm[128];
        if (!strcmp(ct_name(t, ct_rd32(t, info + 20 * i), nm, sizeof nm), name)) return i;
    }
    return -1;
}
static uint32_t ct_get_desc(const uint32_t* a) {   /* D3DXCONSTANTTABLE_DESC {Creator, Version, Constants} */
    rx_addr t = a[0];
    SU32(a[1], 0, U32(t, O_DATA) + ct_rd32(t, 4)); SU32(a[1], 4, ct_rd32(t, 8)); SU32(a[1], 8, ct_rd32(t, 12));
    return 0;
}
#define CT_HANDLE(i) (0x40000000u + (i))
static uint32_t ct_get_constant(const uint32_t* a) {   /* (parent, index) */
    if (a[1]) { fprintf(stderr, "rxcore: rx_d3d: ID3DXConstantTable::GetConstant with a parent\n"); abort(); }
    return a[2] < ct_rd32(a[0], 12) ? CT_HANDLE(a[2]) : 0;
}
static uint32_t ct_get_constant_by_name(const uint32_t* a) {   /* (parent, name) */
    if (a[1]) { fprintf(stderr, "rxcore: rx_d3d: ID3DXConstantTable::GetConstantByName with a parent\n"); abort(); }
    char nm[128]; size_t i = 0;
    for (; i + 1 < sizeof nm && U8(a[2], i); i++) nm[i] = (char)U8(a[2], i);
    nm[i] = 0;
    int64_t k = ct_index_of_name(a[0], nm);
    return k < 0 ? 0 : CT_HANDLE((uint32_t)k);
}
static int ct_handle_index(rx_addr t, uint32_t h, uint32_t* idx) {
    if (h >= 0x40000000u && h < 0x40000000u + ct_rd32(t, 12)) { *idx = h - 0x40000000u; return 1; }
    char nm[128]; size_t i = 0;
    for (; i + 1 < sizeof nm && U8(h, i); i++) nm[i] = (char)U8(h, i);
    nm[i] = 0;
    int64_t k = ct_index_of_name(t, nm);
    if (k < 0) return 0;
    *idx = (uint32_t)k;
    return 1;
}
static uint32_t ct_get_sampler_index(const uint32_t* a) {
    uint32_t i;
    if (!ct_handle_index(a[0], a[1], &i)) return 0xffffffffu;
    uint32_t e = ct_rd32(a[0], 16) + 20 * i;
    return ct_rd16(a[0], e + 4) == 3 ? ct_rd16(a[0], e + 6) : 0xffffffffu;
}
static uint32_t ct_get_constant_desc(const uint32_t* a) {   /* (handle, D3DXCONSTANT_DESC* out, UINT* count) */
    rx_addr t = a[0]; uint32_t i;
    if (!ct_handle_index(t, a[1], &i)) return 0x8876086cu;
    uint32_t e = ct_rd32(t, 16) + 20 * i, type = ct_rd32(t, e + 12);
    uint32_t elems = ct_rd16(t, type + 8), rows = ct_rd16(t, type + 4), cols = ct_rd16(t, type + 6), dv = ct_rd32(t, e + 16);
    uint32_t v[12] = {U32(t, O_DATA) + ct_rd32(t, e), ct_rd16(t, e + 4), ct_rd16(t, e + 6), ct_rd16(t, e + 8), ct_rd16(t, type),
                      ct_rd16(t, type + 2), rows, cols, elems, ct_rd16(t, type + 10), 4u * rows * cols * (elems ? elems : 1u),
                      dv ? U32(t, O_DATA) + dv : 0};
    if (a[2]) for (int k = 0; k < 12; k++) SU32(a[2], 4u * (uint32_t)k, v[k]);
    if (a[3]) SU32(a[3], 0, 1);
    return 0;
}

uint32_t rx_d3dx_get_shader_constant_table(rx_addr fn, rx_addr out) {
    rx_d3d_init();
    for (rx_addr p = fn + 4, k = 0; k < 100000; k++) {
        uint32_t t = U32(p, 0);
        if (t == 0x0000ffffu) break;
        if ((t & 0xffff) == 0xfffe) {
            uint32_t len = (t >> 16) & 0x7fff;
            if (U32(p, 4) == 0x42415443u) {
                rx_addr blob = rx_heap_alloc(4 * (len - 1) > 16 ? 4 * (len - 1) : 16, 16), o = new_object(RX_D3D_VT_CTAB, RX_D3D_CTAB);
                for (uint32_t w = 0; w + 1 < len; w++) SU32(blob, 4 * w, U32(p, 8 + 4 * w));
                SU32(o, O_SIZE, 4 * (len - 1)); SU32(o, O_DATA, blob);
                SU32(out, 0, o);
                return 0;
            }
            p += 4 * (len + 1);
            continue;
        }
        p += 4;
    }
    SU32(out, 0, 0);
    return 0x8876086cu;
}

/* registration: one method id per (interface, slot) */
static uint32_t m_unsupported(const uint32_t* a) { (void)a; fatal("method", 0); return 0; }
#define REG(iface, slot, fn) rx_fn_register(RX_D3D_FN(iface, slot), (void*)(fn), #fn)
static void vtable(rx_addr vt, int iface, int nslots) { for (int s = 0; s < nslots; s++) SU32(vt, 4 * (uint32_t)s, RX_D3D_FN(iface, s)); }

static const char* const DEVICE_NAMES[119] = {
    "QueryInterface", "AddRef", "Release", "TestCooperativeLevel", "GetAvailableTextureMem", "EvictManagedResources", "GetDirect3D",
    "GetDeviceCaps", "GetDisplayMode", "GetCreationParameters", "SetCursorProperties", "SetCursorPosition", "ShowCursor",
    "CreateAdditionalSwapChain", "GetSwapChain", "GetNumberOfSwapChains", "Reset", "Present", "GetBackBuffer", "GetRasterStatus",
    "SetDialogBoxMode", "SetGammaRamp", "GetGammaRamp", "CreateTexture", "CreateVolumeTexture", "CreateCubeTexture",
    "CreateVertexBuffer", "CreateIndexBuffer", "CreateRenderTarget", "CreateDepthStencilSurface", "UpdateSurface", "UpdateTexture",
    "GetRenderTargetData", "GetFrontBufferData", "StretchRect", "ColorFill", "CreateOffscreenPlainSurface", "SetRenderTarget",
    "GetRenderTarget", "SetDepthStencilSurface", "GetDepthStencilSurface", "BeginScene", "EndScene", "Clear", "SetTransform",
    "GetTransform", "MultiplyTransform", "SetViewport", "GetViewport", "SetMaterial", "GetMaterial", "SetLight", "GetLight",
    "LightEnable", "GetLightEnable", "SetClipPlane", "GetClipPlane", "SetRenderState", "GetRenderState", "CreateStateBlock",
    "BeginStateBlock", "EndStateBlock", "SetClipStatus", "GetClipStatus", "GetTexture", "SetTexture", "GetTextureStageState",
    "SetTextureStageState", "GetSamplerState", "SetSamplerState", "ValidateDevice", "SetPaletteEntries", "GetPaletteEntries",
    "SetCurrentTexturePalette", "GetCurrentTexturePalette", "SetScissorRect", "GetScissorRect", "SetSoftwareVertexProcessing",
    "GetSoftwareVertexProcessing", "SetNPatchMode", "GetNPatchMode", "DrawPrimitive", "DrawIndexedPrimitive", "DrawPrimitiveUP",
    "DrawIndexedPrimitiveUP", "ProcessVertices", "CreateVertexDeclaration", "SetVertexDeclaration", "GetVertexDeclaration", "SetFVF",
    "GetFVF", "CreateVertexShader", "SetVertexShader", "GetVertexShader", "SetVertexShaderConstantF", "GetVertexShaderConstantF",
    "SetVertexShaderConstantI", "GetVertexShaderConstantI", "SetVertexShaderConstantB", "GetVertexShaderConstantB",
    "SetStreamSource", "GetStreamSource", "SetStreamSourceFreq", "GetStreamSourceFreq", "SetIndices", "GetIndices",
    "CreatePixelShader", "SetPixelShader", "GetPixelShader", "SetPixelShaderConstantF", "GetPixelShaderConstantF",
    "SetPixelShaderConstantI", "GetPixelShaderConstantI", "SetPixelShaderConstantB", "GetPixelShaderConstantB", "DrawRectPatch",
    "DrawTriPatch", "DeletePatch", "CreateQuery"};

static int g_init;
void rx_d3d_init(void) {
    if (g_init) return;
    g_init = 1;
    /* common IUnknown + resource slots for the four resource interfaces */
    for (int i = IF_TEXTURE; i <= IF_IB; i++) {
        REG(i, 0, m_query); REG(i, 1, m_addref); REG(i, 2, m_release); REG(i, 3, m_get_device);
        for (int s = 4; s <= 9; s++) REG(i, s, m_ok);   /* private data / priority / PreLoad */
        REG(i, 10, m_get_type);
    }
    REG(IF_TEXTURE, 11, m_ok); REG(IF_TEXTURE, 12, m_ok); REG(IF_TEXTURE, 13, t_level_count); REG(IF_TEXTURE, 14, m_ok);
    REG(IF_TEXTURE, 15, t_autogen); REG(IF_TEXTURE, 16, m_ok); REG(IF_TEXTURE, 17, t_get_level_desc); REG(IF_TEXTURE, 18, t_get_surface_level);
    REG(IF_TEXTURE, 19, t_lock_rect); REG(IF_TEXTURE, 20, m_ok); REG(IF_TEXTURE, 21, m_ok);
    REG(IF_SURFACE, 11, s_get_container); REG(IF_SURFACE, 12, s_get_desc); REG(IF_SURFACE, 13, s_lock_rect); REG(IF_SURFACE, 14, m_ok);
    REG(IF_SURFACE, 15, m_unsupported); REG(IF_SURFACE, 16, m_unsupported);
    for (int i = IF_VB; i <= IF_IB; i++) { REG(i, 11, b_lock); REG(i, 12, m_ok); REG(i, 13, b_get_desc); }
    REG(IF_DECL, 0, m_query); REG(IF_DECL, 1, m_addref); REG(IF_DECL, 2, m_release); REG(IF_DECL, 3, m_get_device); REG(IF_DECL, 4, c_get_decl);
    /* the device: implemented slots, Set* / Begin* / End* / Draw* accepted, the rest unsupported */
    d3d_method dev[119];
    for (int k = 0; k < 119; k++) {
        const char* n = DEVICE_NAMES[k];
        dev[k] = (!strncmp(n, "Set", 3) || !strncmp(n, "Begin", 5) || !strncmp(n, "End", 3) || !strncmp(n, "Draw", 4) ||
                  !strcmp(n, "TestCooperativeLevel") || !strcmp(n, "EvictManagedResources") || !strcmp(n, "LightEnable") ||
                  !strcmp(n, "ValidateDevice") || !strcmp(n, "MultiplyTransform"))
                     ? m_ok : m_unsupported;
    }
    dev[0] = m_query; dev[1] = m_addref; dev[2] = m_release;
    dev[23] = d_create_texture; dev[25] = d_create_cube; dev[26] = d_create_vb; dev[27] = d_create_ib; dev[28] = d_create_rt; dev[29] = d_create_ds;
    dev[32] = d_get_rt_data; dev[36] = d_create_offscreen; dev[37] = d_set_rt; dev[38] = d_get_rt; dev[39] = d_set_ds;
    dev[40] = d_get_ds; dev[43] = d_clear; dev[86] = d_create_decl; dev[91] = d_create_vs; dev[106] = d_create_ps;
    for (int k = 0; k < 119; k++) rx_fn_register(RX_D3D_FN(IF_DEVICE, k), (void*)dev[k], DEVICE_NAMES[k]);
    vtable(RX_D3D_VT_DEVICE, IF_DEVICE, 119);
    vtable(RX_D3D_VT_TEXTURE, IF_TEXTURE, 22);
    vtable(RX_D3D_VT_SURFACE, IF_SURFACE, 17);
    vtable(RX_D3D_VT_VB, IF_VB, 14);
    vtable(RX_D3D_VT_IB, IF_IB, 14);
    vtable(RX_D3D_VT_DECL, IF_DECL, 5);
    for (int s = 0; s <= 10; s++) REG(IF_CUBE, s, s == 0 ? (void*)m_query : s == 1 ? (void*)m_addref : s == 2 ? (void*)m_release : s == 3 ? (void*)m_get_device : s == 10 ? (void*)m_get_type : (void*)m_ok);
    REG(IF_CUBE, 11, m_ok); REG(IF_CUBE, 12, m_ok); REG(IF_CUBE, 13, t_level_count); REG(IF_CUBE, 14, m_ok); REG(IF_CUBE, 15, t_autogen); REG(IF_CUBE, 16, m_ok);
    REG(IF_CUBE, 17, cu_get_level_desc); REG(IF_CUBE, 18, cu_get_surface); REG(IF_CUBE, 19, cu_lock_rect); REG(IF_CUBE, 20, m_ok); REG(IF_CUBE, 21, m_ok);
    vtable(RX_D3D_VT_CUBE, IF_CUBE, 22);
    for (int i = IF_VS; i <= IF_PS; i++) {
        REG(i, 0, m_query); REG(i, 1, m_addref); REG(i, 2, m_release); REG(i, 3, m_get_device); REG(i, 4, sh_get_function);
    }
    vtable(RX_D3D_VT_VS, IF_VS, 5);
    vtable(RX_D3D_VT_PS, IF_PS, 5);
    REG(IF_CT, 0, ct_query); REG(IF_CT, 1, m_addref); REG(IF_CT, 2, m_release); REG(IF_CT, 3, ct_buffer_pointer); REG(IF_CT, 4, ct_buffer_size);
    REG(IF_CT, 5, ct_get_desc); REG(IF_CT, 6, ct_get_constant_desc); REG(IF_CT, 7, ct_get_sampler_index); REG(IF_CT, 8, ct_get_constant);
    REG(IF_CT, 9, ct_get_constant_by_name);
    for (int s = 10; s < 27; s++) REG(IF_CT, s, m_ok);   /* GetConstantElement and the Set* methods: accepted (the boot's table drops them) */
    vtable(RX_D3D_VT_CTAB, IF_CT, 27);
}

rx_addr rx_d3d_device_create(void) {
    rx_d3d_init();
    rx_addr d = new_object(RX_D3D_VT_DEVICE, RX_D3D_DEVICE);
    g_device = d;
    return d;
}

rx_addr rx_d3d_bits(rx_addr obj, uint32_t level, uint32_t* pitch) {
    uint32_t k = U32(obj, O_KIND);
    if (k == RX_D3D_VB || k == RX_D3D_IB) { if (pitch) *pitch = U32(obj, O_SIZE); return U32(obj, O_DATA); }
    if (k != RX_D3D_TEXTURE && k != RX_D3D_SURFACE && k != RX_D3D_CUBE) return 0;
    rx_addr e = level_entry(obj, level);
    if (!e) return 0;
    if (pitch) *pitch = U32(e, L_PITCH);
    return U32(e, L_BITS);
}
#else
void rx_d3d_init(void) {}
rx_addr rx_d3d_device_create(void) { return 0; }
rx_addr rx_d3d_bits(rx_addr obj, uint32_t level, uint32_t* pitch) { (void)obj; (void)level; if (pitch) *pitch = 0; return 0; }
void rx_d3d_track_note(rx_addr dst_obj, int32_t x0, int32_t z0, int32_t x1, int32_t z1) { (void)dst_obj; (void)x0; (void)z0; (void)x1; (void)z1; }
void rx_d3d_track_stats(uint64_t* part, uint64_t* full, uint64_t* bytes_part, uint32_t* epoch) { *part = *full = *bytes_part = 0; *epoch = 0; }
int rx_d3d_track_take(rx_addr bits, int32_t* rect) { (void)bits; (void)rect; return 0; }
uint32_t rx_d3dx_get_shader_constant_table(rx_addr fn, rx_addr out) {   /* the original's D3DX import (0x008fafc2, stdcall) */
    uint32_t a[2] = {fn, out};
    return rx_call_original(0x008fafc2, RX_STDCALL, 0, 2, a);
}
#endif

uint32_t rx_d3d_create_texture(rx_addr dev, uint32_t w, uint32_t h, uint32_t levels, uint32_t usage, uint32_t format, uint32_t pool, rx_addr out) {
    uint32_t a[9] = {dev, w, h, levels, usage, format, pool, out, 0};
    return rx_d3d_call(dev, 23, 9, a);
}
uint32_t rx_d3d_create_offscreen(rx_addr dev, uint32_t w, uint32_t h, uint32_t format, uint32_t pool, rx_addr out) {
    uint32_t a[7] = {dev, w, h, format, pool, out, 0};
    return rx_d3d_call(dev, 36, 7, a);
}
uint32_t rx_d3d_create_vb(rx_addr dev, uint32_t length, uint32_t usage, uint32_t fvf, uint32_t pool, rx_addr out) {
    uint32_t a[7] = {dev, length, usage, fvf, pool, out, 0};
    return rx_d3d_call(dev, 26, 7, a);
}
uint32_t rx_d3d_create_ib(rx_addr dev, uint32_t length, uint32_t usage, uint32_t format, uint32_t pool, rx_addr out) {
    uint32_t a[7] = {dev, length, usage, format, pool, out, 0};
    return rx_d3d_call(dev, 27, 7, a);
}
