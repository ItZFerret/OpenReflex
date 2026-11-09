// bike_setup.cpp -- oracle adapters for core/game/bike_setup.c (group "rx_loop").
#include "oracle.h"
extern "C" {
#include "../game/bike_setup.h"
}
using namespace reflex_port;

// thiscall (ECX = simulation; position, direction, out position, out forward, out up); ret 0x14 (the caller 00460020 reads none)
RX_PORT(0x0043d1f0, RavenSimulationMotocross_vf20, "rx_loop", 0) {
    rx_sim_moto_ground_place(m.r[1], m.arg(0), m.arg(1), m.arg(2), m.arg(3), m.arg(4));
    return m.ret(0x14);
}
