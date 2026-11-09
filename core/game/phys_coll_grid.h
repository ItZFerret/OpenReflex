/* phys_coll_grid.h -- the broadphase grid (0x00882xxx) with its SIMD integer helpers, the floor-to-integer kernel and the
 * int64 helpers. LTCG register conventions (native/game/notes/collision.md): the Int4 helpers take xmm0 / xmm1 and return xmm0
 * (signed 32-bit integer lanes, held bit for bit in the lanes of an rx_v4); the grid functions take the grid in ESI. */
#pragma once
#include "phys_vec.h"
#ifdef __cplusplus
extern "C" {
#endif
/* integer lanes of an rx_v4 (bit patterns) */
rx_v4 phys_int4_gt_mask(rx_v4 a, rx_v4 b);                          /* 00882170 Int4GreaterMask */
rx_v4 phys_int4_below_flags(rx_v4 a, rx_v4 b, uint32_t* flags);     /* 008821e0 Int4BelowFlags */
rx_v4 phys_int4_min(rx_v4 a, rx_v4 b);                              /* 00882290 Int4Min */
rx_v4 phys_int4_max(rx_v4 a, rx_v4 b);                              /* 00882300 Int4Max */
rx_v4 phys_vec4_floor_to_int(rx_v4 x);                              /* 0069b2f0 Vec4FloorToInt */
/* 0069b400 Vec4StoreAligned: stores x at (dest & ~15) unless dest is null; returns the xmm0 the original leaves */
rx_v4 phys_vec4_store_aligned(rx_v4 x, rx_addr dest);

/* the grid object (see phys_coll_grid.c for the layout) */
void phys_grid_query_cells(rx_addr grid, rx_addr lo, rx_addr hi);   /* 00882d90 BroadphaseGrid_QueryCells */
void phys_grid_query_boxes(rx_addr grid, rx_addr lo, rx_addr hi);   /* 00883020 BroadphaseGrid_QueryBoxes */
void phys_grid_insert(rx_addr grid, uint32_t id);                   /* 008826d0 BroadphaseGrid_Insert */
void phys_grid_remove(rx_addr grid, uint32_t id);                   /* 00882980 BroadphaseGrid_Remove */
/* the same with the original's frame (its helpers' too): E = the return slot, the caller's EBP / EBX / EDI (they are pushed) */
void phys_grid_remove_fr(rx_addr grid, uint32_t id, rx_addr E, uint32_t ebp, uint32_t ebx, uint32_t edi);
void phys_grid_update(uint32_t id, rx_addr object, rx_addr lo, rx_addr hi); /* 0084a1a0 BroadphaseGrid_Update */

uint64_t phys_int64_load(rx_addr self);                             /* 0088bd90 FUN_0088bd90 */
uint64_t phys_int64_assign(rx_addr self, rx_addr src);              /* 0088bd50 FUN_0088bd50: returns the (lo, hi) it copied */
void phys_int64_add(rx_addr self, rx_addr out, uint32_t lo, uint32_t hi);   /* 0088be10 FUN_0088be10 */
#ifdef __cplusplus
}
#endif
