/* phys_pair_circle.c -- the contact generators of the dispatcher's circle shape (type 0x400, CollisionShapeCircle, built by the
 * vehicle code for the body circles of the ATV / UTV / buggy / trucks: radius vec4 at +0x20, a disc of that radius in the
 * shape's local x / z plane, its axis the rotated runtime vector 0x00d615c0) against a sphere (008650c0) and a cylinder
 * (0086f3d0). phys_contact.c dispatches both orientations (the swapped case passes the inverted swap flag).
 *
 * Contact set M: count +0, records of 0x30 bytes from M + 0x10 (point +0x10, normal +0x20, depth vec4 +0x30), at most 30 (the
 * index is clamped to 29), touching flag +0x6a0.
 *
 * Pure SSE except the x87 copies of 0086f3d0's float arguments (fld / fstp: a signalling NaN comes out quiet). The quaternion
 * products are the collision code's form (phys_coll_quat.h coll_quat_mul, checked against every product of both functions:
 * x = l.x * r.wzyx * QX, y = l.y * r.zwxy * QY, z = r.yxwz * l.z * QZ, (z + y) + (l.w * r + x)). */
#include "phys_pair_circle.h"
#include "phys_coll_quat.h"
#include "phys_cylinder.h"
#include "world_node.h"
#include "world_util.h"

/* the w-cleared mask 0x0092db80 (x, y, z kept, w 0) */
static rx_v4 xyz_mask(rx_v4 v) { return v4_and(v, rx_ld4(0x0092db80)); }

/* (p.x * p.x + p.z * p.z) + p.y * p.y of the squared lanes, as the shufps 0x66 / 0x55 sums compute it */
static float sum_xzy(rx_v4 sq) { return phys_add(phys_add(sq.v[0], sq.v[2]), sq.v[1]); }

/* (x + y) + z of the squared lanes, as the shufps 0x99 / 0x55 sums compute it */
static float sum_xyz(rx_v4 sq) { return phys_add(phys_add(sq.v[0], sq.v[1]), sq.v[2]); }

/* the arguments (first at the lowest address) and the return address of a call the original makes from stack pointer `sp`;
 * the callee is entered with ESP at the return address */
static rx_addr push_call(rx_addr sp, int n, const uint32_t* args, uint32_t return_address) {
    const rx_addr entry = sp - 4u * (uint32_t)n - 4u;
    for (int i = 0; i < n; i++) SU32(entry + 4, 4u * (uint32_t)i, args[i]);
    SU32(entry, 0, return_address);
    rx_set_sp(entry);
    return entry;
}

/* one contact record at the count (clamped to 29); returns the record's address (the EDX the originals leave) */
static rx_addr add_record(rx_addr M, rx_v4 point, rx_v4 normal, rx_v4 depth, uint32_t* new_count) {
    int32_t count = I32(M, 0);
    if (count >= 0x1d) count = 0x1d;
    const rx_addr record = M + 0x30u * (uint32_t)count;
    v4_st(record + 0x10, point);
    v4_st(record + 0x20, normal);
    v4_st(record + 0x30, depth);
    *new_count = (uint32_t)count + 1u;
    return record;
}

/* ---- 008650c0 sphere against circle -------------------------------------------------------------------------------------------
 * The sphere's center into the circle's frame (local = conj(q) * ((ps - pc) & xyz) * q); its projection on the disc plane
 * (local with x replaced by the runtime 0x00d61670's x, the original's movss) scaled to the circle's radius gives the two rim
 * points rim = p * (R / (|p| + eps)) and rim2 = (0 - p) * (R / (|p| + eps)) (eps 0x009775a4). The nearer one (rim unless |rim -
 * local| is not below |rim2 - local|) must lie inside the sphere (distance below the sphere's radius, every lane). The contact
 * point is where the ray from the circle's center through that rim point enters the sphere: t = (b - sqrt(b * b - a * (c - r2))) / a
 * (a = rim . rim, b = rim . local, c = local . local, r2 = the sphere's radius squared), point = t * rim, back to world space;
 * normal = the normalized (point - ps) (the default normal 0x0092dc30 where the length is 0 or the square sum infinite),
 * negated with the flag; depth = |rim - point| on all four lanes.
 * Frame: push ebp; and esp, -16; sub esp, 0x5c; push esi: +0x10 pc, +0x20 zero, +0x30 ps, +0x40 the last test mask, +0x50 the
 * distance to the rim (later the point in the circle's frame). */
uint32_t phys_pair_sphere_circle(uint32_t ecx, rx_addr qcircle, rx_addr M, rx_addr sphere, rx_addr psphere, rx_addr circle,
                                 rx_addr pcircle, uint32_t* edx) {
    const rx_addr sp0 = rx_sp();
    const rx_addr L = ((sp0 - 4u) & ~15u) - 0x5cu - 4u;
    rx_set_sp(L);
    const int swap = (ecx & 0xffu) != 0;            /* CL */

    const rx_v4 q = v4_ld(qcircle);
    const rx_v4 ps = v4_ld(psphere);
    const rx_v4 pc = v4_ld(pcircle);
    v4_st(L + 0x10, pc);
    v4_st(L + 0x30, ps);

    /* the sphere's center in the circle's frame */
    const rx_v4 d = xyz_mask(v4_sub(ps, pc));
    const rx_v4 conj = v4_mul(q, COLL_QCONJ);
    const rx_v4 local = coll_quat_mul(conj, coll_quat_mul(d, q));
    const rx_v4 sphere_radius = v4_ld(sphere + 0x20);
    const rx_v4 circle_radius = v4_ld(circle + 0x20);

    /* the two rim points */
    const rx_v4 p = v4_movss(local, v4_ld(0x00d61670));
    const rx_v4 length = v4_sqrt(v4_splat(sum_xzy(v4_mul(p, p))));
    const rx_v4 scale = v4_div(circle_radius, v4_add(length, v4_splat(F32(0, 0x009775a4))));
    const rx_v4 rim = v4_mul(scale, p);
    v4_st(L + 0x20, v4_zero());
    const rx_v4 rim2 = v4_mul(v4_sub(v4_zero(), p), scale);
    const rx_v4 d1 = v4_sqrt(v4_splat(sum_xzy(v4_mul(v4_sub(rim, local), v4_sub(rim, local)))));
    v4_st(L + 0x50, d1);
    const rx_v4 d2 = v4_sqrt(v4_splat(sum_xzy(v4_mul(v4_sub(rim2, local), v4_sub(rim2, local)))));

    /* the nearer rim point, inside the sphere */
    rx_v4 mask = v4_cmplt(d1, d2);
    v4_st(L + 0x40, mask);
    const int first = v4_lb(mask, 2) + v4_lb(mask, 1) + v4_lb(mask, 0) == 0xfffffffdu;
    mask = first ? v4_cmplt(d1, sphere_radius) : v4_cmplt(d2, sphere_radius);
    v4_st(L + 0x40, mask);
    *edx = v4_lb(mask, 1);                                   /* mov edx, [esp + 0x44] */
    if (v4_lb(mask, 2) + v4_lb(mask, 1) + v4_lb(mask, 0) != 0xfffffffdu) {
        rx_set_sp(sp0);
        return ecx;                                          /* no contact: ECX as it came */
    }
    const rx_v4 r = first ? rim : rim2;

    /* where the ray from the circle's center through r enters the sphere */
    const rx_v4 ll = v4_mul(local, local);
    const float c = sum_xyz(ll);
    const rx_v4 b = v4_splat(sum_xyz(v4_mul(r, local)));
    const rx_v4 r2 = v4_mul(sphere_radius, sphere_radius);
    const rx_v4 a = v4_splat(sum_xyz(v4_mul(r, r)));
    const rx_v4 disc = v4_sub(v4_mul(b, b), v4_mul(v4_sub(v4_splat(c), r2), a));
    const rx_v4 point_local = v4_mul(v4_div(v4_sub(b, v4_sqrt(disc)), a), r);
    v4_st(L + 0x50, point_local);

    /* back to world space; the depth is |r - point| */
    const rx_v4 rotated = coll_quat_mul(q, coll_quat_mul(xyz_mask(point_local), conj));
    const rx_v4 depth = v4_sqrt(v4_splat(sum_xzy(v4_mul(v4_sub(r, point_local), v4_sub(r, point_local)))));
    const rx_v4 point = v4_add(rotated, v4_ld(L + 0x10));

    /* the normal: (point - ps) normalized */
    const rx_v4 n0 = v4_sub(point, v4_ld(L + 0x30));
    const float s = sum_xyz(v4_mul(n0, n0));
    const rx_v4 len = v4_sqrt(v4_splat(s));
    const rx_v4 inf = v4_ld(0x0092dc20);
    const rx_v4 finite = v4_mask(s != inf.v[0], s != inf.v[1], s != inf.v[2], s != inf.v[3]);
    const rx_v4 zero = v4_ld(L + 0x20);
    const rx_v4 nonzero = v4_mask(zero.v[0] != len.v[0], zero.v[1] != len.v[1], zero.v[2] != len.v[2], zero.v[3] != len.v[3]);
    rx_v4 normal = v4_or(v4_and(v4_and(v4_div(n0, len), nonzero), finite), v4_andn(finite, v4_ld(0x0092dc30)));
    if (swap) normal = v4_sub(zero, normal);

    uint32_t count = 0;
    *edx = add_record(M, point, normal, depth, &count);
    SU32(M, 0x6a0, 1);
    SU32(M, 0, count);
    rx_set_sp(sp0);
    return count;                                            /* ECX = the new count */
}
RX_FUNCTION(0x008650c0, phys_pair_sphere_circle)

/* ---- 0086f3d0 cylinder against circle -----------------------------------------------------------------------------------------
 * The circle's axis (the runtime vector 0x00d615c0 & xyz rotated by the circle's rotation) and center into the cylinder's frame
 * (conj(qa) * v * qa), then 0081c590 (the disc against the cylinder: six flagged points, caps and side) at the original's ESP.
 * The flagged points are averaged (the sum starts at the runtime vector 0x00a95d50, the count at 0); when the count is above 0:
 * point = the mean back in world space (qa * mean * conj(qa) + pa), normal = the cylinder's surface normal at the mean (00846f10)
 * rotated the same way, negated with the flag, depth = the circle's radius vec4 - |mean - center| (lanes (x + z) + y).
 * Frame: push ebp; and esp, -16; sub esp, 0xe4; push ebx / esi / edi: +0x10 M, +0x14 pa*, +0x1c circle, +0x18 the circle's
 * radius copy, +0x20 zero, +0x24 qa*, +0x28 the six flags, +0x40 a product spill / the center, +0x50 the six points, +0xb0 the
 * sum / mean (its w lane is never written), +0xc0 the axis / the depth, +0xd0 the center / the surface normal. */
uint32_t phys_pair_cylinder_circle(rx_addr pcircle, rx_addr qcylinder, rx_addr M, rx_addr cylinder, rx_addr pcylinder,
                                   rx_addr circle, rx_addr qcircle, uint32_t flag, uint32_t* edx) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0xe4u - 12u;
    rx_set_sp(L);

    SF32(L, 0xb0, F32(0, 0x00a95d50));
    SF32(L, 0xb4, F32(0, 0x00a95d54));
    SF32(L, 0xb8, F32(0, 0x00a95d58));
    SF32(L, 0x20, 0.0f);
    SU32(L, 0x10, M);

    /* the circle's axis in world space, then both into the cylinder's frame */
    const rx_v4 qc = v4_ld(qcircle);
    const rx_v4 axis = xyz_mask(rx_ld4(0x00d615c0));
    const rx_v4 axis_world = xyz_mask(coll_quat_mul(qc, coll_quat_mul(axis, v4_mul(qc, COLL_QCONJ))));
    const rx_v4 qa = v4_ld(qcylinder);
    SU32(L, 0x24, qcylinder);
    SU32(L, 0x14, pcylinder);
    SU32(L, 0x1c, circle);
    const rx_v4 conj_a = v4_mul(qa, COLL_QCONJ);
    const rx_v4 axis_product = coll_quat_mul(axis_world, qa);
    /* the original spills conj_a.y * r.zwxy of the next product at +0x40 */
    v4_st(L + 0x40, v4_mul(v4_lane(conj_a, 1), v4_perm(axis_product, 2, 3, 0, 1)));
    const rx_v4 axis_local = coll_quat_mul(conj_a, axis_product);
    v4_st(L + 0xc0, axis_local);
    const rx_v4 offset = xyz_mask(v4_sub(v4_ld(pcircle), v4_ld(pcylinder)));
    const rx_v4 center_local = coll_quat_mul(conj_a, coll_quat_mul(offset, qa));

    /* 0081c590 (ECX the axis, EAX the center, ESI the points, EDI the flags; stack 0x00d6b400, the circle's radius, the
     * cylinder's half height, its radius; RET 0x10) */
    const uint32_t cylinder_radius = world_x87_pass(U32(cylinder, 0x20));     /* fld / fstp */
    SU32(L, 0x18, U32(circle, 0x20));
    const uint32_t circle_radius = world_x87_pass(U32(L, 0x18));
    const float half_height = phys_mul(F32(cylinder, 0x24), 0.5f /* 0x009322cc */);
    v4_st(L + 0xd0, center_local);
    v4_st(L + 0x40, center_local);
    {
        const uint32_t args[4] = {0x00d6b400u, circle_radius, rx_fb(half_height), cylinder_radius};
        push_call(L, 4, args, 0x0086f6f4u);
        rx_world_disc_cylinder(L + 0xc0, L + 0x40, L + 0x50, L + 0x28, 0x00d6b400u, rx_bf(circle_radius), half_height,
                               rx_bf(cylinder_radius), cylinder, ebp);
        rx_set_sp(L);
    }
    *edx = 0;                                                 /* 0081c590 leaves EDX 0 */

    /* the mean of the flagged points (the original's operand order per point) */
    const float one = 1.0f;                                   /* 0x00931f24 */
    float x, y, z, count;
    if (U32(L, 0x28) != 0) {
        x = phys_add(F32(L, 0x50), F32(L, 0xb0));
        z = phys_add(F32(L, 0x58), F32(L, 0xb8));
        y = phys_add(F32(L, 0x54), F32(L, 0xb4));
        count = one;
    } else {
        y = F32(L, 0xb4);
        z = F32(L, 0xb8);
        count = F32(L, 0x20);
        x = F32(L, 0xb0);
    }
    if (U32(L, 0x2c) != 0) {
        x = phys_add(F32(L, 0x60), x);
        y = phys_add(F32(L, 0x64), y);
        z = phys_add(F32(L, 0x68), z);
        count = phys_add(count, one);
    }
    if (U32(L, 0x30) != 0) {
        y = phys_add(y, F32(L, 0x74));
        z = phys_add(z, F32(L, 0x78));
        x = phys_add(F32(L, 0x70), x);
        count = phys_add(count, one);
    }
    for (uint32_t k = 3; k < 6; k++) {
        if (U32(L, 0x28 + 4u * k) == 0) continue;
        const rx_addr pt = L + 0x50 + 0x10u * k;
        x = phys_add(x, F32(pt, 0));
        y = phys_add(y, F32(pt, 4));
        z = phys_add(z, F32(pt, 8));
        count = phys_add(count, one);
    }
    if (!(count > 0.0f /* 0x0092e12c */)) {
        rx_set_sp(sp0);
        return U32(0, 0x009b45e4);                            /* ECX at the epilogue: the security cookie */
    }
    const float inverse = phys_div(one, count);
    SF32(L, 0xb4, phys_mul(y, inverse));
    SF32(L, 0xb8, phys_mul(z, inverse));
    SF32(L, 0xb0, phys_mul(inverse, x));
    const rx_v4 mean = v4_ld(L + 0xb0);

    /* depth: the circle's radius minus the mean's distance from the center */
    const rx_v4 to_center = v4_sub(mean, v4_ld(L + 0xd0));
    const rx_v4 distance = v4_sqrt(v4_splat(sum_xzy(v4_mul(to_center, to_center))));
    const rx_v4 depth = v4_sub(v4_ld(circle + 0x20), distance);
    {
        /* 00846f10 (ESI the cylinder, EDI the point; stack the normal*; RET 4) */
        const uint32_t args[1] = {L + 0xd0};
        push_call(L, 1, args, 0x0086f8bdu);
        v4_st(L + 0xc0, depth);
        (void)phys_cylinder_closest_point_normal(cylinder, L + 0xb0, L + 0xd0);
        rx_set_sp(L);
    }

    /* the point and the normal back in world space */
    const rx_v4 q = v4_ld(U32(L, 0x24));
    const rx_v4 conj = v4_mul(q, COLL_QCONJ);
    const rx_v4 point = v4_add(coll_quat_mul(q, coll_quat_mul(xyz_mask(v4_ld(L + 0xb0)), conj)), v4_ld(U32(L, 0x14)));
    rx_v4 normal = coll_quat_mul(q, coll_quat_mul(xyz_mask(v4_ld(L + 0xd0)), conj));
    if ((flag & 0xffu) != 0) normal = v4_sub(v4_zero(), normal);

    uint32_t count_after = 0;
    (void)add_record(M, point, normal, v4_ld(L + 0xc0), &count_after);
    SU32(M, 0, count_after);
    SU32(M, 0x6a0, 1);
    *edx = M;                                                 /* mov edx, [esp + 0x10] */
    rx_set_sp(sp0);
    return U32(0, 0x009b45e4);
}
RX_FUNCTION(0x0086f3d0, phys_pair_cylinder_circle)
