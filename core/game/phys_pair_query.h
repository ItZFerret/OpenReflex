/* phys_pair_query.h -- contact generators of the dispatcher's query shape (type 0x100: +0x20 a query object whose virtual
 * functions answer a ray (vf8, +0x20) and a sphere (vf17, +0x44)) against a sphere (00864dd0), a box (0086b360), a cylinder
 * (0086f150) and a ray list (00872970) (phys_pair_query.c). No class of the executable constructs a type-0x100 shape (the
 * CollisionShape constructors 00844110 callers set 1, 2, 4, 8, 0x10, 0x20, 0x200, 0x400, 0x8000); the dispatcher still has
 * the cases, so they are ported for completeness. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 00864dd0 sphere against query shape. Convention (LTCG): ESI = the sphere, EDI = its position*; stack: M, the query shape,
 * swap flag; RET 0xc. EAX / ECX / EDX hold dispatcher leftovers (never read). Returns ECX (the security cookie). */
uint32_t phys_pair_sphere_query(rx_addr sphere, rx_addr psphere, rx_addr M, rx_addr shape, uint32_t flag);

/* 0086b360 box against query shape. Convention (LTCG): ECX = the box, EDX = its position*; stack: M, the box's rotation*, the
 * query shape, swap flag (inverted in its stack slot); RET 0x10. */
void phys_pair_box_query(rx_addr box, rx_addr pbox, rx_addr M, rx_addr qbox, rx_addr shape, uint32_t flag);

/* 0086f150 cylinder against query shape. Convention (LTCG): ECX = the cylinder's position*, EDX = its rotation*, EDI = M;
 * stack: the cylinder, the query shape, swap flag (inverted in its stack slot); RET 0xc. */
void phys_pair_cylinder_query(rx_addr pcylinder, rx_addr qcylinder, rx_addr M, rx_addr cylinder, rx_addr shape, uint32_t flag);

/* 00872970 ray list against query shape. Convention (LTCG): ECX = the ray list's position*, EDX = the query shape, ESI = the
 * ray list, EDI = M; stack: the ray list's rotation*, swap flag (inverted in its stack slot); RET 8. */
void phys_pair_rays_query(rx_addr prays, rx_addr shape, rx_addr rays, rx_addr M, rx_addr qrays, uint32_t flag);

#ifdef __cplusplus
}
#endif
