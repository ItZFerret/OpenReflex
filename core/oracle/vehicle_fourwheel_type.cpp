#include "oracle.h"
extern "C" {
#include "../game/vehicle_fourwheel_type.h"
}
using namespace reflex_port;
#define G "rx_FourwheelType"
/* Original leaves EAX=simulation and ECX=type, preserves EDX and x87. */
RX_PORT(0x004a2d00,BuggySetSimulationType,G,OUT_EAX|OUT_ECX|OUT_EDX|OUT_FPU){uint32_t v=m.r[1],type=m.arg(0);rx_buggy_set_simulation_type(v,type);m.r[0]=rx_rd32(v+0x15c0);m.r[1]=type;return m.ret(4);}
RX_PORT(0x004c0a00,UtvSetSimulationType,G,OUT_EAX|OUT_ECX|OUT_EDX|OUT_FPU){uint32_t v=m.r[1],type=m.arg(0);rx_utv_set_simulation_type(v,type);m.r[0]=rx_rd32(v+0x1540);m.r[1]=type;return m.ret(4);}
RX_PORT(0x0046acf0,TruckSetSimulationType,G,OUT_EAX|OUT_ECX|OUT_EDX|OUT_FPU){uint32_t v=m.r[1],type=m.arg(0);rx_truck_set_simulation_type(v,type);m.r[0]=rx_rd32(v+0x15d0);m.r[1]=type;return m.ret(4);}
#undef G
