/* phys_terrain.c -- terrain point queries and the track-strip bookkeeping (0x007c3xxx..0x007c7xxx): the material under a point,
 * the base / deformed height at a point, the base height and normal, the coarse height-range reject of a segment, and the
 * track strips' slot allocation and cutting. The terrain object as phys_coll_terrain.c (base grid +0x1c, deformation surface
 * +0x240, gain +0xbc, row stride +0x44bc, cells +0x44c8 / +0x44cc, world size +0x44d0 / +0x44d4, normal y +0x44ec). LTCG register
 * conventions: see each function and oracle/phys_terrain.cpp. */
#include "phys_terrain.h"
#include "phys_coll_terrain.h"
#include "phys_mech.h"
#include "phys_quat.h"
#include "phys_crt.h"
#include "world_node.h"
#include "world_veh.h"
#include "crt.h"
#include "../src/rx_d3d.h"

#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

/* 005452d0 Vec4_FromScalars (xmm0..xmm3 -> (x3, x2, x1, x0)), inlined: the call sites pass two values twice. */
static void vec_from_pair(rx_addr out, float a, float b) { v4_st(out, v4(b, a, b, a)); }

/* LockRect (vtable slot 13: this, rect*, NULL, flags 0x810) / UnlockRect (slot 14) of the deformation surface; stdcall, called
 * through a register holding the vtable entry (callers of the queries read the EDX left, which the stubs keep). The registers
 * as the original sets them: lock EAX = surface, ECX = rect, EDX = the entry; unlock (success path) ECX = vtable, EDX = the entry,
 * (failure path) EAX = vtable, ECX = the entry, EDX = what it held. Each returns the EDX the call leaves. */
static uint32_t surface_lock(rx_addr tex, rx_addr rect) {
    const uint32_t args[4] = {tex, rect, 0, 0x810};
#ifdef RX_ORACLE
    rx_regs in = phys_regs(), out;
    in.r[0] = tex; in.r[1] = rect; in.r[2] = U32(U32(tex, 0), 0x34);
    rx_call_original_ex(in.r[2], &in, &out, 4, args);
    return out.r[2];
#else
    rx_d3d_call(tex, 13, 4, args);          /* the core's surface: the EDX the stub leaves is only compared against the original */
    return 0;
#endif
}
static uint32_t surface_unlock(rx_addr tex, int success_path, uint32_t edx) {
    const uint32_t args[1] = {tex};
#ifdef RX_ORACLE
    const rx_addr vt = U32(tex, 0), fn = U32(vt, 0x38);
    rx_regs in = phys_regs(), out;
    if (success_path) { in.r[1] = vt; in.r[2] = fn; }
    else { in.r[0] = vt; in.r[1] = fn; in.r[2] = edx; }
    rx_call_original_ex(fn, &in, &out, 1, args);
    return out.r[2];
#else
    (void)success_path;
    rx_d3d_call(tex, 14, 1, args);
    return edx;
#endif
}

/* The grid query the height functions share (each at its own frame offsets): pos = (p.x, -p.z, p.x, -p.z) (the point's
 * (x, z, x, z) times Vec4_FromScalars(-1.0 [0x9cd9e8, writable], 1.0) = (1, -1, 1, -1), built in `signs`) ... */
static void query_position(rx_addr point, rx_addr signs, rx_addr pos) {
    const rx_v4 p = v4_ld(point);
    vec_from_pair(signs, F32(0, 0x009cd9e8), 1.0f /* 0x931f24 */);
    v4_st(pos, v4_mul(v4_shuf(p, p, 0x88), v4_ld(signs)));
}
/* ... dims = (size x, size z, size x, size z), cells = (cells x, cells z, ...), scale = the row stride splat, then
 * TerrainCellSetup (1 when the point is inside the grid; *edx = the EDX it leaves). */
static uint32_t query_cell(rx_addr terrain, rx_addr pos, rx_addr dims, rx_addr cells, rx_addr scale, rx_addr work, uint32_t* edx) {
    vec_from_pair(dims, F32(terrain, 0x44d4), F32(terrain, 0x44d0));
    vec_from_pair(cells, F32(terrain, 0x44cc), F32(terrain, 0x44c8));
    v4_st(scale, v4_splat((float)(int32_t)U32(terrain, 0x44bc)));
    return phys_terrain_cell_setup(work, dims, pos, cells, edx);
}
/* the height result: lane 1 of h replaces the point's y (shufps 0xe5 + movss) */
static void store_height(rx_addr point, rx_v4 h) {
    const rx_v4 p = v4_ld(point);
    v4_st(point, v4_movss(v4_shuf(h, p, 0xe5), p));
}

/* ---- 007c5120 TerrainMaterialAt (EDI = terrain, EDX = point, EAX = layer): the material code of layer `layer` at the point's
 * (x, z): u = (size_x / 2 + x) / size_x, v = (size_z / 2 + (-0 - z)) / size_z, sampled from the material grid ([terrain+0x44ac]
 * +0x44: 007f8d10 with ESI = ECX = grid when the grid's +0x10 is set, else PackedGrid_SampleUV 007f8e00 thiscall), with the busy
 * flag [terrain+0x44ac]+0x4c set around the call. Frame: sub esp, 8; push esi (u / v spilled through the x87 stack: exact).
 * Returns the material (EAX); *out = the callee's registers when out is not NULL; caller = the caller's registers (EBX / EBP / ESI: saved on the stack by
 * the original and its callees -- dead-stack words later code reads; NULL: zeros). */
uint32_t phys_terrain_material_at(rx_addr terrain, rx_addr point, uint32_t layer, const rx_regs* caller, rx_regs* out) {
    const rx_addr sp0 = rx_sp();
    const rx_addr materials = U32(terrain, 0x44ac);
    SU32(materials, 0x4c, 1);
    const float size_x = F32(terrain, 0x44d0), size_z = F32(terrain, 0x44d4);
    const float v = phys_div(phys_add(phys_mul(size_z, 0.5f), phys_sub(-0.0f, F32(point, 8))), size_z);
    const float u = phys_div(phys_add(phys_mul(size_x, 0.5f), F32(point, 0)), size_x);
    const rx_addr grid = U32(materials, 0x44);
    rx_regs in = caller ? *caller : phys_regs(), res;            /* EBX / EBP / ESI as the caller had them (the callees save them) */
    in.r[0] = layer; in.r[1] = grid; in.r[2] = point; in.r[7] = terrain;
    in.xmm[0] = v4(u, 0.0f, 0.0f, 0.0f);
    /* the original's frame words below the return address (v, u, the pushed ESI): later code reads this dead stack */
    SF32(sp0 - 4, 0, v); SF32(sp0 - 8, 0, u); SU32(sp0 - 12, 0, caller ? caller->r[6] : 0);
    rx_set_sp(sp0 - 0xc);
    if (U32(grid, 0x10) != 0) {
        in.r[6] = grid;
        const rx_addr e = sp0 - 0xc - 16;
        SU32(e, 0, 0x007c51a9); SF32(e, 4, u); SF32(e, 8, v); SU32(e, 12, layer);
        const uint32_t saved[3] = {in.r[3], in.r[5], in.r[7]};
        rx_v4 xmm0;
        rx_set_sp(e); res = in;
        res.r[0] = rx_world_packed_grid_interleaved(grid, u, v, layer, saved, &xmm0);
        res.r[1] = 32 - U32(grid, 0x68 + 4 * layer);
        res.r[2] = 0xffffffffu >> (res.r[1] & 31);
        res.xmm[0] = xmm0;
    } else {
        const rx_addr e = sp0 - 0xc - 12;                      /* the call's frame: return address, u, v */
        const uint32_t saved[4] = {in.r[3], in.r[5], in.r[6], in.r[7]};
        uint32_t edx, xmm0;
        SU32(e, 0, 0x007c51ce); SF32(e, 4, u); SF32(e, 8, v);
        rx_set_sp(e);
        res = in;
        res.r[0] = rx_world_packed_grid_sample_regs(grid, layer, u, v, saved, &edx, &xmm0);   /* 007f8e00: ECX = grid, EAX = layer */
        res.r[2] = edx;
        res.xmm[0] = v4(rx_bf(xmm0), 0.0f, 0.0f, 0.0f);          /* floor leaves e & 0x7ff in lane 0 */
    }
    rx_set_sp(sp0);
    SU32(U32(terrain, 0x44ac), 0x4c, 0);
    if (out) *out = res;
    return res.r[0];
}
RX_FUNCTION(0x007c5120, phys_terrain_material_at)

/* ---- 007c5950 TerrainBaseHeightAt (thiscall: ECX = terrain; stack point* in / out; RET 4): the base grid's height at the point's
 * (x, z) (bilinear, TerrainSampleCell) times 2048 into point.y; returns 1, or 0 without a base grid or outside the grid.
 * Frame: push ebp; and esp, -16; sub esp, 0x94; push ebx / esi / edi: +0x0c base grid, +0x10 pos, +0x20 dims / sample,
 * +0x30 signs / scale, +0x40 cells, +0x50 cell work. *edx = the EDX the original leaves (unchanged without a base grid). */
uint32_t phys_terrain_base_height_at(rx_addr terrain, rx_addr point, uint32_t* edx) {
    if (U32(terrain, 0x1c) == 0) return 0;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x94 - 12;
    rx_set_sp(L);
    const rx_addr pos = L + 0x10, dims = L + 0x20, scale = L + 0x30, cells = L + 0x40, work = L + 0x50;
    query_position(point, scale, pos);
    SU32(L, 0xc, U32(terrain, 0x1c));
    uint32_t result = 0;
    if (query_cell(terrain, pos, dims, cells, scale, work, edx)) {
        *edx = phys_terrain_sample_cell(U32(L, 0xc), work, scale, dims);
        store_height(point, v4_mul(v4_splat(2048.0f /* 0x92e22c */), v4_ld(dims)));
        result = 1;
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x007c5950, phys_terrain_base_height_at)

/* ---- 007c5aa0 TerrainHeightAt (stdcall: terrain*, point* in / out; RET 8): the deformed height at the point's (x, z):
 * base * 2048 + (deform - 0.5) * 2 * gain (the deformation surface locked for the sample) into point.y; returns 1, or 0 without
 * a base grid, when the surface does not lock (no bits) or outside the grid. Frame: push ebp; and esp, -16; sub esp, 0xb4;
 * push ebx / esi / edi: +0x10 base grid, +0x14 point*, +0x18 lock rect (pitch, bits), +0x20 pos / base sample, +0x30 scale /
 * deformation sample, +0x40 signs / scale copy, +0x50 cells, +0x60 dims, +0x70 cell work. *edx = the EDX left (the surface
 * calls' or TerrainCellSetup's; unchanged without a base grid). */
uint32_t phys_terrain_height_at(rx_addr terrain, rx_addr point, uint32_t* edx) {
    if (U32(terrain, 0x1c) == 0) return 0;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xb4 - 12;
    rx_set_sp(L);
    const rx_addr rect = L + 0x18, pos = L + 0x20, scale = L + 0x30, scale2 = L + 0x40, cells = L + 0x50, dims = L + 0x60;
    const rx_addr work = L + 0x70, tex = U32(terrain, 0x240);
    SU32(L, 0x14, point);
    query_position(point, scale2, pos);
    SU32(L, 0x10, U32(terrain, 0x1c));
    uint32_t result = 0;
    *edx = surface_lock(tex, rect);
    if (U32(rect, 4) != 0) {
        v4_st(scale2, v4_splat((float)(int32_t)U32(terrain, 0x44bc)));
        if (!query_cell(terrain, pos, dims, cells, scale, work, edx)) {
            *edx = surface_unlock(tex, 0, *edx);
        } else {
            phys_terrain_sample_cell(U32(L, 0x10), work, scale, pos);
            v4_st(pos, v4_mul(v4_splat(2048.0f /* 0x92e22c */), v4_ld(pos)));
            phys_terrain_sample_cell(U32(rect, 4), work, scale2, scale);
            rx_v4 h = v4_sub(v4_ld(scale), v4_splat(0.5f /* 0x9322cc */));
            h = v4_mul(h, v4_splat(2.0f /* 0x92e330 */));
            h = v4_mul(h, v4_splat(F32(terrain, 0xbc)));
            store_height(point, v4_add(h, v4_ld(pos)));
            *edx = surface_unlock(tex, 1, 0);
            result = 1;
        }
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x007c5aa0, phys_terrain_height_at)

/* ---- 007c5c90 TerrainBaseHeightNormal (thiscall: ECX = normal out*; stack terrain*, point* in / out; RET 8): *normal = the up
 * vector (0xd616a0) first; with a base grid and the point inside it: point.y = 2048 * the base height (TerrainSampleWindow:
 * centre and the left / right / up / down neighbours), normal = normalize(n0 - n1, normal_y / 2048, n3 - n2) (normal_y =
 * terrain+0x44ec). Returns 1 / 0. Frame: push ebp; and esp, -16; sub esp, 0xa4; push ebx / esi / edi: +0x18 point*, +0x1c
 * normal*, +0x20 pos / neighbours, +0x30 signs / scale / normal-y splat, +0x40 dims / centre, +0x50 cells, +0x60 cell work. */
uint32_t phys_terrain_base_height_normal(rx_addr normal, rx_addr terrain, rx_addr point) {
    v4_st(normal, v4_ld(0x00d616a0));
    if (U32(terrain, 0x1c) == 0) return 0;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xa4 - 12;
    rx_set_sp(L);
    const rx_addr pos = L + 0x20, scale = L + 0x30, dims = L + 0x40, cells = L + 0x50, work = L + 0x60;
    SU32(L, 0x18, point); SU32(L, 0x1c, normal);
    query_position(point, scale, pos);
    uint32_t edx, result = 0;
    if (query_cell(terrain, pos, dims, cells, scale, work, &edx)) {
        phys_terrain_cell_offsets(work);
        phys_terrain_sample_window(work, dims, pos, U32(terrain, 0x1c), scale);
        store_height(point, v4_mul(v4_splat(2048.0f /* 0x92e22c */), v4_ld(dims)));
        const float ny = phys_mul(F32(terrain, 0x44ec), 0.00048828125f /* 0x977a60 */);
        v4_st(scale, v4_splat(ny));                          /* Vec4_Splat 0041c6c0 */
        const float n0 = F32(pos, 0), n1 = F32(pos, 4), n2 = F32(pos, 8), n3 = F32(pos, 0xc);
        const rx_v4 n = v4(phys_sub(n0, n1), ny, phys_sub(n3, n2), phys_sub(n3, n2));
        const float len = phys_sqrt(phys_hsum3(v4_mul(n, n)));
        v4_st(normal, v4_div(n, v4_splat(len)));
        result = 1;
    }
    rx_set_sp(sp0);
    return result;
}
RX_FUNCTION(0x007c5c90, phys_terrain_base_height_normal)

/* ---- 007c7350 TerrainSegment_CoarseReject (ECX = segment box: min x, y, z at +0 / +4 / +8, max at +0x10 / +0x14 / +0x18; EDX =
 * the coarse grid: cells +0xd4 (min, max height pairs), row stride +0xd8): the box centre's coarse cell (x and z halved, / 16,
 * truncated); returns 1 when the segment's height range [min y, max y] can reach the cell's [lo, hi], else 0 (the exe's test:
 * reject when hi is below both y values or lo above both). */
uint32_t phys_terrain_segment_coarse_reject(rx_addr box, rx_addr coarse) {
    const int32_t row = rx_cvtt(phys_mul(phys_mul(phys_add(F32(box, 0x18), F32(box, 8)), 0.5f), 0.0625f /* 0x97761c */));
    const int32_t col = rx_cvtt(phys_mul(phys_mul(phys_add(F32(box, 0x10), F32(box, 0)), 0.5f), 0.0625f));
    const rx_addr cell = U32(coarse, 0xd4) + 8u * (uint32_t)(row * (int32_t)U32(coarse, 0xd8) + col);
    const float y0 = F32(box, 4), y1 = F32(box, 0x14), lo = F32(cell, 0), hi = F32(cell, 4);
    if (!(hi >= y0) && !(hi >= y1)) return 0;
    return (y0 >= lo || y1 >= lo) ? 1 : 0;
}
RX_FUNCTION(0x007c7350, phys_terrain_segment_coarse_reject)

/* ---- 007c34b0 TerrainTracks_AllocSlotDefault: the first free strip (+0x9c == 0) of the global track set [0x00a9577c]
 * (count +0xc, strips +0x24, 0xc0 bytes each): marks it used (+0x9c = 1, +0x94 = +0xb4 = 0), counts it (+0x28) and returns its
 * index, or -1 when all are in use. */
int32_t phys_tracks_alloc_slot(void) {
    const rx_addr tracks = U32(0, 0x00a9577c);
    for (int32_t i = 0; i < (int32_t)U32(tracks, 0xc); i++) {
        const rx_addr strip = U32(tracks, 0x24) + 0xc0u * (uint32_t)i;
        if (U32(strip, 0x9c) != 0) continue;
        SU32(strip, 0x94, 0);
        SU32(U32(tracks, 0x24) + 0xc0u * (uint32_t)i, 0x9c, 1);
        SU32(U32(tracks, 0x24) + 0xc0u * (uint32_t)i, 0xb4, 0);
        SU32(tracks, 0x28, U32(tracks, 0x28) + 1);
        return i;
    }
    return -1;
}
RX_FUNCTION(0x007c34b0, phys_tracks_alloc_slot)

/* ---- 007c4560 TerrainTracks_CutStrip (ESI = tracks, EAX = strip index): ends strip `index` (when valid and not yet cut): marks
 * it cut (+0xa0 = +0xa4 = 1); a strip with points (+8) gets 4 more (+2 when its pending length +0x14 is positive); at 125 or more
 * it rolls over: the ring position +4 advances modulo the ring size (tracks+4), the count +8 and the length +0x10 restart, and
 * the new ring entry = the old one's + 1 (ring +0xc). The pending length +0x14 is cleared. */
void phys_tracks_cut_strip(rx_addr tracks, int32_t index) {
    if (index < 0) return;
    const rx_addr strip = U32(tracks, 0x24) + 0xc0u * (uint32_t)index;
    if (U32(strip, 0xa0) != 0) return;
    SU32(strip, 0xa0, 1);
    SU32(strip, 0xa4, 1);
    if (U32(strip, 8) == 0) return;
    SU32(strip, 8, U32(strip, 8) + 4);
    if (F32(strip, 0x14) > 0.0f) SU32(strip, 8, U32(strip, 8) + 2);
    if ((int32_t)U32(strip, 8) >= 0x7d) {
        const uint32_t old = U32(strip, 4);
        SU32(strip, 4, old + 1);
        SU32(strip, 4, (uint32_t)((int32_t)(old + 1) % (int32_t)U32(tracks, 4)));
        SU32(strip, 8, 0);
        SF32(strip, 0x10, 0.0f);
        const rx_addr ring = U32(strip, 0xc);
        SU32(ring, 4u * U32(strip, 4), U32(ring, 4u * old) + 1);
    }
    SF32(strip, 0x14, 0.0f);
}
RX_FUNCTION(0x007c4560, phys_tracks_cut_strip)

/* ---- 007c66f0 TerrainGridVertices (ECX = terrain, EAX = centre*; stack count out*, half extent*; RET 8): the height-field
 * vertices of the cells under the rectangle centre +- extent (x, z) into the static vertex buffer 0x00dad500 (16 bytes each:
 * x, y, z), column by column; *count = their number (0 without a base grid, when the surface does not lock or above 1000).
 * Cell range: x from trunc((size_x / 2 + (c.x - e.x)) / size_x * (cells_x - 1)) to trunc(... (e.x + c.x) ... + 1), z likewise
 * from the negated z, each clamped to [0, trunc(cells - 1)]. Vertex (x, z): (x * spacing - size_x / 2, (deform - 0.5) * gain * 2
 * + base * 2048, -0 - (z * spacing - size_z / 2)). Frame: push ebp; and esp, -16; sub esp, 0x44; push ebx / esi / edi
 * (lock rect at +0x40). */
void phys_terrain_grid_vertices(rx_addr terrain, rx_addr centre, rx_addr count_p, rx_addr extent) {
    SU32(count_p, 0, 0);
    const rx_addr base = U32(terrain, 0x1c);
    if (base == 0) return;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x44 - 12;
    rx_set_sp(L);
    const rx_addr tex = U32(terrain, 0x240), rect = L + 0x40;
    const int32_t stride = (int32_t)U32(terrain, 0x44bc);
    surface_lock(tex, rect);
    const rx_addr bits = U32(rect, 4);
    if (bits != 0) {
        const float cz1 = phys_sub(F32(terrain, 0x44cc), 1.0f), cx1 = phys_sub(F32(terrain, 0x44c8), 1.0f);
        const float cx = F32(centre, 0), cz = F32(centre, 8), ex = F32(extent, 0), ez = F32(extent, 8);
        const float size_x = F32(terrain, 0x44d0), size_z = F32(terrain, 0x44d4);
        const float inv_x = phys_div(1.0f, size_x), half_x = phys_mul(size_x, 0.5f);
        const float inv_z = phys_div(1.0f, size_z), half_z = phys_mul(size_z, 0.5f);
        const float near_z = phys_sub(-0.0f, phys_sub(cz, ez)), far_z = phys_sub(-0.0f, phys_add(ez, cz));
        int32_t x0 = rx_cvtt(phys_mul(phys_mul(phys_add(half_x, phys_sub(cx, ex)), inv_x), cx1));
        int32_t z1 = rx_cvtt(phys_add(phys_mul(phys_mul(phys_add(half_z, near_z), inv_z), cz1), 1.0f));
        int32_t x1 = rx_cvtt(phys_add(phys_mul(phys_mul(phys_add(half_x, phys_add(ex, cx)), inv_x), cx1), 1.0f));
        int32_t z0 = rx_cvtt(phys_mul(phys_mul(phys_add(half_z, far_z), inv_z), cz1));
        const int32_t max_x = rx_cvtt(cx1), max_z = rx_cvtt(cz1);
        x0 = x0 < 0 ? 0 : x0 > max_x ? max_x : x0;
        x1 = x1 < 0 ? 0 : x1 > max_x ? max_x : x1;
        z0 = z0 < 0 ? 0 : z0 > max_z ? max_z : z0;
        z1 = z1 < 0 ? 0 : z1 > max_z ? max_z : z1;
        const int32_t count = (x1 - x0 + 1) * (z1 - z0 + 1);
        if (count <= 1000) {
            SU32(count_p, 0, (uint32_t)count);
            rx_addr v = 0x00dad500;
            for (int32_t x = x0; x <= x1; x++) {
                for (int32_t z = z0; z <= z1; z++, v += 0x10) {
                    const uint32_t cell = 4u * (uint32_t)(z * stride + x);
                    const float spacing = F32(terrain, 0x44e8);
                    const float deform = phys_mul(phys_mul(phys_sub(F32(bits, cell), 0.5f), F32(terrain, 0xbc)), 2.0f);
                    SF32(v, 0, phys_sub(phys_mul((float)x, spacing), half_x));
                    SF32(v, 4, phys_add(deform, phys_mul(F32(base, cell), 2048.0f)));
                    SF32(v, 8, phys_sub(-0.0f, phys_sub(phys_mul((float)z, F32(terrain, 0x44e8)), half_z)));
                }
            }
        }
        surface_unlock(tex, 1, 0);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x007c66f0, phys_terrain_grid_vertices)

/* ---- 007c2ab0 DeformablePatch_AddStamp (ESI = patch set, EAX = p0*; stack p1*, depth0, depth1, width0, width1; RET 0x14;
 * _re/notes/terrain-deformation.md): queues a stamp quad between p0 and p1 into the current patch (set +0x18; at most 64:
 * patch +0x230 count): the corners (+-width0, 0, |p1 - p0| / 2) and (+-width1, 0, -max(|p1 - p0| / 2, spacing)) (widths at least
 * the grid spacing), turned from the up axis to the segment's horizontal direction (Vec3_RotateAboutAxis about up x dir by
 * acos(-dir.z) when that is above 0.001) and scaled by 2 / size_x, appended with the midpoint (x, z) to the patch's pair list
 * (+0x30) and as 30 floats (two triangles: six vertices of (x, -z, depth, u, v)) to its vertex list (+0x448,
 * 0x1e04 per patch); the patch's index count (+0x440) grows by 2. Returns the count's address (or the patch offset when full);
 * *edx = the EDX left. Frame: push ebp; and esp, -16; sub esp, 0x78; push ebx / edi (axis +0x10, direction +0x20, midpoint
 * +0x30, corners +0x40 / +0x50 / +0x60 / +0x70). */
static void cross_ba(rx_addr a, rx_addr b, rx_addr out) {   /* 00401630 Vec3_Cross (EAX = a, ECX = b, stack out): b x a */
    const float ax = F32(a, 0), ay = F32(a, 4), az = F32(a, 8), bx = F32(b, 0), by = F32(b, 4), bz = F32(b, 8);
    SF32(out, 0, phys_sub(phys_mul(az, by), phys_mul(ay, bz)));
    SF32(out, 4, phys_sub(phys_mul(ax, bz), phys_mul(bx, az)));
    SF32(out, 8, phys_sub(phys_mul(bx, ay), phys_mul(ax, by)));
}
rx_addr phys_patch_add_stamp(rx_addr set, rx_addr p0, rx_addr p1, float depth0, float depth1, float width0, float width1,
                             uint32_t* edx) {
    const uint32_t patch_off = U32(set, 0x18) * 0x208u;
    if (U32(set + patch_off, 0x230) >= 0x40) return patch_off;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x78 - 8;
    rx_set_sp(L);
    const rx_addr axis = L + 0x10, dir = L + 0x20, mid = L + 0x30;
    const rx_addr c0 = L + 0x40, c1 = L + 0x50, c2 = L + 0x60, c3 = L + 0x70;
    /* the horizontal direction */
    CALL_AT_SP(L, 1, phys_vec3_sub(p0, p1, mid));
    {
        const float dx = F32(mid, 0), dz = F32(mid, 8);
        const float len2 = phys_add(phys_add(phys_mul(dx, dx), phys_mul(0.0f, 0.0f)), phys_mul(dz, dz));
        float ux = dx, uy = 0.0f, uz = dz;
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, phys_sqrt(len2));
            ux = phys_mul(dx, inv); uy = phys_mul(inv, 0.0f); uz = phys_mul(inv, dz);
        }
        SF32(dir, 0, ux); SF32(dir, 4, uy); SF32(dir, 8, uz);
    }
    SF32(mid, 0, phys_mul(phys_add(F32(p0, 0), F32(p1, 0)), 0.5f));
    SF32(mid, 8, phys_mul(phys_add(F32(p1, 8), F32(p0, 8)), 0.5f));
    CALL_AT_SP(L, 1, phys_vec3_sub(p0, p1, axis));
    const float ex = F32(axis, 0), ey = F32(axis, 4), ez = F32(axis, 8);
    const float half = phys_mul(phys_sqrt(phys_add(phys_add(phys_mul(ex, ex), phys_mul(ey, ey)), phys_mul(ez, ez))), 0.5f);
    const rx_addr terrain = U32(set, 0x4268);
    const float spacing = F32(terrain, 0x44e8);
    const float back = half > spacing ? half : spacing;
    const float w0 = width0 > spacing ? width0 : spacing, w1 = width1 > spacing ? width1 : spacing;
    SF32(c0, 0, w0); SF32(c0, 4, 0.0f); SF32(c0, 8, half);
    SF32(c1, 0, phys_sub(-0.0f, w0)); SF32(c1, 4, 0.0f); SF32(c1, 8, half);
    SF32(c2, 0, w1); SF32(c2, 4, 0.0f); SF32(c2, 8, phys_sub(-0.0f, back));
    SF32(c3, 0, phys_sub(-0.0f, w1)); SF32(c3, 4, 0.0f); SF32(c3, 8, phys_sub(-0.0f, back));
    /* turned from +z to the direction */
    const float angle = phys_acos_poly(phys_sub(-0.0f, F32(dir, 8)));
    if (rx_bf(rx_fb(angle) & 0x7fffffffu) > 0.001f /* 0x97743c */) {
        CALL_AT_SP(L, 1, cross_ba(dir, 0x009cb4c0u, axis));
        const float x = F32(axis, 0), y = F32(axis, 4), z = F32(axis, 8);
        const float len2 = phys_add(phys_add(phys_mul(y, y), phys_mul(x, x)), phys_mul(z, z));
        if (len2 > 0.0f) {
            const float inv = phys_div(1.0f, phys_sqrt(len2));
            SF32(axis, 0, phys_mul(x, inv)); SF32(axis, 4, phys_mul(y, inv)); SF32(axis, 8, phys_mul(z, inv));
        }
        const rx_addr corners[4] = {c0, c1, c2, c3};
        for (int i = 0; i < 4; i++) CALL_AT_SP(L, 1, phys_vec3_rotate_about_axis(corners[i], corners[i], axis, angle));
    }
    /* to patch units: / (size_x / 2) */
    const float s = phys_mul(F32(terrain, 0x44d0), 0.5f);
    const rx_addr corners[4] = {c0, c1, c2, c3};
    for (int i = 0; i < 4; i++) {
        SF32(corners[i], 0, phys_div(F32(corners[i], 0), s));
        SF32(corners[i], 8, phys_div(F32(corners[i], 8), s));
    }
    const uint32_t slot = U32(set, 0x18);
    phys_float_pair_list_push(set + slot * 0x208u + 0x30, U32(mid, 0), U32(mid, 8));
    const float neg_c0z = phys_sub(-0.0f, F32(c0, 8)), neg_c3z = phys_sub(-0.0f, F32(c3, 8));
    const float values[30] = {
        F32(c1, 0), phys_sub(-0.0f, F32(c1, 8)), depth0, 0.0f, 0.0f,
        F32(c3, 0), neg_c3z, depth1, 0.0f, 1.0f,
        F32(c0, 0), neg_c0z, depth0, 1.0f, 0.0f,
        F32(c0, 0), neg_c0z, depth0, 1.0f, 0.0f,
        F32(c3, 0), neg_c3z, depth1, 0.0f, 1.0f,
        F32(c2, 0), phys_sub(-0.0f, F32(c2, 8)), depth1, 1.0f, 1.0f,
    };
    for (int i = 0; i < 30; i++) phys_float_list_push(set + U32(set, 0x18) * 0x1e04u + 0x448, rx_fb(values[i]));
    *edx = U32(set, 0x18) * 0x1e04u;
    const rx_addr count = set + 4u * U32(set, 0x18) + 0x440;
    SU32(count, 0, U32(count, 0) + 2);
    rx_set_sp(sp0);
    return count;
}
RX_FUNCTION(0x007c2ab0, phys_patch_add_stamp)

/* ---- 007c3510 TerrainTracks_AddPoint (EAX = tracks; stdcall strip index, pos*, prev*, width, offset, intensity, flag; RET 0x1c;
 * native/game notes rt-bump): extends the ribbon of a track strip (tracks +0x24 strips of 0xc0 bytes, ring size +4) by one cross
 * section at pos (the ribbon geometry of the tire tracks). Strip: +4 ring position, +8 index count, +0xc ring, +0x10 / +0x14 lengths, +0x20 direction, +0x30 last point,
 * +0x40 intensity, +0x44.. / +0x54.. / +0x6c.. / +0x7c.. int16 cross sections (x, z, intensity, v of both edges, in 1/32767 of
 * the half world size), +0x64 / +0x8c ring entries, +0x68 / +0x90 counts, +0xa0 cut, +0xa4 new, +0xa8 rising, +0xac flag, +0xb0
 * v ramp. A step against the strip's direction or longer than 25 cuts the strip
 * (TerrainTracks_CutStrip); one shorter than 0.15 on a running strip is ignored. The edges are pos +- the horizontal side
 * vector (up x direction) * width; on a running strip each edge is first moved back along the strip direction to the previous
 * section's (the projection fraction of the new edge's motion onto the direction, clamped to [0, 1]; a backward motion marks a
 * change). The intensity is rounded to 32767ths (negated with the flag); the v ramp runs 0 -> 1 -> 0 at length / 128 per unit
 * (offset / 128 at the ends). Frame: push ebp; and esp, -16; sub esp, 0xe4; push ebx / esi / edi (direction +0x30, side /
 * scaled side +0x40, first edge's unit motion +0xa0 (stale when the motion is zero)). Returns the ECX the original leaves. */
static float x87_copy(float f) { return f != f ? rx_bf(rx_fb(f) | 0x400000u) : f; }   /* fld / fstp m32 */
static int16_t to_i16(float f) { return (int16_t)rx_cvtt(f); }
/* the edge coordinate on a running strip: the previous section's value moved toward the new one by the fraction t */
static float edge_lerp(float prev, float next, float t) {
    if (!(0.0f < t || t != t)) return prev;                   /* t <= 0 */
    if (t >= 1.0f) return next;
    return phys_add(phys_mul(phys_sub(next, prev), t), prev);
}
/* the projection of an edge's unit motion (x, y, z) onto the strip direction (+0x20) */
static float edge_fraction(rx_addr strip, float x, float y, float z) {
    return phys_add(phys_add(phys_mul(F32(strip, 0x24), y), phys_mul(x, F32(strip, 0x20))), phys_mul(F32(strip, 0x28), z));
}
uint32_t phys_tracks_add_point(rx_addr tracks, int32_t index, rx_addr pos, rx_addr prev, float width, float offset, float intensity,
                               uint32_t flag, uint32_t ecx_in) {
    if (index < 0 || U32(tracks, 0x2c) != 0) return ecx_in;
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xe4 - 12;
    rx_set_sp(L);
    uint32_t ecx = 0;
    if ((U8(0, 0x009e30e0) & 1) == 0) { SU32(0, 0x009e30e0, U32(0, 0x009e30e0) | 1); SU32(0, 0x009e30dc, 0); }
    rx_addr terrain;
    CALL_AT_SP(L, 0, terrain = rx_world_find_by_controller(0x009e30d4));   /* 00402dc0: EDX = the registry */
    const uint32_t off = 0xc0u * (uint32_t)index;
#define STRIP (U32(tracks, 0x24) + off)
    if (U32(STRIP, 0xa4) != 0) {
        for (int i = 0; i < 3; i++) SF32(STRIP, 0x30 + 4 * i, x87_copy(F32(prev, 4 * i)));
        SF32(STRIP, 0x40, intensity);
        SU32(STRIP, 0xac, flag);
    }
    const rx_addr dir = L + 0x30, side = L + 0x40, a_pt = L + 0x40, b_pt = L + 0x60, out = L + 0x90, unit_a = L + 0xa0;
    /* the horizontal step from the last point */
    SF32(a_pt, 0, F32(STRIP, 0x30)); SF32(a_pt, 4, 0.0f); SF32(a_pt, 8, F32(STRIP, 0x38));
    SF32(b_pt, 0, F32(pos, 0)); SF32(b_pt, 4, 0.0f); SF32(b_pt, 8, F32(pos, 8));
    CALL_AT_SP(L, 1, phys_vec3_sub(a_pt, b_pt, out));
    float dx = F32(out, 0), dy = F32(out, 4), dz = F32(out, 8);
    const float len2 = phys_add(phys_add(phys_mul(dy, dy), phys_mul(dx, dx)), phys_mul(dz, dz));
    const float len = phys_sqrt(len2);
    const rx_addr s0 = STRIP;
    const float along = phys_add(phys_add(phys_mul(F32(s0, 0x24), dy), phys_mul(F32(s0, 0x20), dx)), phys_mul(F32(s0, 0x28), dz));
    if (U32(s0, 0xa4) == 0 && 0.15f /* 0x9322c8 */ > len && !(0.0f > along)) { ecx = a_pt; goto out; }
    if (0.0f > along || len > 25.0f /* 0x92e134 */) {
        SF32(s0, 0x20, dx); SF32(s0, 0x24, dy); SF32(s0, 0x28, dz);
        phys_tracks_cut_strip(tracks, index);
        ecx = off;
        goto out;
    }
    SF32(STRIP, 0x30, F32(pos, 0));
    SF32(STRIP, 0x34, x87_copy(F32(pos, 4)));
    SF32(STRIP, 0x38, x87_copy(F32(pos, 8)));
    SU32(STRIP, 0xa0, 0);
    if (len2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(len2));
        dx = phys_mul(inv, dx); dy = phys_mul(dy, inv); dz = phys_mul(inv, dz);
    }
    SF32(dir, 0, dx); SF32(dir, 4, dy); SF32(dir, 8, dz);
    CALL_AT_SP(L, 1, cross_ba(dir, 0x009cb4b0u, side));
    if (U32(STRIP, 0xa4) != 0) { SF32(STRIP, 0x20, dx); SF32(STRIP, 0x24, dy); SF32(STRIP, 0x28, dz); }
    SF32(STRIP, 0x14, phys_add(F32(STRIP, 0x14), len));
    SF32(STRIP, 0x10, phys_add(F32(STRIP, 0x10), len));
    /* the intensity in 32767ths (_copysign(0.5, x) + x on the x87, _ftol2_sse) */
    const float inten = phys_mul(intensity, 32767.0f /* 0x935cd0 */);
    const rx_f80 r = rx_f80_add(rx_f80_from_f64(crt_copysign(0.5, (double)inten)), rx_f80_from_f32(inten), 53, 0);
    uint32_t n16 = (uint32_t)(uint16_t)phys_ftol2_sse(r, 53);
    if (flag != 0) n16 = (uint16_t)(0u - n16);
    /* the v ramp */
    float ramp = 0.0f;
    int changed = 0;
    if (U32(STRIP, 0xa4) != 0) { SF32(STRIP, 0xb0, 0.0f); SU32(STRIP, 0xa8, 1); }
    if (U32(STRIP, 0xa8) != 0) {
        const float w = 1.0f > width ? 1.0f : width;
        SF32(STRIP, 0xb0, phys_add(phys_div(phys_mul(len, 0.0078125f), w), F32(STRIP, 0xb0)));
        if (F32(STRIP, 0xb0) > 1.0f) {
            SF32(STRIP, 0xb0, phys_sub(2.0f, F32(STRIP, 0xb0)));
            SU32(STRIP, 0xa8, 0);
            changed = 1;
        }
        ramp = phys_mul(offset, 0.0078125f /* 0x9777dc */);
    }
    if (U32(STRIP, 0xa8) == 0) {
        SF32(STRIP, 0xb0, phys_sub(F32(STRIP, 0xb0), phys_mul(len, 0.0078125f)));
        if (0.0f > F32(STRIP, 0xb0)) {
            SF32(STRIP, 0xb0, phys_sub(-0.0f, F32(STRIP, 0xb0)));
            SU32(STRIP, 0xa8, 1);
            changed = 1;
        }
        ramp = phys_mul(offset, -0.0078125f /* 0x97791c */);
    }
    if (U32(STRIP, 0xac) != flag) changed = 1;
    if (rx_bf(rx_fb(phys_sub(F32(STRIP, 0x40), intensity)) & 0x7fffffffu) > 0.333f /* 0x977478 */) changed = 1;
    SF32(STRIP, 0x40, intensity);
    const uint32_t v16 = (uint16_t)to_i16(phys_mul(phys_add(F32(STRIP, 0xb0), ramp), 32767.0f));
    SU32(STRIP, 0x68, U32(STRIP, 8));
    if (U32(STRIP, 8) == 0 && U32(STRIP, 0xa4) == 0) {
        for (int i = 0; i < 4; i++) SU32(STRIP, 0x44 + 4 * i, U32(STRIP, 0x54 + 4 * i));
        SU32(STRIP, 0x64, U32(STRIP, 4));
        SU32(STRIP, 8, U32(STRIP, 8) + 2);
    }
    /* the two edges in grid units */
    const float sx = phys_mul(F32(side, 0), width), sy = phys_mul(F32(side, 4), width), sz = phys_mul(F32(side, 8), width);
    const float half = phys_mul(F32(terrain, 0x44d0), 0.5f);
    const float e1x = phys_mul(phys_div(phys_add(sx, F32(b_pt, 0)), half), 32767.0f);
    const float e1y = phys_mul(phys_div(phys_add(sy, 0.0f), half), 32767.0f);
    const float e1z = phys_mul(phys_div(phys_add(sz, F32(b_pt, 8)), half), 32767.0f);
    SF32(L, 0x78, sx); SF32(L, 0x18, sz);
    SF32(L, 0xc0, e1x); SF32(L, 0xc4, e1y); SF32(L, 0xc8, e1z);
    SF32(side, 0, sx); SF32(side, 4, sy); SF32(side, 8, sz);
    CALL_AT_SP(L, 1, phys_vec3_sub(side, b_pt, out));
    const float e2x = phys_mul(phys_div(F32(out, 0), half), 32767.0f);
    SF32(L, 0x94, phys_mul(phys_div(F32(out, 4), half), 32767.0f));
    const float e2z = phys_mul(phys_div(F32(out, 8), half), 32767.0f);
    SF32(L, 0x40, e2x); SF32(L, 0x48, e2z); SF32(L, 0x90, e2x); SF32(L, 0x98, e2z);
    const float b0 = F32(STRIP, 0xb0), b0v = phys_mul(b0, 32767.0f);
    const float v_edge = b0 == 0.0f ? F32(0, 0x009cd9e8) : phys_sub(-0.0f, b0v);
    const float i_f = (float)(int16_t)n16;
    float xa = e1x, za = e1z, ia = i_f, va = v_edge, xb = e2x, zb = e2z, ib = i_f, vb = b0v;
    const rx_addr strip = STRIP;
    if (U32(strip, 0xa4) == 0) {
        /* move each new edge back along the strip to the previous section */
        const float p54 = (float)(int16_t)U16(strip, 0x54), p56 = (float)(int16_t)U16(strip, 0x56);
        const float p5c = (float)(int16_t)U16(strip, 0x5c), p5e = (float)(int16_t)U16(strip, 0x5e);
        const float p5a = (float)(int16_t)U16(strip, 0x5a), p62 = (float)(int16_t)U16(strip, 0x62);
        const float p58 = (float)(int16_t)U16(strip, 0x58), p60 = (float)(int16_t)U16(strip, 0x60);
        const rx_addr pa = L + 0xb0, pb = L + 0x60;
        SF32(pa, 0, p54); SF32(pa, 4, 0.0f); SF32(pa, 8, p56);
        SF32(pb, 0, p5c); SF32(pb, 4, 0.0f); SF32(pb, 8, p5e);
        CALL_AT_SP(L, 1, phys_vec3_sub(pa, L + 0xc0, L + 0xe0));
        float mx = F32(L, 0xe0), my = F32(L, 0xe4), mz = F32(L, 0xe8);
        float m2 = phys_add(phys_add(phys_mul(mx, mx), phys_mul(my, my)), phys_mul(mz, mz));
        if (m2 > 0.0f) {
            const float inv = phys_div(1.0f, phys_sqrt(m2));
            SF32(unit_a, 0, phys_mul(inv, mx)); SF32(unit_a, 4, phys_mul(inv, my)); SF32(unit_a, 8, phys_mul(inv, mz));
        }
        float t = edge_fraction(strip, F32(unit_a, 0), F32(unit_a, 4), F32(unit_a, 8));
        if (0.0f > t) { t = 0.0f; changed = 1; }
        xa = edge_lerp(p54, e1x, t); za = edge_lerp(p56, e1z, t); va = edge_lerp(p5a, v_edge, t); ia = edge_lerp(p58, i_f, t);
        CALL_AT_SP(L, 1, phys_vec3_sub(pb, L + 0x90, L + 0xe0));
        mx = F32(L, 0xe0); my = F32(L, 0xe4); mz = F32(L, 0xe8);
        m2 = phys_add(phys_add(phys_mul(mx, mx), phys_mul(my, my)), phys_mul(mz, mz));
        float ux = F32(unit_a, 0), uy = F32(unit_a, 4), uz = F32(unit_a, 8);
        if (m2 > 0.0f) {
            const float inv = phys_div(1.0f, phys_sqrt(m2));
            ux = phys_mul(inv, mx); uy = phys_mul(inv, my); uz = phys_mul(inv, mz);
        }
        float t2 = edge_fraction(strip, ux, uy, uz);
        if (0.0f > t2) t2 = 0.0f;                              /* (no change mark on this edge) */
        xb = edge_lerp(p5c, e2x, t2); zb = edge_lerp(p5e, e2z, t2); vb = edge_lerp(p62, b0v, t2); ib = edge_lerp(p60, i_f, t2);
    }
    SU16(strip, 0x54, (uint16_t)to_i16(xa)); SU16(strip, 0x56, (uint16_t)to_i16(za)); SU16(strip, 0x58, (uint16_t)to_i16(ia));
    SU16(strip, 0x5c, (uint16_t)to_i16(xb)); SU16(strip, 0x5e, (uint16_t)to_i16(zb)); SU16(strip, 0x60, (uint16_t)to_i16(ib));
    SU16(strip, 0x5a, (uint16_t)to_i16(va)); SU16(strip, 0x62, (uint16_t)to_i16(vb));
    SU32(strip, 0x64, U32(strip, 4));
    for (int i = 0; i < 4; i++) SU32(strip, 0x6c + 4 * i, U32(strip, 0x54 + 4 * i));
    /* the section ahead (0.75 of the side, offset along the direction) */
    {
        const float sx75 = phys_mul(F32(L, 0x78), 0.75f), sz75 = phys_mul(F32(L, 0x18), 0.75f);
        const float dxe = phys_mul(F32(dir, 0), offset), dze = phys_mul(F32(dir, 8), offset);
        const float hx = phys_mul(F32(terrain, 0x44d0), 0.5f), hz = phys_mul(F32(terrain, 0x44d4), 0.5f);
        SU16(strip, 0x7c, (uint16_t)to_i16(phys_div(phys_mul(phys_add(phys_add(F32(pos, 0), sx75), dxe), 32767.0f), hx)));
        SU16(strip, 0x7e, (uint16_t)to_i16(phys_div(phys_mul(phys_add(phys_add(F32(pos, 8), sz75), dze), 32767.0f),
                                                    phys_mul(F32(terrain, 0x44d4), 0.5f))));
        SU16(strip, 0x80, 0);
        SU16(strip, 0x82, (uint16_t)((v16 & 0xffff) == 0 ? -1 : -(int32_t)(int16_t)v16));
        SU16(strip, 0x84, (uint16_t)to_i16(phys_div(phys_mul(phys_add(phys_sub(F32(pos, 0), sx75), dxe), 32767.0f),
                                                    phys_mul(F32(terrain, 0x44d0), 0.5f))));
        SU16(strip, 0x86, (uint16_t)to_i16(phys_div(phys_mul(phys_add(phys_sub(F32(pos, 8), sz75), dze), 32767.0f), hz)));
        SU16(strip, 0x88, 0);
        SU16(strip, 0x8a, (uint16_t)v16);
    }
    SU32(strip, 0x8c, U32(strip, 0x64));
    SU32(strip, 0x90, U32(strip, 0x68) + 2);
    if ((int32_t)U32(strip, 0x90) >= 0x7f) {
        SU32(strip, 0x8c, (uint32_t)((int32_t)(U32(strip, 4) + 1) % I32(tracks, 4)));
        SU32(strip, 0x90, 0);
    }
    if (U32(strip, 0xa4) != 0) {
        /* a new strip: its first section around prev */
        const float hx = phys_mul(F32(terrain, 0x44d0), 0.5f), hz = phys_mul(F32(terrain, 0x44d4), 0.5f);
        const float px = F32(prev, 0), pz = F32(prev, 8), qx = F32(L, 0x78), qz = F32(L, 0x18);
        SU16(strip, 0x44, (uint16_t)to_i16(phys_div(phys_mul(phys_add(px, qx), 32767.0f), hx)));
        SU16(strip, 0x46, (uint16_t)to_i16(phys_div(phys_mul(phys_add(pz, qz), 32767.0f), hz)));
        SU16(strip, 0x48, 0);
        SU16(strip, 0x4c, (uint16_t)to_i16(phys_div(phys_mul(phys_sub(px, qx), 32767.0f), phys_mul(F32(terrain, 0x44d0), 0.5f))));
        SU16(strip, 0x4e, (uint16_t)to_i16(phys_div(phys_mul(phys_sub(pz, qz), 32767.0f), phys_mul(F32(terrain, 0x44d4), 0.5f))));
        SU16(strip, 0x50, 0); SU16(strip, 0x4a, 0xffff); SU16(strip, 0x52, 0);
        SU32(strip, 0x64, U32(strip, 4));
        SU32(strip, 8, U32(strip, 8) + 2);
        ecx = U32(strip, 0x68) + 2;
        SU32(strip, 0x90, ecx);
        goto out;
    }
    {
        /* a running strip: the section closes when something changed or it has run far enough */
        const float acc = F32(strip, 0x14);
        const rx_addr back = L + 0x90, fwd = L + 0x40;
        SF32(back, 0, phys_mul(acc, F32(strip, 0x20))); SF32(back, 4, phys_mul(F32(strip, 0x24), acc));
        SF32(back, 8, phys_mul(F32(strip, 0x28), acc));
        SF32(fwd, 0, phys_mul(acc, F32(dir, 0))); SF32(fwd, 4, phys_mul(F32(dir, 4), acc)); SF32(fwd, 8, phys_mul(acc, F32(dir, 8)));
        CALL_AT_SP(L, 1, phys_vec3_sub(back, fwd, L + 0xe0));
        const float ox = F32(L, 0xe0), oy = F32(L, 0xe4), oz = F32(L, 0xe8);
        if (!changed) {
            const float d = phys_sqrt(phys_add(phys_add(phys_mul(ox, ox), phys_mul(oy, oy)), phys_mul(oz, oz)));
            if (0.5f > d && 128.0f /* 0x971670 */ > F32(strip, 0x14)) { ecx = strip; goto out; }
        }
        SU32(strip, 0xac, flag);
        SF32(strip, 0x20, F32(dir, 0)); SF32(strip, 0x24, F32(dir, 4)); SF32(strip, 0x28, F32(dir, 8));
        SF32(strip, 0x14, 0.0f);
        SU32(strip, 8, U32(strip, 8) + 2);
        ecx = U32(tracks, 0x24);
        if ((int32_t)U32(strip, 8) >= 0x7f) {
            const uint32_t old = U32(strip, 4);
            SU32(strip, 4, old + 1);
            SU32(strip, 4, (uint32_t)((int32_t)(old + 1) % I32(tracks, 4)));
            SU32(strip, 8, 0);
            SF32(strip, 0x10, 0.0f);
            const rx_addr ring = U32(strip, 0xc);
            ecx = U32(ring, 4u * old) + 1;
            SU32(ring, 4u * U32(strip, 4), ecx);
        }
    }
#undef STRIP
out:
    rx_set_sp(sp0);
    return ecx;
}
RX_FUNCTION(0x007c3510, phys_tracks_add_point)
