/* world_node_water.c -- water surface sampling of the physics world's node share (0x007exxxx / 0x007f8xxx): the height of
 * the water patches under a point (007efef0 over the set, 007e9dc0 per patch) and the packed height-grid sampler
 * PackedGrid_SampleUV (007f8e00).
 *
 * Water patch (b): +0x14 / +0x18 cell size x / z, +0x80 surface normal (plane), +0xa0 plane point (grid origin x +0xa0,
 * z +0xa8), +0xb0 / +0xb4 / +0xb8 / +0xbc the box min x, min z, max x, max z, +0x3c the grid descriptor, +0x47c the frame tag
 * it was updated in, +0x490 / +0x494 the height scale / offset, +0x54 the patch object.
 * Grid descriptor (g): +0 cells (4 bytes each: +1 valid, +2 / +3 height hi / lo), +4 count x, +8 count z, +0xc row stride,
 * +0x10 format (0x15 = 16-bit heights).
 * The x87 parts run at the main thread's 53-bit precision (cw 0x027f). */
#include "phys_mech.h"
#include "world_util.h"
#include "world_node.h"
#include "phys_crt.h"
#include "crt_util.h"   /* CRT_WITH_REGS: floor's NaN path saves the caller's registers */

#define X87_BITS 53

/* the cell index of a coordinate as the original computes it: u = (float)(((c - origin) + size / 2) * count / size) (x87),
 * i = ftol(copysign(0.5, u) + u) (rounding half away from zero), then through a float (cvtsi2ss / cvttss2si) */
static int32_t water_cell_index(float c, float origin, float size, uint32_t count) {
    rx_f80 s = rx_f80_sub(rx_f80_from_f32(c), rx_f80_from_f32(origin), X87_BITS, 0);
    s = rx_f80_add(s, rx_f80_mul(rx_f80_from_f64(0.5) /* 0x9322cc */, rx_f80_from_f32(size), X87_BITS, 0), X87_BITS, 0);
    rx_f80 n = rx_f80_from_int((int32_t)count);
    if ((int32_t)count < 0) n = rx_f80_add(n, rx_f80_from_f32(4294967296.0f) /* 0x97731c */, X87_BITS, 0);
    s = rx_f80_mul(s, n, X87_BITS, 0);
    const float u = rx_f80_to_f32(rx_f80_div(s, rx_f80_from_f32(size), X87_BITS, 0), 0);
    const double half = crt_copysign(0.5 /* 0x9774e8 */, (double)u);   /* _copysign (IAT 0x916230) */
    const int32_t i = phys_ftol2_sse(rx_f80_add(rx_f80_from_f64(half), rx_f80_from_f32(u), X87_BITS, 0), X87_BITS);
    const float fi = (float)i;
    return fi != fi || fi >= 2147483648.0f || fi < -2147483648.0f ? (int32_t)0x80000000 : (int32_t)fi;
}

/* 007e9dc0 (stdcall patch, point in / out, grid, scale, offset; RET 0x14; frame push ebp; and esp, -16; sub 0x34; EAX):
 * outside the patch's box -> 0. The vertical segment through the point (y from 1e6 down to -1e6) meets the patch plane
 * (Segment_PlaneIntersect 008523b0 into the point): 0 when it does not; without a grid the plane point stands (EAX = the
 * intersect result). With a 16-bit grid the point moves along the normal by the cell's height ((hi * 256 + lo) / 65535 *
 * scale + offset); an invalid cell -> 0. */
uint32_t rx_world_water_patch_height(rx_addr b, rx_addr p, rx_addr g, float scale, float offset) {
    const float px = F32(p, 0);
    if (F32(b, 0xb0) > px || px > F32(b, 0xb8)) return 0;
    const float pz = F32(p, 8);
    if (F32(b, 0xb4) > pz || pz > F32(b, 0xbc)) return 0;
    world_frame f = world_frame_enter(0x34, 3, 1);
    const rx_addr s0 = f.esp + 0x30, s1 = f.esp + 0x20;
    SF32(s0, 0, px); SF32(s0, 4, 1000000.0f /* 0x977658 */); SF32(s0, 8, pz);
    SF32(s1, 0, px); SF32(s1, 4, -1000000.0f /* 0x9777d8 */); SF32(s1, 8, pz);
    uint32_t hit;
    WORLD_CALL_SP(8, hit = phys_segment_plane_intersect(s0, s1, b + 0xa0, p, b + 0x80));
    uint32_t r = hit;
    if (hit != 0 && g != 0) {
        SU32(f.esp, 0x1c, world_x87_pass(U32(b, 0x18)));
        SU32(f.esp, 0x18, world_x87_pass(U32(b, 0x14)));
        int32_t ix = water_cell_index(F32(p, 0), F32(b, 0xa0), F32(f.esp, 0x18), U32(g, 4));
        int32_t iz = water_cell_index(F32(p, 8), F32(b, 0xa8), F32(f.esp, 0x1c), U32(g, 8));
        const int32_t mx = I32(g, 4) - 1, mz = I32(g, 8) - 1;
        if (ix < 0) ix = 0; else if (ix > mx) ix = mx;
        if (iz < 0) iz = 0; else if (iz > mz) iz = mz;
        r = 1;
        if (U32(g, 0x10) == 0x15) {
            const rx_addr cell = U32(g, 0xc) * (uint32_t)iz + 4u * (uint32_t)ix + U32(g, 0);
            if (U8(cell, 1) == 0) r = 0;
            else {
                float h = phys_add((float)((uint32_t)U8(cell, 2) << 8), (float)U8(cell, 3));
                h = phys_add(phys_mul(phys_div(h, 65535.0f /* 0x97759c */), scale), offset);
                SF32(p, 0, phys_add(F32(p, 0), phys_mul(F32(b, 0x80), h)));
                SF32(p, 4, phys_add(F32(p, 4), phys_mul(F32(b, 0x84), h)));
                SF32(p, 8, phys_add(F32(p, 8), phys_mul(F32(b, 0x88), h)));
            }
        }
    }
    world_frame_leave(&f);
    return r;
}
RX_FUNCTION(0x007e9dc0, rx_world_water_patch_height)

/* 007efef0 (ESI = water set {+0x10 / +0x14 the patch pointers}; stdcall point in / out; RET 4; EAX): the highest water
 * surface over the point among the patches updated this frame (tag = the frame counter [[0xa95844] + 0x1c + 4 * [+4]]): the
 * point is kept as the query (each patch's 007e9dc0 refines the same local copy) and becomes (-FLT_MAX, ...) until a patch
 * reports a higher y; 1 when one did. Checked vector indexing (CRT handler). */
uint32_t rx_world_water_set_height(rx_addr set, rx_addr pt) {
    if (((U32(set, 0x14) - U32(set, 0x10)) & ~3u) == 0) return 0;
    world_frame f = world_frame_enter(0x18, 2, 1);
    const rx_addr q = f.esp + 0x10;
    SU32(q, 0, U32(pt, 0)); SU32(q, 4, U32(pt, 4)); SU32(q, 8, U32(pt, 8));
    SF32(pt, 0, -3.40282347e+38f); SF32(pt, 4, -3.40282347e+38f); SF32(pt, 8, -3.40282347e+38f);   /* 0x977550 */
    uint32_t found = 0;
    SU32(f.esp, 8, 0);
    for (uint32_t i = 0; i < (uint32_t)((int32_t)(U32(set, 0x14) - U32(set, 0x10)) >> 2); i++) {
#define COUNT() ((uint32_t)((int32_t)(U32(set, 0x14) - U32(set, 0x10)) >> 2))
        world_bounds_check(i, COUNT());
        if (U32(U32(set, 0x10), 4 * i) == 0) continue;
        const rx_addr gl = U32(0, 0x00a95844);
        const uint32_t tag = U32(gl, 0x1c + 4 * U32(gl, 4));
        SU32(f.esp, 0xc, tag);
        world_bounds_check(i, COUNT());
        if (tag != U32(U32(U32(set, 0x10), 4 * i), 0x47c)) continue;
        world_bounds_check(i, COUNT());
#undef COUNT
        const rx_addr w = U32(U32(set, 0x10), 4 * i);
        const float hscale = rx_bf(world_x87_pass(U32(w, 0x490))), hofs = rx_bf(world_x87_pass(U32(w, 0x494)));
        uint32_t ok;
        WORLD_CALL_SP(0x14, ok = rx_world_water_patch_height(U32(w, 0x54), q, w + 0x3c, hscale, hofs));
        if (ok == 0) continue;
        if (!(F32(q, 4) > F32(pt, 4))) continue;
        SU32(pt, 4, U32(q, 4)); SU32(pt, 0, U32(q, 0)); SU32(pt, 8, U32(q, 8));
        found = 1;
        SU32(f.esp, 8, 1);
    }
    world_frame_leave(&f);
    return found;
}
RX_FUNCTION(0x007efef0, rx_world_water_set_height)

/* 007f8e00 PackedGrid_SampleUV (ECX = grid, EAX = level; stdcall u, v; RET 8; EAX): the packed sample of level `level` at
 * (u, v) clamped to [0, 1]: index = trunc(floor(w * u) * stride + w * v) (x87, w = +0x48 [level] the width, stride
 * +0x38 [level]; floor of msvcr90), bits per sample +0x68 [level], data +0x88 [level]: the sample's bits (possibly across two
 * dwords).
 * The frame is left as the original leaves it (dead after the RET 8, but a later particle update copies this stack
 * uninitialized into heap records, notes/world.md): sub esp, 0xc; push ebx / ebp / esi / edi (`saved`, the caller's);
 * [E-0xc] the fistp qword index; [E-0x24] the floor argument w * u (floor stores its result back there), [E-0x28] the
 * return address 0x7f8e6f, floor's stmxcsr / fnstcw at [E-0x2c] / [E-0x30]; the argument slots end as hi (u) and word (v). */
uint32_t rx_world_packed_grid_sample_regs(rx_addr grid, uint32_t level, float u, float v, const uint32_t saved[4], uint32_t* edx,
                                          uint32_t* xmm0) {
    const rx_addr e = rx_sp();                                /* [e] the return address, [e+4] u, [e+8] v */
    SU32(e, -0x10, saved[0]); SU32(e, -0x14, saved[1]); SU32(e, -0x18, saved[2]); SU32(e, -0x1c, saved[3]);
    if (u >= 1.0f) { u = 1.0f; SF32(e, 4, u); }               /* 0x931f24 */
    if (v >= 1.0f) { v = 1.0f; SF32(e, 8, v); }
    if (0.0f >= u) { u = 0.0f; SF32(e, 4, u); }
    if (0.0f >= v) { v = 0.0f; SF32(e, 8, v); }
    const float w = F32(grid, 0x48 + 4 * level);
    const uint32_t bits = U32(grid, 0x68 + 4 * level);
    const rx_f80 wf = rx_f80_from_f32(w);
    SF32(e, -0xc, w);                                         /* fst dword [esp+0x10] */
    const rx_addr arg = e - 0x24;                             /* sub esp, 8; fstp qword [esp]; call floor */
    SF64(arg, 0, rx_f80_to_f64(rx_f80_mul(wf, rx_f80_from_f32(u), X87_BITS, 0), 0));
    SU32(e, -0x28, 0x007f8e6f);
    SU32(e, -0x2c, 0x1f80);                                   /* floor: sub esp, 8; stmxcsr [esp+4]; fnstcw [esp] */
    SU16(e, -0x30, CRT_CW_MAIN);
    /* xmm0 lane 0 at the RET: floor's psrlq 52 / and 0x7ff of the argument (u, v >= 0 after the clamps: the positive paths); a NaN u
     * passes the clamps (comiss unordered: jb taken) and a negative NaN takes floor's sign branch 785a3052, where xmm0 is the
     * cmpltpd mask of the NaN: 0 */
    const uint32_t e11 = (uint32_t)(rx_db(F64(arg, 0)) >> 52);
    if (xmm0) *xmm0 = (e11 & 0x800) ? 0 : e11 & 0x7ff;
    double fu;   /* floor's NaN path saves the registers of this point (EBX bits, EBP the caller's, ESI level, EDI grid) */
    CRT_WITH_REGS(bits, saved[1], level, grid, fu = crt_floor_args(arg, CRT_CW_MAIN, 0x1f80));
    rx_f80 x = rx_f80_mul(rx_f80_from_f64(fu), rx_f80_from_f32(F32(grid, 0x38 + 4 * level)), X87_BITS, 0);
    x = rx_f80_add(x, rx_f80_mul(wf, rx_f80_from_f32(v), X87_BITS, 0), X87_BITS, 0);
    const int64_t i64 = rx_f80_to_int(x, 3);                  /* fistp qword with the rounding forced to truncation */
    SU32(e, -0xc, (uint32_t)i64); SU32(e, -8, (uint32_t)((uint64_t)i64 >> 32));
    const uint32_t index = (uint32_t)i64;
    const uint32_t pos = index * bits, word = pos >> 5, off = pos & 31;
    const uint32_t hi = bits + off >= 32 ? bits + off - 32 : 0;
    SU32(e, 4, hi); SU32(e, 8, word);
    const rx_addr data = U32(grid, 0x88 + 4 * level);
    const uint32_t mask = 0xffffffffu >> ((32 - bits) & 31);
    uint32_t r = ((mask << off) & U32(data, 4 * word)) >> off;
    uint32_t high = 0;                                        /* EDX at the RET: the second dword's bits (0 when none) */
    if (hi != 0) high = (U32(data, 4 * word + 4) & (0xffffffffu >> ((32 - hi) & 31))) << ((bits - hi) & 31);
    r |= high;
    if (edx) *edx = high;
    return r;
}
uint32_t rx_world_packed_grid_sample(rx_addr grid, uint32_t level, float u, float v, const uint32_t saved[4]) {
    return rx_world_packed_grid_sample_regs(grid, level, u, v, saved, 0, 0);
}
RX_FUNCTION(0x007f8e00, rx_world_packed_grid_sample)

/* 007f8d10: ESI = grid (ECX is ignored); stack u, v, layer; RET 12, EAX sample.
 * Interleaved format: +14 total record bits, +48 width, +38 stride, +88 data;
 * +68[layer] field width and +78[layer] field shift. UV is deliberately NOT clamped.
 * x87 main-thread CW 027f (53-bit); saved = incoming EBX, EBP, EDI.
 * Reproduces the dead frame and overwritten u/v slots, as 007f8e00 does. */
uint32_t rx_world_packed_grid_interleaved(rx_addr grid, float u, float v, uint32_t layer,
                                        const uint32_t saved[3], rx_v4* xmm0) {
    const rx_addr e = rx_sp();
    SU32(e, -0x10, saved[0]); SU32(e, -0x14, saved[1]); SU32(e, -0x18, saved[2]);
    const float w = F32(grid, 0x48);
    const uint32_t bits = U32(grid, 0x14);
    SF32(e, -8, w);
    const rx_addr arg = e - 0x20;
    const double product = rx_f80_to_f64(rx_f80_mul(rx_f80_from_f32(w), rx_f80_from_f32(u), X87_BITS, 0), 0);
    SF64(arg, 0, product);
    SU32(e, -0x24, 0x007f8d2f);
    SU32(e, -0x28, 0x1f80); SU16(e, -0x2c, CRT_CW_MAIN);
    if (xmm0) {
        const uint64_t raw = rx_db(product);
        const uint32_t exponent = (uint32_t)(raw >> 52), shift = 0x433u - (exponent & 0x7ff);
        const uint64_t truncated = shift > 63 ? 0 : (raw >> shift) << shift;
        uint64_t result = exponent & 0x7ff;
        if (exponent & 0x800) {
            result = product < rx_bd(truncated) ? UINT64_MAX : 0;
            if (exponent >= 0xbff && exponent <= 0xc32) result &= UINT64_C(0x3ff0000000000000);
        }
        *xmm0 = v4(rx_bf((uint32_t)result), rx_bf((uint32_t)(result >> 32)), 0.0f, 0.0f);
    }
    double fu;   /* floor's NaN path saves the registers of this point (EBX / EBP the caller's, ESI grid, EDI bits) */
    CRT_WITH_REGS(saved[0], saved[1], grid, bits, fu = crt_floor_args(arg, CRT_CW_MAIN, 0x1f80));
    rx_f80 x = rx_f80_mul(rx_f80_from_f64(fu), rx_f80_from_f32(F32(grid, 0x38)), X87_BITS, 0);
    x = rx_f80_add(x, rx_f80_mul(rx_f80_from_f32(w), rx_f80_from_f32(v), X87_BITS, 0), X87_BITS, 0);
    SU16(e, 4, CRT_CW_MAIN); SU32(e, 8, CRT_CW_MAIN | 0xc00);
    const int64_t index = rx_f80_to_int(x, 3);
    SU32(e, -8, (uint32_t)index); SU32(e, -4, (uint32_t)((uint64_t)index >> 32));
    const uint32_t pos = (uint32_t)index * bits, word = pos >> 5, off = pos & 31;
    const uint32_t hi = bits + off >= 32 ? bits + off - 32 : 0;
    SU32(e, 4, hi); SU32(e, 8, word);
    const rx_addr data = U32(grid, 0x88);
    const uint32_t mask = 0xffffffffu >> ((32 - U32(grid, 0x68 + 4 * layer)) & 31);
    uint32_t r = ((mask << off) & U32(data, 4 * word)) >> off;
    if (hi) r |= (U32(data, 4 * word + 4) & (0xffffffffu >> ((32 - hi) & 31))) << ((bits - hi) & 31);
    return (r >> (U32(grid, 0x78 + 4 * layer) & 31)) & mask;
}
RX_FUNCTION(0x007f8d10, rx_world_packed_grid_interleaved)
