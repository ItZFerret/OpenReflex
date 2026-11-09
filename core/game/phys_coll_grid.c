/* phys_coll_grid.c -- the broadphase grid of the collision system: a uniform 2D grid over x / z holding object boxes by id, with
 * an overflow list for boxes that reach outside the grid; plus the SIMD integer helpers it uses and the int64 helpers next to
 * them in the exe. (native/game/notes/collision.md; the original functions are LTCG: grid in ESI, see phys_coll.cpp.)
 *
 * Grid object (all offsets from the grid):
 *   +0x000 query results: ids, at most 0x100        +0x400 result count
 *   +0x404 cells in x, +0x408 cells in z            +0x414 ids per cell (capacity), +0x418 overflow capacity
 *   +0x41c box minimum per id (vec4 array)          +0x420 box maximum per id (vec4 array)
 *   +0x430 grid origin (vec4, x / z used)           +0x460 inverse cell size (vec4, x / z used)
 *   +0x470 cell id lists (cell * capacity + k)      +0x474 cell id counts (per cell)
 *   +0x478 overflow count                           +0x47c overflow ids
 * A box covers the cells floor((min.xz - origin.xz) * inv) .. floor((max.xz - origin.xz) * inv), clamped to the grid; "outside"
 * means it misses the grid entirely, "partial" that it reaches beyond it (such boxes also live in the overflow list). */
#include "phys_coll_grid.h"
#include "phys_math.h"

#define GRID_RESULTS        0x000
#define GRID_RESULT_COUNT   0x400
#define GRID_CELLS_X        0x404
#define GRID_CELLS_Z        0x408
#define GRID_CELL_CAPACITY  0x414
#define GRID_OVERFLOW_CAP   0x418
#define GRID_BOX_MIN        0x41c
#define GRID_BOX_MAX        0x420
#define GRID_ORIGIN         0x430
#define GRID_INV_CELL       0x460
#define GRID_CELL_LISTS     0x470
#define GRID_CELL_COUNTS    0x474
#define GRID_OVERFLOW_COUNT 0x478
#define GRID_OVERFLOW_IDS   0x47c
#define GRID_MAX_RESULTS    0x100

/* ---- integer lanes: an rx_v4 holds the signed 32-bit lanes bit for bit */
static inline int32_t lane_i(rx_v4 a, int i) { return (int32_t)v4_lb(a, i); }

/* 00882170 Int4GreaterMask: per lane, a > b (signed) ? all ones : 0 */
rx_v4 phys_int4_gt_mask(rx_v4 a, rx_v4 b) {
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = rx_bf(lane_i(a, i) > lane_i(b, i) ? ~0u : 0u);
    return r;
}
RX_FUNCTION(0x00882170, phys_int4_gt_mask)

/* 008821e0 Int4BelowFlags: per lane mask b < a (unsigned); in *flags (the word the caller passes, other bits kept) bits 4..7 are
 * cleared, bit 7 is set when every lane is set and bit 5 when no lane is. */
rx_v4 phys_int4_below_flags(rx_v4 a, rx_v4 b, uint32_t* flags) {
    uint32_t m[4];
    for (int i = 0; i < 4; i++) m[i] = v4_lb(b, i) < v4_lb(a, i) ? ~0u : 0u;
    const int all = m[0] && m[1] && m[2] && m[3];
    const int none = !m[0] && !m[1] && !m[2] && !m[3];
    uint32_t w = *flags & 0xffffff0fu;
    if (all) w |= 0x80;
    if (none) w |= 0x20;
    *flags = w;
    return v4_bits(m[0], m[1], m[2], m[3]);
}
RX_FUNCTION(0x008821e0, phys_int4_below_flags)

/* 00882290 Int4Min: per lane, a < b ? a : b (signed) */
rx_v4 phys_int4_min(rx_v4 a, rx_v4 b) {
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = lane_i(a, i) < lane_i(b, i) ? a.v[i] : b.v[i];
    return r;
}
RX_FUNCTION(0x00882290, phys_int4_min)

/* 00882300 Int4Max: per lane, a >= b ? a : b (signed) */
rx_v4 phys_int4_max(rx_v4 a, rx_v4 b) {
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = lane_i(a, i) >= lane_i(b, i) ? a.v[i] : b.v[i];
    return r;
}
RX_FUNCTION(0x00882300, phys_int4_max)

/* 0069b2f0 Vec4FloorToInt: per lane the integer floor of the float. r = (x + c) - c with c = +-2^23 (0x9774f0) carrying x's sign
 * (the sign mask 0x9774a8) rounds to nearest; one is subtracted (0x931f24) when that rounded above x; then truncated (cvttss2si:
 * 0x80000000 for NaN and out of range). */
static int32_t floor_to_int_lane(float x) {
    const uint32_t sign = rx_fb(x) & 0x80000000u;
    const float c = rx_bf(0x4b000000u | sign);
    float r = phys_sub(phys_add(x, c), c);
    const float above = phys_sub(r, x);
    const uint32_t above_mask = !(above <= rx_bf(sign)) ? ~0u : 0u;   /* cmpnless: also true for NaN */
    r = phys_sub(r, rx_bf(above_mask & rx_fb(1.0f)));
    return rx_cvtt(r);
}
rx_v4 phys_vec4_floor_to_int(rx_v4 x) {
    uint32_t r[4];
    for (int i = 0; i < 4; i++) r[i] = (uint32_t)floor_to_int_lane(x.v[i]);
    return v4_bits(r[0], r[1], r[2], r[3]);
}
RX_FUNCTION(0x0069b2f0, phys_vec4_floor_to_int)

/* 0069b400 Vec4StoreAligned (xmm0, cdecl dest): two 8-byte stores of x at dest & ~15 (the original traps on a misaligned dest, and
 * checks its stack cookie). Returns the xmm0 the original leaves: lanes 2 and 3 of x, upper half cleared (movq) -- x itself when
 * dest is null. */
rx_v4 phys_vec4_store_aligned(rx_v4 x, rx_addr dest) {
    if (dest == 0) return x;
    const rx_addr at = dest & 0xfffffff0u;
    SU32(at, 0, v4_lb(x, 0)); SU32(at, 4, v4_lb(x, 1));
    SU32(at, 8, v4_lb(x, 2)); SU32(at, 12, v4_lb(x, 3));
    return v4(x.v[2], x.v[3], 0.0f, 0.0f);
}
RX_FUNCTION(0x0069b400, phys_vec4_store_aligned)

/* ---- the cells a box covers */
typedef struct { int outside, partial; int32_t x0, z0, x1, z1; } grid_cells;

/* The preamble shared by Insert / Remove / QueryCells / QueryBoxes: c = floor_to_int(floor(((lo.x, lo.z, hi.x, hi.z) - origin.xz)
 * * inv.xz)); masks (0 > c) and (c > last cell); outside = any of (hi < 0, hi.z < 0, lo > last x, lo.z > last z), partial = any of
 * (lo < 0, hi > last); the cells clamped to the grid. */
static grid_cells grid_box_cells(rx_addr grid, rx_v4 lo, rx_v4 hi) {
    const uint32_t last_x = U32(grid, GRID_CELLS_X) - 1, last_z = U32(grid, GRID_CELLS_Z) - 1;
    const rx_v4 limit = v4_bits(last_x, last_z, last_x, last_z), zero = v4_zero();
    const rx_v4 origin = v4_ld(grid + GRID_ORIGIN), inv = v4_ld(grid + GRID_INV_CELL);
    const rx_v4 scaled = v4_mul(v4_sub(v4_shuf(lo, hi, 0x88), v4_shuf(origin, origin, 0x88)), v4_shuf(inv, inv, 0x88));
    const rx_v4 c = phys_vec4_floor_to_int(phys_vec4_floor(scaled));
    const rx_v4 below = phys_int4_gt_mask(zero, c), above = phys_int4_gt_mask(c, limit);
    uint32_t flags_outside = 0, flags_partial = 0;
    phys_int4_below_flags(v4_shuf(below, above, 0x4e), zero, &flags_outside);
    phys_int4_below_flags(v4_shuf(below, above, 0xe4), zero, &flags_partial);
    grid_cells r;
    r.outside = (~(flags_outside >> 5) & 1) != 0;
    r.partial = (~(flags_partial >> 5) & 1) != 0;
    const rx_v4 clamped = phys_int4_min(phys_int4_max(c, zero), limit);
    r.x0 = lane_i(clamped, 0); r.z0 = lane_i(clamped, 1); r.x1 = lane_i(clamped, 2); r.z1 = lane_i(clamped, 3);
    return r;
}

/* Appends every overflow id to the results (capped at 0x100, no duplicate check). */
static void results_add_overflow(rx_addr grid) {
    for (int32_t i = 0; i < I32(grid, GRID_OVERFLOW_COUNT); i++) {
        const uint32_t n = U32(grid, GRID_RESULT_COUNT);
        if (n != GRID_MAX_RESULTS) {
            SU32(grid, 4 * n, U32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)i));
            SU32(grid, GRID_RESULT_COUNT, U32(grid, GRID_RESULT_COUNT) + 1);
        }
    }
}

/* Appends id to the results unless the results are full or already hold it. */
static void results_add_unique(rx_addr grid, uint32_t id) {
    const uint32_t n = U32(grid, GRID_RESULT_COUNT);
    if (n == GRID_MAX_RESULTS) return;
    for (uint32_t i = 0; i < n; i++)
        if (U32(grid, 4 * i) == id) return;
    SU32(grid, 4 * n, id);
    SU32(grid, GRID_RESULT_COUNT, U32(grid, GRID_RESULT_COUNT) + 1);
}

/* 00882d90 BroadphaseGrid_QueryCells (ESI grid, ECX hi*, stack lo*; RET 4): the ids of the cells the box covers into the results
 * (without duplicates), preceded by the overflow ids when the box reaches outside the grid (a box missing the grid gets only those). */
void phys_grid_query_cells(rx_addr grid, rx_addr lo_p, rx_addr hi_p) {
    const rx_v4 hi = v4_ld(hi_p), lo = v4_ld(lo_p);
    SU32(grid, GRID_RESULT_COUNT, 0);
    const grid_cells c = grid_box_cells(grid, lo, hi);
    if (c.outside || c.partial) {
        results_add_overflow(grid);
        if (c.outside) return;
    }
    for (int32_t z = c.z0; z <= c.z1; z++) {
        const uint32_t row = U32(grid, GRID_CELLS_X) * (uint32_t)z;
        for (int32_t x = c.x0; x <= c.x1; x++) {
            const uint32_t cell = (uint32_t)x + row;
            int32_t count = I32(U32(grid, GRID_CELL_COUNTS), 4 * cell);   /* read once; the list base is read per id */
            const uint32_t at = cell * U32(grid, GRID_CELL_CAPACITY) * 4;
            for (uint32_t k = 0; count > 0; k++, count--)
                results_add_unique(grid, U32(U32(grid, GRID_CELL_LISTS), at + 4 * k));
        }
    }
}
RX_FUNCTION(0x00882d90, phys_grid_query_cells)

/* The overlap test of the box of an id with the query box: box.min <= qhi and qlo <= box.max on x, y and z (cmpleps; the three
 * lane masks summed as -3 when all are set). */
static int box_overlaps(rx_addr grid, uint32_t id, rx_v4 qlo, rx_v4 qhi) {
    const rx_v4 a = v4_cmple(v4_ld(U32(grid, GRID_BOX_MIN) + id * 16), qhi);
    if (v4_lb(a, 2) + v4_lb(a, 1) + v4_lb(a, 0) != 0xfffffffdu) return 0;
    const rx_v4 b = v4_cmple(qlo, v4_ld(U32(grid, GRID_BOX_MAX) + id * 16));
    return v4_lb(b, 2) + v4_lb(b, 1) + v4_lb(b, 0) == 0xfffffffdu;
}

/* 00883020 BroadphaseGrid_QueryBoxes (ESI grid, EDX lo*, stack hi*; RET 4): as QueryCells, but only the ids whose stored box overlaps
 * the query box. Overflow ids are appended without a duplicate check, cell ids with one. */
void phys_grid_query_boxes(rx_addr grid, rx_addr lo_p, rx_addr hi_p) {
    const rx_v4 qlo = v4_ld(lo_p), qhi = v4_ld(hi_p);
    SU32(grid, GRID_RESULT_COUNT, 0);
    const grid_cells c = grid_box_cells(grid, qlo, qhi);
    if (c.outside || c.partial) {
        for (int32_t i = 0; i < I32(grid, GRID_OVERFLOW_COUNT); i++) {
            const uint32_t id = U32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)i);
            if (!box_overlaps(grid, id, qlo, qhi)) continue;
            const uint32_t n = U32(grid, GRID_RESULT_COUNT);
            if (n != GRID_MAX_RESULTS) {
                SU32(grid, 4 * n, id);
                SU32(grid, GRID_RESULT_COUNT, U32(grid, GRID_RESULT_COUNT) + 1);
            }
        }
        if (c.outside) return;
    }
    for (int32_t z = c.z0; z <= c.z1; z++) {
        const uint32_t row = U32(grid, GRID_CELLS_X) * (uint32_t)z;
        for (int32_t x = c.x0; x <= c.x1; x++) {
            const uint32_t cell = (uint32_t)x + row;
            int32_t count = I32(U32(grid, GRID_CELL_COUNTS), 4 * cell);
            const uint32_t at = cell * U32(grid, GRID_CELL_CAPACITY) * 4;
            for (uint32_t k = 0; count > 0; k++, count--) {
                const uint32_t id = U32(U32(grid, GRID_CELL_LISTS), at + 4 * k);
                if (box_overlaps(grid, id, qlo, qhi)) results_add_unique(grid, id);
            }
        }
    }
}
RX_FUNCTION(0x00883020, phys_grid_query_boxes)

/* Overflow list: insert (unless listed; the count is capped at +0x418 and the id goes to the last slot) / remove (the last match is
 * overwritten by the last entry, which becomes -1). */
static void overflow_insert(rx_addr grid, uint32_t id) {
    const int32_t n = I32(grid, GRID_OVERFLOW_COUNT);
    for (int32_t i = 0; i < n; i++)
        if (U32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)i) == id) return;
    const int32_t cap = I32(grid, GRID_OVERFLOW_CAP);
    const int32_t m = n + 1 < cap ? n + 1 : cap;
    SU32(grid, GRID_OVERFLOW_COUNT, m);
    SU32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)m - 4, id);
}


/* 008826d0 BroadphaseGrid_Insert (ESI grid, stack id; RET 4): puts the id into every cell its box covers (unless already listed;
 * a cell holds at most +0x414 ids, a full cell's last slot is overwritten) and into the overflow list when the box reaches outside. */
void phys_grid_insert(rx_addr grid, uint32_t id) {
    const grid_cells c = grid_box_cells(grid, v4_ld(U32(grid, GRID_BOX_MIN) + id * 16), v4_ld(U32(grid, GRID_BOX_MAX) + id * 16));
    if (c.outside || c.partial) {
        overflow_insert(grid, id);
        if (c.outside) return;
    }
    for (int32_t x = c.x0; x <= c.x1; x++) {
        for (int32_t z = c.z0; z <= c.z1; z++) {
            const uint32_t cell = U32(grid, GRID_CELLS_X) * (uint32_t)z + (uint32_t)x;
            const int32_t capacity = I32(grid, GRID_CELL_CAPACITY);
            const uint32_t base = (uint32_t)capacity * cell;
            const rx_addr count_at = U32(grid, GRID_CELL_COUNTS) + cell * 4;
            if (I32(count_at, 0) > 0) {
                int32_t count = I32(U32(grid, GRID_CELL_COUNTS), cell * 4);
                int listed = 0;
                for (uint32_t k = 0; count > 0; k++, count--)
                    if (U32(U32(grid, GRID_CELL_LISTS) + base * 4, 4 * k) == id) listed = 1;
                if (listed) continue;
            }
            const int32_t n = I32(count_at, 0) + 1;
            SU32(count_at, 0, n < capacity ? n : capacity);
            const uint32_t last = U32(U32(grid, GRID_CELL_COUNTS), cell * 4) + base;
            SU32(U32(grid, GRID_CELL_LISTS), last * 4 - 4, id);
        }
    }
}
RX_FUNCTION(0x008826d0, phys_grid_insert)

/* ---- the helpers' frames (00882980 calls them with the stack pointer at their return slot e; ebp = its frame pointer, which
 * each pushes): `push ebp; mov ebp, esp; and esp, -16; sub esp, n` and their stores as the original makes them. The frames are dead
 * after the call, but a later light event record of the vehicle's move (VehicleWorldMove 00464a00 -> Lights_Add) copies one of
 * their words (known-differences 9). */
static rx_addr hf_base(rx_addr e, uint32_t ret, uint32_t ebp, uint32_t sub) {
    SU32(e, 0, ret); SU32(e, -4, ebp);
    return ((e - 4) & ~15u) - sub;
}
static rx_v4 hf_floor(rx_addr e, uint32_t ret, uint32_t ebp, rx_v4 x) {            /* 0042ccd0 */
    const rx_addr F = hf_base(e, ret, ebp, 0x20);
    const rx_v4 r = phys_vec4_floor(x);
    v4_st(F, x); v4_st(F + 0x10, r);
    return r;
}
static rx_v4 hf_floor_to_int(rx_addr e, uint32_t ret, uint32_t ebp, rx_v4 x) {     /* 0069b2f0 */
    const rx_addr F = hf_base(e, ret, ebp, 0x20);
    const rx_v4 r = phys_vec4_floor_to_int(x);
    v4_st(F, x); v4_st(F + 0x10, r);
    return r;
}
static rx_v4 hf_gt_mask(rx_addr e, uint32_t ret, uint32_t ebp, rx_v4 a, rx_v4 b) { /* 00882170 */
    const rx_addr F = hf_base(e, ret, ebp, 0x30);
    const rx_v4 r = phys_int4_gt_mask(a, b);
    v4_st(F, a); v4_st(F + 0x10, b); v4_st(F + 0x20, r);
    return r;
}
static rx_v4 hf_min_max(rx_addr e, uint32_t ret, uint32_t ebp, rx_v4 a, rx_v4 b, int max) {   /* 00882290 / 00882300 */
    const rx_addr F = hf_base(e, ret, ebp, 0x30);
    const rx_v4 r = max ? phys_int4_max(a, b) : phys_int4_min(a, b);
    v4_st(F + 0x20, b); v4_st(F + 0x10, a); v4_st(F, r);
    return r;
}
/* 008821e0 (EDX = the flags word, read-modify-written): push ebx / esi / edi after its sub esp, 0x34 */
static rx_v4 hf_below_flags(rx_addr e, uint32_t ret, uint32_t ebp, uint32_t ebx, uint32_t esi, uint32_t edi, rx_v4 a, rx_v4 b, rx_addr flags) {
    const rx_addr F = hf_base(e, ret, ebp, 0x34);
    v4_st(F + 0x14, b); v4_st(F + 0x24, a);
    SU32(F, -4, ebx); SU32(F, -8, esi); SU32(F, -0xc, edi);
    uint32_t w = U32(flags, 0);
    const rx_v4 r = phys_int4_below_flags(a, b, &w);
    for (int k = 0; k < 4; k++) SU32(F, 4 + 4 * k, v4_lb(r, k));
    SU32(flags, 0, w);
    return r;
}
/* 0069b400 (cdecl dest, xmm0): the stack cookie at F + 0x1c, x at F, the store; __security_check_cookie's return address */
static void hf_store_aligned(rx_addr e, uint32_t ret, uint32_t ebp, rx_v4 x, rx_addr dest) {
    const rx_addr F = hf_base(e, ret, ebp, 0x20);
    SU32(F, 0x1c, U32(0, 0x009b45e4) ^ F);
    v4_st(F, x);
    (void)phys_vec4_store_aligned(x, dest);
    SU32(F, -4, 0x0069b446);
}

/* 00882980 BroadphaseGrid_Remove (ESI grid, stack id; RET 4): takes the id out of every cell its (current) box covers -- the last
 * match is overwritten by the cell's last entry, which becomes -1 -- and out of the overflow list. Transliterated with its frame:
 * push ebp; and esp, -16; sub esp, 0x88; push ebx / edi (EDI = id * 16): the last cells +0x50, the floored box +0x70 (later the
 * inverse cell size), zero +0x80, the masks +0x60 / +0x40, the two flag words +0x10 / +0x08 (Int4BelowFlags keeps their other bits:
 * dead stack), the partial flag byte +0x31, the clamped cells +0x20, the second floor of the box (Vec4Floor / Vec4FloorToInt /
 * Vec4StoreAligned into +0x40 / +0x60, without effect), the loop locals +0x08 (the match), +0x0c / +0x10 (x / z), +0x18 / +0x1c
 * (the cell's count). E = the return slot; ebp / ebx / edi = the caller's. */
void phys_grid_remove_fr(rx_addr grid, uint32_t id, rx_addr E, uint32_t ebp, uint32_t ebx, uint32_t edi) {
    const rx_addr fp = E - 4, A = fp & ~15u, S = A - 0x90;
    SU32(E, 4, id); SU32(E, -4, ebp); SU32(A, -0x8c, ebx); SU32(A, -0x90, edi);
    const uint32_t last_x = U32(grid, GRID_CELLS_X) - 1, last_z = U32(grid, GRID_CELLS_Z) - 1;
    const rx_v4 limit = v4_bits(last_x, last_z, last_x, last_z), zero = v4_zero();
    SU32(S, 0x50, last_x); SU32(S, 0x54, last_z); SU32(S, 0x58, last_x); SU32(S, 0x5c, last_z);
    const rx_v4 lo = v4_ld(U32(grid, GRID_BOX_MIN) + id * 16), hi = v4_ld(U32(grid, GRID_BOX_MAX) + id * 16);
    const rx_v4 origin = v4_ld(grid + GRID_ORIGIN), inv = v4_ld(grid + GRID_INV_CELL);
    const rx_v4 scaled = v4_mul(v4_sub(v4_shuf(lo, hi, 0x88), v4_shuf(origin, origin, 0x88)), v4_shuf(inv, inv, 0x88));
    const rx_v4 f1 = hf_floor(S - 4, 0x008829eb, fp, scaled);
    const rx_v4 c = hf_floor_to_int(S - 4, 0x008829f0, fp, f1);
    v4_st(S + 0x70, c); v4_st(S + 0x80, zero);
    const rx_v4 below = hf_gt_mask(S - 4, 0x00882a08, fp, zero, c);
    v4_st(S + 0x60, below);
    const rx_v4 above = hf_gt_mask(S - 4, 0x00882a1c, fp, c, limit);
    v4_st(S + 0x40, above);
    (void)hf_below_flags(S - 4, 0x00882a3e, fp, ebx, grid, id * 16, v4_shuf(below, above, 0x4e), zero, S + 0x10);
    (void)hf_below_flags(S - 4, 0x00882a5a, fp, ebx, grid, id * 16, v4_shuf(below, above, 0xe4), zero, S + 8);
    const int outside = (~(U32(S, 0x10) >> 5) & 1) != 0, partial = (~(U32(S, 8) >> 5) & 1) != 0;
    v4_st(S + 0x40, limit);
    SU8(S, 0x31, partial);
    const rx_v4 c1 = hf_min_max(S - 4, 0x00882a91, fp, c, zero, 1);
    const rx_v4 cl = hf_min_max(S - 4, 0x00882a9b, fp, c1, limit, 0);
    v4_st(S + 0x20, cl);
    /* the second floor of the box into the frame (no effect on the cells) */
    const rx_v4 vhi = v4_mul(v4_sub(hi, origin), inv);
    v4_st(S + 0x40, origin); v4_st(S + 0x70, inv);
    const rx_v4 f2 = hf_floor(S - 4, 0x00882acd, fp, vhi);
    SU32(S, -4, S + 0x40);                                        /* push eax (&S + 0x40) */
    v4_st(S + 0x60, f2);
    SU32(S, -8, last_z);                                          /* push ecx (Int4Min's last lane) */
    const rx_v4 vlo = v4_mul(v4_sub(lo, origin), inv);
    const rx_v4 f3 = hf_floor(S - 0xc, 0x00882af1, fp, vlo);
    const rx_v4 i3 = hf_floor_to_int(S - 0xc, 0x00882af6, fp, f3);
    hf_store_aligned(S - 8, 0x00882afe, fp, i3, S + 0x40);
    SU32(S, -4, S + 0x40); SU32(S, -8, U32(0, 0x009b45e4));      /* push edx; push ecx (the cookie the check left) */
    const rx_v4 i2 = hf_floor_to_int(S - 0xc, 0x00882b11, fp, f2);
    hf_store_aligned(S - 8, 0x00882b19, fp, i2, S + 0x40);
    /* the overflow list (the box reaches outside; outside: only there) */
    if (outside || partial) {
        const int32_t n = I32(grid, GRID_OVERFLOW_COUNT);
        SU32(S, 8, 0xffffffffu);
        if (n > 0) {
            for (int32_t k = 0; k < n; k++) if (U32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)k) == id) SU32(S, 8, (uint32_t)k);
            const int32_t at = I32(S, 8);
            if (at >= 0) {
                const rx_addr list = U32(grid, GRID_OVERFLOW_IDS);
                SU32(list, 4 * (uint32_t)at, U32(list, 4 * (uint32_t)n - 4));
                SU32(U32(grid, GRID_OVERFLOW_IDS), 4 * (uint32_t)n - 4, 0xffffffffu);
                SU32(grid, GRID_OVERFLOW_COUNT, U32(grid, GRID_OVERFLOW_COUNT) - 1);
            }
        }
        if (outside) return;
    }
    /* the cells: x outer (+0x0c), z inner (+0x10) */
    SU32(S, 0xc, (uint32_t)lane_i(cl, 0));
    for (int32_t x = lane_i(cl, 0); x <= lane_i(cl, 2); x++) {
        SU32(S, 0xc, (uint32_t)x);
        SU32(S, 0x10, (uint32_t)lane_i(cl, 1));
        for (int32_t z = lane_i(cl, 1); z <= lane_i(cl, 3); z++) {
            SU32(S, 0x10, (uint32_t)z);
            const uint32_t cell = U32(grid, GRID_CELLS_X) * (uint32_t)z + (uint32_t)x;
            const uint32_t base = U32(grid, GRID_CELL_CAPACITY) * cell;
            const int32_t count = I32(U32(grid, GRID_CELL_COUNTS), cell * 4);
            SU32(S, 8, 0xffffffffu); SU32(S, 0x18, (uint32_t)count);
            if (count > 0) {
                SU32(S, 0x1c, U32(U32(grid, GRID_CELL_COUNTS), cell * 4));
                const rx_addr items = U32(grid, GRID_CELL_LISTS) + base * 4;
                for (int32_t k = 0; k < I32(S, 0x1c); k++) if (U32(items, 4 * (uint32_t)k) == id) SU32(S, 8, (uint32_t)k);
                const int32_t at = I32(S, 8);
                if (at >= 0) {
                    const uint32_t lastw = ((uint32_t)count + base - 1) * 4;
                    SU32(U32(grid, GRID_CELL_LISTS), ((uint32_t)at + base) * 4, U32(lastw, U32(grid, GRID_CELL_LISTS)));
                    SU32(lastw, U32(grid, GRID_CELL_LISTS), 0xffffffffu);
                    const rx_addr count_at = U32(grid, GRID_CELL_COUNTS) + cell * 4;
                    SU32(count_at, 0, U32(count_at, 0) - 1);
                }
            }
            SU32(S, 0x10, (uint32_t)z + 1);
        }
        SU32(S, 0xc, (uint32_t)x + 1);
    }
}
void phys_grid_remove(rx_addr grid, uint32_t id) { phys_grid_remove_fr(grid, id, rx_sp(), 0, 0, 0); }
RX_FUNCTION(0x00882980, phys_grid_remove)

/* 0084a1a0 BroadphaseGrid_Update (EDI id, stack object*, lo*, hi*; RET 0xc): moves an id's box in the grid of the object (+0x30):
 * removes it, stores the new min / max, inserts it again. */
void phys_grid_update(uint32_t id, rx_addr object, rx_addr lo, rx_addr hi) {
    const rx_addr grid = U32(object, 0x30);
    phys_grid_remove(grid, id);
    v4_st(U32(grid, GRID_BOX_MIN) + id * 16, v4_ld(lo));
    v4_st(U32(grid, GRID_BOX_MAX) + id * 16, v4_ld(hi));
    phys_grid_insert(grid, id);
}
RX_FUNCTION(0x0084a1a0, phys_grid_update)

/* 0088bd90 FUN_0088bd90 (thiscall): the int64 at this (edx:eax). */
uint64_t phys_int64_load(rx_addr self) { return (uint64_t)U32(self, 0) | (uint64_t)U32(self, 4) << 32; }
RX_FUNCTION(0x0088bd90, phys_int64_load)

/* 0088bd50 FUN_0088bd50 (thiscall, stack src*; RET 4): copies an int64 (low word first); returns the words read (eax = this). */
uint64_t phys_int64_assign(rx_addr self, rx_addr src) {
    const uint32_t lo = U32(src, 0);
    SU32(self, 0, lo);
    const uint32_t hi = U32(src, 4);
    SU32(self, 4, hi);
    return (uint64_t)lo | (uint64_t)hi << 32;
}
RX_FUNCTION(0x0088bd50, phys_int64_assign)

/* 0088be10 FUN_0088be10 (thiscall, stack out*, lo, hi; RET 0xc): *out = *this + (hi:lo) (add / adc). */
void phys_int64_add(rx_addr self, rx_addr out, uint32_t lo, uint32_t hi) {
    const uint64_t sum = ((uint64_t)U32(self, 0) | (uint64_t)U32(self, 4) << 32) + ((uint64_t)lo | (uint64_t)hi << 32);
    SU32(out, 0, (uint32_t)sum);
    SU32(out, 4, (uint32_t)(sum >> 32));
}
RX_FUNCTION(0x0088be10, phys_int64_add)
