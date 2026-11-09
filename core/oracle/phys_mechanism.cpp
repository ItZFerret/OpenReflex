// mechanism.cpp -- oracle adapters for core/game/mechanism.c.
#include "oracle.h"
extern "C" {
#include "../game/phys_mechanism.h"
}
using reflex_port::OUT_EAX; using reflex_port::OUT_ECX;

// 0085d270 (EDX = M, stack arg = dt; RET 4). The original leaves ECX = node count, EAX = the last node pointer (+0x50 + n*0xa0).
RX_PORT(0x0085d270, Mechanism_IntegrateVelocities, "rx_phys", OUT_EAX | OUT_ECX) {
    const uint32_t M = m.r[2];
    rx_mechanism_integrate_velocities(M, rx_oracle::f32(m.arg(0)));
    const int32_t n = int32_t(m.mem.read(M + 0x1c, 4));
    if (n > 0) { m.r[1] = uint32_t(n); m.r[0] = M + 0x50 + 0xa0u * uint32_t(n); } else m.r[1] = 0;
    return m.ret(4);
}
