#include "oracle.h"
extern "C" {
#include "../game/vehicle_fourwheel_ground_place.h"
}
using namespace reflex_port;
RX_PORT(0x004cc980,FourWheel_GroundPlaceTruck,"rx_FourWheelGroundPlace",0) {
 rx_fourwheel_ground_place_truck(m.r[1],m.arg(0),m.arg(1),m.arg(2),m.arg(3),m.arg(4));return m.ret(20);
}
RX_PORT(0x004b2180,FourWheel_GroundPlaceBuggy,"rx_FourWheelGroundPlace",0) {
 rx_fourwheel_ground_place_buggy(m.r[1],m.arg(0),m.arg(1),m.arg(2),m.arg(3),m.arg(4));return m.ret(20);
}
