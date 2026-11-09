/* phys_coll_shape.h -- collision shapes (0x0084xxxx): world bounds, composite children, box corners, point tests.
 * Shape objects keep the exe's layout; every bounds function has the natural signature
 *     void f(rx_addr self, rx_addr rotation_quat, rx_addr position, rx_addr out_min, rx_addr out_max)
 * (vec4s in game memory), also when it is reached through a virtual call (phys_shape_bounds). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
typedef void (*phys_shape_bounds_fn)(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);
/* a virtual bounds call: slot address `fn` of the shape's vtable -> the core function when ported, else the original */
void phys_shape_bounds(rx_addr fn, rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);
/* the same call as the original makes it: the four stack arguments below the caller's ESP and the ESP lowered by them and the
 * return address, so the callee's frame lies at the original's addresses */
void phys_shape_bounds_call(rx_addr fn, rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);

void phys_box_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);          /* 008455f0 CollisionShapeBox::vf2 */
void phys_hfterrain_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);    /* 008448e0 CollisionShapeHFTerrain::vf2 */
void phys_dynrays_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);      /* 00845100 CollisionShapeDynamicRays::vf2 */
void phys_dyncyl_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);       /* 00846700 CollisionShapeDynamicCylinder::vf2 */
void phys_circle_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);       /* 00845480 CollisionShapeCircle::vf2 */
void phys_triangles_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);     /* 00845010 CollisionShapeTriangles::vf3 */
void phys_box_vf3(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);             /* 00845a70 CollisionShapeBox::vf3 */
void phys_composite_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);    /* 00848340 CollisionShapeComposite::vf3 */

void phys_composite_child_rotation(rx_addr comp, uint32_t index, rx_addr q, rx_addr out);               /* 00848650 */
void phys_composite_child_position(rx_addr comp, uint32_t index, rx_addr q, rx_addr pos, rx_addr out);  /* 008486f0 */
void phys_composite_set_child_position(rx_addr comp, rx_addr world_pos, rx_addr body_pos, uint32_t index, rx_addr q);  /* 00848800 */
void phys_composite_set_child_rotation(rx_addr comp, rx_addr world_rot, uint32_t index, rx_addr q);     /* 00848910 */

void phys_box_corners(rx_addr self, rx_addr out);                    /* 00845a80 CollisionShapeBox_Corners */
uint32_t phys_box_contains_point(rx_addr point, rx_addr box);        /* 00845f20 CollisionShapeBox_ContainsPoint */
uint32_t phys_cylinder_contains_point(rx_addr point, rx_addr cyl);   /* 00846bb0 */
void phys_sphere_scale(rx_addr self, float scale);                   /* 00843fc0 CollisionShapeSphere::vf4 */
void phys_sphere_restore(rx_addr self);                              /* 00843ff0 CollisionShapeSphere::vf5 */
void phys_dyncyl_scale(rx_addr self, float scale);                   /* 00844050 CollisionShapeDynamicCylinder::vf4 */
void phys_dyncyl_restore(rx_addr self);                              /* 00844080 CollisionShapeDynamicCylinder::vf5 */
void phys_shape_scale_to_radius(rx_addr shape, rx_addr dir_out, rx_addr v);   /* 008454d0 */
uint32_t phys_xenon_core_vf2(rx_addr self);                          /* 008439f0 XenonCore::vf2 */
rx_v4 phys_shape_vec20(rx_addr shape);                               /* 008454c0 */
#ifdef __cplusplus
}
#endif
