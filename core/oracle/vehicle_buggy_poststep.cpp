#include "oracle.h"
extern "C" {
#include "../game/vehicle_buggy_poststep.h"
}
using namespace reflex_port;
namespace {
void buggy_poststep_outputs(reflex_vm::Machine& m,uint32_t s){
    /* 004be1da loads probe count. The debug loop advances EAX by20 and
     * decrements EDX to zero; with count<=0 the last wheel helper leaves
     * EAX=its wheel record and EDX=simulation. Tail MOVAPS loads sim+8830. */
    uint32_t count=rx_rd32(s+0x1318);
    m.r[0]=int32_t(count)>0?s+0x1c30+0x20u*count:s+0x8460;
    m.r[2]=int32_t(count)>0?0:s;
    uint32_t p=s+0x8830;
    m.xmm[0].lo=uint64_t(rx_rd32(p))|(uint64_t(rx_rd32(p+4))<<32);
    m.xmm[0].hi=uint64_t(rx_rd32(p+8))|(uint64_t(rx_rd32(p+12))<<32);
}
}
RX_PORT(0x004ac520,BuggyPoststep,"rx_BuggyPoststep",OUT_EAX|OUT_EDX|OUT_XMM0|OUT_FPU){
    uint32_t v=m.r[1];int32_t counter=int32_t(rx_rd32(0x00ad3ed4)),phase=counter%3;
    rx_buggy_poststep(v);
    if(phase==1||phase==2)buggy_poststep_outputs(m,rx_rd32(v+0x15c0));
    else{m.r[0]=uint32_t(phase);m.r[2]=uint32_t(counter)-uint32_t(phase);}
    return m.ret(0);
}
RX_PORT(0x004bd860,BuggySimulationPoststep,"rx_BuggyPoststep",OUT_EAX|OUT_EDX|OUT_XMM0|OUT_FPU){
    uint32_t s=m.r[6];rx_buggy_simulation_poststep(s);buggy_poststep_outputs(m,s);return m.ret(0);
}
