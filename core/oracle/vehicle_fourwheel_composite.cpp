#include "oracle.h"
extern "C" {
#include "../game/vehicle_fourwheel_composite.h"
}
using namespace reflex_port;
RX_PORT(0x004c0e70,Utv_PlaceComposite,"rx_FourwheelComposite",0){rx_utv_place_composite(m.r[1]);return m.ret(0);}
RX_PORT(0x004a3170,Buggy_PlaceComposite,"rx_FourwheelComposite",0){rx_buggy_place_composite(m.r[1]);return m.ret(0);}
RX_PORT(0x0046b160,Truck_PlaceComposite,"rx_FourwheelComposite",0){rx_truck_place_composite(m.r[1]);return m.ret(0);}
