/* phys_pair3.c -- box / cylinder contact generators (phys_contact.c dispatches; contact set M: count +0, 0x30-byte records
 * (point +0x10, normal +0x20, depth +0x30) from +0x10, at most 30, touching flag +0x6a0). */
#include "phys_pair3.h"
#include "phys_coll_shape.h"
#include "phys_coll_shape2.h"
#include "phys_cylinder.h"
#include "phys_crt.h"
#include "world_node.h"
#ifdef RX_ORACLE
#include <stdlib.h>   /* getenv: the RX_PRODUCT_W_LANES oracle diagnostic */
#endif

/* a direct call of an original stdcall kernel that is a core port now: its arguments and the 0 return address go on the stack as
 * the oracle's call into the original left them, the callee entered with ESP at the return address */
#define KERNEL_CALL(n, args, stmt) do { const rx_addr sp_ = rx_sp(); \
    for (int i_ = 0; i_ < (n); i_++) SU32(sp_ - 4u * (n) + 4u * (uint32_t)i_, 0, (args)[i_]); \
    SU32(sp_ - 4u * (n) - 4u, 0, 0); rx_set_sp(sp_ - 4u * (n) - 4u); stmt; rx_set_sp(sp_); } while (0)

#define CALL_AT_SP(cur, nstack, stmt) do { rx_set_sp((cur) - 4u * (nstack) - 4u); stmt; rx_set_sp(cur); } while (0)

static int all3(rx_v4 m) { return v4_lb(m, 1) + v4_lb(m, 2) + v4_lb(m, 0) == 0xfffffffdu; }
static int inside3(rx_v4 v, rx_v4 ext) { return all3(v4_cmple(v, ext)) && all3(v4_cmple(v4_sub(v4_zero(), ext), v)); }
static const int32_t k_edges[12][2] = {{0, 4}, {1, 5}, {3, 7}, {2, 6}, {0, 1}, {4, 5}, {6, 7}, {2, 3}, {0, 2}, {1, 3}, {5, 7}, {4, 6}};

/* a box's frame as the exe stores it: rows R0..R2 of its rotation (w lanes 0) and the position row (p, 1), in the scalar
 * products of q = (x, y, z, w) with K = 2 (0x92e330) */
static void box_frame(rx_addr q, rx_addr p, rx_addr out) {
    const float x = F32(q, 0), y = F32(q, 4), z = F32(q, 8), w = F32(q, 0xc);
    const float x2 = phys_mul(x, 2.0f), y2 = phys_mul(y, 2.0f), z2 = phys_mul(z, 2.0f);
    const float xx = phys_mul(x2, x), yy = phys_mul(y2, y), zz = phys_mul(z2, z);
    const float wx = phys_mul(x2, w), wy = phys_mul(y2, w), wz = phys_mul(z2, w);
    const float xy = phys_mul(x2, y), yz = phys_mul(y2, z), xz = phys_mul(x2, z);
    v4_st(out, v4(phys_sub(1.0f, phys_add(zz, yy)), phys_add(xy, wz), phys_sub(xz, wy), 0.0f));
    v4_st(out + 0x10, v4(phys_sub(xy, wz), phys_sub(1.0f, phys_add(zz, xx)), phys_add(yz, wx), 0.0f));
    v4_st(out + 0x20, v4(phys_add(xz, wy), phys_sub(yz, wx), phys_sub(1.0f, phys_add(yy, xx)), 0.0f));
    v4_st(out + 0x30, v4(F32(p, 0), F32(p, 4), F32(p, 8), 1.0f));
}
/* local point c to world: ((c.y R1 + c.x R0) + c.z R2) + P */
static rx_v4 frame_to_world(rx_addr f, rx_v4 c) {
    const rx_v4 xy = v4_add(v4_mul(v4_splat(c.v[1]), v4_ld(f + 0x10)), v4_mul(v4_splat(c.v[0]), v4_ld(f)));
    return v4_add(v4_add(xy, v4_mul(v4_splat(c.v[2]), v4_ld(f + 0x20))), v4_ld(f + 0x30));
}
/* world point to the frame's local coordinates: each row's dot with (w - P) in double precision ((r.y d.y + r.x d.x) + r.z d.z) */
static float row_dot_d(rx_v4 r, rx_v4 d) {
    return (float)(((double)r.v[1] * (double)d.v[1] + (double)r.v[0] * (double)d.v[0]) + (double)r.v[2] * (double)d.v[2]);
}
static rx_v4 world_to_frame(rx_addr f, rx_v4 w) {
    const rx_v4 d = v4_sub(w, v4_ld(f + 0x30));
    const float lx = row_dot_d(v4_ld(f), d), ly = row_dot_d(v4_ld(f + 0x10), d), lz = row_dot_d(v4_ld(f + 0x20), d);
    return v4(lx, ly, lz, lz);
}

/* one edge pass: each edge of the box whose corners (in the other box's frame) are at `corners` against the other box's slabs
 * [-ext, ext]: the six crossings p0 + t d (t in [0, 1], the other two coordinates inside) are accumulated into *sum / *count. The
 * direction's components get a small nudge where >= 0 (pass 1: + 1e-5f; pass 2: through a double + 1e-05, `nudge_double`).
 * The crossings are written into the six slots at `slots` (x, y, z; lane 3 keeps what was there). */
static void edge_pass(rx_addr corners, rx_v4 ext, int nudge_double, rx_addr slots, rx_v4* sum, int32_t* count, rx_addr tmp) {
    const float nx = phys_sub(-0.0f, ext.v[0]), ny = phys_sub(-0.0f, ext.v[1]), nz = phys_sub(-0.0f, ext.v[2]);
    for (int e = 0; e < 12; e++) {
        const rx_addr a = corners + 0x10u * (uint32_t)k_edges[e][0], b = corners + 0x10u * (uint32_t)k_edges[e][1];
        const float x0 = F32(a, 0), y0 = F32(a, 4), z0 = F32(a, 8);
        const float dx = phys_sub(F32(b, 0), x0), dy = phys_sub(F32(b, 4), y0), dz = phys_sub(F32(b, 8), z0);
        float d[3] = {dx, dy, dz};
        for (int i = 0; i < 3; i++)
            if (d[i] >= 0.0f) d[i] = nudge_double ? (float)((double)d[i] + 1e-05 /* 0x9777c8 */) : phys_add(d[i], 9.99999975e-06f /* 0x9774a4 */);
        const float t[6] = {
            phys_div(phys_sub(ext.v[0], x0), d[0]), phys_div(phys_sub(nx, x0), d[0]),
            phys_div(phys_sub(ext.v[1], y0), d[1]), phys_div(phys_sub(ny, y0), d[1]),
            phys_div(phys_sub(ext.v[2], z0), d[2]), phys_div(phys_sub(nz, z0), d[2]),
        };
        for (int k = 0; k < 6; k++) {
            const float px = phys_add(phys_mul(dx, t[k]), x0), py = phys_add(phys_mul(dy, t[k]), y0), pz = phys_add(phys_mul(dz, t[k]), z0);
            const rx_addr s = slots + 0x10u * (uint32_t)k;
            SF32(s, 0, px); SF32(s, 4, py); SF32(s, 8, pz);
            if (tmp == 0) continue;
            /* the second pass's spills in the frame (dead afterwards, but later generators at this depth copy dead lanes) */
            static const uint16_t kx[6] = {0, 0, 0x190, 0x1d0, 0x180, 0}, ky[6] = {0x154, 0x144, 0x194, 0x1d4, 0x184, 0};
            static const uint16_t kz[6] = {0x158, 0x148, 0x198, 0x1d8, 0, 0};
            if (kx[k]) SF32(tmp, kx[k], px);
            if (ky[k]) SF32(tmp, ky[k], py);
            if (kz[k]) SF32(tmp, kz[k], pz);
            if (k == 0) { SF32(tmp, 0x164, dy); SF32(tmp, 0x168, dz); }
            if (k == 3) { SF32(tmp, 0x170, phys_mul(t[k], dx)); SF32(tmp, 0x174, phys_mul(dy, t[k])); SF32(tmp, 0x178, phys_mul(dz, t[k])); }
            if (k == 4) { SF32(tmp, 0x1a0, phys_mul(t[k], dx)); SF32(tmp, 0x1a4, phys_mul(dy, t[k])); SF32(tmp, 0x1a8, phys_mul(dz, t[k])); }
            if (k == 5) { SF32(tmp, 0x130, phys_mul(t[k], dx)); SF32(tmp, 0x134, phys_mul(dy, t[k])); }
        }
        for (int k = 0; k < 6; k++) {
            const rx_addr s = slots + 0x10u * (uint32_t)k;
            const float px = F32(s, 0), py = F32(s, 4), pz = F32(s, 8);
            const int axis = k >> 1;
            int ok = t[k] >= 0.0f && 1.0f >= t[k];
            if (ok && axis != 0) ok = ext.v[0] >= px && px >= nx;
            if (ok && axis != 1) ok = ext.v[1] >= py && py >= ny;
            if (ok && axis != 2) ok = ext.v[2] >= pz && pz >= nz;
            if (!ok) continue;
            *sum = v4_add(*sum, v4_ld(s));
            (*count)++;
        }
    }
}

/* the contact normal direction in a box's frame from a point's coordinates relative to the extents: per axis
 * r ((3 r^2 - 2 r^2 r^2) r^2 r^2 r^2 ... as the exe's polynomial), normalized (zero below 1e-4); *depth2 = the point's smallest
 * squared distance to a face */
static rx_v4 box_face_weight(rx_v4 local, rx_v4 ext, float* depth2) {
    const rx_v4 r = v4_div(local, ext);
    float dd[3];
    for (int i = 0; i < 3; i++) {
        const float g = r.v[i] > 0.0f ? phys_sub(ext.v[i], local.v[i]) : (i == 0 ? phys_add(ext.v[i], local.v[i]) : phys_add(local.v[i], ext.v[i]));
        dd[i] = phys_mul(g, g);
    }
    float m = dd[1] > dd[0] ? dd[0] : dd[1];
    m = dd[2] > m ? m : dd[2];
    *depth2 = m;
    float w[3];
    for (int i = 0; i < 3; i++) {
        const float s = phys_mul(r.v[i], r.v[i]);
        const float p = phys_sub(phys_mul(s, 3.0f /* 0x92fe48 */), phys_mul(phys_mul(s, 2.0f), s));
        w[i] = phys_mul(phys_mul(phys_mul(phys_mul(p, s), s), s), r.v[i]);
    }
    const float len = phys_sqrt(phys_add(phys_add(phys_mul(w[1], w[1]), phys_mul(w[0], w[0])), phys_mul(w[2], w[2])));
    if (9.99999975e-05f /* 0x9775a4 */ > len) return v4(0.0f, 0.0f, 0.0f, 0.0f);
    const float n0 = phys_div(w[0], len), n1 = phys_div(w[1], len), n2 = phys_div(w[2], len);
    return v4(n0, n1, n2, n2);
}

/* ---- 008657d0 CollisionPair_BoxVsBox (ECX = box B, EDX = B's rotation*; stack: M, box A, A's rotation*, A's position*, B's
 * position*, swap flag; RET 0x18): one contact from the corners of each box inside the other (B's extents widened by 0.001)
 * and the edges of each crossing the other's faces, all averaged: the point is the count-weighted mean of the two means (each
 * back in world space), the normal the half sum of A's face direction and minus B's (world space, normalized, the up vector
 * below 0.001; negated with the flag), the depth the sum of the two smallest face distances. Frame: push ebp; and esp, -16;
 * sub esp, 0x3d4; push ebx / esi / edi (A's frame +0x60, B's +0x200, B's corners / the crossing slots +0x240, A's corners +0x2d0,
 * A's corners in B's frame +0x350). */
void phys_pair_box_box(rx_addr B, rx_addr QB, rx_addr M, rx_addr A, rx_addr QA, rx_addr PA, rx_addr PB, uint32_t flag, uint32_t* edx) {
    *edx = 0x0092df24;                                                  /* past the edge table (no contact) */
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x3d4 - 12;
    rx_set_sp(L);
    const rx_addr fa = L + 0x60, fb = L + 0x200, cb = L + 0x240, ca = L + 0x2d0, ca_in_b = L + 0x350;
    box_frame(QB, PB, fb);
    box_frame(QA, PA, fa);
    const rx_v4 ext_b = v4_add(v4_splat(0.001f /* 0x97743c */), v4_ld(B + 0x20)), ext_a = v4_ld(A + 0x20);
    SU32(L, 0x12c, M);
    v4_st(L + 0x1b0, ext_b); v4_st(L + 0x1c0, ext_a);
    const rx_v4 zero = v4_ld(0x00d61670);
    v4_st(L + 0x2c0, zero);
    CALL_AT_SP(L, 1, phys_box_corners(B, cb));
    CALL_AT_SP(L, 1, phys_box_corners(A, ca));
    for (int i = 0; i < 8; i++) {
        const rx_addr c = ca + 0x10u * (uint32_t)i;
        v4_st(ca_in_b + 0x10u * (uint32_t)i, world_to_frame(fb, frame_to_world(fa, v4_ld(c))));
    }
    for (int i = 0; i < 8; i++) {
        const rx_addr c = cb + 0x10u * (uint32_t)i;
        const rx_v4 lc = world_to_frame(fa, frame_to_world(fb, v4_ld(c)));
        v4_st(ca + 0x10u * (uint32_t)i, lc);   /* B's corners in A's frame */
        v4_st(L + 0x190, v4_splat(lc.v[0])); v4_st(L + 0x170, v4_splat(lc.v[1])); v4_st(L + 0x1a0, v4_splat(lc.v[2]));   /* spills */
    }
    int32_t count_a = 0, count_b = 0;
    rx_v4 sum_a = zero, sum_b = zero;
    for (int i = 0; i < 8; i++) {
        const rx_v4 v = v4_ld(ca_in_b + 0x10u * (uint32_t)i);
        if (inside3(v, ext_b)) { count_a++; sum_a = v4_add(sum_a, v); }
    }
    for (int i = 0; i < 8; i++) {
        const rx_v4 v = v4_ld(ca + 0x10u * (uint32_t)i);
        if (inside3(v, ext_a)) { sum_b = v4_add(sum_b, v); count_b++; }
    }
    edge_pass(ca_in_b, ext_b, 0, cb, &sum_a, &count_a, 0);
    edge_pass(ca, ext_a, 1, cb, &sum_b, &count_b, L);
    v4_st(L + 0x1e0, sum_a); v4_st(L + 0x1f0, sum_b);
    rx_v4 acc = v4_ld(L + 0x2c0);
    if (count_a > 0) {
        const rx_v4 n = v4_splat((float)count_a), mean = v4_div(sum_a, n);
        v4_st(L + 0x130, mean); v4_st(L + 0x160, mean);
        acc = v4_add(v4_mul(frame_to_world(fb, mean), n), v4_ld(L + 0x2c0));
    }
    if (count_b > 0) {
        const rx_v4 n = v4_splat((float)count_b), mean = v4_div(sum_b, n);
        v4_st(L + 0x130, mean); v4_st(L + 0x160, mean);
        acc = v4_add(v4_mul(frame_to_world(fa, mean), n), acc);
    }
    const int32_t total = count_a + count_b;
    if (total <= 0) { rx_set_sp(sp0); return; }
    *edx = M;
    const rx_v4 point = v4_div(acc, v4_splat((float)total));
    const rx_v4 lb = world_to_frame(fb, point), la = world_to_frame(fa, point);
    float d2b, d2a;
    const rx_v4 nb = box_face_weight(lb, ext_b, &d2b), na = box_face_weight(la, ext_a, &d2a);
    v4_st(L + 0x2c0, point);
    /* the frame words the original leaves (later generators at this depth copy dead lanes from here, the cylinder ones at
     * +0x17c..+0x1dc): the local points, the scaled point, and the splats of both face weights */
    v4_st(L + 0x1e0, lb); v4_st(L + 0x1f0, la);
    v4_st(L + 0x180, v4_div(la, ext_a));
    v4_st(L + 0x150, v4_splat(nb.v[0])); v4_st(L + 0x140, v4_splat(nb.v[1])); v4_st(L + 0x170, v4_splat(nb.v[2]));
    v4_st(L + 0x1a0, v4_splat(na.v[0])); v4_st(L + 0x190, v4_splat(na.v[1])); v4_st(L + 0x1d0, v4_splat(na.v[2]));
    v4_st(L + 0x160, v4_splat(0.5f));
    const rx_v4 wb = v4_add(v4_add(v4_mul(v4_ld(fb), v4_splat(nb.v[0])), v4_mul(v4_ld(fb + 0x10), v4_splat(nb.v[1]))),
                            v4_mul(v4_ld(fb + 0x20), v4_splat(nb.v[2])));
    const rx_v4 wa = v4_add(v4_add(v4_mul(v4_ld(fa), v4_splat(na.v[0])), v4_mul(v4_ld(fa + 0x10), v4_splat(na.v[1]))),
                            v4_mul(v4_ld(fa + 0x20), v4_splat(na.v[2])));
    rx_v4 n = v4_mul(v4_add(v4_sub(v4_zero(), wb), wa), v4_splat(0.5f /* 0x9322cc */));
    const rx_v4 n2 = v4_mul(n, n);
    const float len = phys_sqrt(phys_add(phys_add(n2.v[0], n2.v[2]), n2.v[1]));
    n = 0.001f > len ? v4_ld(0x00d616a0) : v4_div(n, v4_splat(len));
    if ((flag & 0xff) != 0) n = v4_sub(v4_zero(), n);
    v4_st(L + 0x130, n);
    const float depth = phys_add(phys_sqrt(d2a), phys_sqrt(d2b));
    int32_t c = I32(M, 0);
    if (c >= 0x1d) c = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)c;
    v4_st(rec + 0x10, point);
    v4_st(rec + 0x20, n);
    v4_st(rec + 0x30, v4_splat(depth));
    SU32(M, 0, (uint32_t)c + 1);
    SU32(M, 0x6a0, 1);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x008657d0, phys_pair_box_box)

/* ---- 00867ca0 CollisionPair_BoxVsCylinder ---------------------------------------------------------------------------------------
 * ECX = the cylinder's rotation*, EDX = the cylinder (radius +0x20, height +0x24); stack: M, the box (extents +0x20), the box's
 * rotation*, the box's position*, the cylinder's position*, swap flag; RET 0x18. One contact from two averages:
 *   1. in the cylinder's frame: the box's edges against the cylinder (0081d480 per edge) and the box corners inside it;
 *   2. in the box's frame: the cylinder's axis segment against the box (0081ecb0 from each end, 0081b6b0 for the faces).
 * The point is the count-weighted mean of the two means (in world space); the normal the half sum of minus the box's face normal and
 * the cylinder's surface normal at the point (one of them alone when only that average has samples; normalized), negated unless the
 * flag is set; the depth the sum of the point's distances to the box (00845b50) and to the cylinder's surface (00846f10).
 * The flag byte is inverted in place (sete [ebp+0x1c]). Frame: push ebp; and esp, -16; sub esp, 0x1e4; push ebx / esi / edi:
 *   +0x20 count 1 (float), +0x24 count 1 > 0, +0x28 -h, +0x2c count 2, +0x34 r, +0x38 h (half height), +0x40 sum 2 (later the box's
 *   closest point and normal), +0x60 axis end 0 in the box's frame, +0x70 sum 1 (later the point), +0x80 axis direction, +0x94 the
 *   cylinder*, +0x98 dist 1, +0x9c the box* (later dist 2), +0xa0 the box's extents (later the callees' normals), +0xb0 the box's
 *   frame, then its corners in the cylinder's frame, +0x130 axis end 0 again, +0x154 M, +0x15c r^2, +0x160 axis end 1, +0x170 the
 *   box's frame (copy), +0x1b0 the cylinder's frame.
 * The 0081xxxx kernels run as the original with the registers the original has at each call (they save EBX / ESI / EDI / EAX / ECX
 * in their frames). Returns ECX. */

/* a direction in a frame's rotation to world: ((n.y R1 + n.x R0) + n.z R2) */
static rx_v4 frame_rotate(rx_addr f, rx_v4 n) {
    const rx_v4 xy = v4_add(v4_mul(v4_splat(n.v[1]), v4_ld(f + 0x10)), v4_mul(v4_splat(n.v[0]), v4_ld(f)));
    return v4_add(xy, v4_mul(v4_splat(n.v[2]), v4_ld(f + 0x20)));
}
/* a world point in a frame's local coordinates in single precision: each row's (r.x d.x + r.y d.y) + r.z d.z with d = w - P */
static float row_dot_f(rx_v4 r, rx_v4 d) { return phys_add(phys_add(phys_mul(r.v[0], d.v[0]), phys_mul(r.v[1], d.v[1])), phys_mul(r.v[2], d.v[2])); }
static rx_v4 to_local(rx_addr f, rx_v4 w) {
    const rx_v4 d = v4_sub(w, v4_ld(f + 0x30));
    const float x = row_dot_f(v4_ld(f), d), y = row_dot_f(v4_ld(f + 0x10), d), z = row_dot_f(v4_ld(f + 0x20), d);
    return v4(x, y, z, z);
}
static void st3(rx_addr p, rx_v4 v) { SF32(p, 0, v.v[0]); SF32(p, 4, v.v[1]); SF32(p, 8, v.v[2]); }
/* SqrtF 0042c230 called with ESP = esp (push ecx; store the argument; call): its argument, the return address and SqrtF's push ebp
 * land on the dead stack as in the original (w-parity5: a later CylinderVsCylinder 0081c590 of the same frame reads [esp - 4] as
 * its Newton step's start: the paired NAT_Bedford ATV ride's AI 3 / 6 at event frame 6620 found a wheel contact the original did
 * not with the stale word under it) */
static float sqrt_at(rx_addr esp, float x, rx_addr ret, rx_addr ebp) {
    SU32(esp - 4, 0, rx_fb(x)); SU32(esp - 8, 0, ret); SU32(esp - 12, 0, ebp);
    return phys_sqrtf(x);
}
static float dist3(rx_addr a, rx_addr b, rx_addr esp, rx_addr ret, rx_addr ebp) {
    const float dx = phys_sub(F32(a, 0), F32(b, 0)), dy = phys_sub(F32(a, 4), F32(b, 4)), dz = phys_sub(F32(a, 8), F32(b, 8));
    return sqrt_at(esp, phys_add(phys_add(phys_mul(dx, dx), phys_mul(dy, dy)), phys_mul(dz, dz)), ret, ebp);
}

uint32_t phys_pair_box_cylinder(rx_addr cyl, rx_addr qcyl, rx_addr M, rx_addr box, rx_addr qbox, rx_addr pbox, rx_addr pcyl,
                                uint32_t flag, const rx_regs* caller, uint32_t* edx) {
    enum { EAX, ECX, EDX, EBX, ESP_, EBP, ESI, EDI };
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x1e4 - 12;
    rx_set_sp(L);
    const rx_addr fbox = L + 0x170, fcyl = L + 0x1b0, corners = L + 0xb0;
    const rx_addr sum1 = L + 0x70, cnt1 = L + 0x20, sum2 = L + 0x40, cnt2 = L + 0x2c, dir = L + 0x80, ext = L + 0xa0;
    const rx_addr end0 = L + 0x130, end1 = L + 0x160;
    const float neg0 = rx_bf(0x80000000u) /* 0x9774a8 */;
    const int inverted = (flag & 0xff) == 0;
    SU8(sp0, 0x18, inverted);                                           /* sete byte [ebp+0x1c] */

    /* the frames (the cylinder's built at +0x170 and moved to +0x1b0; the box's at +0xb0, copied to +0x170) */
    box_frame(qcyl, pcyl, L + 0x170);
    for (uint32_t o = 0; o < 0x40; o += 0x10) v4_st(fcyl + o, v4_ld(L + 0x170 + o));
    SU32(L, 0x154, M); SU32(L, 0x9c, box); SU32(L, 0x94, cyl);
    const float r = F32(cyl, 0x20), h = phys_mul(F32(cyl, 0x24), 0.5f /* 0x9322cc */), nh = phys_sub(neg0, h), r2 = phys_mul(r, r);
    SF32(L, 0x34, r); SF32(L, 0x38, h); SF32(L, 0x28, nh); SF32(L, 0x15c, r2);
    v4_st(ext, v4_ld(box + 0x20));
    box_frame(qbox, pbox, corners);
    for (uint32_t o = 0; o < 0x40; o += 0x10) v4_st(fbox + o, v4_ld(corners + o));
    const rx_v4 g = v4(F32(0, 0xa95d50), F32(0, 0xa95d54), F32(0, 0xa95d58), 0.0f);   /* writable zero vector */
    st3(sum1, g); st3(sum2, g);
    SF32(cnt1, 0, 0.0f); SF32(cnt2, 0, 0.0f); SU32(L, 0x24, 0); SU32(L, 0x98, 0);

    /* the axis ends (0, -h, 0) and (0, h, 0) of the cylinder in the box's frame, and the unit direction between them (each
     * component nudged by 1e-7 where >= 0) */
    const rx_v4 p0 = to_local(fbox, frame_to_world(fcyl, v4(0.0f, nh, 0.0f, 0.0f)));
    st3(L + 0x60, p0); st3(end0, p0);
    const rx_v4 p1 = to_local(fbox, frame_to_world(fcyl, v4(0.0f, h, 0.0f, 0.0f)));
    st3(end1, p1);
    float d[3] = {phys_sub(p1.v[0], p0.v[0]), phys_sub(p1.v[1], p0.v[1]), phys_sub(p1.v[2], p0.v[2])};
    const float len2 = phys_add(phys_add(phys_mul(d[1], d[1]), phys_mul(d[0], d[0])), phys_mul(d[2], d[2]));
    SF32(L, 0x5c, len2);
    if (len2 > 0.0f) {
        const float inv = phys_div(1.0f, sqrt_at(L, len2, 0x008684ee, sp0 - 4));
        for (int i = 0; i < 3; i++) d[i] = phys_mul(inv, d[i]);
    }
    for (int i = 0; i < 3; i++) {
        if (d[i] >= 0.0f) d[i] = phys_add(d[i], 1.00000001e-07f /* 0x977324 */);
        SF32(dir, 4u * (uint32_t)i, d[i]);
    }

    /* the box's corners in the cylinder's frame (x, y, z replaced in place) */
    CALL_AT_SP(L, 1, phys_box_corners(box, corners));
    for (int i = 0; i < 8; i++) {
        const rx_addr c = corners + 0x10u * (uint32_t)i;
        st3(c, to_local(fcyl, frame_to_world(fbox, v4_ld(c))));
    }

    /* average 1: each box edge against the cylinder (0081d480: ECX / EAX the edge's corners, ESI the sum, EDI the count; stdcall
     * h, r, r), then the corners strictly inside (x^2 + z^2 < r^2, -h < y < h), each weighted by its 0 / 1 */
    rx_regs in = caller ? *caller : phys_regs(), out = phys_regs();
    in.r[ECX] = qcyl; in.r[EDX] = cyl; in.r[EBX] = box; in.r[ESI] = pcyl; in.r[EDI] = pbox; in.r[EAX] = qbox; in.r[EBP] = sp0 - 4;
    for (int e = 0; e < 12; e++) {
        const uint32_t args[3] = {rx_fb(h), rx_fb(r), rx_fb(r)};
        in.r[EBX] = 0x0092dec0u + 8u * (uint32_t)e;                     /* the edge table (same pairs as k_edges) */
        in.r[EAX] = corners + 0x10u * (uint32_t)k_edges[e][1];
        in.r[ECX] = corners + 0x10u * (uint32_t)k_edges[e][0];
        in.r[EDI] = cnt1; in.r[ESI] = sum1;
        KERNEL_CALL(3, args, rx_world_segment_cone(in.r[ECX], in.r[EAX], in.r[ESI], in.r[EDI], h, r, r));   /* 0081d480 */
    }
    float sx = F32(sum1, 0), sy = F32(sum1, 4), sz = F32(sum1, 8), n1 = F32(cnt1, 0);
    uint32_t inside = 0;
    for (int i = 0; i < 8; i++) {
        const rx_addr c = corners + 0x10u * (uint32_t)i;
        const float x = F32(c, 0), y = F32(c, 4), z = F32(c, 8);
        inside = r2 > phys_add(phys_mul(x, x), phys_mul(z, z)) && h > y && y > nh;
        const float f = (float)(int32_t)inside;
        sx = phys_add(phys_mul(x, f), sx); sy = phys_add(phys_mul(y, f), sy); sz = phys_add(phys_mul(f, z), sz);
        n1 = phys_add(f, n1);
    }
    SF32(sum1, 0, sx); SF32(sum1, 4, sy); SF32(sum1, 8, sz); SF32(cnt1, 0, n1);
    const int has1 = n1 > 0.0f;
    if (has1) SU32(L, 0x24, 1);

    /* average 2: the axis segment against the box (0081ecb0 from each end: EAX the extents; stdcall direction, end, r, sum, count;
     * then 0081b6b0: ECX the extents, ESI end 0, EAX the sum, EDI the count; stdcall end 1) */
    in.r[EAX] = ext; in.r[ECX] = end0; in.r[EDX] = dir; in.r[EBX] = 0x0092df20;   /* EBX: past the edge table */
    {
        const uint32_t args[5] = {dir, end0, rx_fb(r), sum2, cnt2};
        KERNEL_CALL(5, args, rx_world_line_box_edges(in.r[EAX], dir, end0, r, sum2, cnt2));   /* 0081ecb0 */
    }
    in.r[EAX] = ext; in.r[ECX] = sum2; in.r[EDX] = end1;
    {
        const uint32_t args[5] = {dir, end1, rx_fb(r), sum2, cnt2};
        KERNEL_CALL(5, args, rx_world_line_box_edges(in.r[EAX], dir, end1, r, sum2, cnt2));   /* 0081ecb0 */
    }
    const int has2 = F32(cnt2, 0) > 0.0f;                               /* EBX: 1, else the 0 at +0x98 */
    in.r[EBX] = has2 ? 1u : U32(L, 0x98);
    in.r[EDI] = cnt2; in.r[EAX] = sum2; in.r[ECX] = ext; in.r[ESI] = end0;
    {
        const uint32_t args[1] = {end1};
        uint32_t ecx5;
        KERNEL_CALL(1, args, rx_world_segment_box_faces_ecx(in.r[ECX], in.r[ESI], in.r[EAX], in.r[EDI], end1, in.r[EBP], &ecx5));   /* 0081b6b0 */
        out = in; out.r[ECX] = ecx5; out.r[EDX] = end1;           /* what it leaves: ECX = face 5's flag, EDX = its argument */
    }

    /* the point: (mean 1 in world) * count 1 + (mean 2 in world) * count 2, over the total */
    rx_v4 acc = v4(F32(0, 0xa95d50), F32(0, 0xa95d54), F32(0, 0xa95d58), 0.0f);
    const float c1 = F32(cnt1, 0);
    if (c1 > 0.0f) {
        const rx_v4 m = v4(phys_div(F32(sum1, 0), c1), phys_div(F32(sum1, 4), c1), phys_div(F32(sum1, 8), c1), 0.0f);
        SF32(L, 0xa0, m.v[0]); SF32(L, 0xa8, m.v[2]);
        acc = v4_add(v4_mul(frame_to_world(fcyl, m), v4_splat(c1)), acc);
    }
    const float c2 = F32(cnt2, 0);
    if (c2 > 0.0f) {
        const rx_v4 m = v4(phys_div(F32(sum2, 0), c2), phys_div(F32(sum2, 4), c2), phys_div(F32(sum2, 8), c2), 0.0f);
        SF32(L, 0x130, m.v[0]); SF32(L, 0x138, m.v[2]);
        acc = v4_add(v4_mul(frame_to_world(fbox, m), v4_splat(c2)), acc);
    }
    const float total = phys_add(c2, c1);
    *edx = out.r[EDX];
    if (!(total > 0.0f)) { rx_set_sp(sp0); return out.r[ECX]; }
    *edx = M;
    const rx_v4 point = v4_div(acc, v4_splat(total));
    st3(sum1, point);

    /* distance and face normal on the box (00845b50: EDI the box, ESI the point (clamped in place); stack the normal) */
    const rx_v4 lb = to_local(fbox, point);
    st3(L + 0x60, lb); st3(sum2, lb);
    /* the call's frame words the ports do not write: its argument (EDX = the normal out), the return address, its push ebp and,
     * inside it, its push ebx (EBX: the face count flag loaded for 0081b6b0) -- dead stack a later Newton step reads (w-parity5) */
    SU32(L - 4, 0, ext); SU32(L - 8, 0, 0x00869886); SU32(L - 12, 0, sp0 - 4); SU32(L - 0x30, 0, in.r[EBX]);
    CALL_AT_SP(L, 1, phys_box_clamp_point(box, sum2, ext));
    const float dist1 = dist3(L + 0x60, sum2, L, 0x008698d2, sp0 - 4);
    SF32(L, 0x98, dist1);
    const rx_v4 nbox = frame_rotate(fbox, v4_ld(ext));
    st3(sum2, nbox);

    /* distance and surface normal on the cylinder (00846f10: ESI the cylinder, EDI the point (moved in place); stack the normal) */
    const rx_v4 lc = to_local(fcyl, point);
    st3(L + 0x60, lc); st3(dir, lc);
    /* as above for 00846f10 (its push ebx lands at its frame base L - 0xa0: the word a CylinderVsCylinder 0081c590 of the same
     * composite pair reads as its Newton step's start when its frame lies there: the paired NAT_Bedford ATV ride's AI 3 / 6 at event
     * frame 6620 found a wheel contact the original did not, with the -0.125 0081b6b0 left there in the product) */
    SU32(L - 4, 0, ext); SU32(L - 8, 0, 0x00869a7b); SU32(L - 12, 0, sp0 - 4); SU32(L - 0xa0, 0, in.r[EBX]);
    CALL_AT_SP(L, 1, phys_cylinder_closest_point_normal(cyl, dir, ext));
    const float dist2 = dist3(L + 0x60, dir, L, 0x00869ad0, sp0 - 4);
    SF32(L, 0x9c, dist2);
    const rx_v4 ncyl = frame_rotate(fcyl, v4_ld(ext));

    float nx, ny, nz;
    if (!has1) {
        nx = phys_sub(neg0, nbox.v[0]); ny = phys_sub(neg0, nbox.v[1]); nz = phys_sub(neg0, nbox.v[2]);
    } else if (!has2) {
        nx = ncyl.v[0]; ny = ncyl.v[1]; nz = ncyl.v[2];
    } else {
        nx = phys_mul(phys_add(phys_sub(neg0, nbox.v[0]), ncyl.v[0]), 0.5f);
        ny = phys_mul(phys_add(phys_sub(neg0, nbox.v[1]), ncyl.v[1]), 0.5f);
        nz = phys_mul(phys_add(phys_sub(neg0, nbox.v[2]), ncyl.v[2]), 0.5f);
        SF32(L, 0x60, nx); SF32(L, 0x64, ny); SF32(L, 0x68, nz);
        const float n2 = phys_add(phys_add(phys_mul(ny, ny), phys_mul(nx, nx)), phys_mul(nz, nz));
        SF32(L, 0x94, n2);
        if (n2 > 0.0f) {
            float inv = 0.0f;
            inv = phys_div(1.0f, sqrt_at(L, n2, 0x00869c21, sp0 - 4));
            nx = phys_mul(nx, inv); ny = phys_mul(ny, inv); nz = phys_mul(nz, inv);
        }
    }
    if (inverted) { nx = phys_sub(neg0, nx); ny = phys_sub(neg0, ny); nz = phys_sub(neg0, nz); }
    const float depth = phys_add(dist2, dist1);
    int32_t c = I32(M, 0);
    if (c >= 0x1d) c = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)c;
    v4_st(rec + 0x10, v4(point.v[0], point.v[1], point.v[2], point.v[2]));
    v4_st(rec + 0x20, v4(nx, ny, nz, nz));
    v4_st(rec + 0x30, v4_splat(depth));
    SU32(M, 0, (uint32_t)c + 1);
    SU32(M, 0x6a0, 1);
    rx_set_sp(sp0);
    return (uint32_t)c + 1;
}
RX_FUNCTION(0x00867ca0, phys_pair_box_cylinder)

/* ---- 0086bd80 CollisionPair_CylinderVsCylinder ----------------------------------------------------------------------------------
 * ECX = B's rotation*, EDX = B's position*; stack: M, cylinder A, A's rotation*, A's position*, cylinder B, swap flag; RET 0x18.
 * Two averages, each over samples of one cylinder's axis segment against the other cylinder in that cylinder's frame:
 *   1. A's axis in B's frame (sum +0x20, count +0x10): the point of A's axis line nearest B's axis pushed in by A's radius
 *      (when it lies within the segment, B's height and B's radius), the segment's midpoint when inside B, the 0081c590 cap
 *      crossings from each end (six flagged points each) and the 0081beb0 side crossings (two);
 *   2. B's axis in A's frame (sum +0x30, count +0x18): the same without the nearest-point sample.
 * The point is the count-weighted mean of both means in world space; the normal the half sum of A's surface normal and minus
 * B's at the point (00846f10 on each; normalized), negated with the flag; the depth the distance between the two surface points.
 * The original's record w lanes are the frame's +0x9c (never written) and the +0x6c 00846f10 leaves; the product initialises
 * them to 0 at the record copy (see the note there; oracle build unchanged).
 * Frame: push ebp; and esp, -16; sub esp, 0x1f4; push ebx / esi / edi: +0x4c rA, +0x54 rB, +0x5c hA, +0x8c hB (half heights),
 * +0x60 / +0x130 / +0x140 the segment ends, +0x90 the segment direction (later the point), +0xa0 the midpoint (later A's
 * normal), +0xb8 the kernels' six flags, +0xd0 their six points, +0x15c B*, +0x17c M, +0x180 A's frame, +0x1c0 B's frame.
 * Returns ECX. */

/* the flagged points of a 0081c590 / 0081beb0 call (flags +0xb8, points +0xd0, 0x10 apart) added to a sum / count */
static void add_flagged(rx_addr L, int n, float* s, float* count) {
    for (int k = 0; k < n; k++) {
        if (U32(L, 0xb8 + 4u * (uint32_t)k) == 0) continue;
        const rx_addr p = L + 0xd0 + 0x10u * (uint32_t)k;
        s[0] = phys_add(F32(p, 0), s[0]); s[1] = phys_add(F32(p, 4), s[1]); s[2] = phys_add(F32(p, 8), s[2]);
        *count = phys_add(*count, 1.0f);
    }
}
/* the kernels of one segment (ends +0x60 and +0x140, direction +0x90) against a cylinder (radius r, half height h) in its frame:
 * 0081c590 (the caps) from each end (EAX the end, ECX the direction, ESI the points, EDI the flags; stdcall 0xd6b400, the segment's
 * radius rs, h, r; RET 0x10), then 0081beb0 (the side: EAX / EDI the ends, ECX the points, EDX the flags; stdcall h, r, points 0
 * and 1, flags 0 and 1; RET 0x18) */
static void segment_kernels(rx_addr L, rx_regs* in, float rs, float h, float r, float* s, float* count) {
    enum { EAX, ECX, EDX, EBX, ESP_, EBP, ESI, EDI };
    const rx_addr ends[2] = {L + 0x140, L + 0x60};
    for (int i = 0; i < 2; i++) {
        const uint32_t args[4] = {0x00d6b400u, rx_fb(rs), rx_fb(h), rx_fb(r)};
        in->r[EDI] = L + 0xb8; in->r[ESI] = L + 0xd0; in->r[EAX] = ends[i]; in->r[ECX] = L + 0x90;
        KERNEL_CALL(4, args, rx_world_disc_cylinder(in->r[ECX], in->r[EAX], in->r[ESI], in->r[EDI], 0x00d6b400u, rs, h, r, in->r[EBX], in->r[EBP]));   /* 0081c590 */
        add_flagged(L, 6, s, count);
    }
    const uint32_t args[6] = {rx_fb(h), rx_fb(r), L + 0xd0, L + 0xe0, L + 0xb8, L + 0xbc};
    in->r[ECX] = L + 0xd0; in->r[EDX] = L + 0xb8; in->r[EAX] = L + 0x140; in->r[EDI] = L + 0x60;
    KERNEL_CALL(6, args, rx_world_segment_cylinder(in->r[EDI], in->r[EAX], h, r, L + 0xd0, L + 0xe0, L + 0xb8, L + 0xbc,
                                                   in->r[EBP], in->r[EBX], in->r[ESI]));   /* 0081beb0 */
    in->r[EAX] = L + 0xbc; in->r[ECX] = U32(L, 0xbc); in->r[EDX] = U32(L, 0xb8);   /* what it leaves: flag 1's address and the two flags */
    add_flagged(L, 2, s, count);
}
static void st_dir(rx_addr L, float dx, float dy, float dz, float len2, int keep_d) {
    if (keep_d) { SF32(L, 0x90, dx); SF32(L, 0x94, dy); SF32(L, 0x98, dz); }
    SF32(L, 0x1c, len2);
    if (len2 > 0.0f) {
        float inv = 0.0f;
        CALL_AT_SP(L, 1, inv = phys_div(1.0f, phys_sqrtf(len2)));
        SF32(L, 0x90, phys_mul(inv, dx)); SF32(L, 0x94, phys_mul(dy, inv)); SF32(L, 0x98, phys_mul(inv, dz));
    }
}

uint32_t phys_pair_cylinder_cylinder(rx_addr qb, rx_addr pb, rx_addr M, rx_addr A, rx_addr qa, rx_addr pa, rx_addr B, uint32_t flag,
                                     const rx_regs* caller, uint32_t* edx) {
    enum { EAX, ECX, EDX, EBX, ESP_, EBP, ESI, EDI };
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x1f4 - 12;
    rx_set_sp(L);
    const rx_addr fa = L + 0x180, fb = L + 0x1c0;
    const float neg0 = rx_bf(0x80000000u) /* 0x9774a8 */, tiny = 9.99999975e-05f /* 0x9775a4 */;
    box_frame(qb, pb, L + 0xd0);
    for (uint32_t o = 0; o < 0x40; o += 0x10) v4_st(fb + o, v4_ld(L + 0xd0 + o));
    SU32(L, 0x17c, M); SU32(L, 0x15c, B);
    box_frame(qa, pa, L + 0xd0);
    for (uint32_t o = 0; o < 0x40; o += 0x10) v4_st(fa + o, v4_ld(L + 0xd0 + o));
    const float rA = F32(A, 0x20), hA = phys_mul(F32(A, 0x24), 0.5f), rB = F32(B, 0x20), hB = phys_mul(F32(B, 0x24), 0.5f);
    SF32(L, 0x4c, rA); SF32(L, 0x54, rB); SF32(L, 0x5c, hA); SF32(L, 0x8c, hB);
    float s1[3] = {0.0f, 0.0f, 0.0f}, s2[3] = {0.0f, 0.0f, 0.0f}, c1 = 0.0f, c2 = 0.0f;
    rx_regs in = caller ? *caller : phys_regs();
    in.fpu_in = 0; in.r[EBX] = A; in.r[EBP] = sp0 - 4; in.r[ESI] = pa; in.r[EDI] = B; in.r[EAX] = qa;

    /* 1. A's axis ends in B's frame */
    const rx_v4 e0 = to_local(fb, frame_to_world(fa, v4(0.0f, phys_sub(neg0, hA), 0.0f, 0.0f)));
    const rx_v4 e1 = to_local(fb, frame_to_world(fa, v4(0.0f, hA, 0.0f, 0.0f)));
    st3(L + 0x130, e1); st3(L + 0x140, e1); st3(L + 0xa0, e0); st3(L + 0x60, e0);
    float dx = phys_sub(e1.v[0], e0.v[0]), dz = phys_sub(e1.v[2], e0.v[2]);
    const float dy = phys_sub(e1.v[1], e0.v[1]);
    SF32(L, 0x164, dy);
    if (dx == 0.0f) dx = tiny;
    if (dz == 0.0f) dz = tiny;
    SF32(L, 0x70, dx); SF32(L, 0x78, dz);
    {   /* the point of A's axis line nearest B's axis (in the xz plane) */
        const float dx2 = phys_mul(dx, dx), dz2 = phys_mul(dz, dz);
        const float t = phys_sub(neg0, phys_div(phys_add(phys_mul(e0.v[2], dz), phys_mul(e0.v[0], dx)), phys_add(dx2, dz2)));
        const float px = phys_add(phys_mul(t, dx), e0.v[0]), pz = phys_add(phys_mul(t, dz), e0.v[2]);
        const float py = phys_add(phys_mul(dy, t), e0.v[1]);
        SF32(L, 0x1c, t); SF32(L, 0x90, px); SF32(L, 0x94, py); SF32(L, 0x98, pz);
        float s = 0.0f;
        CALL_AT_SP(L, 1, s = phys_sqrtf(phys_add(phys_mul(pz, pz), phys_mul(px, px))));
        const float q = phys_sub(s, rA);
        if (s == 0.0f) s = tiny;
        const float k = phys_div(q, s);
        if (1.0f > t && t > 0.0f && py > phys_sub(neg0, hB) && hB > py && rB > q) {
            s1[0] = phys_add(phys_mul(px, k), 0.0f); s1[1] = phys_add(py, 0.0f); s1[2] = phys_add(phys_mul(pz, k), 0.0f);
            c1 = phys_add(0.0f, 1.0f);
        }
        const rx_v4 mid = v4_mul(v4_add(v4_ld(L + 0x130), v4_ld(L + 0xa0)), v4_splat(0.5f));
        st3(L + 0xa0, mid);
        uint32_t inside = 0;
        CALL_AT_SP(L, 1, inside = phys_cylinder_contains_point(L + 0xa0, B));
        if (inside) {
            s1[1] = phys_add(mid.v[1], s1[1]); s1[0] = phys_add(mid.v[0], s1[0]); s1[2] = phys_add(mid.v[2], s1[2]);
            c1 = phys_add(c1, 1.0f);
        }
        SF32(L, 0x90, F32(0, 0xa95d50)); SF32(L, 0x94, F32(0, 0xa95d54)); SF32(L, 0x98, F32(0, 0xa95d58));
        SF32(L, 0x14, dx2); SF32(L, 0x58, dz2);
        st_dir(L, dx, dy, dz, phys_add(phys_add(phys_mul(dy, dy), dx2), dz2), 0);
    }
    in.r[EDX] = pb;
    segment_kernels(L, &in, rA, hB, rB, s1, &c1);

    /* 2. B's axis ends in A's frame */
    const rx_v4 f0 = to_local(fa, frame_to_world(fb, v4(0.0f, phys_sub(neg0, hB), 0.0f, 0.0f)));
    const rx_v4 f1 = to_local(fa, frame_to_world(fb, v4(0.0f, hB, 0.0f, 0.0f)));
    st3(L + 0x130, f0); st3(L + 0x60, f0); st3(L + 0x140, f1); SF32(L, 0x168, f1.v[2]);
    {
        const rx_v4 mid = v4_mul(v4_add(f1, f0), v4_splat(0.5f));
        st3(L + 0x70, mid); st3(L + 0xa0, mid);
        uint32_t inside = 0;
        CALL_AT_SP(L, 1, inside = phys_cylinder_contains_point(L + 0xa0, A));
        if (inside) {
            for (int i = 0; i < 3; i++) s2[i] = phys_add(mid.v[i], 0.0f);
            c2 = phys_add(0.0f, 1.0f);
        }
        const float ex = phys_sub(f1.v[0], f0.v[0]), ey = phys_sub(f1.v[1], f0.v[1]), ez = phys_sub(f1.v[2], f0.v[2]);
        SF32(L, 0x70, ex); SF32(L, 0x74, ey); SF32(L, 0x78, ez);
        st_dir(L, ex, ey, ez, phys_add(phys_add(phys_mul(ey, ey), phys_mul(ex, ex)), phys_mul(ez, ez)), 1);
    }
    segment_kernels(L, &in, rB, hA, rA, s2, &c2);
    st3(L + 0x20, v4(s1[0], s1[1], s1[2], 0.0f)); SF32(L, 0x10, c1);
    st3(L + 0x30, v4(s2[0], s2[1], s2[2], 0.0f)); SF32(L, 0x18, c2);

    /* the point: (mean 1 in world) * count 1 + (mean 2 in world) * count 2, over the total */
    rx_v4 acc = v4(F32(0, 0xa95d50), F32(0, 0xa95d54), F32(0, 0xa95d58), 0.0f);
    if (c1 > 0.0f) {
        const rx_v4 m = v4(phys_div(s1[0], c1), phys_div(s1[1], c1), phys_div(s1[2], c1), 0.0f);
        SF32(L, 0x168, m.v[2]);
        acc = v4_add(v4_mul(frame_to_world(fb, m), v4_splat(c1)), acc);
    }
    if (c2 > 0.0f) {
        const rx_v4 m = v4(phys_div(s2[0], c2), phys_div(s2[1], c2), phys_div(s2[2], c2), 0.0f);
        SF32(L, 0x168, m.v[2]);
        acc = v4_add(v4_mul(frame_to_world(fa, m), v4_splat(c2)), acc);
    }
    const float total = phys_add(c2, c1);
    *edx = in.r[EDX];
    if (!(total > 0.0f)) { rx_set_sp(sp0); return in.r[ECX]; }
    *edx = M;
    const rx_v4 point = v4_div(acc, v4_splat(total));
    st3(L + 0x90, point);

    /* the surface points and normals of both cylinders nearest the point (00846f10: ESI the cylinder, EDI the point in / out; stack
     * the normal) */
    const rx_v4 la = to_local(fa, point), lb = to_local(fb, point);
    SF32(L, 0x160, la.v[0]); st3(L + 0x60, la); st3(L + 0x140, lb);
    CALL_AT_SP(L, 1, phys_cylinder_closest_point_normal(B, L + 0x140, L + 0x70));
    CALL_AT_SP(L, 1, phys_cylinder_closest_point_normal(A, L + 0x60, L + 0xa0));
    const rx_v4 wa = frame_to_world(fa, v4_ld(L + 0x60)), wb = frame_to_world(fb, v4_ld(L + 0x140));
    SF32(L, 0x38, wa.v[2]); SF32(L, 0x168, wb.v[2]);
    const rx_v4 na = frame_rotate(fa, v4_ld(L + 0xa0)), nb = frame_rotate(fb, v4_ld(L + 0x70));
    st3(L + 0x60, na); st3(L + 0x130, nb);
    const float ddx = phys_sub(wb.v[0], wa.v[0]), ddy = phys_sub(wb.v[1], wa.v[1]), ddz = phys_sub(wb.v[2], wa.v[2]);
    float depth = 0.0f;
    CALL_AT_SP(L, 1, depth = phys_sqrtf(phys_add(phys_add(phys_mul(ddy, ddy), phys_mul(ddx, ddx)), phys_mul(ddz, ddz))));
    SF32(L, 0x1c, depth);
    float n[3];
    for (int i = 0; i < 3; i++) n[i] = phys_mul(phys_add(phys_sub(neg0, nb.v[i]), na.v[i]), 0.5f);
    SF32(L, 0x70, n[0]); SF32(L, 0x74, n[1]); SF32(L, 0x78, n[2]);
    st3(L + 0x60, v4(n[0], n[1], n[2], 0.0f));
    const float n2 = phys_add(phys_add(phys_mul(n[1], n[1]), phys_mul(n[0], n[0])), phys_mul(n[2], n[2]));
    SF32(L, 0x15c, n2);
    if (n2 > 0.0f) {
        float inv = 0.0f;
        CALL_AT_SP(L, 1, inv = phys_div(1.0f, phys_sqrtf(n2)));
        for (int i = 0; i < 3; i++) n[i] = phys_mul(n[i], inv);
        st3(L + 0x60, v4(n[0], n[1], n[2], 0.0f));
    }
    if ((flag & 0xff) != 0) {
        for (int i = 0; i < 3; i++) n[i] = phys_sub(neg0, n[i]);
        st3(L + 0x60, v4(n[0], n[1], n[2], 0.0f));
    }
#ifndef RX_ORACLE
    /* The original writes only x, y, z of the point (+0x90) and the normal (+0x60) and copies the whole vec4s into the record,
     * so the record's w lanes are the frame's +0x9c (never written) and +0x6c (what 00846f10 leaves) - uninitialised stack
     * words. The engine reads them in 4-lane response arithmetic, and this port's residue held 0xffffffff (a NaN) there: the
     * body-probe correction then made the bike's pose.w NaN at race 396 of the steer-from-205 ride. The product initialises
     * the two dead lanes to zero; the oracle build keeps the original's residue so per-call Verify of the record bytes stays
     * exact (live x,y,z,depth are unaffected either way). See core/notes/phys.md "dead w lanes". */
    SU32(L, 0x9c, 0); SU32(L, 0x6c, 0);
#else
    if (getenv("RX_PRODUCT_W_LANES")) { SU32(L, 0x9c, 0); SU32(L, 0x6c, 0); }   /* oracle diagnostic: the product's zeroed lanes */
#endif
    int32_t c = I32(M, 0);
    if (c >= 0x1d) c = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)c;
    v4_st(rec + 0x10, v4_ld(L + 0x90));
    v4_st(rec + 0x20, v4_ld(L + 0x60));
    v4_st(rec + 0x30, v4_splat(depth));
    SU32(M, 0, (uint32_t)c + 1);
    SU32(M, 0x6a0, 1);
    rx_set_sp(sp0);
    return rec;
}
RX_FUNCTION(0x0086bd80, phys_pair_cylinder_cylinder)
