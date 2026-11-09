#include "../src/fmt_modtrack.h"
#include "map_scale.h"
#include <math.h>
/* build_collision.c -- see build_collision.h. Hand transliteration from the disassembly of 00819130 (collision part), 00819650,
 * 00819510, 007693c0, 00769550, 0084fc20, 00774a90 / 00777310, 00774850, 008445e0, 008446e0, 008449d0, 00844930, 00846320,
 * 008453f0, 00847790 / 00847b00, 00847bd0, 00848cd0 (+ the vector copies 004f0220 / 008491e0 / 00849340 / 008499d0), 00844d30,
 * 00847f60, the entity's vectors 0081f4f0 / 0081f740 / 0081f850 / 0081f610, the handle release 00768420, and the TEMP 00818f60.
 * SSE single precision in the original's operation order; the quaternion products are phys_coll_quat.h's (the exe's SIMD form). */
#include "phys_shape_dtors.h"
#include "phys_static_objects.h"
#include "build_collision.h"
#include "build_alloc.h"
#include "build_world.h"
#include "build_scene_int.h"
#include "build_terrain.h"
#include "phys_coll_quat.h"
#include "phys_coll_shape.h"
#include "phys_coll_grid.h"
#include "world_veh.h"
#include "../src/fmt_scene.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void fatal(const char* what) { fprintf(stderr, "rxcore: build_collision: %s\n", what); abort(); }
static void st_splat(rx_addr a, float v) { for (uint32_t k = 0; k < 16; k += 4) SF32(a, k, v); }
static void copy_words(rx_addr dst, rx_addr src, uint32_t n) { for (uint32_t k = 0; k < n; k += 4) SU32(dst, k, U32(src, k)); }

/* a shape's reference dropped (the IntrusiveHandle release: vtable slot 0 = scalar deleting destructor at 0) */
static void shape_release(rx_addr s) {
    rx_collision_shape_release(s);
}
/* 00768420 (EAX = StaticObjectHandle): vtable 0x0096ddc4, the record's reference dropped (the last one removes the static object: phys_static_objects.c) */
static void static_handle_release(rx_addr h) {
    rx_addr r = U32(h, 4);
    SU32(h, 0, 0x0096ddc4);
    if (r) { SU32(r, 0, U32(r, 0) - 1); if (!U32(r, 0) && U32(h, 4)) rx_static_record_free(U32(h, 4)); }
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * shapes */

/* 0070cf80: _aligned_malloc(0x40, 16) */
static rx_addr alloc_shape40(void) { return rx_crt_aligned_malloc(0x40, 16); }

/* 008449d0 */
rx_addr rx_shape_triangles_construct(rx_addr s, rx_addr tris) {
    rx_collision_shape_construct(s, 0x10);
    SU32(s, 0, 0x00976608);   /* RavenPhysics::CollisionShapeTriangles::vftable */
    SU32(s, 0xc, 0x11);
    st_splat(s + 0x30, F32(tris, 0x14));
    SU32(s, 0x20, U32(tris, 0x10));
    SU32(s, 0x24, rx_crt_aligned_malloc(rx_mul_sat(U32(tris, 0x10) * 3, 0x10), 16));
    for (int32_t i = 0; i < I32(s, 0x20); i++) copy_words(U32(s, 0x24) + 0x30u * (uint32_t)i, U32(tris, 0xc) + 0x30u * (uint32_t)i, 0x30);
    return s;
}
/* 00844930 */
rx_addr rx_shape_unknown_construct(rx_addr s, float radius) {
    rx_collision_shape_construct(s, 1);
    SU32(s, 0, 0x009765c0);   /* RavenPhysics::CollisionShapeUnknown::vftable */
    st_splat(s + 0x20, radius);
    SU32(s, 0xc, 0x83ff);
    return s;
}
/* 00846320 */
rx_addr rx_shape_static_box_construct(rx_addr s, rx_addr g) {
    uint32_t ext[4]; for (int k = 0; k < 4; k++) ext[k] = U32(g, 0x50 + 4u * (uint32_t)k);
    rx_collision_shape_construct(s, 4);
    for (int k = 0; k < 4; k++) SU32(s, 0x20 + 4u * (uint32_t)k, ext[k]);
    SU32(s, 0, 0x009766bc);   /* CollisionShapeStaticBox::vftable */
    SU32(s, 0x30, U32(g, 0x40));
    copy_words(s + 0x40, g + 0x10, 0x30);
    return s;
}
/* 008453f0 */
rx_addr rx_shape_sphere_construct(rx_addr s, rx_addr g) {
    rx_collision_shape_construct(s, 2);
    SU32(s, 0, 0x00976674);   /* CollisionShapeSphere::vftable */
    st_splat(s + 0x20, 1.0f /* 0x931f24 */);
    if (g) st_splat(s + 0x20, F32(g, 0xc));
    return s;
}
/* 00847b00 (ESI = shape, xmm0 = radius, xmm1 = half height): the cylinder's class and vectors */
static void cylinder_class(rx_addr s, float r, float h) {
    const float half = 0.5f /* 0x9322cc */, zero = 0.0f /* 0x92e12c */;
    float ratio = rx_divs(h, r);
    if (half > ratio) {                                  /* comiss 0.5, ratio; jbe skips (unordered too) */
        float p = rx_muls(r, 1.41499996f /* 0x9776b0 */);
        SF32(s, 0x50, p); SF32(s, 0x54, zero); SF32(s, 0x58, zero); SF32(s, 0x5c, zero);
        SU32(s, 0x30, 3);
        SF32(s, 0x60, zero); SF32(s, 0x64, zero); SF32(s, 0x68, p); SF32(s, 0x6c, p);
        st_splat(s + 0x40, rx_muls(h, half));
        return;
    }
    if (ratio > 2.0f /* 0x92e330 */) {
        float hh = rx_muls(h, half);
        SU32(s, 0x30, 2);
        st_splat(s + 0x40, r);
        SF32(s, 0x50, zero); SF32(s, 0x54, hh); SF32(s, 0x58, zero); SF32(s, 0x5c, zero);
        return;
    }
    float d = rx_adds(rx_muls(r, r), rx_muls(h, h));
    SU32(s, 0x30, 1);
    st_splat(s + 0x40, rx_sqrts(d));                    /* SqrtF 0042c230 (sqrtss) */
}
/* 00847790 */
rx_addr rx_shape_static_cylinder_construct(rx_addr s, rx_addr g) {
    float r = F32(g, 0x44), h = F32(g, 0x48);
    rx_collision_shape_construct(s, 8);
    SF32(s, 0x20, r); SF32(s, 0x24, h);
    SU32(s, 0, 0x00976728);   /* CollisionShapeStaticCylinder::vftable */
    cylinder_class(s, F32(g, 0x44), F32(g, 0x48));
    return s;
}

/* 00847bd0 (EAX = shape): the composite base: four std::vectors (children: ReflexAlignedAlloc proxy; rotations / offsets /
 * positions: _aligned_malloc(4, 4) proxies) */
static void composite_base_construct(rx_addr s) {
    rx_collision_shape_construct(s, 0x8000);
    SU32(s, 0, 0x00976774);   /* CollisionShapeComposite::vftable */
    rx_vec_proxy_init16(s + 0x20);
    SU32(s, 0x2c, 0); SU32(s, 0x30, 0); SU32(s, 0x34, 0);
    for (uint32_t v = 0x38; v <= 0x68; v += 0x18) { rx_vec_proxy_init(s + v); SU32(s, v + 0xc, 0); SU32(s, v + 0x10, 0); SU32(s, v + 0x14, 0); }
}
/* 004f0220 (stdcall dst, src): copy construction of a vector of bytes (allocator<char>::allocate = operator new) */
static void vec_copy_bytes(rx_addr dst, rx_addr src) {
    rx_vec_proxy_init16(dst);
    uint32_t n = U32(src, 0x10) - U32(src, 0xc);
    SU32(dst, 0xc, 0); SU32(dst, 0x10, 0); SU32(dst, 0x14, 0);
    if (!n) return;
    rx_addr b = rx_crt_malloc(n);
    SU32(dst, 0xc, b); SU32(dst, 0x10, b); SU32(dst, 0x14, b + n);
    for (uint32_t k = 0; k < n; k++) SU8(b, k, U8(U32(src, 0xc), k));   /* memmove_s */
    SU32(dst, 0x10, b + n);
}
/* 008491e0 (stdcall dst, src): copy construction of a vector of 4-byte elements (ReflexAlignedAlloc) */
static void vec_copy_u32(rx_addr dst, rx_addr src) {
    rx_vec_proxy_init16(dst);
    uint32_t n = (U32(src, 0x10) - U32(src, 0xc)) >> 2;
    SU32(dst, 0xc, 0); SU32(dst, 0x10, 0); SU32(dst, 0x14, 0);
    if (!n) return;
    rx_addr b = rx_reflex_aligned_alloc(n * 4);
    SU32(dst, 0xc, b); SU32(dst, 0x10, b); SU32(dst, 0x14, b + n * 4);
    for (uint32_t k = 0; k < n * 4; k += 4) SU32(b, k, U32(U32(src, 0xc), k));
    SU32(dst, 0x10, b + n * 4);
}
/* 00849340 (EAX = dst, ESI = src): assignment of a vector of 16-byte elements to an EMPTY vector (the path taken at construction:
 * the old block freed, 00820520 allocates exactly n (_aligned_malloc(n * 16, 16)), 00849fd0 copies) */
static void vec_assign_v4(rx_addr dst, rx_addr src) {
    if (dst == src) return;
    uint32_t n = (U32(src, 0x10) - U32(src, 0xc)) >> 4;
    if (!n) { if (U32(dst, 0x10) != U32(dst, 0xc)) fatal("vector clear of a non-empty vector"); return; }
    if (U32(dst, 0x10) != U32(dst, 0xc)) fatal("vector assignment to a non-empty vector");
    if (U32(dst, 0xc)) rx_crt_aligned_free(U32(dst, 0xc));
    SU32(dst, 0xc, 0); SU32(dst, 0x10, 0); SU32(dst, 0x14, 0);
    rx_addr b = rx_crt_aligned_malloc(n * 16, 16);
    SU32(dst, 0xc, b); SU32(dst, 0x10, b); SU32(dst, 0x14, b + n * 16);
    for (uint32_t k = 0; k < n * 16; k += 4) SU32(b, k, U32(U32(src, 0xc), k));
    SU32(dst, 0x10, b + n * 16);
}
/* 008499d0 (EAX = n, EDI = vector): resize of an empty vector of pointers to n zeros (_Insert_n: exactly n, ReflexAlignedAlloc) */
static void vec_resize_u32(rx_addr v, uint32_t n) {
    if (U32(v, 0x10) != U32(v, 0xc)) fatal("resize of a non-empty vector");
    if (!n) return;
    rx_addr b = rx_reflex_aligned_alloc(n * 4);
    for (uint32_t k = 0; k < n * 4; k += 4) SU32(b, k, 0);
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, b + n * 4); SU32(v, 0x10, b + n * 4); SU32(v, 0xc, b);
}

/* a composite's child shape from its geometry (the switch of 00848cd0: kinds 0 / 2 / 11 / 16 / 17 / 18) */
static rx_addr composite_child(rx_addr g) {
    rx_addr m;
    switch (U32(g, 4)) {
    case 0:  m = alloc_shape40(); return m ? rx_shape_triangles_construct(m, g) : 0;
    case 2:  fatal("composite child of kind 2 (FUN_00844ea0) not ported"); return 0;
    case 11: m = rx_reflex_aligned_alloc(0x70); return m ? rx_shape_static_box_construct(m, g) : 0;   /* FUN_00401f90 */
    case 16: m = alloc_shape40(); return m ? rx_shape_sphere_construct(m, g) : 0;
    case 17: m = rx_crt_aligned_malloc(0x70, 16); return m ? rx_shape_static_cylinder_construct(m, g) : 0;   /* 00767f40 */
    default: m = rx_reflex_aligned_alloc(0x30); return m ? rx_shape_unknown_construct(m, 1.0f) : 0;
    }
}
/* 00848cd0 */
rx_addr rx_shape_static_composite_construct(rx_addr s, rx_addr g) {
    composite_base_construct(s);
    SU32(s, 0, 0x009767a0);   /* CollisionShapeStaticComposite::vftable */
    vec_copy_bytes(s + 0x80, g + 0xc);
    vec_copy_u32(s + 0x98, g + 0x24);
    vec_assign_v4(s + 0x38, g + 0x6c);
    vec_assign_v4(s + 0x50, g + 0x84);
    vec_assign_v4(s + 0x68, g + 0x84);
    vec_resize_u32(s + 0x20, (U32(g, 0x4c) - U32(g, 0x48)) >> 2);
    for (uint32_t i = 0; i < (U32(g, 0x4c) - U32(g, 0x48)) >> 2; i++)
        SU32(U32(s, 0x2c), 4 * i, composite_child(U32(U32(g, 0x48), 4 * i)));
    return s;
}

/* 008446e0 (ESI = collis handle) */
rx_addr rx_shape_from_handle(rx_addr h) {
    if (U32(h, 0) == 0x0096dd90) fatal("SplitTriangleCollisionAssetHandle (FUN_00844ae0) not ported");   /* __RTDynamicCast */
    rx_addr g = U32(U32(h, 0x44), 0x10), s;
    SU32(h, 0x4c, 1);
    switch (U32(g, 4)) {
    case 0:  s = alloc_shape40(); if (s) rx_shape_triangles_construct(s, g); break;
    case 11: s = rx_reflex_aligned_alloc(0x70); if (s) rx_shape_static_box_construct(s, g); break;
    case 15: s = rx_reflex_aligned_alloc(0xb0); if (s) rx_shape_static_composite_construct(s, g); break;
    case 16: s = alloc_shape40(); if (s) rx_shape_sphere_construct(s, g); break;
    case 17: s = rx_crt_aligned_malloc(0x70, 16); if (s) rx_shape_static_cylinder_construct(s, g); break;
    default: s = rx_reflex_aligned_alloc(0x30); if (s) rx_shape_unknown_construct(s, 1.0f); break;   /* kinds 1, 2, 6, 12, 13, 18, > 18 */
    }
    SU32(h, 0x4c, 0);
    SU32(s, 0x1c, h);
    SU32(s, 0x18, h);
    return s;
}

/* 008445e0 */
rx_addr rx_shape_handle_get(rx_addr out, rx_addr h, int share) {
    rx_addr s = 0;
    if (share) {   /* the shape list (lock 0x00d6d8f8): the first shape made from this handle */
        uint32_t n = rx_vec_size(RX_SHAPE_LIST, 4);
        for (uint32_t i = 0; i < n; i++) { rx_addr c = U32(U32(RX_SHAPE_LIST, 0xc), 4 * i); if (U32(c, 0x1c) == h) { s = c; break; } }
        if (s) { SU32(s, 4, U32(s, 4) + 1); SU32(out, 4, s); SU32(out, 0, 0x009765a8); return out; }
    }
    s = rx_shape_from_handle(h);
    if (!s) {
        s = rx_reflex_aligned_alloc(0x30);
        if (s) rx_shape_unknown_construct(s, 2.0f /* 0x92e330 */);
        SU32(s, 0x1c, h); SU32(s, 0x18, 0);
    }
    SU32(s, 4, U32(s, 4) + 1);
    SU32(out, 4, s);
    SU32(out, 0, 0x009765a8);   /* RavenPhysics::CollisionShapeHandle::vftable */
    return out;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * bounds */

/* 00844d30: every vertex (w cleared) rotated q * v * conj(q), min / max over them (FLT_MAX starts when there are triangles, the
 * runtime zero vector otherwise), then + pos */
void rx_shape_triangles_bounds(rx_addr self, rx_addr q_p, rx_addr pos_p, rx_addr mn, rx_addr mx) {
    v4_st(mn, v4_ld(0x00d61670));
    v4_st(mx, v4_ld(0x00d61670));
    if (I32(self, 0x20) > 0) {
        v4_st(mn, v4_splat(rx_bf(0x7f7fffffu)) /* 0x977494 */);
        v4_st(mx, v4_splat(rx_bf(0xff7fffffu)) /* 0x977550 */);
        int32_t nv = I32(self, 0x20) * 3;
        for (int32_t k = 0; k < nv; k++) {
            const rx_v4 q = v4_ld(q_p);
            const rx_v4 v = v4_xyz(v4_ld(U32(self, 0x24) + 0x10u * (uint32_t)k));
            const rx_v4 r = coll_quat_mul(q, coll_quat_mul(v, v4_mul(q, COLL_QCONJ)));
            v4_st(mn, v4_min(v4_ld(mn), r));
            v4_st(mx, v4_max(v4_ld(mx), r));
        }
    }
    v4_st(mn, v4_add(v4_ld(pos_p), v4_ld(mn)));
    v4_st(mx, v4_add(v4_ld(mx), v4_ld(pos_p)));
}
RX_FUNCTION(0x00844d30, rx_shape_triangles_bounds)

/* 00847f60 (the composite's bounds): phys_composite_vf2 of the race port (phys_coll_shape2.c), reached through the vtable */

/* ---------------------------------------------------------------------------------------------------------------------------
 * static objects */

/* 0084fc20 */
void rx_static_record_init(rx_addr rec, uint32_t material, rx_addr m) {
    SU32(rec, 0x20, material);
    SU32(rec, 0x28, 0);
    SU32(rec, 0x24, 0x009765a8);
    SU32(rec, 0x2c, 0); SU32(rec, 0x30, 0); SU32(rec, 0x34, 0);
    SU8(rec, 0x38, 1);
    RX_FRAME_ENTER(F, 0x10);
    rx_world_matrix3_to_quaternion_to(F, m);   /* Matrix3_ToQuaternionToEsi 00401740 */
    const uint32_t x = U32(m, 0x30), y = U32(m, 0x34), z = U32(m, 0x38);
    SU32(rec, 0, x); SU32(rec, 4, y); SU32(rec, 8, z); SU32(rec, 0xc, z);
    copy_words(rec + 0x10, F, 0x10);
    RX_FRAME_LEAVE(F, 0x10);
}

/* 00777310 (ECX = source record, stack dest, n = 1): the copy constructor of a static object record */
static void static_record_copy(rx_addr d, rx_addr s) {
    copy_words(d, s, 0x20);
    SU32(d, 0x20, U32(s, 0x20));
    SU32(d, 0x24, 0x009765b4);
    SU32(d, 0x28, U32(s, 0x28));
    if (U32(s, 0x28)) SU32(U32(s, 0x28), 4, U32(U32(s, 0x28), 4) + 1);
    SU32(d, 0x24, 0x009765a8);
    SU32(d, 0x2c, U32(s, 0x2c)); SU32(d, 0x30, U32(s, 0x30)); SU32(d, 0x34, U32(s, 0x34));
    SU8(d, 0x38, U8(s, 0x38));
}
/* 00774a90 */
void rx_static_vector_push(rx_addr v, rx_addr rec) {
    rx_addr first = U32(v, 0xc), last = U32(v, 0x10);
    uint32_t cap = first ? (U32(v, 0x14) - first) >> 6 : 0;
    if ((last - first) >> 6 >= cap) fatal("the static object vector is full (FUN_00775720 not ported)");
    static_record_copy(last, rec);
    SU32(v, 0x10, last + 0x40);
}
/* 00774850 */
void rx_static_broadphase_insert(rx_addr v, uint32_t index) {
    if (!U32(v, 0x18)) return;
    rx_addr e = U32(v, 0xc) + (index << 6);
    rx_addr shape = U32(e, 0x28);
    const rx_addr entry_sp = rx_sp();
    const rx_addr F = ((entry_sp - 4) & ~15u) - 0x38 - 8;   /* push ebp; and esp, -16; sub esp, 0x38; push ebx; push esi */
    rx_set_sp(F);
    rx_addr mn = F + 0x10, mx = F + 0x20;
    phys_shape_bounds(U32(U32(shape, 0), 8), shape, e + 0x10, e, mn, mx);   /* the shape's vf2 at its pose */
    rx_addr g = U32(U32(v, 0x18), 0x30);
    copy_words(U32(g, 0x41c) + (index << 4), mn, 0x10);
    copy_words(U32(g, 0x420) + (index << 4), mx, 0x10);
    rx_set_sp(entry_sp);
    phys_grid_insert(U32(U32(v, 0x18), 0x30), index);                       /* BroadphaseGrid_Insert 008826d0 */
}

/* 00769550 */
rx_addr rx_static_add_shape(rx_addr w, rx_addr out, rx_addr m, rx_addr sh, int drive_on, uint32_t material) {
    rx_addr v = w + 0x19b4;
    uint32_t index = (U32(w, 0x19c4) - U32(w, 0x19c0)) >> 6;
    RX_FRAME_ENTER(F, 0x40);
    rx_addr rec = F;
    rx_static_record_init(rec, material, m);
    rx_addr r = rx_reflex_aligned_alloc(0xc);
    if (r) { SU32(r, 0, 0); SU32(r, 4, w); SU32(r, 8, index); }
    SU32(rec, 0x2c, r);
    if (U32(sh, 4) != U32(rec, 0x28)) {
        shape_release(U32(rec, 0x28));
        SU32(rec, 0x28, U32(sh, 4));
        if (U32(rec, 0x28)) SU32(U32(rec, 0x28), 4, U32(U32(rec, 0x28), 4) + 1);
    }
    rx_addr s = U32(rec, 0x28);
    if (drive_on) SU32(s, 0x10, 4);
    SU32(s, 0x10, U32(s, 0x10) | 2);
    uint32_t count = (U32(v, 0x10) - U32(v, 0xc)) >> 6;
    rx_static_vector_push(v, rec);
    rx_static_broadphase_insert(v, count);
    SU32(U32(rec, 0x2c), 8, count);
    SU32(U32(rec, 0x2c), 4, U32(v, 0x1c));
    SU32(out, 0, 0x0096ddc4);
    SU32(out, 4, U32(rec, 0x2c));
    SU32(U32(rec, 0x2c), 0, U32(U32(rec, 0x2c), 0) + 1);
    SU32(out, 0, 0x0096ddb8);   /* RavenPhysics::StaticObjectHandle::vftable */
    shape_release(U32(rec, 0x28));
    RX_FRAME_LEAVE(F, 0x40);
    return out;
}

/* 007693c0 */
rx_addr rx_static_add_asset(rx_addr w, rx_addr out, rx_addr m, int drive_on, int share, rx_addr h, uint32_t material) {
    rx_addr v = w + 0x19b4;
    uint32_t index = (U32(w, 0x19c4) - U32(w, 0x19c0)) >> 6;
    RX_FRAME_ENTER(F, 0x50);
    rx_addr rec = F, tmp = F + 0x40;
    rx_static_record_init(rec, material, m);
    rx_addr r = rx_reflex_aligned_alloc(0xc);
    if (r) { SU32(r, 0, 0); SU32(r, 4, w); SU32(r, 8, index); }
    SU32(rec, 0x2c, r);
    rx_shape_handle_get(tmp, h, share);
    if (U32(tmp, 4) != U32(rec, 0x28)) {
        shape_release(U32(rec, 0x28));
        SU32(rec, 0x28, U32(tmp, 4));
        if (U32(rec, 0x28)) SU32(U32(rec, 0x28), 4, U32(U32(rec, 0x28), 4) + 1);
    }
    SU32(tmp, 0, 0x009765b4);
    shape_release(U32(tmp, 4));
    rx_addr s = U32(rec, 0x28);
    if (drive_on) SU32(s, 0x10, 4);
    SU32(s, 0x10, U32(s, 0x10) | 2);
    uint32_t count = (U32(v, 0x10) - U32(v, 0xc)) >> 6;
    rx_static_vector_push(v, rec);
    rx_static_broadphase_insert(v, count);
    SU32(U32(rec, 0x2c), 8, count);
    SU32(U32(rec, 0x2c), 4, U32(v, 0x1c));
    SU32(out, 0, 0x0096ddc4);
    SU32(out, 4, U32(rec, 0x2c));
    SU32(U32(rec, 0x2c), 0, U32(U32(rec, 0x2c), 0) + 1);
    SU32(out, 0, 0x0096ddb8);
    shape_release(U32(rec, 0x28));
    RX_FRAME_LEAVE(F, 0x50);
    return out;
}

/* ---------------------------------------------------------------------------------------------------------------------------
 * the CollisionEntity */

/* TEMP until scene unit: 00818f60 with 00401db0 (Entity / ModelEntity base; its model instance FUN_007f11f0 not built) */
rx_addr rx_collision_entity_construct_temp(rx_addr e) {
    SU32(e, 0, 0x0095a2e4); SU32(e, 0x18, 0); SU32(e, 0x10, 0); SU32(e, 0x70, 0);
    SF32(e, 0x24, 0.0f); SF32(e, 0x20, 0.0f);
    copy_words(e + 0x30, 0x009cb510, 0x40);
    SU32(e, 0x1c, 0); SU32(e, 0x7c, 0); SU32(e, 0x74, 0); SU32(e, 0x28, 0); SU32(e, 0x2c, 0); SF32(e, 0x78, 0.0f);
    SU32(e, 0x90, 0x0093708c); SU32(e, 0x94, 0xf0); SU32(e, 0, 0x009370d4); SU32(e, 0x90, 0x0093714c);
    SU32(e, 0, 0x00974f84);    /* CollisionEntity::vftable */
    SU32(e, 0x90, 0x00974ffc);
    SU32(e, 0x148, 0); SU32(e, 0x14c, 0); SU32(e, 0x150, 5); SU32(e, 0x160, 0);
    SU32(e, 0x15c, 0x0096ddb8);
    rx_vec_proxy_init16(e + 0x164); SU32(e, 0x170, 0); SU32(e, 0x174, 0); SU32(e, 0x178, 0);
    rx_vec_proxy_init(e + 0x17c); SU32(e, 0x188, 0); SU32(e, 0x18c, 0); SU32(e, 0x190, 0);
    SU8(e, 0x194, 0); SU32(e, 0x70, 0); SU8(e, 0x19c, 1);
    return e;
}

/* 0081f4f0 (stdcall vector, n): reserve n StaticObjectHandles (8 bytes, 00821580 = ReflexAlignedAlloc) of an empty vector */
static void handles_reserve(rx_addr v, uint32_t n) {
    uint32_t cap = U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 3 : 0;
    if (cap >= n) return;
    if (U32(v, 0x10) != U32(v, 0xc)) fatal("reserve of a non-empty handle vector");
    rx_addr b = rx_reflex_aligned_alloc(rx_mul_sat(n, 8));
    if (U32(v, 0xc)) rx_reflex_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, b + n * 8); SU32(v, 0x10, b); SU32(v, 0xc, b);
}
/* 0081f740 (ECX = vector, n): reserve n matrices (0x40, 00820ee0 = _aligned_malloc(n * 64, 16)) */
static void matrices_reserve(rx_addr v, uint32_t n) {
    uint32_t cap = U32(v, 0xc) ? (U32(v, 0x14) - U32(v, 0xc)) >> 6 : 0;
    if (cap >= n) return;
    if (U32(v, 0x10) != U32(v, 0xc)) fatal("reserve of a non-empty matrix vector");
    rx_addr b = rx_crt_aligned_malloc(n << 6, 16);
    if (U32(v, 0xc)) rx_crt_aligned_free(U32(v, 0xc));
    SU32(v, 0x14, b + (n << 6)); SU32(v, 0x10, b); SU32(v, 0xc, b);
}
/* 0081f850 (EAX = vector, matrix*): push_back within the capacity (00821690 copies 16 dwords) */
static void matrices_push(rx_addr v, rx_addr m) {
    rx_addr last = U32(v, 0x10);
    if (last >= U32(v, 0x14)) fatal("matrix vector full (FUN_0081ffb0 not ported)");
    copy_words(last, m, 0x40);
    SU32(v, 0x10, last + 0x40);
}
/* 0081f610 (EDI = vector, handle*): push_back of a StaticObjectHandle copy (008215e0: vtable, record, record refcount + 1) */
static void handles_push(rx_addr v, rx_addr h) {
    rx_addr last = U32(v, 0x10);
    if (last >= U32(v, 0x14)) fatal("handle vector full (FUN_0081fda0 not ported)");
    SU32(last, 0, 0x0096ddc4);
    SU32(last, 4, U32(h, 4));
    if (U32(h, 4)) SU32(U32(h, 4), 0, U32(U32(h, 4), 0) + 1);
    SU32(last, 0, 0x0096ddb8);
    SU32(v, 0x10, last + 8);
}

/* vf28 00819650 */
void rx_collision_entity_add_to_world(rx_addr e, rx_addr w) {
    rx_addr h = U32(e, 0x144);
    SU32(h, 0x4c, 1);
    rx_addr g = U32(U32(h, 0x44), 0x10);
    if (U32(g, 4) == 0) fatal("collision geometry kind 0 (FUN_00771130 split triangles) not ported");
    SU32(U32(e, 0x144), 0x4c, 1);
    g = U32(U32(U32(e, 0x144), 0x44), 0x10);
    if (g && U32(g, 4) == 0x16) {
        uint32_t n = U32(g, 0x14);
        handles_reserve(e + 0x164, n);
        matrices_reserve(e + 0x17c, n);
        RX_FRAME_ENTER(F, 0x58);
        rx_addr L = F, sh = F + 0x40, out = F + 0x48;
        const rx_addr M = e + 0x30;
        for (uint32_t i = 0; i < n; i++) {
            rx_addr c = U32(g, 0xc) + 0x10 * i;
            const float cx = F32(c, 0), cy = F32(c, 4), cz = F32(c, 8);
            float x = (F32(M, 0x20) * cz + F32(M, 0x10) * cy) + F32(M, 0x00) * cx;
            float y = (F32(M, 0x24) * cz + F32(M, 0x14) * cy) + F32(M, 0x04) * cx;
            float z = (F32(M, 0x28) * cz + F32(M, 0x18) * cy) + F32(M, 0x08) * cx;
            copy_words(L, M, 0x40);
            SF32(L, 0x30, x + F32(L, 0x30));
            SF32(L, 0x34, y + F32(L, 0x34));
            SF32(L, 0x38, z + F32(L, 0x38));
            matrices_push(e + 0x17c, L);
            rx_addr mem = alloc_shape40();
            rx_addr s = mem ? rx_shape_triangles_construct(mem, U32(U32(g, 0x10), 4 * i)) : 0;
            SU32(s, 4, U32(s, 4) + 1);
            SU32(sh, 4, s);
            SU32(sh, 0, 0x009765a8);
            rx_static_add_shape(w, out, L, sh, U8(e, 0x19c), U32(e, 0x198));
            if (U32(out, 4)) {
                rx_addr r = U32(out, 4), wv = U32(r, 4) + 0x19b4;
                SU32(U32(wv, 0xc) + (U32(r, 8) << 6), 0x30, e);
            }
            handles_push(e + 0x164, out);
            static_handle_release(out);
            SU32(sh, 0, 0x009765b4);
            shape_release(U32(sh, 4));
        }
        RX_FRAME_LEAVE(F, 0x58);
    } else {
        RX_FRAME_ENTER(F, 8);
        rx_static_add_asset(w, F, e + 0x30, U8(e, 0x19c), 1, U32(e, 0x144), U32(e, 0x198));
        if (U32(F, 4) != U32(e, 0x160)) {
            rx_addr old = U32(e, 0x160);
            if (old) { SU32(old, 0, U32(old, 0) - 1); if (!U32(old, 0)) rx_static_record_free(old); }
            SU32(e, 0x160, U32(F, 4));
            if (U32(e, 0x160)) SU32(U32(e, 0x160), 0, U32(U32(e, 0x160), 0) + 1);
        }
        static_handle_release(F);
        RX_FRAME_LEAVE(F, 8);
        rx_addr r = U32(e, 0x160);
        if (r) SU32(U32(U32(r, 4) + 0x19b4, 0xc) + (U32(r, 8) << 6), 0x30, e);
    }
    SU32(U32(e, 0x144), 0x4c, 0);
}
static void vf28_entry(rx_addr e, rx_addr w) { rx_collision_entity_add_to_world(e, w); }
RX_FUNCTION(0x00819650, vf28_entry)

/* 00819510: the drive-on flag (shape +0x10 bit 2) of every static object of the entity */
void rx_collision_entity_set_drive_on(rx_addr e) {
    rx_addr r = U32(e, 0x160);
    if (r) {
        rx_addr sh = U32(U32(U32(r, 4), 0x19c0) + (U32(r, 8) << 6), 0x28);
        SU32(sh, 0x10, U8(e, 0x19c) ? U32(sh, 0x10) | 4 : U32(sh, 0x10) & ~4u);
    }
    uint32_t n = (U32(e, 0x174) - U32(e, 0x170)) >> 3;
    for (uint32_t i = 0; i < n; i++) {
        rx_addr q = U32(U32(e, 0x170) + 8 * i, 4);
        if (!q) continue;
        rx_addr sh = U32(U32(U32(q, 4), 0x19c0) + (U32(q, 8) << 6), 0x28);
        SU32(sh, 0x10, U8(e, 0x19c) ? U32(sh, 0x10) | 4 : U32(sh, 0x10) & ~4u);
    }
}

/* the "Enum" child's text of a field (the XMLReader's node value, reader vf 0xe0) */
static const char* field_enum_text(const struct fmt_entity* rs, const char* field) {
    int n = fmt_entity_field_tagged(rs, field, "Enum");
    if (n < 0) return NULL;
    fmt_bxml_value v;
    if (fmt_bxml_node_value(rs->doc, n, &v) != 0 || v.kind != FMT_BX_STRING) return NULL;
    return v.str;
}

/* 00819130 after ModelInstance_Init(+0xa0) and Entity::vf5 */
void rx_collision_entity_load(rx_addr e, const struct fmt_entity* rs) {
    copy_words(e + 0xc0, e + 0x30, 0x40);
    SF32(e, 0x3c, 0.0f); SF32(e, 0x4c, 0.0f); SF32(e, 0x5c, 0.0f); SF32(e, 0x6c, 1.0f);
    for (uint32_t r = 0; r < 0x30; r += 0x10) {   /* each rotation row scaled by 1 / sqrt((z*z + y*y) + x*x) */
        rx_addr row = e + 0x30 + r;
        float x = F32(row, 0), y = F32(row, 4), z = F32(row, 8);
        float l2 = rx_adds(rx_adds(rx_muls(z, z), rx_muls(y, y)), rx_muls(x, x));
        if (l2 > 0.0f) {
            float inv = rx_divs(1.0f, rx_sqrts(rx_bf(rx_fb(l2))));   /* fld / fstp (copy) then SqrtF */
            x = rx_muls(inv, x); y = rx_muls(inv, y); z = rx_muls(inv, z);
        }
        SF32(row, 0, x); SF32(row, 4, y); SF32(row, 8, z);
    }
    SU32(e, 0x144, rx_rs_asset_handle(rs, "FemaleCollisionAsset"));
    SU32(e, 0x140, rx_rs_asset_handle(rs, "MaleCollisionAsset"));
    if (fmt_entity_field_tagged(rs, "CollisionAudioMaterial", "EnumValue") >= 0) SU32(e, 0x14c, (uint32_t)rx_rs_enum(rs, "CollisionAudioMaterial"));
    else SU32(e, 0x14c, 0);
    if (fmt_entity_field_tagged(rs, "CollisionAudioHardness", "UInt") >= 0) {
        uint32_t v = (uint32_t)fmt_entity_int(rs, "CollisionAudioHardness", 0);
        SU32(e, 0x14c, v);
        if (v > 10) SU32(e, 0x150, 10);
    } else if (fmt_entity_field_tagged(rs, "UseSoftCollision", "Bool") >= 0)
        SU32(e, 0x150, fmt_entity_bool(rs, "UseSoftCollision", 0) ? 0 : 5);
    else SU32(e, 0x150, 5);
    SU32(e, 0x154, 0);
    int have = 0; int32_t zone = 0;
    if (fmt_entity_field_tagged(rs, "SoundZone", "Int") >= 0) { zone = fmt_entity_int(rs, "SoundZone", 0); have = 1; }
    else if (fmt_entity_field_tagged(rs, "DriveOnMaterial", "EnumValue") >= 0) { zone = rx_rs_enum(rs, "DriveOnMaterial"); have = 1; }
    SU32(e, 0x158, !have ? 0 : zone < 0 ? 0 : zone > 7 ? 7 : (uint32_t)zone);
    if (rx_rs_has(rs, "AIAvoid") && fmt_entity_bool(rs, "AIAvoid", 0)) SU32(e, 0x74, U32(e, 0x74) | 8);
    if (rx_rs_has(rs, "PhysicsType")) {
        const char* t = field_enum_text(rs, "PhysicsType");
        SU32(e, 0x198, rx_phys_props_find(rx_phys_props_get(), t ? t : ""));
    } else SU32(e, 0x198, 0);
    SU8(e, 0x19c, rx_rs_has(rs, "DriveOn") ? fmt_entity_bool(rs, "DriveOn", 0) != 0 : 0);
    rx_collision_entity_add_to_world(e, rx_phys_world_of_scene(U32(e, 0x10)));   /* vf28 (slot 0x70) */
    rx_collision_entity_set_drive_on(e);
}

/* RXPK scenery: register the imported, tiled static triangle meshes in the normal Reflex world. */
void rx_modtrack_add_collision(rx_addr world) {
    if (!fmt_modtrack_collision_claim(world)) return;
    uint32_t chunks=fmt_modtrack_collision_count(), total=0;
    /* World finalization runs after the level-loading scale bracket has closed. */
    const float scale=rx_ms_scale();
    for(uint32_t chunk=0;chunk<chunks;chunk++) {
        uint32_t nv=0;const float* vertices=fmt_modtrack_collision_chunk(chunk,&nv);
        if(!vertices||!nv)continue;
        const float* anchors=fmt_modtrack_collision_anchors(chunk);
        float* scaled=malloc(nv*3*sizeof(float));if(!scaled)continue;
        for(uint32_t i=0;i<nv;i++)fmt_modtrack_scale_point(vertices+3*i,anchors?anchors+4*i:NULL,scale,scaled+3*i);
        vertices=scaled;
        float mn[3]={vertices[0],vertices[1],vertices[2]},mx[3]={vertices[0],vertices[1],vertices[2]},center[3];
        for(uint32_t i=0;i<nv;i++)for(int k=0;k<3;k++){float v=vertices[i*3+k];if(v<mn[k])mn[k]=v;if(v>mx[k])mx[k]=v;}
        for(int k=0;k<3;k++)center[k]=(mn[k]+mx[k])*.5f;
        rx_addr tri=rx_crt_aligned_malloc(nv*16,16);float radius2=0;
        for(uint32_t i=0;i<nv;i++) {
            float d2=0;
            for(int k=0;k<3;k++){float v=(vertices[i*3+k]-center[k]);SF32(tri+i*16,k*4,v);d2+=v*v;}
            SF32(tri+i*16,12,0);if(d2>radius2)radius2=d2;
        }
        RX_FRAME_ENTER(F,0x70);
        rx_addr geometry=F,matrix=F+0x20,handle=F+0x60;
        for(uint32_t i=0;i<0x70;i+=4)SU32(F,i,0);
        SU32(geometry,0xc,tri);SU32(geometry,0x10,nv/3);SF32(geometry,0x14,sqrtf(radius2));
        for(int k=0;k<4;k++)SF32(matrix,k*20,1);
        for(int k=0;k<3;k++)SF32(matrix,0x30+k*4,center[k]);
        rx_addr shape=rx_shape_triangles_construct(rx_crt_aligned_malloc(0x40,16),geometry);
        SU32(shape,4,U32(shape,4)+1);SU32(handle,0,0x009765a8);SU32(handle,4,shape);
        /* Static contact queries return record +0x30 to rider-hit and audio code, which expect a
         * CollisionEntity, not a null owner. Keep the entity and its handle for the session lifetime. */
        rx_addr entity=rx_collision_entity_create();
        copy_words(entity+0x30,matrix,0x40);copy_words(entity+0xc0,matrix,0x40);
        /* Imported chunks include riding decks as well as barriers. Native wheel
         * queries require DriveOn; body-only collision strands bikes beneath decks. */
        SU32(entity,0x198,rx_phys_props_find(rx_phys_props_get(),"Terrain"));SU8(entity,0x19c,1);
        rx_static_add_shape(world,entity+0x15c,matrix,handle,1,U32(entity,0x198));
        rx_addr ref=U32(entity,0x160),vector=world+0x19b4;
        SU32(U32(vector,0xc)+(U32(ref,8)<<6),0x30,entity);
        rx_collision_shape_release(shape);rx_crt_aligned_free(tri);
        RX_FRAME_LEAVE(F,0x70);free(scaled);total+=nv/3;
    }
    fprintf(stderr,"rxcore: MTX scenery collision: %u chunks, %u triangles\n",chunks,total);
}
