// phys_pair.cpp -- oracle adapters for core/game/phys_pair.c (pair contact generators).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair.h"
}
using namespace phys_oracle;
using namespace reflex_port;

// 00861a60 (ECX = position A*, EDX = sphere A; 4 stack args; RET 0x10). ECX / EDX are live: the dispatcher returns right
// after the generator. On a contact they are count + 1 / M + 0x30 * count; on a miss the compare mask's lane 1 and its sum.
RX_PORT(0x00861a60, CollisionPair_SphereVsSphere, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t ecx = m.r[1], edx = m.r[2];
    phys_pair_sphere_sphere(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), &ecx, &edx);
    m.r[1] = ecx; m.r[2] = edx;
    return m.ret(0x10);
}
// 00861ba0 (ECX, EDX; 6 stack args; RET 0x18): ECX = the stack cookie value (the check leaves it).
RX_PORT(0x00861ba0, CollisionPair_SphereVsBox, "rx_phys", OUT_ECX) {
    phys_pair_sphere_box(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x18);
}
RX_PORT(0x00862da0, CollisionPair_SphereVsCylinder, "rx_phys", 0) {
    phys_pair_sphere_cylinder(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
// 00873010: ECX = the stack cookie value (the check), EDX not reproduced (callee leftover).
RX_PORT(0x00873010, CollisionPair_CompositeVsHFTerrain, "rx_phys", OUT_ECX) {
    phys_pair_composite_hfterrain(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x18);
}
RX_PORT(0x0086b4d0, CollisionPair_BoxVsHFTerrain, "rx_phys", OUT_ECX) {
    phys_pair_box_hfterrain(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x10);
}
// 0086f2a0 (ECX = cylinder position*, EDX = cylinder rotation*, ESI = height-field shape, EDI = M; 3 stack args; RET 0xc)
RX_PORT(0x0086f2a0, CollisionPair_CylinderVsHFTerrain, "rx_phys", 0) {
    phys_pair_cylinder_hfterrain(m.r[1], m.r[2], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
RX_PORT(0x00869d40, CollisionPair_BoxVsTriangles, "rx_phys", OUT_ECX) {
    phys_pair_box_triangles(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x18);
}
RX_PORT(0x00863f70, CollisionPair_SphereVsTriangles, "rx_phys", OUT_ECX) {
    phys_pair_sphere_triangles(m.r[1], m.r[2], m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x10);
}
RX_PORT(0x0086e090, CollisionPair_CylinderVsTriangles, "rx_phys", OUT_ECX) {
    phys_pair_cylinder_triangles(m.r[1], m.r[2], m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x14);
}
