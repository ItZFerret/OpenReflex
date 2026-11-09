#include "oracle.h"
extern "C" {
#include "../game/vehicle_truck_poststep.h"
}
using namespace reflex_port;
// Own code uses only SSE; leave the incoming x87 stack untouched and compare it.
RX_PORT(0x00459fa0,TruckSimulationPoststep,"rx_TruckPoststep",OUT_EAX|OUT_EDX|OUT_FPU){uint32_t edx;uint32_t eax=rx_sim_truck_poststep_regs(m.r[1],&edx);m.r[2]=edx;return m.ret(0,eax);}
RX_PORT(0x00473450,TruckPoststep,"rx_TruckPoststep",OUT_EAX|OUT_EDX|OUT_FPU){uint32_t edx;uint32_t eax=rx_truck_poststep_regs(m.r[1],&edx);m.r[2]=edx;return m.ret(0,eax);}
