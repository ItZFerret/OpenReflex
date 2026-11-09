/* build_mxgame_scene.c -- unit 10: the Game base of MXGame and its three SceneManagers (build_mxgame.h), transliterated from the
 * disassembly of 00882080, 007d14e0, 007df410, 007f9de0, 007f9f20, 007d1d50, 007dfba0, 007d6510, 005ba080, 00843480, 007d1cd0.
 * The OS objects (critical sections, events, the scene ticker and the four render-sync worker threads) go through the exe's kernel32
 * import slots: the product's rx_os stand-ins (single thread: no thread runs), the original imports in the oracle. */
#include "build_mxgame_int.h"
#include "build_globals_int.h"
#include "build_rider.h"
#include "anim.h"

/* Fn_007d1450 (ECX = record): a scene's subscene record (0x110: the name's first byte, +0x104 array, +0x108 count, +0x10c used) */
static void subscene_record_construct(rx_addr r) { SU8(r, 0, 0); SU32(r, 0x104, 0); SU32(r, 0x108, 0); SU32(r, 0x10c, 0); }
/* Fn_007d1360 (ECX = record): a render-list record {+0 the zero vector 0x00a95d50 (x, y, z), +0x10 0, +0x14 0.0f} */
static void render_record_construct(rx_addr r) {
    SU32(r, 0x10, 0); SF32(r, 0x14, 0.0f);
    SU32(r, 0, U32(0, 0x00a95d50)); SU32(r, 4, U32(0, 0x00a95d54)); SU32(r, 8, U32(0, 0x00a95d58));
}

/* 00843480 (ESI = t): the string hash table: build_rider_ctor.c's rx_rider_strtab_init (the same function, one owner) */
rx_addr rx_strtab_construct(rx_addr t) { return rx_rider_strtab_init(t); }

/* 005ba080 (ECX = vector; n; RET 4): std::vector<T*>::reserve (+0xc first, +0x10 last, +0x14 end; n > 0x3fffffff throws: not
 * reached). The 0042bf30 allocation and the move are build_globals_class.c's rx_vecp_reserve. */
void rx_vecp_reserve_ecx(rx_addr v, uint32_t n) {
    if (n > 0x3fffffffu) mx_unreached(0x005ba0b8, "vector too long");
    rx_vecp_reserve(v, n);
}
RX_FUNCTION(0x00843480, rx_strtab_construct)
RX_FUNCTION(0x005ba080, rx_vecp_reserve_ecx)

/* Scene_ProfileScope 007d1cd0: the animation unit's rx_scene_profile_scope (anim.h) */

/* ---- the culling grid (0xfc0 bytes, render container + 0x10f0) ---- */

/* 007d6510 (ESI = grid, EDX = edx; lo, hi, capacity, container; RET 0x10): the cell grid over the box [lo, hi]: the cell size from
 * the box and the cell count (+0xf8c x +0xf90, 20 x 20 to start; coarser when a cell would be under 75 units: a quarter, then the
 * count for 1/75 of the shorter side), the cell array (0x70 bytes each, +0xf78), the four 32-byte record arrays (+0x130 .. +0x13c,
 * 2 * max(capacity, cells + 1) each), the object arrays (+0x148 / +0x14c / +0xfa4 / +0xfb0) and the work buffers; each cell gets its
 * centre and extent (two movaps of frame vectors: their w lanes are words of the frame this function never writes, known-differences
 * entry 1), its four record ranges and the neighbour links (+0x50 previous, +0x54 next). */
void rx_render_grid_init(rx_addr g, uint32_t edx, rx_addr lo, rx_addr hi, uint32_t cap, rx_addr container) {
    MX_FRAME_ALIGN(F, 16, 0x48, 2);
    SU32(g, 0xf80, 0); SU32(g, 0xf7c, 0); SU32(g, 0xf84, 0);
    SU32(g, 0xf98, edx);
    SU32(g, 0xf94, container);
    SU32(g, 0x10, (uint32_t)rx_cvtt(F32(lo, 0) - F32(hi, 0)));
    SU32(g, 0x14, (uint32_t)rx_cvtt(F32(lo, 8) - F32(hi, 8)));
    const float two = 2.0f;   /* 0x92e330 */
    int32_t nx = I32(g, 0xf8c);
    SU32(g, 0x18, (uint32_t)rx_cvtt((F32(hi, 0) * two) / (float)nx));
    int32_t nz = I32(g, 0xf90);
    int32_t cz = rx_cvtt((F32(hi, 8) * two) / (float)nz);
    SU32(g, 0x1c, (uint32_t)cz);
    if (I32(g, 0x18) < 0x4b || cz < 0x4b) {
        int32_t q = (nx + (int32_t)((uint32_t)(nx >> 31) & 3u)) >> 2;      /* cdq; and edx, 3; add; sar 2 */
        SU32(g, 0xf8c, (uint32_t)q); SU32(g, 0xf90, (uint32_t)q);
        const float fq = (float)q;
        SU32(g, 0x18, (uint32_t)rx_cvtt((F32(hi, 0) * two) / fq));
        int32_t a = I32(g, 0x18), b = rx_cvtt((F32(hi, 8) * two) / fq);
        SU32(g, 0x1c, (uint32_t)b);
        if (a < b) b = a;
        int32_t n = rx_cvtt(((float)b * 0.013333334f /* 0x9776d0 */) * fq);
        SU32(g, 0xf90, (uint32_t)n); SU32(g, 0xf8c, (uint32_t)n);
        SU32(g, 0x18, (uint32_t)rx_cvtt((F32(hi, 0) * two) / (float)n));
        nz = n;
        SU32(g, 0x1c, (uint32_t)rx_cvtt((F32(hi, 8) * two) / (float)nz));
    }
    const int32_t cells = I32(g, 0xf8c) * nz + 1;
    SU32(F, 0x24, (uint32_t)cells);
    int32_t recs = (int32_t)(cap * 2u);
    if (cells * 2 > recs) recs = cells * 2;
    SU32(g, 0xf88, (uint32_t)recs);
    const uint32_t cell_bytes = (uint32_t)cells * 0x70u;
    SU32(F, 0x2c, cell_bytes);
    SU32(g, 0xf78, mx_aligned_malloc_locked(cell_bytes, 0x10));
    for (uint32_t k = 0; k < 4; k++) SU32(g, 0x130 + 4 * k, mx_aligned_malloc_locked(U32(g, 0xf88) << 5, 0x10));
    SU32(g, 0x150, cap); SU32(g, 0xfac, cap); SU32(g, 0xfa0, cap);
    SU32(g, 0x158, 0); SU32(g, 0x154, 0); SU32(g, 0xfa8, 0); SU32(g, 0xf9c, 0);
    SU32(g, 0x148, mx_aligned_malloc_locked(cap * 4, 0x10));
    SU32(g, 0x14c, mx_aligned_malloc_locked(U32(g, 0x150) * 4, 0x10));
    SU32(g, 0xfa4, mx_aligned_malloc_locked(U32(g, 0xfa0) * 4, 4));
    SU32(g, 0xfb0, mx_aligned_malloc_locked(U32(g, 0xfac) * 4, 0x10));
    SU32(g, 0xf6c, 0);
    SU32(g, 0x140, mx_aligned_malloc_locked(0x17700, 0x10));
    SU32(g, 0x144, mx_aligned_malloc_locked(0x17700, 0x10));
    SU32(g, 0xf70, mx_aligned_malloc_locked(0x23280, 0x10));
    SU32(g, 0xf74, mx_aligned_malloc_locked(0x23280, 0x10));
    /* the cells: record ranges in steps of per = records / cells (the four arrays share one range per cell) */
    const int32_t per = I32(g, 0xf88) / cells;
    int32_t r0 = 0, r1 = per - 1;          /* ebx / [esp+0x28] and [esp+0x14]: arrays 0x130 / 0x134 */
    int32_t s0 = 0, s1 = per - 1;          /* [esp+0x20] / [esp+0x18]: arrays 0x138 / 0x13c */
    const rx_addr v1 = F + 0x30, v2 = F + 0x40;
    for (int32_t i = 0; i < I32(g, 0xf8c); i++) {
        for (int32_t j = 0; j < I32(g, 0xf90); j++) {
            const uint32_t c = (uint32_t)(I32(g, 0xf90) * i + j) * 0x70u;
            const rx_addr base = U32(g, 0xf78);
            SU32(base + c, 0x38, 0); SU32(U32(g, 0xf78) + c, 0x44, 0); SU32(U32(g, 0xf78) + c, 0x20, 0); SU32(U32(g, 0xf78) + c, 0x2c, 0);
            SU32(U32(g, 0xf78) + c, 0x58, U32(g, 0x130));
            SU32(U32(g, 0xf78) + c, 0x3c, (uint32_t)r0);
            SU32(U32(g, 0xf78) + c, 0x40, (uint32_t)r1);
            SU32(U32(g, 0xf78) + c, 0x5c, U32(g, 0x134));
            SU32(U32(g, 0xf78) + c, 0x48, (uint32_t)r0);
            SU32(U32(g, 0xf78) + c, 0x4c, (uint32_t)r1);
            SU32(U32(g, 0xf78) + c, 0x60, U32(g, 0x138));
            SU32(U32(g, 0xf78) + c, 0x24, (uint32_t)s0);
            SU32(U32(g, 0xf78) + c, 0x28, (uint32_t)s1);
            SU32(U32(g, 0xf78) + c, 0x64, U32(g, 0x13c));
            SU32(U32(g, 0xf78) + c, 0x30, (uint32_t)s0);
            SU32(U32(g, 0xf78) + c, 0x34, (uint32_t)s1);
            const int32_t sx = I32(g, 0x18), sz = I32(g, 0x1c);
            const float hx = F32(hi, 0), hz = F32(hi, 8);
            SF32(v1, 0, (float)(sx * i + (sx >> 1)) - hx);
            SF32(v1, 8, (float)(sz * j + (sz >> 1)) - hz);
            SF32(v2, 0, (hx + 1.0f /* 0x931f24 */) / (float)I32(g, 0xf8c));
            SF32(v1, 4, 0.0f);
            SU32(v2, 4, U32(0, 0x009cf5b0));                        /* 1000.0f (.data) */
            SF32(v2, 8, (hz + 1.0f) / (float)I32(g, 0xf90));
            const rx_addr cell = U32(g, 0xf78) + c;
            mx_copy_words(cell, v1, 4);                             /* movaps: w = frame + 0x3c (never written here) */
            r1 += per; s0 += per; s1 += per; r0 += per;
            mx_copy_words(cell + 0x10, v2, 4);                      /* movaps: w = frame + 0x4c */
        }
    }
    SU32(g, 0x40, U32(0, 0x00a95d50)); SU32(g, 0x44, U32(0, 0x00a95d54)); SU32(g, 0x48, U32(0, 0x00a95d58));
    SF32(g, 0x54, 0.0f);
    SF32(g, 0x50, (float)(I32(g, 0xf8c) * I32(g, 0x18)));
    SF32(g, 0x58, (float)(I32(g, 0xf90) * I32(g, 0x1c)));
    /* the extra cell (index cells - 1, the last 0x70 bytes of the array): the ranges after the grid's */
    const uint32_t e = U32(F, 0x2c);
    SU32(U32(g, 0xf78) + e, -0x38, 0);
    SU32(U32(g, 0xf78) + e, -0x18, U32(g, 0x130));
    SU32(U32(g, 0xf78) + e, -0x34, (uint32_t)r0);
    SU32(U32(g, 0xf78) + e, -0x30, (uint32_t)r1);
    SU32(U32(g, 0xf78) + e, -0x2c, 0);
    SU32(U32(g, 0xf78) + e, -0x14, U32(g, 0x134));
    SU32(U32(g, 0xf78) + e, -0x28, (uint32_t)r0);
    SU32(U32(g, 0xf78) + e, -0x24, (uint32_t)r1);
    SU32(U32(g, 0xf78) + e, -0x50, 0);
    SU32(U32(g, 0xf78) + e, -0x10, U32(g, 0x138));
    SU32(U32(g, 0xf78) + e, -0x4c, (uint32_t)s0);
    SU32(U32(g, 0xf78) + e, -0x48, (uint32_t)s1);
    SU32(U32(g, 0xf78) + e, -0x44, 0);
    SU32(U32(g, 0xf78) + e, -0xc, U32(g, 0x13c));
    SU32(U32(g, 0xf78) + e, -0x40, (uint32_t)s0);
    SU32(U32(g, 0xf78) + e, -0x3c, (uint32_t)s1);
    /* the links */
    SU32(U32(g, 0xf78), 0x54, U32(g, 0xf78) + 0x70);
    SU32(U32(g, 0xf78), 0x50, 0);
    const uint32_t n = (uint32_t)(I32(g, 0xf8c) * I32(g, 0xf90));
    SU32(U32(g, 0xf78) + n * 0x70u, 0x54, 0);
    { const rx_addr last = U32(g, 0xf78) + (uint32_t)(I32(g, 0xf8c) * I32(g, 0xf90)) * 0x70u; SU32(last, 0x50, last - 0x70); }
    for (int32_t k = 1, off = 0x70; k < I32(g, 0xf8c) * I32(g, 0xf90); k++, off += 0x70) {
        rx_addr c = U32(g, 0xf78) + (uint32_t)off;
        SU32(c, 0x54, c + 0x70);
        c = U32(g, 0xf78) + (uint32_t)off;
        SU32(c, 0x50, c - 0x70);
    }
    SU32(g, 0xfb4, 1);
    MX_END(F);
}
RX_FUNCTION(0x007d6510, rx_render_grid_init)

/* 007dfba0 (EAX = container; capacity, &lo, &hi; RET 0xc): under the container's lock (+0x1114) the grid (0xfc0, _aligned_malloc under
 * the allocator lock; +0x20 a critical section, 20 x 20 cells, +0xfb4 1) at +0x10f0 and 007d6510; the octree node pool (+0x10f4:
 * 0xb0-byte nodes, n = capacity / 2 + capacity / 4 + 1 at +0x110c, the block (n * 0x16 + capacity) * 8 bytes, the nodes zeroed) and
 * its root (+0x1100: node 0 with the box lo / hi, +0x1104 the count 1). The lock's address is kept in the third argument's slot. */
void rx_render_grid_create(rx_addr c, uint32_t cap, rx_addr lo, rx_addr hi) {
    MX_FRAME(F, 0, 4);
    const rx_addr cs = c + 0x1114;
    mx_cs_enter(cs);
    SU32(F_sp, 0xc, cs);                                     /* mov [esp + 0x20], eax: the &hi slot */
    if (U8(0, 0x00da67d8)) mx_cs_enter(0x00d61704);
    rx_addr g = rx_crt_aligned_malloc(0xfc0, 0x10);
    if (U8(0, 0x00da67d8)) mx_cs_leave(0x00d61704);
    if (g) {
        mx_cs_init(g + 0x20);
        SU32(g, 0xf8c, 0x14); SU32(g, 0xf90, 0x14);
        SU32(g, 0xfb4, 1);
    }
    SU32(c, 0x10f0, g);
    MX_CALL(4, rx_render_grid_init(g, 0, lo, hi, cap, c));
    const uint32_t n = (cap >> 2) + (cap >> 1) + 1;
    SU32(c, 0x110c, n);
    SU32(c, 0x1104, 0); SU32(c, 0x1108, 0); SU32(c, 0x1110, cap);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc((n * 0x16u + cap) * 8u));
    SU32(c, 0x10f4, p);
    MX_AT(F, 0); crt_memset(p, 0, U32(c, 0x110c) * 0xb0u);
    SU32(c, 0x10fc, U32(c, 0x10f4) + U32(c, 0x110c) * 0xb0u);
    SU32(c, 0x10f8, U32(c, 0x10f4));
    const uint32_t k = U32(c, 0x1104);
    const rx_addr root = U32(c, 0x10f4) + k * 0xb0u;
    SU32(c, 0x1100, root);
    SU32(c, 0x1104, k + 1);
    for (uint32_t i = 0; i < 3; i++) SU32(root, 4 * i, rx_x87_load_f32(U32(lo, 4 * i)));
    for (uint32_t i = 0; i < 3; i++) SU32(root, 0x10 + 4 * i, rx_x87_load_f32(U32(hi, 4 * i)));
    SU32(root, 0x20, 0); SU32(root, 0x24, 0);
    mx_cs_leave(U32(F_sp, 0xc));
    MX_END(F);
}
RX_FUNCTION(0x007dfba0, rx_render_grid_create)

/* 007d1d50 (stdcall scene, capacity; RET 8): the octree of the scene's render container (+0x1a18) over (0, 0, 0) .. (10000, 10000,
 * 10000) inside two profiler scopes "Octree" */
void rx_scene_octree_init(rx_addr scene, uint32_t cap) {
    MX_FRAME_ALIGN(F, 16, 0x20, 0);
    MX_CALL(1, rx_scene_profile_scope(0x00971fa0));
    SF32(F, 0x10, 0.0f); SF32(F, 0x14, 0.0f); SF32(F, 0x18, 0.0f);
    SU32(F, 0, 0x461c4000u); SU32(F, 4, 0x461c4000u); SU32(F, 8, 0x461c4000u);   /* 10000.0f (0x931e98) */
    MX_CALL(3, rx_render_grid_create(scene + 0x1a18, cap, F + 0x10, F));
    MX_CALL(1, rx_scene_profile_scope(0x00971fa0));
    MX_END(F);
}
RX_FUNCTION(0x007d1d50, rx_scene_octree_init)

/* 007f9de0 (ESI = list; n, grow; RET 8): a pool list of n 0x20-byte render records (Fn_007d1360), +4 count 0, +8 n, +0xc grow, +0x10
 * max 0x7fffffff */
static void record_pool32(rx_addr l, uint32_t n, uint32_t grow) {
    MX_FRAME(F, 0, 2);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(rx_mul_sat(n, 0x20)));
    if (p) for (uint32_t i = 0; i < n; i++) render_record_construct(p + 0x20 * i);   /* `vector constructor iterator' */
    SU32(l, 0, p); SU32(l, 8, n); SU32(l, 0xc, grow); SU32(l, 4, 0); SU32(l, 0x10, 0x7fffffffu);
    MX_END(F);
}
/* 007f9f20 (ESI = list): 100 0x40-byte records (Fn_007d1360 on each), count 0, 100 / 100, max 0x7fffffff */
static void record_pool64(rx_addr l) {
    MX_FRAME(F, 0, 1);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(0x64 * 0x40));
    if (p) for (uint32_t i = 0; i < 0x64; i++) render_record_construct(p + 0x40 * i);
    SU32(l, 0, p); SU32(l, 4, 0); SU32(l, 8, 0x64); SU32(l, 0xc, 0x64); SU32(l, 0x10, 0x7fffffffu);
    MX_END(F);
}

/* 007df410 (EDI = container): the render container of a scene (scene + 0x1a18): nine pointer lists {data, count, capacity, grow, max}
 * (+0x1000 50, +0x1014 4, +0x1028 50, +0x103c 1024, +0x1050 256, +0x1064 / +0x1078 / +0x108c / +0x10a0 1024), the render record
 * pools +0x10b4 (100 x 0x20), +0x10c8 (64 x 0x20), +0x10dc (100 x 0x40), the lock +0x1114 and +0x112c, the four render-sync workers
 * (threads +0x1150 + 8k / ids +0x1154 + 8k, events +0x1170 + 4k / +0x1180 + 4k, records +0x1190 + 0x10k {&event a, &event b,
 * container, 1}; Render_SyncWorkerThread 007df370, affinity 1 / 2 / 4 / 8, priority 0 / 0 / 1 / 0). */
rx_addr rx_render_container_construct(rx_addr c) {
    MX_FRAME(F, 8, 3);
    static const struct { uint16_t at; uint16_t n; } lists[9] = {
        {0x1000, 0x32}, {0x1014, 4}, {0x1028, 0x32}, {0x103c, 0x400}, {0x1050, 0x100},
        {0x1064, 0x400}, {0x1078, 0x400}, {0x108c, 0x400}, {0x10a0, 0x400}};
    for (int k = 0; k < 9; k++) {
        rx_addr p;
        rx_set_sp(F - 4u * (uint32_t)k);                     /* the cdecl arguments stay pushed until one add esp, 0x24 */
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(lists[k].n)));
        const rx_addr l = c + lists[k].at;
        SU32(l, 0, p); SU32(l, 4, 0); SU32(l, 8, lists[k].n); SU32(l, 0xc, lists[k].n); SU32(l, 0x10, 0x7fffffffu);
    }
    rx_set_sp(F);
    MX_CALL(2, record_pool32(c + 0x10b4, 0x64, 0x64));
    MX_CALL(2, record_pool32(c + 0x10c8, 0x40, 0x40));
    MX_CALL(0, record_pool64(c + 0x10dc));
    for (uint32_t i = 0; i < 6; i++) SU32(c + 0x1114, 4 * i, 0);
    mx_cs_init(c + 0x112c);
    for (uint32_t i = 0; i < 4; i++) SU32(c + 0x1150, 8 * i, 0);   /* `vector constructor iterator'(+0x1150, 8, 4, 004dc090) */
    for (uint32_t i = 0; i < 4; i++) SU32(c + 0x1170, 4 * i, 0);   /* (+0x1170, 4, 4) */
    for (uint32_t i = 0; i < 4; i++) SU32(c + 0x1180, 4 * i, 0);   /* (+0x1180, 4, 4) */
    SU32(c, 0x10f0, 0); SU32(c, 0x10f4, 0); SU32(c, 0x114c, 0);
    mx_cs_init(c + 0x1114);
    SU32(c, 0x1144, 0); SU32(c, 0x11d0, 0); SU32(c, 0x11d4, 0); SU32(c, 0x11e0, 0);
    for (uint32_t k = 0; k < 4; k++) {
        const rx_addr cur = c + 0x1170 + 4 * k, rec = c + 0x1190 + 0x10 * k;
        SU32(cur, 0x10, mx_event_create(0, 0, 0));
        SU32(cur, 0, mx_event_create(0, 0, 0));
        SU32(rec, 4, cur + 0x10);
        SU32(rec, 0, cur);
        SU32(rec, 8, c);
        SU32(rec, 0xc, 1);
    }
    static const uint8_t aff[4] = {1, 2, 4, 8}, pri[4] = {0, 0, 1, 0};
    for (uint32_t k = 0; k < 4; k++) {
        const rx_addr slot = c + 0x1150 + 8 * k;
        const uint32_t h = mx_thread_create(0x007df370 /* Render_SyncWorkerThread */, c + 0x1190 + 0x10 * k, 4, slot + 4);
        SU32(slot, 0, h);
        mx_thread_affinity(h, aff[k]);
        mx_thread_priority(U32(slot, 0), pri[k]);
        mx_thread_resume(U32(slot, 0));
    }
    MX_END(F);
    return c;
}
RX_FUNCTION(0x007df410, rx_render_container_construct)

/* 007d14e0 (stdcall scene, kind, capacity, grow; RET 0x10): SceneManager. kind = the scene slot (0 front end, 1 event world, 2 UI),
 * capacity = the octree's object count, grow = the entity vectors' reserve. */
rx_addr rx_scene_manager_construct(rx_addr s, uint32_t kind, uint32_t cap, uint32_t grow) {
    MX_FRAME_ALIGN(F, 8, 0, 4);
    for (uint32_t i = 4; i < 0x34; i += 4) SU32(s, i, 0);
    for (uint32_t i = 0; i < 0x18; i++) subscene_record_construct(s + 0x38 + 0x110 * i);   /* `vector constructor iterator' */
    rx_strtab_construct(s + 0x19d4);
    rx_strtab_construct(s + 0x19f8);
    rx_render_container_construct(s + 0x1a18);
    SU32(s, 0x2bfc, 0); SU32(s, 0x2c00, 0); SU32(s, 0x2c04, 0); SU32(s, 0x2c08, 0); SU32(s, 0x2c0c, 0);
    static const uint16_t vecs[5] = {0x2c10, 0x2c3c, 0x2c54, 0x2c6c, 0x2c84};
    for (int k = 0; k < 5; k++) {
        if (k == 1) { SU32(s, 0x2c28, 0); SU32(s, 0x2c2c, 0); SU32(s, 0x2c30, 0); SU32(s, 0x2c34, 0); SU32(s, 0x2c38, 0); }
        const rx_addr v = s + vecs[k];
        rx_addr p;
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(4));
        if (p) SU32(p, 0, v);
        SU32(v, 0, p);
        SU32(v, 0xc, 0); SU32(v, 0x10, 0); SU32(v, 0x14, 0);
    }
    SU32(s, 0x2c9c, 0); SU32(s, 0x2ca4, 0); SU32(s, 0x2ca8, 0);
    mx_cs_init(s + 4);
    mx_cs_init(s + 0x1c);
    SU32(s, 0x2cc4, kind);
    SU32(s, 0x19f4, 1); SU32(s, 0x2cb0, 1); SU32(s, 0x2cb4, 1);
    SU32(s, 0x19d0, 0); SU32(s, 0x34, 0); SU32(s, 0, 0);
    SF32(s, 0x2d14, 1.0f);   /* 0x931f24 */
    SU32(s, 0x2cbc, 0);
    rx_addr p;
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(0x10)));
    SU32(s, 0x2bfc, p); SU32(s, 0x2c04, 0x10); SU32(s, 0x2c08, 0x10); SU32(s, 0x2c00, 0); SU32(s, 0x2c0c, 0x7fffffffu);
    MX_CALLR(p, 1, rx_reflex_aligned_alloc(mx_size4(grow)));
    SU32(s, 0x2c2c, 0);
    SU32(s, 0x2c28, p); SU32(s, 0x2c30, grow); SU32(s, 0x2c34, grow); SU32(s, 0x2c38, 0x7fffffffu);
    MX_CALL(1, rx_vecp_reserve_ecx(s + 0x2c10, grow));
    MX_CALL(1, rx_vecp_reserve_ecx(s + 0x2c3c, grow));
    MX_CALL(1, rx_vecp_reserve_ecx(s + 0x2c54, grow));
    MX_CALL(1, rx_vecp_reserve_ecx(s + 0x2c6c, 10));
    MX_CALL(1, rx_vecp_reserve_ecx(s + 0x2c84, 200));
    SU32(s, 0x2cc0, 0); SU32(s, 0x2cc8, 0);
    mx_copy_words(s + 0x2cd0, 0x009cb510, 0x10);               /* the identity matrix */
    SU32(s, 0x2d10, 0);
    MX_CALL(2, rx_scene_octree_init(s, cap));
    SU32(s, 0x2ca4, mx_event_create(0, 0, 0));
    SU32(s, 0x2ca8, mx_event_create(0, 0, 0));
    const uint32_t th = mx_thread_create(0x007d22a0 /* Scene_TickerThread */, s, 4, s + 0x2ca0);
    SU32(s, 0x2c9c, th);
    mx_thread_affinity(th, 0x10);
    mx_thread_priority(U32(s, 0x2c9c), 1);
    mx_thread_resume(U32(s, 0x2c9c));
    SU32(s, 0x2cac, 0);
    MX_END(F);
    return s;
}
RX_FUNCTION(0x007d14e0, rx_scene_manager_construct)

/* Game::Game 00882080 (ESI = game): vtable 0x009772e0, the three SceneManagers (0x2d20 each): +0xc (kind 2, 10, 5), +4 (0, 100, 5),
 * +8 (1, 10000, 50) */
rx_addr rx_game_base_construct(rx_addr g) {
    SU32(g, 0, 0x009772e0);
    static const struct { uint8_t at, kind; uint16_t cap; uint8_t grow; } sc[3] = {{0xc, 2, 10, 5}, {4, 0, 100, 5}, {8, 1, 10000, 0x32}};
    for (int k = 0; k < 3; k++) {
        rx_addr p;
        MX_CALLR(p, 1, rx_reflex_aligned_alloc(0x2d20));
        if (p) MX_CALLR(p, 4, rx_scene_manager_construct(p, sc[k].kind, sc[k].cap, sc[k].grow));
        SU32(g, sc[k].at, p);
    }
    return g;
}
RX_FUNCTION(0x00882080, rx_game_base_construct)
