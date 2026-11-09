#include "oracle.h"
extern "C" void rx_atv_post_step(uint32_t s);
using namespace reflex_port;
RX_PORT(0x0049d7c0,Atv_PostStep,"rx_AtvPhysics",0){rx_atv_post_step(m.r[2]);return m.ret(0);}
