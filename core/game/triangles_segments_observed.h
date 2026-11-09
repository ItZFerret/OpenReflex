#pragma once
#include "phys_pair2.h"
#ifdef __cplusplus
extern "C" {
#endif
/* Entered original frame; leaves its ESP intact, caller models RET14.
 * Genuine source arrays/poses/manifold must be disjoint from private frame.
 * Native85ee70 exact dependency remains explicitly unclosed. */
void phys_pair_triangles_segments_observed(rx_addr PS,rx_addr QT,rx_addr M,rx_addr T,rx_addr PT,rx_addr S,rx_addr QS,uint32_t flag);
#ifdef __cplusplus
}
#endif
