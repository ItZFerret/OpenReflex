#pragma once
#include "phys_coll_pair.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Actual ECX=rotA, EDX=posA, six stack args. Entered ESP retained;
 * caller models RET18. live is the actual incoming/outgoing packed XMM0. */
uint32_t phys_pair_shape_vs_composite_b_observed(rx_addr rotA,rx_addr posA,rx_addr M,rx_addr shapeA,rx_addr B,rx_addr qB,rx_addr pB,uint32_t swap,rx_v4*live);
/* Complete finite original-call provider first; native closure separately gated. */
void rx_composite_pair_dependency(rx_addr target,rx_addr ra,unsigned nargs,rx_addr expected,rx_v4*live);
#ifdef __cplusplus
}
#endif
