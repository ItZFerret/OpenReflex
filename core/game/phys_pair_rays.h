/* phys_pair_rays.h -- contact generators of the dispatcher's ray-list shape (type 0x20, CollisionShapeStaticRays /
 * DynamicRays: count +0x20, 0x20-byte entries at +0x24, each two vec4 end points) against a sphere (00864460) and the height
 * field terrain (00872c90) (phys_pair_rays.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif

/* 00864460 sphere against ray list. Convention (LTCG): ECX = the ray list's position*, EDX = the sphere, ESI = M; stack: the
 * sphere's position*, the ray list, its rotation*, swap flag; RET 0x10. EAX / EDI hold dispatcher copies (never read).
 * Returns the ECX the original leaves; *edx = the EDX it leaves (both preset with the values at entry). */
uint32_t phys_pair_sphere_rays(rx_addr prays, rx_addr sphere, rx_addr M, rx_addr psphere, rx_addr rays, rx_addr qrays,
                               uint32_t flag, uint32_t* edx);

/* 00872c90 ray list against the height-field terrain. Convention (LTCG): ECX = the ray list, EDX = its position*; stack: M,
 * the ray list's rotation*, the terrain shape (+0x20 its terrain registry), swap flag (inverted in its stack slot); RET 0x10.
 * Returns the ECX the original leaves; *edx = the EDX it leaves (both preset with the values at entry). */
uint32_t phys_pair_rays_hfterrain(rx_addr rays, rx_addr prays, rx_addr M, rx_addr qrays, rx_addr terrain_shape, uint32_t flag,
                                  uint32_t* edx);

#ifdef __cplusplus
}
#endif
