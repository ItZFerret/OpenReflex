// phys_shape_dtors.cpp -- oracle adapters for core/game/phys_shape_dtors.c (group rx_phys): the RavenPhysics collision shape
// destructors and the shape handle's. No reference world deletes a shape (they live as long as the world); the probe
// core/oracle/dtor_probe.cpp drives them (the shapes of the physics world's dynamic records are unpinned, the vehicle / rider body
// removals then reach the last reference) and core/oracle/shape_dtor_probe.cpp builds shapes of every class.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_shape_dtors.h"
#include "../game/phys_coll_shape.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX;

// thiscall flags; RET 4 -> this
RX_PORT(0x00847770, CollisionShape_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x008455a0, CollisionShapeBox_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_box_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00845570, CollisionShapeCircle_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_circle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00844970, CollisionShapeUnknown_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_unknown_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00844860, CollisionShapeHFTerrain_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_hfterrain_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00844a90, CollisionShapeTriangles_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_triangles_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00844f70, CollisionShapeStaticRays_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_static_rays_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00847c60, CollisionShapeComposite_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_composite_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00848ec0, CollisionShapeStaticComposite_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_static_composite_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00849040, CollisionShapeDynamicComposite_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_dynamic_composite_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
RX_PORT(0x00849830, CollisionShapeHandle_Sdtor, "rx_phys", OUT_EAX) { m.r[0] = rx_collision_shape_handle_sdtor(m.r[1], m.arg(0)); return m.ret(4); }
// 00844c50 / 00844f90 / 00847d50 (ESI = the shape), 00819e40 (ESI = the pool block), 0042b280 (stack block; RET 4)
RX_PORT(0x00844c50, CollisionShapeTriangles_Destroy, "rx_phys", 0) { rx_collision_shape_triangles_destroy(m.r[6]); return m.ret(0); }
RX_PORT(0x00844f90, CollisionShapeStaticRays_Destroy, "rx_phys", 0) { rx_collision_shape_static_rays_destroy(m.r[6]); return m.ret(0); }
RX_PORT(0x00847d50, CollisionShapeComposite_Destroy, "rx_phys", 0) { rx_collision_shape_composite_destroy(m.r[6]); return m.ret(0); }
RX_PORT(0x00819e40, PoolBlock_Free, "rx_phys", 0) { rx_pool_block_free(m.r[6]); return m.ret(0); }
RX_PORT(0x0042b280, StringArray_Free, "rx_phys", 0) { rx_string_array_free(m.arg(0)); return m.ret(4); }
// 00845010 (thiscall: q*, pos*, out min*, out max*; RET 0x10): the Triangles / StaticRays bounds (the static record's grid box); only memory is written
RX_PORT(0x00845010, CollisionShapeTriangles_vf3, "rx_phys", 0) {
    phys_triangles_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3)); return m.ret(0x10);
}
// 00849630 IntrusiveHandle<CollisionShape>::vf1 (ECX = handle; plain ret): the damage manager's part shape handles (w-partdtor)
RX_PORT(0x00849630, CollisionShapeHandle_vf1, "rx_phys", 0) { rx_collision_shape_handle_reset(m.r[1]); return m.ret(0); }
