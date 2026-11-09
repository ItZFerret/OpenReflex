/* phys_coll_shape2.h -- more collision shape functions: static box / composite shapes, composite vf2 / vf4 / vf5 and the vertex tables
 * of boxes and cylinders (globals at 0xd6d5e0..0xd6daa0 that the contact generators read). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_static_box_bounds(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);   /* 00846380 StaticBox::vf3 */
int32_t phys_static_composite_find(rx_addr self, rx_addr name);        /* 00848ef0 StaticComposite::vf8: child index by name, -1 */
uint32_t phys_composite_vf2(rx_addr self, rx_addr q, rx_addr pos, rx_addr out_min, rx_addr out_max);  /* 00847f60 Composite::vf2; returns EAX */
rx_addr phys_composite_scale(rx_addr self, float scale);               /* 00848af0 Composite::vf4; returns EAX */
uint32_t phys_composite_restore(rx_addr self);                         /* 00848bd0 Composite::vf5; returns EAX */
void phys_box_vertex_table(rx_addr box);                               /* 00845f90 FUN_00845f90 */
void phys_box_world_vertices(rx_addr box, rx_addr q, rx_addr pos);     /* 00846230 FUN_00846230 */
void phys_cylinder_vertex_table(rx_addr cyl);                          /* 008472d0 FUN_008472d0 */
void phys_cylinder_world_vertices(rx_addr cyl, rx_addr pos, rx_addr q);/* 00847630 FUN_00847630 */
/* 00845b50 FUN_00845b50 (ESI point* in / out, EDI box, stack normal out*; RET 4): the closest point of the box and its normal */
void phys_box_clamp_point(rx_addr box, rx_addr point, rx_addr normal);
#ifdef __cplusplus
}
#endif
