/* world_wreck.c -- float properties with listeners (0x0063b0e0-0x0063b8f0) and the uniform cell grid of the terrain-side
 * collision items (0x00667e40-0x006689c0).
 *
 * A listener vector is a checked std::vector of 8-byte elements {float* target, int enabled}: +0 the container proxy (its +0 is
 * the container, whose +0x10 bounds the iterators), +0xc begin, +0x10 end. Every iterator check calls the CRT's invalid-parameter
 * abort as the original does (never fires in practice).
 * The grid (0x00668xxx): +0 the level object ([+0x9c] -> +0x44 the level data, +0x4c a busy flag), +4 cells (128-byte records:
 * +0 items (48 bytes each: centre, +0x10 radius, +0x14 angle), +0x10.. per-layer item ranges, +0x60 centre, +0x70 half extent),
 * +8 layer count, +0xc width, +0x10 cell count, +0x18 / +0x1c inverse cell sizes, +0x24 layer objects (16 bytes each: +0 the
 * receiver-2 object, +8 the receiver-1 object), +0x160 layer flags (2 bytes each), +0x188 the 3x3 neighbourhood cells, +0x1ac
 * their hit flags, +0x1d0 the hit count. */
#include "world_wreck.h"
#include "crt.h"
#include "world_util.h"
#include "world_extern.h"

static void crt_abort(void) { crt_invalid_parameter_noinfo(); }

/* ---- listener vectors ---------------------------------------------------------------------------------------------- */

/* the checked-iterator test of the element at p: the proxy is set and p is below its container's bound */
static void check_element(rx_addr proxy, rx_addr p) {
    if (proxy == 0) crt_abort();
    if (!(p < U32(U32(proxy, 0), 0x10))) crt_abort();
}

/* 0063b830 FloatListeners_Set (stdcall listeners, float* value; RET 8): every enabled listener's target gets the value (fld /
 * fstp: a signalling NaN arrives quiet). */
void rx_world_float_listeners_set(rx_addr v, rx_addr value) {
    const uint32_t bits = U32(value, 0);
    const rx_addr begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return;
    if (begin > end) crt_abort();
    const rx_addr proxy = U32(v, 0);
    for (rx_addr p = begin;; p += 8) {
        const rx_addr last = U32(v, 0x10);
        if (U32(v, 0xc) > last) crt_abort();
        if (proxy == 0 || proxy != U32(v, 0)) crt_abort();
        if (p == last) break;
        check_element(proxy, p);
        if (U32(p, 4) != 0) {
            check_element(proxy, p);
            SU32(U32(p, 0), 0, world_x87_pass(bits));
        }
        check_element(proxy, p);
    }
}
RX_FUNCTION(0x0063b830, rx_world_float_listeners_set)

/* 0063b8f0 FloatListeners_Get (stdcall listeners; RET 4): xmm0 = the first enabled listener's value (0 when none). */
float rx_world_float_listeners_get(rx_addr v) {
    const rx_addr begin = U32(v, 0xc), end = U32(v, 0x10);
    if (((end - begin) & 0xfffffff8u) == 0) return rx_bf(0);
    if (begin > end) crt_abort();
    const rx_addr proxy = U32(v, 0);
    for (rx_addr p = begin;; p += 8) {
        const rx_addr last = U32(v, 0x10);
        if (U32(v, 0xc) > last) crt_abort();
        if (proxy == 0 || proxy != U32(v, 0)) crt_abort();
        if (p == last) break;
        check_element(proxy, p);
        if (U32(p, 4) != 0) {
            check_element(proxy, p);
            return F32(U32(p, 0), 0);
        }
        check_element(proxy, p);
    }
    return rx_bf(0);
}
RX_FUNCTION(0x0063b8f0, rx_world_float_listeners_get)

/* ---- ranged / clamped / wrapped floats --------------------------------------------------------------------------------
 * The ranged float {min +0, max +4, rate +8, limit +0xc, value +0x10, listeners +0x14}: the value moves toward the input by
 * min(rate, limit) / rate of the gap; the listeners get its position in [min, max] as 0..1. The clamps keep the original's
 * comparison forms (a NaN passes through). */
static float clamp_to(float x, float lo, float hi) {
    if (x >= hi) x = hi;
    if (lo >= x) x = lo;
    return x;
}
static float ranged_step(rx_addr obj, rx_addr input) {
    const float rate = F32(obj, 8), limit = F32(obj, 0xc);
    float step = rate;
    if (step > limit) step = limit;
    const float value = F32(obj, 0x10);
    const float next = phys_add(phys_mul(phys_sub(F32(input, 0), value), phys_div(step, rate)), value);
    SF32(obj, 0x10, next);
    const float hi = F32(obj, 4), lo = F32(obj, 0);
    return phys_div(phys_sub(clamp_to(next, lo, hi), lo), phys_sub(hi, lo));
}
/* the broadcast of a value through a local the listeners read by address (the original's stack slot `slot`) */
static void broadcast(rx_addr listeners, rx_addr slot, float value) {
    SF32(slot, 0, value);
    WORLD_CALL_SP(8, rx_world_float_listeners_set(listeners, slot));
}

/* 0063b0e0 RangedFloat_Get (EAX = ranged float; RET 0): its listeners' value (xmm0). */
float rx_world_ranged_float_get(rx_addr obj) {
    float r;
    WORLD_CALL_SP(4, r = rx_world_float_listeners_get(obj + 0x14));
    return r;
}
RX_FUNCTION(0x0063b0e0, rx_world_ranged_float_get)

/* 0063b0f0 RangedFloat_Step (ESI = ranged float, EAX = float* input; RET 0): step toward the input and broadcast. */
void rx_world_ranged_float_step(rx_addr obj, rx_addr input) {
    world_frame f = world_frame_flat(0, 4);   /* push ecx: the broadcast slot */
    broadcast(obj + 0x14, f.esp, ranged_step(obj, input));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0063b0f0, rx_world_ranged_float_step)

/* 0063b160 RangedFloat_StepAny (EAX = ranged float, ECX = float* input; RET 0; EAX = 1 when it has listeners): as 0063b0f0. */
int rx_world_ranged_float_step_any(rx_addr obj, rx_addr input) {
    world_frame f = world_frame_flat(0, 4);
    broadcast(obj + 0x14, f.esp, ranged_step(obj, input));
    world_frame_leave(&f);
    return ((U32(obj, 0x24) - U32(obj, 0x20)) & 0xfffffff8u) != 0;
}
RX_FUNCTION(0x0063b160, rx_world_ranged_float_step_any)

/* 0063b480 ClampedFloat_Write (EAX = {min, max, float* out}, ECX = float* input; RET 0): *out = clamp(*input) when out is set. */
void rx_world_clamped_float_write(rx_addr obj, rx_addr input) {
    const float v = clamp_to(F32(input, 0), F32(obj, 0), F32(obj, 4));
    const rx_addr out = U32(obj, 8);
    if (out) SF32(out, 0, v);
}
RX_FUNCTION(0x0063b480, rx_world_clamped_float_write)

/* 0063b500 ClampedFloat_Diff (ECX = {min, max, float* src}; stdcall float* b; RET 4): xmm0 = clamp((src ? *src : 0) - *b). */
float rx_world_clamped_float_diff(rx_addr obj, rx_addr b) {
    const rx_addr src = U32(obj, 8);
    const float a = src ? F32(src, 0) : rx_bf(0);
    return clamp_to(phys_sub(a, F32(b, 0)), F32(obj, 0), F32(obj, 4));
}
RX_FUNCTION(0x0063b500, rx_world_clamped_float_diff)

/* 0063b540 ClampedFloat_Set (ESI = {min, max, listeners +8}, EAX = float* input; RET 0): broadcast clamp(*input). */
void rx_world_clamped_float_set(rx_addr obj, rx_addr input) {
    world_frame f = world_frame_flat(0, 4);
    broadcast(obj + 8, f.esp, clamp_to(F32(input, 0), F32(obj, 0), F32(obj, 4)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x0063b540, rx_world_clamped_float_set)

/* 0063b590 ClampedFloat_Add (ESI = {min, max, listeners +8}; stdcall float* delta; RET 4): broadcast clamp(current + *delta)
 * (current = the listeners' value); the original broadcasts through its own argument slot (written with the value). */
void rx_world_clamped_float_add(rx_addr obj, rx_addr delta) {
    const rx_addr slot = rx_sp() + 4;
    float cur;
    WORLD_CALL_SP(4, cur = rx_world_float_listeners_get(obj + 8));
    const float sum = phys_add(F32(delta, 0), cur);
    const float hi = F32(obj, 4), lo = F32(obj, 0);
    float v = hi;
    if (!(sum >= hi)) v = sum;
    if (lo >= v) v = lo;
    broadcast(obj + 8, slot, v);
}
RX_FUNCTION(0x0063b590, rx_world_clamped_float_add)

/* 0063b650 WrappedFloat_Add (ESI = {min, max, float* rate +0xc, listeners +0x10}; stdcall float* delta; RET 4): broadcast
 * current + *delta * rate, wrapped once into [min, max] (through its argument slot as 0063b590). */
void rx_world_wrapped_float_add(rx_addr obj, rx_addr delta) {
    const rx_addr slot = rx_sp() + 4;
    float cur;
    WORLD_CALL_SP(4, cur = rx_world_float_listeners_get(obj + 0x10));
    const rx_addr rate_p = U32(obj, 0xc);
    const float rate = rate_p ? F32(rate_p, 0) : rx_bf(0);
    float v = phys_add(phys_mul(F32(delta, 0), rate), cur);
    const float hi = F32(obj, 4);
    if (v > hi) v = phys_sub(v, hi);
    else {
        const float lo = F32(obj, 0);
        if (lo > v) v = phys_add(phys_sub(v, lo), hi);
    }
    broadcast(obj + 0x10, slot, v);
}
RX_FUNCTION(0x0063b650, rx_world_wrapped_float_add)

/* 0063b6c0 ShortProperty_Set (EAX = {?, byte* changed +4, short* out +8}, EDX = short* input; RET 0). */
void rx_world_short_property_set(rx_addr obj, rx_addr input) {
    if (U32(obj, 4)) SU8(U32(obj, 4), 0, 1);
    if (U32(obj, 8)) SU16(U32(obj, 8), 0, U16(input, 0));
}
RX_FUNCTION(0x0063b6c0, rx_world_short_property_set)

/* ---- the uniform cell grid ------------------------------------------------------------------------------------------- */

/* 00668910 Grid_CellContains (thiscall grid; point, cell; RET 8): 1 when the cell is used (+8) and the point's x / z lie in the
 * cell's box in x and z (centre +0x60, half extent +0x70). */
int rx_world_grid_cell_contains(rx_addr grid, rx_addr point, uint32_t cell) {
    const rx_addr rec = (cell << 7) + U32(grid, 4);
    if (U32(rec, 8) == 0) return 0;
    const rx_v4 half = v4_ld(rec + 0x70), centre = v4_ld(rec + 0x60), p0 = v4_ld(point), z = v4_ld(WORLD_ZERO4);
    const rx_v4 hi0 = v4_add(half, centre), lo0 = v4_sub(centre, half);
    /* the y lane from the zero vector (shufps z, v, 0xe5 then movss): x and z are compared */
    const rx_v4 p = v4(p0.v[0], z.v[1], p0.v[2], p0.v[3]);
    const rx_v4 hi = v4(hi0.v[0], z.v[1], hi0.v[2], hi0.v[3]), lo = v4(lo0.v[0], z.v[1], lo0.v[2], lo0.v[3]);
    if (!v4_all3(v4_cmple(p, hi))) return 0;
    return v4_all3(v4_cmple(lo, p));
}
RX_FUNCTION(0x00668910, rx_world_grid_cell_contains)

/* 00668820 Grid_CellOf (ECX = grid, EDX = point; RET 0): the cell index of the point (row from z, column from x), -1 outside the
 * grid. The level's busy flag (+0x4c) is set while the level bounds are read. */
uint32_t rx_world_grid_cell_of(rx_addr grid, rx_addr point) {
    const rx_addr level_obj = U32(U32(grid, 0), 0x9c);
    const float m1 = F32(0, 0x009cd9e8);   /* -1 (a runtime global) */
    SU32(level_obj, 0x4c, 1);
    const rx_addr level = U32(level_obj, 0x44);
    const float min_x = phys_add(F32(level, 0x24b00), phys_mul(F32(level, 0x24af0), m1));
    const float min_z = phys_add(F32(level, 0x24b08), phys_mul(F32(level, 0x24af8), m1));
    const float dx = phys_sub(F32(point, 0), min_x), dz = phys_sub(F32(point, 8), min_z);
    const float zero = rx_bf(0);
    int outside = zero > dz || zero > dx;
    if (!outside) {
        outside = dz > phys_mul(F32(level, 0x24af8), 2.0f /* 0x92e330 */);
        if (!outside) outside = dx > phys_mul(F32(level, 0x24af0), 2.0f);
    }
    if (outside) { SU32(U32(U32(grid, 0), 0x9c), 0x4c, 0); return 0xffffffffu; }
    const float col = phys_mul(F32(grid, 0x18), dz);
    SU32(U32(U32(grid, 0), 0x9c), 0x4c, 0);
    const int32_t row = rx_cvtt(phys_mul(F32(grid, 0x1c), dx));
    return U32(grid, 0xc) * (uint32_t)row + (uint32_t)rx_cvtt(col);
}
RX_FUNCTION(0x00668820, rx_world_grid_cell_of)

/* 00668710 Grid_Neighbours (ECX = grid, EDX = cell; RET 0; EAX = 9): the 3x3 neighbourhood of the cell into +0x188 (centre,
 * up, up-right, right, down-right, down, down-left, left, up-left; -1 outside the grid), the 9 flags +0x1ac = 9, the hit count
 * +0x1d0 = 0. */
void rx_world_grid_neighbours(rx_addr grid, uint32_t cell) {
    const uint32_t width = U32(grid, 0xc), down = width + cell, up = cell - width;
    SU32(grid, 0x198, down + 1);
    SU32(grid, 0x18c, up);
    SU32(grid, 0x190, up + 1);
    SU32(grid, 0x1a0, down - 1);
    SU32(grid, 0x1a8, up - 1);
    SU32(grid, 0x188, cell);
    SU32(grid, 0x194, cell + 1);
    SU32(grid, 0x19c, down);
    SU32(grid, 0x1a4, cell - 1);
    if ((int32_t)up < 0) { SU32(grid, 0x1a8, 0xffffffffu); SU32(grid, 0x190, 0xffffffffu); SU32(grid, 0x18c, 0xffffffffu); }
    if ((int32_t)down > (int32_t)(U32(grid, 0x10) - 1)) { SU32(grid, 0x1a0, 0xffffffffu); SU32(grid, 0x19c, 0xffffffffu); SU32(grid, 0x198, 0xffffffffu); }
    if ((int32_t)(cell + 1) % (int32_t)width == 0) { SU32(grid, 0x198, 0xffffffffu); SU32(grid, 0x194, 0xffffffffu); SU32(grid, 0x190, 0xffffffffu); }
    if ((int32_t)(cell - 1) % (int32_t)width == (int32_t)(width - 1)) {
        SU32(grid, 0x1a8, 0xffffffffu); SU32(grid, 0x1a4, 0xffffffffu); SU32(grid, 0x1a0, 0xffffffffu);
    }
    for (uint32_t i = 0; i < 9; i++) SU32(grid, 0x1ac + 4 * i, 9);
    SU32(grid, 0x1d0, 0);
}
RX_FUNCTION(0x00668710, rx_world_grid_neighbours)

/* 006689c0 Grid_TestNeighbours (EDI = grid; stdcall point; RET 4; AL): tests the point against the neighbourhood cells, each
 * flag (+0x1ac) 0 / 1, the hits counted in +0x1d0. The loop bound shrinks as side cells hit (the original's ladder: up hit ->
 * down-right / down / down-left skipped; up-right -> stop after right; right / down-right -> stop after down-left). Returns 1
 * when the centre cell holds the point, else 1 when nothing did. */
int rx_world_grid_test_neighbours(rx_addr grid, rx_addr point) {
    uint32_t bound = 9;
    int up_hit = 0;
    for (uint32_t i = 0; i < bound; i++) {
        SU32(grid, 0x1ac + 4 * i, 0);
        if (up_hit && (i == 4 || i == 5 || i == 6)) continue;
        const uint32_t cell = U32(grid, 0x188 + 4 * i);
        if ((int32_t)cell < 0) continue;
        int in;
        WORLD_CALL_SP(8, in = rx_world_grid_cell_contains(grid, point, cell));
        if (!in) continue;
        SU32(grid, 0x1ac + 4 * i, 1);
        SU32(grid, 0x1d0, U32(grid, 0x1d0) + 1);
        if (i == 1) up_hit = 1;
        else if (i == 2) bound = 4;
        else if (i == 3 || i == 4) bound = 6;
    }
    if (U32(grid, 0x1ac) == 1) return 1;
    return U32(grid, 0x1d0) == 0;
}
RX_FUNCTION(0x006689c0, rx_world_grid_test_neighbours)

/* 00668690 Grid_Locate (EAX = grid; stdcall point; RET 4; EAX = 0, or -1 outside the grid): the point's cell and its
 * neighbourhood; when no cell held it, recentre on the first neighbour that did and test again. */
int32_t rx_world_grid_locate(rx_addr grid, rx_addr point) {
    uint32_t cell;
    WORLD_CALL_SP(0, cell = rx_world_grid_cell_of(grid, point));
    if ((int32_t)cell < 0) return -1;
    WORLD_CALL_SP(0, rx_world_grid_neighbours(grid, cell));
    int ok;
    WORLD_CALL_SP(4, ok = rx_world_grid_test_neighbours(grid, point));
    if (!ok) {
        uint32_t centre = 0;
        for (uint32_t i = 1; i < 9; i++)
            if (U32(grid, 0x1ac + 4 * i) == 1) { centre = U32(grid, 0x188 + 4 * i); break; }
        WORLD_CALL_SP(0, rx_world_grid_neighbours(grid, centre));
        WORLD_CALL_SP(4, rx_world_grid_test_neighbours(grid, point));
    }
    return 0;
}
RX_FUNCTION(0x00668690, rx_world_grid_locate)

/* 00668650 Grid_CollectHits (EAX = grid, EDX = cell list ending in -1; RET 0): every neighbourhood cell that held the point is
 * added to the list unless already there (the caller's list is pre-filled with -1: no terminator is written). */
void rx_world_grid_collect_hits(rx_addr grid, rx_addr list) {
    for (uint32_t n = 0; n < 9; n++) {
        const rx_addr slot = grid + 0x188 + 4 * n;
        if (U32(slot, 0x24) != 1) continue;
        uint32_t k = 0;
        if (U32(list, 0) != 0xffffffffu) {
            const uint32_t cell = U32(slot, 0);
            while (U32(list, 4 * k) != cell) {
                k++;
                if (U32(list, 4 * k) == 0xffffffffu) break;
            }
        }
        SU32(list, 4 * k, U32(slot, 0));
    }
}
RX_FUNCTION(0x00668650, rx_world_grid_collect_hits)

/* ---- the item rotation of Grid_ReportOverlaps: a vectorized cosine (0x935ad0..0x935b60) ----------------------------------
 * cos_poly(x) = cos of the reduced angle: |x| reduced modulo 2 pi (lane-wise truncation), t = pi - that, then the polynomial
 * ((c5 t^2 + c4) t^4 + (c3 t^2 + c2)) t^4 + (c1 t^2 + c0) in the original's order (it approximates -cos). */
static rx_v4 trunc_lanes(rx_v4 x) {
    rx_v4 r;
    for (int i = 0; i < 4; i++) r.v[i] = (float)rx_cvtt(x.v[i]);
    return r;
}
static rx_v4 reduced_square(rx_v4 x) {
    const rx_v4 a = v4_max(v4_sub(v4_zero(), x), x);
    const rx_v4 k = trunc_lanes(v4_mul(a, v4_ld(0x00935ad0) /* 1 / 2pi */));
    const rx_v4 r = v4_sub(a, v4_mul(k, v4_ld(0x00935ae0) /* 2pi */));
    const rx_v4 t = v4_sub(v4_ld(0x00935af0) /* pi */, r);
    return v4_mul(t, t);
}
static rx_v4 cos_poly(rx_v4 t2, rx_v4 t4) {
    rx_v4 a = v4_mul(v4_add(v4_mul(t2, v4_ld(0x00935b60)), v4_ld(0x00935b50)), t4);
    a = v4_mul(v4_add(a, v4_add(v4_mul(t2, v4_ld(0x00935b40)), v4_ld(0x00935b30))), t4);
    return v4_add(a, v4_add(v4_mul(t2, v4_ld(0x00935b20)), v4_ld(0x00935b10)));
}

/* 00667e40 Grid_ReportOverlaps (ECX = shape (bounds vfunc +0xc), EDX = contact set 2; stdcall grid, position, rotation,
 * out 2 (id*), contact set 1, out 1 (id*); RET 0x18): the terrain-side collision items (trees, rocks: 48-byte records with a
 * centre, a radius and an angle) near a swept shape. The shape's bounds give the swept extent; the grid cells around four probe
 * points are collected; then for every layer of every collected cell, every item whose box (centre +- radius * the layer's scale)
 * overlaps the swept range in x and z is reported: its rotation about the up axis (from its angle, through the cosine
 * polynomial) and centre go to the narrow phase (0085ddd0) against the layer's collision object, bracketed by that object's
 * vfuncs +0x10 (with the item's radius) and +0x14; contact set 1 for layers with flag +1 (then *out1 = the layer's id), set 2 for
 * flag +0 (*out2). */
void rx_world_grid_report_overlaps(rx_addr shape, rx_addr set2, rx_addr grid, rx_addr pos, rx_addr rot, rx_addr out2, rx_addr set1,
                                   rx_addr out1) {
    world_frame f = world_frame_enter(0x164, 3, 1);
    const rx_addr P = f.esp, bmin = P + 0x110, bmax = P + 0xf0, list = P + 0x80, probes = P + 0xa0;
    const rx_addr item_rot = P + 0x120, item_pos = P + 0x130;
    WORLD_CTX(U32(U32(shape, 0), 0xc), &WCTX(.ecx = shape, .edx = U32(shape, 0), .eax = U32(U32(shape, 0), 0xc), .ebx = pos,
                                              .esi = bmin, .edi = grid), 0, rot, pos, bmin, bmax);
    const rx_v4 D = v4_sub(v4_ld(bmax), v4_ld(pos));   /* the swept half extent */
    const float m1 = F32(0, 0x009cd9e8);              /* -1 (a runtime global) */
    const float px = F32(pos, 0), py = F32(pos, 4), pz = F32(pos, 8);
    const float dzm = phys_mul(D.v[2], m1);
    /* the four probe points (x, y, z; w as the stack holds it): the bounds' min, pos + D (z mirrored), the bounds' max, pos - D */
    SU32(probes, 0, U32(bmin, 0)); SU32(probes, 4, U32(bmin, 4)); SU32(probes, 8, U32(bmin, 8));
    SF32(probes, 0x10, phys_add(px, D.v[0])); SF32(probes, 0x14, phys_add(py, D.v[1])); SF32(probes, 0x18, phys_add(pz, dzm));
    SU32(probes, 0x20, U32(bmax, 0)); SU32(probes, 0x24, U32(bmax, 4)); SU32(probes, 0x28, U32(bmax, 8));
    SF32(probes, 0x30, phys_add(px, phys_mul(D.v[0], m1))); SF32(probes, 0x34, phys_add(py, phys_mul(D.v[1], m1)));
    SF32(probes, 0x38, phys_add(pz, phys_mul(dzm, m1)));
    for (int i = 0; i < 8; i++) SU32(list, 4 * i, 0xffffffffu);
    for (int i = 0; i < 4; i++) {
        WORLD_CALL_SP(4, rx_world_grid_locate(grid, probes + 16u * (uint32_t)i));
        WORLD_CALL_SP(0, rx_world_grid_collect_hits(grid, list));
    }
    if (U32(list, 0) == 0xffffffffu) { world_frame_leave(&f); return; }
    const rx_addr level_obj = U32(U32(grid, 0), 0x9c);
    const rx_addr level = U32(level_obj, 0x44);
    SU32(level_obj, 0x4c, 1);
    for (uint32_t k = 0; U32(list, 4 * k) != 0xffffffffu; k++) {
        const rx_addr rec = U32(list, 4 * k) << 7;
        if (I32(grid, 8) <= 0) continue;
        rx_addr flags = grid + 0x160, layer = level + 0x1d588, objs = grid + 0x24, range = rec + 0x10;
        for (int32_t j = 0; j < I32(grid, 8); j++, flags += 2, layer += 0x5f0, objs += 0x10, range += 4) {
            const rx_v4 scale = v4_ld(layer - 0x78);
            const rx_addr cells = U32(grid, 4);
            const int32_t first = I32(range + cells, -4), end = I32(range + cells, 0);
            for (int32_t it = first; it < end; it++) {
                const rx_addr items = U32(rec + U32(grid, 4), 0);
                const rx_addr item = items + 0x30u * (uint32_t)it + 0x10;   /* radius +0, angle +4 */
                const rx_v4 centre = v4_ld(item - 0x10);
                const rx_v4 r = v4_mul(v4_lane(v4_ld(item), 0), scale);
                const rx_v4 hi = v4_add(centre, r), lo = v4_sub(centre, r);
                const rx_v4 swept_hi = v4_add(D, v4_ld(pos)), swept_lo = v4_sub(v4_ld(pos), D);
                if (hi.v[2] < swept_lo.v[2] || swept_hi.v[2] < lo.v[2] || hi.v[0] < swept_lo.v[0] || swept_hi.v[0] < lo.v[0]) continue;
                /* the item's rotation about the up axis: (cos-poly(h + pi/2) * up, -cos-poly(h)), h = (angle - pi) / 2 */
                const rx_v4 up = v4_shuf(v4_splat(1.0f), v4_splat(rx_bf(0) /* 0x92e12c */), 0xee);
                const rx_v4 axis0 = v4_movss(up, v4_splat(rx_bf(0)));                  /* (0, 1, 0, 0) */
                const rx_v4 len2 = v4_splat(phys_hsum3(v4_mul(axis0, axis0)));
                const rx_v4 len = v4_sqrt(len2), finite = v4_cmpneq(len2, v4_ld(WORLD_INF4));
                const rx_v4 h = v4_mul(v4_splat(phys_sub(F32(item, 4), 3.14159274f /* 0x977498 */)), v4_splat(0.5f /* 0x9322cc */));
                const rx_v4 u2 = reduced_square(v4_add(h, v4_ld(0x00935b00) /* pi / 2 */)), u4 = v4_mul(u2, u2);
                const rx_v4 w2 = reduced_square(h), w4 = v4_mul(w2, w2);
                const rx_v4 neg_cos = v4_sub(v4_zero(), cos_poly(w2, w4));
                rx_v4 axis = v4_and(v4_and(v4_div(axis0, len), v4_cmpneq(len, v4_zero())), finite);
                axis = v4_or(axis, v4_andn(finite, v4_ld(WORLD_NAN4)));
                rx_v4 q = v4_mul(cos_poly(u2, u4), axis);
                q.v[3] = neg_cos.v[3];
                for (int pass = 0; pass < 2; pass++) {
                    const int use = pass == 0 ? (U8(flags, 1) != 0 && set1 != 0) : (U8(flags, 0) != 0 && set2 != 0);
                    if (!use) continue;
                    const rx_addr obj = pass == 0 ? U32(objs, 8) : U32(objs, 0), set = pass == 0 ? set1 : set2;
                    const uint32_t edi = pass == 0 ? item : set2;
                    v4_st(item_pos, centre);
                    v4_st(item_rot, q);
                    const uint32_t radius = world_x87_pass(U32(item, 0));
                    /* the shape's vf4: a float stack argument (00848af0 composite / 00843fc0 sphere are registered ports with
                     * (this, float); WORLD_CTX's integer dispatch would abort on them -- 2026-10-20 dispatchfix) */
                    WORLD_CTX_F(U32(U32(obj, 0), 0x10), &WCTX(.ecx = obj, .edx = U32(obj, 0), .ebx = pos, .esi = objs, .edi = item), 0,
                                rx_bf(radius));
                    world_contact_dispatch(set, shape, rot, pos, obj, item_rot, item_pos, 0,
                                           WCTX(.eax = obj, .edx = shape, .ebx = pos, .esi = objs, .edi = edi));
                    WORLD_CTX0(U32(U32(obj, 0), 0x14), &WCTX(.ecx = obj, .eax = U32(obj, 0), .edx = U32(U32(obj, 0), 0x14), .ebx = pos,
                                                            .esi = objs, .edi = edi), 0);
                    if (U32(set, 0x6a0) != 0) {
                        if (pass == 0) SU32(out1, 0, U32(layer, 0));
                        else SU32(out2, 0, U32(layer, -0xc));
                    }
                }
            }
        }
    }
    SU32(U32(U32(grid, 0), 0x9c), 0x4c, 0);
    world_frame_leave(&f);
}
RX_FUNCTION(0x00667e40, rx_world_grid_report_overlaps)
