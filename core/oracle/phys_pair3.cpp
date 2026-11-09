// phys_pair3.cpp -- oracle adapters for core/game/phys_pair3.c. ECX and EDX are live: the dispatcher returns right after the
// pair call, and its callers (CompositeVsComposite among them) read the EDX the generator left.
#include "phys_oracle.h"
extern "C" {
#include "../game/phys_pair3.h"
}
using reflex_port::OUT_ECX; using reflex_port::OUT_EDX;

RX_PORT(0x008657d0, CollisionPair_BoxVsBox, "rx_phys", OUT_ECX | OUT_EDX) {
    uint32_t edx = m.r[2];
    phys_pair_box_box(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &edx);
    m.r[2] = edx;
    m.r[1] = m.mem.read(0x009b45e4, 4);
    return m.ret(0x18);
}
// 00867ca0: the kernels it calls through the original get the dispatcher's registers (EBP / the saved registers in their frames)
RX_PORT(0x00867ca0, CollisionPair_BoxVsCylinder, "rx_phys", OUT_ECX | OUT_EDX) {
    rx_regs caller;
    std::memset(&caller, 0, sizeof caller);
    for (int i = 0; i < 8; i++) caller.r[i] = m.r[i];
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_box_cylinder(m.r[2], m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &caller, &edx);
    m.r[2] = edx;
    return m.ret(0x18);
}
RX_PORT(0x0086bd80, CollisionPair_CylinderVsCylinder, "rx_phys", OUT_ECX | OUT_EDX) {
    rx_regs caller;
    std::memset(&caller, 0, sizeof caller);
    for (int i = 0; i < 8; i++) caller.r[i] = m.r[i];
    uint32_t edx = m.r[2];
    m.r[1] = phys_pair_cylinder_cylinder(m.r[1], m.r[2], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4), m.arg(5), &caller, &edx);
    m.r[2] = edx;
    return m.ret(0x18);
}
