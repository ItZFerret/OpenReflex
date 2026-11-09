#include "oracle.h"
extern "C" {
#include "../game/vehicle_utv_poststep.h"
}
using namespace reflex_port;
namespace {
void utv_poststep_outputs(reflex_vm::Machine& m,uint32_t s){
 uint32_t count=rx_rd32(s+0x1318);
 m.r[0]=int32_t(count)>0?s+0x1c30+0x20u*count:s+0x8470;
 m.r[2]=int32_t(count)>0?0:s;
 uint32_t p=s+0x8840;
 m.xmm[0].lo=uint64_t(rx_rd32(p))|(uint64_t(rx_rd32(p+4))<<32);
 m.xmm[0].hi=uint64_t(rx_rd32(p+8))|(uint64_t(rx_rd32(p+12))<<32);
}
}
RX_PORT(0x004c6e80,UtvVehiclePoststep,"rx_UtvPoststep",OUT_EAX|OUT_EDX|OUT_XMM0){
 uint32_t v=m.r[1];int32_t counter=int32_t(rx_rd32(0x00ad3ed4)),phase=counter%3;
 rx_utv_vehicle_poststep(v);
 if(phase==1||phase==2)utv_poststep_outputs(m,rx_rd32(v+0x1540));
 else{m.r[0]=uint32_t(phase);m.r[2]=uint32_t(counter)-uint32_t(phase);}
 return m.ret(0);
}
RX_PORT(0x004d8160,UtvSimulationPoststep,"rx_UtvPoststep",OUT_EAX|OUT_EDX|OUT_XMM0){uint32_t s=m.r[6];rx_utv_simulation_poststep(s);utv_poststep_outputs(m,s);return m.ret(0);}
