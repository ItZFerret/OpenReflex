// bike_air.cpp -- oracle adapters for core/game/bike_air.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_slot12.h"
}
using namespace bike_oracle;

// 00440da0: EDI = S, stack dt (ret 4). EAX as the original leaves it (see bike_stunt_latches). EDX is the cone tests'
// (0042de70's CRT callees') leftover, not reproduced (the rx_phys port of 0042de70 does not reproduce it either).
RX_PORT(0x00440da0, Simulation_StuntLatches, "rx_bike", OUT_EAX) {
    check_cw(m, "00440da0");
    Scratch s(m);
    const uint32_t eax = bike_stunt_latches(m.r[7], argf(m, 0));
    s.restore(m);
    m.r[0] = eax;
    return m.ret(4);
}
// 0043f2e0: stack S, dt (ret 8). Leaves EDX = lane 1 of the angular speed cap mask, EAX = lanes 2 + 1 + 0.
RX_PORT(0x0043f2e0, Simulation_AirControl, "rx_bike", OUT_EAX | OUT_EDX | OUT_FPU) {
    check_cw(m, "0043f2e0");
    Scratch s(m);
    const uint32_t lane = bike_air_control(m.arg(0), argf(m, 1));
    s.restore(m);
    m.r[0] = 3u * lane; m.r[2] = lane;
    return m.ret(8);
}
