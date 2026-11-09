#include "oracle.h"
#include "../game/vehicle_fourwheel_place.h"
using namespace reflex_port;
RX_PORT(0x004c6c40,UtvResetGround,"rx_FourwheelPlace",0){rx_utv_reset_ground(m.r[1]);return m.ret(0);}
RX_PORT(0x004ac2e0,BuggyResetGround,"rx_FourwheelPlace",0){rx_buggy_reset_ground(m.r[1]);return m.ret(0);}
RX_PORT(0x00473210,TruckResetGround,"rx_FourwheelPlace",0){rx_truck_reset_ground(m.r[1]);return m.ret(0);}
