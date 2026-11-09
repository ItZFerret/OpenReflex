/* phys_tri.h -- ray and vertex queries against triangle lists (phys_tri.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 00860e90 (ECX = count, EDX = hits*, ESI = points*; cdecl triangles*, normals*, rays*, box min*, box max*) */
int32_t phys_rays4_triangles(int32_t count, rx_addr hits, rx_addr points, rx_addr tris, rx_addr normals, rx_addr rays,
                             rx_addr qmin, rx_addr qmax);
/* the caller's EDI / EBP words of 00860e90's frame (entered at ESP sp0) */
void phys_rays4_saved_regs(rx_addr sp0, uint32_t edi, uint32_t ebp);
/* 008639f0 (ECX = a*, EDX = p*; cdecl out*, b*, c*) */
void phys_triangle_closest_point(rx_addr pa, rx_addr pp, rx_addr out, rx_addr pb, rx_addr pc);
/* the caller's ESI / EDI / EBP words of 00873540's frame (entered at ESP sp0) */
void phys_edges12_saved_regs(rx_addr sp0, uint32_t esi, uint32_t edi, uint32_t ebp);
/* 00873540 (ECX = hits*, EDX = normals*; cdecl triangles*, count, points*, edges*, box min*, box max*) */
int32_t phys_edges12_triangles(rx_addr hits, rx_addr normals, rx_addr tris, int32_t count, rx_addr points, rx_addr edges,
                               rx_addr qmin, rx_addr qmax);
#ifdef __cplusplus
}
#endif
