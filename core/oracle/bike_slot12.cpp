// bike_slot12.cpp -- oracle adapters for core/game/bike_slot12.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_slot12.h"
}
using namespace bike_oracle;

// 00443610 (virtual slot 12): ECX = S, stack dt (ret 4). The step 00431d60 reads no register after it.
RX_PORT(0x00443610, RavenSimulationMotocross_vf12, "rx_bike", 0) {
    check_cw(m, "00443610");
    Scratch s(m);
    bike_slot12(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0043f270: stack S (ret 4). Leaves EAX = S, ECX = mask lane 1, EDX = lanes 2 + 1 + 0 (the "all three" test).
RX_PORT(0x0043f270, Simulation_ClampRootAngularVelocity, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    Scratch s(m);
    const uint32_t S = m.arg(0);
    uint32_t lane1, sum;
    bike_clamp_root_spin(S, &lane1, &sum);
    s.restore(m);
    m.r[0] = S; m.r[1] = lane1; m.r[2] = sum;
    return m.ret(4);
}
// 00440c40: stack x, y, EDI = &out a, ESI = &out b (ret 8). Leaves ECX = the bits of r (its pop ecx pops the local r).
RX_PORT(0x00440c40, Stick_CircleToSquare, "rx_bike", OUT_ECX | OUT_FPU) {
    check_cw(m, "00440c40");
    Scratch s(m);
    const float r = bike_stick_circle_to_square(argf(m, 0), argf(m, 1), m.r[7], m.r[6]);
    s.restore(m);
    m.r[1] = rx_oracle::bits(r);
    return m.ret(8);
}
// 00441300: ESI = S, stack dt (ret 4). Leaves ECX = the security cookie (its check's argument, restored by the check).
RX_PORT(0x00441300, Simulation_GroundCorrection, "rx_bike", OUT_ECX | OUT_FPU) {
    check_cw(m, "00441300");
    Scratch s(m);
    bike_ground_correction(m.r[6], argf(m, 0));
    s.restore(m);
    m.r[1] = U32(0, 0x009b45e4);
    return m.ret(4);
}
