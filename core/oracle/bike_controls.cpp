// bike_controls.cpp -- oracle adapters for core/game/bike_controls.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_controls.h"
}
using namespace bike_oracle;

// 0044c170 (virtual slot 9): ECX = S, stack dt (ret 4). The step 00431d60 reads no register after it (reloads EDX / ECX).
RX_PORT(0x0044c170, RavenSimulationMotocross_vf9, "rx_bike", 0) {
    check_cw(m, "0044c170");
    Scratch s(m);
    bike_controls_update(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0044e0d0: EDI = S, stack dt (ret 4). Its only caller (0044c170) returns right after it: no register is read.
RX_PORT(0x0044e0d0, BikeControls_UpdateRiderOff, "rx_bike", 0) {
    Scratch s(m);
    bike_controls_rider_off(m.r[7], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 0043e9f0: stack S, dt (ret 8). No register is read by its callers.
RX_PORT(0x0043e9f0, BikeControls_SmoothGroundDirection, "rx_bike", 0) {
    Scratch s(m);
    bike_smooth_ground_direction(m.arg(0), argf(m, 1));
    s.restore(m);
    return m.ret(8);
}
