#include "oracle.h"
#include "../game/vehicle_fourwheel_methods.h"
using namespace reflex_port;
#define G "rx_vehicle_fourwheel"
RX_PORT(0x004cc610,UtvTuning,G,0){rx_sim_utv_apply_tuning(m.r[1]);return m.ret(0);}
RX_PORT(0x004b1e10,BuggyTuning,G,0){rx_sim_buggy_apply_tuning(m.r[1]);return m.ret(0);}
RX_PORT(0x00450ab0,TruckTuning,G,0){rx_sim_truck_apply_tuning(m.r[1]);return m.ret(0);}
RX_PORT(0x004d75b0,UtvReset,G,0){rx_sim_utv_reset(m.r[1],m.arg(0),m.arg(1));return m.ret(8);}
RX_PORT(0x004bce80,BuggyReset,G,0){rx_sim_buggy_reset(m.r[1],m.arg(0),m.arg(1));return m.ret(8);}
RX_PORT(0x00459400,TruckReset,G,0){rx_sim_truck_reset(m.r[1],m.arg(0),m.arg(1));return m.ret(8);}

RX_PORT(0x004d4750,UtvGeometry,G,0){rx_sim_utv_geometry(m.r[1]);return m.ret(0);}

RX_PORT(0x004ba270,BuggyGeometry,G,0){rx_sim_buggy_geometry(m.r[1]);return m.ret(0);}
RX_PORT(0x004564d0,TruckGeometry,G,0){rx_sim_truck_geometry(m.r[1]);return m.ret(0);}
