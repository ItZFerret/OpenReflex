/* phys_pair_query.c -- the contact generators of the dispatcher's query shape (type 0x100): the shape's +0x20 is an object
 * whose virtual functions answer the collision queries itself:
 *   vf8  (+0x20) ray query:    (hit point out*, hit normal out*, material out* (a 16-bit word), ray* (start, end vec4), 0, 1)
 *   vf17 (+0x44) sphere query: (contact point out*, contact normal out*, material out*, center*, radius (float))
 * both thiscall, returning nonzero for a hit. Against a sphere (00864dd0: the sphere query), a box (0086b360: its 12 center ->
 * vertex rays), a cylinder (0086f150: its 24 rays) and a ray list (00872970: each segment in world space). phys_contact.c
 * dispatches both orientations (the swapped case passes the inverted swap flag).
 *
 * No class of the executable builds a type-0x100 shape (see phys_pair_query.h), so the query object's class is unknown: the
 * virtual call goes to the core function registered at the slot's address, else to the original (the oracle; in the product a
 * fatal unported call naming the address). Integer arguments only (the sphere query's radius goes as its float bits).
 *
 * Contact set M: count +0, records of 0x30 bytes from M + 0x10 (point +0x10, normal +0x20, depth vec4 +0x30), at most 30 (the
 * index is clamped to 29), touching flag +0x6a0. */
#include "phys_pair_query.h"
#include "phys_coll_quat.h"
#include "phys_coll_shape2.h"
#include "world_util.h"

/* the w-cleared mask 0x0092db80 */
static rx_v4 xyz_mask(rx_v4 v) { return v4_and(v, rx_ld4(0x0092db80)); }

/* |x| as Float_Abs (00401000) leaves it in xmm0 lane 0 */
static float float_abs(float x) { return rx_bf(rx_fb(x) & 0x7fffffffu /* 0x00977570 */); }

/* the stack words a call to Float_Abs leaves below `sp`: its return address and the saved EBP */
static void float_abs_frame(rx_addr sp, uint32_t return_address, rx_addr ebp) {
    SU32(sp, -4, return_address);
    SU32(sp, -8, ebp);
}

/* the arguments (first at the lowest address) and the return address of a call made from stack pointer `sp` */
static rx_addr push_call(rx_addr sp, int n, const uint32_t* args, uint32_t return_address) {
    const rx_addr entry = sp - 4u * (uint32_t)n - 4u;
    for (int i = 0; i < n; i++) SU32(entry + 4, 4u * (uint32_t)i, args[i]);
    SU32(entry, 0, return_address);
    return entry;
}

/* a virtual thiscall of the query object at vtable byte offset `slot`, made from stack pointer `sp` with n (5 or 6) arguments:
 * the core function at the slot's address when there is one, else the original */
static uint32_t query_vcall(rx_addr object, uint32_t slot, rx_addr sp, int n, const uint32_t* args, uint32_t return_address) {
    typedef uint32_t (*f5_t)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    typedef uint32_t (*f6_t)(rx_addr, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t);
    const rx_addr target = U32(U32(object, 0), slot);
    void* f = rx_fn(target);
    uint32_t r;
    rx_set_sp(sp);
    if (!f) {
        r = rx_call_original(target, RX_THISCALL, object, n, args);
    } else {
        rx_set_sp(push_call(sp, n, args, return_address));
        if (n == 5) r = ((f5_t)f)(object, args[0], args[1], args[2], args[3], args[4]);
        else r = ((f6_t)f)(object, args[0], args[1], args[2], args[3], args[4], args[5]);
    }
    rx_set_sp(sp);
    return r;
}

/* one record (point, normal, depth splat) at the count clamped to 29 */
static void add_record(rx_addr M, rx_v4 point, rx_v4 normal, rx_v4 depth) {
    int32_t k = I32(M, 0);
    if (k >= 0x1d) k = 0x1d;
    const rx_addr record = M + 0x30u * (uint32_t)k;
    v4_st(record + 0x10, point);
    v4_st(record + 0x20, normal);
    v4_st(record + 0x30, depth);
    SU32(M, 0, (uint32_t)k + 1u);
    SU32(M, 0x6a0, 1);
}

/* the hit normal at `n` negated per lane (-0.0 - n, 0x009774a8) for x, y, z */
static void negate_normal(rx_addr n) {
    const float minus_zero = -0.0f;
    const float x = phys_sub(minus_zero, F32(n, 0));
    const float y = phys_sub(minus_zero, F32(n, 4));
    const float z = phys_sub(minus_zero, F32(n, 8));
    SF32(n, 0, x);
    SF32(n, 4, y);
    SF32(n, 8, z);
}

/* the ray-query contacts of `count` rays at `rays` (0x20 apart) against the query shape: per hit a record (point = the hit,
 * normal = the hit normal, negated when the inverted flag byte at `inverted` is set, depth = |(end - hit) . normal| as
 * (dy * ny + dx * nx) + dz * nz). L = the caller's frame (hit +hit_at, normal +normal_at, material +material_at). The original
 * reloads the object (shape +0x20) and its slot every ray. */
static void ray_queries(rx_addr L, rx_addr ebp, rx_addr M, rx_addr shape, rx_addr rays, uint32_t count, rx_addr inverted,
                        uint32_t hit_at, uint32_t normal_at, uint32_t material_at, rx_addr counter, uint32_t call_return,
                        uint32_t abs_return) {
    SU32(counter, 0, count);
    uint32_t i = 0;
    do {
        const rx_addr ray = rays + 0x20u * i;
        const uint32_t args[6] = {L + hit_at, L + normal_at, L + material_at, ray, 0, 1};
        if (query_vcall(U32(shape, 0x20), 0x20, L, 6, args, call_return) != 0) {
            if (U8(inverted, 0) != 0) negate_normal(L + normal_at);
            const rx_addr hit = L + hit_at, n = L + normal_at, end = ray + 0x10;
            const float dy = phys_mul(phys_sub(F32(end, 4), F32(hit, 4)), F32(n, 4));
            const float dx = phys_mul(phys_sub(F32(end, 0), F32(hit, 0)), F32(n, 0));
            const float dz = phys_mul(phys_sub(F32(end, 8), F32(hit, 8)), F32(n, 8));
            const float depth = float_abs(phys_add(phys_add(dy, dx), dz));
            float_abs_frame(L, abs_return, ebp);
            add_record(M, v4_ld(hit), v4_ld(n), v4_splat(depth));
        }
        ++i;
        SU32(counter, 0, U32(counter, 0) - 1u);
    } while (U32(counter, 0) != 0);
}

/* ---- 00864dd0 sphere against query shape ------------------------------------------------------------------------------------
 * The sphere query of the shape's object (vf17) with the sphere's center and radius (the radius vec4's x, an x87 copy). A hit:
 * point = the returned point, normal = the returned normal (negated when the flag is 0), depth = the radius vec4 - |point -
 * center| ((x + z) + y).
 * Frame: push ebp; and esp, -16; sub esp, 0x4c; push ebx: +0x08 the material word, +0x0c the radius, +0x10 the center copy,
 * +0x20 the normal, +0x30 the point, the security cookie at +0x4c. */
uint32_t phys_pair_sphere_query(rx_addr sphere, rx_addr psphere, rx_addr M, rx_addr shape, uint32_t flag) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr A = (ebp & ~15u) - 0x4cu;
    const rx_addr L = A - 4u;
    rx_set_sp(L);
    SU32(A, 0x48, U32(0, 0x009b45e4) ^ A);                    /* the security cookie */
    SU16(L, 8, 0);
    const rx_addr object = U32(shape, 0x20);
    SU32(L, 0xc, U32(sphere, 0x20));
    const uint32_t radius = world_x87_pass(U32(L, 0xc));      /* fld / fstp */
    v4_st(L + 0x10, v4_ld(psphere));
    const uint32_t args[5] = {L + 0x30, L + 0x20, L + 8, L + 0x10, radius};
    if (query_vcall(object, 0x44, L, 5, args, 0x00864e2au) != 0) {
        if ((flag & 0xffu) == 0) v4_st(L + 0x20, v4_sub(v4_zero(), v4_ld(L + 0x20)));
        const rx_v4 point = v4_ld(L + 0x30);
        const rx_v4 d = v4_sub(point, v4_ld(psphere));
        const rx_v4 sq = v4_mul(d, d);
        const rx_v4 distance = v4_sqrt(v4_splat(phys_add(phys_add(sq.v[0], sq.v[2]), sq.v[1])));
        add_record(M, point, v4_ld(L + 0x20), v4_sub(v4_ld(sphere + 0x20), distance));
    }
    rx_set_sp(sp0);
    return U32(0, 0x009b45e4);                                /* ECX after the cookie check */
}
RX_FUNCTION(0x00864dd0, phys_pair_sphere_query)

/* ---- 0086b360 box against query shape ---------------------------------------------------------------------------------------
 * The box's 12 world rays (00846230 into 0x00d6d910.., copied to the frame) each go to the ray query.
 * Frame: push ebp; and esp, -16; sub esp, 0x1c4; push ebx / esi / edi: +0x14 the material word, +0x18 M, +0x1c the ray count,
 * +0x20 the normal, +0x3c the query shape, +0x40 the hit, +0x50 the 12 rays. */
void phys_pair_box_query(rx_addr box, rx_addr pbox, rx_addr M, rx_addr qbox, rx_addr shape, uint32_t flag) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0x1c4u - 12u;
    rx_set_sp(L);
    SU32(L, 0x18, M);
    SU32(L, 0x3c, shape);
    SU8(sp0, 0x10, (flag & 0xffu) == 0);                     /* sete byte [ebp + 0x14]: the flag inverted in its slot */
    SU32(L, -4, 0x0086b38du);
    rx_set_sp(L - 4);
    phys_box_world_vertices(box, qbox, pbox);                  /* 00846230 (ECX the box, ESI its rotation*, EDX its position*) */
    rx_set_sp(L);
    SU16(L, 0x14, 0);
    for (uint32_t i = 0; i < 0x180; i += 0x10) v4_st(L + 0x50 + i, v4_ld(0x00d6d910 + i));
    ray_queries(L, ebp, M, U32(L, 0x3c), L + 0x50, 12, sp0 + 0x10, 0x40, 0x20, 0x14, L + 0x1c, 0x0086b3f3u, 0x0086b499u);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0086b360, phys_pair_box_query)

/* ---- 0086f150 cylinder against query shape ----------------------------------------------------------------------------------
 * The cylinder's 24 world rays (00847630 into 0x00d6d5e0.., copied to the frame) each go to the ray query.
 * Frame: push ebp; and esp, -16; sub esp, 0x328; push ebx / esi: +0x08 the material word, +0x0c the ray count, +0x10 the normal,
 * +0x20 the hit, +0x30 the 24 rays. */
void phys_pair_cylinder_query(rx_addr pcylinder, rx_addr qcylinder, rx_addr M, rx_addr cylinder, rx_addr shape, uint32_t flag) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0x328u - 8u;
    rx_set_sp(L);
    {
        /* 00847630 (ECX the position*, EDX the rotation*; stack the cylinder; RET 4) */
        const uint32_t args[1] = {cylinder};
        SU8(sp0, 0xc, (flag & 0xffu) == 0);                  /* sete byte [ebp + 0x10]: the flag inverted in its slot */
        rx_set_sp(push_call(L, 1, args, 0x0086f172u));
        phys_cylinder_world_vertices(cylinder, pcylinder, qcylinder);
        rx_set_sp(L);
    }
    SU16(L, 8, 0);
    for (uint32_t i = 0; i < 0x300; i += 0x10) v4_st(L + 0x30 + i, v4_ld(0x00d6d5e0 + i));
    ray_queries(L, ebp, M, shape, L + 0x30, 24, sp0 + 0xc, 0x20, 0x10, 8, L + 0xc, 0x0086f1cfu, 0x0086f24eu);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0086f150, phys_pair_cylinder_query)

/* ---- 00872970 ray list against query shape ----------------------------------------------------------------------------------
 * Per segment: both ends into world space (q * (p & xyz) * conj(q) + position, as 00872c90), then the ray query from the first
 * end to the second.
 * Frame: push ebp; and esp, -16; sub esp, 0x8c; push ebx: +0x08 the material word, +0x0c the loop count, +0x10 the normal,
 * +0x24 the query shape, +0x28 the list's position*, +0x2c its rotation*, +0x30 the hit, +0x40 / +0x50 the ray (the two ends),
 * +0x60 / +0x70 / +0x80 product spills (later the position). */
void phys_pair_rays_query(rx_addr prays, rx_addr shape, rx_addr rays, rx_addr M, rx_addr qrays, uint32_t flag) {
    const rx_addr sp0 = rx_sp();
    const rx_addr ebp = sp0 - 4u;
    const rx_addr L = (ebp & ~15u) - 0x8cu - 4u;
    rx_set_sp(L);
    SU32(L, 0x2c, qrays);
    SU8(sp0, 8, (flag & 0xffu) == 0);                        /* sete byte [ebp + 0xc]: the flag inverted in its slot */
    SU16(L, 8, 0);
    SU32(L, 0x28, prays);
    SU32(L, 0x24, shape);
    const int32_t count = I32(rays, 0x20);
    if (count <= 0) {
        rx_set_sp(sp0);
        return;
    }
    SU32(L, 0xc, (uint32_t)count);
    uint32_t offset = 0;
    do {
        const rx_addr entries = U32(rays, 0x24);
        const rx_v4 q = v4_ld(U32(L, 0x2c));
        const rx_v4 conj = v4_mul(q, COLL_QCONJ);
        const rx_v4 t1 = coll_quat_mul(xyz_mask(v4_ld(entries + offset)), conj);
        v4_st(L + 0x70, v4_perm(t1, 2, 3, 0, 1));
        v4_st(L + 0x60, conj);
        const rx_v4 t2 = coll_quat_mul(xyz_mask(v4_ld(entries + offset + 0x10)), conj);
        v4_st(L + 0x80, v4_perm(t2, 2, 3, 0, 1));
        const rx_v4 position = v4_ld(U32(L, 0x28));
        v4_st(L + 0x60, position);
        v4_st(L + 0x70, v4_mul(v4_lane(q, 1), v4_perm(t1, 2, 3, 0, 1)));
        v4_st(L + 0x40, v4_add(coll_quat_mul(q, t1), position));
        v4_st(L + 0x50, v4_add(coll_quat_mul(q, t2), position));

        const uint32_t args[6] = {L + 0x30, L + 0x10, L + 8, L + 0x40, 0, 1};
        if (query_vcall(U32(U32(L, 0x24), 0x20), 0x20, L, 6, args, 0x00872baeu) != 0) {
            if (U8(sp0, 8) != 0) negate_normal(L + 0x10);
            const float dx = phys_mul(phys_sub(F32(L, 0x50), F32(L, 0x30)), F32(L, 0x10));
            const float dy = phys_mul(phys_sub(F32(L, 0x54), F32(L, 0x34)), F32(L, 0x14));
            const float dz = phys_mul(phys_sub(F32(L, 0x58), F32(L, 0x38)), F32(L, 0x18));
            const float depth = float_abs(phys_add(phys_add(dx, dy), dz));
            float_abs_frame(L, 0x00872c31u, ebp);
            add_record(M, v4_ld(L + 0x30), v4_ld(L + 0x10), v4_splat(depth));
        }
        offset += 0x20;
        SU32(L, 0xc, U32(L, 0xc) - 1u);
    } while (U32(L, 0xc) != 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00872970, phys_pair_rays_query)
