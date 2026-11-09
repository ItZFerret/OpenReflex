/* phys_pair.h -- contact generators of shape pairs (phys_pair.c). */
#pragma once
#include "phys_vec.h"
#include "phys_crt.h"
#ifdef __cplusplus
extern "C" {
#endif
void phys_quat_to_rows(rx_v4 q, rx_v4 rows[3]);
/* 00861a60 (ECX = position A*, EDX = sphere A; M, sphere B, position B*, flag; RET 0x10); returns ECX (the mask lane 1 / count +
 * 1), *ecx_out / *edx_out the original's ECX / EDX */
uint32_t phys_pair_sphere_sphere(rx_addr PA, rx_addr A, rx_addr M, rx_addr B, rx_addr PB, uint32_t flag, uint32_t* ecx_out,
                                 uint32_t* edx_out);
/* 00861ba0 (ECX = sphere, EDX = position A*; M, qA*, box, qB*, pB*, flag; RET 0x18) */
void phys_pair_sphere_box(rx_addr A, rx_addr PA, rx_addr M, rx_addr QA, rx_addr B, rx_addr QB, rx_addr PB, uint32_t flag);
/* 00862da0 (ECX = cylinder position*, EDX = sphere; M, sphere position*, cylinder, cylinder rotation*, flag; RET 0x14) */
void phys_pair_sphere_cylinder(rx_addr PB, rx_addr A, rx_addr M, rx_addr PA, rx_addr B, rx_addr QB, uint32_t flag);
/* 00873010 (ECX = height field, EDX = its rotation*; M, its position*, composite, rotation*, position*, flag; RET 0x18) */
void phys_pair_composite_hfterrain(rx_addr T, rx_addr QT, rx_addr M, rx_addr PT, rx_addr C, rx_addr QC, rx_addr PC,
                                   uint32_t flag);
/* 0086b4d0 (ECX = height field, EDX = box; M, box rotation*, box position*, flag; RET 0x10) */
void phys_pair_box_hfterrain(rx_addr T, rx_addr X, rx_addr M, rx_addr QX, rx_addr PX, uint32_t flag);
/* 0086f2a0 (ECX = cylinder position*, EDX = cylinder rotation*, ESI = height field, EDI = M; cylinder, height field, flag;
 * RET 0xc) */
void phys_pair_cylinder_hfterrain(rx_addr PA, rx_addr QA, rx_addr M, rx_addr A, rx_addr B, uint32_t flag);
/* 00869d40 (ECX = box, EDX = mesh rotation*; M, box rotation*, box position*, mesh, mesh position*, flag; RET 0x18) */
void phys_pair_box_triangles(rx_addr X, rx_addr QT, rx_addr M, rx_addr QX, rx_addr PX, rx_addr T, rx_addr PT, uint32_t flag);
/* 00863f70 (ECX = mesh position*, EDX = sphere, EDI = mesh; M, sphere position*, mesh rotation*, flag; RET 0x10) */
void phys_pair_sphere_triangles(rx_addr PT, rx_addr S, rx_addr T, rx_addr M, rx_addr PS, rx_addr QT, uint32_t flag);
/* 0086e090 (ECX = cylinder position*, EDX = mesh rotation*, EDI = M; cylinder, rotation*, mesh, mesh position*, flag; RET 0x14) */
void phys_pair_cylinder_triangles(rx_addr PC, rx_addr QT, rx_addr M, rx_addr C, rx_addr QC, rx_addr T, rx_addr PT,
                                  uint32_t flag);
#ifdef __cplusplus
}
#endif
