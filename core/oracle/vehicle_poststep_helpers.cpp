#include "oracle.h"
extern "C" {
#include "../game/vehicle_poststep_helpers.h"
}
using namespace reflex_port;
#define G "rx_VehiclePoststepHelpers"
RX_PORT(0x0085c6a0,VehicleMechanismNodePoststep,G,0){rx_vehicle_mechanism_node_poststep(m.arg(0));return m.ret(4);}
RX_PORT(0x004d7fa0,VehicleWheelOffsetDebug,G,OUT_ECX|OUT_EDX){uint32_t sim=m.r[2],wheel=m.arg(0);rx_vehicle_wheel_offset_debug(sim,wheel,rx_bf(m.arg(1)));m.r[1]=sim+0xa0u*rx_rd32(wheel);return m.ret(8);}
RX_PORT(0x00459de0,VehicleWheelAxisDebug,G,OUT_EDX){rx_vehicle_wheel_axis_debug(m.r[2],m.arg(0));return m.ret(4);}
#undef G
