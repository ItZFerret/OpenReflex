/* build_terrain_asset.c -- see build_terrain_asset.h. Hand transliteration of TerrainAssetIO::vf3 006d8900 + FUN_006d9a90 and
 * TerrainCollisionAssetIO::vf3 006d8bd0 + FUN_006d8b10 / FUN_0041cfb0 / FUN_0041d090 from the disassembly. */
#include "build_terrain_asset.h"
#include "build_alloc.h"
#include "build_render.h"
#include "../src/rx_d3d.h"
#include <stdio.h>
#include <stdlib.h>

/* x87 fld dword / fstp dword: the bits, a signalling NaN quieted */
static uint32_t x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static void cs_init(rx_addr cs) { SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0); }

/* IDirect3DIndexBuffer9 / VertexBuffer9: Lock(0, 0, &p, 0) -> the pointer; Unlock */
static rx_addr d3d_lock(rx_addr res, rx_addr tmp) {
    uint32_t a[5] = {res, 0, 0, tmp, 0};
    rx_d3d_call(res, 11, 5, a);
    return U32(tmp, 0);
}
static void d3d_unlock(rx_addr res) { uint32_t a[1] = {res}; rx_d3d_call(res, 12, 1, a); }

/* FUN_006d9a90 (ESI = asset, stack: stream) */
static void terrain_load(rx_addr a, rx_stream* s) {
    rx_addr dev = rx_renderer_device();
    RX_FRAME_ENTER(H, 0x1a0);                 /* the header (0x194 bytes) and one local */
    rx_stream_read(s, H, 0x194);
    #define HW(k) U32(H, 4 * (k))
    uint32_t dc = HW(3), cols = HW(4), rows = HW(5);
    SU32(a, 0xdc, dc); SU32(a, 0x18, rows); SU32(a, 0x14, cols); SU32(a, 0x10, rows * cols);
    SU32(a, 0x30, HW(6)); SU32(a, 0x34, HW(7)); SU32(a, 0x38, HW(8)); SU32(a, 0x40, HW(9)); SU32(a, 0x44, HW(10)); SU32(a, 0x48, HW(11));
    SF32(a, 0x50, rx_divs(rx_muls(F32(a, 0x40), 2.0f /* 0x92e330 */), (float)I32(a, 0x14)));
    SF32(a, 0x54, rx_divs(rx_muls(F32(a, 0x48), 2.0f), (float)I32(a, 0x18)));
    uint32_t nl = HW(12);
    SU32(a, 0x58, nl);
    uint32_t sum = 0;
    for (int32_t j = 0; j < (int32_t)nl; j++) {
        SU32(a, 0x5c + 8 * (uint32_t)j, HW(13 + 2 * (uint32_t)j));
        SU32(a, 0x60 + 8 * (uint32_t)j, HW(14 + 2 * (uint32_t)j));
        sum += HW(14 + 2 * (uint32_t)j);
    }
    SU32(a, 0xe4, rx_vertex_decl_get(H + 0xb4, U32(U32(0, RX_RENDERER), 0x14)));
    uint32_t n = U32(a, 0x10);
    uint32_t psize = dc * 0x94u + (sum + 0xc) * n * 8u;
    rx_addr P = rx_crt_aligned_malloc(psize, 16);
    SU32(a, 0xe8, P); SU32(a, 0x20, P);
    SU32(a, 0x1c, P + n * sum * 8u);
    SU32(a, 0xe0, U32(a, 0x1c) + n * 0x60u);
    uint32_t want = n * 24u, have = dc << 8;
    rx_addr scr = rx_crt_aligned_malloc(have < want ? want : have, 16);
    rx_stream_read(s, scr, n * 24u);
    rx_stream_read(s, U32(a, 0x20), n * sum * 8u);
    rx_addr p = U32(a, 0x20);
    for (uint32_t i = 0; i < n; i++) {
        rx_addr rec = U32(a, 0x1c) + 0x60 * i, sc = scr + 0x18 * i;
        SU32(rec, 0x40, x87_copy(U32(sc, 0))); SU32(rec, 0x44, U32(sc, 4)); SU32(rec, 0x48, U32(sc, 8));
        SU32(rec, 0x50, x87_copy(U32(sc, 0xc))); SU32(rec, 0x54, U32(sc, 0x10)); SU32(rec, 0x58, U32(sc, 0x14));
        for (int32_t j = 0; j < (int32_t)U32(a, 0x58); j++) {
            SU32(rec, 4 * (uint32_t)j, p);
            p += U32(a, 0x60 + 8 * (uint32_t)j) * 8;
        }
    }
    rx_stream_read(s, scr, dc << 8);
    rx_addr tmp = H + 0x198;
    for (uint32_t k = 0; k < dc; k++) {
        rx_addr rec = U32(a, 0xe0) + 0x94 * k, sc = scr + 0x100 * k;
        SU32(rec, 4, U32(sc, 0)); SU32(rec, 8, U32(sc, 8)); SU32(rec, 0xc, U32(sc, 4)); SU32(rec, 0x10, U32(sc, 0xc));
        rx_d3d_create_ib(dev, U32(rec, 8), 8, 0x66, 1, rec);            /* CreateIndexBuffer(len, usage 8, INDEX32, MANAGED) */
        rx_addr ib = U32(rec, 0);
        rx_addr dst = d3d_lock(ib, tmp);
        rx_stream_read(s, dst, U32(rec, 8));
        d3d_unlock(ib);
        for (int32_t j = 0; j < (int32_t)U32(rec, 0x10); j++) {
            SU32(rec, 0x54 + 4 * (uint32_t)j, U32(sc, 0x10 + 4 * (uint32_t)j));
            rx_d3d_create_vb(dev, U32(rec, 0x54 + 4 * (uint32_t)j), 8, 0, 1, rec + 0x14 + 4 * (uint32_t)j);
            rx_addr vb = U32(rec, 0x14 + 4 * (uint32_t)j);
            rx_addr vd = d3d_lock(vb, tmp);
            rx_stream_read(s, vd, U32(rec, 0x54 + 4 * (uint32_t)j));
            d3d_unlock(vb);
        }
    }
    rx_crt_aligned_free(scr);
    #undef HW
    RX_FRAME_LEAVE(H, 0x1a0);
}

/* TerrainAssetIO::vf3 006d8900 */
rx_addr rx_terrain_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0xf0);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x009624e4); SU32(a, 0x58, 0); SU32(a, 0xdc, 0); SU32(a, 0xe4, 0); SU32(a, 0xe8, 0); }
    terrain_load(a, s);
    return a;
}

/* FUN_0041cfb0 (ESI = S) */
static void collision_mesh_construct(rx_addr S) {
    cs_init(S);
    for (uint32_t k = 0; k < 0x400; k++) { SU32(S, 0x18 + 8 * k, 0); SU32(S, 0x1c + 8 * k, 0); }   /* FUN_008a3100 */
    SU32(S, 0x2058, 0); SU32(S, 0x205c, 0); SU32(S, 0x2020, 0); SU32(S, 0x2050, 0); SU32(S, 0x2024, 0); SU32(S, 0x202c, 0);
    SU32(S, 0x2028, 0); SU32(S, 0x2060, 0); SU32(S, 0x201c, 0); SU32(S, 0x2018, 0); SF32(S, 0x2054, 0.0f);
}

/* FUN_0041d090 (ESI = S, ECX = the stream whose first word, the magic, was read already): the binary mesh */
static void collision_mesh_load(rx_addr S, rx_stream* s, uint32_t magic) {
    uint32_t h[0x16]; h[0] = magic;
    rx_stream_read_host(s, &h[1], 0x58 - 4);
    SU32(S, 0x2054, h[7]); SU32(S, 0x2024, h[2]); SU32(S, 0x202c, h[3]); SU32(S, 0x2058, h[5]); SU32(S, 0x205c, h[6]);
    SU32(S, 0x2030, h[8]); SU32(S, 0x2034, h[9]); SU32(S, 0x2038, h[10]); SU32(S, 0x2040, h[11]); SU32(S, 0x2044, h[12]); SU32(S, 0x2048, h[13]);
    uint32_t nF = U32(S, 0x2024), nV = U32(S, 0x202c), cols = U32(S, 0x2058), rows = U32(S, 0x205c), nI = h[4];
    uint32_t words = ((rows + 1) * cols + nI) + (nF + nV * 2) * 4;
    rx_addr blk = rx_crt_aligned_malloc(words * 4, 16);
    SU32(S, 0x201c, blk);
    if (blk) for (uint32_t i = 0; i < nF; i++) SU16(blk + 16 * i, 0xc, 0);    /* the face constructor 0041cfa0 */
    SU32(S, 0x2020, blk);
    SU32(S, 0x2028, blk + nF * 16);
    SU32(S, 0x2050, U32(S, 0x2028) + nV * 32);
    rx_addr grid = U32(S, 0x2050) + nI * 4;
    SU32(S, 0x2060, grid);
    SU32(grid, 0, grid + cols * 4);
    for (uint32_t i = 1; i < cols; i++) SU32(grid, 4 * i, U32(grid, 4 * i - 4) + rows * 4);
    rx_stream_read(s, U32(S, 0x2028), nV << 5);
    for (uint32_t i = 0; i < nF; i++) {
        uint32_t f[8]; rx_stream_read_host(s, f, 0x20);
        rx_addr r = U32(S, 0x2020) + 16 * i;
        SU32(r, 0, (f[4] << 5) + U32(S, 0x2028)); SU32(r, 4, (f[5] << 5) + U32(S, 0x2028)); SU32(r, 8, (f[6] << 5) + U32(S, 0x2028));
        SU16(r, 0xc, f[7] & 0xffff);
    }
    rx_stream_read(s, U32(S, 0x2050), nI * 4);
    uint32_t cur = 0;
    for (uint32_t c = 0; c < cols; c++)
        for (uint32_t r = 0; r < rows; r++) {
            SU32(U32(grid, 4 * c), 4 * r, U32(S, 0x2050) + cur * 4);
            cur = cur + U32(U32(S, 0x2050), cur * 4) + 1;
        }
}

/* TerrainCollisionAssetIO::vf3 006d8bd0 (+ FUN_006d8b10) */
rx_addr rx_terrain_collision_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x2080);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x0096251c); collision_mesh_construct(a + 0x10); }
    uint32_t magic = rx_stream_u32(s);
    if (magic != 0xbada55u) { fprintf(stderr, "rxcore: TerrainCollisionAsset in XML form is not ported (FUN_0041d350)\n"); abort(); }
    collision_mesh_load(a + 0x10, s, magic);
    return a;
}
