// phys_coll.cpp -- oracle adapters for core/game/phys_coll_*.c (the collision system: broadphase grid, shapes, terrain, pairs).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_coll_grid.h"
#include "../game/phys_coll_shape.h"
#include "../game/phys_coll_terrain.h"
#include "../game/phys_coll_pair.h"
}
using namespace phys_oracle;
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX; using reflex_port::OUT_XMM0;

static inline uint32_t lane_bits(const rx_v4& v, int i) { uint32_t b; std::memcpy(&b, &v.v[i], 4); return b; }

// ---------------------------------------------------------------- broadphase grid (collision_grid) and Int4 helpers

// 00882170 (xmm0 = a, xmm1 = b -> xmm0). The original leaves EAX = lane 3 of the result, ECX = a[3], EDX = a[2].
RX_PORT(0x00882170, Int4GreaterMask, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const rx_v4 a = xmm(m, 0), b = xmm(m, 1), r = phys_int4_gt_mask(a, b);
    set_xmm(m, 0, r);
    m.r[0] = lane_bits(r, 3); m.r[1] = lane_bits(a, 3); m.r[2] = lane_bits(a, 2);
    return m.ret(0);
}

// 008821e0 (xmm0 = a, xmm1 = b, EDX = flags word*). Leaves EAX = the flags word without the "none" bit, ECX = (mask lane 1 with
// its low byte replaced by the "none" flag).
RX_PORT(0x008821e0, Int4BelowFlags, "rx_phys", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const rx_v4 a = xmm(m, 0), b = xmm(m, 1);
    uint32_t flags = rx_rd32(m.r[2]);
    const rx_v4 r = phys_int4_below_flags(a, b, &flags);
    rx_wr32(m.r[2], flags);
    set_xmm(m, 0, r);
    m.r[0] = flags & ~0x20u;
    m.r[1] = (lane_bits(r, 1) & 0xffffff00u) | ((flags & 0x20u) ? 1u : 0u);
    return m.ret(0);
}

// 00882290 / 00882300 (xmm0 = a, xmm1 = b -> xmm0). EAX = a[3], ECX = b[3] (the last lane's loads).
RX_PORT(0x00882290, Int4Min, "rx_phys", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const rx_v4 a = xmm(m, 0), b = xmm(m, 1);
    set_xmm(m, 0, phys_int4_min(a, b));
    m.r[0] = lane_bits(a, 3); m.r[1] = lane_bits(b, 3);
    return m.ret(0);
}
RX_PORT(0x00882300, Int4Max, "rx_phys", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const rx_v4 a = xmm(m, 0), b = xmm(m, 1);
    set_xmm(m, 0, phys_int4_max(a, b));
    m.r[0] = lane_bits(a, 3); m.r[1] = lane_bits(b, 3);
    return m.ret(0);
}

// 0069b2f0 (xmm0 -> xmm0, integer lanes). EAX = lane 3, ECX = lane 1, EDX = lane 2.
RX_PORT(0x0069b2f0, Vec4FloorToInt, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM0) {
    const rx_v4 r = phys_vec4_floor_to_int(xmm(m, 0));
    set_xmm(m, 0, r);
    m.r[0] = lane_bits(r, 3); m.r[1] = lane_bits(r, 1); m.r[2] = lane_bits(r, 2);
    return m.ret(0);
}

// 0069b400 (xmm0, cdecl dest*). Leaves EAX = dest & ~15 (dest when null), ECX = the security cookie 0x9b45e4 (the cookie check
// returns cookie ^ esp ^ esp), xmm0 = lanes 2, 3 of the stored vector.
RX_PORT(0x0069b400, Vec4StoreAligned, "rx_phys", OUT_EAX | OUT_ECX | OUT_XMM0) {
    const uint32_t dest = m.arg(0);
    set_xmm(m, 0, phys_vec4_store_aligned(xmm(m, 0), dest));
    m.r[0] = dest ? (dest & 0xfffffff0u) : dest;
    m.r[1] = rx_rd32(0x9b45e4);
    return m.ret(0);
}

// 00882d90 (ESI grid, ECX hi*, stack lo*; RET 4)
RX_PORT(0x00882d90, BroadphaseGrid_QueryCells, "rx_phys", 0) {
    phys_grid_query_cells(m.r[6], m.arg(0), m.r[1]);
    return m.ret(4);
}
// 00883020 (ESI grid, EDX lo*, stack hi*; RET 4)
RX_PORT(0x00883020, BroadphaseGrid_QueryBoxes, "rx_phys", 0) {
    phys_grid_query_boxes(m.r[6], m.r[2], m.arg(0));
    return m.ret(4);
}
// 008826d0 / 00882980 (ESI grid, stack id; RET 4)
RX_PORT(0x008826d0, BroadphaseGrid_Insert, "rx_phys", 0) {
    phys_grid_insert(m.r[6], m.arg(0));
    return m.ret(4);
}
RX_PORT(0x00882980, BroadphaseGrid_Remove, "rx_phys", 0) {
    phys_grid_remove_fr(m.r[6], m.arg(0), m.r[4], m.r[5], m.r[3], m.r[7]);   // the frame's pushes: the caller's EBP / EBX / EDI
    return m.ret(4);
}
// 0084a1a0 (EDI id, stack object*, lo*, hi*; RET 0xc)
RX_PORT(0x0084a1a0, BroadphaseGrid_Update, "rx_phys", 0) {
    phys_grid_update(m.r[7], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}

// 0088bd90 (thiscall): EDX:EAX = the int64 at this.
RX_PORT(0x0088bd90, FUN_0088bd90, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint64_t v = phys_int64_load(m.r[1]);
    m.r[0] = uint32_t(v); m.r[2] = uint32_t(v >> 32);
    return m.ret(0);
}
// 0088bd50 (thiscall, stack src*; RET 4): EAX = this, ECX = high word, EDX = low word.
RX_PORT(0x0088bd50, FUN_0088bd50, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t self = m.r[1];
    const uint64_t v = phys_int64_assign(self, m.arg(0));
    m.r[0] = self; m.r[1] = uint32_t(v >> 32); m.r[2] = uint32_t(v);
    return m.ret(4);
}
// 0088be10 (thiscall, stack out*, lo, hi; RET 0xc): *out = *this + (hi:lo); EAX = out, ECX = sum high word, EDX = sum low word.
RX_PORT(0x0088be10, FUN_0088be10, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    phys_int64_add(m.r[1], out, m.arg(1), m.arg(2));
    m.r[0] = out; m.r[1] = rx_rd32(out + 4); m.r[2] = rx_rd32(out);
    return m.ret(0xc);
}

// ---------------------------------------------------------------- collision shapes (collision_shapes)

static inline uint32_t OUT_XMM(int i) { return reflex_port::OUT_XMM0 << i; }

// 008455f0 / 00846700 / 00845480 (thiscall: q*, pos*, out min*, out max*; RET 0x10): EAX = out min, ECX = pos (box / cylinder; the
// circle leaves ECX = this), EDX = out max.
RX_PORT(0x008455f0, CollisionShapeBox_vf2, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    phys_box_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(2); m.r[1] = m.arg(1); m.r[2] = m.arg(3);
    return m.ret(0x10);
}
RX_PORT(0x00846700, CollisionShapeDynamicCylinder_vf2, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    phys_dyncyl_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(2); m.r[1] = m.arg(1); m.r[2] = m.arg(3);
    return m.ret(0x10);
}
RX_PORT(0x00845480, CollisionShapeCircle_vf2, "rx_phys", OUT_EAX | OUT_EDX) {
    phys_circle_bounds(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(2); m.r[2] = m.arg(3);
    return m.ret(0x10);
}
// 00845a70: a tail call of vtable slot 2 (the Box's vf2 leaves EAX = out min, EDX = out max).
RX_PORT(0x00845a70, CollisionShapeBox_vf3, "rx_phys", OUT_EAX | OUT_EDX) {
    phys_box_vf3(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = m.arg(2); m.r[2] = m.arg(3);
    return m.ret(0x10);
}
// 00848340: EAX = out min without children, else out max (EDX is whatever the last child's vf3 left).
RX_PORT(0x00848340, CollisionShapeComposite_vf3, "rx_phys", OUT_EAX) {
    const uint32_t self = m.r[1];
    const bool empty = ((rx_rd32(self + 0x30) - rx_rd32(self + 0x2c)) >> 2) == 0;
    phys_composite_bounds(self, m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[0] = empty ? m.arg(2) : m.arg(3);
    return m.ret(0x10);
}

// 00848650 (EDI composite, ECX child index, stack q*, out*; RET 8): EAX = child rotation count, ECX = q.
RX_PORT(0x00848650, CompositeChildRotation, "rx_phys", 0) {
    const uint32_t comp = m.r[7];
    phys_composite_child_rotation(comp, m.r[1], m.arg(0), m.arg(1));
    m.r[0] = uint32_t(int32_t(rx_rd32(comp + 0x48) - rx_rd32(comp + 0x44)) >> 4); m.r[1] = m.arg(0);
    return m.ret(8);
}
// 008486f0 (EDI composite, ECX child index, stack q*, pos*, out*; RET 0xc): EAX = child offset count, ECX = q, EDX = pos.
RX_PORT(0x008486f0, CompositeChildPosition, "rx_phys", 0) {
    const uint32_t comp = m.r[7];
    phys_composite_child_position(comp, m.r[1], m.arg(0), m.arg(1), m.arg(2));
    m.r[0] = uint32_t(int32_t(rx_rd32(comp + 0x60) - rx_rd32(comp + 0x5c)) >> 4); m.r[1] = m.arg(0); m.r[2] = m.arg(1);
    return m.ret(0xc);
}
// 00848800 (ESI composite, ECX world pos*, EDX body pos*, stack index, q*; RET 8): EAX = child offset count.
RX_PORT(0x00848800, CompositeSetChildPosition, "rx_phys", 0) {
    const uint32_t comp = m.r[6];
    phys_composite_set_child_position(comp, m.r[1], m.r[2], m.arg(0), m.arg(1));
    m.r[0] = uint32_t(int32_t(rx_rd32(comp + 0x60) - rx_rd32(comp + 0x5c)) >> 4);
    return m.ret(8);
}
// 00848910 (ESI composite, ECX world rot*, EDX child index, stack q*; RET 4): EAX = q, EDX = child rotation count (ECX kept).
RX_PORT(0x00848910, CompositeSetChildRotation, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    const uint32_t comp = m.r[6];
    phys_composite_set_child_rotation(comp, m.r[1], m.r[2], m.arg(0));
    m.r[0] = m.arg(0); m.r[2] = uint32_t(int32_t(rx_rd32(comp + 0x48) - rx_rd32(comp + 0x44)) >> 4);
    return m.ret(4);
}

// 00845a80 (thiscall: out[8]*; RET 4): EAX = out. The original leaves its sign patterns in xmm0 (C = (1,1,-1,-1)), xmm1 (the last
// corner), xmm2 / xmm4 (the half extents), xmm3 (B = (1,-1,1,1)); xmm5..7, ECX, EDX are untouched.
RX_PORT(0x00845a80, CollisionShapeBox_Corners, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX | OUT_XMM(0) | OUT_XMM(1) | OUT_XMM(2) | OUT_XMM(3) | OUT_XMM(4) | OUT_XMM(5) | OUT_XMM(6) | OUT_XMM(7)) {
    const uint32_t self = m.r[1], out = m.arg(0);
    phys_box_corners(self, out);
    const rx_v4 h = v4_ld(self + 0x20);
    const float one = 1.0f, m1 = rx_bf(rx_rd32(0x9cd9e8));
    set_xmm(m, 0, v4(one, one, m1, m1)); set_xmm(m, 1, v4_ld(out + 0x70)); set_xmm(m, 2, h);
    set_xmm(m, 3, v4(one, m1, one, one)); set_xmm(m, 4, h);
    m.r[0] = out;
    return m.ret(4);
}

// 00845f20 / 00846bb0 (ECX point*, stack shape*; RET 4): EAX = 1 / 0 (the callers read only EAX).
RX_PORT(0x00845f20, CollisionShapeBox_ContainsPoint, "rx_phys", OUT_EAX) {
    m.r[0] = phys_box_contains_point(m.r[1], m.arg(0));
    return m.ret(4);
}
RX_PORT(0x00846bb0, FUN_00846bb0, "rx_phys", OUT_EAX) {
    m.r[0] = phys_cylinder_contains_point(m.r[1], m.arg(0));
    return m.ret(4);
}

// 008439f0 (thiscall): EAX = this+0x34.
RX_PORT(0x008439f0, XenonCore_vf2, "rx_phys", OUT_EAX) {
    m.r[0] = phys_xenon_core_vf2(m.r[1]);
    return m.ret(0);
}
// 008454c0 (EAX shape): xmm0 = the shape's vec4 +0x20.
RX_PORT(0x008454c0, FUN_008454c0, "rx_phys", OUT_XMM(0)) {
    set_xmm(m, 0, phys_shape_vec20(m.r[0]));
    return m.ret(0);
}
// 00843fc0 (thiscall, stack scale; RET 4): EAX = this, xmm0 = the scaled vector, xmm1 = the saved (old) vector.
RX_PORT(0x00843fc0, CollisionShapeSphere_vf4, "rx_phys", OUT_EAX | OUT_XMM(0) | OUT_XMM(1)) {
    const uint32_t self = m.r[1];
    phys_sphere_scale(self, argf(m, 0));
    set_xmm(m, 0, v4_ld(self + 0x20)); set_xmm(m, 1, v4_ld(self + 0x30));
    m.r[0] = self;
    return m.ret(4);
}
// 00843ff0 (thiscall): EAX = this, xmm0 = the restored vector.
RX_PORT(0x00843ff0, CollisionShapeSphere_vf5, "rx_phys", OUT_EAX | OUT_XMM(0)) {
    const uint32_t self = m.r[1];
    phys_sphere_restore(self);
    set_xmm(m, 0, v4_ld(self + 0x20));
    m.r[0] = self;
    return m.ret(0);
}
// 00844050 CollisionShapeDynamicCylinder::vf4 (thiscall, stack scale; RET 4): xmm0 = scaled radius, xmm1 = scaled height,
// xmm2 = the scale (movss loads zero the upper lanes; EAX untouched). Measured with the shape probe (finishui).
RX_PORT(0x00844050, CollisionShapeDynamicCylinder_vf4, "rx_phys", OUT_XMM(0) | OUT_XMM(1) | OUT_XMM(2)) {
    const uint32_t self = m.r[1];
    const float s = argf(m, 0);
    phys_dyncyl_scale(self, s);
    set_xmm(m, 0, (rx_v4){{F32(self, 0x20), 0.0f, 0.0f, 0.0f}});
    set_xmm(m, 1, (rx_v4){{F32(self, 0x24), 0.0f, 0.0f, 0.0f}});
    set_xmm(m, 2, (rx_v4){{s, 0.0f, 0.0f, 0.0f}});
    return m.ret(4);
}
// 00844080 CollisionShapeDynamicCylinder::vf5 (thiscall): xmm0 = the restored radius; EAX untouched.
RX_PORT(0x00844080, CollisionShapeDynamicCylinder_vf5, "rx_phys", OUT_XMM(0)) {
    const uint32_t self = m.r[1];
    phys_dyncyl_restore(self);
    set_xmm(m, 0, (rx_v4){{F32(self, 0x20), 0.0f, 0.0f, 0.0f}});
    return m.ret(0);
}
// 008454d0 (EDX shape, ECX direction out*, stack v*; RET 4): EAX = v (ECX kept).
RX_PORT(0x008454d0, FUN_008454d0, "rx_phys", OUT_EAX | OUT_ECX) {
    phys_shape_scale_to_radius(m.r[2], m.r[1], m.arg(0));
    m.r[0] = m.arg(0);
    return m.ret(4);
}

// ---------------------------------------------------------------- terrain queries (collision_terrain)

// 007c5440 (ESI cell work, EDI center out, stack neighbours out*, grid*, scale*; cdecl). Callers read nothing but the x87 depth.
RX_PORT(0x007c5440, TerrainSampleWindow, "rx_phys", 0) {
    phys_terrain_sample_window(m.r[6], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    m.r[1] = rx_rd32(0x9b45e4);   // the stack-cookie check leaves ECX = the cookie
    return m.ret(0);
}
// 007c51e0 (ESI work, ECX dims*, EDX position*, stack cells*; cdecl): EAX = 1 inside the grid else 0, EDX = first lane-mask sum, ECX = cookie.
RX_PORT(0x007c51e0, TerrainCellSetup, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t edx = 0;
    const uint32_t eax = phys_terrain_cell_setup(m.r[6], m.r[1], m.r[2], m.arg(0), &edx);
    m.r[0] = eax; m.r[1] = rx_rd32(0x9b45e4); m.r[2] = edx;
    return m.ret(0);
}
// 007c52f0 (ESI cell work; cdecl)
RX_PORT(0x007c52f0, TerrainCellOffsets, "rx_phys", OUT_ECX) {
    phys_terrain_cell_offsets(m.r[6]);
    m.r[1] = rx_rd32(0x9b45e4);
    return m.ret(0);
}
// 007c5380 (ESI grid, EDI cell work, ECX scale*, stack out*; cdecl): EAX = out, EDX = the first corner index.
RX_PORT(0x007c5380, FUN_007c5380, "rx_phys", OUT_EAX | OUT_EDX) {
    const uint32_t out = m.arg(0);
    m.r[2] = phys_terrain_sample_cell(m.r[6], m.r[7], m.r[1], out);
    m.r[0] = out;
    return m.ret(0);
}
// 007c5e80 (thiscall: ECX normal out*; stack terrain*, point*; RET 8): EAX = 1 / 0.
RX_PORT(0x007c5e80, TerrainHeightNormal, "rx_phys", OUT_EAX) {
    m.r[0] = phys_terrain_height_normal(m.r[1], m.arg(0), m.arg(1));
    return m.ret(8);
}
// 007c6160 (ECX query point*; stack terrain*, triangles out*, count out*, capacity, extent*; RET 0x14)
RX_PORT(0x007c6160, TerrainTriangles, "rx_phys", 0) {
    phys_terrain_triangles(m.r[1], m.arg(0), m.arg(1), m.arg(2), int32_t(m.arg(3)), m.arg(4));
    return m.ret(0x14);
}
// 007cda90 (EAX list, xmm0 value): ECX = the old count.
RX_PORT(0x007cda90, FUN_007cda90, "rx_phys", OUT_ECX) {
    m.r[1] = phys_float_list_push(m.r[0], lane_bits(xmm(m, 0), 0));
    return m.ret(0);
}
// 007cda50 (EAX list, stack two floats; RET 8): ECX = the entry address, xmm0 = (second float, 0, 0, 0).
RX_PORT(0x007cda50, FUN_007cda50, "rx_phys", OUT_ECX | OUT_XMM0) {
    m.r[1] = phys_float_pair_list_push(m.r[0], m.arg(0), m.arg(1));
    set_xmm(m, 0, v4_bits(m.arg(1), 0, 0, 0));
    return m.ret(8);
}

// ---------------------------------------------------------------- composite pairs (collision_pairs)

// 008732d0 (thiscall M; stack A, qA, pA, B, qB, pB, swap; RET 0x1c): EAX as the original leaves it, ECX = the stack cookie.
RX_PORT(0x008732d0, CollisionPair_CompositeVsComposite, "rx_phys", OUT_EAX | OUT_ECX) {
    m.r[0] = phys_pair_composite_vs_composite(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), m.arg(6));
    m.r[1] = rx_rd32(0x9b45e4);
    return m.ret(0x1c);
}
// 008656c0 / 00872860 (ECX rotA*, EDX posA*; stack M, shapeA, composite B, qB, pB, swap; RET 0x18)
RX_PORT(0x008656c0, CollisionPair_ShapeVsComposite, "rx_phys", OUT_EAX | OUT_ECX) {
    m.r[0] = phys_pair_shape_vs_composite(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = rx_rd32(0x9b45e4);
    return m.ret(0x18);
}
RX_PORT(0x00872860, CollisionPair_ShapeVsCompositeB, "rx_phys", OUT_EAX | OUT_ECX) {
    m.r[0] = phys_pair_shape_vs_composite_b(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = rx_rd32(0x9b45e4);
    return m.ret(0x18);
}
