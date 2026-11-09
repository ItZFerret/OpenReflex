// Terrain deformation (wheel ruts / berms): the GPU pass of the original game on the CPU.
// Port of _re/formats/terrain_deform_reference.py (validated against a live capture; see
// _re/notes/terrain-deformation.md). The game itself queues the stamp quads (AddStamp
// 007c2ab0); this renders one frame's queue into the deformation grid exactly as
// FUN_007caf70 + ps_hft_deform do: per covered texel centre (top-left rule, 1/256 vertex
// snap), new = ps_hft_deform(previous, brush, softness, z), reading the previous grid,
// last triangle wins, only texels inside this frame's vertex tiles (128x32) are written.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace reflex_vm {

struct DeformModel {
    int mask_w = 0, mask_h = 0;
    std::vector<uint8_t> mask;                 // RGB u8, row 0 = +Z edge
    struct Mip { int w, h; std::vector<uint8_t> rgb; };
    std::vector<Mip> brush;
    bool load(const std::string& path);        // RXDF file (terrain_deform_pack.py)
    bool loaded() const { return !mask.empty() && !brush.empty(); }
};

struct StampVertex { float x, y, z, u, v; };   // clip space (already offset), depth, brush uv

struct DeformRect { int x0 = 0x7fffffff, z0 = 0x7fffffff, x1 = -1, z1 = -1; bool empty() const { return x1 < x0; } };

// Applies one frame of queued quads (6 vertices each) to `grid` (width x width, R32F).
// Returns the rectangle of texels written.
DeformRect apply_stamps(std::vector<float>& grid, int width, const std::vector<StampVertex>& verts, const DeformModel& model);

// ps_hft_deform for one texel (binary32 per instruction).
float ps_hft_deform(float D, float brush_r, float brush_g, float soft_r, float soft_g, float soft_b, float z);

}  // namespace reflex_vm
