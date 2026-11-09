#include "oracle.h"
#include "../game/vehicle_atv_wheels.h"
using namespace reflex_port;
RX_PORT(0x00487b30, AtvWheels, "rx_AtvWheels", 0) {
    rx_atv_vehicle_wheels(m.arg(0), rx_bf(m.arg(1)));
    return m.ret(8);
}

RX_PORT(0x00424340, AtvWaterSingleton, "rx_AtvWheels", OUT_EAX) {
    return m.ret(0,rx_atv_water_singleton());
}
RX_PORT(0x0045f500, AtvWaterForce, "rx_AtvWheels", 0) {
    rx_atv_vehicle_add_water_force(m.r[0],rx_bf(m.arg(0)));
    return m.ret(4);
}
