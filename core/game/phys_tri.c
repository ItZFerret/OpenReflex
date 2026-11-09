/* phys_tri.c -- ray and vertex queries against triangle lists (the contact generators' helpers, 0x0086xxxx). */
#include "phys_tri.h"

static rx_v4 v4_and3(rx_v4 a, rx_v4 b, rx_v4 c) { return v4_and(v4_and(a, b), c); }

/* 00860e90 (ECX = triangle count, EDX = hit flags out (4 x u32), ESI = hit points out (4 vec4); cdecl triangles* (3 vec4 each),
 * normals out* (4 vec4), rays* (4 pairs start / end, vec4 each), query box min*, max*): four rays (start S_k = rays[2k],
 * end E_k = rays[2k+1]) against every triangle whose box overlaps the query box, Moller-Trumbore in four lanes at once
 * (D = unit ray direction, e1 / e2 the edges from the triangle's first vertex A, T = S - A, P = D x e2, Q = T x e1,
 * det = e1 . P (front faces only: det >= 1e-6), u = (T . P) / det, v = (D . Q) / det, t = (e2 . Q) / det): a ray hits when
 * 0 <= u, u + v <= the runtime vec4 0x00d61690 (one), 0 <= v, 0 < t < the nearest so far and t < |E - S|. Per ray: the hit
 * flag, the point S + t (E - S) / |E - S| (guarded normalization) and the unit triangle normal (x, y, z, 1).
 * Returns 1 when any ray hit (EAX).
 *
 * Frame: push ebp; and esp, -16; sub esp, 0x2d8; push ebx / edi -> E = ((entry ESP - 4) & ~15) - 0x2e0 (16-aligned). The
 * original leaves its working set there and later code reads it uninitialized: CylinderVsCylinder 0086bd80's Newton step
 * (0081c590's [S + 0x60]) is this frame's +0x230 (Dz, the rays' unit z directions) when a BoxVsTriangles (00869d40) ran
 * before it at the same depth (core/notes/phys.md "The frame of 00860e90"). So the port leaves the original's frame image
 * (disassembly 00860e90..00861a56): every frame store's last value (r4_frame below, written once at the end), the cookie
 * (+0x2dc), the loop counter (+0x24, 0 after the loop), the normals argument (+0x2c), the record words of the Vec4_CmpLe_CR6
 * calls (0041c640: [out] = ([out] & 0xffffff0f) | 0x80 all lanes | 0x20 no lane; out = +0x3c det, +0x38 / +0x30 u,
 * +0x28 / +0x34 v), the last such call's own frame below E (its argument, return address, EBP, mask vector, EBX / ESI / EDI)
 * and the cookie check's return address (+0x04). The two caller registers it saves, EDI (+0x00) and EBP (entry ESP - 4), are
 * the caller's to write (phys_rays4_saved_regs). */
typedef struct {
    rx_v4 v[0x2d0 / 16];   /* E + 0x00 .. E + 0x2cf, one vec4 per 16 bytes */
    uint64_t written;      /* bit i: E + 0x10 i was stored */
    uint32_t cr_flags[5];  /* the CR6 record flags of each call site (cr_off) */
    unsigned cr_called;    /* bit k: call site k ran */
    int cr_last;           /* the last call site, its mask and EBX (the triangle pointer + 0x20) */
    rx_v4 cr_mask;
    rx_addr cr_ebx;
} r4_frame;
#define R4(off, val) do { fr.v[(off) >> 4] = (val); fr.written |= 1ull << ((off) >> 4); } while (0)
static const uint16_t r4_cr_off[5] = {0x3c, 0x38, 0x30, 0x28, 0x34};
static const uint32_t r4_cr_ret[5] = {0x00861310, 0x00861393, 0x008613b8, 0x00861488, 0x008614ad};

/* a CR6 call of site k: mask = b <= a (a = xmm0, b = xmm1), *none when no lane holds */
static rx_v4 r4_cr(r4_frame* fr, int k, rx_v4 a, rx_v4 b, rx_addr ebx, int* none) {
    const rx_v4 m = v4_cmple(b, a);
    const int all = v4_lb(m, 0) && v4_lb(m, 1) && v4_lb(m, 2) && v4_lb(m, 3);
    *none = !(v4_lb(m, 0) | v4_lb(m, 1) | v4_lb(m, 2) | v4_lb(m, 3));
    fr->cr_flags[k] = (all ? 0x80u : 0u) | (*none ? 0x20u : 0u);
    fr->cr_called |= 1u << k;
    fr->cr_last = k; fr->cr_mask = m; fr->cr_ebx = ebx;
    return m;
}
static rx_v4 r4_cmpneq(rx_v4 a, rx_v4 b) {
    return v4_mask(a.v[0] != b.v[0], a.v[1] != b.v[1], a.v[2] != b.v[2], a.v[3] != b.v[3]);
}

static void r4_flush(const r4_frame* fr, rx_addr sp0, rx_addr E, rx_addr hits, rx_addr points) {
    for (unsigned i = 0; i < 0x2d0 / 16; i++)
        if (fr->written & (1ull << i)) v4_st(E + 0x10u * i, fr->v[i]);
    for (int k = 0; k < 5; k++)
        if (fr->cr_called & (1u << k)) SU32(E, r4_cr_off[k], (U32(E, r4_cr_off[k]) & 0xffffff0fu) | fr->cr_flags[k]);
    if (fr->cr_called) {
        SU32(E - 4, 0, E + r4_cr_off[fr->cr_last]);
        SU32(E - 8, 0, r4_cr_ret[fr->cr_last]);
        SU32(E - 12, 0, sp0 - 4);                         /* 00860e90's EBP */
        v4_st(E - 0x20, fr->cr_mask);
        SU32(E - 0x28, 0, fr->cr_ebx);
        SU32(E - 0x2c, 0, points);
        SU32(E - 0x30, 0, hits);
    }
    SU32(E, 0x2dc, U32(0, 0x9b45e4) ^ (E + 8));           /* the cookie */
    SU32(E, 4, 0x00861a53);                               /* __security_check_cookie's return address (over the saved EBX) */
}

/* the caller's EDI and EBP as 00860e90 (entered at ESP sp0) leaves them in its frame */
void phys_rays4_saved_regs(rx_addr sp0, uint32_t edi, uint32_t ebp) {
    SU32(((sp0 - 4) & ~15u) - 0x2e0, 0, edi);
    SU32(sp0 - 4, 0, ebp);
}

int32_t phys_rays4_triangles(int32_t count, rx_addr hits, rx_addr points, rx_addr tris, rx_addr normals, rx_addr rays,
                             rx_addr qmin, rx_addr qmax) {
    const rx_addr sp0 = rx_sp(), E = ((sp0 - 4) & ~15u) - 0x2e0;
    r4_frame fr;
    fr.written = 0; fr.cr_called = 0; fr.cr_last = 0;
    rx_v4 S[4], E4[4];
    for (int k = 0; k < 4; k++) { S[k] = v4_ld(rays + 0x20u * (uint32_t)k); E4[k] = v4_ld(rays + 0x20u * (uint32_t)k + 0x10); }
    SU32(E, 0x2c, normals);
    /* the rays in structure-of-arrays form: lane k = ray k */
    const rx_v4 sx = v4(S[0].v[0], S[1].v[0], S[2].v[0], S[3].v[0]), sy = v4(S[0].v[1], S[1].v[1], S[2].v[1], S[3].v[1]);
    const rx_v4 sz = v4(S[0].v[2], S[1].v[2], S[2].v[2], S[3].v[2]);
    const rx_v4 dx = v4_sub(v4(E4[0].v[0], E4[1].v[0], E4[2].v[0], E4[3].v[0]), sx);
    const rx_v4 dy = v4_sub(v4(E4[0].v[1], E4[1].v[1], E4[2].v[1], E4[3].v[1]), sy);
    const rx_v4 dz = v4_sub(v4(E4[0].v[2], E4[1].v[2], E4[2].v[2], E4[3].v[2]), sz);
    const rx_v4 len = v4_sqrt(v4_add(v4_add(v4_mul(dy, dy), v4_mul(dx, dx)), v4_mul(dz, dz)));
    const rx_v4 inv = v4_div(v4_splat(1.0f), len);                                   /* 0092dba0 */
    const rx_v4 Dx = v4_mul(inv, dx), Dy = v4_mul(inv, dy), Dz = v4_mul(inv, dz);
    const rx_v4 eps = v4_splat(rx_bf(0x358637bdu)), one = v4_ld(0x00d61690), zero = v4_ld(0x00d61660);   /* 1e-6 (009777c4) */
    rx_v4 best = v4_splat(rx_bf(0x7f7fffffu));                                       /* FLT_MAX (00977494) */
    rx_v4 any = zero, nx = zero, ny = zero, nz = zero;
    R4(0x80, S[0]); R4(0x90, S[1]); R4(0x10, E4[3]); R4(0xa0, S[2]); R4(0x1a0, eps); R4(0x250, one); R4(0x240, sy);
    R4(0x70, S[3]); R4(0xd0, best); R4(0x2b0, sx); R4(0x150, zero); R4(0x200, zero); R4(0x2c0, sz); R4(0x1b0, len);
    R4(0x40, E4[0]); R4(0x50, E4[1]); R4(0x60, E4[2]); R4(0x280, Dx); R4(0x1c0, Dy); R4(0x230, Dz);
    R4(0x1e0, zero); R4(0x180, zero); R4(0xe0, zero);
    for (int k = 0; k < 4; k++) SU32(hits, 4 * k, 0);
    for (int k = 0; k < 4; k++) v4_st(points + 0x10u * (uint32_t)k, best);
    if (count > 0) SU32(E, 0x24, 0);                                                 /* the loop counter, run down to 0 */
    for (int32_t i = 0; i < count; i++) {
        const rx_addr t = tris + 0x30u * (uint32_t)i, ebx = t + 0x20;
        const rx_v4 A = v4_ld(t), B = v4_ld(t + 0x10), C = v4_ld(t + 0x20);
        R4(0x270, B); R4(0x2a0, C);
        const rx_v4 lo = v4_min(A, v4_min(B, C)), hi = v4_max(A, v4_max(B, C));
        const rx_v4 m1 = v4_cmple(lo, v4_ld(qmax));
        R4(0xc0, m1);
        if (!(v4_lb(m1, 0) && v4_lb(m1, 1) && v4_lb(m1, 2))) continue;
        const rx_v4 m2 = v4_cmple(v4_ld(qmin), hi);
        R4(0xc0, m2);
        if (!(v4_lb(m2, 0) && v4_lb(m2, 1) && v4_lb(m2, 2))) continue;
        const rx_v4 e1 = v4_sub(B, A), e2 = v4_sub(C, A);
        rx_v4 n = phys_cross_ba(e2, e1);
        n = v4_div(n, v4_splat(phys_sqrt(phys_hsum3(v4_mul(n, n)))));
        const rx_v4 Tx = v4_sub(sx, v4_lane(A, 0)), Ty = v4_sub(sy, v4_lane(A, 1)), Tz = v4_sub(sz, v4_lane(A, 2));
        const rx_v4 e1x = v4_lane(e1, 0), e1y = v4_lane(e1, 1), e1z = v4_lane(e1, 2);
        const rx_v4 e2x = v4_lane(e2, 0), e2y = v4_lane(e2, 1), e2z = v4_lane(e2, 2);
        const rx_v4 Px = v4_sub(v4_mul(e2z, Dy), v4_mul(e2y, Dz));
        const rx_v4 Py = v4_sub(v4_mul(e2x, Dz), v4_mul(e2z, Dx));
        const rx_v4 Pz = v4_sub(v4_mul(e2y, Dx), v4_mul(e2x, Dy));
        const rx_v4 det = v4_add(v4_add(v4_mul(Pz, e1z), v4_mul(Py, e1y)), v4_mul(Px, e1x));
        R4(0x220, Tx); R4(0xc0, Ty); R4(0x260, e1z); R4(0xb0, n); R4(0x290, Tz); R4(0x160, e2z); R4(0x190, e2x);
        R4(0xf0, Pz); R4(0x120, e1x); R4(0x110, e1y); R4(0x170, e2y); R4(0x130, Px); R4(0x100, Py); R4(0x140, det);
        int none_a, none_b;
        const rx_v4 front = r4_cr(&fr, 0, det, eps, ebx, &none_a);
        R4(0x210, front);
        if (none_a) continue;
        const rx_v4 inv_det = v4_div(v4_splat(1.0f), det);
        const rx_v4 u = v4_mul(v4_add(v4_add(v4_mul(Pz, Tz), v4_mul(Py, Ty)), v4_mul(Px, Tx)), inv_det);
        R4(0x140, inv_det); R4(0xf0, u);
        const rx_v4 u_le1 = r4_cr(&fr, 1, one, u, ebx, &none_a);
        R4(0x1f0, u_le1);
        const rx_v4 u_ge0 = r4_cr(&fr, 2, u, zero, ebx, &none_b);
        R4(0x1d0, u_ge0);
        if (none_a && none_b) continue;
        const rx_v4 Qx = v4_sub(v4_mul(Ty, e1z), v4_mul(Tz, e1y));
        const rx_v4 Qy = v4_sub(v4_mul(Tz, e1x), v4_mul(Tx, e1z));
        const rx_v4 Qz = v4_sub(v4_mul(Tx, e1y), v4_mul(Ty, e1x));
        const rx_v4 v = v4_mul(v4_add(v4_add(v4_mul(Qz, Dz), v4_mul(Qy, Dy)), v4_mul(Qx, Dx)), inv_det);
        R4(0x110, Qz); R4(0x100, Qx); R4(0x130, Qy); R4(0x120, v);
        const rx_v4 uv_le1 = r4_cr(&fr, 3, one, v4_add(v, u), ebx, &none_a);
        R4(0xf0, uv_le1);
        const rx_v4 v_ge0 = r4_cr(&fr, 4, v, zero, ebx, &none_b);
        if (none_a && none_b) continue;
        const rx_v4 tt = v4_mul(v4_add(v4_add(v4_mul(Qz, e2z), v4_mul(Qy, e2y)), v4_mul(Qx, e2x)), inv_det);
        rx_v4 hit = v4_and3(v4_cmplt(tt, best), v4_cmplt(zero, tt), v4_cmplt(tt, len));
        hit = v4_and(v4_and(v4_and(hit, v4_and(v_ge0, uv_le1)), v4_and(u_ge0, u_le1)), front);
        best = v4_sel(hit, tt, best);
        any = v4_or(hit, any);
        nx = v4_sel(hit, v4_lane(n, 0), nx);
        ny = v4_sel(hit, v4_lane(n, 1), ny);
        nz = v4_sel(hit, v4_lane(n, 2), nz);
        R4(0xd0, best); R4(0x200, any); R4(0x1e0, nx); R4(0x180, ny); R4(0xe0, nz);
    }
    R4(0xb0, any);
    int32_t result = 0;
    for (int k = 0; k < 4; k++) {
        const uint32_t h = v4_lb(any, k) != 0;
        SU32(hits, 4 * k, h);
        if (result || h) result = 1;
    }
    if (!result) { r4_flush(&fr, sp0, E, hits, points); return 0; }
    for (int k = 0; k < 4; k++)
        v4_st(normals + 0x10u * (uint32_t)k, v4(nx.v[k], ny.v[k], nz.v[k], 1.0f));
    for (int k = 0; k < 4; k++) {
        const rx_v4 dir = phys_normalize3(v4_sub(E4[k], S[k]));
        v4_st(points + 0x10u * (uint32_t)k, v4_add(v4_mul(dir, v4_splat(best.v[k])), S[k]));
    }
    {   /* the epilogue's frame stores: per ray |d|^2 (splatted) -> |d| and the |d|^2 != 0092dc20 mask; best's lanes splatted;
         * the normals assembled with w = 1.0 (00931f24) through +0x10 / +0xb0 */
        static const uint16_t len_at[4] = {0x210, 0x1b0, 0x160, 0x130}, neq_at[4] = {0x1f0, 0x190, 0x120, 0x100};
        static const uint16_t best_at[4] = {0x1d0, 0x170, 0x110, 0x1a0}, n_at[4] = {0x60, 0x50, 0x40, 0x10};
        const rx_v4 inf = v4_ld(0x0092dc20);
        for (int k = 0; k < 4; k++) {
            const rx_v4 d = v4_sub(E4[k], S[k]);
            const rx_v4 sum = v4_splat(phys_hsum3(v4_mul(d, d)));
            R4(len_at[k], v4_sqrt(sum));
            R4(neq_at[k], r4_cmpneq(sum, inf));
            R4(best_at[k], v4_lane(best, k));
            R4(n_at[k], v4(nx.v[k], ny.v[k], nz.v[k], F32(0, 0x00931f24)));
        }
        R4(0xb0, nz);
    }
    r4_flush(&fr, sp0, E, hits, points);
    return 1;
}
RX_FUNCTION(0x00860e90, phys_rays4_triangles)

/* s < 0 for the splatted s against the runtime zero vec4 on x, y, z (the original sums the three cmpltps masks) */
static int lt0_3(float s, rx_v4 zero) { return s < zero.v[0] && s < zero.v[1] && s < zero.v[2]; }
/* (1 - t) a + t b with the runtime one vec4 (0x00d61650) */
static rx_v4 lerp_one(rx_v4 a, rx_v4 b, float t) {
    return v4_add(v4_mul(v4_sub(v4_ld(0x00d61650), v4_splat(t)), a), v4_mul(b, v4_splat(t)));
}

/* 008639f0 (ECX = a*, EDX = p*; cdecl out*, b*, c*): the point of triangle (a, b, c) closest to p, by its Voronoi regions:
 * outside edge ab (the normals of (ap, ab) and (ac, ab) disagree): a, b or the projection on ab; else outside edge ac: a, c or
 * the projection on ac; else outside edge bc: b, c or the projection on bc; else the projection on the plane.
 * (n(u, v) = the exe's cross product v x u, phys_cross_ba; zero = the runtime vec4 0x00d61670.) */
void phys_triangle_closest_point(rx_addr pa, rx_addr pp, rx_addr out, rx_addr pb, rx_addr pc) {
    const rx_v4 a = v4_ld(pa), p = v4_ld(pp), b = v4_ld(pb), c = v4_ld(pc);
    const rx_v4 ap = v4_sub(p, a), ab = v4_sub(b, a), ac = v4_sub(c, a);
    const rx_v4 zero = v4_ld(0x00d61670);
    if (lt0_3(phys_hsum3(v4_mul(phys_cross_ba(ap, ab), phys_cross_ba(ac, ab))), zero)) {
        const float d_ab = phys_hsum3(v4_mul(ab, ap));
        if (lt0_3(d_ab, zero)) { v4_st(out, a); return; }
        if (lt0_3(phys_hsum3(v4_mul(v4_sub(p, b), v4_sub(a, b))), zero)) { v4_st(out, b); return; }
        v4_st(out, lerp_one(a, b, phys_div(d_ab, phys_hsum3(v4_mul(ab, ab)))));
        return;
    }
    if (lt0_3(phys_hsum3(v4_mul(phys_cross_ba(ap, ac), phys_cross_ba(ab, ac))), zero)) {
        const float d_ac = phys_hsum3(v4_mul(ac, ap));
        if (lt0_3(d_ac, zero)) { v4_st(out, a); return; }
        if (lt0_3(phys_hsum3(v4_mul(v4_sub(p, c), v4_sub(a, c))), zero)) { v4_st(out, c); return; }
        v4_st(out, lerp_one(a, c, phys_div(d_ac, phys_hsum3(v4_mul(ac, ac)))));
        return;
    }
    const rx_v4 ba = v4_sub(a, b), bc = v4_sub(c, b), bp = v4_sub(p, b);
    if (lt0_3(phys_hsum3(v4_mul(phys_cross_ba(bp, bc), phys_cross_ba(ba, bc))), zero)) {
        const float d_bc = phys_hsum3(v4_mul(bc, bp));
        if (lt0_3(d_bc, zero)) { v4_st(out, b); return; }
        if (lt0_3(phys_hsum3(v4_mul(v4_sub(b, c), v4_sub(p, c))), zero)) { v4_st(out, c); return; }
        v4_st(out, lerp_one(b, c, phys_div(d_bc, phys_hsum3(v4_mul(bc, bc)))));
        return;
    }
    const rx_v4 n = phys_cross_ba(ab, ac);
    const float k = phys_div(phys_hsum3(v4_mul(ap, n)), phys_hsum3(v4_mul(n, n)));
    v4_st(out, v4_sub(p, v4_mul(v4_splat(k), n)));
}
RX_FUNCTION(0x008639f0, phys_triangle_closest_point)

/* a splat value's x, y, z lanes all satisfy the comparison (the original's three-mask sum == -3) */
static int all3(rx_v4 m) { return v4_lb(m, 0) && v4_lb(m, 1) && v4_lb(m, 2); }

/* 00873540 (ECX = hit flags out (12 x u32), EDX = normals out (12 vec4); cdecl triangles*, triangle count, hit points out
 * (12 vec4), edges* (12 pairs start / end), query box min*, max*): the 12 box edges as segments against the triangles whose
 * box overlaps the query box (Moller-Trumbore per edge, the values splatted in vec4s: D_k = unit edge direction, front faces
 * only (det >= 1e-6), 0 <= u <= 1, 0 <= v, u + v <= 1, 0 < t < |edge| and nearer than the edge's best so far). A hit edge
 * records t (in its point slot), the triangle's unit normal and its flag; at the end the hit points become S + t D.
 * Returns 1 when any edge hit (EAX). One (0x00d61650) and zero (0x00d61670) are runtime vec4s.
 *
 * Frame: push ebp; and esp, -16; sub esp, 0xc24; push ebx / esi / edi -> E = ((entry ESP - 4) & ~15) - 0xc30. The original
 * keeps every per-edge quantity in arrays there (disassembly 00873540..008740e9), and later code at the same depth reads them
 * uninitialized (a CylinderVsCylinder after a CylinderVsTriangles: 0081c590's Newton step can be +0xb80, edge 2's recorded v),
 * so the port leaves the original's image (e12_frame, written once at the end): the arrays of 16-byte rows t +0x260, u +0x320,
 * D +0x3e0, v +0x4a0, 1/det +0x560, T +0x620, P +0x6e0, det +0x7a0, Q +0x860, u + v +0x920, |edge| +0x9e0, and per hit edge its u
 * +0xaa0 / v +0xb60; the dword flag arrays ok_u +0x20, ok_v +0x50, front +0xd0, u <= 1 +0x100, u + v <= 1 +0x130, t < |edge|
 * +0x160, 0 < t +0x190, hit +0x1c0, t < best +0x1f0; the last compare mask +0x240, the normal +0x250, the box max +0x220, 1e-6
 * +0x230; the words +0x14 (the points pointer, 0 after a hit pass), +0x18 (any hit), +0x1c (the triangle index * 3), +0x80 / +0xa0
 * / +0xbc / +0xc0 (the hit pass's array bases), +0x98 / +0x9c / +0xb4 / +0xb8 (points, 3 * count, normals, hits), the cookie +0xc2c
 * and the cookie check's return address +0x08. The caller's ESI / EDI / EBP (+0x04 / +0x00 / entry ESP - 4) are the caller's to
 * write (phys_edges12_saved_regs). */
typedef struct {
    rx_v4 t[12], u[12], v[12], inv[12], T[12], P[12], det[12], Q[12], uv[12], hu[12], hv[12];
    uint32_t ok_u[12], ok_v[12], front[12], u_le1[12], uv_le1[12], t_len[12], t_pos[12], hit[12], t_best[12];
    rx_v4 mask, n, qmax;
    uint32_t w14;
    unsigned have;   /* E12_* bits: which groups the run stored */
    uint32_t hits_at;   /* bit k: edge k's hit u / v rows were stored */
} e12_frame;
enum { E12_BOX = 1, E12_FRONT = 2, E12_U = 4, E12_V = 8, E12_PASS = 16, E12_MASK = 32, E12_LOOP = 64 };

static void e12_flush(const e12_frame* f, rx_addr E, rx_addr hits, rx_addr normals, rx_addr points, int32_t count,
                      int32_t any, const rx_v4 dir[12], const rx_v4 len[12], uint32_t ret) {
    for (int k = 0; k < 12; k++) { v4_st(E + 0x3e0 + 0x10u * (uint32_t)k, dir[k]); v4_st(E + 0x9e0 + 0x10u * (uint32_t)k, len[k]); }
    v4_st(E + 0x230, v4_splat(rx_bf(0x358637bdu)));
    SU32(E, 0x98, points); SU32(E, 0x9c, (uint32_t)(3 * count)); SU32(E, 0xb4, normals); SU32(E, 0xb8, hits);
    SU32(E, 0x18, (uint32_t)any);
    SU32(E, 0x1c, f->have & E12_LOOP ? (uint32_t)(3 * count) : 0);
    if (f->have & E12_LOOP) v4_st(E + 0x220, f->qmax);
    if (f->have & E12_MASK) v4_st(E + 0x240, f->mask);
#define ROWS(off, arr) for (int k = 0; k < 12; k++) v4_st(E + (off) + 0x10u * (uint32_t)k, f->arr[k])
#define FLAGS(off, arr) for (int k = 0; k < 12; k++) SU32(E, (off) + 4u * (uint32_t)k, f->arr[k])
    if (f->have & E12_BOX) { v4_st(E + 0x250, f->n); ROWS(0x620, T); ROWS(0x6e0, P); ROWS(0x7a0, det); ROWS(0x320, u); FLAGS(0xd0, front);
                             SU32(E, 0x14, f->w14); }
    if (f->have & E12_FRONT) { ROWS(0x560, inv); FLAGS(0x20, ok_u); FLAGS(0x100, u_le1); }
    if (f->have & E12_U) { ROWS(0x860, Q); ROWS(0x4a0, v); ROWS(0x260, t); ROWS(0x920, uv); FLAGS(0x50, ok_v); FLAGS(0x130, uv_le1); }
    if (f->have & E12_V) { FLAGS(0x160, t_len); FLAGS(0x190, t_pos); FLAGS(0x1f0, t_best); FLAGS(0x1c0, hit); }
    if (f->have & E12_PASS) {
        SU32(E, 0x80, E + 0xaa0 - points); SU32(E, 0xa0, E + 0x4a0 - points); SU32(E, 0xbc, E + 0x1c0 - hits);
        SU32(E, 0xc0, E + 0xb60 - points);
    }
    for (int k = 0; k < 12; k++)
        if (f->hits_at & (1u << k)) { v4_st(E + 0xaa0 + 0x10u * (uint32_t)k, f->hu[k]); v4_st(E + 0xb60 + 0x10u * (uint32_t)k, f->hv[k]); }
#undef ROWS
#undef FLAGS
    SU32(E, 0xc2c, U32(0, 0x9b45e4) ^ (E + 0xc));    /* the cookie */
    SU32(E, 8, ret);                                  /* __security_check_cookie's return address (over the saved EBX) */
}

/* the caller's ESI / EDI / EBP as 00873540 (entered at ESP sp0) leaves them in its frame */
void phys_edges12_saved_regs(rx_addr sp0, uint32_t esi, uint32_t edi, uint32_t ebp) {
    const rx_addr E = ((sp0 - 4) & ~15u) - 0xc30;
    SU32(E, 4, esi); SU32(E, 0, edi); SU32(sp0 - 4, 0, ebp);
}

int32_t phys_edges12_triangles(rx_addr hits, rx_addr normals, rx_addr tris, int32_t count, rx_addr points, rx_addr edges,
                               rx_addr qmin, rx_addr qmax) {
    const rx_addr sp0 = rx_sp(), E = ((sp0 - 4) & ~15u) - 0xc30;
    e12_frame f;
    f.have = 0; f.hits_at = 0;
    rx_v4 dir[12], len[12], S[12];
    for (int k = 0; k < 12; k++) SU32(hits, 4 * k, 0);
    for (int k = 0; k < 12; k++) v4_st(points + 0x10u * (uint32_t)k, v4_splat(rx_bf(0x7f7fffffu)));   /* FLT_MAX */
    for (int k = 0; k < 12; k++) {
        S[k] = v4_ld(edges + 0x20u * (uint32_t)k);
        const rx_v4 d = v4_sub(v4_ld(edges + 0x20u * (uint32_t)k + 0x10), S[k]);
        const rx_v4 p = v4_mul(d, d);
        len[k] = v4_splat(phys_sqrt((p.v[0] + p.v[2]) + p.v[1]));
        dir[k] = phys_normalize3(d);
    }
    const rx_v4 eps = v4_splat(rx_bf(0x358637bdu));                                   /* 1e-6 (009777c4) */
    int32_t any = 0;
    if (3 * count > 0) { f.have |= E12_LOOP; f.qmax = v4_ld(qmax); }
    for (int32_t i = 0; i < 3 * count; i += 3) {
        const rx_v4 A = v4_ld(tris + 0x10u * (uint32_t)i), B = v4_ld(tris + 0x10u * (uint32_t)i + 0x10);
        const rx_v4 C = v4_ld(tris + 0x10u * (uint32_t)i + 0x20);
        f.have |= E12_MASK;
        f.mask = v4_cmple(v4_min(A, v4_min(B, C)), f.qmax);
        if (!all3(f.mask)) continue;
        f.mask = v4_cmple(v4_ld(qmin), v4_max(A, v4_max(B, C)));
        if (!all3(f.mask)) continue;
        const rx_v4 e2 = v4_sub(C, A), e1 = v4_sub(B, A);
        rx_v4 n = phys_cross_ba(e2, e1);
        n = v4_div(n, v4_splat(phys_sqrt(phys_hsum3(v4_mul(n, n)))));
        f.have |= E12_BOX; f.n = n; f.w14 = points;
        int any_front = 0, any_u = 0;
        for (int k = 0; k < 12; k++) {
            f.T[k] = v4_sub(S[k], A);
            f.P[k] = phys_cross_ba(e2, dir[k]);
            f.det[k] = v4_splat(phys_hsum3(v4_mul(f.P[k], e1)));
            f.u[k] = v4_splat(phys_hsum3(v4_mul(f.T[k], f.P[k])));                      /* T . P (u before the division) */
        }
        for (int k = 0; k < 12; k++) {
            f.mask = v4_cmple(eps, f.det[k]);
            f.front[k] = (uint32_t)all3(f.mask);
            any_front |= (int)f.front[k];
        }
        if (!any_front) continue;
        f.have |= E12_FRONT;
        const rx_v4 one = v4_ld(0x00d61650), zero = v4_ld(0x00d61670);
        for (int k = 0; k < 12; k++) f.inv[k] = v4_div(one, f.det[k]);
        for (int k = 0; k < 12; k++) f.u[k] = v4_mul(f.inv[k], f.u[k]);
        for (int k = 0; k < 12; k++) { f.mask = v4_cmple(zero, f.u[k]); f.ok_u[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) { f.mask = v4_cmple(f.u[k], one); f.u_le1[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) { f.ok_u[k] &= f.u_le1[k]; any_u |= (int)f.ok_u[k]; }
        if (!any_u) continue;
        f.have |= E12_U;
        int any_v = 0;
        for (int k = 0; k < 12; k++) {
            f.Q[k] = phys_cross_ba(e1, f.T[k]);
            f.v[k] = v4_splat(phys_hsum3(v4_mul(f.Q[k], dir[k])));
            f.t[k] = v4_splat(phys_hsum3(v4_mul(f.Q[k], e2)));
        }
        for (int k = 0; k < 12; k++) f.v[k] = v4_mul(f.inv[k], f.v[k]);
        for (int k = 0; k < 12; k++) f.uv[k] = v4_add(f.v[k], f.u[k]);
        for (int k = 0; k < 12; k++) { f.mask = v4_cmple(zero, f.v[k]); f.ok_v[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) { f.mask = v4_cmple(f.uv[k], one); f.uv_le1[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) { f.ok_v[k] &= f.uv_le1[k]; any_v |= (int)f.ok_v[k]; }
        if (!any_v) continue;
        f.have |= E12_V | E12_PASS;
        for (int k = 0; k < 12; k++) f.t[k] = v4_mul(f.inv[k], f.t[k]);
        for (int k = 0; k < 12; k++) { f.mask = v4_cmplt(f.t[k], len[k]); f.t_len[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) { f.mask = v4_cmplt(zero, f.t[k]); f.t_pos[k] = (uint32_t)all3(f.mask); }
        for (int k = 0; k < 12; k++) {
            f.mask = v4_cmplt(f.t[k], v4_ld(points + 0x10u * (uint32_t)k));
            f.t_best[k] = (uint32_t)all3(f.mask);
        }
        for (int k = 0; k < 12; k++) f.hit[k] = f.t_best[k] & f.t_pos[k] & f.t_len[k] & f.ok_v[k] & f.ok_u[k] & f.front[k];
        for (int k = 0; k < 12; k++) {
            if (!f.hit[k]) continue;
            v4_st(points + 0x10u * (uint32_t)k, f.t[k]);
            f.hu[k] = f.u[k]; f.hv[k] = f.v[k]; f.hits_at |= 1u << k;
            SU32(hits, 4 * k, 1);
            v4_st(normals + 0x10u * (uint32_t)k, n);
            any = 1;
        }
        f.w14 = 0;                                                                     /* the hit pass's counter, run down */
    }
    if (!any) {
        e12_flush(&f, E, hits, normals, points, count, 0, dir, len, 3 * count > 0 ? 0x008740cfu : 0x008740e6u);
        return 0;
    }
    for (int k = 0; k < 12; k++) {
        const rx_addr p = points + 0x10u * (uint32_t)k;
        v4_st(p, v4_add(v4_mul(dir[k], v4_ld(p)), S[k]));
    }
    e12_flush(&f, E, hits, normals, points, count, 1, dir, len, 0x008740b8u);
    return 1;
}
RX_FUNCTION(0x00873540, phys_edges12_triangles)
