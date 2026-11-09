// phys_terrain.cpp -- oracle adapters for core/game/phys_terrain.c (LTCG register conventions; the callers' live registers
// from native/game/plan/live.tsv are reproduced: ECX = the stack cookie after __security_check_cookie).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_terrain.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX; using reflex_port::OUT_XMM0;

static uint32_t cookie() { return rx_rd32(0x9b45e4); }

// 007c5120 (EDI terrain, EDX point, EAX layer): EAX / EDX / xmm0 as the grid sampler leaves them, ECX = [terrain+0x44ac].
RX_PORT(0x007c5120, TerrainMaterialAt, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t terrain = m.r[7];
    rx_regs r, caller;
    std::memset(&caller, 0, sizeof caller);
    for (int i = 0; i < 8; i++) caller.r[i] = m.r[i];
    for (int i = 0; i < 8; i++) caller.xmm[i] = xmm(m, i);
    m.r[0] = phys_terrain_material_at(terrain, m.r[2], m.r[0], &caller, &r);
    m.r[2] = r.r[2]; m.r[1] = rx_rd32(terrain + 0x44ac); set_xmm(m, 0, r.xmm[0]);
    return m.ret(0);
}
RX_PORT(0x007c5950, TerrainBaseHeightAt, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[0] = phys_terrain_base_height_at(m.r[1], m.arg(0), &edx);
    m.r[1] = cookie(); m.r[2] = edx; return m.ret(4);
}
RX_PORT(0x007c5aa0, TerrainHeightAt, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[0] = phys_terrain_height_at(m.arg(0), m.arg(1), &edx);
    m.r[1] = cookie(); m.r[2] = edx; return m.ret(8);
}
// 007c5c90: EDX = what TerrainSampleWindow leaves (not reproduced; not declared)
RX_PORT(0x007c5c90, TerrainBaseHeightNormal, "rx_phys", OUT_EAX | OUT_ECX) {
    m.r[0] = phys_terrain_base_height_normal(m.r[1], m.arg(0), m.arg(1));
    m.r[1] = cookie(); return m.ret(8);
}
// 007c7350 (ECX box, EDX coarse grid): EDX = the grid's cell array
RX_PORT(0x007c7350, TerrainSegment_CoarseReject, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t coarse = m.r[2];
    m.r[0] = phys_terrain_segment_coarse_reject(m.r[1], coarse);
    m.r[2] = rx_rd32(coarse + 0xd4); return m.ret(0);
}
// 007c34b0: ECX = the track set
RX_PORT(0x007c34b0, TerrainTracks_AllocSlotDefault, "rx_phys", OUT_EAX | OUT_ECX) {
    m.r[0] = uint32_t(phys_tracks_alloc_slot()); m.r[1] = rx_rd32(0x00a9577c); return m.ret(0);
}
// 007c4560 (ESI tracks, EAX index): the registers the original's path leaves (ECX = the strip offset, EAX = strips / &strip+8 /
// the new ring position, EDX = strips, xmm0 = 0 and xmm1 = the old pending length once the point count is updated).
RX_PORT(0x007c4560, TerrainTracks_CutStrip, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const uint32_t tracks = m.r[6]; const int32_t index = int32_t(m.r[0]);
    if (index < 0) { phys_tracks_cut_strip(tracks, index); return m.ret(0); }
    const uint32_t strips = rx_rd32(tracks + 0x24), off = 0xc0u * uint32_t(index), strip = strips + off;
    const uint32_t cut = rx_rd32(strip + 0xa0), count = rx_rd32(strip + 8), pending = rx_rd32(strip + 0x14);
    phys_tracks_cut_strip(tracks, index);
    m.r[1] = off; m.r[0] = strips;
    if (cut == 0) {
        m.r[2] = strips;
        if (count != 0) {
            rx_v4 z; std::memset(&z, 0, sizeof z); set_xmm(m, 0, z);
            rx_v4 p = z; std::memcpy(&p.v[0], &pending, 4); set_xmm(m, 1, p);
            const int32_t n = int32_t(count) + 4 + (rx_bf(pending) > 0.0f ? 2 : 0);
            m.r[0] = n >= 0x7d ? rx_rd32(strip + 4) : strip + 8;
        }
    }
    return m.ret(0);
}
// 007c66f0 (ECX terrain, EAX centre*; stack count*, extent*): callers read no registers.
RX_PORT(0x007c66f0, TerrainGridVertices, "rx_phys", 0) {
    phys_terrain_grid_vertices(m.r[1], m.r[0], m.arg(0), m.arg(1)); return m.ret(8);
}

// ---- the segment query (phys_terrain_ray.c): EAX = hit, ECX / EDX as the original leaves them
// 007c6cc0 (ESI terrain, ECX start*, EAX end*; stack hit*, normal*; RET 8)
RX_PORT(0x007c6cc0, TerrainSegmentQuery, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t regs[2] = {m.r[1], m.r[2]};
    m.r[0] = phys_terrain_segment_query(m.r[6], m.r[1], m.r[0], m.arg(0), m.arg(1), regs);
    m.r[1] = regs[0]; m.r[2] = regs[1]; return m.ret(8);
}
// 007c73e0 (EAX end*; stack terrain*, hit*, normal*, start*; RET 0x10)
RX_PORT(0x007c73e0, TerrainSegment_CoarseTraversal, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t regs[2] = {m.r[1], m.r[2]};
    m.r[0] = phys_terrain_coarse_traversal(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), regs);
    m.r[1] = regs[0]; m.r[2] = regs[1]; return m.ret(0x10);
}
// 007c83b0 (ESI P*, EAX Q*, EDI direction*; stack terrain*, hit*, normal*, (x, z) direction*; RET 0x10)
RX_PORT(0x007c83b0, TerrainSegment_FineTraversal, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t regs[2] = {m.r[1], m.r[2]};
    m.r[0] = phys_terrain_fine_traversal(m.r[6], m.r[0], m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3), regs);
    phys_terrain_fine_traversal_saved_regs(m.r[4], m.r[3], m.r[5]);   // its prologue's pushes (as the caller's port writes them)
    m.r[1] = regs[0]; m.r[2] = regs[1]; return m.ret(0x10);
}
// 007c7d10 (EAX base grid; stack terrain*, hit*, normal*, segment*; RET 0x10)
RX_PORT(0x007c7d10, TerrainSegment_CellIntersect, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t regs[2] = {m.r[1], m.r[2]};
    m.r[0] = phys_terrain_cell_intersect(m.r[0], m.arg(0), m.arg(1), m.arg(2), m.arg(3), regs);
    m.r[1] = regs[0]; m.r[2] = regs[1]; return m.ret(0x10);
}
// 007c2ab0 (ESI patch set, EAX p0*; stack p1*, depth0, depth1, width0, width1; RET 0x14): EAX = the count address, EDX as left
RX_PORT(0x007c2ab0, DeformablePatch_AddStamp, "rx_phys", OUT_EAX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[0] = phys_patch_add_stamp(m.r[6], m.r[0], m.arg(0), argf(m, 1), argf(m, 2), argf(m, 3), argf(m, 4), &edx);
    m.r[2] = edx; return m.ret(0x14);
}
// 007c3510 (EAX tracks; stack index, pos*, prev*, width, offset, intensity, flag; RET 0x1c): ECX as the original leaves it
RX_PORT(0x007c3510, TerrainTracks_AddPoint, "rx_phys", OUT_ECX) {
    m.r[1] = phys_tracks_add_point(m.r[0], int32_t(m.arg(0)), m.arg(1), m.arg(2), argf(m, 3), argf(m, 4), argf(m, 5), m.arg(6), m.r[1]);
    return m.ret(0x1c);
}
