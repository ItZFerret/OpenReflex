// phys_pair2.cpp -- oracle adapters for core/game/phys_pair2.c (ECX = the cookie after the stack check).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair2.h"
}
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

RX_PORT(0x008716b0, Triangles_CircleContact, "rx_phys", OUT_ECX | OUT_EDX) {
    m.r[2] = phys_pair_triangles_circle(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x18);
}
// 0085ee70 (ECX ends, EDX first vertices; cdecl second, third vertices, starts, hits*, points*, normals*): EAX / EDX as left
RX_PORT(0x0085ee70, Rays6_Triangles, "rx_phys", OUT_EAX | OUT_ECX | OUT_EDX) {
    uint32_t edx = 0;
    m.r[0] = phys_rays6_triangles(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &edx);
    m.r[2] = edx; m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0);
}
// 0086fac0 (ECX segment mesh position*, EDX triangle mesh rotation*, ESI M; stack T, PT, S, QS, flag; RET 0x14). The EDX the
// original leaves (a loop leftover) is not reproduced (its caller, the contact dispatcher, does not read it).
RX_PORT(0x0086fac0, Triangles_DynamicRayContact, "rx_phys", OUT_ECX) {
    phys_pair_triangles_segments(m.r[1], m.r[2], m.r[6], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x14);
}
