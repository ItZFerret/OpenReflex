#pragma once
#include "phys_coll_shape.h"
#ifdef __cplusplus
extern "C" {
#endif
/* EDI comp, ECX index, actual stackarguments q/out or q/pos/out.
 * Retain enteredESP; callers modelRET8/RETc. Packed output is incidental
 * actual XMM0, not the mathematical output quaternion/worldposition. */
void phys_composite_child_rotation_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr out,rx_v4*live);
void phys_composite_child_position_observed(rx_addr comp,uint32_t index,rx_addr q,rx_addr pos,rx_addr out,rx_v4*live);
#ifdef __cplusplus
}
#endif
