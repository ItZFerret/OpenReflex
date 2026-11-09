/* phys_pair2.h -- shape-pair contact generators (phys_pair2.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t phys_pair_triangles_circle(rx_addr QC, rx_addr T, rx_addr M, rx_addr QT, rx_addr PT, rx_addr C, rx_addr PC,
                                    uint32_t flag);   /* 008716b0 (ECX, EDX; RET 0x18); returns EDX */
uint32_t phys_rays6_triangles(rx_addr ends, rx_addr v0, rx_addr v1, rx_addr v2, rx_addr starts, rx_addr hits, rx_addr points,
                              rx_addr normals, uint32_t* edx);   /* 0085ee70 (ECX, EDX; cdecl) */
void phys_pair_triangles_segments(rx_addr PS, rx_addr QT, rx_addr M, rx_addr T, rx_addr PT, rx_addr S, rx_addr QS,
                                  uint32_t flag);   /* 0086fac0 (ECX, EDX, ESI; RET 0x14) */
#ifdef __cplusplus
}
#endif
