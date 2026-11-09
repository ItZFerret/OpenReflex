#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/vehicle_atv_pose.h"
}
using namespace reflex_port;
RX_PORT(0x0047cfa0,Atv_PlaceComposite,"rx_AtvPose",0){rx_atv_place_composite(m.r[1]);return m.ret(0);}
RX_PORT(0x0047e850,Atv_UpdateModelPose,"rx_AtvPose",0){
 static unsigned calls=0;
 if(++calls==1 && std::getenv("RX_ATV_POSE_DIAG")){
  uint32_t v=m.r[1],sim=rx_rd32(v+0x1700);
  std::fprintf(stderr,"[atv-pose] V=%08x S=%08x\n",v,sim);
  for(uint32_t off=0x1528;off<=0x1580;off+=4){
   uint32_t node=rx_rd32(v+off),model=rx_rd32(node),index=uint32_t(m.mem.read(node+0xb,1));
   std::fprintf(stderr,"[atv-pose] node +%04x=%08x index=%u local=%08x world=%08x\n",off,node,index,rx_rd32(model+0x18)+64*index,rx_rd32(model+0x1c)+64*index);
  }
  const uint32_t inputs[]={0x180,0x2c0,0x360,0x400,0x4a0,0x730,0x7c0,0x900,0x9a0,0xae0,0x3640,0x58a0,0x58b0};
  for(uint32_t off:inputs)std::fprintf(stderr,"[atv-pose] input +%04x %08x %08x %08x %08x\n",off,rx_rd32(sim+off),rx_rd32(sim+off+4),rx_rd32(sim+off+8),rx_rd32(sim+off+12));
 }
 rx_atv_update_model_pose(m.r[1]);return m.ret(4);
}
