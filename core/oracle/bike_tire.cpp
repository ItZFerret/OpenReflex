// bike_tire.cpp -- oracle adapters for core/game/bike_tire.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_tire.h"
}
using namespace bike_oracle;

// 00447f10: EDI = S, stack dt, W (ret 8). Left over: ECX = the security cookie (__security_check_cookie's argument), EAX = &W+d0
// (active) or &W+e0 (airborne), EDX = S+d0 (85c770's mechanism argument, preserved by it) when active, else untouched.
RX_PORT(0x00447f10, BikeTire_ContactForce, "rx_bike", OUT_EAX | OUT_ECX | OUT_EDX | OUT_FPU) {
    const uint32_t S = m.r[7], W = m.arg(1);
    const bool active = m.mem.read(W + 0x100, 4) != 0;
    Scratch s(m);
    bike_tire_contact_force(S, argf(m, 0), W);
    s.restore(m);
    m.r[0] = active ? W + 0xd0 : W + 0xe0;
    m.r[1] = uint32_t(m.mem.read(0x9b45e4, 4));
    if (active) m.r[2] = S + 0xd0;
    return m.ret(8);
}
