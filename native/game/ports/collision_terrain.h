// The collision group's terrain functions, callable from its other ports (they operate on guest memory as the originals).
#pragma once
#include "collision_util.h"

namespace collision {

// 0069b2f0: four lanes floored to int32, with the registers the original leaves.
struct Floor4 { X i, xmm1, xmm2, xmm3, xmm4, xmm5, xmm6; };
Floor4 floor4i(const Guest& g, const X& in);

// 007c5440: the window sampler (work = ESI cell work, out_center = EDI). Returns the eax / edx the original leaves.
struct SampleRegs { uint32_t eax, edx; };
SampleRegs terrain_sample(Guest& g, uint32_t work, uint32_t out_center, uint32_t out_nbr, uint32_t grid, uint32_t scale);
// 007c51e0: cell setup (work = ESI, dims = ECX, pos = EDX, cells = stack); returns eax, *edx_out the edx left.
uint32_t terrain_cell_setup(Guest& g, uint32_t work, uint32_t dims, uint32_t pos, uint32_t cells, uint32_t* edx_out);
// 007c52f0: window origin (work = ESI).
void terrain_cell_offsets(Guest& g, uint32_t work);
// 007c6160: the terrain triangles around a point.
void terrain_triangles(Guest& g, uint32_t q, uint32_t T, uint32_t out, uint32_t countp, int32_t cap, uint32_t extp);
// 007c5e80: height into point.y and the normal; returns eax.
uint32_t terrain_height_normal(Guest& g, uint32_t normal, uint32_t terrain, uint32_t point);

}  // namespace collision
