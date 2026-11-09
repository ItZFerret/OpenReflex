#include "oracle.h"
#include "../game/vehicle_atv_destroy.h"
using namespace reflex_port;
#define G "rx_AtvDestroy"
RX_PORT(0x0042f9f0,VehicleWheelBlockDestruct,G,0){rx_vehicle_wheel_block_destruct(m.r[0]);return m.ret(0);}
RX_PORT(0x0042fd50,VehicleChassisBlockDestruct,G,0){rx_vehicle_chassis_block_destruct(m.r[0]);return m.ret(0);}
RX_PORT(0x0085c390,VehicleMechanismDestruct,G,0){rx_vehicle_mechanism_destruct(m.r[0]);return m.ret(0);}
RX_PORT(0x0081f6e0,VehicleSimVectorDestruct,G,0){rx_vehicle_sim_vector_destruct(m.r[0]);return m.ret(0);}
RX_PORT(0x0048cfc0,AtvSimDestruct,G,0){rx_atv_sim_destruct(m.r[1]);return m.ret(0);}
RX_PORT(0x0048c730,AtvSimDelete,G,OUT_EAX){return m.ret(4,rx_atv_sim_delete(m.r[1],m.arg(0)));}
