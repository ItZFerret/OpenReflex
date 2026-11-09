/* phys_terrain.h -- terrain point queries and track-strip bookkeeping (phys_terrain.c). */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
uint32_t phys_terrain_material_at(rx_addr terrain, rx_addr point, uint32_t layer, const rx_regs* caller, rx_regs* out);   /* 007c5120 (EDI, EDX, EAX) */
uint32_t phys_terrain_base_height_at(rx_addr terrain, rx_addr point, uint32_t* edx);              /* 007c5950 (thiscall; RET 4) */
uint32_t phys_terrain_height_at(rx_addr terrain, rx_addr point, uint32_t* edx);                   /* 007c5aa0 (stdcall; RET 8) */
uint32_t phys_terrain_base_height_normal(rx_addr normal, rx_addr terrain, rx_addr point);         /* 007c5c90 (thiscall; RET 8) */
uint32_t phys_terrain_segment_coarse_reject(rx_addr box, rx_addr coarse);                         /* 007c7350 (ECX, EDX) */
int32_t  phys_tracks_alloc_slot(void);                                                            /* 007c34b0 */
void     phys_terrain_grid_vertices(rx_addr terrain, rx_addr centre, rx_addr count_p, rx_addr extent);   /* 007c66f0 (ECX, EAX; RET 8) */
rx_addr  phys_patch_add_stamp(rx_addr set, rx_addr p0, rx_addr p1, float depth0, float depth1, float width0, float width1,
                              uint32_t* edx);                                                   /* 007c2ab0 (ESI, EAX; RET 0x14) */
uint32_t phys_tracks_add_point(rx_addr tracks, int32_t index, rx_addr pos, rx_addr prev, float width, float offset, float intensity,
                               uint32_t flag, uint32_t ecx_in);                                  /* 007c3510 (EAX; RET 0x1c); returns ECX */
void     phys_tracks_cut_strip(rx_addr tracks, int32_t index);                                    /* 007c4560 (ESI, EAX) */
/* phys_terrain_ray.c: the segment query; regs[0] / regs[1] = the ECX / EDX the original leaves (preset by the caller with the
 * values at entry) */
uint32_t phys_terrain_segment_query(rx_addr terrain, rx_addr start, rx_addr end, rx_addr hit, rx_addr normal, uint32_t regs[2]); /* 007c6cc0 */
uint32_t phys_terrain_coarse_traversal(rx_addr E, rx_addr terrain, rx_addr hit, rx_addr normal, rx_addr S, uint32_t regs[2]);   /* 007c73e0 */
uint32_t phys_terrain_fine_traversal(rx_addr P, rx_addr Q, rx_addr dir3, rx_addr terrain, rx_addr hit, rx_addr normal,
                                     rx_addr dir2, uint32_t regs[2]);                                                         /* 007c83b0 */
void     phys_terrain_fine_traversal_saved_regs(rx_addr sp0, uint32_t ebx, uint32_t ebp);   /* 007c83b0's saved EBX / EBP (not an original function) */
uint32_t phys_terrain_cell_intersect(rx_addr base, rx_addr terrain, rx_addr hit, rx_addr normal, rx_addr seg, uint32_t regs[2]); /* 007c7d10 */
#ifdef __cplusplus
}
#endif
