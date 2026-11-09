// bike_slot13.cpp -- oracle adapters for core/game/bike_slot13.c.
#include "bike_oracle.h"
extern "C" {
#include "../game/bike_slot13.h"
}
using namespace bike_oracle;

// 00445800: ESI = S, stack dt (ret 4). Its caller 00444b90 writes EDX next and calls 00446f80 (inputs EDI only): no register
// left by it is read (live.tsv's EAX / ECX come from that chain).
RX_PORT(0x00445800, BikeWheels_UpdateRollingOrientation, "rx_bike", 0) {
    Scratch s(m);
    bike_wheels_update_rolling_orientation(m.r[6], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 00446f80: EDI = S (ret 0). Its caller 00444b90 returns right after it (cookie check): no register is read.
RX_PORT(0x00446f80, Fn_00446f80, "rx_bike", 0) {
    Scratch s(m);
    bike_rest_pin(m.r[7]);
    s.restore(m);
    return m.ret(0);
}
// 00435b00: ESI = S (ret 0). The bike's caller sets EDX before its next call; no register left by it is read there.
RX_PORT(0x00435b00, Simulation_BodyProbeContacts, "rx_bike", 0) {
    Scratch s(m);
    bike_body_probe_contacts(m.r[6]);
    s.restore(m);
    return m.ret(0);
}
// 00444b90 (virtual slot 13): ECX = S, stack dt (ret 4). The step reads no register after it.
RX_PORT(0x00444b90, RavenSimulationMotocross_vf13, "rx_bike", 0) {
    Scratch s(m);
    bike_slot13(m.r[1], argf(m, 0));
    s.restore(m);
    return m.ret(4);
}
// 00438260: ECX = &out point, EDI = &out normal, stack (&q, &p, &extent, triangles, count) (ret 0x14). EAX = hit, EDX = the
// &q argument after a hit (reloaded for the rotation), else the ring index (still 0).
RX_PORT(0x00438260, Wheel_TriangleContactTest, "rx_bike", OUT_EAX | OUT_EDX) {
    Scratch s(m);
    int32_t hits = 0;
    const uint32_t hit = bike_wheel_triangle_contact(m.r[1], m.r[7], m.arg(0), m.arg(1), m.arg(2), m.arg(3), int32_t(m.arg(4)), &hits);
    s.restore(m);
    m.r[0] = hit;
    m.r[2] = hits > 0 ? m.arg(0) : 0;
    return m.ret(0x14);
}
