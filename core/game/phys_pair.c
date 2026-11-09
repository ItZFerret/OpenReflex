/* phys_pair.c -- contact generators of shape pairs (the narrow phase's cases, 0x0086xxxx): each writes contacts into a contact
 * set M (count +0 (at most 30: index clamped to 0x1d), records of 0x30 bytes at +0x10: +0 point (x, y, z, z), +0x10 normal
 * (x, y, z, z), +0x20 depth (splat); touching flag +0x6a0) for shape A against shape B, A's normal sign chosen by the swap flag.
 * The original's frames are reproduced (callees run at its ESP; the locals passed to them live at its offsets). */
#include "phys_pair.h"
#include "crt.h"
#include "phys_coll_shape.h"
#include "phys_coll_shape2.h"
#include "phys_cylinder.h"
#include "phys_terrain.h"
#include "phys_coll_terrain.h"
#include "phys_contact.h"
#include "phys_tri.h"
#include "world_veh.h"
#ifdef RX_ORACLE
#include <stdlib.h>   /* getenv: the RX_PRODUCT_W_LANES oracle diagnostic */
#endif

/* ---- shared pieces */

/* rotation matrix rows of a unit quaternion as the pair generators build them (xx = (2x) x, xy = (2x) y, ..., zw = (2z) w):
 * r0 = (1 - (zz + yy), xy + zw, xz - yw, 0), r1 = (xy - zw, 1 - (zz + xx), yz + xw, 0), r2 = (xz + yw, yz - xw, 1 - (yy + xx), 0) */
void phys_quat_to_rows(rx_v4 q, rx_v4 rows[3]) {
    const float x2 = q.v[0] * 2.0f, y2 = q.v[1] * 2.0f, z2 = q.v[2] * 2.0f, w = q.v[3];   /* 0092e330 */
    const float xx = x2 * q.v[0], yy = y2 * q.v[1], zz = z2 * q.v[2];
    const float xy = x2 * q.v[1], xz = x2 * q.v[2], yz = y2 * q.v[2];
    const float xw = x2 * w, yw = y2 * w, zw = z2 * w;
    rows[0] = v4(1.0f - (zz + yy), xy + zw, xz - yw, 0.0f);
    rows[1] = v4(xy - zw, 1.0f - (zz + xx), yz + xw, 0.0f);
    rows[2] = v4(xz + yw, yz - xw, 1.0f - (yy + xx), 0.0f);
}
/* rows * p + origin with the products summed x, y, z (each row scaled by a splat lane) */
static rx_v4 rows_transform(const rx_v4 rows[3], rx_v4 p, rx_v4 origin) {
    return v4_add(v4_add(v4_add(v4_mul(v4_lane(p, 0), rows[0]), v4_mul(v4_lane(p, 1), rows[1])),
                         v4_mul(v4_lane(p, 2), rows[2])), origin);
}
/* all of lanes 0..2 set in a compare mask (the original sums the three mask words to -3) */
static int all3(rx_v4 m) { return v4_lb(m, 1) + v4_lb(m, 2) + v4_lb(m, 0) == 0xfffffffdu; }
/* 00861a60 CollisionPair_SphereVsSphere (ECX = position A*, EDX = sphere A; stack: M, sphere B, position B*, swap flag;
 * RET 0x10). The two spheres' radii are the vec4s A+0x20 / B+0x20 (all four lanes are used: the record's depth keeps the w
 * lane). d = pB - pA; when d.d < (rA + rB)^2 in every lane 0..2: dist = sqrt((d.x^2 + d.y^2) + d.z^2) (splatted), penetration
 * half = ((rA + rB) - dist) * 0.5 per lane, unit normal d / dist (0 where dist is 0, the qNaN 0x92dc30 where d.d is infinite),
 * the contact point pA + (rA - half) * n (per lane), depth 2 * half, the normal negated for a swapped call. One contact.
 * Frame: push ebp; and esp, -16; sub esp, 0x20. Returns the ECX left (count + 1 on a contact; lane 1 of the compare mask when
 * the spheres miss) and *ecx_out / *edx_out = the original's ECX / EDX (on a miss the mask's lane 1 / its three-lane sum, after
 * a contact M + 0x30 * count). */
uint32_t phys_pair_sphere_sphere(rx_addr PA, rx_addr A, rx_addr M, rx_addr B, rx_addr PB, uint32_t flag, uint32_t* ecx_out,
                                 uint32_t* edx_out) {
    const rx_v4 pa = v4_ld(PA), ra = v4_ld(A + 0x20), rb = v4_ld(B + 0x20);
    const rx_v4 d = v4_sub(v4_ld(PB), pa);
    const rx_v4 dd = v4_mul(d, d);
    const rx_v4 d2 = v4_splat(phys_add(phys_add(dd.v[0], dd.v[1]), dd.v[2]));
    /* (rB^2 + rA^2) + (2 * rB) * rA, lane-wise */
    const rx_v4 sum = v4_add(v4_add(v4_mul(rb, rb), v4_mul(ra, ra)), v4_mul(v4_mul(v4_splat(2.0f /* 0x92e330 */), rb), ra));
    const rx_v4 cmp = v4_cmplt(d2, sum);
    if (!all3(cmp)) {
        /* the mask sum's leftovers: ECX = lane 1 of the compare, EDX = lanes 2 + 1 + 0 */
        const uint32_t e1 = v4_lb(cmp, 1);
        *ecx_out = e1;
        *edx_out = e1 + v4_lb(cmp, 2) + v4_lb(cmp, 0);
        return e1;
    }
    const rx_v4 dist = v4_sqrt(d2);
    const rx_v4 pen = v4_mul(v4_sub(v4_add(rb, ra), dist), v4_splat(0.5f /* 0x9322cc */));
    const rx_v4 inf = v4_ld(0x0092dc20);
    const rx_v4 finite = v4_mask(d2.v[0] != inf.v[0], d2.v[1] != inf.v[1], d2.v[2] != inf.v[2], d2.v[3] != inf.v[3]);
    const rx_v4 nz = v4_mask(dist.v[0] != 0.0f, dist.v[1] != 0.0f, dist.v[2] != 0.0f, dist.v[3] != 0.0f);
    const rx_v4 unit = v4_and(v4_and(nz, v4_div(d, dist)), finite);
    const rx_v4 n = v4_or(unit, v4_andn(finite, v4_ld(0x0092dc30)));
    const rx_v4 point = v4_add(v4_mul(v4_sub(ra, pen), n), pa);
    const rx_v4 depth = v4_mul(v4_splat(2.0f /* 0x92e330 */), pen);
    const rx_v4 normal = (flag & 0xff) != 0 ? v4_sub(v4_zero(), n) : n;
    int32_t i = I32(M, 0);
    if (i >= 0x1d) i = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)i;
    v4_st(rec + 0x10, point);
    v4_st(rec + 0x20, normal);
    v4_st(rec + 0x30, depth);
    SU32(M, 0, (uint32_t)i + 1);
    SU32(M, 0x6a0, 1);
    *ecx_out = (uint32_t)i + 1;
    *edx_out = (uint32_t)rec;
    return (uint32_t)i + 1;
}
RX_FUNCTION(0x00861a60, phys_pair_sphere_sphere)

/* a row . d in double precision (y, x, z order as the original's cvtps2pd / mulsd / addsd), rounded to float */
static float row_dot_double(rx_v4 row, rx_v4 d) {
    return (float)(((double)row.v[1] * (double)d.v[1] + (double)row.v[0] * (double)d.v[0]) + (double)row.v[2] * (double)d.v[2]);
}
static float float_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu); }      /* Float_Abs 00401000 */
static float flag_lt0(float x) { return x < 0.0f || x != x ? 1.0f : 0.0f; }     /* comiss x, 0; jb -> 1 */
static float flag_ge0(float x) { return x < 0.0f || x != x ? 0.0f : 1.0f; }     /* comiss x, 0; jb -> 0 */
/* appends a contact (point, normal, depth) to M (index clamped to 29) and sets the touching flag */
static void contact_add(rx_addr M, rx_v4 point, rx_v4 normal, float depth) {
    int32_t i = I32(M, 0);
    if (i >= 0x1d) i = 0x1d;
    const rx_addr rec = M + 0x30u * (uint32_t)i;
    v4_st(rec + 0x10, v4(point.v[0], point.v[1], point.v[2], point.v[2]));
    v4_st(rec + 0x20, v4(normal.v[0], normal.v[1], normal.v[2], normal.v[2]));
    v4_st(rec + 0x30, v4_splat(depth));
    SU32(M, 0, (uint32_t)i + 1);
    SU32(M, 0x6a0, 1);
}

/* the 12 box edges as corner index pairs (0092dec0) */
static const int32_t k_box_edges[12][2] = {{0, 4}, {1, 5}, {3, 7}, {2, 6}, {0, 1}, {4, 5}, {6, 7}, {2, 3},
                                           {0, 2}, {1, 3}, {5, 7}, {4, 6}};

/* 00861ba0 CollisionPair_SphereVsBox (ECX = sphere A, EDX = position A*; stack: M, rotation A*, box B, rotation B*,
 * position B*, swap flag; RET 0x18). Sphere: radius A+0x20; box: half extents B+0x20 (vec4).
 * In the box frame (c = the sphere center): the cases are scored with 0 / 1 weights instead of branches -- center inside
 * (all |c| < half), near a face (inside two slabs, within r of the third), near an edge (inside one slab, within r of the
 * edge line), near a corner. When any applies, the contact point is the mean of: the box corners inside the sphere, the
 * sphere / edge intersections, the center when inside, and (for a deep center: inside weight < 0.1) its projections on the
 * three faces; at least half a sample (0.5) is needed. The point goes to world space; the sphere point (00845b50 / 008454d0
 * box clamp and sphere support) gives the depth, the normal blends the box normal and the sphere direction (deep: half
 * each), normalized; negated for a swapped call. */
void phys_pair_sphere_box(rx_addr A, rx_addr PA, rx_addr M, rx_addr QA, rx_addr B, rx_addr QB, rx_addr PB, uint32_t flag) {
    /* frame: push ebp; and esp, -16; sub esp, 0x1d4; push ebx / esi / edi -> locals at L = the ESP after the pushes */
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x1d4 - 12;
    rx_set_sp(L);
    SU8(sp0, 0x18, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    const rx_v4 half = v4_ld(B + 0x20);
    rx_v4 rb[3], ra[3];
    phys_quat_to_rows(v4_ld(QB), rb);
    const rx_v4 pb = v4(F32(PB, 0), F32(PB, 4), F32(PB, 8), 1.0f);
    const float r = F32(A, 0x20);
    const rx_v4 qb = v4_ld(QB);
    const rx_v4 c = phys_quat_mul(phys_quat_conj(qb), phys_quat_mul(v4_xyz(v4_sub(v4_ld(PA), v4_ld(PB))), qb));
    phys_quat_to_rows(v4_ld(QA), ra);
    const rx_v4 pa = v4(F32(PA, 0), F32(PA, 4), F32(PA, 8), 1.0f);

    /* classification in the box frame: d = |c| - half (inside a slab when < 0), e = |c| - (half + r) (within r of a face) */
    const float ax = float_abs(c.v[0]), ay = float_abs(c.v[1]), az = float_abs(c.v[2]);
    const float gx = r + half.v[0], gy = r + half.v[1], gz = r + half.v[2];
    const float dx = ax - half.v[0], dy = ay - half.v[1], dz = az - half.v[2];
    const float dx2 = dx * dx, dy2 = dy * dy, dz2 = dz * dz, r2 = r * r;
    const float in_x = flag_lt0(dx), in_y = flag_lt0(dy), in_z = flag_lt0(dz);
    const float near_z = flag_lt0(az - gz), near_y = flag_lt0(ay - gy), near_x = flag_lt0(ax - gx);
    const float edge_x = flag_ge0((r2 - dy2) - dz2), edge_y = flag_ge0((r2 - dx2) - dz2);
    const float edge_z = flag_ge0((r2 - dx2) - dy2), corner = flag_ge0(((r2 - dx2) - dy2) - dz2);
    const float in_xy = in_y * in_x;
    const float inside = in_xy * in_z;
    const float face_z = in_xy * near_z, face_y = (in_z * in_x) * near_y, face_x = (in_z * in_y) * near_x;
    const float any = ((((((inside + corner) + face_z) + face_y) + face_x) + edge_x * in_x) + edge_y * in_y) + edge_z * in_z;
    if (any == 0.0f) { rx_set_sp(sp0); return; }

    /* samples: box corners inside the sphere, then the sphere / edge intersections (t in [0, 1) of the edge segment) */
    const rx_addr corners = L + 0x150;
    PHYS_CALL_SP(4, phys_box_corners(B, corners));                      /* CollisionShapeBox_Corners */
    rx_v4 acc = v4_ld(0x00d61670);
    float count = 0.0f;
    for (int k = 0; k < 8; k++) {
        const rx_v4 p = v4_ld(corners + 16u * (uint32_t)k);
        const float ex = p.v[0] - c.v[0], ey = p.v[1] - c.v[1], ez = p.v[2] - c.v[2];
        const float in = flag_ge0(r2 - ((ex * ex + ey * ey) + ez * ez));
        count = in + count;
        acc = v4_add(v4_mul(v4_splat(in), p), acc);
    }
    for (int k = 0; k < 12; k++) {
        const rx_v4 p0 = v4_ld(corners + 16u * (uint32_t)k_box_edges[k][0]);
        const rx_v4 e = v4_sub(v4_ld(corners + 16u * (uint32_t)k_box_edges[k][1]), p0);
        const rx_v4 w = v4_sub(c, p0);
        const float a2 = ((e.v[0] * e.v[0] + e.v[1] * e.v[1]) + e.v[2] * e.v[2]) * 2.0f;
        const float b2 = ((w.v[0] * e.v[0] + w.v[1] * e.v[1]) + w.v[2] * e.v[2]) * 2.0f;
        const float cc = (w.v[0] * w.v[0] + w.v[1] * w.v[1]) + w.v[2] * w.v[2];
        const float disc = b2 * b2 - (cc - r2) * (a2 * 2.0f);
        const float valid = flag_ge0(disc);
        const float s = phys_sqrtf(float_abs(disc));
        const float t1 = phys_div(s + b2, a2);
        const float w1 = (flag_ge0(t1) * valid) * flag_lt0(t1 - 1.0f);
        const rx_v4 p1 = v4_add(v4_mul(v4_splat(t1), e), p0);
        const float count1 = w1 + count;
        const float t2 = phys_div(b2 - s, a2);
        const rx_v4 acc1 = v4_add(v4_mul(p1, v4_splat(w1)), acc);
        const float w2 = (flag_ge0(t2) * valid) * flag_lt0(t2 - 1.0f);
        const rx_v4 p2 = v4_add(v4_mul(v4_splat(t2), e), p0);
        acc = v4_add(v4_mul(p2, v4_splat(w2)), acc1);
        count = w2 + count1;
    }
    /* the center when inside; for a deep center its three face projections */
    acc = v4_add(v4_mul(v4_splat(inside), c), acc);
    count = count + inside;
    const float deep = flag_lt0(inside - 0.1f);                                       /* 00931d94 */
    rx_v4 proj = c;
    proj.v[2] = c.v[2] < 0.0f || c.v[2] != c.v[2] ? rx_bf(0x80000000u) - half.v[2] : half.v[2];
    float wgt = deep * face_z;
    count = wgt + count;
    acc = v4_add(v4_mul(v4_splat(wgt), proj), acc);
    proj = c;
    proj.v[1] = c.v[1] < 0.0f || c.v[1] != c.v[1] ? rx_bf(0x80000000u) - half.v[1] : half.v[1];
    wgt = deep * face_y;
    count = wgt + count;
    acc = v4_add(v4_mul(v4_splat(wgt), proj), acc);
    proj = c;
    proj.v[0] = c.v[0] < 0.0f || c.v[0] != c.v[0] ? rx_bf(0x80000000u) - half.v[0] : half.v[0];
    wgt = deep * face_x;
    const float total = wgt + count;
    if (0.5f > total) { rx_set_sp(sp0); return; }                                       /* 009322cc */
    const rx_v4 local = v4_div(v4_add(v4_mul(v4_splat(wgt), proj), acc), v4_splat(total));

    /* the contact point in world space and in A's frame (double precision row dots) */
    const rx_v4 point = rows_transform(rb, local, pb);
    const rx_v4 d = v4_sub(point, pa);
    const float u = row_dot_double(ra[0], d), v = row_dot_double(ra[1], d), wl = row_dot_double(ra[2], d);
    const rx_addr box_point = L + 0x110, box_normal = L + 0x140, sphere_dir = L + 0x130, sphere_point = L + 0xf0;
    v4_st(sphere_point, v4(u, v, wl, wl));
    v4_st(box_point, local);
    v4_st(sphere_dir, local);
    PHYS_CALL_SP(4, phys_box_clamp_point(B, box_point, box_normal));    /* 00845b50: box clamp / face normal (EDI, ESI) */
    PHYS_CALL_SP(4, phys_shape_scale_to_radius(A, sphere_dir, sphere_point));   /* 008454d0: the sphere's support (EDX, ECX) */
    const rx_v4 n_box = v4_ld(box_normal), n_sph = v4_ld(sphere_dir);
    const rx_v4 nb = v4_add(v4_add(v4_mul(v4_lane(n_box, 1), rb[1]), v4_mul(v4_lane(n_box, 0), rb[0])),
                            v4_mul(v4_lane(n_box, 2), rb[2]));
    const rx_v4 ns = v4_add(v4_add(v4_mul(v4_lane(n_sph, 1), ra[1]), v4_mul(v4_lane(n_sph, 0), ra[0])),
                            v4_mul(v4_lane(n_sph, 2), ra[2]));
    const rx_v4 on_sphere = rows_transform(ra, v4_ld(sphere_point), pa);
    const rx_v4 on_box = rows_transform(rb, v4_ld(box_point), pb);
    const float sx = on_sphere.v[0] - on_box.v[0], sy = on_sphere.v[1] - on_box.v[1], sz = on_sphere.v[2] - on_box.v[2];
    const float depth = phys_sqrtf((sx * sx + sy * sy) + sz * sz);
    const rx_v4 blend = v4_add(v4_mul(v4_sub(nb, ns), v4_splat(deep * 0.5f)), v4_mul(v4_splat(1.0f - deep), nb));
    rx_v4 normal = phys_normalize3(blend);
    if ((flag & 0xff) == 0) normal = v4_sub(v4_zero(), normal);
    contact_add(M, point, normal, depth);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00861ba0, phys_pair_sphere_box)

/* sqrt of a non-negative distance with the generators' guard: an exact zero becomes 1e-5 (009774a4) */
static float guarded_length(float squared) {
    const float d = phys_sqrtf(squared);
    return d == 0.0f ? rx_bf(0x3727c5acu) : d;
}

/* 00862da0 CollisionPair_SphereVsCylinder (ECX = cylinder position*, EDX = sphere; stack: M, sphere position*, cylinder,
 * cylinder rotation*, swap flag; RET 0x14). Cylinder: radius B+0x20, height B+0x24 (axis y of its frame); sphere radius
 * A+0x20. In the cylinder frame (c = the sphere center, rho = its distance from the axis, h = half height) six cases are
 * tested, each with its own contact point on the sphere:
 *   cap inside   (rho < R, above the top cap, c - k below it): c - k        (k = r * the up vector at 0x009cb4b0, writable)
 *   cap rim      (rho > R, above the top, rim within r): c + r (rim - c) / |rim - c|
 *   bottom inside / bottom rim: as the cap cases mirrored (c + k)
 *   side         (between the caps, R in (rho - r, rho)): c (rho - r) / rho on the ring
 *   axis         (rho < R between the caps): c pushed to the axis by r
 * The flagged points are summed (a single case applies) onto the base vec4 at 0x00a95d50; 00846c70 projects it onto the
 * cylinder (EDI = cylinder, ESI = point*); depth = the distance between the two, normal = the point - c, normalized when
 * nonzero; both to world space (normal negated for a swapped call). */
void phys_pair_sphere_cylinder(rx_addr PB, rx_addr A, rx_addr M, rx_addr PA, rx_addr B, rx_addr QB, uint32_t flag) {
    /* frame: push ebp; and esp, -16; sub esp, 0x154; push ebx / esi / edi -> L */
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x154 - 12;
    rx_set_sp(L);
    rx_v4 rows[3];
    phys_quat_to_rows(v4_ld(QB), rows);
    const rx_v4 origin = v4(F32(PB, 0), F32(PB, 4), F32(PB, 8), 1.0f);
    const float R = F32(B, 0x20), h = F32(B, 0x24) * 0.5f, r = F32(A, 0x20);
    const rx_v4 qb = v4_ld(QB);
    const rx_v4 c = phys_quat_mul_ex(phys_quat_conj(qb), phys_quat_mul(v4_xyz(v4_sub(v4_ld(PA), v4_ld(PB))), qb), 1);
    const float cx = c.v[0], cy = c.v[1], cz = c.v[2];
    const float rho = guarded_length(cz * cz + cx * cx);
    const float kx = r * F32(0, 0x009cb4b0), ky = r * F32(0, 0x009cb4b4), kz = r * F32(0, 0x009cb4b8);
    const float ux = cx - kx, uy = cy - ky, uz = cz - kz;
    const int cap_inside = R > rho && cy > h && h > uy;
    /* the rim point below / above the center and the sphere points toward it */
    const float rim_x = (cx * R) / rho, rim_z = (cz * R) / rho;
    float ex = rim_x - cx, ey = h - cy, ez = rim_z - cz;
    float dist = guarded_length((ey * ey + ex * ex) + ez * ez);
    const float top_x = (ex * r) / dist + cx, top_y = (ey * r) / dist + cy, top_z = (ez * r) / dist + cz;
    const int cap_rim = rho > R && cy > h && r > dist;
    const float lo = rx_bf(0x80000000u) - h;
    const float sx = kx + cx, sy = ky + cy, sz = kz + cz;
    const int bottom_inside = R > rho && lo > cy && sy > lo;
    ey = lo - cy;
    dist = guarded_length((ey * ey + ex * ex) + ez * ez);
    const float bot_x = (ex * r) / dist + cx, bot_y = (ey * r) / dist + cy, bot_z = (ez * r) / dist + cz;
    const int bottom_rim = rho > R && lo > cy && r > dist;
    const float ring = (rho - r) / rho;
    const float side_x = ring * cx, side_z = ring * cz;
    const int side = h > cy && cy > lo && rho > R && R > rho - r;
    const float mx = rx_bf(0x80000000u) - cx, mz = rx_bf(0x80000000u) - cz;
    dist = guarded_length((mx * mx + 0.0f * 0.0f) + mz * mz);
    const float ax_x = (mx * r) / dist + cx, ax_y = (r * 0.0f) / dist + cy, ax_z = (mz * r) / dist + cz;
    const int axis = R > rho && h > cy && cy > lo;
    if (!(axis | side | bottom_rim | bottom_inside | cap_rim | cap_inside)) { rx_set_sp(sp0); return; }

    /* the flagged case's point (0 / 1 weights, summed in the original's order onto the base vec4) */
    float w = (float)(cap_inside == 1);
    float px = w * ux + F32(0, 0x00a95d50), py = uy * w + F32(0, 0x00a95d54), pz = uz * w + F32(0, 0x00a95d58);
    w = (float)(cap_rim == 1);
    px = w * top_x + px; py = top_y * w + py; pz = top_z * w + pz;
    w = (float)(bottom_inside == 1);
    px = w * sx + px; py = sy * w + py; pz = sz * w + pz;
    w = (float)(bottom_rim == 1);
    px = w * bot_x + px; py = bot_y * w + py; pz = bot_z * w + pz;
    w = (float)(side == 1);
    px = w * side_x + px; py = w * cy + py; pz = w * side_z + pz;
    w = (float)(axis == 1);
    px = ax_x * w + px; py = ax_y * w + py; pz = ax_z * w + pz;
    const rx_addr surface = L + 0x30;
    SF32(surface, 0, px); SF32(surface, 4, py); SF32(surface, 8, pz);
    PHYS_CALL_SP(0, (void)phys_cylinder_closest_point(B, surface));   /* the point onto the cylinder (ESI in / out) */
    const float dx = F32(surface, 0) - px, dy = F32(surface, 4) - py, dz = F32(surface, 8) - pz;
    const float depth = phys_sqrtf((dy * dy + dx * dx) + dz * dz);
    float nx = px - cx, ny = py - cy, nz = pz - cz;
    const float n2 = (nx * nx + ny * ny) + nz * nz;
    if (n2 > 0.0f) {
        const float inv = 1.0f / phys_sqrtf(n2);
        nx = inv * nx; ny = inv * ny; nz = inv * nz;
    }
    const rx_v4 r0 = rows[0], r1 = rows[1], r2 = rows[2];
    const float wx = ((r1.v[0] * py + r0.v[0] * px) + r2.v[0] * pz) + origin.v[0];
    const float wy = ((r0.v[1] * px + r1.v[1] * py) + r2.v[1] * pz) + origin.v[1];
    const float wz = ((r0.v[2] * px + r1.v[2] * py) + r2.v[2] * pz) + origin.v[2];
    float mx_ = (r1.v[0] * ny + r0.v[0] * nx) + r2.v[0] * nz;
    float my_ = (r0.v[1] * nx + r1.v[1] * ny) + r2.v[1] * nz;
    float mz_ = (r0.v[2] * nx + r1.v[2] * ny) + r2.v[2] * nz;
    if ((flag & 0xff) != 0) {
        mx_ = rx_bf(0x80000000u) - mx_; my_ = rx_bf(0x80000000u) - my_; mz_ = rx_bf(0x80000000u) - mz_;
    }
    contact_add(M, v4(wx, wy, wz, wz), v4(mx_, my_, mz_, mz_), depth);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00862da0, phys_pair_sphere_cylinder)

/* a <= b on x, y and z (the original sums the three cmpleps masks and compares with -3) */
static int box_le3(rx_v4 a, rx_v4 b) { return a.v[0] <= b.v[0] && a.v[1] <= b.v[1] && a.v[2] <= b.v[2]; }

/* 00873010 CollisionPair_CompositeVsHFTerrain (ECX = height-field shape T, EDX = its rotation*; stack: M, its position*,
 * composite C, C's rotation*, C's position*, swap flag; RET 0x18): the composite's world box (vfunc 2) gives the terrain
 * triangles under it (007c66f0 on the terrain found by 00402dc0 from T+0x20, into the vec4 buffer 0x00dad500); when the
 * triangles' bounds overlap the box, every child whose own world box overlaps them is dispatched against T (0085ddd0) and the
 * contacts it adds get the child's index in M's feature ids (+0x628, or +0x5b0 when not swapped).
 * Frame: push ebp; and esp, -16; sub esp, 0xd4; push ebx / esi / edi; the boxes / child pose live there (passed by address). */
void phys_pair_composite_hfterrain(rx_addr T, rx_addr QT, rx_addr M, rx_addr PT, rx_addr C, rx_addr QC, rx_addr PC,
                                   uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0xd4 - 12;
    rx_set_sp(L);
    const rx_addr ids = (flag & 0xff) ? M + 0x628 : M + 0x5b0;
    int32_t done = I32(M, 0);
    rx_addr terrain;
    PHYS_CALL_SP(0, terrain = rx_world_find_by_controller(U32(T, 0x20)));   /* the terrain object of the shape */
    const rx_addr box_max = L + 0x90, box_min = L + 0xa0, center = L + 0x60, half = L + 0x50, count = L + 0x14;
    phys_shape_bounds_call(U32(U32(C, 0), 8), C, QC, PC, box_min, box_max);
    const rx_v4 lo = v4_ld(box_min), hi = v4_ld(box_max);
    v4_st(center, v4_mul(v4_add(lo, hi), v4_splat(0.5f)));
    v4_st(half, v4_mul(v4_sub(hi, lo), v4_splat(0.5f)));
    SU32(0, 0x00db45fc, U32(0, 0x00db45fc) | 1);               /* the triangle buffer's static guard */
    SU32(count, 0, terrain);
    PHYS_CALL_SP(8, phys_terrain_grid_vertices(terrain, center, count, half));   /* triangles under the box (count out) */
    rx_v4 tmin = v4_splat(rx_bf(0x7f7fffffu)), tmax = v4_splat(rx_bf(0xff7fffffu));   /* +-FLT_MAX (00977494 / 00977550) */
    for (int32_t i = 0; i < I32(count, 0); i++) {
        const rx_v4 v = v4_ld(0x00dad500 + 16u * (uint32_t)i);
        tmin = v4_min(tmin, v); tmax = v4_max(tmax, v);
    }
    /* box overlap on x, y, z: the three cmpleps masks sum to -3 */
#define OVERLAP3(a, b) (box_le3(a, b))
    if (OVERLAP3(tmin, v4_ld(box_max)) && OVERLAP3(v4_ld(box_min), tmax)) {
        const int32_t children = (I32(C, 0x30) - I32(C, 0x2c)) >> 2;
        for (int32_t i = 0; i < children; i++) {
            if ((uint32_t)i >= (uint32_t)((I32(C, 0x30) - I32(C, 0x2c)) >> 2))
                crt_invalid_parameter_noinfo();   /* _invalid_parameter_noinfo */
            const rx_addr child = U32(U32(C, 0x2c), 4u * (uint32_t)i);
            const rx_addr child_rot = L + 0x50, child_pos = L + 0x60, cmax = L + 0xb0, cmin = L + 0xc0;
            PHYS_CALL_SP(8, phys_composite_child_rotation(C, (uint32_t)i, QC, child_rot));       /* CompositeChildRotation */
            PHYS_CALL_SP(12, phys_composite_child_position(C, (uint32_t)i, QC, PC, child_pos));  /* CompositeChildPosition */
            /* at the original's ESP: the child's bounds frame (00846700 for a cylinder) is the dead stack the child's pair
             * generator reads (core/notes/phys.md "The w lanes of CompositeVsHFTerrain") */
            phys_shape_bounds_call(U32(U32(child, 0), 8), child, child_rot, child_pos, cmin, cmax);
            if (!OVERLAP3(tmin, v4_ld(cmax)) || !OVERLAP3(v4_ld(cmin), tmax)) continue;
            PHYS_CALL_SP(28, phys_contact_dispatch(M, T, QT, PT, child, child_rot, child_pos, flag));
            for (; done < I32(M, 0); done++) SU32(ids, 4 * done, (uint32_t)i);
            done = I32(M, 0);
        }
    }
#undef OVERLAP3
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00873010, phys_pair_composite_hfterrain)

/* A 00860e90 call as the original makes it: its five stack arguments and the return address `ret` written below the caller's ESP,
 * the caller's EDI / EBP saved in the callee's frame (whose image lies under them; core/notes/phys.md "The frame of 00860e90"). */
static int32_t rays4_call(uint32_t ret, uint32_t edi, uint32_t ebp, int32_t count, rx_addr hits, rx_addr points, rx_addr tris,
                          rx_addr normals, rx_addr rays, rx_addr qmin, rx_addr qmax) {
    const rx_addr sp = rx_sp();
    SU32(sp - 24, 0, ret); SU32(sp - 20, 0, tris); SU32(sp - 16, 0, normals); SU32(sp - 12, 0, rays); SU32(sp - 8, 0, qmin);
    SU32(sp - 4, 0, qmax);
    rx_set_sp(sp - 24);
    const int32_t r = phys_rays4_triangles(count, hits, points, tris, normals, rays, qmin, qmax);
    phys_rays4_saved_regs(sp - 24, edi, ebp);
    rx_set_sp(sp);
    return r;
}

/* 0086b4d0 CollisionPair_BoxVsHFTerrain (ECX = height-field shape T, EDX = box X; stack: M, box rotation*, box position*,
 * swap flag; RET 0x10): the box's world vertices (00846230: 0x00d6d910 = center, then 12 vertices of three face groups, 0x20
 * apart), the terrain triangles under its world box (TerrainTriangles 007c6160 into 0x00db4740, at most 1000), and per face
 * group the vertex / triangle contacts (00860e90: hit flags, contact points and normals per vertex); every hit vertex gives a
 * contact (point, normal (negated when not swapped), depth |(vertex - point) . normal|).
 * Frame: push ebp; and esp, -16; sub esp, 0x214; push ebx / esi / edi (flags at +0x20, normals +0x90, points +0x150). */
void phys_pair_box_hfterrain(rx_addr T, rx_addr X, rx_addr M, rx_addr QX, rx_addr PX, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x214 - 12;
    rx_set_sp(L);
    SU8(sp0, 0x10, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    PHYS_CALL_SP(0, phys_box_world_vertices(X, QX, PX));       /* the box's world vertices into 0x00d6d910.. */
    rx_addr terrain;
    PHYS_CALL_SP(0, terrain = rx_world_find_by_controller(U32(T, 0x20)));   /* the terrain object */
    const rx_addr box_max = L + 0x50, box_min = L + 0x60, half = L + 0x70, center = L + 0x80, count = L + 0x18;
    const rx_addr flags = L + 0x20, normals = L + 0x90, points = L + 0x150;
    phys_shape_bounds(U32(U32(X, 0), 8), X, QX, PX, box_min, box_max);
    const rx_v4 lo = v4_ld(box_min), hi = v4_ld(box_max);
    v4_st(center, v4_mul(v4_add(lo, hi), v4_splat(0.5f)));
    v4_st(half, v4_mul(v4_sub(hi, lo), v4_splat(0.5f)));
    SU32(0, 0x00dc02c0, U32(0, 0x00dc02c0) | 1);               /* the triangle buffer's static guard */
    SU32(count, 0, terrain);
    PHYS_CALL_SP(20, phys_terrain_triangles(center, terrain, 0x00db4740, count, 1000, half));   /* TerrainTriangles */
    for (uint32_t g = 0; g < 3; g++) {
        /* the group's box: its four vertices and the center (0x00d6d910, included by the original in every group) */
        const rx_addr v = 0x00d6d920 + 0x80 * g;
        const rx_v4 gmin = v4_min(v4_ld(0x00d6d910), v4_min(v4_ld(v), v4_min(v4_ld(v + 0x20), v4_min(v4_ld(v + 0x40), v4_ld(v + 0x60)))));
        const rx_v4 gmax = v4_max(v4_ld(0x00d6d910), v4_max(v4_ld(v), v4_max(v4_ld(v + 0x20), v4_max(v4_ld(v + 0x40), v4_ld(v + 0x60)))));
        v4_st(box_min, gmin);
        v4_st(box_max, gmax);
        static const uint32_t ret[3] = {0x0086b620, 0x0086b6a2, 0x0086b724};
        (void)rays4_call(ret[g], U32(count, 0) /* EDI */, sp0 - 4 /* EBP */, (int32_t)U32(count, 0), flags + 0x10 * g,
                         points + 0x40 * g, 0x00db4740, normals + 0x40 * g, v - 0x10, box_min, box_max);   /* vertex / triangle contacts */
    }
    for (uint32_t i = 0; i < 12; i++) {
        if (U32(flags, 4 * i) == 0) continue;
        const rx_addr n = normals + 0x10 * i;
        if ((flag & 0xff) == 0) v4_st(n, v4_sub(v4_zero(), v4_ld(n)));
        const rx_v4 p = v4_ld(points + 0x10 * i), nv = v4_ld(n);
        const float d = phys_dot3(v4_sub(v4_ld(0x00d6d920 + 0x20 * i), p), nv);
        const float depth = phys_max(0.0f - d, d);
        int32_t k = I32(M, 0);
        if (k >= 0x1d) k = 0x1d;
        const rx_addr rec = M + 0x30u * (uint32_t)k;
        v4_st(rec + 0x10, p);
        v4_st(rec + 0x20, nv);
        v4_st(rec + 0x30, v4_splat(depth));
        SU32(M, 0, (uint32_t)k + 1);
        SU32(M, 0x6a0, 1);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0086b4d0, phys_pair_box_hfterrain)

/* 0086f2a0 CollisionPair_CylinderVsHFTerrain (ECX = cylinder position*, EDX = cylinder rotation*, ESI = height-field shape,
 * EDI = M; stack: cylinder, height-field shape, swap flag; RET 0xc). The cylinder's 24 world rays (00847630 into
 * 0x00d6d5e0..: the center, then the three rings' 8 vertices) each query the height field (TerrainSegmentQuery 007c6cc0 on
 * the terrain object found from the shape's +0x20); a hit gives a contact: point = the hit point, normal = the patch normal
 * (negated when the flag's byte is 0), depth |(ray end - point) . normal|. The flag is inverted in its stack slot.
 * Frame: push ebp; and esp, -16; sub esp, 0x48; push ebx / esi (normal +0x10, point +0x20, the ray +0x30 / +0x40). The
 * query writes only x, y, z of the normal / point; their w lanes are dead stack words in the original and are copied into the
 * record. The product initialises them to 0 (see the note at the loop) because the 4-lane response code reads them; the oracle
 * build keeps the copied residue so per-call Verify stays exact. */
void phys_pair_cylinder_hfterrain(rx_addr PA, rx_addr QA, rx_addr M, rx_addr A, rx_addr B, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), T = ((sp0 - 4) & ~15u) - 0x48 - 8;
    rx_set_sp(T);
    SU8(sp0, 0xc, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    PHYS_CALL_SP(4, phys_cylinder_world_vertices(A, PA, QA));  /* the center / vertex rays into 0x00d6d5e0.. */
    rx_addr terrain;
    PHYS_CALL_SP(0, terrain = rx_world_find_by_controller(U32(B, 0x20)));   /* the terrain object of the shape */
    const rx_addr normal = T + 0x10, point = T + 0x20, start = T + 0x30, end = T + 0x40;
#ifndef RX_ORACLE
    /* The query writes only x, y, z of its two outputs and the original copies the whole vec4s into the records, so the two w
     * lanes are uninitialised stack words there (the original leaves whatever the previous generator spilled at those frame
     * addresses; the probe shows pointers / 2048.0 / FLT_MAX). The engine's 4-lane response code does read them: the water and
     * keyed-body impulses multiply whole vectors, and with this port's residue (FLT_MAX from a box generator's points buffer)
     * FLT_MAX * vn overflows to inf and inf + -inf becomes the 0xffc00000 NaN that reached the bike's pose.w at race 287. The
     * contact's point / normal are 3-vectors, so the product initialises the two dead lanes to zero and keeps them finite.
     * Oracle build: not done, because Verify compares the record bytes against the original's own stack residue and would flag
     * these dead words (the record's live x,y,z,depth lanes are identical either way). See core/notes/phys.md "dead w lanes". */
    SU32(normal, 0xc, 0); SU32(point, 0xc, 0);
#else
    if (getenv("RX_PRODUCT_W_LANES")) { SU32(normal, 0xc, 0); SU32(point, 0xc, 0); }   /* oracle diagnostic: the product's zeroed lanes */
#endif
    for (uint32_t i = 0; i < 24; i++) {
        const rx_addr edge = 0x00d6d5e0 + 0x20u * i;
        v4_st(start, v4_ld(edge));
        v4_st(end, v4_ld(edge + 0x10));
        /* the query's stack arguments (hit, normal) and its ECX / EDX (the start / hit pointers) */
        SU32(T - 8, 0, point); SU32(T - 4, 0, normal);
        uint32_t regs[2] = {start, point}, found;
        rx_set_sp(T - 0xc);                                    /* the original's frame (its return address above the args) */
        found = phys_terrain_segment_query(terrain, start, end, point, normal, regs);
        rx_set_sp(T);
        if (found == 0) continue;
        if (U8(sp0, 0xc) != 0) {                               /* the inverted flag: the normal negated per lane */
            for (uint32_t c = 0; c < 3; c++) SF32(normal, 4 * c, phys_sub(-0.0f /* 0x9774a8 */, F32(normal, 4 * c)));
        }
        /* d = ((end.y - point.y) * n.y + (end.x - point.x) * n.x) + (end.z - point.z) * n.z, then |d| */
        const float dy = phys_sub(F32(end, 4), F32(point, 4)), dx = phys_sub(F32(end, 0), F32(point, 0));
        const float dz = phys_sub(F32(end, 8), F32(point, 8));
        const float depth = float_abs(phys_add(phys_add(phys_mul(dy, F32(normal, 4)), phys_mul(dx, F32(normal, 0))),
                                               phys_mul(dz, F32(normal, 8))));
        int32_t k = I32(M, 0);
        if (k >= 0x1d) k = 0x1d;
        const rx_addr rec = M + 0x30u * (uint32_t)k;
        v4_st(rec + 0x10, v4_ld(point));                       /* the full vec4s (the w lanes are the stack's) */
        v4_st(rec + 0x20, v4_ld(normal));
        v4_st(rec + 0x30, v4_splat(depth));
        SU32(M, 0, (uint32_t)k + 1);
        SU32(M, 0x6a0, 1);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0086f2a0, phys_pair_cylinder_hfterrain)

/* 00869d40 CollisionPair_BoxVsTriangles (ECX = box X, EDX = mesh rotation*; stack: M, box rotation*, box position*, triangle
 * mesh T (+0x20 triangle count, +0x24 triangles: 3 vec4 each), mesh position*, swap flag; RET 0x18): as the height-field
 * case (0086b4d0) in the mesh's frame: the box pose relative to the mesh (conj(qT) (pX - pT) qT, conj(qT) qX), its vertices
 * (00846230 into 0x00d6d910..), its box (vfunc 2), the three face groups' vertex rays against the mesh triangles
 * (00860e90), and per hit vertex a contact: the point and normal rotated back to world space (qT v conj(qT) + pT), depth
 * |(vertex - point) . normal| (the normal negated when not swapped). Frame as 0086b4d0 (sub esp, 0x214). */
void phys_pair_box_triangles(rx_addr X, rx_addr QT, rx_addr M, rx_addr QX, rx_addr PX, rx_addr T, rx_addr PT, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x214 - 12;
    rx_set_sp(L);
    const rx_v4 qt = v4_ld(QT), cq = phys_quat_conj(qt);
    const rx_addr local_rot = L + 0x50, local_pos = L + 0x80, box_max = L + 0x60, box_min = L + 0x70;
    const rx_addr flags = L + 0x20, normals = L + 0x90, points = L + 0x150;
    v4_st(local_pos, phys_quat_mul(cq, phys_quat_mul(v4_xyz(v4_sub(v4_ld(PX), v4_ld(PT))), qt)));
    SU8(sp0, 0x18, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    v4_st(local_rot, phys_quat_mul(cq, v4_ld(QX)));
    PHYS_CALL_SP(0, phys_box_world_vertices(X, local_rot, local_pos));   /* the box's vertices (mesh frame) into 0x00d6d910.. */
    phys_shape_bounds(U32(U32(X, 0), 8), X, local_rot, local_pos, box_min, box_max);
    for (uint32_t g = 0; g < 3; g++) {
        static const uint32_t ret[3] = {0x00869f1e, 0x00869f4f, 0x00869f80};
        (void)rays4_call(ret[g], T /* EDI */, sp0 - 4 /* EBP */, (int32_t)U32(T, 0x20), flags + 0x10 * g, points + 0x40 * g,
                         U32(T, 0x24), normals + 0x40 * g, 0x00d6d910 + 0x80 * g, box_min, box_max);   /* the group's vertex rays */
    }
    for (uint32_t i = 0; i < 12; i++) {
        if (U32(flags, 4 * i) == 0) continue;
        const rx_addr n = normals + 0x10 * i;
        if ((flag & 0xff) == 0) v4_st(n, v4_sub(v4_zero(), v4_ld(n)));
        const rx_v4 q = v4_ld(QT), qc = phys_quat_conj(q);
        const rx_v4 p = v4_ld(points + 0x10 * i);
        const rx_v4 world_point = v4_add(phys_quat_mul_ex(q, phys_quat_mul(v4_xyz(p), qc), 1), v4_ld(PT));
        const rx_v4 world_normal = phys_quat_mul(q, phys_quat_mul(v4_xyz(v4_ld(n)), phys_quat_conj(q)));
        const float d = phys_dot3(v4_sub(v4_ld(0x00d6d920 + 0x20 * i), p), v4_ld(n));
        int32_t k = I32(M, 0);
        if (k >= 0x1d) k = 0x1d;
        const rx_addr rec = M + 0x30u * (uint32_t)k;
        v4_st(rec + 0x10, world_point);
        v4_st(rec + 0x20, world_normal);
        v4_st(rec + 0x30, v4_splat(phys_max(0.0f - d, d)));
        SU32(M, 0, (uint32_t)k + 1);
        SU32(M, 0x6a0, 1);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00869d40, phys_pair_box_triangles)

/* 00863f70 CollisionPair_SphereVsTriangles (ECX = mesh position*, EDX = sphere S, EDI = triangle mesh T; stack: M, sphere
 * position*, mesh rotation*, swap flag; RET 0x10): every triangle, in world space relative to the sphere center, gets its
 * closest point to the center (008639f0 with p = the runtime zero vec4 0x00d61670); inside the radius vec4 S+0x20 (|cp|^2 <
 * r^2 on x, y, z) a contact: point = cp + center, normal = the triangle's world normal (negated when not swapped), depth =
 * r - |cp| (per lane). Frame: push ebp; and esp, -16; sub esp, 0x118; push ebx / esi (the triangle's world vertices and
 * the closest point live there: 008639f0 reads / writes them). */
void phys_pair_sphere_triangles(rx_addr PT, rx_addr S, rx_addr T, rx_addr M, rx_addr PS, rx_addr QT, uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x118 - 8;
    rx_set_sp(L);
    const rx_v4 radius = v4_ld(S + 0x20), radius2 = v4_mul(radius, radius);
    rx_v4 rows[3];
    phys_quat_to_rows(v4_ld(QT), rows);
    const rx_v4 origin = v4(F32(PT, 0), F32(PT, 4), F32(PT, 8), 1.0f);
    SU8(sp0, 0x10, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    const rx_addr wa = L + 0xd0, wb = L + 0xb0, wc = L + 0x100, closest = L + 0xf0;
    for (int32_t i = 0; i < I32(T, 0x20); i++) {
        const rx_addr tri = U32(T, 0x24) + 0x30u * (uint32_t)i;
        const rx_addr w[3] = {wa, wb, wc};
        for (int j = 0; j < 3; j++) {
            const rx_v4 v = v4_ld(tri + 0x10u * (uint32_t)j);
            const rx_v4 xy = v4_add(v4_mul(v4_lane(v, 0), rows[0]), v4_mul(v4_lane(v, 1), rows[1]));
            v4_st(w[j], v4_sub(v4_add(v4_add(v4_mul(v4_lane(v, 2), rows[2]), xy), origin), v4_ld(PS)));
        }
        PHYS_CALL_SP(12, phys_triangle_closest_point(wa, 0x00d61670, closest, wb, wc));   /* the triangle's point closest to the center */
        const rx_v4 cp = v4_ld(closest);
        const float d2 = phys_hsum3(v4_mul(cp, cp));
        if (!(d2 < radius2.v[0] && d2 < radius2.v[1] && d2 < radius2.v[2])) continue;
        const rx_v4 a = v4_ld(tri), e1 = v4_sub(v4_ld(tri + 0x10), a), e2 = v4_sub(v4_ld(tri + 0x20), a);
        const rx_v4 n = phys_normalize3(phys_cross_ba(e2, e1));
        rx_v4 normal = v4_add(v4_add(v4_mul(v4_lane(n, 0), rows[0]), v4_mul(rows[1], v4_lane(n, 1))),
                              v4_mul(rows[2], v4_lane(n, 2)));
        if ((flag & 0xff) == 0) normal = v4_sub(v4_zero(), normal);
        int32_t k = I32(M, 0);
        if (k >= 0x1d) k = 0x1d;
        const rx_addr rec = M + 0x30u * (uint32_t)k;
        v4_st(rec + 0x10, v4_add(cp, v4_ld(PS)));
        v4_st(rec + 0x20, normal);
        v4_st(rec + 0x30, v4_sub(radius, v4_sqrt(v4_splat(d2))));
        SU32(M, 0, (uint32_t)k + 1);
        SU32(M, 0x6a0, 1);
    }
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00863f70, phys_pair_sphere_triangles)

/* 0086e090 CollisionPair_CylinderVsTriangles (ECX = cylinder position*, EDX = mesh rotation*, EDI = M; stack: cylinder C,
 * cylinder rotation*, triangle mesh T, mesh position*, swap flag; RET 0x14): in the mesh's frame, the cylinder's edges
 * (00847630: two sets of 12 segments at 0x00d6d5e0 and 0x00d6d760, start / end pairs) against the mesh triangles under its
 * box (00873540); every hit edge gives a contact: point and normal rotated back to world space, depth |(end - point) .
 * normal| (the normal negated when not swapped). Frame: push ebp; and esp, -16; sub esp, 0x218; push ebx / esi.
 * The bounds call and the two 00873540 calls run at the original's ESP with its arguments: their frames are dead stack that a
 * later pair generator at the same depth reads (core/notes/phys.md "CylinderVsTriangles' bounds and edge-call frames"). */
void phys_pair_cylinder_triangles(rx_addr PC, rx_addr QT, rx_addr M, rx_addr C, rx_addr QC, rx_addr T, rx_addr PT,
                                  uint32_t flag) {
    const rx_addr sp0 = rx_sp(), L = ((sp0 - 4) & ~15u) - 0x218 - 8;
    rx_set_sp(L);
    const rx_v4 qt = v4_ld(QT), cq = phys_quat_conj(qt);
    const rx_addr local_rot = L + 0x60, local_pos = L + 0x80, box_min = L + 0x50, box_max = L + 0x70;
    const rx_addr hits = L + 0x20, normals = L + 0x90, points = L + 0x150;
    v4_st(local_pos, phys_quat_mul(cq, phys_quat_mul(v4_xyz(v4_sub(v4_ld(PC), v4_ld(PT))), qt)));
    SU8(sp0, 0x14, (flag & 0xff) == 0);                         /* the original inverts its flag argument in place */
    v4_st(local_rot, phys_quat_mul(cq, v4_ld(QC)));
    PHYS_CALL_SP(4, phys_cylinder_world_vertices(C, local_pos, local_rot));   /* the cylinder's edges (mesh frame) */
    phys_shape_bounds_call(U32(U32(C, 0), 8), C, local_rot, local_pos, box_min, box_max);   /* at the original's ESP (0086e23f) */
    if (U32(U32(C, 0), 8) == 0x00846700) SU32(L - 0xf0, 0, C);   /* DynamicCylinder::vf2's saved ESI (= C here; its frame base) */
    uint32_t esi = C;                                           /* the original's ESI at the edge calls: C, then the record count */
    for (uint32_t set = 0; set < 2; set++) {
        const rx_addr edges = 0x00d6d5e0 + 0x180 * set;
        {   /* the 12 edges against the triangles: 00873540 called as the original calls it (its six stack arguments and the return
             * address 0086e270 / 0086e4f1 written below L) */
            const uint32_t args[6] = {U32(T, 0x24), U32(T, 0x20), points, edges, box_min, box_max};
            SU32(L - 28, 0, set ? 0x0086e4f1u : 0x0086e270u);
            for (int k = 0; k < 6; k++) SU32(L - 24 + 4 * k, 0, args[k]);
            rx_set_sp(L - 28);
            (void)phys_edges12_triangles(hits, normals, args[0], (int32_t)args[1], points, edges, box_min, box_max);
            phys_edges12_saved_regs(L - 28, esi, M /* EDI */, sp0 - 4 /* EBP */);
            rx_set_sp(L);
        }
        for (uint32_t i = 0; i < 12; i++) {
            if (U32(hits, 4 * i) == 0) continue;
            const rx_addr n = normals + 0x10 * i;
            if ((flag & 0xff) == 0) v4_st(n, v4_sub(v4_zero(), v4_ld(n)));
            const rx_v4 q = v4_ld(QT);
            const rx_v4 p = v4_ld(points + 0x10 * i);
            const rx_v4 world_point = v4_add(phys_quat_mul(q, phys_quat_mul(v4_xyz(p), phys_quat_conj(q))), v4_ld(PT));
            const rx_v4 world_normal = phys_quat_mul(q, phys_quat_mul(v4_xyz(v4_ld(n)), phys_quat_conj(q)));
            const float d = phys_dot3(v4_sub(v4_ld(edges + 0x10 + 0x20 * i), p), v4_ld(n));
            int32_t k = I32(M, 0);
            if (k >= 0x1d) k = 0x1d;
            const rx_addr rec = M + 0x30u * (uint32_t)k;
            v4_st(rec + 0x10, world_point);
            v4_st(rec + 0x20, world_normal);
            v4_st(rec + 0x30, v4_splat(phys_max(0.0f - d, d)));
            SU32(M, 0, (uint32_t)k + 1);
            SU32(M, 0x6a0, 1);
            esi = (uint32_t)k + 1;
        }
    }
    /* the frame words the original leaves: PT +0x14, the first set's hit-flag cursor +0x18 (run to the end), QT +0x1c, the cookie
     * +0x21c and __security_check_cookie's return address over the saved EBX (+0x04) */
    SU32(L, 0x14, PT); SU32(L, 0x18, hits + 0x30); SU32(L, 0x1c, QT);
    SU32(L, 0x21c, U32(0, 0x9b45e4) ^ (L + 8)); SU32(L, 4, 0x0086e749);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0086e090, phys_pair_cylinder_triangles)
