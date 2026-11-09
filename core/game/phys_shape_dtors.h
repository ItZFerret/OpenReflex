/* phys_shape_dtors.h -- the destructors of the RavenPhysics collision shapes and of their reference handle (phys_shape_dtors.c).
 * A shape is a reference counted object (+4 = the count); the last reference released runs the scalar deleting destructor through
 * vtable slot 0 with flags = 1. All classes derive from CollisionShape (vtable 0x976584), which keeps a registry of its instances
 * (0x00d6d8e0 vector, lock 0x00d6d8f8; rx_registry_remove 00844170). */
#pragma once
#include "rx_game.h"
#ifdef __cplusplus
extern "C" {
#endif
/* the inlined release of a shape reference: the count (+4) decremented, and at 0 the scalar deleting destructor (vf0, flags 1) */
void    rx_collision_shape_release(rx_addr shape);
rx_addr rx_collision_shape_sdtor(rx_addr s, uint32_t flags);            /* 00847770 CollisionShape / DynamicRays / DynamicCylinder (thiscall; RET 4) -> this */
rx_addr rx_collision_shape_box_sdtor(rx_addr s, uint32_t flags);        /* 008455a0 Sphere / Box / StaticBox / DynamicBox / StaticCylinder (locked free) */
rx_addr rx_collision_shape_circle_sdtor(rx_addr s, uint32_t flags);     /* 00845570 Circle */
rx_addr rx_collision_shape_unknown_sdtor(rx_addr s, uint32_t flags);    /* 00844970 Unknown */
rx_addr rx_collision_shape_hfterrain_sdtor(rx_addr s, uint32_t flags);  /* 00844860 HFTerrain */
rx_addr rx_collision_shape_triangles_sdtor(rx_addr s, uint32_t flags);  /* 00844a90 Triangles */
void    rx_collision_shape_triangles_destroy(rx_addr s);                /* 00844c50 (ESI = the shape) */
rx_addr rx_collision_shape_static_rays_sdtor(rx_addr s, uint32_t flags);/* 00844f70 StaticRays */
void    rx_collision_shape_static_rays_destroy(rx_addr s);              /* 00844f90 (ESI = the shape) */
rx_addr rx_collision_shape_composite_sdtor(rx_addr s, uint32_t flags);  /* 00847c60 Composite */
void    rx_collision_shape_composite_destroy(rx_addr s);                /* 00847d50 (ESI = the shape) */
rx_addr rx_collision_shape_static_composite_sdtor(rx_addr s, uint32_t flags);   /* 00848ec0 StaticComposite */
rx_addr rx_collision_shape_dynamic_composite_sdtor(rx_addr s, uint32_t flags);  /* 00849040 DynamicComposite */
rx_addr rx_collision_shape_handle_sdtor(rx_addr h, uint32_t flags);     /* 00849830 CollisionShapeHandle (thiscall; RET 4) -> this */
void    rx_collision_shape_handle_reset(rx_addr h);                     /* 00849630 CollisionShapeHandle::vf1 (ECX = the handle) */
void    rx_pool_block_free(rx_addr block);                              /* 00819e40 (ESI = the block) */
void    rx_string_array_free(rx_addr block);                            /* 0042b280 (stack block; RET 4) */
#ifdef __cplusplus
}
#endif
