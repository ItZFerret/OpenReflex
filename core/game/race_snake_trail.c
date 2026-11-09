/* race_snake_trail.c -- the Snake party mode's trails, hand-transliterated from the original (each function's address is in its comment;
 * frames and the stack depth of each call are the original's where a callee or a later read depends on them).
 *
 * Gameplay. Each SnakeDriver owns a trail object T (0x150 bytes at the driver's +0xf14): a list of CHAINS (one per continuous piece of
 * wall; 0x50-byte records from a pool of 100), each a list of SEGMENTS (0x1c0-byte records from a pool of 2000), the cross sections the
 * rider leaves behind. The activity's spatial hash (MXActivitySnake +0xff070) maps 10-ft cells (Bob Jenkins' lookup2 of the cell's
 * three ints) to the collision records of the segments whose midpoint is in them; the collision query (0069bba0) tests the 27 cells
 * around a point against the records' planes (a rider's own newest 25 segments excluded).
 *
 * T:       +0 the hash, +0x10 the mesh (below), +0x100 the shape record (0x9308cc), +0x104 the owner id, +0x108 the segment pool
 *          {+0 block, +4 n, +8 / +0xc the free list, +0x10 free}, +0x11c / +0x120 the chain list head / tail, +0x124 the chain pool,
 *          +0x138 the total length, +0x13c the distance laid (the texture's v), +0x140 the segment counter, +0x144 0.
 * chain:   +0 next, +4 prev, +8 / +0xc its segments head / tail, +0x10 their count, +0x20 the last direction, +0x30 the anchor of the
 *          current straight piece, +0x40 the chain's length, +0x44 its shape record.
 * segment: +0 next, +4 prev, +0x10 the position, +0x20 its side axis, +0x30 its up axis, +0x40 the width scale, +0x44 the height,
 *          +0x48 the distance at it, +0x4c the counter at its birth, +0x50 its collision record (0x170: +0 the owner id, +4 the counter,
 *          +8 / +0xc the cell list links, +0x10 the midpoint of the slab to the previous segment, +0x20 the plane count, +0x30 the
 *          planes (0x20 each: normal, +0x10 d): the two end faces, then two per outline edge).
 *
 * Render side. T + 0x10 is a XenonSnakeVisibleMesh (vtable 0x0095e5b4; IRenderable +0x90, IRenderEvent +0x98): a ring of 8000 triangles
 * (+0xcc: 24000 vertices of 0x28 bytes {position, normal, u, v, the distance}; +0xbc used, +0xc0 the tail, +0xc4 the head) that the
 * code above writes as segments come and go (the caps 006a0e70 / 006a1030 / 006a1510, the sides 006a11f0). As for the boundary wall
 * (race_snake_wall.c) the core keeps the CPU array exactly; the trail meshes' shared GPU resources (006a0cd0: 12 x 2 vertex buffers of
 * 960000 bytes and the declaration; 006a0c70: "SnakeVS" / "SnakePS") go through the core's D3D layer; the render event
 * (XenonSnakeVisibleMesh::vf0_o98 006a1750: the ring copied into the buffers and drawn) and the IRenderable slots are the renderer's and
 * are not ported. The shape is the record at [0x9cd974] (0x9308cc, the only one: no code writes the pointer): {+0 side points 8, +4
 * their profile fn, +8 side edges 4, +0xc the edge pairs, +0x10 cap points 4, +0x14 their fn, +0x18 cap triangles 2, +0x1c the cap
 * indices, +0x20 outline points 4, +0x24 their 2-D fn, +0x28 the merge distance 5}. */
#include "race_snake_trail.h"
#include "race_snake.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "build_render.h"
#include "anim.h"
#include "phys_crt.h"
#include "world_veh.h"
#include "race_beh1_extern.h"
#include "crt.h"
#include "../src/rx_d3d.h"
#include "../src/rx_os.h"
#include <string.h>

#define VT_MESH       0x0095e5b4u
#define VT_MESH_O90   0x0095e62cu
#define VT_MESH_O98   0x0095e66cu
#define VT_ENTITY     0x0095a2e4u
#define VT_RENDERABLE 0x0093708cu
#define VT_RENDEREVT  0x009370ccu
#define IAT_ENTER_CS  0x0091600cu
#define IAT_LEAVE_CS  0x009160f8u
#define IAT_INIT_CS   0x00916014u
#define IAT_DELETE_CS 0x00916010u
#define SHAPE         0x009308ccu   /* the trail shape record ([0x9cd974]) */
#define RING          8000u         /* triangles in a trail mesh's ring */
#define VB_TABLE      0x00a7f0b8u   /* the 12 pairs of shared vertex buffers */
#define GPU_LIST_CS   0x00d649f8u   /* the GPU resource registry's lock */

static inline void os_cs(uint32_t slot, rx_addr cs) { const uint32_t a[1] = {cs}; (void)rx_os_import(slot, 1, a); }
static inline uint32_t x87_bits(uint32_t b) { return ((b & 0x7f800000u) == 0x7f800000u && (b & 0x7fffffu)) ? b | 0x400000u : b; }
static inline float x87_copy(float v) { return rx_bf(x87_bits(rx_fb(v))); }
static inline void fcopy(rx_addr d, uint32_t od, rx_addr s, uint32_t os) { SU32(d, od, x87_bits(U32(s, os))); }
static inline float sqrt_call(float x) { float r; ACT_CALLR(r, 1, phys_sqrtf(x)); return r; }
static inline double rdata_f64(rx_addr a) { const uint64_t b = (uint64_t)U32(0, a) | (uint64_t)U32(0, a + 4) << 32; double d; memcpy(&d, &b, 8); return d; }

/* ======================================================================================================================== */
/* pools and the spatial hash                                                                                                */

/* a record appended to a free list {+8 head, +0xc tail, +0x10 free} (link +0 next, +4 prev) */
static void pool_put(rx_addr p, rx_addr r) {
    const rx_addr t = U32(p, 0xc);
    if (t == 0) { SU32(p, 8, r); SU32(p, 0xc, r); SU32(r, 4, 0); }
    else { SU32(t, 0, r); SU32(r, 4, U32(p, 0xc)); SU32(p, 0xc, r); }
    SU32(r, 0, 0);
    SU32(p, 0x10, U32(p, 0x10) + 1);
}
/* the free list's head unlinked (0 when empty) */
static rx_addr pool_take(rx_addr p) {
    const rx_addr r = U32(p, 8);
    if (r == 0) return 0;
    if (U32(r, 0) != 0) SU32(U32(r, 0), 4, U32(r, 4)); else SU32(p, 0xc, U32(r, 4));
    if (U32(r, 4) != 0) SU32(U32(r, 4), 0, U32(r, 0)); else SU32(p, 8, U32(r, 0));
    SU32(r, 4, 0);
    SU32(r, 0, 0);
    SU32(p, 0x10, U32(p, 0x10) - 1);
    return r;
}
/* a pool {+0 the block, +4 n, the free list}: n records of `size` from ReflexAlignedAlloc, all free. Frame: push edi. */
static rx_addr pool_init(rx_addr p, uint32_t n, uint32_t size) {
    ACT_FRAME(F, 0, 1);
    rx_addr b;
    ACT_CALLR(b, 1, rx_reflex_aligned_alloc(n * size));
    SU32(p, 0, b);
    SU32(p, 4, n);
    SU32(p, 8, 0);
    SU32(p, 0xc, 0);
    SU32(p, 0x10, n);
    for (uint32_t i = 0; i < n; i++) {
        const rx_addr r = U32(p, 0) + i * size, t = U32(p, 0xc);
        if (t == 0) { SU32(p, 8, r); SU32(p, 0xc, r); SU32(r, 4, 0); }
        else { SU32(t, 0, r); SU32(r, 4, U32(p, 0xc)); SU32(p, 0xc, r); }
        SU32(r, 0, 0);
    }
    ACT_END(F);
    return p;
}
/* 006a2650 (ESI = pool): 4000 hash nodes of 0x28 bytes */
rx_addr rx_snake_hash_pool_init(rx_addr p) { return pool_init(p, 4000, 0x28); }
RX_FUNCTION(0x006a2650, rx_snake_hash_pool_init)
/* 006a24f0 (ESI = pool): 2000 segments of 0x1c0 bytes */
rx_addr rx_snake_segment_pool_init(rx_addr p) { return pool_init(p, 2000, 0x1c0); }
RX_FUNCTION(0x006a24f0, rx_snake_segment_pool_init)
/* 006a25a0 (ESI = pool): 100 chains of 0x50 bytes */
rx_addr rx_snake_chain_pool_init(rx_addr p) { return pool_init(p, 100, 0x50); }
RX_FUNCTION(0x006a25a0, rx_snake_chain_pool_init)

/* 006a2600 (EDX = pool) -> a chain record, +0 .. +0xc cleared (0 when the pool is empty) */
rx_addr rx_snake_chain_take(rx_addr p) {
    const rx_addr r = pool_take(p);
    if (r) { SU32(r, 0, 0); SU32(r, 4, 0); SU32(r, 8, 0); SU32(r, 0xc, 0); }
    return r;
}
RX_FUNCTION(0x006a2600, rx_snake_chain_take)
/* 006a2550 (EDX = pool) -> a segment record, its links and its collision record's cell links (+0x58 / +0x5c) cleared */
rx_addr rx_snake_segment_take(rx_addr p) {
    const rx_addr r = pool_take(p);
    if (r) { SU32(r, 0, 0); SU32(r, 4, 0); SU32(r, 0x58, 0); SU32(r, 0x5c, 0); }
    return r;
}
RX_FUNCTION(0x006a2550, rx_snake_segment_take)

/* 006a21d0 (EAX = the hash): {+0 the node pool, +0x14 the buckets ({head, tail}), +0x18 their bits, +0x1c / +0x20 the node list,
 * +0x24 the nodes in use}: the pool, the list empty, 2^10 buckets (the smallest power of two >= 4000, / 4, at most 2^13), zeroed.
 * Frame: push ebp / esi / edi. */
void rx_snake_hash_init(rx_addr h) {
    ACT_FRAME(F, 0, 3);
    ACT_CALL(0, (void)rx_snake_hash_pool_init(h));
    SU32(h, 0x1c, 0); SU32(h, 0x20, 0); SU32(h, 0x24, 0); SU32(h, 0x18, 0);
    do SU32(h, 0x18, U32(h, 0x18) + 1); while ((1u << (U32(h, 0x18) & 0x1f)) < 4000u);
    if (U32(h, 0x18) > 2) SU32(h, 0x18, U32(h, 0x18) - 2);
    if (U32(h, 0x18) > 0xd) SU32(h, 0x18, 0xd);
    const uint32_t n = 1u << (U32(h, 0x18) & 0x1f);
    rx_addr b;
    ACT_CALLR(b, 1, rx_reflex_aligned_alloc(n * 8u));
    if (b != 0) for (uint32_t i = 0; i < n; i++) { SU32(b, 8 * i, 0); SU32(b, 8 * i + 4, 0); }
    SU32(h, 0x14, b);
    ACT_END(F);
}
RX_FUNCTION(0x006a21d0, rx_snake_hash_init)

static inline rx_addr bucket_of(rx_addr h, uint32_t hv) { return U32(h, 0x14) + 8u * (((1u << (U32(h, 0x18) & 0x1f)) - 1u) & hv); }
/* a node unlinked from its bucket and the node list and put back in the pool */
static void node_release(rx_addr h, rx_addr n, rx_addr bk) {
    if (U32(n, 0) != 0) SU32(U32(n, 0), 4, U32(n, 4)); else SU32(bk, 4, U32(n, 4));
    if (U32(n, 4) != 0) SU32(U32(n, 4), 0, U32(n, 0)); else SU32(bk, 0, U32(n, 0));
    SU32(n, 4, 0); SU32(n, 0, 0);
    if (U32(n, 8) != 0) SU32(U32(n, 8), 0xc, U32(n, 0xc)); else SU32(h, 0x20, U32(n, 0xc));
    if (U32(n, 0xc) != 0) SU32(U32(n, 0xc), 8, U32(n, 8)); else SU32(h, 0x1c, U32(n, 8));
    SU32(n, 0xc, 0); SU32(n, 8, 0);
    SU32(h, 0x24, U32(h, 0x24) - 1);
    pool_put(h, n);
}

/* 006a26b0 (EDX = the hash): every node back in the pool */
void rx_snake_hash_clear(rx_addr h) {
    for (rx_addr n = U32(h, 0x1c); n != 0; n = U32(h, 0x1c)) node_release(h, n, bucket_of(h, U32(n, 0x10)));
}
RX_FUNCTION(0x006a26b0, rx_snake_hash_clear)

/* 0069b8e0 (ECX = a key of three ints) -> EAX: Bob Jenkins' lookup2 mix of (w0 + golden, w1 + golden, w2) */
uint32_t rx_snake_hash_key(rx_addr k) {
    uint32_t a = U32(k, 0) + 0x9e3779b9u, b = U32(k, 4) + 0x9e3779b9u, c = U32(k, 8);
    a -= c; a -= b; a ^= c >> 13;
    b -= c; b -= a; b ^= a << 8;
    c -= b; c -= a; c ^= b >> 13;
    a -= c; a -= b; a ^= c >> 12;
    b -= c; b -= a; b ^= a << 16;
    c -= b; c -= a; c ^= b >> 5;
    a -= c; a -= b; a ^= c >> 3;
    b -= c; b -= a; b ^= a << 10;
    c -= b; c -= a; c ^= b >> 15;
    return c;
}
RX_FUNCTION(0x0069b8e0, rx_snake_hash_key)

static rx_addr node_find(rx_addr bk, uint32_t hv, rx_addr k) {
    for (rx_addr n = U32(bk, 0); n != 0; n = U32(n, 0))
        if (U32(n, 0x10) == hv && U32(n, 0x14) == U32(k, 0) && U32(n, 0x18) == U32(k, 4) && U32(n, 0x1c) == U32(k, 8)) return n;
    return 0;
}

/* 006a24a0 (ESI = key, EDI = the hash) -> EAX: the key's value (node + 0x20: its records {head, tail}) or 0 */
rx_addr rx_snake_hash_find(rx_addr k, rx_addr h) {
    const uint32_t hv = rx_snake_hash_key(k);
    const rx_addr n = node_find(bucket_of(h, hv), hv, k);
    return n ? n + 0x20 : 0;
}
RX_FUNCTION(0x006a24a0, rx_snake_hash_find)

/* 006a2280 (EAX = key, ESI = the hash) -> EAX: the key's value; absent, a node from the pool (its links and value cleared) appended
 * to its bucket and to the node list; 0 when the pool is empty */
rx_addr rx_snake_hash_insert(rx_addr k, rx_addr h) {
    const uint32_t hv = rx_snake_hash_key(k);
    const rx_addr bk = bucket_of(h, hv);
    rx_addr n = node_find(bk, hv, k);
    if (n) return n + 0x20;
    n = pool_take(h);
    if (n == 0) return 0;
    SU32(n, 0, 0); SU32(n, 4, 0); SU32(n, 8, 0); SU32(n, 0xc, 0); SU32(n, 0x20, 0); SU32(n, 0x24, 0);
    SU32(n, 0x10, hv);
    SU32(n, 0x14, U32(k, 0)); SU32(n, 0x18, U32(k, 4)); SU32(n, 0x1c, U32(k, 8));
    if (U32(bk, 4) == 0) { SU32(bk, 0, n); SU32(bk, 4, n); SU32(n, 4, 0); }
    else { SU32(U32(bk, 4), 0, n); SU32(n, 4, U32(bk, 4)); SU32(bk, 4, n); }
    SU32(n, 0, 0);
    if (U32(h, 0x20) == 0) { SU32(h, 0x1c, n); SU32(h, 0x20, n); SU32(n, 0xc, 0); SU32(n, 8, 0); }
    else { SU32(U32(h, 0x20), 8, n); SU32(n, 0xc, U32(h, 0x20)); SU32(h, 0x20, n); SU32(n, 8, 0); }
    SU32(h, 0x24, U32(h, 0x24) + 1);
    return n + 0x20;
}
RX_FUNCTION(0x006a2280, rx_snake_hash_insert)

/* 006a23b0 (EAX = key, ESI = the hash): the key's node back in the pool (nothing when absent) */
void rx_snake_hash_remove(rx_addr k, rx_addr h) {
    const uint32_t hv = rx_snake_hash_key(k);
    const rx_addr bk = bucket_of(h, hv);
    const rx_addr n = node_find(bk, hv, k);
    if (n) node_release(h, n, bk);
}
RX_FUNCTION(0x006a23b0, rx_snake_hash_remove)

/* one coordinate of 0069b9b0: (int)(float)(int) of copysign(0.5, v) + v, v = p / 10 - 0.49999999999 (53-bit x87; _ftol2_sse) */
static int32_t cell_of(float p) {
    const double v = (double)rx_divs(p, 10.0f /* 0x92fd88 */) - rdata_f64(0x00977558);
    const double r = crt_copysign(rdata_f64(0x009774e8) /* 0.5 */, v) + v;
    return rx_cvtt((double)(float)rx_cvtt(r));
}
/* 0069b9b0 (EDI = a position, ESI = out) -> EAX = out: the position's 10-ft cell {x, y, z} (computed z, y, x) */
rx_addr rx_snake_cell(rx_addr pos, rx_addr out) {
    const int32_t z = cell_of(F32(pos, 8)), y = cell_of(F32(pos, 4)), x = cell_of(F32(pos, 0));
    SU32(out, 0, (uint32_t)x); SU32(out, 4, (uint32_t)y); SU32(out, 8, (uint32_t)z);
    return out;
}
RX_FUNCTION(0x0069b9b0, rx_snake_cell)

/* 0069bab0 (stdcall hash, record; RET 8): the collision record appended to its midpoint's cell ({head, tail}; record +8 next, +0xc
 * prev), the hash's record count (+0x28) + 1. Frame: sub esp, 0xc; push ebx / ebp / esi / edi (the cell F + 0x10). */
void rx_snake_hash_add(rx_addr h, rx_addr rec) {
    ACT_FRAME(F, 0xc, 4);
    rx_snake_cell(rec + 0x10, F + 0x10);
    const rx_addr v = rx_snake_hash_insert(F + 0x10, h);
    if (v != 0) {
        if (U32(v, 4) == 0) { SU32(v, 0, rec); SU32(v, 4, rec); SU32(rec, 0xc, 0); SU32(rec, 8, 0); }
        else { SU32(U32(v, 4), 8, rec); SU32(rec, 0xc, U32(v, 4)); SU32(v, 4, rec); SU32(rec, 8, 0); }
        SU32(h, 0x28, U32(h, 0x28) + 1);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0069bab0, rx_snake_hash_add)

/* 0069bb20 (stdcall hash, record; RET 8): the collision record unlinked from its cell (the cell removed when it empties), the record
 * count - 1. Frame: sub esp, 0xc; push ebx / ebp / esi / edi. */
void rx_snake_hash_del(rx_addr h, rx_addr rec) {
    ACT_FRAME(F, 0xc, 4);
    rx_snake_cell(rec + 0x10, F + 0x10);
    const rx_addr v = rx_snake_hash_find(F + 0x10, h);
    if (v != 0) {
        if (U32(rec, 8) != 0) SU32(U32(rec, 8), 0xc, U32(rec, 0xc)); else SU32(v, 4, U32(rec, 0xc));
        if (U32(rec, 0xc) != 0) SU32(U32(rec, 0xc), 8, U32(rec, 8)); else SU32(v, 0, U32(rec, 8));
        SU32(rec, 0xc, 0); SU32(rec, 8, 0);
        if (U32(v, 0) == 0) rx_snake_hash_remove(F + 0x10, h);
        SU32(h, 0x28, U32(h, 0x28) - 1);
    }
    ACT_END(F);
}
RX_FUNCTION(0x0069bb20, rx_snake_hash_del)

/* 0069bba0 (stdcall hash, position, radius, id, counter; RET 0x14) -> EAX: the first collision record of the 27 cells around the
 * position whose every plane has the position within radius of it (normal . p - d <= radius), skipping the asker's own records less
 * than 25 segments old (id equal and counter + 25 > the asker's counter); each record looked at counts [0xa942a0]. 0 when none.
 * Frame: sub esp, 0x20; push ebx / ebp / esi / edi (the cell F + 0x18, the key F + 0x24). */
rx_addr rx_snake_hash_hit(rx_addr h, rx_addr pos, float radius, uint32_t id, uint32_t age) {
    ACT_FRAME(F, 0x20, 4);
    rx_snake_cell(pos, F + 0x18);
    for (int32_t i = -1; i <= 1; i++)
        for (int32_t j = -1; j <= 1; j++)
            for (int32_t k = -1; k <= 1; k++) {
                SU32(F, 0x24, U32(F, 0x18) + (uint32_t)i);
                SU32(F, 0x28, U32(F, 0x1c) + (uint32_t)j);
                SU32(F, 0x2c, U32(F, 0x20) + (uint32_t)k);
                const rx_addr v = rx_snake_hash_find(F + 0x24, h);
                if (v == 0) continue;
                for (rx_addr r = U32(v, 0); r != 0; r = U32(r, 8)) {
                    SU32(0, 0x00a942a0, U32(0, 0x00a942a0) + 1);
                    if (U32(r, 0) == id && U32(r, 4) + 0x19u > age) continue;
                    const uint32_t n = U32(r, 0x20);
                    uint32_t q = 0;
                    for (; q < n; q++) {
                        const rx_addr pl = r + 0x30 + 0x20 * q;
                        float d = rx_adds(rx_muls(F32(pl, 0), F32(pos, 0)), rx_muls(F32(pl, 8), F32(pos, 8)));
                        d = rx_subs(rx_adds(d, rx_muls(F32(pl, 4), F32(pos, 4))), F32(pl, 0x10));
                        if (d > radius) break;   /* comiss; ja: the next record (NaN inside) */
                    }
                    if (q >= n) { ACT_END(F); return r; }
                }
            }
    ACT_END(F);
    return 0;
}
RX_FUNCTION(0x0069bba0, rx_snake_hash_hit)

/* ======================================================================================================================== */
/* the shape's profile callbacks (cdecl buffer, v)                                                                           */

static inline float dataf(rx_addr a) { return F32(0, a); }   /* a writable .data constant (>= 0x9a6000), read from memory */

/* 0069bcf0: the 8 side points (0x30 each: position (u, v, 0), normal, +0x20 u, +0x24 v): the wall's two faces and its top; the texture
 * u = v * 0.1 wrapped by [0x9ddfdc] (16) */
void rx_snake_shape_side(rx_addr b, float v) {
    const float r = 0.1f /* 0x931d94 */, wrap = dataf(0x009ddfdc);
    float u = rx_muls(v, r);
    if (!(wrap >= u)) u = rx_subs(u, wrap);
    const float z = 0.0f, one = 1.0f /* 0x931f24 */, l = dataf(0x009cd9dc) /* -0.1 */, m1 = dataf(0x009cd9e8) /* -1 */;
    static const struct { uint8_t x, y, nx, ny, tv; } P[8] = {   /* 0 zero, 1 one, 2 l, 3 r, 4 m1 */
        {2, 0, 4, 0, 1}, {2, 1, 4, 0, 0}, {2, 1, 0, 1, 0}, {3, 1, 0, 1, 0}, {3, 1, 1, 0, 0}, {3, 0, 1, 0, 1}, {3, 0, 0, 4, 1}, {2, 0, 0, 4, 0},
    };
    const float val[5] = {z, one, l, r, m1};
    for (uint32_t i = 0; i < 8; i++) {
        const rx_addr p = b + 0x30 * i;
        SF32(p, 0, val[P[i].x]); SF32(p, 4, val[P[i].y]); SF32(p, 8, z);
        SF32(p, 0x10, val[P[i].nx]); SF32(p, 0x14, val[P[i].ny]); SF32(p, 0x18, z);
        SF32(p, 0x20, u); SF32(p, 0x24, val[P[i].tv]);
    }
}
RX_FUNCTION(0x0069bcf0, rx_snake_shape_side)

/* 0069bf00: the 4 cap points ((-0.1, 0), (-0.1, 1), (0.1, 1), (0.1, 0); normal 0; u -0.125 / 0.125 ([0x9c4754]), v 0 / 1; v unused) */
void rx_snake_shape_cap(rx_addr b, float v) {
    (void)v;
    const float l = dataf(0x009cd9dc), r = 0.1f, e = -0.125f /* 0x9776f4 */, f = dataf(0x009c4754);
    const float x[4] = {l, l, r, r}, y[4] = {0.0f, 1.0f, 1.0f, 0.0f}, u[4] = {e, e, f, f};
    for (uint32_t i = 0; i < 4; i++) {
        const rx_addr p = b + 0x30 * i;
        SF32(p, 0, x[i]); SF32(p, 4, y[i]); SF32(p, 8, 0.0f);
        SF32(p, 0x10, 0.0f); SF32(p, 0x14, 0.0f); SF32(p, 0x18, 0.0f);
        SF32(p, 0x20, u[i]); SF32(p, 0x24, y[i]);
    }
}
RX_FUNCTION(0x0069bf00, rx_snake_shape_cap)

/* 0069c000: the 4 outline points (2-D: (-0.1, 0), (-0.1, 1), (0.1, 1), (0.1, 0)) */
void rx_snake_shape_outline(rx_addr b, float v) {
    (void)v;
    const float l = dataf(0x009cd9dc), r = 0.1f;
    SF32(b, 0, l); SF32(b, 4, 0.0f); SF32(b, 8, l); SF32(b, 0xc, 1.0f); SF32(b, 0x10, r); SF32(b, 0x14, 1.0f); SF32(b, 0x18, r); SF32(b, 0x1c, 0.0f);
}
RX_FUNCTION(0x0069c000, rx_snake_shape_outline)

/* a call through one of the shape record's function pointers (the three above) */
static void shape_call(rx_addr fn, rx_addr buf, float v) {
    switch (fn) {
    case 0x0069bcf0: ACT_CALL(2, rx_snake_shape_side(buf, v)); break;
    case 0x0069bf00: ACT_CALL(2, rx_snake_shape_cap(buf, v)); break;
    case 0x0069c000: ACT_CALL(2, rx_snake_shape_outline(buf, v)); break;
    default: act_unreached(fn, "a trail shape other than 0x9308cc (the pointer [0x9cd974] has no writer)");
    }
}

/* ======================================================================================================================== */
/* the mesh's triangle ring                                                                                                  */

/* one ring vertex (0x28 bytes) from a shape point (0x30): position, normal, u, v (fld / fstp), +0x20 the distance w; the caps' u is
 * also w */
static void vtx(rx_addr d, rx_addr s, float w, int cap) {
    fcopy(d, 0, s, 0); fcopy(d, 4, s, 4); fcopy(d, 8, s, 8);
    fcopy(d, 0xc, s, 0x10); fcopy(d, 0x10, s, 0x14); fcopy(d, 0x14, s, 0x18);
    fcopy(d, 0x18, s, 0x20); fcopy(d, 0x1c, s, 0x24);
    if (cap) SF32(d, 0x18, w);
    SF32(d, 0x20, w);
}
static inline rx_addr tri(rx_addr m, uint32_t t) { return U32(m, 0xcc) + 0x78u * t; }
static inline void head_next(rx_addr m) {
    SU32(m, 0xbc, U32(m, 0xbc) + 1);
    SU32(m, 0xc4, U32(m, 0xc4) + 1);
    if (U32(m, 0xc4) >= RING) SU32(m, 0xc4, 0);
}

/* the cap triangles (shape +0x18, indices +0x1c) at the ring's head under the mesh's lock; 0 when the ring has no room */
static uint32_t cap_head(rx_addr m, rx_addr shape, rx_addr pts, float w, int reverse) {
    if (RING - U32(m, 0xbc) < U32(shape, 0x18)) return 0;
    os_cs(IAT_ENTER_CS, m + 0xd0);
    for (uint32_t i = 0; i < U32(shape, 0x18); i++) {
        const rx_addr ix = U32(shape, 0x1c) + 0xc * i, t = tri(m, U32(m, 0xc4));
        for (uint32_t c = 0; c < 3; c++) vtx(t + 0x28 * c, pts + 0x30 * U32(ix, 4 * (reverse ? 2 - c : c)), w, 1);
        head_next(m);
    }
    os_cs(IAT_LEAVE_CS, m + 0xd0);
    return 1;
}
/* 006a0e70 (ESI = mesh, EDI = shape; stdcall points, w; RET 8) -> AL: the front cap (in index order) */
uint32_t rx_snake_mesh_cap_front(rx_addr m, rx_addr shape, rx_addr pts, float w) { return cap_head(m, shape, pts, w, 0); }
RX_FUNCTION(0x006a0e70, rx_snake_mesh_cap_front)
/* 006a1030 (ESI = mesh, EDI = shape; stdcall points, w; RET 8) -> AL: the end cap (each triangle's first and last index swapped) */
uint32_t rx_snake_mesh_cap_end(rx_addr m, rx_addr shape, rx_addr pts, float w) { return cap_head(m, shape, pts, w, 1); }
RX_FUNCTION(0x006a1030, rx_snake_mesh_cap_end)

/* 006a1510 (ESI = mesh, EDI = shape; stdcall points, w; RET 8) -> AL: a front cap at the ring's tail (each triangle one before it) */
uint32_t rx_snake_mesh_cap_tail(rx_addr m, rx_addr shape, rx_addr pts, float w) {
    if (RING - U32(m, 0xbc) < U32(shape, 0x18)) return 0;
    os_cs(IAT_ENTER_CS, m + 0xd0);
    for (uint32_t i = 0; i < U32(shape, 0x18); i++) {
        SU32(m, 0xbc, U32(m, 0xbc) + 1);
        SU32(m, 0xc0, U32(m, 0xc0) == 0 ? RING - 1 : U32(m, 0xc0) - 1);
        const rx_addr ix = U32(shape, 0x1c) + 0xc * i, t = tri(m, U32(m, 0xc0));
        for (uint32_t c = 0; c < 3; c++) vtx(t + 0x28 * c, pts + 0x30 * U32(ix, 4 * c), w, 1);
    }
    os_cs(IAT_LEAVE_CS, m + 0xd0);
    return 1;
}
RX_FUNCTION(0x006a1510, rx_snake_mesh_cap_tail)

/* 006a11f0 (ESI = mesh, EDI = the previous section's points; stdcall shape, w0, the new points, w1; RET 0x10) -> AL: per side edge
 * (shape +8, pairs +0xc) two triangles (a0, a1, b0) and (b0, a1, b1) at the head; 0 when the ring has no room */
uint32_t rx_snake_mesh_sides(rx_addr m, rx_addr p0, rx_addr shape, float w0, rx_addr p1, float w1) {
    if (RING - U32(m, 0xbc) < 2u * U32(shape, 8)) return 0;
    os_cs(IAT_ENTER_CS, m + 0xd0);
    for (uint32_t i = 0; i < U32(shape, 8); i++) {
        const rx_addr e = U32(shape, 0xc) + 8 * i;
        const uint32_t a = 0x30 * U32(e, 0), b = 0x30 * U32(e, 4);
        rx_addr t = tri(m, U32(m, 0xc4));
        vtx(t, p0 + a, w0, 0); vtx(t + 0x28, p1 + a, w1, 0); vtx(t + 0x50, p0 + b, w0, 0);
        head_next(m);
        t = tri(m, U32(m, 0xc4));
        vtx(t, p0 + b, w0, 0); vtx(t + 0x28, p1 + a, w1, 0); vtx(t + 0x50, p1 + b, w1, 0);
        head_next(m);
    }
    os_cs(IAT_LEAVE_CS, m + 0xd0);
    return 1;
}
RX_FUNCTION(0x006a11f0, rx_snake_mesh_sides)

/* the last n triangles dropped from the head */
static void head_drop(rx_addr m, uint32_t n) {
    SU32(m, 0xbc, U32(m, 0xbc) - n);
    const uint32_t h = U32(m, 0xc4);
    SU32(m, 0xc4, h >= n ? h - n : h - n + RING);
}
/* the first n triangles dropped from the tail */
static void tail_drop(rx_addr m, uint32_t n) {
    SU32(m, 0xbc, U32(m, 0xbc) - n);
    SU32(m, 0xc0, U32(m, 0xc0) + n);
    while (U32(m, 0xc0) >= RING) SU32(m, 0xc0, U32(m, 0xc0) - RING);
}
/* 006a14e0 (ECX = mesh, EDX = shape): the front cap's triangles dropped from the head */
void rx_snake_mesh_uncap_head(rx_addr m, rx_addr shape) { head_drop(m, U32(shape, 0x18)); }
RX_FUNCTION(0x006a14e0, rx_snake_mesh_uncap_head)
/* 006a16c0 (EAX = mesh, ECX = shape): a cap's triangles dropped from the tail */
void rx_snake_mesh_drop_tail(rx_addr m, rx_addr shape) { tail_drop(m, U32(shape, 0x18)); }
RX_FUNCTION(0x006a16c0, rx_snake_mesh_drop_tail)
/* 006a1700 (EAX = mesh, ECX = shape): one section's side triangles (2 per edge) dropped from the tail */
void rx_snake_mesh_drop_sides(rx_addr m, rx_addr shape) { tail_drop(m, 2u * U32(shape, 8)); }
RX_FUNCTION(0x006a1700, rx_snake_mesh_drop_sides)

/* ======================================================================================================================== */
/* sections and collision records                                                                                            */

/* a shape point placed by a segment: (p + side * (u * width scale)) + up * (v * height) */
static void xform_point(rx_addr s, rx_addr pt) {
    const float us = rx_muls(F32(s, 0x40), F32(pt, 0)), vs = rx_muls(F32(pt, 4), F32(s, 0x44));
    const float x = rx_adds(rx_adds(F32(s, 0x10), rx_muls(F32(s, 0x20), us)), rx_muls(F32(s, 0x30), vs));
    const float y = rx_adds(rx_adds(F32(s, 0x14), rx_muls(F32(s, 0x24), us)), rx_muls(F32(s, 0x34), vs));
    const float z = rx_adds(rx_adds(F32(s, 0x18), rx_muls(F32(s, 0x28), us)), rx_muls(F32(s, 0x38), vs));
    SF32(pt, 0, x); SF32(pt, 4, y); SF32(pt, 8, z);
}
/* a shape point's normal placed: side * nx + up * ny */
static void xform_normal(rx_addr s, rx_addr pt) {
    const float nx = F32(pt, 0x10), ny = F32(pt, 0x14);
    for (uint32_t c = 0; c < 12; c += 4) SF32(pt, 0x10 + c, rx_adds(rx_muls(F32(s, 0x20 + c), nx), rx_muls(F32(s, 0x30 + c), ny)));
}
/* a segment's front face normal (up x side) and its end face normal (side x up), unnormalised */
static void normal_front(rx_addr s, float n[3]) {
    const float ax = F32(s, 0x20), ay = F32(s, 0x24), az = F32(s, 0x28), bx = F32(s, 0x30), by = F32(s, 0x34), bz = F32(s, 0x38);
    n[0] = rx_subs(rx_muls(az, by), rx_muls(ay, bz));
    n[1] = rx_subs(rx_muls(ax, bz), rx_muls(bx, az));
    n[2] = rx_subs(rx_muls(bx, ay), rx_muls(ax, by));
}
static void normal_end(rx_addr s, float n[3]) {
    const float ax = F32(s, 0x20), ay = F32(s, 0x24), az = F32(s, 0x28), bx = F32(s, 0x30), by = F32(s, 0x34), bz = F32(s, 0x38);
    n[0] = rx_subs(rx_muls(bz, ay), rx_muls(by, az));
    n[1] = rx_subs(rx_muls(bx, az), rx_muls(ax, bz));
    n[2] = rx_subs(rx_muls(ax, by), rx_muls(bx, ay));
}
/* a cap section in buf: the shape's cap points (its fn with v) placed by the segment, with the face normal */
static void cap_points(rx_addr shape, rx_addr s, rx_addr buf, float v, const float n[3]) {
    shape_call(U32(shape, 0x14), buf, v);
    for (uint32_t i = 0; i < U32(shape, 0x10); i++) {
        const rx_addr pt = buf + 0x30 * i;
        xform_point(s, pt);
        SF32(pt, 0x10, n[0]); SF32(pt, 0x14, n[1]); SF32(pt, 0x18, n[2]);
    }
}
/* |a - b| of two segments' positions as 006a0470 / 0069fcd0 sum it: sqrt((dy^2 + dz^2) + dx^2) */
static float seg_dist(rx_addr a, rx_addr b) {
    const float dy = rx_subs(F32(a, 0x14), F32(b, 0x14)), dz = rx_subs(F32(a, 0x18), F32(b, 0x18)), dx = rx_subs(F32(a, 0x10), F32(b, 0x10));
    return sqrt_call(rx_adds(rx_adds(rx_muls(dy, dy), rx_muls(dz, dz)), rx_muls(dx, dx)));
}
/* a list unlink of a record {+0 next, +4 prev} from a list {head, tail} */
static void list_unlink(rx_addr r, rx_addr head, rx_addr tail) {
    if (U32(r, 0) != 0) SU32(U32(r, 0), 4, U32(r, 4)); else SU32(tail, 0, U32(r, 4));
    if (U32(r, 4) != 0) SU32(U32(r, 4), 0, U32(r, 0)); else SU32(head, 0, U32(r, 0));
    SU32(r, 4, 0); SU32(r, 0, 0);
}

/* 0069b530 (ECX = c, EDX = d, EDI = q; stdcall p, out B; ESI = out A; RET 8): the two planes of the quad p (the previous section's
 * outline point i), c (its point j), d (the new section's point j), q (the new point i): A = (p - c) x (d - c), B = (d - q) x (p - q);
 * when they disagree (A . B < 0) the other diagonal: A = (q - p) x (c - p), B = (c - d) x (q - d); each normalised when its length^2
 * is > 0; d_A = A . p, d_B = B . q. Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x1c; push ebx. */
void rx_snake_quad_planes(rx_addr c, rx_addr d, rx_addr q, rx_addr p, rx_addr ob, rx_addr oa) {
    ACT_FRAME_EBP(F, 16, 0x1c, 1);
    {   const float ux = rx_subs(F32(d, 0), F32(c, 0)), uy = rx_subs(F32(d, 4), F32(c, 4)), uz = rx_subs(F32(d, 8), F32(c, 8));
        const float wx = rx_subs(F32(p, 0), F32(c, 0)), wy = rx_subs(F32(p, 4), F32(c, 4)), wz = rx_subs(F32(p, 8), F32(c, 8));
        SF32(oa, 0, rx_subs(rx_muls(wy, uz), rx_muls(wz, uy)));
        SF32(oa, 8, rx_subs(rx_muls(wx, uy), rx_muls(wy, ux)));
        SF32(oa, 4, rx_subs(rx_muls(wz, ux), rx_muls(wx, uz))); }
    {   const float sx = rx_subs(F32(p, 0), F32(q, 0)), sy = rx_subs(F32(p, 4), F32(q, 4)), sz = rx_subs(F32(p, 8), F32(q, 8));
        const float tx = rx_subs(F32(d, 0), F32(q, 0)), ty = rx_subs(F32(d, 4), F32(q, 4)), tz = rx_subs(F32(d, 8), F32(q, 8));
        SF32(ob, 0, rx_subs(rx_muls(ty, sz), rx_muls(tz, sy)));
        SF32(ob, 4, rx_subs(rx_muls(tz, sx), rx_muls(sz, tx)));
        SF32(ob, 8, rx_subs(rx_muls(sy, tx), rx_muls(ty, sx))); }
    const float dot = rx_adds(rx_adds(rx_muls(F32(oa, 8), F32(ob, 8)), rx_muls(F32(oa, 0), F32(ob, 0))), rx_muls(F32(oa, 4), F32(ob, 4)));
    if (0.0f > dot) {
        {   const float qx = rx_subs(F32(q, 0), F32(p, 0)), qy = rx_subs(F32(q, 4), F32(p, 4)), qz = rx_subs(F32(q, 8), F32(p, 8));
            const float cx = rx_subs(F32(c, 0), F32(p, 0)), cy = rx_subs(F32(c, 4), F32(p, 4)), cz = rx_subs(F32(c, 8), F32(p, 8));
            SF32(oa, 0, rx_subs(rx_muls(qy, cz), rx_muls(qz, cy)));
            SF32(oa, 4, rx_subs(rx_muls(qz, cx), rx_muls(cz, qx)));
            SF32(oa, 8, rx_subs(rx_muls(cy, qx), rx_muls(qy, cx))); }
        {   const float ex = rx_subs(F32(c, 0), F32(d, 0)), ey = rx_subs(F32(c, 4), F32(d, 4)), ez = rx_subs(F32(c, 8), F32(d, 8));
            const float fx = rx_subs(F32(q, 0), F32(d, 0)), fy = rx_subs(F32(q, 4), F32(d, 4)), fz = rx_subs(F32(q, 8), F32(d, 8));
            SF32(ob, 0, rx_subs(rx_muls(ey, fz), rx_muls(ez, fy)));
            SF32(ob, 4, rx_subs(rx_muls(ez, fx), rx_muls(fz, ex)));
            SF32(ob, 8, rx_subs(rx_muls(fy, ex), rx_muls(ey, fx))); }
    }
    for (int k = 0; k < 2; k++) {
        const rx_addr o = k ? ob : oa;
        const float x = F32(o, 0), y = F32(o, 4), z = F32(o, 8);
        const float l2 = rx_adds(rx_adds(rx_muls(x, x), rx_muls(y, y)), rx_muls(z, z));
        if (l2 > 0.0f) {
            const float inv = rx_divs(1.0f, sqrt_call(l2));
            SF32(o, 0, rx_muls(x, inv)); SF32(o, 4, rx_muls(y, inv)); SF32(o, 8, rx_muls(z, inv));
        }
    }
    SF32(oa, 0x10, rx_adds(rx_adds(rx_muls(F32(p, 0), F32(oa, 0)), rx_muls(F32(p, 8), F32(oa, 8))), rx_muls(F32(p, 4), F32(oa, 4))));
    SF32(ob, 0x10, rx_adds(rx_adds(rx_muls(F32(q, 8), F32(ob, 8)), rx_muls(F32(q, 4), F32(ob, 4))), rx_muls(F32(q, 0), F32(ob, 0))));
    ACT_END(F);
}
RX_FUNCTION(0x0069b530, rx_snake_quad_planes)

/* 0069b450 (ECX = the new end plane, EDX = the previous end plane; stdcall out, id, counter, midpoint, shape, the previous outline, the
 * new outline; RET 0x1c) -> EAX = out: a collision record {id, counter, links 0, the midpoint, 2 + 2 * outline planes: EDX's at +0x30,
 * ECX's at +0x50 (normal, d at +0x10), then per outline edge (i, i + 1) the quad's two planes (0069b530)}. Frame: push ebx / ebp /
 * esi / edi. */
rx_addr rx_snake_record_build(rx_addr pn, rx_addr pp, rx_addr out, uint32_t id, uint32_t counter, rx_addr mid, rx_addr shape, rx_addr o0, rx_addr o1) {
    ACT_FRAME(F, 0, 4);
    SU32(out, 0, id);
    SU32(out, 4, counter);
    SU32(out, 8, 0);
    SU32(out, 0xc, 0);
    fcopy(out, 0x10, mid, 0); fcopy(out, 0x14, mid, 4); fcopy(out, 0x18, mid, 8);
    fcopy(out, 0x30, pp, 0); fcopy(out, 0x34, pp, 4); fcopy(out, 0x38, pp, 8); fcopy(out, 0x40, pp, 0x10);
    fcopy(out, 0x50, pn, 0); fcopy(out, 0x54, pn, 4); fcopy(out, 0x58, pn, 8); fcopy(out, 0x60, pn, 0x10);
    const uint32_t n = U32(shape, 0x20);
    for (uint32_t i = 0; i < n; i++) {
        const uint32_t j = (i + 1) % n;
        const rx_addr pl = out + 0x70 + 0x40 * i;
        ACT_CALL(2, rx_snake_quad_planes(o0 + 0x10 * j, o1 + 0x10 * j, o1 + 0x10 * i, o0 + 0x10 * i, pl + 0x20, pl));
    }
    SU32(out, 0x20, 2u * n + 2u);
    ACT_END(F);
    return out;
}
RX_FUNCTION(0x0069b450, rx_snake_record_build)

/* 0069fb00 (ECX = the record built, EAX = the segment's record (+0x50)): the ints and the counts copied, the midpoint and the 10 planes
 * (normal, d) through fld / fstp */
void rx_snake_record_copy(rx_addr src, rx_addr dst) {
    for (uint32_t k = 0; k < 0x10; k += 4) SU32(dst, k, U32(src, k));
    fcopy(dst, 0x10, src, 0x10); fcopy(dst, 0x14, src, 0x14); fcopy(dst, 0x18, src, 0x18);
    SU32(dst, 0x20, U32(src, 0x20));
    for (uint32_t p = 0x30; p < 0x170; p += 0x20) { fcopy(dst, p, src, p); fcopy(dst, p + 4, src, p + 4); fcopy(dst, p + 8, src, p + 8); fcopy(dst, p + 0x10, src, p + 0x10); }
}
RX_FUNCTION(0x0069fb00, rx_snake_record_copy)

/* ======================================================================================================================== */
/* the trail                                                                                                                 */

/* 006a0470 (ECX = trail): the oldest segment of the oldest chain freed: with a successor its slab leaves the hash and the mesh (the
 * front cap and the sides at the ring's tail; the successor's own front cap at the tail when it is not the last: 006a1510, closing the
 * newest segment (0069fcd0) while the ring is full) and the lengths shrink by its length; an emptied chain goes back to the pool.
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc54; push ebx / esi / edi (the cap points F + 0x60). */
void rx_snake_trail_free_oldest(rx_addr t) {
    ACT_FRAME_EBP(F, 16, 0xc54, 3);
    const rx_addr chain = U32(t, 0x11c);
    if (chain == 0) { ACT_END(F); return; }
    const rx_addr seg = U32(chain, 8);
    if (seg != 0) {
        const rx_addr next = U32(seg, 0);
        if (next != 0) {
            const float d = seg_dist(next, seg);
            SF32(chain, 0x40, rx_subs(F32(chain, 0x40), d));
            SF32(t, 0x138, rx_subs(F32(t, 0x138), d));
            ACT_CALL(2, rx_snake_hash_del(U32(t, 0), next + 0x50));
            const rx_addr sh = U32(chain, 0x44), m = t + 0x10;
            rx_snake_mesh_drop_tail(m, sh);
            rx_snake_mesh_drop_sides(m, sh);
            if (U32(next, 0) == 0) rx_snake_mesh_drop_tail(m, sh);
            else {
                float n[3];
                normal_front(next, n);
                cap_points(sh, next, F + 0x60, x87_copy(F32(next, 0x48)), n);
                for (;;) {
                    uint32_t ok;
                    ACT_CALLR(ok, 2, rx_snake_mesh_cap_tail(m, sh, F + 0x60, x87_copy(F32(seg, 0x48))));
                    if (ok & 0xff) break;
                    ACT_CALL(1, rx_snake_trail_close(t));
                }
            }
        }
        list_unlink(seg, chain + 8, chain + 0xc);
        SU32(chain, 0x10, U32(chain, 0x10) - 1);
        pool_put(t + 0x108, seg);
    }
    if (U32(chain, 8) == 0) {
        list_unlink(chain, t + 0x11c, t + 0x120);
        pool_put(t + 0x124, chain);
    }
    ACT_END(F);
}
RX_FUNCTION(0x006a0470, rx_snake_trail_free_oldest)

/* 0069fcd0 (stdcall trail; RET 4): the newest chain's newest segment taken back (emptied chains at the tail returned to the pool
 * first): its slab leaves the hash, the lengths shrink, its end cap and sides leave the ring's head, and the previous segment gets the
 * end cap (or, when it is the chain's first, loses its front cap). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0xc74;
 * push ebx / esi / edi (the cap points F + 0x80). */
void rx_snake_trail_close(rx_addr t) {
    ACT_FRAME_EBP(F, 16, 0xc74, 3);
    rx_addr chain;
    for (;;) {
        chain = U32(t, 0x120);
        if (chain == 0) { ACT_END(F); return; }
        if (U32(chain, 0x10) != 0) break;
        list_unlink(chain, t + 0x11c, t + 0x120);
        pool_put(t + 0x124, chain);
    }
    const rx_addr last = U32(chain, 0xc), prev = U32(last, 4);
    if (prev != 0) {
        const float d = seg_dist(last, prev);
        SF32(chain, 0x40, rx_subs(F32(chain, 0x40), d));
        SF32(t, 0x138, rx_subs(F32(t, 0x138), d));
        const float v = rx_subs(F32(t, 0x13c), d);
        SF32(t, 0x13c, v);
        SF32(t, 0xc8, v);
        ACT_CALL(2, rx_snake_hash_del(U32(t, 0), last + 0x50));
        const rx_addr sh = U32(chain, 0x44), m = t + 0x10;
        head_drop(m, U32(sh, 0x18));
        head_drop(m, 2u * U32(sh, 8));
        if (U32(prev, 4) == 0) ACT_CALL(0, rx_snake_mesh_uncap_head(m, sh));
        else {
            float n[3];
            normal_end(prev, n);
            cap_points(sh, prev, F + 0x80, x87_copy(F32(prev, 0x48)), n);
            for (;;) {
                uint32_t ok;
                ACT_CALLR(ok, 2, rx_snake_mesh_cap_end(m, sh, F + 0x80, x87_copy(F32(last, 0x48))));
                if (ok & 0xff) break;
                ACT_CALL(0, rx_snake_trail_free_oldest(t));
            }
        }
    }
    list_unlink(last, chain + 8, chain + 0xc);
    SU32(chain, 0x10, U32(chain, 0x10) - 1);
    SU32(t, 0x140, U32(t, 0x140) - 1);
    pool_put(t + 0x108, last);
    ACT_END(F);
}
RX_FUNCTION(0x0069fcd0, rx_snake_trail_close)

/* 0069ed90 (ECX = the up axis; stdcall trail, position, side axis, width scale, height; RET 0x14): a segment appended to the newest
 * chain (the oldest freed first when the pool is empty) with the distance and counter; after a previous segment: the lengths grow by
 * the step, the slab's collision record (its end planes: the previous segment's front face, the new one's end face; its outline quads)
 * goes into the segment and the hash, and the mesh gets the previous segment's front cap (the chain's first) or loses its end cap, the
 * sides between the two and the new end cap (each retried after freeing the oldest segment while the ring is full).
 * Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x1b04; push ebx / esi / edi: F + 0x70 / + 0xc0 the previous front normal
 * (d at + 0xd0), F + 0x80 / + 0xa0 the new end normal (d at + 0xb0), F + 0x90 the midpoint, F + 0xe0 / + 0x100 the new / previous 2-D
 * outline, F + 0x120 / + 0x160 the previous / new placed outline, F + 0x1a0 and + 0xda0 the sections, F + 0x19a0 the record. */
void rx_snake_trail_append(rx_addr up, rx_addr t, rx_addr pos, rx_addr side, float sx, float sy) {
    ACT_FRAME_EBP(F, 16, 0x1b04, 3);
    if (U32(t, 0x118) == 0) ACT_CALL(0, rx_snake_trail_free_oldest(t));
    rx_addr s;
    ACT_CALLR(s, 0, rx_snake_segment_take(t + 0x108));
    fcopy(s, 0x10, pos, 0); fcopy(s, 0x14, pos, 4); fcopy(s, 0x18, pos, 8);
    fcopy(s, 0x20, side, 0); fcopy(s, 0x24, side, 4); fcopy(s, 0x28, side, 8);
    fcopy(s, 0x30, up, 0); fcopy(s, 0x34, up, 4); fcopy(s, 0x38, up, 8);
    SF32(s, 0x40, sx);
    SF32(s, 0x44, sy);
    fcopy(s, 0x48, t, 0x13c);
    SU32(s, 0x4c, U32(t, 0x140));
    fcopy(t, 0xc8, t, 0x13c);
    const rx_addr chain = U32(t, 0x120);
    if (U32(chain, 0xc) == 0) { SU32(chain, 8, s); SU32(chain, 0xc, s); SU32(s, 4, 0); }
    else { SU32(U32(chain, 0xc), 0, s); SU32(s, 4, U32(chain, 0xc)); SU32(chain, 0xc, s); }
    SU32(s, 0, 0);
    SU32(chain, 0x10, U32(chain, 0x10) + 1);
    SU32(t, 0x140, U32(t, 0x140) + 1);
    const rx_addr prev = U32(s, 4);
    if (prev == 0) { ACT_END(F); return; }
    {   const float dx = rx_subs(F32(s, 0x10), F32(prev, 0x10)), dz = rx_subs(F32(s, 0x18), F32(prev, 0x18)), dy = rx_subs(F32(s, 0x14), F32(prev, 0x14));
        const float d = sqrt_call(rx_adds(rx_adds(rx_muls(dx, dx), rx_muls(dz, dz)), rx_muls(dy, dy)));
        SF32(chain, 0x40, rx_adds(F32(chain, 0x40), d));
        SF32(t, 0x138, rx_adds(F32(t, 0x138), d));
        const float v = rx_adds(F32(t, 0x13c), d);
        SF32(t, 0x13c, v); SF32(t, 0xc8, v); SF32(s, 0x48, v); }
    float na[3], nb[3];
    normal_front(prev, na);
    normal_end(s, nb);
    const float da = rx_adds(rx_adds(rx_muls(F32(prev, 0x18), na[2]), rx_muls(F32(prev, 0x14), na[1])), rx_muls(F32(prev, 0x10), na[0]));
    const float db = rx_adds(rx_adds(rx_muls(F32(s, 0x18), nb[2]), rx_muls(F32(s, 0x14), nb[1])), rx_muls(F32(s, 0x10), nb[0]));
    for (uint32_t c = 0; c < 3; c++) {
        SF32(F, 0x70 + 4 * c, na[c]); SF32(F, 0xc0 + 4 * c, na[c]);
        SF32(F, 0x80 + 4 * c, nb[c]); SF32(F, 0xa0 + 4 * c, nb[c]);
    }
    SF32(F, 0xd0, da);
    SF32(F, 0xb0, db);
    const rx_addr sh = U32(chain, 0x44), m = t + 0x10;
    SU32(F, 0xc, sh);
    shape_call(U32(sh, 0x24), F + 0x100, x87_copy(F32(prev, 0x48)));
    shape_call(U32(sh, 0x24), F + 0xe0, x87_copy(F32(s, 0x48)));
    for (uint32_t i = 0; i < U32(sh, 0x20); i++) {   /* the outlines placed: (p + side * (sx * u)) + up * (v * sy) */
        for (int k = 0; k < 2; k++) {
            const rx_addr g = k ? s : prev, src = F + (k ? 0xe0 : 0x100) + 8 * i, dst = F + (k ? 0x160 : 0x120) + 0x10 * i;
            const float us = rx_muls(F32(g, 0x40), F32(src, 0)), vs = rx_muls(F32(src, 4), F32(g, 0x44));
            for (uint32_t c = 0; c < 12; c += 4)
                SF32(dst, c, rx_adds(rx_adds(F32(g, 0x10 + c), rx_muls(F32(g, 0x20 + c), us)), rx_muls(F32(g, 0x30 + c), vs)));
        }
    }
    for (uint32_t c = 0; c < 12; c += 4) SF32(F, 0x90 + c, rx_muls(rx_adds(F32(prev, 0x10 + c), F32(s, 0x10 + c)), 0.5f /* 0x9322cc */));
    rx_addr rec;
    ACT_CALLR(rec, 7, rx_snake_record_build(F + 0xa0, F + 0xc0, F + 0x19a0, U32(t, 0x104), U32(s, 0x4c), F + 0x90, sh, F + 0x120, F + 0x160));
    rx_snake_record_copy(rec, s + 0x50);
    ACT_CALL(2, rx_snake_hash_add(U32(t, 0), s + 0x50));
    if (U32(prev, 4) == 0) {   /* the chain's first slab: the previous segment's front cap */
        cap_points(sh, prev, F + 0x1a0, x87_copy(F32(prev, 0x48)), na);
        for (;;) {
            uint32_t ok;
            ACT_CALLR(ok, 2, rx_snake_mesh_cap_front(m, sh, F + 0x1a0, x87_copy(F32(prev, 0x48))));
            if (ok & 0xff) break;
            ACT_CALL(0, rx_snake_trail_free_oldest(t));
        }
    } else head_drop(m, U32(sh, 0x18));   /* the previous end cap */
    shape_call(U32(sh, 4), F + 0x1a0, x87_copy(F32(prev, 0x48)));
    shape_call(U32(sh, 4), F + 0xda0, x87_copy(F32(s, 0x48)));
    for (uint32_t i = 0; i < U32(sh, 0); i++) {
        xform_point(prev, F + 0x1a0 + 0x30 * i); xform_normal(prev, F + 0x1a0 + 0x30 * i);
        xform_point(s, F + 0xda0 + 0x30 * i); xform_normal(s, F + 0xda0 + 0x30 * i);
    }
    for (;;) {
        uint32_t ok;
        ACT_CALLR(ok, 4, rx_snake_mesh_sides(m, F + 0x1a0, sh, x87_copy(F32(prev, 0x48)), F + 0xda0, x87_copy(F32(s, 0x48))));
        if (ok & 0xff) break;
        ACT_CALL(0, rx_snake_trail_free_oldest(t));
    }
    cap_points(sh, s, F + 0x1a0, x87_copy(F32(s, 0x48)), nb);
    for (;;) {
        uint32_t ok;
        ACT_CALLR(ok, 2, rx_snake_mesh_cap_end(m, sh, F + 0x1a0, x87_copy(F32(s, 0x48))));
        if (ok & 0xff) break;
        ACT_CALL(0, rx_snake_trail_free_oldest(t));
    }
    ACT_END(F);
}
RX_FUNCTION(0x0069ed90, rx_snake_trail_append)

/* 0069ed10 (ESI = trail): a new chain at the list's tail (the oldest chain freed first, segment by segment, when the pool is empty):
 * no segments, length 0, the trail's shape. Frame: push edi. */
void rx_snake_trail_new_chain(rx_addr t) {
    ACT_FRAME(F, 0, 1);
    if (U32(t, 0x134) == 0 && U32(t, 0x11c) != 0) {
        const rx_addr h = U32(t, 0x11c);
        do ACT_CALL(0, rx_snake_trail_free_oldest(t)); while (h == U32(t, 0x11c));
    }
    rx_addr c;
    ACT_CALLR(c, 0, rx_snake_chain_take(t + 0x124));
    SU32(c, 0x10, 0);
    SF32(c, 0x40, 0.0f);
    SU32(c, 0x44, U32(t, 0x100));
    if (U32(t, 0x120) == 0) { SU32(t, 0x11c, c); SU32(t, 0x120, c); SU32(c, 4, 0); }
    else { SU32(U32(t, 0x120), 0, c); SU32(c, 4, U32(t, 0x120)); SU32(t, 0x120, c); }
    SU32(c, 0, 0);
    ACT_END(F);
}
RX_FUNCTION(0x0069ed10, rx_snake_trail_new_chain)

/* 0069e4c0 (EAX = the vehicle's matrix; stdcall trail, back offset, width scale, height; RET 0x10): the trail follows the rider: the up
 * axis is the matrix's +0x10 row normalised with y made non-negative; the newest chain's first segment goes at the rider; its second
 * turns the first to face it and lays one at the rider moved back by the offset; then a segment is laid at the moved point once it is
 * 0.01 from the newest, replacing the newest instead when that one is under 2 ft from its predecessor, or when the direction is within
 * 1 degree of the chain's last and the point within the merge distance (shape +0x28) of its anchor (where the direction and the anchor
 * are updated otherwise). Frame: push ebp; mov ebp, esp; and esp, -16; sub esp, 0x64; push ebx / esi / edi: F + 0x20 the up axis,
 * F + 0x30 the position, F + 0x40 the step direction, F + 0x50 / + 0x60 the segment's position / side axis. With two or more segments
 * and the rider exactly on the newest, the original moves it back along F + 0x40 .. 0x48 as left on the stack (read here as is). */
void rx_snake_trail_update(rx_addr mat, rx_addr t, float a, float b, float c) {
    ACT_FRAME_EBP(F, 16, 0x64, 3);
    float fx = F32(mat, 0x10), fy = F32(mat, 0x14), fz = F32(mat, 0x18);
    float px = F32(mat, 0x30), py = F32(mat, 0x34), pz = F32(mat, 0x38);
    SF32(F, 0x30, px); SF32(F, 0x34, py); SF32(F, 0x38, pz);
    const float l2 = rx_adds(rx_adds(rx_muls(fx, fx), rx_muls(fy, fy)), rx_muls(fz, fz));
    if (l2 > 0.0f) {
        const float inv = rx_divs(1.0f, sqrt_call(x87_copy(l2)));
        fx = rx_muls(fx, inv); fy = rx_muls(inv, fy); fz = rx_muls(fz, inv);
    }
    SF32(F, 0x20, fx); SF32(F, 0x24, fy); SF32(F, 0x28, fz);
    if (0.0f > fy) SF32(F, 0x24, rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, fy));
    const float uy = F32(F, 0x24);
    const rx_addr chain = U32(t, 0x120);
    const uint32_t count = U32(chain, 0x10);
    if (count == 0) {
        SF32(F, 0x50, 1.0f); SF32(F, 0x54, 0.0f); SF32(F, 0x58, 0.0f);
        ACT_CALL(5, rx_snake_trail_append(F + 0x20, t, F + 0x30, F + 0x50, x87_copy(b), x87_copy(c)));
        ACT_END(F);
        return;
    }
    if (count == 1) {
        const rx_addr s = U32(chain, 8);
        float dy = rx_subs(py, F32(s, 0x14)), dz = rx_subs(pz, F32(s, 0x18)), dx = rx_subs(px, F32(s, 0x10));
        const float d2 = rx_adds(rx_adds(rx_muls(dy, dy), rx_muls(dx, dx)), rx_muls(dz, dz));
        if (0.25f /* 0x92ff84 */ > d2) { ACT_END(F); return; }
        if (d2 > 0.0f) {
            const float inv = rx_divs(1.0f, sqrt_call(x87_copy(d2)));
            dx = rx_muls(dx, inv); dy = rx_muls(dy, inv); dz = rx_muls(dz, inv);
        }
        SF32(F, 0x40, dx); SF32(F, 0x44, dy); SF32(F, 0x48, dz);
        {   const float bx = F32(s, 0x30), by = F32(s, 0x34), bz = F32(s, 0x38);
            const float sx0 = rx_subs(rx_muls(by, dz), rx_muls(bz, dy));
            const float sy0 = rx_subs(rx_muls(bz, dx), rx_muls(bx, dz));
            const float sz0 = rx_subs(rx_muls(bx, dy), rx_muls(by, dx));
            SF32(s, 0x28, sz0); SF32(s, 0x20, sx0); SF32(s, 0x24, sy0); }
        const float ax = rx_muls(dx, a), ay = rx_muls(dy, a), az = rx_muls(dz, a);
        SF32(s, 0x10, rx_subs(F32(s, 0x10), ax));
        SF32(s, 0x14, rx_subs(F32(s, 0x14), ay));
        SF32(s, 0x18, rx_subs(F32(s, 0x18), az));
        SF32(F, 0x60, rx_subs(rx_muls(dz, uy), rx_muls(dy, fz)));
        SF32(F, 0x64, rx_subs(rx_muls(fz, dx), rx_muls(dz, fx)));
        SF32(F, 0x68, rx_subs(rx_muls(dy, fx), rx_muls(dx, uy)));
        SF32(F, 0x50, rx_subs(px, ax)); SF32(F, 0x54, rx_subs(py, ay)); SF32(F, 0x58, rx_subs(pz, az));
        ACT_CALL(5, rx_snake_trail_append(F + 0x20, t, F + 0x50, F + 0x60, x87_copy(b), x87_copy(c)));
        for (uint32_t k = 0; k < 12; k += 4) { SU32(chain, 0x20 + k, U32(F, 0x40 + k)); SU32(chain, 0x30 + k, U32(F, 0x50 + k)); }
        ACT_END(F);
        return;
    }
    const rx_addr last = U32(chain, 0xc);
    {   const float dx = rx_subs(px, F32(last, 0x10)), dy = rx_subs(py, F32(last, 0x14)), dz = rx_subs(pz, F32(last, 0x18));
        SF32(F, 0x50, dx); SF32(F, 0x54, dy); SF32(F, 0x58, dz);
        const float d2 = rx_adds(rx_adds(rx_muls(dy, dy), rx_muls(dx, dx)), rx_muls(dz, dz));
        float ux, uy2, uz;
        if (d2 > 0.0f) {
            const float inv = rx_divs(1.0f, sqrt_call(x87_copy(d2)));
            ux = rx_muls(dx, inv); uy2 = rx_muls(dy, inv); uz = rx_muls(dz, inv);
        } else { ux = F32(F, 0x40); uy2 = F32(F, 0x44); uz = F32(F, 0x48); }
        pz = rx_subs(pz, rx_muls(uz, a)); py = rx_subs(py, rx_muls(uy2, a)); px = rx_subs(px, rx_muls(ux, a)); }
    const float ex = rx_subs(px, F32(last, 0x10)), ey = rx_subs(py, F32(last, 0x14)), ez = rx_subs(pz, F32(last, 0x18));
    SF32(F, 0x30, px); SF32(F, 0x34, py); SF32(F, 0x38, pz);
    const float len = sqrt_call(rx_adds(rx_adds(rx_muls(ex, ex), rx_muls(ey, ey)), rx_muls(ez, ez)));
    if (0.01f /* 0x9322ac */ > len) { ACT_END(F); return; }
    const float ux = rx_divs(ex, len), vy = rx_divs(ey, len), uz = rx_divs(ez, len);
    SF32(F, 0x40, ux); SF32(F, 0x44, vy); SF32(F, 0x48, uz);
    SF32(F, 0x50, rx_subs(rx_muls(uz, uy), rx_muls(vy, fz)));
    SF32(F, 0x58, rx_subs(rx_muls(vy, fx), rx_muls(ux, uy)));
    SF32(F, 0x54, rx_subs(rx_muls(fz, ux), rx_muls(uz, fx)));
    const rx_addr lp = U32(last, 4);
    const float gx = rx_subs(F32(last, 0x10), F32(lp, 0x10)), gy = rx_subs(F32(last, 0x14), F32(lp, 0x14)), gz = rx_subs(F32(last, 0x18), F32(lp, 0x18));
    const float g2 = rx_adds(rx_adds(rx_muls(gx, gx), rx_muls(gy, gy)), rx_muls(gz, gz));
    if (4.0f /* 0x9322c0 */ > g2) {
        ACT_CALL(1, rx_snake_trail_close(t));
        ACT_CALL(5, rx_snake_trail_append(F + 0x20, t, F + 0x30, F + 0x50, x87_copy(b), x87_copy(c)));
        ACT_END(F);
        return;
    }
    float ang;
    ACT_CALLR(ang, 0, rx_world_float_abs(F32(0, 0x00977678) /* 1 degree */));
    const float tr = b1_trig_reduce(ang), t2 = rx_muls(tr, tr), t4 = rx_muls(t2, t2);
    const float cosv = rx_subs(rx_bf(0x80000000u) /* -0.0, 0x9774a8 */, b1_cos_poly(t2, t4));
    const float dot = rx_adds(rx_adds(rx_muls(F32(chain, 0x24), vy), rx_muls(F32(chain, 0x20), ux)), rx_muls(F32(chain, 0x28), uz));
    if (dot > cosv) {
        const float qx = rx_subs(px, F32(chain, 0x30)), qy = rx_subs(py, F32(chain, 0x34)), qz = rx_subs(pz, F32(chain, 0x38));
        const float q2 = rx_adds(rx_adds(rx_muls(qx, qx), rx_muls(qy, qy)), rx_muls(qz, qz));
        const float r = F32(U32(chain, 0x44), 0x28);
        if (rx_muls(r, r) > q2) {
            ACT_CALL(1, rx_snake_trail_close(t));
            ACT_CALL(5, rx_snake_trail_append(F + 0x20, t, F + 0x30, F + 0x50, x87_copy(b), x87_copy(c)));
            ACT_END(F);
            return;
        }
    }
    ACT_CALL(5, rx_snake_trail_append(F + 0x20, t, F + 0x30, F + 0x50, x87_copy(b), x87_copy(c)));
    for (uint32_t k = 0; k < 12; k += 4) { SU32(chain, 0x20 + k, U32(F, 0x40 + k)); SU32(chain, 0x30 + k, U32(F, 0x30 + k)); }
    ACT_END(F);
}
RX_FUNCTION(0x0069e4c0, rx_snake_trail_update)

/* 0069e360 (EAX = trail): the rider's trail broken (a crash, a respawn): an empty newest chain takes the trail's shape again; one with a
 * single segment loses it (0069fcd0) and takes the shape; a longer one is ended by a new chain (0069ed10). Frame: push ecx / esi / edi. */
void rx_snake_trail_refresh(rx_addr t) {
    ACT_FRAME(F, 0, 3);
    const rx_addr c = U32(t, 0x120);
    const uint32_t n = U32(c, 0x10);
    if (n == 0) SU32(c, 0x44, U32(t, 0x100));
    else if (n == 1) { ACT_CALL(1, rx_snake_trail_close(t)); SU32(c, 0x44, U32(t, 0x100)); }
    else ACT_CALL(0, rx_snake_trail_new_chain(t));
    ACT_END(F);
}
RX_FUNCTION(0x0069e360, rx_snake_trail_refresh)

/* 0069ec90 (EAX = trail): every chain freed (segment by segment, 006a0470), a new empty chain, the ring and the distance 0.
 * Frame: push ecx / esi / edi. */
void rx_snake_trail_reset(rx_addr t) {
    ACT_FRAME(F, 0, 3);
    while (U32(t, 0x11c) != 0) {
        const rx_addr h = U32(t, 0x11c);
        do ACT_CALL(0, rx_snake_trail_free_oldest(t)); while (h == U32(t, 0x11c));
    }
    ACT_CALL(0, rx_snake_trail_new_chain(t));
    SU32(t, 0xd4, 0); SU32(t, 0xd0, 0); SU32(t, 0xcc, 0);
    SF32(t, 0x13c, 0.0f);
    SF32(t, 0xc8, 0.0f);
    ACT_END(F);
}
RX_FUNCTION(0x0069ec90, rx_snake_trail_reset)

/* ======================================================================================================================== */
/* XenonSnakeVisibleMesh                                                                                                     */

/* 006a0b40 (stdcall mesh, id; EAX = colour; RET 8) -> mesh: the Entity base (identity matrix 0x9cb510), the vtables, +0xa0 the colour,
 * +0xb0 / +0xb4 0, the ring empty, +0xc8 the owner id, the lock (+0xd0), the vertex ring (+0xcc: 24000 x 0x28 bytes), +0x9c 0.
 * Frame: push ebx / ebp / esi / edi. */
rx_addr rx_snake_mesh_construct(rx_addr m, uint32_t id, rx_addr color) {
    ACT_FRAME(F, 0, 4);
    SU32(m, 0, VT_ENTITY);
    SU32(m, 0x18, 0); SU32(m, 0x10, 0);
    SF32(m, 0x24, 0.0f); SF32(m, 0x20, 0.0f);
    SU32(m, 0x70, 0);
    for (uint32_t k = 0; k < 0x40; k += 4) SU32(m + 0x30, k, U32(0, 0x009cb510 + k));
    SU32(m, 0x1c, 0); SU32(m, 0x7c, 0);
    SF32(m, 0x78, 0.0f);
    SU32(m, 0x74, 0); SU32(m, 0x28, 0); SU32(m, 0x2c, 0);
    SU32(m, 0x90, VT_RENDERABLE);
    SU32(m, 0x94, 0xf0);
    SU32(m, 0x98, VT_RENDEREVT);
    SU32(m, 0, VT_MESH);
    SU32(m, 0x90, VT_MESH_O90);
    SU32(m, 0x98, VT_MESH_O98);
    fcopy(m, 0xa0, color, 0); fcopy(m, 0xa4, color, 4); fcopy(m, 0xa8, color, 8);
    SF32(m, 0xb0, 0.0f); SF32(m, 0xb4, 0.0f);
    SU32(m, 0xbc, 0); SU32(m, 0xc0, 0); SU32(m, 0xc4, 0);
    SU32(m, 0xc8, id);
    rx_set_sp(F - 4);
    os_cs(IAT_INIT_CS, m + 0xd0);
    rx_set_sp(F);
    rx_addr v;
    ACT_CALLR(v, 1, rx_reflex_aligned_alloc(3u * RING * 0x28u));
    SU32(m, 0xcc, v);
    SU32(m, 0x9c, 0);
    ACT_END(F);
    return m;
}
RX_FUNCTION(0x006a0b40, rx_snake_mesh_construct)

/* 006a0db0 (ESI = mesh): the destructor's body: the ring freed, the lock deleted, the Entity base's registries ([0xa95824] /
 * [0xa95828] vf1 with the entity; null in the product) */
void rx_snake_mesh_destroy(rx_addr m) {
    SU32(m, 0, VT_MESH);
    SU32(m, 0x90, VT_MESH_O90);
    SU32(m, 0x98, VT_MESH_O98);
    ACT_CALL(1, rx_reflex_aligned_free(U32(m, 0xcc)));
    rx_set_sp(rx_sp() - 4);
    os_cs(IAT_DELETE_CS, m + 0xd0);
    rx_set_sp(rx_sp() + 4);
    SU32(m, 0x90, VT_RENDERABLE);
    SU32(m, 0, VT_ENTITY);
    for (rx_addr g = 0x00a95824; g <= 0x00a95828; g += 4) {
        const rx_addr r = U32(0, g);
        if (r != 0) { const uint32_t a[1] = {m}; (void)act_vcall(r, 4, 1, a); }
    }
}
RX_FUNCTION(0x006a0db0, rx_snake_mesh_destroy)

/* 006a0c50 slot 0 (thiscall; flags; RET 4) -> mesh: 006a0db0, freed with flags & 1. Frame: push esi. */
rx_addr rx_snake_mesh_sdtor(rx_addr m, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_snake_mesh_destroy(m));
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(m));
    ACT_END(F);
    return m;
}
RX_FUNCTION(0x006a0c50, rx_snake_mesh_sdtor)
/* 006a2770 IRenderable slot 0 (ECX = mesh + 0x90): the adjustor into 006a0c50 */
rx_addr rx_snake_mesh_sdtor_o90(rx_addr r, uint32_t flags) { return rx_snake_mesh_sdtor(r - 0x90, flags); }
RX_FUNCTION(0x006a2770, rx_snake_mesh_sdtor_o90)

/* 0069e3a0 (ECX = id, EAX = the hash; stdcall obj, scene, colour; RET 0xc) -> obj: the trail object (see the top), its mesh (006a0b40)
 * added to the scene twice as the original does (006a0e20, then a second id under the scene's lock and 007d2f70 again), and its first
 * chain (0069ed10). Frame: push ebx / ebp / esi / edi. */
rx_addr rx_snake_trail_construct(rx_addr obj, rx_addr scene, rx_addr color, uint32_t id, rx_addr hash) {
    ACT_FRAME(F, 0, 4);
    SU32(obj, 0, hash);
    const rx_addr m = obj + 0x10;
    ACT_CALL(2, (void)rx_snake_mesh_construct(m, id, color));
    SU32(obj, 0x104, id);
    SU32(obj, 0x100, SHAPE);
    ACT_CALL(0, (void)rx_snake_segment_pool_init(obj + 0x108));
    SU32(obj, 0x11c, 0);
    SU32(obj, 0x120, 0);
    ACT_CALL(0, (void)rx_snake_chain_pool_init(obj + 0x124));
    SF32(obj, 0x138, 0.0f);
    SF32(obj, 0x13c, 0.0f);
    SU32(obj, 0x140, 0);
    SF32(obj, 0x144, 0.0f);
    ACT_CALL(0, rx_snake_mesh_scene_add(m, scene));
    os_cs(IAT_ENTER_CS, scene + 4);
    const uint32_t n = U32(scene, 0x19f4);
    SU32(scene, 0x19f4, n + 1);
    SU32(m, 0x14, n);
    os_cs(IAT_LEAVE_CS, scene + 4);
    ACT_CALL(1, rx_scene_manager_add_entity(m, scene));
    ACT_CALL(0, rx_snake_trail_new_chain(obj));
    ACT_END(F);
    return obj;
}
RX_FUNCTION(0x0069e3a0, rx_snake_trail_construct)

/* 0069e460 (EDI = trail): the mesh out of its scene (007d3040 with the mesh's scene +0x10) and its vf9, every segment freed, both pools'
 * blocks freed, the mesh destroyed (006a0db0). Frame: push esi. */
void rx_snake_trail_destroy(rx_addr t) {
    ACT_FRAME(F, 0, 1);
    const rx_addr m = t + 0x10;
    ACT_CALL(1, rx_scene_manager_remove_entity(m, U32(t, 0x20)));
    (void)act_vcall(m, 0x24, 0, NULL);
    while (U32(t, 0x11c) != 0) ACT_CALL(0, rx_snake_trail_free_oldest(t));
    ACT_CALL(1, rx_reflex_aligned_free(U32(t, 0x124)));
    ACT_CALL(1, rx_reflex_aligned_free(U32(t, 0x108)));
    ACT_CALL(0, rx_snake_mesh_destroy(m));
    ACT_END(F);
}
RX_FUNCTION(0x0069e460, rx_snake_trail_destroy)

/* ======================================================================================================================== */
/* the trail meshes' shared GPU resources                                                                                    */

/* 006a0c70: the "SnakeShader" asset of the engine's database (vf27 slot 0x6c, tag 'shader'), its loading flag (+0x4c) held while
 * "SnakeVS" ([0xa7f120]) and "SnakePS" ([0xa7f118]) are taken from its XenonShaderAsset (+0x44). Frame: push esi / edi. */
void rx_snake_mesh_shaders_load(void) {
    ACT_FRAME(F, 0, 2);
    const rx_addr db = U32(U32(0, 0x00a95844), 8);
    const uint32_t a[4] = {0x009426c4 /* "SnakeShader" */, 0x64616873u, 0x7265u, 1};
    const rx_addr h = act_vcall(db, 0x6c, 4, a);
    const rx_addr asset = U32(h, 0x44);
    SU32(h, 0x4c, 1);
    ACT_CALL(4, rx_shader_asset_vertex_shader(asset, 0x0095e4e0 /* "SnakeVS" */, 0x00a7f120, 0));
    ACT_CALL(4, rx_shader_asset_pixel_shader(asset, 0x0095e4e8 /* "SnakePS" */, 0x00a7f118, 0));
    SU32(h, 0x4c, 0);
    ACT_END(F);
}
RX_FUNCTION(0x006a0c70, rx_snake_mesh_shaders_load)

/* 006a0cd0: 12 times: the vertex declaration (007afd60: elements 0x9c45dc, the renderer's cache; into [0xa7f11c], each overwriting the
 * last) and two vertex buffers of 960000 bytes (007b0cd0, file 0x95e4f0 lines 0x106 / 0x107) into the pair at 0xa7f0b8 + 8i.
 * Frame: push ecx / esi / edi. */
void rx_snake_mesh_statics_load(void) {
    ACT_FRAME(F, 0, 3);
    for (uint32_t i = 0; i < 12; i++) {
        rx_addr d;
        ACT_CALLR(d, 0, rx_vertex_decl_get(0x009c45dc, U32(U32(0, 0x00a95960), 0x14)));
        SU32(0, 0x00a7f11c, d);
        ACT_CALL(7, (void)rx_gpu_create_vb(0xea600, 0, VB_TABLE + 8 * i, 0, 0, 0x0095e4f0, 0x106));
        ACT_CALL(7, (void)rx_gpu_create_vb(0xea600, 0, VB_TABLE + 8 * i + 4, 0, 0, 0x0095e4f0, 0x107));
    }
    ACT_END(F);
}
RX_FUNCTION(0x006a0cd0, rx_snake_mesh_statics_load)

/* 007b0ee0 (cdecl out): the GPU registry's record for the resource slot `out` (record +4) dropped under the registry's lock: the record
 * freed, its list node erased (007cdb70: unlinked, freed, the count - 1). Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc;
 * push ebx / esi / edi. */
void rx_gpu_release_record(rx_addr out) {
    ACT_FRAME_EBP(F, 8, 0xc, 3);
    const rx_addr reg = U32(0, RX_GPU_REGISTRY);
    if (reg == 0) { ACT_END(F); return; }
    os_cs(IAT_ENTER_CS, GPU_LIST_CS);
    const rx_addr head = U32(U32(0, RX_GPU_REGISTRY), 0x14);
    for (rx_addr n = U32(head, 0); n != head; n = U32(n, 0)) {
        const rx_addr rec = U32(n, 8);
        if (U32(rec, 4) != out) continue;
        ACT_CALL(1, rx_reflex_aligned_free(rec));
        SU32(U32(n, 4), 0, U32(n, 0));
        SU32(U32(n, 0), 4, U32(n, 4));
        ACT_CALL(3, rx_reflex_aligned_free(n));
        SU32(U32(0, RX_GPU_REGISTRY), 0x18, U32(U32(0, RX_GPU_REGISTRY), 0x18) - 1);
        break;
    }
    os_cs(IAT_LEAVE_CS, GPU_LIST_CS);
    ACT_END(F);
}
RX_FUNCTION(0x007b0ee0, rx_gpu_release_record)

/* 006a0d50: the 12 pairs of vertex buffers dropped from the registry (007b0ee0) and released (slot 2), each slot zeroed. The shaders
 * and the declaration are released by the activity (MXActivitySnake vf6). Frame: push ebx / esi. */
void rx_snake_mesh_statics_release(void) {
    ACT_FRAME(F, 0, 2);
    for (uint32_t i = 0; i < 12; i++) {
        const rx_addr p = VB_TABLE + 8 * i;
        ACT_CALL(1, rx_gpu_release_record(p));
        ACT_CALL(1, rx_gpu_release_record(p + 4));
        for (uint32_t k = 0; k < 8; k += 4) {
            const rx_addr o = U32(0, p + k);
            if (o != 0) { const uint32_t a[1] = {o}; ACT_CALL(1, (void)rx_d3d_call(o, 2, 1, a)); SU32(0, p + k, 0); }
        }
    }
    ACT_END(F);
}
RX_FUNCTION(0x006a0d50, rx_snake_mesh_statics_release)

/* ======================================================================================================================== */
/* port addition: the trail's segments for the host's renderer                                                                */

uint32_t rx_snake_trail_segments(rx_addr t, float* out, uint32_t max) {
    uint32_t n = 0, ci = 0;
    for (rx_addr c = U32(t, 0x11c); c != 0; c = U32(c, 0), ci++)
        for (rx_addr s = U32(c, 8); s != 0; s = U32(s, 0), n++) {
            if (n >= max || out == NULL) continue;
            float* o = out + RX_SNAKE_TRAIL_SEG_FLOATS * n;
            for (uint32_t k = 0; k < 9; k++) o[k] = F32(s, 0x10 + 4 * (k + k / 3));   /* +0x10, +0x20, +0x30 rows of 3 */
            o[9] = F32(s, 0x40);
            o[10] = F32(s, 0x44);
            o[11] = F32(s, 0x48);
            o[12] = (float)ci;
            o[13] = o[14] = o[15] = 0.0f;
        }
    return n;
}
