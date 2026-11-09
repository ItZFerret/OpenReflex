/* phys_cylinder.c -- the closest point on a collision cylinder's surface (shape +0x20 radius, +0x24 height; local frame, axis y,
 * centred) to a point: the candidates are the side point (radial projection at the radius, y clamped to [-h/2, h/2]), the top
 * and the bottom cap points (x, z scaled in to the radius when outside it); the nearest wins (ties: the bottom). The original
 * builds each candidate in a stack vec4 and copies the chosen one whole: lane 3 is a word of its dead stack, which the port
 * reads at the same frame address (the dead-stack trap). */
#include "phys_cylinder.h"

/* the comparisons as 0 / 1 floats (seta + cvtsi2ss) */
static float gt(float a, float b) { return a > b ? 1.0f : 0.0f; }
static float ge(float a, float b) { return a >= b ? 1.0f : 0.0f; }
static float sq(float a) { return phys_mul(a, a); }
static rx_v4 with_stack_w(float x, float y, float z, rx_addr w_slot) { return v4(x, y, z, F32(w_slot, 0)); }

/* the scale that brings (x, z) to the radius: s itself below 1, else 1 (no change) */
static float cap_scale(float s) { return phys_add(phys_mul(gt(1.0f, s), s), ge(s, 1.0f)); }

/* ---- 00846c70 (EDI = cylinder, ESI = point in / out): the point replaced by the closest surface point. Frame: push ebp; and esp,
 * -16; sub esp, 0x40 (top +0x10, side +0x20, bottom +0x30). Returns the ECX the original leaves (1 when the top won). */
uint32_t phys_cylinder_closest_point(rx_addr cylinder, rx_addr point) {
    const rx_addr L = ((rx_sp() - 4) & ~15u) - 0x40;
    const float x = F32(point, 0), y = F32(point, 4), z = F32(point, 8);
    const float h = phys_mul(F32(cylinder, 0x24), 0.5f), nh = phys_sub(-0.0f, h);
    const float s = phys_div(F32(cylinder, 0x20), phys_sqrt(phys_add(phys_add(sq(x), sq(z)), 1e-07f /* 0x977324 */)));
    /* the side point */
    const float rx = phys_mul(x, s), rz = phys_mul(z, s);
    const float y1 = phys_add(phys_mul(phys_sub(h, y), gt(y, h)), y);
    const float y2 = phys_add(phys_mul(phys_sub(nh, y1), gt(nh, y1)), y1);
    const float d_side = phys_add(phys_add(sq(phys_sub(rz, z)), sq(phys_sub(y2, y))), sq(phys_sub(rx, x)));
    /* the caps */
    const float k = cap_scale(s);
    const float tz = phys_mul(z, k), tx = phys_mul(x, k);
    const float d_top = phys_add(phys_add(sq(phys_sub(tz, z)), sq(phys_sub(h, y))), sq(phys_sub(tx, x)));
    const float bx = phys_mul(x, k), bz = phys_mul(z, k);
    const float d_bot = phys_add(phys_add(sq(phys_sub(bz, z)), sq(phys_sub(nh, y))), sq(phys_sub(bx, x)));
    const int top = d_side > d_top && d_bot > d_top, side = d_top > d_side && d_bot > d_side;
    if (top) v4_st(point, with_stack_w(tx, h, tz, L + 0x1c));
    else if (side) v4_st(point, with_stack_w(rx, y2, rz, L + 0x2c));
    else v4_st(point, with_stack_w(bx, nh, bz, L + 0x3c));
    return (uint32_t)top;
}
RX_FUNCTION(0x00846c70, phys_cylinder_closest_point)

/* ---- 00846f10 (ESI = cylinder, EDI = point in / out; stack normal out*; RET 4): as 00846c70, with the surface normal: (0, 1, 0)
 * on the top, (0, -1, 0) on the bottom (-1 from the writable float 0x9cd9e8), the unit radial direction on the side (y = inv *
 * 0, unnormalized when the radial length is zero). Frame: push ebp; and esp, -16; sub esp, 0x8c; push ebx (side +0x30 / +0x40,
 * bottom +0x50 / +0x70, top +0x60 / +0x80). Returns the ECX the original leaves (1 when the top won). */
uint32_t phys_cylinder_closest_point_normal(rx_addr cylinder, rx_addr point, rx_addr normal) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x8c - 4;
    const float x = F32(point, 0), y = F32(point, 4), z = F32(point, 8);
    const float h = phys_mul(F32(cylinder, 0x24), 0.5f), nh = phys_sub(-0.0f, h);
    const float s = phys_div(F32(cylinder, 0x20), phys_sqrt(phys_add(phys_add(sq(x), sq(z)), 1e-07f)));
    const float rx = phys_mul(x, s), rz = phys_mul(z, s);
    const float y1 = phys_add(phys_mul(phys_sub(h, y), gt(y, h)), y);
    const float y2 = phys_add(phys_mul(phys_sub(nh, y1), gt(nh, y1)), y1);
    const float d_side = phys_add(phys_add(sq(phys_sub(y2, y)), sq(phys_sub(rz, z))), sq(phys_sub(rx, x)));
    float nx = rx, ny = 0.0f, nz = rz;
    const float r2 = phys_add(sq(rz), sq(rx));
    if (r2 > 0.0f) {
        const float inv = phys_div(1.0f, phys_sqrt(r2));
        nx = phys_mul(inv, rx); nz = phys_mul(inv, rz); ny = phys_mul(inv, 0.0f);
    }
    const float k = cap_scale(s);
    const float tx = phys_mul(x, k), tz = phys_mul(z, k);
    const float d_top = phys_add(phys_add(sq(phys_sub(tz, z)), sq(phys_sub(h, y))), sq(phys_sub(tx, x)));
    const float bz = phys_mul(z, k), bx = phys_mul(x, k);
    const float d_bot = phys_add(phys_add(sq(phys_sub(bz, z)), sq(phys_sub(nh, y))), sq(phys_sub(bx, x)));
    const int top = d_side > d_top && d_bot > d_top, side = d_top > d_side && d_bot > d_side;
    /* The frame as the original leaves it (disassembly 00846f10..008472c2; L = its ESP after push ebx). Later code reads this
     * stack uninitialized: 0081c590's Newton step is its never-written slot S + 0x60, which in a composite pair's child loop
     * (008732d0: BoxVsCylinder, then CylinderVsCylinder) is this frame's +0x20, the side distance (core/notes/phys.md "The
     * Newton step"). Not written by the original: +0x04..+0x0c and the w lanes +0x3c / +0x4c / +0x5c / +0x6c / +0x7c / +0x8c
     * (read below); +0x00 (the caller's EBX) and the caller's EBP above the return address are not modelled. */
    SF32(L, 0x10, h); SF32(L, 0x14, x); SF32(L, 0x18, z); SF32(L, 0x1c, y); SF32(L, 0x20, d_side); SF32(L, 0x24, r2);
    SF32(L, 0x28, nh); SF32(L, 0x2c, s); SF32(L, 0x30, rx); SF32(L, 0x34, y2); SF32(L, 0x38, rz);
    SF32(L, 0x40, nx); SF32(L, 0x44, ny); SF32(L, 0x48, nz);
    SF32(L, 0x50, bx); SF32(L, 0x54, nh); SF32(L, 0x58, bz); SF32(L, 0x60, tx); SF32(L, 0x64, h); SF32(L, 0x68, tz);
    SF32(L, 0x70, 0.0f); SF32(L, 0x74, F32(0, 0x009cd9e8)); SF32(L, 0x78, 0.0f);
    SF32(L, 0x80, 0.0f); SF32(L, 0x84, 1.0f /* 0x931f24 */); SF32(L, 0x88, 0.0f);
    /* the last SqrtF (0042c230) call's stack: its argument, the return address and its push ebp (this function's EBP) */
    SF32(L - 4, 0, r2 > 0.0f ? r2 : phys_add(phys_add(sq(x), sq(z)), 1e-07f));
    SU32(L - 8, 0, r2 > 0.0f ? 0x00847091u : 0x00846f6du);
    SU32(L - 12, 0, sp0 - 4);
    if (top) {
        v4_st(point, with_stack_w(tx, h, tz, L + 0x6c));
        v4_st(normal, with_stack_w(0.0f, 1.0f, 0.0f, L + 0x8c));
    } else if (side) {
        v4_st(point, with_stack_w(rx, y2, rz, L + 0x3c));
        v4_st(normal, with_stack_w(nx, ny, nz, L + 0x4c));
    } else {
        v4_st(point, with_stack_w(bx, nh, bz, L + 0x5c));
        v4_st(normal, with_stack_w(0.0f, F32(0, 0x009cd9e8), 0.0f, L + 0x7c));
    }
    return (uint32_t)top;
}
RX_FUNCTION(0x00846f10, phys_cylinder_closest_point_normal)
