/* build_water.c -- see build_water.h. Hand transliteration of WaterAssetIO::vf3 007e3ec0, FUN_007ede50, FUN_007ee0a0, XenonWaterAsset_Load 007f0320,
 * FUN_007ee5a0 / 007ee600 / 007ee690 / 007ee700 / 007ee540, WaterMesh_Build 007e4280 with 007e4570 / 007e4800 / 007e4d50, FUN_007e5330,
 * Ocean_Create 007e3b90, FUN_007eba00 / 007ebd10 / 007edb00 and FUN_004242f0 from the disassembly; the clipmap's index / vertex data follows
 * Unity/Reflex/Assets/Scripts/Content/ReflexWater.cs (BuildBuffers, byte-identical to the device buffers of a recorded frame). The x87 parts
 * (the mask's divisions, the _ftol2_sse roundings) are computed in double, the 53-bit precision of the game's threads. */
#include "build_water.h"
#include "map_scale.h"
#include "build_alloc.h"
#include "build_load.h"
#include "build_render.h"
#include "../src/rx_d3d.h"
#include "../src/rx_x86.h"
#include "crt.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define G_OCEAN 0x00a9578cu
#define G_WAKE  0x00a95790u
#define G_REFR  0x00a95794u
#define WATER_SET_GUARD 0x00d72aa8u
#define WATER_SET_WAKE  0x00d72aacu
#define WATER_SET_VEC   0x00d72ab0u
#define FILE_STR 0x009723c8u   /* the engine source path the GPU registry records */

static void fatal(const char* what) { fprintf(stderr, "rxcore: water asset: %s\n", what); abort(); }
static uint32_t x87_copy(uint32_t b) {
    if ((b & 0x7f800000u) == 0x7f800000u && (b & 0x007fffffu) && !(b & 0x00400000u)) b |= 0x00400000u;
    return b;
}
static void cs_init(rx_addr cs) { SU32(cs, 0, 0); SU32(cs, 4, 0xffffffffu); SU32(cs, 8, 0); SU32(cs, 0xc, 0); SU32(cs, 0x10, 0); SU32(cs, 0x14, 0); }
static uint32_t dcall(rx_addr obj, uint32_t slot, int n, const uint32_t* a) { return rx_d3d_call(obj, slot, n, a); }
static rx_addr lock_buf(rx_addr res, rx_addr tmp) {                       /* Lock(0, 0, &data, 0) */
    uint32_t a[5] = {res, 0, 0, tmp, 0};
    dcall(res, 11, 5, a);
    return U32(tmp, 0);
}
static void unlock_buf(rx_addr res) { uint32_t a[1] = {res}; dcall(res, 12, 1, a); }
static void create_ib(rx_addr dev, uint32_t bytes, rx_addr out) { rx_d3d_create_ib(dev, bytes, 8, 0x65, 1, out); }
static void create_vb(rx_addr dev, uint32_t bytes, rx_addr out) { rx_d3d_create_vb(dev, bytes, 8, 0, 1, out); }
static void put2(rx_addr p, uint32_t* at, uint32_t v) { SU16(p, *at, v); *at += 2; }
static void put_xy(rx_addr p, uint32_t* at, float x, float y) { SF32(p, *at, x); SF32(p, *at + 4, y); *at += 8; }
/* _ftol2_sse(copysign(0.5, v) + v), then through a float (cvtsi2ss / cvttss2si) */
static int32_t round_via_float(double v) {
    double s = copysign(0.5, v) + v;
    int32_t i = rx_cvtt(s);
    return rx_cvtt((double)(float)i);
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the clipmap mesh: WaterMesh_Build 007e4280 (EAX = n, XMM0 = cell, stack: mesh, extent x, extent z) */

static void mesh_level_init(rx_addr mesh, uint32_t i) {   /* the head of 007e4570 / 007e4800 / 007e4d50: scale 2^i * cell, centre = the zero vector */
    rx_addr rec = U32(mesh, 4) + 0x50 * i;
    SF32(rec, 0, (float)ldexp(1.0, (int)i) * F32(mesh, 0x1c));
    SU32(rec, 8, U32(0, 0x00a95d40)); SU32(rec, 0xc, U32(0, 0x00a95d44));
}

/* 007e4570 (ECX = level, EDI = mesh) */
static void mesh_inner(rx_addr mesh, uint32_t li, rx_addr dev, rx_addr tmp) {
    int32_t n = I32(mesh, 0xc), h = n / 2;
    uint32_t r = (uint32_t)h + 1;
    mesh_level_init(mesh, li);
    if (U32(mesh, 0x2c)) return;
    SU32(mesh, 0x34, r * r);
    uint32_t count = 2u * (uint32_t)h * ((uint32_t)h + 2) - 2;
    create_ib(dev, 2 * count, mesh + 0x2c);
    rx_addr p = lock_buf(U32(mesh, 0x2c), tmp); uint32_t at = 0;
    for (uint32_t row = 0; row < (uint32_t)h; row++) {
        for (uint32_t i = 0; i < r; i++) { put2(p, &at, row * r + i); put2(p, &at, (row + 1) * r + i); }
        if (row + 1 < (uint32_t)h) { put2(p, &at, U16(p, at - 2)); put2(p, &at, (row + 1) * r); }
    }
    unlock_buf(U32(mesh, 0x2c));
    SU32(mesh, 0x30, count - 2);
    create_vb(dev, U32(mesh, 0x10) * r * r, mesh + 0x54);
    p = lock_buf(U32(mesh, 0x54), tmp); at = 0;
    float s = F32(U32(mesh, 4), 0), y = -0.0f - (s * (float)h * 0.5f);
    for (uint32_t j = 0; j < r; j++) {
        float x = -0.0f - (s * (float)h * 0.5f);
        for (uint32_t i = 0; i < r; i++) { put_xy(p, &at, x, y); x = x + s; }
        y = y + s;
    }
    unlock_buf(U32(mesh, 0x54));
}

/* 007e4800 (ECX = level, EAX = mesh) */
static void mesh_rows(rx_addr mesh, uint32_t li, rx_addr dev, rx_addr tmp) {
    int32_t n = I32(mesh, 0xc), h = n / 2;
    uint32_t r1 = (uint32_t)h + 1, N = (uint32_t)n;
    mesh_level_init(mesh, li);
    if (U32(mesh, 0x38)) return;
    SU32(mesh, 0x40, r1 * N);
    uint32_t total = 6u * (uint32_t)h + 2u * ((r1 - 2) * N + r1) - 4;
    create_ib(dev, 2 * total, mesh + 0x38);
    rx_addr p = lock_buf(U32(mesh, 0x38), tmp); uint32_t at = 0;
    for (int32_t cc = h; cc > 0; cc--) {                           /* the stitch strip along the coarse edge */
        uint32_t b = r1 + 2u * (uint32_t)cc;
        if (cc == h) { put2(p, &at, b); put2(p, &at, (uint32_t)cc); put2(p, &at, b - 1); put2(p, &at, (uint32_t)cc - 1); put2(p, &at, b - 2); }
        else { put2(p, &at, b); put2(p, &at, b); put2(p, &at, (uint32_t)cc); put2(p, &at, b - 1); put2(p, &at, (uint32_t)cc - 1); put2(p, &at, b - 2); }
    }
    put2(p, &at, r1); put2(p, &at, r1); put2(p, &at, r1);
    for (uint32_t row = 1; row < (uint32_t)h; row++) {
        uint32_t a0 = r1 + (row - 1) * N, b0 = r1 + row * N;
        for (uint32_t i = 0; i < N; i++) { put2(p, &at, a0 + i); put2(p, &at, b0 + i); }
        if (row + 1 < (uint32_t)h) { put2(p, &at, b0 + N - 1); put2(p, &at, b0); }
    }
    unlock_buf(U32(mesh, 0x38));
    rx_addr tbl = rx_reflex_aligned_alloc(4 * r1);
    SU32(mesh, 0x3c, tbl);
    SU32(tbl, 4 * (uint32_t)h, total - 2);
    SU32(tbl, 0, 0);
    for (uint32_t k = 1; k < (uint32_t)h; k++) SU32(tbl, 4 * k, 6u * (uint32_t)h + 2u * ((k - 1) * N + k) - 4 + (k & 1));
    float s = F32(U32(mesh, 4), 0);
    float y0 = ((float)(h) * s) * -1.0f, xs = -0.0f - ((float)(n - 1) * s * 0.5f);
    create_vb(dev, U32(mesh, 0x10) * r1 * N, mesh + 0x58);
    rx_addr v1 = lock_buf(U32(mesh, 0x58), tmp); at = 0;
    {   float y = y0, x = xs;
        for (uint32_t i = 0; i < r1; i++) { put_xy(v1, &at, x, y); x = x + s * 2.0f; }
        y = y + s;
        for (uint32_t k = 1; k <= (uint32_t)h; k++) {
            x = xs;
            for (uint32_t i = 0; i < N; i++) { put_xy(v1, &at, x, ((k & 1) && (i == 0 || i == N - 1)) ? y + s : y); x = x + s; }
            y = y + s;
        }
    }
    create_vb(dev, U32(mesh, 0x10) * r1 * N, mesh + 0x5c);
    rx_addr v2 = lock_buf(U32(mesh, 0x5c), tmp); at = 0;
    {   uint32_t src = 0;                                           /* the top rows: each row reversed, y negated */
        for (uint32_t k = 0; k <= (uint32_t)h; k++) {
            uint32_t len = k == 0 ? r1 : N;
            for (uint32_t i = len; i-- > 0;) { SU32(v2, at, U32(v1, src + 8 * i)); SU32(v2, at + 4, U32(v1, src + 8 * i + 4) ^ 0x80000000u); at += 8; }
            src += 8 * len;
        }
    }
    unlock_buf(U32(mesh, 0x58));
    unlock_buf(U32(mesh, 0x5c));
}

/* the column strip (007e4d50): `odd` starts one column in */
static void column_strip(rx_addr p, uint32_t* at, uint32_t h, int odd) {
    uint32_t c = h + 1, c0 = h - (odd ? 1 : 0);
    put2(p, at, c + h); put2(p, at, c0); put2(p, at, c + h - 1); put2(p, at, c0 - 2); put2(p, at, c + h - 2);
    uint32_t b = c + h - 2, cc = c0 - 2;
    while (b > c) {
        uint32_t nx = (int32_t)(cc - 2) >= 0 ? cc - 2 : cc;
        put2(p, at, b); put2(p, at, b); put2(p, at, cc); put2(p, at, b - 1); put2(p, at, nx); put2(p, at, b - 2);
        b -= 2; cc -= 2;
    }
    put2(p, at, c); put2(p, at, c); put2(p, at, c);
    for (uint32_t row = 1; row < h; row++) {
        uint32_t a0 = c + (row - 1) * c, b0 = c + row * c;
        for (uint32_t i = 0; i < c; i++) { put2(p, at, a0 + i); put2(p, at, b0 + i); }
        if (row + 1 < h) { put2(p, at, b0 + c - 1); put2(p, at, b0); }
    }
}
/* 007e4d50 (ECX = level, stack: mesh; EDI = mesh) */
static void mesh_columns(rx_addr mesh, uint32_t li, rx_addr dev, rx_addr tmp) {
    int32_t n = I32(mesh, 0xc), h = n / 2;
    uint32_t c = (uint32_t)h + 1;
    mesh_level_init(mesh, li);
    if (U32(mesh, 0x44)) return;
    SU32(mesh, 0x50, c * c);
    uint32_t total = 6u * ((c - 1) / 2) + 2u * ((c - 2) * c + (c - 1)) - 2;
    create_ib(dev, 2 * total, mesh + 0x44);
    rx_addr p = lock_buf(U32(mesh, 0x44), tmp); uint32_t at = 0;
    column_strip(p, &at, (uint32_t)h, 0);
    create_ib(dev, 2 * total, mesh + 0x48);
    rx_addr p2 = lock_buf(U32(mesh, 0x48), tmp); at = 0;
    column_strip(p2, &at, (uint32_t)h, 1);
    rx_addr tbl = rx_reflex_aligned_alloc(4 * c);
    SU32(mesh, 0x4c, tbl);
    SU32(tbl, 4 * (uint32_t)h, total - 2);
    SU32(tbl, 0, 0);
    for (uint32_t k = 1; k < (uint32_t)h; k++) SU32(tbl, 4 * k, 6u * ((c - 1) / 2) + 2u * ((k - 1) * c + k) - 3);
    unlock_buf(U32(mesh, 0x48));
    unlock_buf(U32(mesh, 0x44));
    float s = F32(U32(mesh, 4), 0);
    float x0 = ((float)(c - 1) * s * 0.5f) * -2.0f /* 0x009d7b30 */, y0 = ((float)(c - 1) * s) * 0.5f;
    create_vb(dev, U32(mesh, 0x10) * c * c, mesh + 0x60);
    rx_addr v3 = lock_buf(U32(mesh, 0x60), tmp); at = 0;
    {   float x = x0;
        for (uint32_t col = 0; col < c; col++) {
            float y = y0;
            for (uint32_t j = 0; j < c; j++) { put_xy(v3, &at, x, y); y = y - s; }
            x = x + s;
        }
    }
    create_vb(dev, U32(mesh, 0x10) * c * c, mesh + 0x64);
    rx_addr v4 = lock_buf(U32(mesh, 0x64), tmp); at = 0;
    for (uint32_t col = 0; col < c; col++) {
        float y = -0.0f - ((float)(c - 1) * s * 0.5f);
        for (uint32_t j = 0; j < c; j++) { put_xy(v4, &at, -F32(v3, 8 * (col * c + j)), y); y = y + s; }
    }
    unlock_buf(U32(mesh, 0x60));
    unlock_buf(U32(mesh, 0x64));
}

/* WaterMesh_Build 007e4280 */
static rx_addr mesh_build(rx_addr mesh, int32_t n, float cell, float ex, float ez) {
    rx_addr dev = rx_renderer_device();
    SF32(mesh, 0x1c, cell); SF32(mesh, 0x20, 5.0f /* 0x00935890 */); SF32(mesh, 0x24, 0.0f);
    SU32(mesh, 0xc, (uint32_t)n);
    SU32(mesh, 0x2c, 0); SU32(mesh, 0x30, 0); SU32(mesh, 0x38, 0); SU32(mesh, 0x3c, 0); SU32(mesh, 0x44, 0); SU32(mesh, 0x48, 0); SU32(mesh, 0x4c, 0);
    SU32(mesh, 0x68, 0);
    for (uint32_t k = 0; k < 64; k += 4) SU32(mesh, 0x70 + k, U32(0, 0x009cb510 + k));      /* the identity matrix */
    SF32(mesh, 0xb0, 0.0f); SF32(mesh, 0xb4, 0.0f); SF32(mesh, 0xb8, 0.0f); SF32(mesh, 0xbc, 0.0f);
    SU32(mesh, 0xc0, 0); SU32(mesh, 0xc8, 0); SU32(mesh, 0xcc, 0); SU32(mesh, 0xd8, 1); SF32(mesh, 0xe0, 1.0f); SF32(mesh, 0xe4, 1.0f);
    SU32(mesh, 0x54, 0); SU32(mesh, 0x58, 0); SU32(mesh, 0x5c, 0); SU32(mesh, 0x60, 0); SU32(mesh, 0x64, 0);
    SF32(mesh, 0x14, ex); SF32(mesh, 0x18, ez);
    if ((n & 1) == 0) { n++; SU32(mesh, 0xc, (uint32_t)n); }
    RX_FRAME_ENTER(F, 0x60);
    /* the vertex declaration: one FLOAT2 position in stream 0 (through the declaration cache) */
    SU16(F, 0x10, 0); SU16(F, 0x12, 0); SU8(F, 0x14, 1); SU8(F, 0x15, 0); SU8(F, 0x16, 0); SU8(F, 0x17, 0);
    SU16(F, 0x18, 0xff); SU16(F, 0x1a, 0); SU8(F, 0x1c, 0x11); SU8(F, 0x1d, 0); SU8(F, 0x1e, 0); SU8(F, 0x1f, 0);
    SU32(mesh, 0, rx_vertex_decl_get(F + 0x10, U32(U32(0, RX_RENDERER), 0x14)));
    SU32(mesh, 0x10, 8);
    SU32(mesh, 8, 1);
    /* the number of levels: 64 * 2^L must cover the larger extent (rounded like _ftol2_sse(copysign(0.5, x) + x)) */
    int32_t rz = round_via_float((double)ez), rx = round_via_float((double)ex);
    int32_t ext = rx > rz ? rx : rz;
    float e = (float)(I32(mesh, 0xc) - 1) * F32(mesh, 0x1c);
    SF32(mesh, 0x28, e);
    if ((float)ext > e) {
        do { SU32(mesh, 8, U32(mesh, 8) + 1); e = F32(mesh, 0x28) * 2.0f; SF32(mesh, 0x28, e); } while ((float)ext > e);
    }
    uint32_t levels = U32(mesh, 8);
    rx_addr lv = rx_reflex_aligned_alloc(rx_mul_sat(levels, 0x50));
    for (uint32_t k = 0; k < levels; k++) {                  /* Fn_007e4240 */
        rx_addr rec = lv + 0x50 * k;
        SF32(rec, 0, 1.0f); SU32(rec, 8, U32(0, 0x00a95d40)); SU32(rec, 0xc, U32(0, 0x00a95d44)); SF32(rec, 0x10, 0.0f);
    }
    SU32(mesh, 4, lv);
    for (uint32_t k = 0; k < levels; k++) {
        mesh_inner(mesh, k, dev, F);
        mesh_rows(mesh, k, dev, F);
        mesh_columns(mesh, k, dev, F);
    }
    RX_FRAME_LEAVE(F, 0x60);
    return mesh;
}

/* FUN_007e5330 (ESI = the mesh, stack: the surface's grid descriptor {+0 cells, +4 columns, +8 rows, +0xc row stride}): the coarse mask of the
 * valid cells at mesh + 0xc0 */
static void mesh_mask(rx_addr mesh, rx_addr g) {
    double gx = (double)U32(g, 4), gz = (double)U32(g, 8);          /* fild + 2^32 for a negative count */
    float extx = F32(mesh, 0x14), extz = F32(mesh, 0x18);
    float fa = (float)((double)extx / gx), fb = (float)((double)extz / gz);
    int32_t nx = rx_cvtt(fa), nz = rx_cvtt(fb);
    if (nx <= 0x100) nx = 0x100;
    if (nz <= 0x100) nz = 0x100;
    SU32(mesh, 0xc8, (uint32_t)nx); SU32(mesh, 0xcc, (uint32_t)nz);
    int32_t d0 = round_via_float((double)(extx / (float)nx) + 0.49999999999);
    SU32(mesh, 0xd0, (uint32_t)d0);
    int32_t d4 = round_via_float((double)(extz / (float)(int32_t)U32(mesh, 0xcc)) + 0.49999999999);
    SU32(mesh, 0xd4, (uint32_t)d4);
    uint32_t size = (uint32_t)(d0 * d4);
    SU32(mesh, 0xc4, size);
    rx_addr mask = rx_reflex_aligned_alloc(size);
    SU32(mesh, 0xc0, mask);
    for (uint32_t k = 0; (int32_t)k < (int32_t)U32(mesh, 0xc4); k++) SU8(mask, k, 1);
    float fd0 = (float)d0, fd4 = (float)d4;
    float inv_gx = (float)(1.0 / gx), inv_gz = (float)(1.0 / gz);
    uint32_t cols = U32(g, 4), rows = U32(g, 8), stride = U32(g, 0xc);
    float zf = 0.0f;
    for (uint32_t z = 0; (int32_t)z < (int32_t)rows; z++) {
        int32_t zlo = round_via_float((double)((zf * inv_gz) * fd4) - 0.49999999999);
        float zf1 = zf + 1.0f;
        int32_t zhi = round_via_float((double)((zf1 * inv_gz) * fd4) + 0.49999999999);
        int32_t zmax = d4 - 1; if (zhi < zmax) zmax = zhi;
        float xf = 0.0f;
        for (uint32_t x = 0; (int32_t)x < (int32_t)cols; x++) {
            if (U8(U32(g, 0), stride * z + 4 * x + 1) != 0) {
                int32_t xlo = round_via_float((double)((xf * inv_gx) * fd0) - 0.49999999999);
                int32_t xhi = round_via_float((double)(((xf + 1.0f) * inv_gx) * fd0) + 0.49999999999);
                int32_t xmax = d0 - 1; if (xhi < xmax) xmax = xhi;
                for (int32_t iz = zlo; iz <= zmax; iz++)
                    for (int32_t ix = xlo; ix <= xmax; ix++) SU8(mask, (uint32_t)(ix + d0 * iz), 0);
            }
            xf = xf + 1.0f;
        }
        zf = zf1;
    }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the objects shared by all water surfaces */

/* Ocean_InitSpectrum 007ea9e0's draws of the CRT rand() (w-rng). The original runs it on its level loader thread when the first water
 * surface creates the shared ocean (FUN_007eb420): for every k of the N x N grid (N = +0x14000, 64) whose spectrum is not 0 -- every k
 * but (row 0, col N/2), (row N/2, col 0), (N/2, N/2), which take the vector at 0x00a95d40 (zero), k = (c - N/2, r - N/2) * (1 / N) * 2 pi
 * otherwise -- two Box-Muller numbers, each from a first draw repeated while it is 0 ((float)r * 1/32767 == 0) and a second draw: 4 draws
 * a k and the repeats (PX_Laguna's boot: 16372 draws on the loader thread before its scene entities' vf5 draws; RX_RNG_LOG). The
 * spectrum itself (h0 / w through msvcr90's SSE2 log / cos / pow / exp, which the core does not port) only the renderer reads: Unity's
 * ReflexWater.Ocean.Generate makes it in the same order from the loader stream's state before the first draw, which this records
 * (rx_session_ocean_seed). The draws' count does not depend on the order (the zero k draw nothing). */
static struct { uint32_t valid, seed, draws; } g_ocean_rng;
static void ocean_spectrum_draws(rx_addr o) {
    const int thread = crt_thread_switch(CRT_THREAD_LOADER);
    const rx_addr ptd = crt_ptd();
    g_ocean_rng.valid = 1; g_ocean_rng.seed = ptd ? U32(ptd, 0x14) : 0; g_ocean_rng.draws = 0;
    const int32_t n = I32(o, 0x14000), h = n / 2;
    const float inv = 1.0f / (float)n;
    for (int32_t r = 0; r < n; r++) {
        for (int32_t c = 0; c < n; c++) {
            float kx, ky;
            if ((r == 0 && c == h) || (c == 0 && r == h) || (c == h && r == h)) { kx = F32(0, 0x00a95d40); ky = F32(0, 0x00a95d44); }
            else { kx = (float)(c - h) * inv * 6.2831855f /* 0x97749c */; ky = (float)(r - h) * inv * 6.2831855f; }
            if (!(rx_sqrts(kx * kx + ky * ky) > 0.0f)) continue;   /* (float)pow(|k| x the wind factor (0.5 .. 1), 4) == 0 */
            for (int g = 0; g < 2; g++) {
                int32_t u;
                do { u = crt_rand(); g_ocean_rng.draws++; } while ((float)u * 3.0518509e-05f /* 0x92e054 */ == 0.0f);
                (void)crt_rand(); g_ocean_rng.draws++;
            }
        }
    }
    crt_thread_switch(thread);
}
int rx_water_ocean_seed(uint32_t* seed, uint32_t* draws) {
    if (!g_ocean_rng.valid) return 0;
    if (seed) *seed = g_ocean_rng.seed;
    if (draws) *draws = g_ocean_rng.draws;
    return 1;
}

/* the ocean (Ocean_Create 007e3b90 + the field setup of 007ee0a0 + FUN_007eb420): the block, its two 64 x 64 R32F targets; the Phillips spectrum
 * (Ocean_InitSpectrum 007ea9e0) only as its draws (above): only the renderer reads the spectrum */
static rx_addr ocean_create(void) {
    rx_addr o = rx_crt_aligned_malloc(0x14080, 0x80);
    if (!o) return 0;
    SU32(o, 0x14000, 0x40); SU32(o, 0x1400c, 0); SU32(o, 0x14010, 0); SU32(o, 0x14014, 0); SF32(o, 0x14018, 0.0f);
    SU32(o, 0x14004, 0); SU32(o, 0x14008, 0);
    SU32(0, G_OCEAN, o);
    SU32(o, 0x14010, U32(o, 0x14010) + 1);                                   /* FUN_007eb420 */
    ocean_spectrum_draws(o);                                                 /* its Ocean_InitSpectrum (w-rng) */
    rx_gpu_create_texture(0x40, 0x40, 1, 0x200, 0x72, 0, o + 0x14004, 0, 0, 0, FILE_STR, 0x24d);
    rx_gpu_create_texture(0x40, 0x40, 1, 0x200, 0x72, 0, o + 0x14008, 0, 0, 0, FILE_STR, 0x24e);
    SU32(o, 0x1400c, 0);
    return o;
}

/* FUN_007eba00 (ESI = the block): the wake's constructor */
static rx_addr wake_construct(rx_addr w) {
    SU32(w, 0x2c, 0);
    for (uint32_t k = 0; k < 2; k++) SU32(w, 0x80 + 0x188 * k + 0x180, 0);                 /* Fn_007f9500 */
    for (uint32_t k = 0; k < 2; k++) SU32(w, 0x3d0 + 0x1684 * k + 0x1680, 0);              /* Fn_007f9540 */
    cs_init(w + 0x30d8);
    SU32(w, 0x390, 0); SU32(w, 0x394, 0); SU32(w, 0x24, 0); SU32(w, 0x28, 0); SU32(w, 0x30, 0xffffffffu); SU32(w, 0x34, 0xffffffffu);
    static const uint32_t zf[] = {0x398, 0x39c, 0x3a0, 0x3a4, 0x3b0, 0x3b4, 0x3b8, 0x3c0, 0x3c4, 0x3c8};
    for (size_t i = 0; i < sizeof zf / sizeof zf[0]; i++) SF32(w, zf[i], 0.0f);
    SU32(w, 0, 0); SU32(w, 4, 0);
    return w;
}

/* the water set (FUN_004242f0 / FUN_0042b410): the std::vector of surfaces at 0x00d72ab0, made once */
static void water_set_ensure(void) {
    if (U8(0, WATER_SET_GUARD) & 1) return;
    SU8(0, WATER_SET_GUARD, U8(0, WATER_SET_GUARD) | 1);
    rx_addr px = rx_reflex_aligned_alloc(4);
    if (px) SU32(px, 0, WATER_SET_VEC);
    SU32(0, WATER_SET_VEC, px);
    rx_addr blk = rx_reflex_aligned_alloc(0x40);                                          /* 16 elements */
    SU32(0, WATER_SET_VEC + 0xc, blk); SU32(0, WATER_SET_VEC + 0x10, blk); SU32(0, WATER_SET_VEC + 0x14, blk + 0x40);
}

static void set_elem(rx_addr e, uint32_t i, uint32_t stream, uint32_t off, uint32_t type, uint32_t method, uint32_t usage, uint32_t index) {
    SU16(e, 8 * i, stream); SU16(e, 8 * i + 2, off); SU8(e, 8 * i + 4, type); SU8(e, 8 * i + 5, method); SU8(e, 8 * i + 6, usage); SU8(e, 8 * i + 7, index);
}
/* the shader entry of that name from the end of the asset's list, AddRef'ed (the inline loops of 007ebd10) */
static rx_addr shader_vs(rx_addr asset, const char* name) {
    rx_addr out = 0;
    RX_FRAME_ENTER(F, 0x10);
    rx_shader_find_vs(asset, name, F, 0);
    out = U32(F, 0);
    RX_FRAME_LEAVE(F, 0x10);
    return out;
}

/* FUN_007ebd10: the wake's render objects (R32F targets, the Water_Wake shader entries, two declarations, the cleared targets) */
static void wake_setup(void) {
    rx_addr dev = rx_renderer_device(), w = U32(0, G_WAKE);
    SU32(w, 0x2c, U32(w, 0x2c) + 1);
    static const uint32_t big_line[3] = {0x3b2, 0x3bc, 0x3c6}, small_line[2][3] = {{0x3f9, 0x403, 0x40d}, {0x3f9, 0x403, 0x40d}};
    for (uint32_t k = 0; k < 3; k++) rx_gpu_create_texture(0x200, 0x200, 1, 1, 0x72, 0, w + 4 * k, 0x007ebc60, w, w + 0x40 + 4 * k, FILE_STR, big_line[k]);
    SU32(w, 0x24, 0); SU32(w, 0x28, 0);
    for (uint32_t j = 0; j < 2; j++)
        for (uint32_t k = 0; k < 3; k++)
            rx_gpu_create_texture(0x100, 0x100, 1, 1, 0x72, 0, w + 0xc + 12 * j + 4 * k, 0x007ebc60, w, w + 0x4c + 12 * j + 4 * k, FILE_STR, small_line[j][k]);
    rx_addr th = rx_load_find_handle_c("texture", "WaterPressure"), sh = rx_load_find_handle_c("shader", "Water_Wake");
    SU32(w, 0x3c, th); SU32(w, 0x38, sh);
    if (!sh || !U32(sh, 0x44)) fatal("the Water_Wake shader asset is not loaded");
    rx_addr sa = U32(sh, 0x44);
    SU32(sh, 0x4c, 1);
    SU32(w, 0x64, 0);
    SU32(w, 0x64, shader_vs(sa, "vs_wake_sim"));
    rx_shader_find_ps(sa, "ps_wake_sim", w + 0x68, 0);
    SU32(w, 0x6c, shader_vs(sa, "vs_pressure"));
    rx_shader_find_ps(sa, "ps_pressure", w + 0x70, 0);
    SU32(sh, 0x4c, 0);
    for (uint32_t k = 0; k < 9; k++) {                                                    /* GetSurfaceLevel(0) of each target */
        rx_addr tex = U32(w, 4 * k);
        uint32_t a[3] = {tex, 0, w + 0x40 + 4 * k};
        dcall(tex, 18, 3, a);
    }
    RX_FRAME_ENTER(F, 0x80);
    rx_addr saved = F;                                                                    /* GetRenderTarget(0) */
    SU32(saved, 0, 0);
    { uint32_t a[3] = {dev, 0, saved}; dcall(dev, 38, 3, a); }
    for (uint32_t k = 0; k < 3; k++) {                                                    /* clear the three 512 targets (the pointers at +0x40.. in reverse) */
        rx_addr sf = U32(w, 0x40 + 4 * (2 - k));
        { uint32_t a[3] = {dev, 0, sf}; dcall(dev, 37, 3, a); }
        { uint32_t a[7] = {dev, 0, 0, 1, 0, rx_fb(1.0f), 0}; dcall(dev, 43, 7, a); }
    }
    for (uint32_t j = 0; j < 2; j++)
        for (uint32_t k = 0; k < 3; k++) {
            rx_addr sf = U32(w, 0x4c + 12 * j + 4 * k);
            { uint32_t a[3] = {dev, 0, sf}; dcall(dev, 37, 3, a); }
            { uint32_t a[7] = {dev, 0, 0, 1, 0, rx_fb(1.0f), 0}; dcall(dev, 43, 7, a); }
        }
    { uint32_t a[3] = {dev, 0, U32(saved, 0)}; dcall(dev, 37, 3, a); }
    if (U32(saved, 0)) { uint32_t a[1] = {U32(saved, 0)}; dcall(U32(saved, 0), 2, 1, a); SU32(saved, 0, 0); }
    set_elem(F + 0x20, 0, 0, 0, 1, 0, 0, 0); set_elem(F + 0x20, 1, 0xff, 0, 0x11, 0, 0, 0);
    { uint32_t a[3] = {dev, F + 0x20, w + 0x74}; dcall(dev, 86, 3, a); }                  /* CreateVertexDeclaration */
    set_elem(F + 0x40, 0, 0, 0, 2, 0, 0, 0); set_elem(F + 0x40, 1, 0, 0xc, 1, 0, 5, 0); set_elem(F + 0x40, 2, 0xff, 0, 0x11, 0, 0, 0);
    SU32(w, 0x78, rx_vertex_decl_get(F + 0x40, U32(U32(0, RX_RENDERER), 0x14)));
    SU32(w, 0x7c, 0x14);
    RX_FRAME_LEAVE(F, 0x80);
    water_set_ensure();
    SU32(0, WATER_SET_WAKE, w);
}

/* FUN_007edb00 (ESI = the 0x18-byte block): the refraction object; its render targets exist only where the renderer reports the capability
 * (the byte at [cache + 0x50]: 0 on the PC build the install runs on) */
static rx_addr refraction_construct(rx_addr o) {
    SU8(o, 4, 0); SU32(o, 0, 0); SU32(o, 8, 0); SU32(o, 0xc, 0); SU32(o, 0x10, 0); SU32(o, 0x14, 0);
    rx_addr cache = U32(U32(0, RX_RENDERER), 0x14);
    SU8(o, 4, U32(cache, 0x50) != 0);
    if (U8(o, 4)) fatal("the refraction render targets (FUN_007edb00 with the capability set) are not ported");
    return o;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the surface */

/* FUN_007ede50 (EAX = the surface) */
static void surface_construct(rx_addr s) {
    SU32(s, 4, 0xf0);
    SU32(s, 0, 0x0097298c); SU32(s, 8, 0x009729cc); SU32(s, 0x10, 0); SU32(s, 0x14, 0x009729d4); SU32(s, 0x18, 0);
    SU32(s, 0x50, 0); SU32(s, 0x54, 0);
    SU32(s, 0x488, 0xffffffffu); SF32(s, 0x48c, 0.0f); SF32(s, 0x490, 1.0f); SF32(s, 0x494, 0.0f); SU32(s, 0x498, 0); SF32(s, 0x49c, 0.0f);
    SU32(s, 0x1c, 0); SU32(s, 0x34, 0); SU32(s, 0x38, 0); SU32(s, 0x5c, 0); SU32(s, 0x68, 0); SU32(s, 0x74, 0); SU32(s, 0x80, 0);
    SU32(s, 0x60, 0); SU32(s, 0x6c, 0); SU32(s, 0x78, 0); SU32(s, 0x84, 0); SU32(s, 0x64, 0); SU32(s, 0x70, 0); SU32(s, 0x7c, 0); SU32(s, 0x88, 0);
    SF32(s, 0x480, 0.0f); SF32(s, 0x484, 0.0f);
    SU32(s, 0xc, s);
}

/* FUN_007ee0a0 (stdcall: the surface, the matrix, the extents) */
static void surface_init(rx_addr s, rx_addr matrix, rx_addr extents) {
    rx_addr ocean = U32(0, G_OCEAN);
    if (!ocean) ocean_create();
    else SU32(ocean, 0x14010, U32(ocean, 0x14010) + 1);
    rx_addr wake = U32(0, G_WAKE);
    if (!wake) {
        rx_addr blk = rx_reflex_aligned_alloc(0x30f0);
        wake = blk ? wake_construct(blk) : 0;
        SU32(0, G_WAKE, wake);
        wake_setup();
        wake = U32(0, G_WAKE);
    } else SU32(wake, 0x2c, U32(wake, 0x2c) + 1);
    SU32(s, 0x18, wake);
    rx_addr refr = U32(0, G_REFR);
    if (!refr) {
        rx_addr blk = rx_reflex_aligned_alloc(0x18);
        refr = blk ? refraction_construct(blk) : 0;
        SU32(0, G_REFR, refr);
    }
    SU32(refr, 0, U32(refr, 0) + 1);
    rx_addr mesh = rx_reflex_aligned_alloc(0xf0);
    if (mesh) mesh = mesh_build(mesh, 0x40, 1.0f, rx_bf(x87_copy(U32(extents, 0))), rx_bf(x87_copy(U32(extents, 4))));
    SU32(s, 0x54, mesh);
    for (uint32_t k = 0; k < 64; k += 4) SU32(mesh, 0x70 + k, U32(matrix, k));
    float x = F32(mesh, 0xa0), w2 = F32(mesh, 0x14) * 0.5f;
    SF32(mesh, 0xb0, x - w2);
    float d2 = F32(mesh, 0x18) * 0.5f, z = F32(mesh, 0xa8);
    SF32(mesh, 0xb4, z - d2);
    SF32(mesh, 0xb8, x + w2);
    SF32(mesh, 0xbc, z + d2);
    SU32(s, 0x49c, x87_copy(U32(matrix, 0x34)));
}

/* FUN_007ee5a0 (EDI = width, ESI = the surface, stack: height, &size): the flow texture (G16R16F) at +0x1c */
static rx_addr tex_flow(rx_addr s, uint32_t w, uint32_t h, rx_addr size_out) {
    rx_addr dev = rx_renderer_device();
    SU32(size_out, 0, w * h * 4);
    rx_d3d_create_texture(dev, w, h, 1, 0, 0x70, 1, s + 0x1c);
    SU32(s, 0x2c, w * 4); SU32(s, 0x28, h); SU32(s, 0x20, 0); SU32(s, 0x30, 0x70); SU32(s, 0x24, w);
    return U32(s, 0x1c);
}
/* FUN_007ee600 (XMM0 = scale, EDI = width, ESI = the surface, stack: height, offset, &size): the description texture with heights */
static rx_addr tex_desc_heights(rx_addr s, uint32_t w, uint32_t h, float scale, float offset, rx_addr size_out) {
    rx_addr dev = rx_renderer_device();
    SF32(s, 0x490, scale);
    SF32(s, 0x494, offset);
    SU32(size_out, 0, w * h * 4);
    rx_d3d_create_texture(dev, w, h, 1, 0, 0x15, 1, s + 0x34);
    rx_addr cpu = rx_reflex_aligned_alloc(U32(size_out, 0));
    SU32(s, 0x38, cpu); SU32(s, 0x3c, cpu);
    SU32(s, 0x44, h); SU32(s, 0x4c, 0x15); SU32(s, 0x40, w); SU32(s, 0x48, w * 4);
    return U32(s, 0x34);
}
/* FUN_007ee690 (EDI = width, ESI = the surface, stack: height, &size): the description texture without heights */
static rx_addr tex_desc(rx_addr s, uint32_t w, uint32_t h, rx_addr size_out) {
    rx_addr dev = rx_renderer_device();
    SU32(size_out, 0, w * h * 4);
    rx_d3d_create_texture(dev, w, h, 1, 0, 0x15, 1, s + 0x34);
    rx_addr cpu = rx_reflex_aligned_alloc(U32(size_out, 0));
    SU32(s, 0x38, cpu); SU32(s, 0x3c, cpu);
    SU32(s, 0x44, h); SU32(s, 0x4c, 0x15); SU32(s, 0x40, w); SU32(s, 0x48, w * 4);
    return U32(s, 0x34);
}
/* FUN_007ee700 (EAX = the surface): the CPU copy of the description texture, row by row */
static void desc_cpu_copy(rx_addr s) {
    RX_FRAME_ENTER(F, 0x20);
    rx_addr tex = U32(s, 0x34), lr = F;                       /* D3DLOCKED_RECT {Pitch, pBits} */
    { uint32_t a[5] = {tex, 0, lr, 0, 0x10}; dcall(tex, 19, 5, a); }   /* LockRect(0, &lr, NULL, D3DLOCK_READONLY) */
    rx_addr src = U32(lr, 4);
    for (uint32_t row = 0; row < U32(s, 0x44); row++) {
        for (uint32_t k = 0; k < U32(s, 0x48); k++) SU8(U32(s, 0x3c), U32(s, 0x48) * row + k, U8(src, k));
        src += U32(lr, 0);
    }
    { uint32_t a[2] = {tex, 0}; dcall(tex, 20, 2, a); }                 /* UnlockRect(0) */
    RX_FRAME_LEAVE(F, 0x20);
}
/* FUN_007ee540 (stack: the surface): the valid-cell mask, the water set, the flag */
static void water_set_add(rx_addr s) {
    rx_addr mesh = U32(s, 0x54);
    mesh_mask(mesh, s + 0x3c);
    water_set_ensure();
    rx_addr first = U32(0, WATER_SET_VEC + 0xc), last = U32(0, WATER_SET_VEC + 0x10), cap_end = U32(0, WATER_SET_VEC + 0x14);
    uint32_t cap = first ? (cap_end - first) >> 2 : 0;
    if (((last - first) >> 2) < cap) { SU32(last, 0, s); SU32(0, WATER_SET_VEC + 0x10, last + 4); }
    else {                                                           /* FUN_004178e0: grow by half, at least one more */
        uint32_t size = (last - first) >> 2, ncap = 0x3fffffffu - (cap >> 1) < cap ? 0 : cap + (cap >> 1);
        if (ncap < size + 1) ncap = size + 1;
        rx_addr nb = rx_reflex_aligned_alloc(ncap * 4);
        for (uint32_t k = 0; k < size; k++) SU32(nb, 4 * k, U32(first, 4 * k));
        SU32(nb, 4 * size, s);
        if (first) rx_reflex_aligned_free(first);
        SU32(0, WATER_SET_VEC + 0xc, nb); SU32(0, WATER_SET_VEC + 0x10, nb + (size + 1) * 4); SU32(0, WATER_SET_VEC + 0x14, nb + ncap * 4);
    }
    SU32(s, 0x498, 1);
}

/* the locked-rect form of GetSurfaceLevel(0) / LockRect(&lr, NULL, 0) of a texture: the bits and pitch */
static rx_addr lock_level0(rx_addr tex, rx_addr surf_out, rx_addr lr) {
    { uint32_t a[3] = {tex, 0, surf_out}; dcall(tex, 18, 3, a); }
    rx_addr sf = U32(surf_out, 0);
    { uint32_t a[4] = {sf, lr, 0, 0}; dcall(sf, 13, 4, a); }
    return sf;
}
static void unlock_release(rx_addr sf) {
    { uint32_t a[1] = {sf}; dcall(sf, 14, 1, a); }
    { uint32_t a[1] = {sf}; dcall(sf, 2, 1, a); }
}

/* XenonWaterAsset_Load 007f0320 (stdcall: the asset, the stream) */
static void water_load(rx_addr a, rx_stream* s) {
    RX_FRAME_ENTER(F, 0x80);
    rx_addr surf = a + 0x10, sizev = F, surfp = F + 4, lr = F + 8;
    uint32_t hdr[0x23];
    rx_stream_read_host(s, hdr, 0x8c);
    uint32_t version = hdr[1];
    if (version < 5) { RX_FRAME_LEAVE(F, 0x80); return; }
    if (version != 5) fatal("a version above 5 (the original keeps its defaults: no maps)");
    rx_addr matrix = F + 0x40, ext = F + 0x30;
    rx_stream_read(s, matrix, 0x40);
    rx_stream_read(s, ext, 8);
    const float ms = rx_ms_load();   /* MAP SCALE (port, map_scale.h): the surface's place, extent and wave heights (nothing at 1) */
    if (ms != 1.0f) { rx_ms_v3(matrix, 0x30, ms); rx_ms_f(ext, 0, ms); rx_ms_f(ext, 4, ms); }
    uint32_t has_flow = rx_stream_u32(s), has_desc = rx_stream_u32(s), desc_float = rx_stream_u32(s);
    surface_init(surf, matrix, ext);
    uint32_t w, h;
    if (has_flow) {
        w = rx_stream_u32(s); h = rx_stream_u32(s);
        rx_addr tex = tex_flow(surf, w, h, sizev);
        rx_addr sf = lock_level0(tex, surfp, lr);
        rx_stream_read(s, U32(lr, 4), U32(sizev, 0));
        unlock_release(sf);
    }
    float mesh20 = 5.0f;
    if (has_desc) {
        rx_addr tex, sf;
        if (desc_float) {
            w = rx_stream_u32(s); h = rx_stream_u32(s);
            float scale = rx_bf(rx_stream_u32(s)), offset = rx_bf(rx_stream_u32(s));
            if (ms != 1.0f) { scale *= ms; offset *= ms; }
            mesh20 = scale * 0.5f;
            tex = tex_desc_heights(surf, w, h, scale, offset, sizev);
            sf = lock_level0(tex, surfp, lr);
            rx_stream_read(s, U32(lr, 4), U32(sizev, 0));              /* the texels as stored */
        } else {
            w = rx_stream_u32(s); h = rx_stream_u32(s);
            tex = tex_desc(surf, w, h, sizev);
            sf = lock_level0(tex, surfp, lr);
            uint32_t bytes = w * h * 4;
            rx_addr tmp = rx_reflex_aligned_alloc(bytes);
            rx_stream_read(s, tmp, bytes);
            for (uint32_t y = 0; y < h; y++)                          /* FUN_007f0504: (B, G, R, A) = (src[2], src[1], 0, 0) */
                for (uint32_t x = 0; x < w; x++) {
                    rx_addr d = U32(lr, 4) + U32(lr, 0) * y + 4 * x, p = tmp + 4 * (w * y + x);
                    SU8(d, 0, U8(p, 2)); SU8(d, 1, U8(p, 1)); SU8(d, 2, 0); SU8(d, 3, 0);
                }
            rx_reflex_aligned_free(tmp);
        }
        unlock_release(sf);
        desc_cpu_copy(surf);
    }
    SF32(U32(a, 0x64), 0x20, mesh20);
    water_set_add(surf);
    RX_FRAME_LEAVE(F, 0x80);
}

/* WaterAssetIO::vf3 007e3ec0 */
rx_addr rx_water_asset_io_create(rx_stream* s) {
    rx_addr a = rx_reflex_aligned_alloc(0x4b0);
    if (a) { SU32(a, 8, 0); SU32(a, 0xc, 0); SU32(a, 0, 0x00972a50); surface_construct(a + 0x10); }
    water_load(a, s);
    return a;
}
