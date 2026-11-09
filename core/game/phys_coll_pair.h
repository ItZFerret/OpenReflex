/* phys_coll_pair.h -- shape-pair contact generation with composite shapes (0x0086xxxx / 0x0087xxxx). The pairs go through the
 * contact dispatcher 0x0085ddd0 (thiscall manifold M; shapeA, rotA*, posA*, shapeB, rotB*, posB*, swap; RET 0x1c), which is not part
 * of this package: it is called through rx_call_original. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 008732d0 CollisionPair_CompositeVsComposite (thiscall M; A, qA, pA, B, qB, pB, swap; RET 0x1c): returns the EAX the original leaves */
uint32_t phys_pair_composite_vs_composite(rx_addr M, rx_addr A, rx_addr qA, rx_addr pA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap);
/* 008656c0 CollisionPair_ShapeVsComposite (ECX rotA*, EDX posA*; stack M, shapeA, composite B, qB, pB, swap; RET 0x18) */
uint32_t phys_pair_shape_vs_composite(rx_addr rotA, rx_addr posA, rx_addr M, rx_addr shapeA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap);
/* 00872860 CollisionPair_ShapeVsCompositeB (same arguments; the child count is read again for every child) */
uint32_t phys_pair_shape_vs_composite_b(rx_addr rotA, rx_addr posA, rx_addr M, rx_addr shapeA, rx_addr B, rx_addr qB, rx_addr pB, uint32_t swap);
#ifdef __cplusplus
}
#endif
