/* build_vehicle_proxy.c -- construction unit 6: the physics world's proxy objects (the generic dynamic objects of physics world
 * + 0x1954: 0xe0-byte records, broadphase cursor +0x196c) that the vehicles' collision bodies are. Hand transliteration of
 * 00769160 (the proxy object), 0084fcc0 (the record's mass / inertia from a box), 00844240 / 00844360 (the shape of a collis asset),
 * 007743a0 / 00777190 / 00776630 (the record pushed into the vector), 00774120 (its broadphase insert).
 *
 * Dynamic object record R (0xe0): +0 mass, +0x10 / +0x14 / +0x18 the inertia, +0x20 the PhysicsProperties material (0xd0 record),
 * +0x24 its id, +0x28 CollisionShapeHandle {vtable 0x9765a8, +0x2c the shape}, +0x30 the tracking record {+0 references, +4 the
 * physics world, +8 the index}, +0x34 0, +0x40 position, +0x50 orientation, +0x60.. velocities, +0xd0 0, +0xd4 byte 1 (awake).
 * The record is built on the original's frame and copied (00776630): the words it never writes (+4..+0xc, +0x1c, +0x38 / +0x3c,
 * +0xb0..+0xcf) are the frame's old contents in the original (dead stack; the core's are zero). */
#include "phys_shape_dtors.h"
#include "build_vehicle.h"
#include "build_vehicle_util.h"
#include "build_alloc.h"
#include "build_world.h"
#include "build_collision.h"
#include "phys_coll_grid.h"
#include "phys_coll_shape.h"
#include "build_rider.h"
#include <stdio.h>
#include <stdlib.h>

static void fatal(const char* m) { fprintf(stderr, "rxcore: %s\n", m); abort(); }
static uint32_t dyn_count(rx_addr vec) { return (U32(vec, 0x10) - U32(vec, 0xc)) / 0xe0; }

/* 0084fcc0 (ESI = record, EDI = the box's half extents (x, y, z), s0 = material id; RET 4) */
rx_addr rx_phys_dynamic_record_init(rx_addr r, rx_addr dims, uint32_t material) {
    SU32(r, 0, 0x3f800000u); SU32(r, 0x10, 0x3f800000u); SU32(r, 0x14, 0x3f800000u); SU32(r, 0x18, 0x3f800000u);
    rx_addr reg = rx_phys_props_get();   /* the registry's one-time build (guard 0x00dce7d4) */
    rx_addr mat = U32(reg, 0xc) + 0xd0 * material;
    SU32(r, 0x24, material);
    SU32(r, 0x20, mat);
    SU32(r, 0x28, 0x009765a8);   /* RavenPhysics::CollisionShapeHandle::vftable */
    SU32(r, 0x2c, 0); SU32(r, 0x34, 0);
    SU8(r, 0xd4, 1);
    const float dx = F32(dims, 0), dy = F32(dims, 4), dz = F32(dims, 8);
    float m = rx_muls(rx_muls(rx_muls(rx_muls(dz, dy), dx), 8.0f /* [0x9322b8] */), F32(mat, 0x40));
    SF32(r, 0, m);
    const float k = rx_bf(0x3daaaaabu) /* [0x9776ac] 1/12 */;
    const float x2 = rx_muls(dx, 2.0f), z2 = rx_muls(dz, 2.0f), y2 = rx_muls(dy, 2.0f);
    const float zz = rx_muls(z2, z2), m12 = rx_muls(m, k), yy = rx_muls(y2, y2);
    const float ix = rx_muls(rx_adds(yy, zz), m12);
    const float xx = rx_muls(x2, x2);
    SF32(r, 0x14, rx_muls(rx_adds(xx, zz), rx_muls(F32(r, 0), k)));
    SF32(r, 0x10, ix);
    SF32(r, 0x18, rx_muls(rx_adds(xx, yy), rx_muls(F32(r, 0), k)));
    return r;
}
RX_FUNCTION(0x0084fcc0, rx_phys_dynamic_record_init);

/* 00844360 (cdecl h1, h2) with h1 == h2 (the shape's +0x18 / +0x1c = them): a shape from one collis asset's geometry (the kinds of the jump table 0x8445a0: 11 box,
 * 15 composite, 16 sphere, 17 cylinder, every other kind (0 included) the Unknown shape with radius 1); two handles (a composite of
 * the two geometries) are not ported */
rx_addr rx_shape_from_pair(rx_addr h1, rx_addr h2) {
    if (h1 != h2) fatal("00844360 with two collis handles: not ported");
    SU32(h1, 0x4c, 1);
    rx_addr g = U32(U32(h1, 0x44), 0x10), s;
    switch (U32(g, 4)) {
    case 11: s = rx_reflex_aligned_alloc(0x70); if (s) rx_shape_static_box_construct(s, g); break;           /* 00401f90 */
    case 15: s = rx_reflex_aligned_alloc(0xb0); if (s) rx_shape_static_composite_construct(s, g); break;
    case 16: s = rx_crt_aligned_malloc(0x40, 16); if (s) rx_shape_sphere_construct(s, g); break;           /* 0070cf80 */
    case 17: s = rx_crt_aligned_malloc(0x70, 16); if (s) rx_shape_static_cylinder_construct(s, g); break;  /* 00767f40 */
    default: s = rx_reflex_aligned_alloc(0x30); if (s) rx_shape_unknown_construct(s, 1.0f /* fld1 */); break;
    }
    SU32(s, 0x1c, h1); SU32(s, 0x18, h1);
    SU32(h1, 0x4c, 0);
    return s;
}

/* 00844240 (cdecl out, h1, h2, share): out = CollisionShapeHandle of the shape made from (h1, h2): with share, the first shape of
 * the shape list made from the same pair; else a new one (an Unknown shape of radius 2 when 00844360 makes none) */
rx_addr rx_shape_handle_for(rx_addr out, rx_addr a1, rx_addr a2, int share) {
    if (share) {
        rx_addr found = 0;
        uint32_t n = rx_vec_size(RX_SHAPE_LIST, 4);
        for (uint32_t i = 0; i < n; i++) {
            rx_addr c = U32(U32(RX_SHAPE_LIST, 0xc), 4 * i);
            if (U32(c, 0x18) == a1 && U32(c, 0x1c) == a2) found = c;   /* the last match wins (no break) */
        }
        if (found) { SU32(found, 4, U32(found, 4) + 1); SU32(out, 4, found); SU32(out, 0, 0x009765a8); return out; }
    }
    rx_addr s = rx_shape_from_pair(a1, a2);
    if (!s) {
        s = rx_reflex_aligned_alloc(0x30);
        if (s) rx_shape_unknown_construct(s, 2.0f /* [0x92e330] */);
        SU32(s, 0x1c, a2); SU32(s, 0x18, a1);
    }
    SU32(s, 4, U32(s, 4) + 1);
    SU32(out, 4, s);
    SU32(out, 0, 0x009765a8);
    return out;
}
RX_FUNCTION(0x00844240, rx_shape_handle_for);

/* 00776630 (EAX = destination, ECX = source): the record's copy constructor (+0 through the x87: a signalling NaN is quieted) */
static void dynamic_record_copy(rx_addr d, rx_addr s) {
    SU32(d, 0, veh_x87_copy(U32(s, 0)));
    SU32(d, 0x10, veh_x87_copy(U32(s, 0x10))); SU32(d, 0x14, veh_x87_copy(U32(s, 0x14))); SU32(d, 0x18, veh_x87_copy(U32(s, 0x18)));
    SU32(d, 0x20, U32(s, 0x20)); SU32(d, 0x24, U32(s, 0x24));
    SU32(d, 0x28, 0x009765b4);
    rx_addr sh = U32(s, 0x2c);
    SU32(d, 0x2c, sh);
    if (sh) SU32(sh, 4, U32(sh, 4) + 1);
    SU32(d, 0x28, 0x009765a8);
    SU32(d, 0x30, U32(s, 0x30)); SU32(d, 0x34, U32(s, 0x34));
    veh_copy(d + 0x40, s + 0x40, 0x90);
    SU32(d, 0xd0, U32(s, 0xd0));
    SU8(d, 0xd4, U8(s, 0xd4));
}

/* 0042c080 (EAX = first, s0 = last): the records' destructors (their shape handles released) */
static void dynamic_records_destroy(rx_addr first, rx_addr last) {
    for (rx_addr e = first; e != last; e += 0xe0) {
        SU32(e, 0x28, 0x009765b4);
        rx_addr sh = U32(e, 0x2c);
        rx_collision_shape_release(sh);   /* the handle's release: the count - 1, the destructor (vf0 (1)) at 0 */
    }
}

/* 007743a0 (s0 = the vector (physics world + 0x1954), s1 = &record; RET 8): push_back. Full: 00775400 -> _Insert_n 00775ba0 at the end:
 * capacity + capacity / 2 (0 on overflow; at least size + 1), _aligned_malloc(capacity * 0xe0, 16) (007756b0), the new record
 * copied first (007754c0), then the old ones (00776d80 / 007782e0), the old ones destroyed (0042c080) and their block freed
 * (StdMemoryAllocator::vf2 00775540) */
void rx_phys_dynamic_push(rx_addr vec, rx_addr rec) {
    rx_addr first = U32(vec, 0xc), last = U32(vec, 0x10);
    uint32_t cap = first ? (U32(vec, 0x14) - first) / 0xe0 : 0, n = (last - first) / 0xe0;
    if (n < cap) {
        dynamic_record_copy(last, rec);   /* 00777190 with n = 1 */
        SU32(vec, 0x10, last + 0xe0);
        return;
    }
    if (0x1249249u - n < 1) fatal("007743a0: vector too long");
    uint32_t ncap = 0x1249249u - cap / 2 < cap ? 0 : cap + cap / 2;
    if (ncap < n + 1) ncap = n + 1;
    rx_addr blk = rx_crt_aligned_malloc(ncap * 0xe0, 16);
    dynamic_record_copy(blk + 0xe0 * n, rec);
    for (uint32_t i = 0; i < n; i++) dynamic_record_copy(blk + 0xe0 * i, first + 0xe0 * i);
    if (first) {
        dynamic_records_destroy(first, U32(vec, 0x10));
        rx_std_allocator_free(0, U32(vec, 0xc));
    }
    SU32(vec, 0x14, blk + 0xe0 * ncap);
    SU32(vec, 0x10, blk + 0xe0 * (n + 1));
    SU32(vec, 0xc, blk);
}
RX_FUNCTION(0x007743a0, rx_phys_dynamic_push);

/* 00774120 (ECX = the vector, EDI = index): the object's box (its shape's vf2 at its pose: q = +0x50, pos = +0x40) into the grid
 * of the vector's broadphase cursor (+0x18 -> +0x30), then BroadphaseGrid_Insert */
void rx_phys_dynamic_insert(rx_addr vec, uint32_t index) {
    if (!U32(vec, 0x18)) return;
    rx_addr e = U32(vec, 0xc) + 0xe0 * index;
    rx_addr shape = U32(e, 0x2c);
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x38 - 8;   /* push ebp; and esp, -16; sub esp, 0x38; push ebx; push esi */
    rx_set_sp(F);
    rx_addr mn = F + 0x10, mx = F + 0x20;
    phys_shape_bounds(U32(U32(shape, 0), 8), shape, e + 0x50, e + 0x40, mn, mx);
    rx_addr g = U32(U32(vec, 0x18), 0x30);
    veh_copy(U32(g, 0x41c) + 16 * index, mn, 0x10);
    veh_copy(U32(g, 0x420) + 16 * index, mx, 0x10);
    rx_set_sp(entry_sp);
    phys_grid_insert(g, index);
}
RX_FUNCTION(0x00774120, rx_phys_dynamic_insert);

/* 00769160 (ECX = EDX = the collis handle, s0 = the physics world, s1 = out ProxyObjectHandle, s2 = material id; RET 0xc): a box of
 * half extents (4, 3, 2) gives the record's mass / inertia, the collis asset's shape (unshared, flags |= 8), the tracking record
 * {0, world, index}; the record is pushed and inserted; out = ProxyObjectHandle {0x93a644, the tracking record (references + 1)} */
rx_addr rx_proxy_object_create(rx_addr world, rx_addr out, uint32_t material, rx_addr h1, rx_addr h2) {
    rx_addr vec = world + 0x1954;
    const uint32_t n = dyn_count(vec);
    /* the original's frame (push ebp; and esp, -16; sub esp, 0x124; cookie; push ebx, esi, edi): the record (and its dims) at its
     * offsets (dims +0x30, record +0x40 of the ESP after the pushes) -- the record keeps words the initializers leave (the frame's
     * old contents) */
    const rx_addr sp0 = rx_sp(), E = ((sp0 - 4) & ~15u) - 0x124, F = E - 0xc;
    rx_set_sp(F);
    veh_cookie(E, 0x120);
    rx_addr dims = F + 0x30, rec = F + 0x40, tmp = F + 0x14;
    SU32(dims, 0, 0x40800000u); SU32(dims, 4, 0x40400000u); SU32(dims, 8, 0x40000000u);   /* 4.0, 3.0, 2.0 */
    rx_phys_dynamic_record_init(rec, dims, material);
    SU32(rec, 0xd0, 0);
    veh_v4(rec + 0x40, VEH_RT_ZERO); veh_v4(rec + 0x50, VEH_RT_W1);
    for (uint32_t o = 0x60; o <= 0xa0; o += 0x10) veh_v4(rec + o, VEH_RT_ZERO);
    rx_shape_handle_for(tmp, h1, h2, 0);   /* a1 = the ECX handle, a2 = the EDX handle */
    rx_addr cur = U32(rec, 0x2c), s = U32(tmp, 4);
    if (s != cur) {
        rx_collision_shape_release(cur);
        SU32(rec, 0x2c, s);
        if (s) SU32(s, 4, U32(s, 4) + 1);
    }
    /* the temporary handle released */
    SU32(tmp, 0, 0x009765b4);
    rx_collision_shape_release(U32(tmp, 4));
    SU32(U32(rec, 0x2c), 0x10, U32(U32(rec, 0x2c), 0x10) | 8);
    rx_addr t = rx_reflex_aligned_alloc(0xc);
    if (t) { SU32(t, 0, 0); SU32(t, 4, world); SU32(t, 8, n); }
    SU32(rec, 0x30, t);
    const uint32_t idx = dyn_count(vec);
    rx_phys_dynamic_push(vec, rec);
    rx_phys_dynamic_insert(vec, idx);
    SU32(t, 4, U32(vec, 0x1c));
    SU32(t, 8, idx);
    SU32(out, 0, 0x0093a650);
    SU32(out, 4, t);
    SU32(t, 0, U32(t, 0) + 1);
    SU32(out, 0, 0x0093a644);   /* RavenPhysics::ProxyObjectHandle::vftable */
    rx_addr sh = U32(rec, 0x2c);
    if (sh) { SU32(sh, 4, U32(sh, 4) - 1); if (!U32(sh, 4)) fatal("00769160: shape freed"); }
    rx_set_sp(sp0);
    return out;
}

/* the registry: the transliterations above that are whole original functions (their conventions in the comments) */
RX_FUNCTION(0x00776630, dynamic_record_copy);
RX_FUNCTION(0x0042c080, dynamic_records_destroy);
RX_FUNCTION(0x00844360, rx_shape_from_pair);
RX_FUNCTION(0x00769160, rx_proxy_object_create);
