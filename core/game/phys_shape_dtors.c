/* phys_shape_dtors.c -- the destructors of the RavenPhysics collision shapes (2026-10-26, agent w-teardown).
 *
 * A dynamic record of the physics world (0xe0 bytes, vector +0x1954 / +0x1994) holds its shape through a handle embedded at +0x28
 * (vtable 0x9765b4, the shape at +0x2c); the shape's reference count is at +4. When the last reference is dropped (a body removal,
 * a vehicle transfer, the rider / vehicle teardown) the shape's scalar deleting destructor runs through vtable slot 0:
 *
 *   00847770  CollisionShape (also DynamicRays, DynamicCylinder)  registry entry removed (00844170), free (ReflexAlignedFree)
 *   008455a0  Sphere / Box / StaticBox / DynamicBox / StaticCylinder   the same, the free under the allocator lock (0xd61704)
 *   00845570  Circle      vtable 0x976750 first
 *   00844970  Unknown     vtable 0x9765c0 first
 *   00844860  HFTerrain   vtable 0x9765e4 first
 *   00844a90  Triangles   its index block (+0x24) freed under the lock (00844c50), the locked free
 *   00844f70  StaticRays  its ray block (+0x24) freed under the lock (00844f90), ReflexAlignedFree
 *   00847c60  Composite   children deleted (vf0), three vectors (0081f6e0), child array, name block, registry entry (00847d50)
 *   00848ec0  StaticComposite   the pool block at +0x80 (00819e40), then the composite body
 *   00849040  DynamicComposite  the name strings at +0x80 (0042b280) and their block, then the composite body
 *   00849830  the handle: vtable 0x9765b4, its shape released;   00849630  its vf1: the shape released, the handle cleared
 *
 * Every function is a thiscall with the flags on the stack (bit 0: free the object) returning this (RET 4), unless the heading says
 * a register convention. The "lock" is the engine allocator's critical section 0x00d61704, taken only while [0x00da67d8] is set. */
#include "phys_shape_dtors.h"
#include "build_activity_util.h"
#include "build_alloc.h"
#include "loop_platform.h"
#include "loop_vehicle.h"
#include "vehicle_atv_destroy.h"
#include "crt.h"

/* ReflexAlignedFree 0078fc50 (cdecl, one argument), called at the current ESP */
static void engine_free(rx_addr p) {
    ACT_CALL(1, (SU32(rx_sp(), 4, p), rx_reflex_aligned_free(p)));
}

/* operator delete as the compiler inlined it in the scalar deleting destructors: _aligned_free (IAT 0x916350) between the
 * allocator lock's enter / leave (IAT 0x91600c / 0x9160f8) while [0xda67d8] is set */
static void delete_locked(rx_addr p) {
    if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
    ACT_CALL(1, (SU32(rx_sp(), 4, p), crt_aligned_free(p)));
    if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
}

/* the shape's reference dropped: the count at +4 decremented; at 0 the shape (reloaded: it is never null here) is deleted through
 * vf0 with flags 1. This is the code the compiler inlined for every IntrusiveHandle release (0042c080 and its siblings). */
void rx_collision_shape_release(rx_addr shape) {
    if (shape == 0) return;
    SU32(shape, 4, U32(shape, 4) - 1);
    if (U32(shape, 4) == 0) {
        const uint32_t flags = 1;
        (void)act_vcall(shape, 0, 1, &flags);
    }
}

/* ---- the simple classes: vtable (optional), registry removal, free ---- */

/* 00847770 RavenPhysics::CollisionShape::sdtor (thiscall; flags; RET 4) -> this. Frame: push esi. */
rx_addr rx_collision_shape_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_registry_remove(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00847770, rx_collision_shape_sdtor)

/* 008455a0 RavenPhysics::CollisionShapeBox::sdtor: as 00847770 with the locked free. Frame: push esi. */
rx_addr rx_collision_shape_box_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_registry_remove(s));
    if (flags & 1) delete_locked(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x008455a0, rx_collision_shape_box_sdtor)

/* 00845570 RavenPhysics::CollisionShapeCircle::sdtor: the Circle vtable 0x976750 stored first, then as 00847770. */
rx_addr rx_collision_shape_circle_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(s, 0, 0x00976750);
    ACT_CALL(0, rx_registry_remove(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00845570, rx_collision_shape_circle_sdtor)

/* 00844970 RavenPhysics::CollisionShapeUnknown::sdtor: vtable 0x9765c0 stored first, then as 00847770. */
rx_addr rx_collision_shape_unknown_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(s, 0, 0x009765c0);
    ACT_CALL(0, rx_registry_remove(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00844970, rx_collision_shape_unknown_sdtor)

/* 00844860 RavenPhysics::CollisionShapeHFTerrain::sdtor: vtable 0x9765e4 stored first, then as 00847770. */
rx_addr rx_collision_shape_hfterrain_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(s, 0, 0x009765e4);
    ACT_CALL(0, rx_registry_remove(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00844860, rx_collision_shape_hfterrain_sdtor)

/* ---- Triangles / StaticRays: an owned block ---- */

/* 00844c50 (ESI = the shape; no stack arguments; ends with a jump to 00844170): the Triangles vtable 0x976608 stored, the block
 * at +0x24 freed under the lock, the registry entry removed. */
void rx_collision_shape_triangles_destroy(rx_addr s) {
    const rx_addr block = U32(s, 0x24);
    SU32(s, 0, 0x00976608);
    delete_locked(block);
    ACT_CALL(0, rx_registry_remove(s));
}
RX_FUNCTION(0x00844c50, rx_collision_shape_triangles_destroy)

/* 00844a90 RavenPhysics::CollisionShapeTriangles::sdtor. Frame: push esi. */
rx_addr rx_collision_shape_triangles_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_collision_shape_triangles_destroy(s));
    if (flags & 1) delete_locked(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00844a90, rx_collision_shape_triangles_sdtor)

/* 00844f90 (ESI = the shape; the ray block pointer is read before the lock; falls into 00844170): the StaticRays vtable 0x97662c
 * stored, the block at +0x24 freed under the lock, the registry entry removed. Frame: push edi (the block). */
void rx_collision_shape_static_rays_destroy(rx_addr s) {
    const rx_addr block = U32(s, 0x24);
    SU32(s, 0, 0x0097662c);
    delete_locked(block);
    ACT_CALL(0, rx_registry_remove(s));
}
RX_FUNCTION(0x00844f90, rx_collision_shape_static_rays_destroy)

/* 00844f70 RavenPhysics::CollisionShapeStaticRays::sdtor (the free is ReflexAlignedFree). Frame: push esi. */
rx_addr rx_collision_shape_static_rays_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_collision_shape_static_rays_destroy(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00844f70, rx_collision_shape_static_rays_sdtor)

/* ---- the composites ---- */

/* 0081f6e0 (EAX = the vector record {+0 the proxy block, +0xc begin, +0x10 end, +0x14 capacity end}): rx_vehicle_sim_vector_destruct
 * (vehicle_atv_destroy.c), shared with the vehicle simulations; called below for the composite's three vectors. */

/* 00847d50 (ESI = the shape; plain ret): the Composite vtable 0x976774 stored; every child (the pointer array +0x2c .. +0x30, the
 * bounds re-checked each iteration) deleted through vf0 (1) when not null; the three vectors at +0x68, +0x50, +0x38 released
 * (0081f6e0); the child array (+0x2c) freed when allocated and its three pointers cleared; the block at +0x20 freed; the registry
 * entry removed (00844170). Frame: push ecx, ebp, edi. */
void rx_collision_shape_composite_destroy(rx_addr s) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    SU32(s, 0, 0x00976774);
    uint32_t count = (uint32_t)(I32(s, 0x30) - I32(s, 0x2c)) >> 2;
    for (uint32_t i = 0; count != 0 && i < count;) {
        uint32_t now = (uint32_t)(I32(s, 0x30) - I32(s, 0x2c)) >> 2;
        if (i >= now) ACT_CALL(0, crt_invalid_parameter_noinfo());
        const rx_addr child = U32(U32(s, 0x2c), i * 4);
        if (child != 0) {
            const uint32_t flags = 1;
            (void)act_vcall(child, 0, 1, &flags);
        }
        i++;
        count = (uint32_t)(I32(s, 0x30) - I32(s, 0x2c)) >> 2;
    }
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(s + 0x68));
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(s + 0x50));
    ACT_CALL(0, rx_vehicle_sim_vector_destruct(s + 0x38));
    if (U32(s, 0x2c) != 0) engine_free(U32(s, 0x2c));
    const rx_addr block = U32(s, 0x20);
    SU32(s, 0x2c, 0);
    SU32(s, 0x30, 0);
    SU32(s, 0x34, 0);
    engine_free(block);
    ACT_CALL(0, rx_registry_remove(s));
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00847d50, rx_collision_shape_composite_destroy)

/* 00847c60 RavenPhysics::CollisionShapeComposite::sdtor. Frame: push esi. */
rx_addr rx_collision_shape_composite_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    ACT_CALL(0, rx_collision_shape_composite_destroy(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00847c60, rx_collision_shape_composite_sdtor)

/* 00819e40 (ESI = the pool block {+0 proxy, +8 allocator, +0xc begin, +0x10 end, +0x14 capacity end, +0x18 a block, +0x24 a vector
 * begin, +0x28 / +0x2c}; plain ret): the vector at +0x24 freed when allocated and its three words cleared; the block at +0x18
 * freed; the element block at +0xc deallocated (allocator::deallocate(ptr, bytes) = operator delete, IAT 0x916198) when
 * allocated and its words cleared; the proxy +0 freed. Frame: push edi. */
void rx_pool_block_free(rx_addr b) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    if (U32(b, 0x24) != 0) engine_free(U32(b, 0x24));
    const rx_addr block = U32(b, 0x18);
    SU32(b, 0x24, 0);
    SU32(b, 0x28, 0);
    SU32(b, 0x2c, 0);
    engine_free(block);
    const rx_addr elements = U32(b, 0xc);
    if (elements != 0) {
        const uint32_t bytes = U32(b, 0x14) - elements;
        (void)bytes;
        ACT_CALL(2, crt_operator_delete(elements));         /* push bytes; push elements; call [0x916198] */
    }
    const rx_addr proxy = U32(b, 0);
    SU32(b, 0xc, 0);
    SU32(b, 0x10, 0);
    SU32(b, 0x14, 0);
    engine_free(proxy);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00819e40, rx_pool_block_free)

/* 00848ec0 RavenPhysics::CollisionShapeStaticComposite::sdtor: the pool block at +0x80 (00819e40), the composite body (00847d50).
 * Frame: push esi, edi. */
rx_addr rx_collision_shape_static_composite_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    ACT_CALL(0, rx_pool_block_free(s + 0x80));
    ACT_CALL(0, rx_collision_shape_composite_destroy(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00848ec0, rx_collision_shape_static_composite_sdtor)

/* 0042b280 (stack the block; RET 4): the std::string array {+0xc begin, +0x10 end; 0x1c-byte elements} destroyed element by element
 * (~basic_string, IAT 0x9161b4) when the block is allocated, the array freed (ReflexAlignedFree), the three pointers cleared.
 * Frame: push ebx, esi, edi. */
void rx_string_array_free(rx_addr b) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 12);
    rx_addr cur = U32(b, 0xc);
    if (cur != 0) {
        const rx_addr end = U32(b, 0x10);
        if (cur != end) {
            do {
                ACT_CALL(0, crt_str_dtor(cur));
                cur += 0x1c;
            } while (cur != end);
        }
        engine_free(U32(b, 0xc));
    }
    SU32(b, 0xc, 0);
    SU32(b, 0x10, 0);
    SU32(b, 0x14, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x0042b280, rx_string_array_free)

/* 00849040 RavenPhysics::CollisionShapeDynamicComposite::sdtor: the name array at +0x80 (0042b280), its proxy block ([+0x80]) freed,
 * the composite body (00847d50). Frame: push esi, edi. */
rx_addr rx_collision_shape_dynamic_composite_sdtor(rx_addr s, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    ACT_CALL(1, rx_string_array_free(s + 0x80));
    engine_free(U32(s, 0x80));
    ACT_CALL(0, rx_collision_shape_composite_destroy(s));
    if (flags & 1) engine_free(s);
    rx_set_sp(sp0);
    return s;
}
RX_FUNCTION(0x00849040, rx_collision_shape_dynamic_composite_sdtor)

/* ---- the handle ---- */

/* 00849830 IntrusiveHandle<CollisionShape>::vf0 (thiscall; flags; RET 4) -> this: vtable 0x9765b4 stored; the shape (+4) released
 * (count - 1; at 0 the shape deleted through its vf0 (1)); freed with bit 0 (ReflexAlignedFree). Frame: push esi. */
rx_addr rx_collision_shape_handle_sdtor(rx_addr h, uint32_t flags) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 4);
    SU32(h, 0, 0x009765b4);
    const rx_addr shape = U32(h, 4);
    if (shape != 0) {
        SU32(shape, 4, U32(shape, 4) - 1);
        if (U32(shape, 4) == 0 && U32(h, 4) != 0) {
            const uint32_t one = 1;
            (void)act_vcall(U32(h, 4), 0, 1, &one);
        }
    }
    if (flags & 1) engine_free(h);
    rx_set_sp(sp0);
    return h;
}
RX_FUNCTION(0x00849830, rx_collision_shape_handle_sdtor)

/* 00849630 IntrusiveHandle<CollisionShape>::vf1 (ECX = the handle; plain ret): the shape (+4) released (count - 1; at 0 the shape, reloaded,
 * deleted through its vf0 (1)) and the handle cleared (+4 = 0). The damage manager's entries release their parts' shape handles with it
 * (00479dd0). Frame: push esi / edi. */
void rx_collision_shape_handle_reset(rx_addr h) {
    const rx_addr sp0 = rx_sp();
    rx_set_sp(sp0 - 8);
    const rx_addr shape = U32(h, 4);
    if (shape != 0) {
        SU32(shape, 4, U32(shape, 4) - 1);
        if (U32(shape, 4) == 0 && U32(h, 4) != 0) {
            const uint32_t one = 1;
            (void)act_vcall(U32(h, 4), 0, 1, &one);
        }
    }
    SU32(h, 4, 0);
    rx_set_sp(sp0);
}
RX_FUNCTION(0x00849630, rx_collision_shape_handle_reset)
