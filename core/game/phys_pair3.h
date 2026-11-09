/* phys_pair3.h -- box / cylinder contact generators (phys_pair3.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_pair_box_box(rx_addr B, rx_addr QB, rx_addr M, rx_addr A, rx_addr QA, rx_addr PA, rx_addr PB, uint32_t flag, uint32_t* edx);   /* 008657d0; *edx = the EDX left */
uint32_t phys_pair_box_cylinder(rx_addr cyl, rx_addr qcyl, rx_addr M, rx_addr box, rx_addr qbox, rx_addr pbox, rx_addr pcyl,
                                uint32_t flag, const rx_regs* caller, uint32_t* edx);   /* 00867ca0; returns ECX, *edx = EDX */
uint32_t phys_pair_cylinder_cylinder(rx_addr qb, rx_addr pb, rx_addr M, rx_addr A, rx_addr qa, rx_addr pa, rx_addr B, uint32_t flag,
                                     const rx_regs* caller, uint32_t* edx);   /* 0086bd80; returns ECX, *edx = EDX */
#ifdef __cplusplus
}
#endif
