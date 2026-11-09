#include "oracle.h"
#include <cstdio>
#include <cstdlib>
extern "C" {
#include "../game/vehicle_truck_pose.h"
}
using namespace reflex_port;
RX_PORT(0x0046c090,Truck_UpdateModelPose,"rx_TruckPose",0){
 static unsigned calls=0;
 if(++calls==1 && std::getenv("RX_TRUCK_POSE_DIAG")){
  uint32_t v=m.r[1],sim=rx_rd32(v+0x15d0);
  std::fprintf(stderr,"[truck-pose] V=%08x S=%08x dt=%08x\n",v,sim,m.arg(0));
  for(uint32_t off=0x1360;off<=0x13ec;off+=4){uint32_t node=rx_rd32(v+off),model=rx_rd32(node),index=uint32_t(m.mem.read(node+0xb,1));std::fprintf(stderr,"[truck-pose] node +%04x=%08x index=%u local=%08x world=%08x\n",off,node,index,rx_rd32(model+0x18)+64*index,rx_rd32(model+0x1c)+64*index);}
  for(uint32_t off:{0x220u,0x2c0u,0x360u,0x400u,0x860u,0x900u,0x9a0u,0xa40u,0x36e0u,0x3f90u,0x41f0u})std::fprintf(stderr,"[truck-pose] input +%04x %08x %08x %08x %08x\n",off,rx_rd32(sim+off),rx_rd32(sim+off+4),rx_rd32(sim+off+8),rx_rd32(sim+off+12));
 }
 rx_truck_update_model_pose_at(m.r[1],rx_bf(m.arg(0)),m.r[3]);return m.ret(4);
}

RX_PORT(0x00472d50,Truck_PublishRider,"rx_TruckRuntime",0){rx_truck_publish_rider(m.r[1],rx_bf(m.arg(0)));return m.ret(4);}
