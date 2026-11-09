#include "phys_oracle.h"
extern "C" {
#include "../game/phys_buoyancy.h"
}
using namespace reflex_port;
RX_PORT(0x0084f700,BuoyancyAngularCorrection,"rx_phys",0){phys_buoyancy_angular_correction(m.r[6],m.arg(0));return m.ret(4);}
