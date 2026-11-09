#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Existing canonical 0086b7f0 math with its returning register interface.
 * Caller owns entered-frame validation and the actual RET18 transition. */
void rx_collision_0400_box_live(rx_addr qa,rx_addr pa,rx_addr manifold,
 rx_addr box,rx_addr qb,rx_addr pb,rx_addr radial,uint32_t flip,rx_v4 *live);
#ifdef __cplusplus
}
#endif
