// phys_pair_circle.cpp -- oracle adapters for core/game/phys_pair_circle.c (008650c0 sphere / circle, 0086f3d0 cylinder /
// circle). ECX and EDX are live (plan/live.tsv mask 6: the dispatcher returns right after the pair call).
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair_circle.h"
}
using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

// 008650c0 (EDX = the circle's rotation*, CL = the swap flag; stack M, sphere, sphere position*, circle, circle position*;
// RET 0x14). ECX = the new count after a contact, else the entry ECX; EDX = the record / the last test mask's lane 1.
RX_PORT(0x008650c0, CollisionPair_SphereVsCircle, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_sphere_circle(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), &edx);
    m.r[2] = edx;
    return m.ret(0x14);
}
// 0086f3d0 (ECX = the circle's position*, EDX = the cylinder's rotation*; stack M, cylinder, cylinder position*, circle,
// circle rotation*, swap flag; RET 0x18). ECX = the security cookie; EDX = M after a contact, else 0 (0081c590's).
RX_PORT(0x0086f3d0, CollisionPair_CylinderVsCircle, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_cylinder_circle(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &edx);
    m.r[2] = edx;
    return m.ret(0x18);
}
