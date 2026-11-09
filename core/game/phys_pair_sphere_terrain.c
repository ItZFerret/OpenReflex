/* 00864ec0, original sphere/height-field narrow phase. The original triangle
 * query, closest-point kernel, lane comparisons and manifold clamp are retained.
 * Source: reflex_011_0082bac0.c; checked against 00864ec0..008650b8 disassembly. */
#include "phys_pair_sphere_terrain.h"
#include "phys_coll_terrain.h"
#include "phys_tri.h"
#include "world_veh.h"

void phys_pair_sphere_hfterrain(rx_addr sphere, rx_addr position, rx_addr manifold, rx_addr terrain_shape, uint32_t flag) {
    const rx_addr entry = rx_sp(), F = ((entry - 4) & ~15u) - 0x78 - 8;
    rx_set_sp(F);
    SU8(entry, 12, (flag & 0xff) == 0);
    rx_addr terrain;
    PHYS_CALL_SP(0, terrain = rx_world_find_by_controller(U32(terrain_shape, 0x20)));
    v4_st(F + 0x10, v4_ld(position));
    v4_st(F + 0x40, v4_ld(sphere + 0x20));
    SU32(0, 0x00dcbe50, U32(0, 0x00dcbe50) | 1);
    PHYS_CALL_SP(20, phys_terrain_triangles(F + 0x10, terrain, 0x00dc02d0, F + 0xc, 1000, F + 0x40));
    for (rx_addr tri = 0x00dc02d0; I32(F, 0xc) > 0; tri += 0x30) {
        v4_st(F + 0x50, v4_ld(tri));
        v4_st(F + 0x10, v4_ld(tri + 0x10));
        v4_st(F + 0x30, v4_ld(tri + 0x20));
        PHYS_CALL_SP(12, phys_triangle_closest_point(F + 0x50, position, F + 0x60, F + 0x10, F + 0x30));
        const rx_v4 point = v4_ld(F + 0x60), radius = v4_ld(F + 0x40);
        const rx_v4 delta = v4_sub(point, v4_ld(position)), sq = v4_mul(delta, delta);
        /* Distance sums x,z,y; the normal below sums x,y,z. */
        const rx_v4 distance = v4_sqrt(v4_splat(phys_add(phys_add(sq.v[0], sq.v[2]), sq.v[1])));
        const rx_v4 mask = v4_cmplt(distance, radius);
        v4_st(F + 0x20, mask);
        if (v4_lb(mask, 0) + v4_lb(mask, 1) + v4_lb(mask, 2) == 0xfffffffdu) {
            const rx_v4 a = v4_ld(F + 0x50);
            const rx_v4 ab = v4_sub(v4_ld(F + 0x10), a), ac = v4_sub(v4_ld(F + 0x30), a);
            rx_v4 normal = phys_normalize3(phys_cross_ba(ac, ab));
            if ((flag & 0xff) == 0) normal = v4_sub(v4_zero(), normal);
            int32_t index = I32(manifold, 0);
            if (index >= 29) index = 29;
            const rx_addr record = manifold + 0x30u * (uint32_t)index;
            v4_st(record + 0x10, point);
            v4_st(record + 0x20, normal);
            v4_st(record + 0x30, v4_sub(radius, distance));
            SU32(manifold, 0, (uint32_t)index + 1);
            SU32(manifold, 0x6a0, 1);
        }
        SU32(F, 0xc, U32(F, 0xc) - 1);
    }
    rx_set_sp(entry);
}
RX_FUNCTION(0x00864ec0, phys_pair_sphere_hfterrain)
