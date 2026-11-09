#include "oracle.h"
#include "f80/f80.h"
extern "C" {
#include "../game/controller_axis_query.h"
}
using namespace reflex_port;
RX_PORT(0x005485e0,Controller_StickDriveAxis,"rx_ControllerAxisQuery",OUT_FPU) {
 const float value=rx_controller_stick_drive_axis(m.r[6]);
 m.fpu.push_back(reflex_f80::from_f32(rx_fb(value)));return m.ret(0);
}
RX_PORT(0x00811c90,Controller_CompoundAvailable,"rx_ControllerAxisQuery",OUT_EAX) {
 return m.ret(4,rx_controller_compound_available(m.r[1],m.arg(0)));
}
