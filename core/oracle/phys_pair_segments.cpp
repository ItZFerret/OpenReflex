// phys_pair_segments.cpp -- oracle adapter for core/game/phys_pair_segments.c (0086e750). ECX and EDX are live (plan/live.tsv
// mask 6: the dispatcher returns right after the pair call).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair_segments.h"
}
using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

RX_PORT(0x0086e750, CollisionPair_CylinderVsSegments, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_cylinder_segments(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &edx);
    m.r[2] = edx;
    return m.ret(0x18);
}
