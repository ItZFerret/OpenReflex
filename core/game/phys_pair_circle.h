/* phys_pair_circle.h -- contact generators of the dispatcher's circle shape (type 0x400, CollisionShapeCircle: the radius vec4
 * at +0x20) against a sphere (008650c0) and a cylinder (0086f3d0) (phys_pair_circle.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 008650c0 sphere against circle. Convention (LTCG): EDX = the circle's rotation*, CL = the swap flag (the rest of ECX is a
 * leftover of the dispatcher); stack: M, sphere, sphere position*, circle, circle position*; RET 0x14.
 * Returns the ECX the original leaves (the new contact count after a contact, else the entry ECX); *edx = the EDX it leaves. */
uint32_t phys_pair_sphere_circle(uint32_t ecx, rx_addr qcircle, rx_addr M, rx_addr sphere, rx_addr psphere, rx_addr circle,
                                 rx_addr pcircle, uint32_t* edx);

/* 0086f3d0 cylinder against circle. Convention (LTCG): ECX = the circle's position*, EDX = the cylinder's rotation*; stack: M,
 * cylinder, cylinder position*, circle, circle rotation*, swap flag; RET 0x18. EAX / ESI / EDI hold dispatcher copies (never
 * read). EBX / EBP are handed to 0081c590, which saves them in its frame.
 * Returns ECX (the security cookie, as the epilogue leaves it); *edx = the EDX it leaves. */
uint32_t phys_pair_cylinder_circle(rx_addr pcircle, rx_addr qcylinder, rx_addr M, rx_addr cylinder, rx_addr pcylinder,
                                   rx_addr circle, rx_addr qcircle, uint32_t flag, uint32_t* edx);

#ifdef __cplusplus
}
#endif
