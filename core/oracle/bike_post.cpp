// bike_post.cpp -- oracle adapters for core/game/bike_post.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_post.h"
}
using namespace bike_oracle;

// 0043ed40: stack S, dt (ret 8). Leaves EAX = S (ECX / EDX untouched).
RX_PORT(0x0043ed40, Fn_0043ed40, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    Scratch s(m);
    const uint32_t S = m.arg(0);
    bike_pitch_spring(S, argf(m, 1));
    s.restore(m);
    m.r[0] = S;
    return m.ret(8);
}
// 0043c1a0 (virtual slot 17): ECX = S, stack dt (ret 4). The step reads no register after it.
RX_PORT(0x0043c1a0, RavenSimulationMotocross_vf17, "rx_bike", 0) {
    Scratch s(m);
    bike_slot17(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0043e200 (virtual slot 16): ECX = S (ret 0). The step reads no register after it.
RX_PORT(0x0043e200, RavenSimulationMotocross_vf16, "rx_bike", 0) {
    Scratch s(m);
    bike_slot16(m.r[1]);
    s.restore(m);
    return m.ret(0);
}
// 0044ef70 (virtual slot 32): ECX = S (ret 0). Its callers (00435b00 ...) reload the registers after the call.
RX_PORT(0x0044ef70, RavenSimulationMotocross_vf32, "rx_bike", 0) {
    Scratch s(m);
    bike_slot32(m.r[1]);
    s.restore(m);
    return m.ret(0);
}
