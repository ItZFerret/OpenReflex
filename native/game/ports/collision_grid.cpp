// The broadphase grid (0x00882xxx): a uniform 2D grid over x / z of object boxes with an overflow list for boxes outside it,
// plus its SIMD integer helpers. Grid object (ESI in the custom conventions):
//   +0x000 query results (ids, up to 0x100)   +0x400 result count
//   +0x404 / +0x408 cells in x / z            +0x414 ids per cell, +0x418 overflow capacity
//   +0x41c / +0x420 box min / max per id (vec4 arrays)
//   +0x430 grid origin, +0x460 inverse cell size (vec4, x / z used)
//   +0x470 cell id lists (cell * per-cell capacity), +0x474 cell id counts, +0x478 overflow count, +0x47c overflow ids
#include "collision_terrain.h"

using namespace collision;
using reflex_port::OUT_EAX;
using reflex_port::OUT_ECX;
using reflex_port::OUT_EDX;
using reflex_port::OUT_XMM0;

namespace {

// 00882170: signed a > b per lane (masks)
X gt_mask(const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = int32_t(a.u[i]) > int32_t(b.u[i]) ? 0xffffffffu : 0u; return r; }
// 00882290 / 00882300: signed min (a < b ? a : b) / max (a >= b ? a : b) per lane
X imin(const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = int32_t(a.u[i]) < int32_t(b.u[i]) ? a.u[i] : b.u[i]; return r; }
X imax(const X& a, const X& b) { X r; for (int i = 0; i < 4; i++) r.u[i] = int32_t(a.u[i]) >= int32_t(b.u[i]) ? a.u[i] : b.u[i]; return r; }
// 008821e0: masks b <u a per lane; all set / none set
struct LtFlags { X mask; bool all, none; };
LtFlags lt_flags(const X& a, const X& b) {
    LtFlags f;
    for (int i = 0; i < 4; i++) f.mask.u[i] = b.u[i] < a.u[i] ? 0xffffffffu : 0u;
    f.all = f.mask.u[0] && f.mask.u[1] && f.mask.u[2] && f.mask.u[3];
    f.none = !f.mask.u[0] && !f.mask.u[1] && !f.mask.u[2] && !f.mask.u[3];
    return f;
}

// The cells a box covers: c = int(floor(floor(((lo.x, lo.z, hi.x, hi.z) - origin.xz) * inv.xz))) (FUN_0042ccd0 then 0069b2f0);
// outside: the box misses the grid (hi below 0 or lo beyond the last cell); partial: it reaches outside; cells clamped.
struct Cells { bool outside, partial; int32_t x0, z0, x1, z1; };
Cells box_cells(Guest& g, uint32_t grid, const X& lo, const X& hi) {
    const uint32_t lx = g.u32(grid + 0x404) - 1, lz = g.u32(grid + 0x408) - 1;
    const X lim = {{lx, lz, lx, lz}};
    const X o = ld(g, grid + 0x430), k = ld(g, grid + 0x460);
    const X v = mulps(subps(shufps(lo, hi, 0x88), shufps(o, o, 0x88)), shufps(k, k, 0x88));
    const X c = floor4i(g, call_floor4f(g, v)).i;
    const X m1 = gt_mask(X::zero(), c), m2 = gt_mask(c, lim);
    Cells r;
    r.outside = !lt_flags(shufps(m1, m2, 0x4e), X::zero()).none;
    r.partial = !lt_flags(shufps(m1, m2, 0xe4), X::zero()).none;
    const X cl = imin(imax(c, X::zero()), lim);
    r.x0 = int32_t(cl.u[0]); r.z0 = int32_t(cl.u[1]); r.x1 = int32_t(cl.u[2]); r.z1 = int32_t(cl.u[3]);
    return r;
}

// Appends id to the query results (at most 0x100), optionally unless already there.
inline void result_add(Guest& g, uint32_t grid, uint32_t id, bool unique) {
    const uint32_t n = g.u32(grid + 0x400);
    if (n == 0x100) return;
    if (unique) for (uint32_t i = 0; i < n; i++) if (g.u32(grid + 4 * i) == id) return;
    g.u32(grid + 4 * n, id);
    g.u32(grid + 0x400, n + 1);
}

// 00882d90 (custom: ESI grid, ECX max*; stack: min*; RET 4): the ids of the cells a box covers (and the overflow ids when it
// reaches outside the grid) into the results, without duplicates from the cells.
void grid_query_cells(Guest& g, uint32_t grid, uint32_t lo_p, uint32_t hi_p) {
    const X hi = ld(g, hi_p), lo = ld(g, lo_p);
    g.u32(grid + 0x400, 0);
    const Cells c = box_cells(g, grid, lo, hi);
    if (c.outside || c.partial) {
        for (int32_t i = 0; i < g.i32(grid + 0x478); i++) {
            const uint32_t n = g.u32(grid + 0x400);
            if (n != 0x100) { g.u32(grid + 4 * n, g.u32(g.u32(grid + 0x47c) + 4 * i)); g.u32(grid + 0x400, n + 1); }
        }
        if (c.outside) return;
    }
    for (int32_t z = c.z0; z <= c.z1; z++) {
        const uint32_t row = g.u32(grid + 0x404) * uint32_t(z);
        for (int32_t x = c.x0; x <= c.x1; x++) {
            const int32_t cnt = g.i32(g.u32(grid + 0x474) + (uint32_t(x) + row) * 4);
            const uint32_t at = (uint32_t(x) + row) * g.u32(grid + 0x414) * 4;
            for (int32_t k = 0; k < cnt; k++) {
                // the count is the loop's (read once); the list base is read per id
                const uint32_t n = g.u32(grid + 0x400);
                const uint32_t id = g.u32(at + 4 * uint32_t(k) + g.u32(grid + 0x470));
                if (n == 0x100) continue;
                bool found = false;
                for (uint32_t i = 0; i < n && !found; i++) found = g.u32(grid + 4 * i) == id;
                if (!found) { g.u32(grid + 4 * n, id); g.u32(grid + 0x400, g.u32(grid + 0x400) + 1); }
            }
        }
    }
}

// The overlap test of a box with an id's box: min <= qmax and qmin <= max on x, y, z (cmpleps, lanes 0..2 summed).
inline bool box_overlaps(Guest& g, uint32_t grid, uint32_t id, const X& qlo, const X& qhi) {
    const X a = cmpleps(ld(g, g.u32(grid + 0x41c) + id * 16), qhi);
    const uint32_t s = a.u[1] + a.u[2] + a.u[0];
    if (s != 0xfffffffdu) return false;
    const X b = cmpleps(qlo, ld(g, g.u32(grid + 0x420) + id * 16));
    return b.u[1] + b.u[2] + b.u[0] == s;
}

// 00883020 (custom: ESI grid, EDX min*; stack: max*; RET 4): as 00882d90, keeping the ids whose boxes overlap the query box.
void grid_query_boxes(Guest& g, uint32_t grid, uint32_t lo_p, uint32_t hi_p) {
    const X qlo = ld(g, lo_p), qhi = ld(g, hi_p);
    g.u32(grid + 0x400, 0);
    const Cells c = box_cells(g, grid, ld(g, lo_p), ld(g, hi_p));
    if (c.outside || c.partial) {
        for (int32_t i = 0; i < g.i32(grid + 0x478); i++) {
            const uint32_t id = g.u32(g.u32(grid + 0x47c) + 4 * i);
            if (!box_overlaps(g, grid, id, qlo, qhi)) continue;
            const uint32_t n = g.u32(grid + 0x400);
            if (n != 0x100) { g.u32(grid + 4 * n, id); g.u32(grid + 0x400, n + 1); }
        }
        if (c.outside) return;
    }
    for (int32_t z = c.z0; z <= c.z1; z++) {
        const uint32_t row = g.u32(grid + 0x404) * uint32_t(z);
        for (int32_t x = c.x0; x <= c.x1; x++) {
            const int32_t cnt = g.i32(g.u32(grid + 0x474) + (uint32_t(x) + row) * 4);
            const uint32_t at = (uint32_t(x) + row) * g.u32(grid + 0x414) * 4;
            for (int32_t k = 0; k < cnt; k++) {
                const uint32_t id = g.u32(at + 4 * uint32_t(k) + g.u32(grid + 0x470));
                if (!box_overlaps(g, grid, id, qlo, qhi)) continue;
                result_add(g, grid, id, true);
            }
        }
    }
}

// Overflow list insert (unless present; the count capped at +0x418, the id at the last slot) / remove (the last match swapped
// with the last entry, which becomes -1).
void overflow_insert(Guest& g, uint32_t grid, uint32_t id) {
    const int32_t n = g.i32(grid + 0x478);
    bool found = false;
    for (int32_t i = 0; i < n; i++) if (g.u32(g.u32(grid + 0x47c) + 4 * i) == id) found = true;
    if (found) return;
    int32_t cap = g.i32(grid + 0x418), m = n + 1 < cap ? n + 1 : cap;
    g.u32(grid + 0x478, uint32_t(m));
    g.u32(g.u32(grid + 0x47c) + uint32_t(m) * 4 - 4, id);
}
void overflow_remove(Guest& g, uint32_t grid, uint32_t id) {
    const int32_t n = g.i32(grid + 0x478);
    int32_t at = -1;
    for (int32_t i = 0; i < n; i++) if (g.u32(g.u32(grid + 0x47c) + 4 * i) == id) at = i;
    if (at < 0) return;
    const uint32_t list = g.u32(grid + 0x47c);
    g.u32(list + uint32_t(at) * 4, g.u32(list + uint32_t(n) * 4 - 4));
    g.u32(g.u32(grid + 0x47c) + uint32_t(n) * 4 - 4, 0xffffffffu);
    g.u32(grid + 0x478, g.u32(grid + 0x478) - 1);
}

// 008826d0 (custom: ESI grid; stack: id; RET 4): inserts the id's box into the cells it covers (unless listed there; a cell's
// count is capped at +0x414, the id going to its last slot) and into the overflow list when it reaches outside.
void grid_insert(Guest& g, uint32_t grid, uint32_t id) {
    const Cells c = box_cells(g, grid, ld(g, g.u32(grid + 0x41c) + id * 16), ld(g, g.u32(grid + 0x420) + id * 16));
    if (c.outside || c.partial) {
        overflow_insert(g, grid, id);
        if (c.outside) return;
    }
    for (int32_t x = c.x0; x <= c.x1; x++) {
        for (int32_t z = c.z0; z <= c.z1; z++) {
            const uint32_t cell = g.u32(grid + 0x404) * uint32_t(z) + uint32_t(x);
            const int32_t cap = g.i32(grid + 0x414);
            const uint32_t base = uint32_t(cap) * cell;
            const uint32_t cnt_p = g.u32(grid + 0x474) + cell * 4;
            if (g.i32(cnt_p) > 0) {
                const int32_t cnt = g.i32(g.u32(grid + 0x474) + cell * 4);
                const uint32_t items = g.u32(grid + 0x470) + base * 4;
                bool found = false;
                for (int32_t k = 0; k < cnt; k++) if (g.u32(items + 4 * uint32_t(k)) == id) found = true;
                if (found) continue;
            }
            const int32_t n = g.i32(cnt_p) + 1;
            g.u32(cnt_p, uint32_t(n < cap ? n : cap));
            const uint32_t last = g.u32(g.u32(grid + 0x474) + cell * 4) + base;
            g.u32(g.u32(grid + 0x470) + last * 4 - 4, id);
        }
    }
}

// 00882980 (custom: ESI grid; stack: id; RET 4): removes the id from the cells its (current) box covers and from the
// overflow list (the last match swapped with the cell's last entry, which becomes -1). The original also floors the box a
// second time into dead locals (FUN_0042ccd0 / 0069b2f0 / 0069b400); without effect, not repeated.
void grid_remove(Guest& g, uint32_t grid, uint32_t id) {
    const Cells c = box_cells(g, grid, ld(g, g.u32(grid + 0x41c) + id * 16), ld(g, g.u32(grid + 0x420) + id * 16));
    if (c.outside || c.partial) {
        overflow_remove(g, grid, id);
        if (c.outside) return;
    }
    for (int32_t x = c.x0; x <= c.x1; x++) {
        for (int32_t z = c.z0; z <= c.z1; z++) {
            const uint32_t cell = g.u32(grid + 0x404) * uint32_t(z) + uint32_t(x);
            const uint32_t base = g.u32(grid + 0x414) * cell;
            const int32_t cnt = g.i32(g.u32(grid + 0x474) + cell * 4);
            if (cnt <= 0) continue;
            int32_t at = -1;
            const uint32_t items = g.u32(grid + 0x470) + base * 4;
            for (int32_t k = 0; k < cnt; k++) if (g.u32(items + 4 * uint32_t(k)) == id) at = k;
            if (at < 0) continue;
            const uint32_t last = (uint32_t(cnt) + base - 1) * 4;
            g.u32(g.u32(grid + 0x470) + (uint32_t(at) + base) * 4, g.u32(last + g.u32(grid + 0x470)));
            g.u32(last + g.u32(grid + 0x470), 0xffffffffu);
            const uint32_t cp = g.u32(grid + 0x474) + cell * 4;
            g.u32(cp, g.u32(cp) - 1);
        }
    }
}

}  // namespace

REFLEX_PORT_FN(0x00882170, Int4GreaterMask, "collision", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    Guest g{m};
    const X a = xr(g, 0), b = xr(g, 1), r = gt_mask(a, b);
    xw(g, 0, r);
    m.r[0] = r.u[3]; m.r[1] = a.u[3]; m.r[2] = a.u[2];
    return m.ret(0);
}

REFLEX_PORT_FN(0x008821e0, Int4BelowFlags, "collision", OUT_EAX | OUT_ECX | OUT_XMM0) {
    Guest g{m};
    const LtFlags f = lt_flags(xr(g, 0), xr(g, 1));
    const uint32_t p = m.r[2];
    uint32_t w = g.u32(p) & 0xffffff0fu;
    g.u32(p, w);
    if (f.all) { w |= 0x80; g.u32(p, w); }
    if (f.none) g.u32(p, g.u32(p) | 0x20);
    xw(g, 0, f.mask);
    m.r[0] = w; m.r[1] = (f.mask.u[1] & 0xffffff00u) | (f.none ? 1u : 0u);
    return m.ret(0);
}

REFLEX_PORT_FN(0x00882290, Int4Min, "collision", OUT_EAX | OUT_ECX | OUT_XMM0) {
    Guest g{m};
    const X a = xr(g, 0), b = xr(g, 1);
    xw(g, 0, imin(a, b)); m.r[0] = a.u[3]; m.r[1] = b.u[3];
    return m.ret(0);
}

REFLEX_PORT_FN(0x00882300, Int4Max, "collision", OUT_EAX | OUT_ECX | OUT_XMM0) {
    Guest g{m};
    const X a = xr(g, 0), b = xr(g, 1);
    xw(g, 0, imax(a, b)); m.r[0] = a.u[3]; m.r[1] = b.u[3];
    return m.ret(0);
}

REFLEX_PORT_FN(0x00882d90, BroadphaseGrid_QueryCells, "collision", 0) {
    Guest g{m};
    grid_query_cells(g, m.r[6], m.arg(0), m.r[1]);
    return m.ret(4);
}

REFLEX_PORT_FN(0x00883020, BroadphaseGrid_QueryBoxes, "collision", 0) {
    Guest g{m};
    grid_query_boxes(g, m.r[6], m.r[2], m.arg(0));
    return m.ret(4);
}

REFLEX_PORT_FN(0x008826d0, BroadphaseGrid_Insert, "collision", 0) {
    Guest g{m};
    grid_insert(g, m.r[6], m.arg(0));
    return m.ret(4);
}

REFLEX_PORT_FN(0x00882980, BroadphaseGrid_Remove, "collision", 0) {
    Guest g{m};
    grid_remove(g, m.r[6], m.arg(0));
    return m.ret(4);
}

// 0084a1a0 (custom: EDI id; stack: object*, min*, max*; RET 0xc): moves an id's box in the object's grid (+0x30): removed,
// the new min / max stored, inserted again.
REFLEX_PORT_FN(0x0084a1a0, BroadphaseGrid_Update, "collision", 0) {
    Guest g{m};
    const uint32_t id = m.r[7], grid = g.u32(m.arg(0) + 0x30), lo = m.arg(1), hi = m.arg(2);
    grid_remove(g, grid, id);
    st(g, g.u32(grid + 0x41c) + id * 16, ld(g, lo));
    st(g, g.u32(grid + 0x420) + id * 16, ld(g, hi));
    grid_insert(g, grid, id);
    return m.ret(0xc);
}

// 0088bd90 (thiscall): the int64 at this in edx:eax.
REFLEX_PORT_FN(0x0088bd90, Int64_Load, "collision", OUT_EAX | OUT_EDX) {
    Guest g{m};
    m.r[0] = g.u32(m.r[1]); m.r[2] = g.u32(m.r[1] + 4);
    return m.ret(0);
}

// 0088bd50 (thiscall: source*; RET 4): copies an int64; eax = this.
REFLEX_PORT_FN(0x0088bd50, Int64_Assign, "collision", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    const uint32_t self = m.r[1], src = m.arg(0);
    const uint32_t lo = g.u32(src);
    g.u32(self, lo);
    const uint32_t hi = g.u32(src + 4);
    g.u32(self + 4, hi);
    m.r[0] = self; m.r[1] = hi; m.r[2] = lo;
    return m.ret(4);
}

// 0088be10 (thiscall: out*, int64 addend (lo, hi); RET 0xc): out = this + addend (add / adc); eax = out.
REFLEX_PORT_FN(0x0088be10, Int64_Add, "collision", OUT_EAX | OUT_ECX | OUT_EDX) {
    Guest g{m};
    const uint32_t self = m.r[1], out = m.arg(0);
    const uint64_t a = uint64_t(g.u32(self)) | uint64_t(g.u32(self + 4)) << 32;
    const uint64_t s = a + (uint64_t(m.arg(1)) | uint64_t(m.arg(2)) << 32);
    g.u32(out, uint32_t(s)); g.u32(out + 4, uint32_t(s >> 32));
    m.r[0] = out; m.r[1] = uint32_t(s >> 32); m.r[2] = uint32_t(s);
    return m.ret(0xc);
}
