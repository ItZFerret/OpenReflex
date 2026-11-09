#include "oracle.h"
extern "C" {
#include "../game/vehicle_water_wake.h"
}
using namespace reflex_port;
RX_PORT(0x007f9510,WakeAppendCenter,"rx_WaterWake",0){rx_wake_append_center(m.r[0],m.arg(0),m.arg(1));return m.ret(8);}
RX_PORT(0x007f9550,WakeAppendFloat,"rx_WaterWake",0){rx_wake_append_float(m.r[0],uint32_t(m.xmm[0].lo));return m.ret(0);}
RX_PORT(0x007ec280,WakeAddQuad,"rx_WaterWake",0){rx_wake_add_quad(m.r[7],m.r[1],m.r[0],m.arg(0),m.arg(1),m.arg(2),m.arg(3),rx_bf(m.arg(4)));return m.ret(20);}
RX_PORT(0x007ecd80,WakeAddSplat,"rx_WaterWake",0){rx_wake_add_splat(m.r[6],m.arg(0),m.arg(1),rx_bf(uint32_t(m.xmm[0].lo)));return m.ret(8);}

RX_PORT(0x007efed0,WakeAddGuardedSplat,"rx_WaterWake",0){rx_wake_add_guarded_splat(m.r[0],m.r[1],m.arg(0),rx_bf(uint32_t(m.xmm[0].lo)));return m.ret(4);}
