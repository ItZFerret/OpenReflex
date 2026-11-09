#include "oracle.h"
extern "C" uint32_t rx_simulation_get_mode_flag(uint32_t);
using namespace reflex_port;
RX_PORT(0x00432190,Simulation_ModeFlag,"rx_AtvPhysics",OUT_EAX){m.r[0]=rx_simulation_get_mode_flag(m.r[1]);return m.ret(0);}
