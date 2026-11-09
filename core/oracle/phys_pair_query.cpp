// phys_pair_query.cpp -- oracle adapters for core/game/phys_pair_query.c (the type-0x100 query shape against a sphere, a box, a
// cylinder and a ray list). The ray-query kernels leave the query object's ECX / EDX (its class is unknown: no constructor in
// the executable builds the shape), so only 00864dd0's ECX (the security cookie) is declared.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair_query.h"
}
using reflex_port::OUT_ECX;

// 00864dd0 (ESI = the sphere, EDI = its position*; stack M, the query shape, swap flag; RET 0xc)
RX_PORT(0x00864dd0, CollisionPair_SphereVsQuery, "rx_phys", OUT_ECX) {
    m.r[1] = phys_pair_sphere_query(m.r[6], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 0086b360 (ECX = the box, EDX = its position*; stack M, the box's rotation*, the query shape, swap flag; RET 0x10)
RX_PORT(0x0086b360, CollisionPair_BoxVsQuery, "rx_phys", 0) {
    phys_pair_box_query(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3));
    return m.ret(0x10);
}
// 0086f150 (ECX = the cylinder's position*, EDX = its rotation*, EDI = M; stack the cylinder, the query shape, swap flag;
// RET 0xc)
RX_PORT(0x0086f150, CollisionPair_CylinderVsQuery, "rx_phys", 0) {
    phys_pair_cylinder_query(m.r[1], m.r[2], m.r[7], m.arg(0), m.arg(1), m.arg(2));
    return m.ret(0xc);
}
// 00872970 (ECX = the list's position*, EDX = the query shape, ESI = the list, EDI = M; stack the list's rotation*, swap flag;
// RET 8)
RX_PORT(0x00872970, CollisionPair_RaysVsQuery, "rx_phys", 0) {
    phys_pair_rays_query(m.r[1], m.r[2], m.r[6], m.r[7], m.arg(0), m.arg(1));
    return m.ret(8);
}
