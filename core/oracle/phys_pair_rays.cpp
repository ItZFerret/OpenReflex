// phys_pair_rays.cpp -- oracle adapters for core/game/phys_pair_rays.c (00864460 sphere / ray list, 00872c90 ray list /
// height-field terrain). ECX and EDX are live (plan/live.tsv mask 6).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair_rays.h"
}
using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 00864460 (ECX = the list's position*, EDX = the sphere, ESI = M; stack the sphere's position*, the list, its rotation*,
// swap flag; RET 0x10)
RX_PORT(0x00864460, CollisionPair_SphereVsRays, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_sphere_rays(m.r[1], m.r[2], m.r[6], m.arg(0), m.arg(1), m.arg(2), m.arg(3), &edx);
    m.r[2] = edx;
    return m.ret(0x10);
}
// 00872c90 (ECX = the list, EDX = its position*; stack M, the list's rotation*, the terrain shape, swap flag; RET 0x10)
RX_PORT(0x00872c90, CollisionPair_RaysVsHFTerrain, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_rays_hfterrain(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), &edx);
    m.r[2] = edx;
    return m.ret(0x10);
}
