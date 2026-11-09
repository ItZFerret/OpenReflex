#include "oracle.h"
#include "../game/vehicle_atv_lifecycle.h"
using namespace reflex_port;
#define G "rx_AtvLifecycle"
RX_PORT(0x0045d3c0,VehicleBaseDestruct,G,0){rx_vehicle_base_destruct(m.r[1]);return m.ret(0);}
RX_PORT(0x0047c780,AtvVehicleDelete,G,OUT_EAX){return m.ret(4,rx_atv_vehicle_delete(m.r[1],m.arg(0)));}
RX_PORT(0x00460230,VehicleBaseDetach,G,0){rx_vehicle_base_detach(m.r[1]);return m.ret(0);}
RX_PORT(0x0047cbc0,AtvVehicleDetach,G,0){rx_atv_vehicle_detach(m.r[1]);return m.ret(0);}
RX_PORT(0x0047d670,AtvListenerPreStep,G,0){rx_atv_listener_pre_step(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
