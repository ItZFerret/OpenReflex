/* phys_coll_terrain.h -- the height-field terrain queries (0x007c5xxx..0x007cdxxx): cell setup and sampling of the base and
 * deformation height grids, height / normal at a point, and the terrain triangles around a point. LTCG register conventions
 * (native/game/notes/collision.md): see phys_coll.cpp. The deformation surface is a D3D texture: LockRect (vtable slot 13, flags
 * 0x810) / UnlockRect (slot 14) of the object at terrain+0x240 are called through its vtable. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* 007c51e0 TerrainCellSetup: returns 1 inside the grid, else 0 (cell clamped); *edx_out = the EDX the original leaves */
uint32_t phys_terrain_cell_setup(rx_addr work, rx_addr dims, rx_addr pos, rx_addr cells, uint32_t* edx_out);
void phys_terrain_cell_offsets(rx_addr work);                                                 /* 007c52f0 TerrainCellOffsets */
void phys_terrain_sample_window(rx_addr work, rx_addr out_center, rx_addr out_nbr, rx_addr grid, rx_addr scale);   /* 007c5440 */
uint32_t phys_terrain_sample_cell(rx_addr grid, rx_addr work, rx_addr scale, rx_addr out);    /* 007c5380; returns the EDX left (cell index of lane 0) */
uint32_t phys_terrain_height_normal(rx_addr normal, rx_addr terrain, rx_addr point);          /* 007c5e80 TerrainHeightNormal */
void phys_terrain_triangles(rx_addr q, rx_addr terrain, rx_addr out, rx_addr count_p, int32_t capacity, rx_addr extent);   /* 007c6160 */
uint32_t phys_float_list_push(rx_addr list, uint32_t value_bits);                            /* 007cda90 FUN_007cda90: returns the old count */
rx_addr phys_float_pair_list_push(rx_addr list, uint32_t a_bits, uint32_t b_bits);           /* 007cda50 FUN_007cda50: returns the entry address */
#ifdef __cplusplus
}
#endif
