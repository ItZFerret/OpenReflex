#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/vehicle_buggy_pose.h"
}
using namespace reflex_port;
RX_PORT(0x004a4270,Buggy_UpdateModelPose,"rx_BuggyPose",0){
    static bool diagnosed=false;
    if(!diagnosed&&std::getenv("RX_BUGGY_POSE_DIAG")){
        diagnosed=true;uint32_t v=m.r[1],sim=rx_rd32(v+0x15c0);
        for(uint32_t off=0x1360;off<=0x13d4;off+=4){uint32_t node=rx_rd32(v+off),set=rx_rd32(node),index=uint32_t(m.mem.read(node+0xb,1));
            std::fprintf(stderr,"[buggy-node] +%04x node=%08x local=%08x world=%08x\n",off,node,rx_rd32(set+0x18)+64*index,rx_rd32(set+0x1c)+64*index);}
        for(uint32_t off:{0x180u,0x220u,0x2c0u,0x360u,0x400u,0x7c0u,0x36e0u,0x3f90u,0x41f0u,0x5620u})
            std::fprintf(stderr,"[buggy-input] +%04x %08x %08x %08x %08x\n",off,rx_rd32(sim+off),rx_rd32(sim+off+4),rx_rd32(sim+off+8),rx_rd32(sim+off+12));
    }
    rx_buggy_update_model_pose(m.r[1],rx_bf(m.arg(0)));return m.ret(4);
}
