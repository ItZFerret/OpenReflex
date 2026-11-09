#include "oracle.h"
#include "../game/vehicle_fourwheel_probes.h"
using namespace reflex_port;
RX_PORT(0x004da510,UtvUpdateProbes,"rx_FourwheelProbes",0){rx_sim_utv_update_probes(m.r[1]);return m.ret(0);}
RX_PORT(0x004bf400,BuggyUpdateProbes,"rx_FourwheelProbes",0){rx_sim_buggy_update_probes(m.r[1]);return m.ret(0);}
RX_PORT(0x0045b8f0,TruckUpdateProbes,"rx_FourwheelProbes",0){rx_sim_truck_update_probes(m.r[1]);return m.ret(0);}
