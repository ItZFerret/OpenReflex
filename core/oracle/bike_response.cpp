// bike_response.cpp -- oracle adapters for core/game/bike_response.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_response.h"
}
using namespace bike_oracle;

// 004360b0: ECX = S (ret 0). No register is read by its callers (plan/live.tsv).
RX_PORT(0x004360b0, BikeSim_ContactResponse, "rx_bike", 0) {
    Scratch s(m);
    bike_contact_response(m.r[1]);
    s.restore(m);
    return m.ret(0);
}

// 00443af0: stack S (ret 4). No register is read by its caller.
RX_PORT(0x00443af0, Bike_ClampAngularVelocities, "rx_bike", 0) {
    Scratch s(m);
    bike_clamp_angular_velocities(m.arg(0));
    s.restore(m);
    return m.ret(4);
}
// 00447810: EDI = S (ret 0). No register is read by its caller.
RX_PORT(0x00447810, BikeWheels_SpinRelativeCorrection, "rx_bike", 0) {
    Scratch s(m);
    bike_wheels_spin_relative(m.r[7]);
    s.restore(m);
    return m.ret(0);
}
// 00443c30 (virtual slot 15): ECX = S, stack dt (ret 4). The step 00431d60 reads no register after the call.
RX_PORT(0x00443c30, RavenSimulationMotocross_vf15, "rx_bike", 0) {
    Scratch s(m);
    bike_slot15(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
