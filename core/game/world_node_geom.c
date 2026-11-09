/* world_node_geom.c -- the line / cylinder intersections of the physics world's node share (0x0081cxxx .. 0x0081exxx).
 *
 * 0081e0b0 / 0081e380 / 0081e630: a line through the origin o with direction d against an axis-aligned cylinder of radius r
 * around the line {axis coordinate = c} (x, y or z axis: the variant), with its half extents a, b along the two other axes:
 * the two intersection points (hit1, hit2) and their "inside the extents" weights w1 / w2 (1 or 0, as the original's product
 * of four 0.0 / 1.0 doubles converted back to float). A line that misses (discriminant <= 0) gives zero points and weights.
 * The arithmetic follows the original's operation order exactly (float SSE; -0.0 - x for the negations). */
#include "world_util.h"
#include "world_node.h"
#include "phys_crt.h"

/* 1 when both |p| <= a and |q| <= b as the original tests them: (a - p >= 0) (p + a >= 0) (b - q >= 0) (q + b >= 0) */
static float extent_weight(float p, float a, float q, float b) {
    return phys_sub(a, p) >= 0.0f && phys_add(p, a) >= 0.0f && phys_sub(b, q) >= 0.0f && phys_add(q, b) >= 0.0f ? 1.0f : 0.0f;
}
static void zero3(rx_addr p) { SF32(p, 0, 0.0f); SF32(p, 4, 0.0f); SF32(p, 8, 0.0f); }

/* 0081e0b0 (EDI = d, ESI = hit1; stdcall o, r, c, a, b, hit2, w1, w2; RET 0x20): the cylinder around the x axis line
 * (x = c): the main direction d.y (1 / d.y), len2 = d.z^2 + d.y^2, c' = c - o.x; weights over (y, z) */
void rx_world_line_cylinder_x(rx_addr d, rx_addr hit1, rx_addr o, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2) {
    const float dy = F32(d, 4);
    const float inv_dy = phys_div(1.0f, dy);
    const float len2 = phys_add(phys_mul(F32(d, 8), F32(d, 8)), phys_mul(dy, dy));
    const float inv_len2 = phys_div(1.0f, len2);
    const float ox = F32(o, 0);
    const float cc = phys_sub(c, ox);
    const float disc = phys_mul(phys_mul(phys_sub(phys_mul(phys_mul(len2, r), r), phys_mul(cc, cc)), dy), dy);
    if (!(disc > 0.0f)) {
        SF32(w1, 0, 0.0f); SF32(w2, 0, 0.0f); zero3(hit1); zero3(hit2);
        return;
    }
    const float s = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(disc))));
    const float dx = F32(d, 0), dz = F32(d, 8);
    const float t1 = phys_mul(phys_sub(s, phys_mul(phys_mul(dx, dz), cc)), inv_len2);
    const float u1 = phys_mul(phys_add(phys_mul(dz, t1), phys_mul(dx, cc)), inv_dy);
    SF32(hit1, 8, phys_add(F32(o, 8), t1));
    SF32(hit1, 4, phys_add(phys_sub(-0.0f, u1), F32(o, 4)));
    SF32(hit1, 0, phys_add(ox, cc));
    const float t2 = phys_mul(phys_sub(phys_sub(-0.0f, phys_mul(phys_mul(F32(d, 0), F32(d, 8)), cc)), s), inv_len2);
    const float u2 = phys_mul(phys_add(phys_mul(F32(d, 8), t2), phys_mul(F32(d, 0), cc)), inv_dy);
    SF32(hit2, 4, phys_add(phys_sub(-0.0f, u2), F32(o, 4)));
    SF32(hit2, 0, phys_add(F32(o, 0), cc));
    SF32(hit2, 8, phys_add(F32(o, 8), t2));
    SF32(w1, 0, extent_weight(F32(hit1, 4), a, F32(hit1, 8), b));
    SF32(w2, 0, extent_weight(F32(hit2, 4), a, F32(hit2, 8), b));
}
RX_FUNCTION(0x0081e0b0, rx_world_line_cylinder_x)

/* 0081e380 (ESI = o, EDI = hit1; stdcall d, r, c, a, b, hit2, w1, w2; RET 0x20): around the y axis line (y = c): main
 * direction d.x, len2 = d.z^2 + d.x^2, c' = c - o.y; weights over (x, z) */
void rx_world_line_cylinder_y(rx_addr o, rx_addr hit1, rx_addr d, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2) {
    const float dx = F32(d, 0);
    const float inv_dx = phys_div(1.0f, dx);
    const float len2 = phys_add(phys_mul(F32(d, 8), F32(d, 8)), phys_mul(dx, dx));
    const float inv_len2 = phys_div(1.0f, len2);
    const float cc = phys_sub(c, F32(o, 4));
    const float disc = phys_mul(phys_mul(phys_sub(phys_mul(phys_mul(len2, r), r), phys_mul(cc, cc)), dx), dx);
    if (!(disc > 0.0f)) {
        SF32(w1, 0, 0.0f); SF32(w2, 0, 0.0f); zero3(hit1); zero3(hit2);
        return;
    }
    const float s = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(disc))));
    const float dy = F32(d, 4), dz = F32(d, 8);
    const float t1 = phys_mul(phys_sub(s, phys_mul(phys_mul(dy, dz), cc)), inv_len2);
    const float u1 = phys_mul(phys_add(phys_mul(dz, t1), phys_mul(dy, cc)), inv_dx);
    const float oy = F32(o, 4), oz = F32(o, 8);
    SF32(hit1, 0, phys_add(phys_sub(-0.0f, u1), F32(o, 0)));
    SF32(hit1, 8, phys_add(t1, oz));
    SF32(hit1, 4, phys_add(oy, cc));
    const float t2 = phys_mul(phys_sub(phys_sub(-0.0f, phys_mul(phys_mul(F32(d, 4), F32(d, 8)), cc)), s), inv_len2);
    const float u2 = phys_mul(phys_add(phys_mul(F32(d, 8), t2), phys_mul(F32(d, 4), cc)), inv_dx);
    const float oy2 = F32(o, 4);
    SF32(hit2, 0, phys_add(phys_sub(-0.0f, u2), F32(o, 0)));
    SF32(hit2, 4, phys_add(oy2, cc));
    SF32(hit2, 8, phys_add(t2, F32(o, 8)));
    SF32(w1, 0, extent_weight(F32(hit1, 0), a, F32(hit1, 8), b));
    SF32(w2, 0, extent_weight(F32(hit2, 0), a, F32(hit2, 8), b));
}
RX_FUNCTION(0x0081e380, rx_world_line_cylinder_y)

/* 0081e630 (ESI = o, EDI = hit1; stdcall d, r, c, a, b, hit2, w1, w2; RET 0x20): around the z axis line (z = c): main
 * direction d.x, len2 = d.y^2 + d.x^2, c' = c - o.z; weights over (x, y) */
void rx_world_line_cylinder_z(rx_addr o, rx_addr hit1, rx_addr d, float r, float c, float a, float b, rx_addr hit2,
                              rx_addr w1, rx_addr w2) {
    const float dx = F32(d, 0);
    const float inv_dx = phys_div(1.0f, dx);
    const float len2 = phys_add(phys_mul(F32(d, 4), F32(d, 4)), phys_mul(dx, dx));
    const float inv_len2 = phys_div(1.0f, len2);
    const float cc = phys_sub(c, F32(o, 8));
    const float disc = phys_mul(phys_mul(phys_sub(phys_mul(phys_mul(len2, r), r), phys_mul(cc, cc)), dx), dx);
    if (!(disc > 0.0f)) {
        SF32(w1, 0, 0.0f); SF32(w2, 0, 0.0f); zero3(hit1); zero3(hit2);
        return;
    }
    const float s = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(disc))));
    const float dy = F32(d, 4), dz = F32(d, 8);
    const float oz = F32(o, 8);
    const float t1 = phys_mul(phys_sub(s, phys_mul(phys_mul(dy, dz), cc)), inv_len2);
    const float oy = F32(o, 4);
    const float u1 = phys_mul(phys_add(phys_mul(dy, t1), phys_mul(dz, cc)), inv_dx);
    SF32(hit1, 0, phys_add(phys_sub(-0.0f, u1), F32(o, 0)));
    SF32(hit1, 8, phys_add(oz, cc));
    SF32(hit1, 4, phys_add(oy, t1));
    const float t2 = phys_mul(phys_sub(phys_sub(-0.0f, phys_mul(phys_mul(F32(d, 4), F32(d, 8)), cc)), s), inv_len2);
    const float oz2 = F32(o, 8), oy2 = F32(o, 4);
    const float u2 = phys_mul(phys_add(phys_mul(F32(d, 4), t2), phys_mul(F32(d, 8), cc)), inv_dx);
    SF32(hit2, 0, phys_add(phys_sub(-0.0f, u2), F32(o, 0)));
    SF32(hit2, 4, phys_add(oy2, t2));
    SF32(hit2, 8, phys_add(oz2, cc));
    SF32(w1, 0, extent_weight(F32(hit1, 0), a, F32(hit1, 4), b));
    SF32(w2, 0, extent_weight(F32(hit2, 0), a, F32(hit2, 4), b));
}
RX_FUNCTION(0x0081e630, rx_world_line_cylinder_z)

/* 0081ecb0 (EAX = the box half extents (ex, ey, ez); stdcall d, o, r, sum vec3*, weight float*; RET 0x14; frame push ebp;
 * and esp, -16; sub 0x104): the line (o, d) against the 12 edges of the box as cylinders of radius r: the edges along x at
 * (y, z) = ... use 0081e0b0 with c = +ex / -ex, along y 0081e380 with c = +ey / -ey, along z 0081e630 with c = +ez / -ez
 * (24 points and weights in the frame: weights at +0x14, 16-byte points at +0x44); then sum += point * weight for each and
 * *weight += the weights (in the original's order). */
void rx_world_line_box_edges(rx_addr ext, rx_addr d, rx_addr o, float r, rx_addr sum, rx_addr wsum) {
    world_frame f = world_frame_enter(0x104, 0, 1);
    const rx_addr S = f.esp, W = S + 0x14, P = S + 0x44;
    SU32(S, 8, world_x87_pass(U32(ext, 8)));
    SU32(S, 0xc, world_x87_pass(U32(ext, 4)));
    SU32(S, 0x10, U32(ext, 0));
    const float ex = F32(S, 0x10), ey = rx_bf(world_x87_pass(rx_fb(F32(S, 0xc)))), ez = F32(S, 8);
    const float rr = rx_bf(world_x87_pass(rx_fb(r)));
    const float exp_ = rx_bf(world_x87_pass(rx_fb(ex)));
    rx_set_sp(S - 0xc);   /* push ebx; push esi; push edi */
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_x(d, P + 0x00, o, rr, exp_, ey, ez, P + 0x10, W + 0x00, W + 0x04));
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_x(d, P + 0x20, o, rr, phys_sub(-0.0f, F32(S, 0x10)), ey, ez, P + 0x30, W + 0x08, W + 0x0c));
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_y(o, P + 0x40, d, rr, ey, exp_, ez, P + 0x50, W + 0x10, W + 0x14));
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_y(o, P + 0x60, d, rr, phys_sub(-0.0f, F32(S, 0xc)), exp_, ez, P + 0x70, W + 0x18, W + 0x1c));
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_z(o, P + 0x80, d, rr, ez, exp_, ey, P + 0x90, W + 0x20, W + 0x24));
    WORLD_CALL_SP(0x20, rx_world_line_cylinder_z(o, P + 0xa0, d, rr, phys_sub(-0.0f, F32(S, 8)), exp_, ey, P + 0xb0, W + 0x28, W + 0x2c));
    for (uint32_t k = 0; k < 12; k += 6) {
        for (uint32_t j = 0; j < 6; j++) {
            const rx_addr p = P + 0x10 * (k + j);
            const float w = F32(W, 4 * (k + j));
            SF32(sum, 0, phys_add(F32(sum, 0), phys_mul(F32(p, 0), w)));
            SF32(sum, 4, phys_add(F32(sum, 4), phys_mul(F32(p, 4), w)));
            SF32(sum, 8, phys_add(F32(sum, 8), phys_mul(F32(p, 8), w)));
        }
        float t = phys_add(F32(W, 4 * k), F32(wsum, 0));
        for (uint32_t j = 1; j < 6; j++) t = phys_add(t, F32(W, 4 * (k + j)));
        SF32(wsum, 0, t);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0081ecb0, rx_world_line_box_edges)

/* one hit of 0081d480's segment: p0 + t d, accumulated into sum / count */
static void cone_add(rx_addr sum, rx_addr count, float x, float y, float z) {
    SF32(sum, 0, phys_add(F32(sum, 0), x));
    SF32(sum, 4, phys_add(F32(sum, 4), y));
    SF32(sum, 8, phys_add(F32(sum, 8), z));
    SF32(count, 0, phys_add(F32(count, 0), 1.0f));
}

/* 0081d480 (ECX = p0, EAX = p1, ESI = sum vec3, EDI = count float; stdcall h, r0, r1; RET 0xc; frame push ebp; and esp,
 * -16; sub 0x50): the segment p0 -> p1 against the cone frustum around the y axis between y = -h (radius r0) and y = +h
 * (radius r1): with d = p1 - p0, k = (r1 - r0) / 2h and the radius at p0 R = (r0 + r1) / 2 + p0.y k, the quadratic
 * A t^2 + B t + C = 0 (A = (k d.y + d.z)(d.z - k d.y) + d.x^2, B = 2 (p0.x d.x - R k d.y + p0.z d.z), C = p0.x^2 - R^2 +
 * p0.z^2; sqrt |B^2 - 4 C A|; A, B and d.y nudged by 1e-6 when >= 0 before dividing) gives the side hits (0 < t < 1, -h < y
 * < h), the linear case (A == 0, B != 0) one, and the caps y = -h (inside r0) and y = +h (inside r1) one each. Every hit adds
 * its point to sum and 1 to count. */
void rx_world_segment_cone(rx_addr p0, rx_addr p1, rx_addr sum, rx_addr count, float h, float r0, float r1) {
    world_frame f = world_frame_enter(0x50, 0, 1);
    const float ox = F32(p0, 0), oy = F32(p0, 4), oz = F32(p0, 8);
    const float dx = phys_sub(F32(p1, 0), ox), dy = phys_sub(F32(p1, 4), oy), dz = phys_sub(F32(p1, 8), oz);
    const float h2 = phys_mul(h, 2.0f /* 0x92e330 */);
    const float k = phys_div(phys_sub(r1, r0), h2);
    const float R = phys_add(phys_mul(phys_add(r0, r1), 0.5f /* 0x9322cc */), phys_mul(oy, k));
    const float kdy = phys_mul(k, dy);
    const float A = phys_add(phys_mul(phys_add(kdy, dz), phys_sub(dz, kdy)), phys_mul(dx, dx));
    const float B = phys_mul(phys_add(phys_sub(phys_mul(ox, dx), phys_mul(phys_mul(R, k), dy)), phys_mul(oz, dz)), 2.0f);
    const float C = phys_add(phys_sub(phys_mul(ox, ox), phys_mul(R, R)), phys_mul(oz, oz));
    const float disc = phys_sub(phys_mul(B, B), phys_mul(phys_mul(C, A), 4.0f /* 0x9322c0 */));
    const float s = phys_sqrtf(rx_bf(rx_fb(disc) & 0x7fffffffu));   /* Float_Abs 00401000, SqrtF 0042c230 */
    const float eps = 9.99999997e-07f /* 0x9777c4 */;
    const float inv2a = phys_div(1.0f, phys_mul(A >= 0.0f ? phys_add(A, eps) : A, 2.0f));
    const float Bn = B >= 0.0f ? phys_add(B, eps) : B;
    const float inv_dy = phys_div(1.0f, dy >= 0.0f ? phys_add(dy, eps) : dy);
    const int linear = A == 0.0f && !(B == 0.0f);
    const float mh = phys_sub(-0.0f, h);
#define SIDE_HIT(t) do { \
        const float t_ = (t); \
        if (t_ > 0.0f && 1.0f > t_) { \
            const float y_ = phys_add(phys_mul(t_, dy), oy), z_ = phys_add(phys_mul(t_, dz), oz); \
            const float x_ = phys_add(phys_mul(t_, dx), ox); \
            if (y_ > mh && h > y_) cone_add(sum, count, x_, y_, z_); \
        } \
    } while (0)
    if (disc > 0.0f && !(A == 0.0f)) {
        SIDE_HIT(phys_mul(phys_sub(s, B), inv2a));
        SIDE_HIT(phys_mul(phys_sub(phys_sub(-0.0f, B), s), inv2a));
    }
    if (linear) SIDE_HIT(phys_sub(-0.0f, phys_div(C, Bn)));
#undef SIDE_HIT
#define CAP_HIT(t, r) do { \
        const float t_ = (t); \
        if (t_ > 0.0f && 1.0f > t_) { \
            const float y_ = phys_add(phys_mul(t_, dy), oy), z_ = phys_add(phys_mul(t_, dz), oz); \
            const float x_ = phys_add(phys_mul(t_, dx), ox); \
            if (phys_mul((r), (r)) > phys_add(phys_mul(z_, z_), phys_mul(x_, x_))) cone_add(sum, count, x_, y_, z_); \
        } \
    } while (0)
    const float t_cap0 = phys_sub(-0.0f, phys_mul(phys_add(oy, h), inv_dy));
    CAP_HIT(t_cap0, r0);
    CAP_HIT(phys_mul(phys_sub(h, oy), inv_dy), r1);
#undef CAP_HIT
    /* The frame as the original leaves it (disassembly 0081d480..0081d9cd, S = ESP after sub 0x50): later code reads this stack
     * uninitialized (+0x0c, d.y, reaches CylinderVsCylinder 0086bd80's record point w lane after a BoxVsCylinder in a composite
     * pair: core/notes/phys.md "The Newton step"). +0x44 holds the y = -h cap's y only when its t is in (0, 1); +0x2c / +0x3c /
     * +0x40 / +0x48 / +0x4c are not written. Below S: SqrtF's argument |disc|, its return address and push ebp (Float_Abs's
     * return address and push ebp at S - 4 / S - 8 are overwritten by them). */
    {
        const rx_addr S = f.esp;
        SF32(S, 0x00, B); SF32(S, 0x04, Bn); SF32(S, 0x08, inv_dy); SF32(S, 0x0c, dy); SF32(S, 0x10, disc); SF32(S, 0x14, s);
        SF32(S, 0x18, inv2a); SF32(S, 0x1c, C); SF32(S, 0x20, ox); SF32(S, 0x24, oy); SF32(S, 0x28, oz);
        SF32(S, 0x30, dx); SF32(S, 0x34, dy); SF32(S, 0x38, dz);
        if (t_cap0 > 0.0f && 1.0f > t_cap0) SF32(S, 0x44, phys_add(phys_mul(t_cap0, dy), oy));
        SF32(S - 4, 0, rx_bf(rx_fb(disc) & 0x7fffffffu));
        SU32(S - 8, 0, 0x0081d5c8u);
        SU32(S - 12, 0, f.ebp);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0081d480, rx_world_segment_cone)

/* 0081beb0 (EDI = p0, EAX = p1; stdcall h, r, out hit0, out hit1, out flag0, out flag1; RET 0x18; frame push ebp; and esp,
 * -16; sub 0x68): the segment p0 -> p1 against the cylinder of radius r around the y axis between y = -h and +h: up to two
 * hit points (preset to the global point 0xa95d50) with their flags, in the order found: a segment on the axis (A = B = C =
 * 0: the end points inside the height), the two roots of A t^2 + B t + C (sqrt of the discriminant; 0 < t < 1, -h < y < h),
 * the linear case (A == 0, B != 0), then the caps y = -h and y = +h (inside the radius; 1 / d.y with d.y == 0 -> 1e-5).
 * The quadratic is the cone form of 0081d480 with slope 0 / 2h. The hit array and flags live in the frame (+0x50, +0x28).
 * EBP / EBX / ESI = the caller's (saved by the prologue): the frame is left as the original leaves it (later code reads this
 * stack uninitialized: notes/world.md traps). */
void rx_world_segment_cylinder(rx_addr p0, rx_addr p1, float h, float r, rx_addr out0, rx_addr out1, rx_addr flag0,
                               rx_addr flag1, uint32_t ebp, uint32_t ebx, uint32_t esi) {
    world_frame f = world_frame_enter(0x68, 2, 1);
    const rx_addr S = f.esp, HITS = S + 0x50, FLAGS = S + 0x28;
    for (uint32_t i = 0; i < 0xc; i += 4) { SU32(HITS, i, U32(0, 0x00a95d50 + i)); SU32(HITS + 0x10, i, U32(0, 0x00a95d50 + i)); }
    SU32(FLAGS, 0, 0); SU32(FLAGS, 4, 0);
    uint32_t n = 0;
#define HIT(x, y, z) do { const rx_addr p_ = HITS + 0x10 * n; SF32(p_, 0, (x)); SF32(p_, 4, (y)); SF32(p_, 8, (z)); \
                          SU32(FLAGS, 4 * n, 1); n++; } while (0)
    const float x0 = F32(p0, 0), y0 = F32(p0, 4), z0 = F32(p0, 8);
    const float x1 = F32(p1, 0), y1 = F32(p1, 4), z1 = F32(p1, 8);
    const float dz = phys_sub(z1, z0), dy = phys_sub(y1, y0), dx = phys_sub(x1, x0);
    const float k = phys_div(0.0f, phys_mul(h, 2.0f /* 0x92e330 */));
    const float R = phys_add(phys_mul(phys_mul(r, 2.0f), 0.5f /* 0x9322cc */), phys_mul(y0, k));
    const float kdy = phys_mul(dy, k);
    const float A = phys_add(phys_mul(phys_add(kdy, dz), phys_sub(dz, kdy)), phys_mul(dx, dx));
    const float B = phys_mul(phys_add(phys_sub(phys_mul(x0, dx), phys_mul(phys_mul(dy, R), k)), phys_mul(z0, dz)), 2.0f);
    const float C = phys_add(phys_sub(phys_mul(x0, x0), phys_mul(R, R)), phys_mul(z0, z0));
    const float disc = phys_sub(phys_mul(B, B), phys_mul(phys_mul(C, A), 4.0f /* 0x9322c0 */));
    const float mh = phys_sub(-0.0f, h);
    int quadratic = 0;
    float s = 0.0f, a2 = 0.0f;
    if (A == 0.0f && B == 0.0f && C == 0.0f) {
        if (y0 > mh && h > y0) HIT(x0, y0, z0);
        if (y1 > mh && h > y1) HIT(x1, y1, z1);
    } else if (disc > 0.0f && !(A == 0.0f)) {
        quadratic = 1;
        s = phys_sqrtf(rx_bf(world_x87_pass(rx_fb(disc))));
        a2 = phys_mul(A, 2.0f);
        const float t1 = phys_div(phys_sub(s, B), a2);
        const float hx1 = phys_add(x0, phys_mul(dx, t1)), hy1 = phys_add(y0, phys_mul(dy, t1));
        const float hz1 = phys_add(z0, phys_mul(dz, t1));
        if (t1 > 0.0f && 1.0f > t1 && hy1 > mh && h > hy1) HIT(hx1, hy1, hz1);
        const float t2 = phys_div(phys_sub(phys_sub(-0.0f, B), s), a2);
        const float hy2 = phys_add(y0, phys_mul(dy, t2)), hz2 = phys_add(z0, phys_mul(dz, t2));
        const float hx2 = phys_add(x0, phys_mul(dx, t2));
        if (t2 > 0.0f && 1.0f > t2 && hy2 > mh && h > hy2) HIT(hx2, hy2, hz2);
    } else if (A == 0.0f && !(B == 0.0f)) {
        const float t = phys_sub(-0.0f, phys_div(C, B));
        const float hx = phys_add(x0, phys_mul(dx, t)), hy = phys_add(y0, phys_mul(dy, t));
        const float hz = phys_add(phys_mul(dz, t), z0);
        if (t > 0.0f && 1.0f > t && hy > mh && h > hy) HIT(hx, hy, hz);
    }
    const float inv_dy = phys_div(1.0f, dy == 0.0f ? 9.99999975e-06f /* 0x9774a4 */ : dy);
    /* the cap y = -h */
    const float tm = phys_sub(-0.0f, phys_mul(phys_add(y0, h), inv_dy));
    const float hym = phys_add(y0, phys_mul(dy, tm)), hzm = phys_add(z0, phys_mul(dz, tm));
    const float hxm = phys_add(x0, phys_mul(dx, tm));
    if (tm > 0.0f && 1.0f > tm && phys_mul(r, r) > phys_add(phys_mul(hzm, hzm), phys_mul(hxm, hxm))) HIT(hxm, hym, hzm);
    {   /* the cap y = +h */
        const float t = phys_mul(phys_sub(h, y0), inv_dy);
        const float hx = phys_add(x0, phys_mul(dx, t)), hy = phys_add(y0, phys_mul(dy, t));
        const float hz = phys_add(z0, phys_mul(dz, t));
        if (t > 0.0f && 1.0f > t && phys_mul(r, r) > phys_add(phys_mul(hz, hz), phys_mul(hx, hx))) {
            const rx_addr p_ = HITS + 0x10 * n;
            SF32(p_, 0, hx); SF32(p_, 4, hy); SF32(p_, 8, hz);
            SU32(FLAGS, 4 * n, 1);
        }
    }
#undef HIT
    for (uint32_t i = 0; i < 0xc; i += 4) { SU32(out0, i, U32(HITS, i)); SU32(out1, i, U32(HITS + 0x10, i)); }
    SU32(flag0, 0, U32(FLAGS, 0));
    SU32(flag1, 0, U32(FLAGS, 4));
    /* the frame image: A, the discriminant, B, p1's y at +8..+0x14; +0x18 / +0x1c / +0x20 p1's x / z and p0's x, or with the
     * quadratic 2A / its root / p0's z; 1 / dy at +0x24; the -h cap's point and dy t / dz t at +0x30..+0x38 / +0x44 / +0x48;
     * with the quadratic, SqrtF's argument, return address and saved EBP below the frame */
    SU32(f.entry, -4, ebp); SU32(S, 4, ebx); SU32(S, 0, esi);   /* push ebp; ...; push ebx; push esi */
    SF32(S, 0x08, A); SF32(S, 0x0c, disc); SF32(S, 0x10, B); SF32(S, 0x14, y1);
    if (quadratic) { SF32(S, 0x18, a2); SF32(S, 0x1c, s); SF32(S, 0x20, z0); }
    else { SF32(S, 0x18, x1); SF32(S, 0x1c, z1); SF32(S, 0x20, x0); }
    SF32(S, 0x24, inv_dy);
    SF32(S, 0x30, hxm); SF32(S, 0x34, hym); SF32(S, 0x38, hzm);
    SF32(S, 0x44, phys_mul(dy, tm)); SF32(S, 0x48, phys_mul(dz, tm));
    if (quadratic) { SF32(S, -4, rx_bf(world_x87_pass(rx_fb(disc)))); SU32(S, -8, 0x0081c143); SU32(S, -0xc, f.ebp); }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0081beb0, rx_world_segment_cylinder)

/* 0081b6b0 (ECX = the box half extents e, ESI = p0, EAX = sum vec3, EDI = count float; stdcall p1; RET 4; frame push ebp;
 * and esp, -16; sub 0xd0; EBP = the caller's, saved): the segment p0 -> p1 against the 6 faces of the box [-e, e]: for
 * each face plane (x = +ex, -ex,
 * y = +ey, -ey, z = +ez, -ez) the parameter t = (plane - p0) / d (each d component nudged by +1e-5 when >= 0), the point
 * p0 + t d, and a weight 1 when 0 <= t <= 1 and the point lies within the face (the other two extents, inclusive); every
 * point is added to sum times its weight and the weights to count (in the original's order). */
void rx_world_segment_box_faces_ecx(rx_addr e, rx_addr p0, rx_addr sum, rx_addr count, rx_addr p1, uint32_t ebp, uint32_t* ecx) {
    world_frame f = world_frame_enter(0xd0, 0, 1);
    const float ex = F32(e, 0), ey = F32(e, 4), ez = F32(e, 8);
    const float x0 = F32(p0, 0), y0 = F32(p0, 4), z0 = F32(p0, 8);
    const float dx = phys_sub(F32(p1, 0), x0), dz = phys_sub(F32(p1, 8), z0), dy = phys_sub(F32(p1, 4), y0);
    const float eps = 9.99999975e-06f;   /* 0x9774a4 */
    const float ix = phys_div(1.0f, dx >= 0.0f ? phys_add(dx, eps) : dx);
    const float iy = phys_div(1.0f, dy >= 0.0f ? phys_add(dy, eps) : dy);
    const float iz = phys_div(1.0f, dz >= 0.0f ? phys_add(dz, eps) : dz);
    const float mex = phys_sub(-0.0f, ex), mey = phys_sub(-0.0f, ey), mez = phys_sub(-0.0f, ez);
    const float t[6] = {
        phys_mul(phys_sub(ex, x0), ix), phys_mul(phys_sub(mex, x0), ix),
        phys_mul(phys_sub(ey, y0), iy), phys_mul(phys_sub(mey, y0), iy),
        phys_mul(phys_sub(ez, z0), iz), phys_mul(phys_sub(mez, z0), iz),
    };
    float px[6], py[6], pz[6], w[6];
    for (int i = 0; i < 6; i++) {
        px[i] = phys_add(phys_mul(t[i], dx), x0);
        py[i] = phys_add(phys_mul(t[i], dy), y0);
        pz[i] = phys_add(phys_mul(t[i], dz), z0);
    }
#define IN(v, hi, lo) ((hi) >= (v) && (v) >= (lo))
#define T_OK(i) (t[i] >= 0.0f && 1.0f >= t[i])
    w[0] = (T_OK(0) && IN(py[0], ey, mey) && IN(pz[0], ez, mez)) ? 1.0f : 0.0f;
    w[1] = (T_OK(1) && IN(py[1], ey, mey) && IN(pz[1], ez, mez)) ? 1.0f : 0.0f;
    w[2] = (T_OK(2) && IN(px[2], ex, mex) && IN(pz[2], ez, mez)) ? 1.0f : 0.0f;
    w[3] = (T_OK(3) && IN(px[3], ex, mex) && IN(pz[3], ez, mez)) ? 1.0f : 0.0f;
    w[4] = (T_OK(4) && IN(py[4], ey, mey) && IN(px[4], ex, mex)) ? 1.0f : 0.0f;
    w[5] = (T_OK(5) && IN(py[5], ey, mey) && IN(px[5], ex, mex)) ? 1.0f : 0.0f;
#undef IN
#undef T_OK
    for (int i = 0; i < 6; i++) {
        SF32(sum, 0, phys_add(F32(sum, 0), phys_mul(px[i], w[i])));
        SF32(sum, 4, phys_add(F32(sum, 4), phys_mul(py[i], w[i])));
        SF32(sum, 8, phys_add(F32(sum, 8), phys_mul(pz[i], w[i])));
    }
    float c = F32(count, 0);
    for (int i = 0; i < 6; i++) c = phys_add(c, w[i]);
    SF32(count, 0, c);
    /* the frame image (later code reads this stack uninitialized: notes/world.md traps): [S] / [S+4] / [S+8] the last
     * weighted point components the sums took (point 1's x, point 4's y and z), the face planes -ez / -ex / -ey at +0x1c /
     * +0x3c / +0x40, the parameters t1..t5 and 1 / dz at +0x48..+0x5c, t4 * d at +0x60 / +0x64, point 5's z at +0x68, the
     * points 1, 2, 3, 0 at +0x70 / +0x80 / +0x90 / +0xa0 and point 4 at +0x20, the weights 0 and 1 at +0xb8 / +0xbc. */
    const rx_addr S = f.esp;
    SU32(f.entry, -4, ebp);   /* push ebp */
    SF32(S, 0, phys_mul(px[1], w[1])); SF32(S, 4, phys_mul(py[4], w[4])); SF32(S, 8, phys_mul(pz[4], w[4]));
    SF32(S, 0x1c, mez); SF32(S, 0x3c, mex); SF32(S, 0x40, mey);
    for (int i = 1; i < 6; i++) SF32(S, 0x44 + 4 * (uint32_t)i, t[i]);
    SF32(S, 0x5c, iz);
    SF32(S, 0x60, phys_mul(t[4], dx)); SF32(S, 0x64, phys_mul(t[4], dy)); SF32(S, 0x68, pz[5]);
    static const uint16_t at[5] = {0xa0, 0x70, 0x80, 0x90, 0x20};   /* points 0..4 */
    for (int i = 0; i < 5; i++) { SF32(S, at[i], px[i]); SF32(S, at[i] + 4u, py[i]); SF32(S, at[i] + 8u, pz[i]); }
    SF32(S, 0xb8, w[0]); SF32(S, 0xbc, w[1]);
    if (ecx) *ecx = w[5] != 0.0f;   /* ECX at the RET: face 5's flag (EDX = p1, the loaded argument) */
    world_frame_leave(&f);
}
void rx_world_segment_box_faces(rx_addr e, rx_addr p0, rx_addr sum, rx_addr count, rx_addr p1, uint32_t ebp) {
    rx_world_segment_box_faces_ecx(e, p0, sum, count, p1, ebp, 0);
}
RX_FUNCTION(0x0081b6b0, rx_world_segment_box_faces)

/* the last iteration of a Newton refinement, as its frame slots hold it afterwards */
typedef struct { float g[10], dY, mdY, dZ, xnz, znx, zcx, xcz4, dXny, dXdZ2, F1, F2, F3; } disc_rim_last;

/* the Newton refinement of 0081c590: 10 steps of (X, Y, Z) -= (gradient . residuals) * step towards the point on the disc
 * rim (centre c, normal n, radius R) that lies on the cylinder (radius^2 rc2 around the y axis). The step is the frame slot
 * S + 0x60: 1e7 once a step sees a zero determinant, else whatever that slot holds (the original never initialises it: a
 * dead-stack read, reproduced by reading the same game address). Returns the last residuals |F2| + |F1| + |F3|; *last gets
 * the last iteration's terms (g[1..9], residuals) for the frame image. */
static float disc_rim_newton(rx_addr S, float* X, float* Y, float* Z, float cx, float cy, float cz, float nx, float ny,
                             float nz, float rc2, float R2, disc_rim_last* last) {
    float x = *X, y = *Y, z = *Z;
    disc_rim_last t = {0};
    for (int it = 0; it < 10; it++) {
        t.dY = phys_sub(y, cy); t.mdY = phys_sub(cy, y); t.dZ = phys_sub(z, cz);
        const float dX = phys_sub(x, cx);
        t.g[1] = phys_mul(phys_add(phys_mul(phys_sub(cz, z), ny), phys_mul(t.dY, nz)), 2.0f);
        t.g[2] = phys_mul(phys_mul(z, ny), 2.0f);
        t.g[3] = phys_mul(phys_mul(t.mdY, z), 4.0f);
        t.g[4] = phys_mul(phys_add(phys_mul(phys_sub(cx, x), nz), phys_mul(t.dZ, nx)), 2.0f);
        t.xnz = phys_mul(x, nz); t.znx = phys_mul(z, nx);
        t.g[5] = phys_sub(phys_mul(t.xnz, 2.0f), phys_mul(t.znx, 2.0f));
        t.zcx = phys_mul(z, cx);
        const float xcz = phys_mul(x, cz);
        t.xcz4 = phys_mul(xcz, 4.0f);
        t.g[6] = phys_sub(t.xcz4, phys_mul(t.zcx, 4.0f));
        t.dXny = phys_mul(dX, ny);
        t.g[7] = phys_mul(phys_add(t.dXny, phys_mul(t.mdY, nx)), 2.0f);
        t.g[8] = phys_mul(phys_mul(x, ny), F32(0, 0x009d7b30));
        t.g[9] = phys_mul(phys_mul(t.dY, x), 4.0f);
        t.F1 = phys_add(phys_sub(phys_mul(z, z), rc2), phys_mul(x, x));
        t.dXdZ2 = phys_add(phys_mul(dX, dX), phys_mul(t.dZ, t.dZ));
        t.F2 = phys_sub(phys_add(t.dXdZ2, phys_mul(t.dY, t.dY)), R2);
        t.F3 = phys_add(phys_add(phys_mul(dX, nx), phys_mul(t.dZ, nz)), phys_mul(t.dY, ny));
        const float det = phys_mul(phys_add(phys_sub(phys_mul(xcz, ny), phys_mul(t.zcx, ny)), phys_mul(phys_sub(t.xnz, t.znx), t.dY)), 4.0f);
        if (det == 0.0f) SF32(S, 0x60, 1.0e7f /* 0x9777d4 */);
        const float step = F32(S, 0x60);
        x = phys_add(phys_sub(-0.0f, phys_mul(phys_add(phys_add(phys_mul(t.F3, t.g[3]), phys_mul(t.F2, t.g[2])), phys_mul(t.F1, t.g[1])), step)), x);
        y = phys_add(phys_sub(-0.0f, phys_mul(phys_add(phys_add(phys_mul(t.F3, t.g[6]), phys_mul(t.F2, t.g[5])), phys_mul(t.F1, t.g[4])), step)), y);
        z = phys_add(phys_sub(-0.0f, phys_mul(phys_add(phys_add(phys_mul(t.F3, t.g[9]), phys_mul(t.F2, t.g[8])), phys_mul(t.F1, t.g[7])), step)), z);
    }
    *X = x; *Y = y; *Z = z; *last = t;
    const float a = rx_bf(rx_fb(t.F2) & 0x7fffffffu), b = rx_bf(rx_fb(t.F1) & 0x7fffffffu), c = rx_bf(rx_fb(t.F3) & 0x7fffffffu);
    return phys_add(phys_add(a, b), c);   /* Float_Abs 00401000 x 3 */
}

static float f_abs(float v) { return rx_bf(rx_fb(v) & 0x7fffffffu); }

/* 0081c590 (ECX = n, the disc normal; EAX = c, its centre; ESI = 6 hit points (16 bytes each); EDI = 6 flags; stdcall obj,
 * R (disc radius), h (cylinder half height), rc (cylinder radius); EBX / EBP = the caller's (saved); RET 0x10; frame push
 * ebp; and esp, -16; sub 0xac; push ebx):
 * the rim of the disc against the cylinder around the y axis: hits 0 / 1 from two Newton refinements (start points c - R c/|c|
 * projected into the disc plane, +/- the half cross product q), accepted when the residuals are below obj +0xf0 and -h < y <
 * h; hits 2 / 3 and 4 / 5 where the rim crosses the planes y = +h and y = -h (inside rc). Hit points start as the global
 * point 0xa95d50, flags as 0.
 * The frame is left as the original leaves it (the last values of its locals, the callees' dead stack): later code reads
 * this stack uninitialized (a vector's w lane from S + 0x8c: notes/world.md traps). */
void rx_world_disc_cylinder(rx_addr n, rx_addr c, rx_addr hits, rx_addr flags, rx_addr obj, float R, float h, float rc,
                            uint32_t ebx, uint32_t ebp) {
    world_frame f = world_frame_enter(0xac, 1, 1);
    const rx_addr S = f.esp;
    for (uint32_t i = 0; i < 6; i++)
        for (uint32_t k = 0; k < 0xc; k += 4) SU32(hits + 0x10 * i, k, U32(0, 0x00a95d50 + k));
    const float nx = F32(n, 0), ny = F32(n, 4), nz = F32(n, 8);
    const float cx = F32(c, 0), cy = F32(c, 4), cz = F32(c, 8);
    for (uint32_t i = 0; i < 6; i++) SU32(flags, 4 * i, 0);
    const float L = phys_sqrtf(phys_add(phys_add(phys_mul(cz, cz), phys_mul(cy, cy)), phys_mul(cx, cx)));
    const float kR = phys_div(R, L);
    const float ux = phys_mul(phys_sub(-0.0f, cx), kR), uy = phys_mul(phys_sub(-0.0f, cy), kR), uz = phys_mul(phys_sub(-0.0f, cz), kR);
    const float dot = phys_add(phys_add(phys_mul(uz, nz), phys_mul(uy, ny)), phys_mul(ux, nx));
    const float q1 = phys_mul(phys_mul(phys_sub(phys_mul(cy, nz), phys_mul(cz, ny)), kR), 0.5f);
    const float q2 = phys_mul(phys_mul(phys_sub(phys_mul(cz, nx), phys_mul(cx, nz)), kR), 0.5f);
    const float q3 = phys_mul(phys_mul(phys_sub(phys_mul(cx, ny), phys_mul(cy, nx)), kR), 0.5f);
    const float bx = phys_add(phys_sub(ux, phys_mul(dot, nx)), cx);
    const float by = phys_add(phys_sub(uy, phys_mul(dot, ny)), cy);
    const float bz = phys_add(phys_sub(uz, phys_mul(dot, nz)), cz);
    const float rc2 = phys_mul(rc, rc), R2 = phys_mul(R, R);
    const float tol = F32(obj, 0xf0), mh = phys_sub(-0.0f, h);
    disc_rim_last last[2];
    float rx_[2], ry_[2], rz_[2];
    for (int side = 0; side < 2; side++) {
        float X = side ? phys_sub(bx, q1) : phys_add(bx, q1);
        float Y = side ? phys_sub(by, q2) : phys_add(by, q2);
        float Z = side ? phys_sub(bz, q3) : phys_add(bz, q3);
        const float res = disc_rim_newton(S, &X, &Y, &Z, cx, cy, cz, nx, ny, nz, rc2, R2, &last[side]);
        rx_[side] = X; ry_[side] = Y; rz_[side] = Z;
        if (tol > res && Y > mh && h > Y) {
            const rx_addr p = hits + 0x10 * (uint32_t)side;
            SF32(p, 4, Y); SF32(p, 0, X); SF32(p, 8, Z);
            SU32(flags, 4 * (uint32_t)side, 1);
        }
    }
    /* the rim crossing the planes y = +h (hits 2 / 3) and y = -h (hits 4 / 5) */
    const float Q = phys_add(phys_mul(nz, nz), phys_mul(nx, nx));
    float m1nx = 0.0f, D[2] = {0, 0}, absD[2] = {0, 0}, s0 = 0.0f, hy1 = 0.0f, z1_last = 0.0f;
    for (int cap = 0; cap < 2; cap++) {
        const float hy = cap == 0 ? phys_sub(h, cy) : phys_sub(phys_sub(-0.0f, h), cy);
        D[cap] = phys_mul(phys_mul(phys_sub(phys_mul(phys_mul(Q, R), R), phys_mul(hy, hy)), nx), nx);
        absD[cap] = rx_bf(world_x87_pass(rx_fb(f_abs(D[cap]))));   /* Float_Abs; fld / fstp */
        const float s = phys_sqrtf(absD[cap]);                        /* SqrtF */
        if (cap == 0) s0 = s; else hy1 = hy;
        const float invQ = phys_div(1.0f, Q);
        if (cap == 0) m1nx = phys_div(F32(0, 0x009cd9e8), nx);
        const float yc = phys_add(hy, cy);
        const float hnz = phys_mul(phys_mul(hy, nz), ny), hny = phys_mul(hy, ny);
        const float t1 = phys_mul(phys_sub(s, hnz), invQ);
        const float z1 = phys_add(t1, cz);
        z1_last = z1;
        const float x1 = phys_add(phys_mul(phys_add(phys_mul(t1, nz), hny), m1nx), cx);
        const rx_addr p1 = hits + 0x20 + 0x20 * (uint32_t)cap, p2 = p1 + 0x10;
        if (D[cap] > 0.0f && rc2 > phys_add(phys_mul(z1, z1), phys_mul(x1, x1))) {
            SF32(p1, 4, yc); SF32(p1, 0, x1); SF32(p1, 8, z1);
            SU32(flags, 8 + 8 * (uint32_t)cap, 1);
        }
        const float t2 = phys_mul(phys_sub(phys_sub(-0.0f, hnz), s), invQ);
        const float z2 = phys_add(t2, cz);
        const float x2 = phys_add(phys_mul(phys_add(phys_mul(t2, nz), hny), m1nx), cx);
        if (D[cap] > 0.0f && rc2 > phys_add(phys_mul(z2, z2), phys_mul(x2, x2))) {
            SF32(p2, 0, x2); SF32(p2, 4, yc); SF32(p2, 8, z2);
            SU32(flags, 0xc + 8 * (uint32_t)cap, 1);
        }
    }
    /* the frame image (S = esp after the prologue; offsets as the original's [esp + x] with no push active) */
    const disc_rim_last* l0 = &last[0];
    const disc_rim_last* l1 = &last[1];
    SU32(f.entry, -4, ebp);                                   /* push ebp */
    SU32(S, 0, ebx);                                          /* push ebx */
    SF32(S, 0x0c, cz); SF32(S, 0x10, cx); SF32(S, 0x14, nz); SF32(S, 0x24, nx); SF32(S, 0x2c, cy); SF32(S, 0x44, ny);
    SF32(S, 0x90, q1); SF32(S, 0x94, q2); SF32(S, 0xac, q1);
    SF32(S, 0xa0, ux); SF32(S, 0xa4, uz); SF32(S, 0xa8, uy);
    SF32(S, 0x48, phys_add(f_abs(l0->F2), f_abs(l0->F1)));   /* side 0: |F2| + |F1| */
    /* side 1's Newton (its last iteration; F2 stays in a register) */
    SF32(S, 0x18, ry_[1]); SF32(S, 0x4c, rx_[1]);
    SF32(S, 0x78, l1->g[1]); SF32(S, 0x8c, l1->g[2]); SF32(S, 0x5c, l1->g[3]); SF32(S, 0x6c, l1->g[4]);
    SF32(S, 0x70, l1->g[5]); SF32(S, 0x64, l1->g[6]); SF32(S, 0x84, l1->g[8]); SF32(S, 0x7c, l1->g[9]);
    SF32(S, 0x88, l1->xnz); SF32(S, 0x80, l1->znx); SF32(S, 0x54, l1->zcx);
    SF32(S, 0x28, l1->dXdZ2); SF32(S, 0x34, l1->F1); SF32(S, 0x68, l1->dZ); SF32(S, 0x74, l1->dY);
    /* the caps */
    SF32(S, 0x1c, s0); SF32(S, 0x20, D[1]); SF32(S, 0x30, rc2); SF32(S, 0x38, f_abs(D[1])); SF32(S, 0x3c, Q);
    SF32(S, 0x40, hy1); SF32(S, 0x50, m1nx); SF32(S, 0x58, Q); SF32(S, 0x98, z1_last);
    /* the last callee's stack: SqrtF's argument |D|, its return address, its push ebp */
    SF32(S, -4, absD[1]); SU32(S, -8, 0x0081d346); SU32(S, -0xc, f.ebp);
    world_frame_leave(&f);
}
RX_FUNCTION(0x0081c590, rx_world_disc_cylinder)
