#include "oracle.h"
extern "C" {
#include "../game/node_poststep_debug.h"
}
using namespace reflex_port;
#define RESULT(value,n) do{auto result=(value);m.r[2]=result.edx;return m.ret((n),result.eax);}while(0)
RX_PORT(0x00859b30,NodeDebugAxes,"rx_NodePoststepDebug",OUT_EAX|OUT_EDX|OUT_FPU){RESULT(rx_node_debug_axes_regs(m.arg(0)),4);}
RX_PORT(0x00859e40,MainBodyNodePoststep,"rx_NodePoststepDebug",OUT_EAX|OUT_EDX|OUT_FPU){RESULT(rx_node_mainbody_poststep_regs(m.r[1],m.r[0],m.r[2]),0);}
RX_PORT(0x0085b180,SlidingRotorPoststep,"rx_NodePoststepDebug",OUT_EAX|OUT_EDX|OUT_FPU){RESULT(rx_node_rotor_poststep_regs(m.r[1],m.r[0],m.r[2]),0);}
RX_PORT(0x0085bed0,FixedBodyNodePoststep,"rx_NodePoststepDebug",OUT_EAX|OUT_EDX|OUT_FPU){RESULT(rx_node_fixed_poststep_regs(m.r[1],m.r[0],m.r[2]),0);}
#undef RESULT
