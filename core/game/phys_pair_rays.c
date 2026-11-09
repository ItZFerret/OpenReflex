/* phys_pair_rays.c -- the contact generators of the dispatcher's ray-list shape (type 0x20: count +0x20, 0x20-byte entries at
 * +0x24, each the two vec4 end points of a segment in the list's frame; CollisionShapeStaticRays / DynamicRays, e.g. the body
 * probe rays of the vehicles at S+0x20d0) against a sphere (00864460) and the height-field terrain (00872c90). phys_contact.c
 * dispatches both orientations (the swapped case passes the inverted swap flag).
 *
 * Contact set M: count +0, records of 0x30 bytes from M + 0x10 (point +0x10, normal +0x20, depth vec4 +0x30), at most 30 (the
 * index is clamped to 29), touching flag +0x6a0.
 *
 * Both are SSE (00864460 has double-precision row dots, cvtps2pd / mulsd / addsd / cvtpd2ps). The frames are written as the
 * originals write them: the next generator at the same depth reads some of these words as uninitialised stack (core/notes/
 * phys.md "A generator's spills feed the next generator"). */
#include "phys_pair_rays.h"
#include "phys_coll_quat.h"
#include "phys_terrain.h"
#include "world_veh.h"
#ifdef RX_ORACLE
#include <stdlib.h>   /* getenv: the RX_PRODUCT_W_LANES oracle diagnostic */
#endif

/* ---- shared helpers ------------------------------------------------------------------------------------------------------------ */

/* the w-cleared mask 0x0092db80 */
static rx_v4 xyz_mask(rx_v4 v) { return v4_and(rx_ld4(0x0092db80), v); }

/* |x| as Float_Abs (00401000) leaves it in xmm0 lane 0 */
static float float_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu /* 0x00977570 */); }

/* double arithmetic with the x86 NaN rule (the first NaN operand, quieted; the real indefinite for a new NaN) */
static double nan_rule_d(double r, double a, double b) {
    if (r == r) return r;
    if (a != a) return rx_bd(rx_db(a) | 0x0008000000000000ull);
    if (b != b) return rx_bd(rx_db(b) | 0x0008000000000000ull);
    return rx_bd(0xfff8000000000000ull);
}
static double add_d(double a, double b) { return nan_rule_d(a + b, a, b); }
static double mul_d(double a, double b) { volatile double vb = b; return nan_rule_d(a * vb, a, b); }

/* a row's dot with d in double precision: (r.y * d.y + r.x * d.x) + r.z * d.z, rounded to float (cvtpd2ps) */
static float row_dot_double(rx_v4 r, rx_v4 d) {
    const double yy = mul_d((double)r.v[1], (double)d.v[1]);
    const double xx = mul_d((double)r.v[0], (double)d.v[0]);
    const double zz = mul_d((double)r.v[2], (double)d.v[2]);
    return (float)add_d(add_d(yy, xx), zz);
}

/* (x + y) + z of the squared lanes (shufps 0x99 / 0x55) and (x + z) + y (shufps 0x66 / 0x55) */
static float sum_xyz(rx_v4 sq) { return phys_add(phys_add(sq.v[0], sq.v[1]), sq.v[2]); }
static float sum_xzy(rx_v4 sq) { return phys_add(phys_add(sq.v[0], sq.v[2]), sq.v[1]); }

/* the normalized vector with the collision code's guards: the default 0x0092dc30 where the square sum is +inf (0x0092dc20)
 * and 0 where the length is 0 (lanes compared against the frame's zero vector) */
static rx_v4 guarded_normal(rx_v4 v, rx_v4 zero) {
    const float s = sum_xyz(v4_mul(v, v));
    const rx_v4 len = v4_sqrt(v4_splat(s));
    const rx_v4 inf = v4_ld(0x0092dc20);
    const rx_v4 finite = v4_mask(s != inf.v[0], s != inf.v[1], s != inf.v[2], s != inf.v[3]);
    const rx_v4 nonzero = v4_mask(zero.v[0] != len.v[0], zero.v[1] != len.v[1], zero.v[2] != len.v[2], zero.v[3] != len.v[3]);
    return v4_or(v4_and(v4_and(nonzero, v4_div(v, len)), finite), v4_andn(finite, v4_ld(0x0092dc30)));
}

/* ---- 00864460 sphere against ray list ---------------------------------------------------------------------------------------
 * The list's frame: rotation rows R0..R2 from its quaternion (scalar products with K = 2, 0x0092e330) and its position P (w 1).
 * The sphere's center into that frame: c = (R0 . d, R1 . d, R2 . d, R2 . d) with d = ps - P (double-precision row dots). Per
 * segment (p0, p1): the two parameters where p0 + t (p1 - p0) meets the sphere (radius r = the radius vec4's x):
 * A2 = 2 a, B2 = 2 (c - p0) . dir, disc = B2 * B2 - (|c - p0|^2 - r^2) * 2 A2; t0 = (sqrt|disc| + B2) / A2,
 * t1 = (B2 - sqrt|disc|) / A2, both 2.0 (0x009774f8) without a root (disc < 0 or NaN). A contact at t0 when 0 <= t0 <= 1,
 * t1 > t0 and t1 > 0; one at t1 when 0 <= t1 <= 1, t0 >= t1 and t0 > 0. Each: point = the hit in world space, normal = the
 * guarded unit (hit - c) in world space (negated with the flag), depth = |(K - t) * dir| on all lanes (K = the runtime vector
 * 0x00d61650).
 * Frame: push ebp; and esp, -16; sub esp, 0x128; push ebx / edi: +0x14 A2, +0x18 B2, +0x1c t0 / scratch, +0x20 the loop count,
 * +0x24 disc / scratch, +0x28 r (then r^2), +0x2c t1, +0x30 / +0x40 / +0x50 R0 / R1 / R2, +0x60 P, +0x70 p0 (zeroed per
 * contact), +0x80 d / scratch, +0x90 the row copy / scratch, +0xa0 the hit / the normal, +0xb0 / +0xc0 / +0xd0 the row-dot
 * splats (later c - p0, dir, normal splats), +0xe0 .. +0x110 normal splats and the hit, +0x120 c. */
uint32_t phys_pair_sphere_rays(rx_addr prays, rx_addr sphere, rx_addr M, rx_addr psphere, rx_addr rays, rx_addr qrays,
                               uint32_t flag, uint32_t* edx) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0x128u - 8u;
    rx_set_sp(L);
    uint32_t ecx = prays;

    /* the list's rotation rows (the original's scalar product order) */
    const float qx = F32(qrays, 0), qy = F32(qrays, 4), qz = F32(qrays, 8), qw = F32(qrays, 0xc);
    const float two = 2.0f;                                   /* 0x0092e330 */
    const float z2 = phys_mul(qz, two);
    SU32(L, 0x28, U32(sphere, 0x20));                         /* the radius vec4's x */
    const float x2 = phys_mul(qx, two);
    const float y2 = phys_mul(qy, two);
    const float zz = phys_mul(qz, z2);
    SF32(L, 0x20, z2);
    const float wx = phys_mul(qw, x2);
    const float xx = phys_mul(qx, x2);
    SF32(L, 0x24, wx);
    const float wz = phys_mul(qw, z2);
    const float wy = phys_mul(qw, y2);
    SF32(L, 0x1c, xx);
    const float yy = phys_mul(qy, y2);
    const float xy = phys_mul(qy, x2);
    SF32(L, 0x14, wz);
    const float yz = phys_mul(qz, y2);
    SF32(L, 0x18, wy);
    const float xz = phys_mul(qz, x2);
    const float one = 1.0f;                                   /* 0x00931f24 */
    const float r00 = phys_sub(one, phys_add(zz, yy));
    const float r01 = phys_add(xy, wz);
    const float r10 = phys_sub(xy, wz);
    SF32(L, 0x34, r01);
    const float r02 = phys_sub(xz, wy);
    const float r20 = phys_add(xz, wy);
    SF32(L, 0x38, r02);
    SF32(L, 0x40, r10);
    const float r11 = phys_sub(one, phys_add(zz, xx));
    SF32(L, 0x44, r11);
    const float r21 = phys_sub(yz, wx);
    SF32(L, 0x54, r21);
    const float r22 = phys_sub(one, phys_add(yy, xx));
    SF32(L, 0x58, r22);
    SU32(L, 0x60, U32(prays, 0));
    SF32(L, 0x30, r00);
    SU32(L, 0x64, U32(prays, 4));
    const float r12 = phys_add(yz, wx);
    SF32(L, 0x6c, one);
    SF32(L, 0x3c, 0.0f);
    SF32(L, 0x48, r12);
    SF32(L, 0x4c, 0.0f);
    SF32(L, 0x50, r20);
    SF32(L, 0x5c, 0.0f);
    SU32(L, 0x68, U32(prays, 8));

    /* the sphere's center in the list's frame */
    const rx_v4 R0 = v4_ld(L + 0x30), R1 = v4_ld(L + 0x40), R2 = v4_ld(L + 0x50), P = v4_ld(L + 0x60);
    const rx_v4 d = v4_sub(v4_ld(psphere), P);
    v4_st(L + 0x80, d);
    v4_st(L + 0x90, R0);
    const float cx = row_dot_double(R0, d);
    v4_st(L + 0xd0, v4_splat(cx));
    v4_st(L + 0x90, R1);
    const float cy = row_dot_double(R1, d);
    v4_st(L + 0xc0, v4_splat(cy));
    v4_st(L + 0x90, R2);
    const float cz = row_dot_double(R2, d);
    v4_st(L + 0xb0, v4_splat(cz));
    const rx_v4 c = v4(cx, cy, cz, cz);                       /* shufps 0xa5 + movss */
    v4_st(L + 0x120, c);

    const int32_t count = I32(rays, 0x20);
    if (count <= 0) {
        *edx = sphere;
        rx_set_sp(sp0);
        return ecx;
    }
    const float r = F32(L, 0x28);
    SF32(L, 0x28, phys_mul(r, r));
    SU32(L, 0x20, (uint32_t)count);
    uint32_t offset = 0;
    do {
        const rx_addr entries = U32(rays, 0x24);
        ecx = entries;
        *edx = entries;
        const rx_v4 p0 = v4_ld(entries + offset);
        const rx_v4 dir = v4_sub(v4_ld(entries + offset + 0x10), p0);
        v4_st(L + 0xc0, dir);
        const rx_v4 w = v4_sub(c, p0);
        v4_st(L + 0xb0, w);
        v4_st(L + 0x70, p0);

        /* the quadratic */
        const float dx = dir.v[0], dy = dir.v[1], dz = dir.v[2];
        const float a = phys_add(phys_add(phys_mul(dx, dx), phys_mul(dy, dy)), phys_mul(dz, dz));
        const float a2 = phys_mul(a, two);
        const float b = phys_add(phys_add(phys_mul(w.v[0], dx), phys_mul(w.v[1], dy)), phys_mul(w.v[2], dz));
        const float b2 = phys_mul(b, two);
        const float ww = phys_add(phys_add(phys_mul(w.v[0], w.v[0]), phys_mul(w.v[1], w.v[1])), phys_mul(w.v[2], w.v[2]));
        const float cc = phys_sub(ww, F32(L, 0x28));
        SF32(L, 0x14, a2);
        const float a4 = phys_mul(a2, two);
        SF32(L, 0x18, b2);
        const float disc = phys_sub(phys_mul(b2, b2), phys_mul(cc, a4));
        SF32(L, 0x24, disc);
        float t0 = 2.0f, t1 = 2.0f;                           /* 0x009774f8 (double 2.0) */
        if (disc >= 0.0f) {
            /* Float_Abs, then SqrtF on the pushed |disc| (the two calls' return addresses and saved EBP below the frame) */
            const float root = phys_sqrt(float_abs(disc));
            SF32(L, -4, float_abs(disc));
            SU32(L, -8, 0x0086491du);
            SU32(L, -12, ebp);
            t0 = phys_div(phys_add(root, b2), a2);
        }
        SF32(L, 0x1c, t0);
        if (disc >= 0.0f) {
            const float root = phys_sqrt(float_abs(disc));
            SF32(L, -4, float_abs(disc));
            SU32(L, -8, 0x0086496fu);
            SU32(L, -12, ebp);
            t1 = phys_div(phys_sub(b2, root), a2);
        }
        SF32(L, 0x2c, t1);

        for (int pass = 0; pass < 2; pass++) {
            const float t = pass ? t1 : t0, other = pass ? t0 : t1;
            /* t in [0, 1]; the other root beyond it (strictly for t0) and above 0 */
            if (!(t >= 0.0f) || !(1.0f >= t)) continue;
            if (pass == 0 ? !(other > t) : !(other >= t)) continue;
            if (!(other > 0.0f)) continue;
            if (pass == 1) ecx = U32(rays, 0x24);
            const rx_v4 start = pass ? v4_ld(U32(rays, 0x24) + offset) : v4_ld(L + 0x70);
            const rx_v4 hit = v4_add(v4_mul(v4_splat(t), dir), start);
            const rx_v4 zero = v4_zero();
            const rx_v4 n = guarded_normal(v4_sub(hit, v4_ld(L + 0x120)), zero);
            v4_st(L + 0x70, zero);
            rx_v4 point;
            if (pass == 0) {
                v4_st(L + 0x110, hit);
                v4_st(L + 0xa0, hit);
                const rx_v4 xy_sum = v4_add(v4_mul(v4_splat(hit.v[1]), R1), v4_mul(v4_splat(hit.v[0]), R0));
                v4_st(L + 0xa0, n);
                v4_st(L + 0xd0, v4_splat(n.v[0]));
                v4_st(L + 0x100, v4_splat(n.v[1]));
                point = v4_add(v4_add(v4_mul(v4_splat(hit.v[2]), R2), xy_sum), P);
                v4_st(L + 0xf0, v4_splat(n.v[2]));
            } else {
                v4_st(L + 0xa0, hit);
                v4_st(L + 0x110, hit);
                v4_st(L + 0xa0, n);
                const rx_v4 xy_sum = v4_add(v4_mul(v4_splat(hit.v[0]), R0), v4_mul(v4_splat(hit.v[1]), R1));
                v4_st(L + 0xe0, v4_splat(n.v[0]));
                v4_st(L + 0x80, v4_splat(n.v[1]));
                point = v4_add(v4_add(v4_mul(v4_splat(hit.v[2]), R2), xy_sum), P);
                v4_st(L + 0x90, v4_splat(n.v[2]));
            }
            rx_v4 normal = v4_add(v4_add(v4_mul(v4_splat(n.v[0]), R0), v4_mul(R1, v4_splat(n.v[1]))),
                                  v4_mul(R2, v4_splat(n.v[2])));
            if ((flag & 0xffu) != 0) normal = v4_sub(v4_ld(L + 0x70), normal);
            int32_t k = I32(M, 0);
            if (k >= 0x1d) k = 0x1d;
            const rx_addr record = M + 0x30u * (uint32_t)k;
            v4_st(record + 0x10, point);
            v4_st(record + 0x20, normal);
            const rx_v4 along = v4_mul(v4_sub(v4_ld(0x00d61650), v4_splat(t)), dir);
            v4_st(record + 0x30, v4_sqrt(v4_splat(sum_xzy(v4_mul(along, along)))));
            SU32(M, 0, (uint32_t)k + 1u);
            SU32(M, 0x6a0, 1);
            ecx = (uint32_t)k + 1u;
            if (pass == 1) *edx = 0x30u * (uint32_t)k;
        }
        offset += 0x20;
        SU32(L, 0x20, U32(L, 0x20) - 1u);
    } while (U32(L, 0x20) != 0);
    rx_set_sp(sp0);
    return ecx;
}
RX_FUNCTION(0x00864460, phys_pair_sphere_rays)

/* ---- 00872c90 ray list against the height-field terrain -------------------------------------------------------------------------
 * Per segment: both ends into world space (q * (p & xyz) * conj(q) + position), then TerrainSegmentQuery (007c6cc0) on the
 * terrain of the shape's registry (00402dc0's scene-identity search, inlined) from the first end to the second. A hit gives one
 * contact: point = the hit, normal = the terrain normal (negated per lane, -0.0 - n, when the inverted flag is set), depth =
 * |(end - hit) . normal| ((x + y) + z) on all lanes. The query writes only x, y, z of the hit / normal and the records copy the
 * whole vec4s (see the product note at the loop).
 * Frame: push ebp; and esp, -16; sub esp, 0x94; push ebx / esi / edi: +0x14 the entry offset, +0x18 M, +0x1c the loop count,
 * +0x20 the normal, +0x30 the terrain shape, +0x34 the list, +0x38 its position*, +0x3c its rotation*, +0x40 the hit, +0x50 /
 * +0x60 the two ends in world space, +0x70 / +0x80 / +0x90 product spills (later the position). */
uint32_t phys_pair_rays_hfterrain(rx_addr rays, rx_addr prays, rx_addr M, rx_addr qrays, rx_addr terrain_shape, uint32_t flag,
                                  uint32_t* edx) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0x94u - 12u;
    rx_set_sp(L);
    uint32_t ecx = rays;
    *edx = prays;

    SU32(L, 0x3c, qrays);
    SU32(L, 0x30, terrain_shape);
    SU8(sp0, 0x10, (flag & 0xffu) == 0);                     /* sete byte [ebp + 0x14]: the flag inverted in its slot */
    SU32(L, 0x18, M);
    SU32(L, 0x34, rays);
    SU32(L, 0x38, prays);
    const int32_t count = I32(rays, 0x20);
    if (count > 0) {
        SU32(L, 0x14, 0);
        SU32(L, 0x1c, (uint32_t)count);
        do {
            const rx_addr entries = U32(rays, 0x24);
            const uint32_t offset = U32(L, 0x14);
            const rx_v4 q = v4_ld(U32(L, 0x3c));
            const rx_v4 conj = v4_mul(q, COLL_QCONJ);

            /* the first end: t1 = (p0 & xyz) * conj(q), then q * t1 (its r.zwxy spilled at +0x80) */
            const rx_v4 t1 = coll_quat_mul(xyz_mask(v4_ld(entries + offset)), conj);
            v4_st(L + 0x80, v4_perm(t1, 2, 3, 0, 1));
            /* the second end (conj(q) spilled at +0x70) */
            v4_st(L + 0x70, conj);
            const rx_v4 t2 = coll_quat_mul(xyz_mask(v4_ld(entries + offset + 0x10)), conj);
            v4_st(L + 0x90, v4_perm(t2, 2, 3, 0, 1));
            const rx_v4 position = v4_ld(U32(L, 0x38));
            v4_st(L + 0x70, position);
            v4_st(L + 0x80, v4_mul(v4_lane(q, 1), v4_perm(t1, 2, 3, 0, 1)));
            const rx_v4 start = v4_add(coll_quat_mul(q, t1), position);
            v4_st(L + 0x50, start);
            const rx_v4 end = v4_add(coll_quat_mul(q, t2), position);
            v4_st(L + 0x60, end);

            /* the terrain of the shape's registry whose identity is the scene's current one (00402dc0, inlined) */
            const rx_addr list = U32(U32(L, 0x30), 0x20);
            const uint32_t n = U32(list, 8);
            rx_addr terrain = 0;
            for (uint32_t i = 0; i < n; i++) {
                const rx_addr candidate = U32(list, 4u * i);
                if (candidate == 0) continue;
                const rx_addr scene = U32(0, 0x00a95844);
                if (U32(scene, 0x1c + 4u * U32(scene, 4)) == U32(candidate, 0x44a8)) { terrain = candidate; break; }
            }

            /* 007c6cc0 (ESI the terrain, ECX the start, EAX the end; stack the hit*, the normal*; RET 8; EDX = the hit*) */
            SU32(L, -4, L + 0x20);
            SU32(L, -8, L + 0x40);
            SU32(L, -12, 0x00872f24u);
            rx_set_sp(L - 12);
            uint32_t regs[2] = {L + 0x50, L + 0x40};
            const uint32_t found = phys_terrain_segment_query(terrain, L + 0x50, L + 0x60, L + 0x40, L + 0x20, regs);
            rx_set_sp(L);
            ecx = regs[0];
            *edx = regs[1];
            if (found != 0) {
                if (U8(sp0, 0x10) != 0) {
                    const float minus_zero = -0.0f;           /* 0x009774a8 */
                    const float nx = phys_sub(minus_zero, F32(L, 0x20));
                    const float ny = phys_sub(minus_zero, F32(L, 0x24));
                    const float nz = phys_sub(minus_zero, F32(L, 0x28));
                    SF32(L, 0x20, nx);
                    SF32(L, 0x24, ny);
                    SF32(L, 0x28, nz);
                }
                const float ex = phys_sub(F32(L, 0x60), F32(L, 0x40));
                const float ey = phys_sub(F32(L, 0x64), F32(L, 0x44));
                const float ez = phys_sub(F32(L, 0x68), F32(L, 0x48));
                const float dot = phys_add(phys_add(phys_mul(ex, F32(L, 0x20)), phys_mul(ey, F32(L, 0x24))),
                                           phys_mul(ez, F32(L, 0x28)));
                const float depth = float_abs(dot);
                SU32(L, -4, 0x00872fa7u);                     /* Float_Abs's return address and saved EBP */
                SU32(L, -8, ebp);
#ifndef RX_ORACLE
                /* Product only, as phys_pair_cylinder_hfterrain (core/notes/phys.md "Dead w lanes in the contact records"): the
                 * query writes x, y, z only and the original copies the whole vec4s, so the record's w lanes would be stale
                 * stack words that the 4-lane response math reads. The live lanes are unchanged. */
                SU32(L, 0x2c, 0);
                SU32(L, 0x4c, 0);
#else
                if (getenv("RX_PRODUCT_W_LANES")) { SU32(L, 0x2c, 0); SU32(L, 0x4c, 0); }   /* oracle diagnostic: the product's zeroed lanes */
#endif
                int32_t k = I32(M, 0);
                if (k >= 0x1d) k = 0x1d;
                const rx_addr record = M + 0x30u * (uint32_t)k;
                v4_st(record + 0x10, v4_ld(L + 0x40));
                v4_st(record + 0x20, v4_ld(L + 0x20));
                v4_st(record + 0x30, v4_splat(depth));
                SU32(M, 0, (uint32_t)k + 1u);
                SU32(M, 0x6a0, 1);
                ecx = (uint32_t)k + 1u;
            }
            SU32(L, 0x14, U32(L, 0x14) + 0x20u);
            SU32(L, 0x1c, U32(L, 0x1c) - 1u);
        } while (U32(L, 0x1c) != 0);
    }
    rx_set_sp(sp0);
    return ecx;
}
RX_FUNCTION(0x00872c90, phys_pair_rays_hfterrain)
