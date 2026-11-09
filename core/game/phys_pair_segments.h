/* phys_pair_segments.h -- cylinder against a segment list (0086e750; phys_pair_segments.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 0086e750 (ECX = the segment-list shape, EDX = its position; stack: M, cylinder, cylinder rotation*, cylinder position*,
 * the segment list's rotation*, swap flag; RET 0x18). Returns ECX; *edx = the EDX the original leaves. */
uint32_t phys_pair_cylinder_segments(rx_addr seg, rx_addr pseg, rx_addr M, rx_addr cyl, rx_addr qcyl, rx_addr pcyl, rx_addr qseg,
                                     uint32_t flag, uint32_t* edx);
#ifdef __cplusplus
}
#endif
