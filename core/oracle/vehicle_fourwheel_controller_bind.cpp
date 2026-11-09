#include "oracle.h"
extern "C" {
#include "../game/vehicle_fourwheel_controller_bind.h"
}
using namespace reflex_port;
RX_PORT(0x004d9460,FourWheel_BindControllerTruck,"rx_FourWheelControllerBind",OUT_EAX|OUT_ECX) {
 const rx_addr sim=m.r[1];rx_fourwheel_bind_controller_truck(sim,m.arg(0));
 m.r[0]=U32(sim,0x4c);m.r[1]=U32(sim,0x50);return m.ret(4);
}
RX_PORT(0x004be490,FourWheel_BindControllerBuggy,"rx_FourWheelControllerBind",OUT_EAX|OUT_ECX) {
 const rx_addr sim=m.r[1];rx_fourwheel_bind_controller_buggy(sim,m.arg(0));
 m.r[0]=U32(sim,0x4c);m.r[1]=U32(sim,0x50);return m.ret(4);
}
