/* Read-only virtual getter coverage on actual Buggy/Truck objects. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
RX_PORT(0x0084ed20,FourwheelLeavesProbe,"thunk",0){
 static unsigned frame;
 if(!std::getenv("RX_FOURWHEEL_LEAVES_PROBE")||++frame!=20)return reflex_vm::RUN_ORIGINAL;
 uint32_t vehicle=0,sim=0,game=rx_rd32(0xa946b8),kind=0;
 if(game)for(unsigned i=0;i<16;i++){
  uint32_t v=rx_rd32(game+0x7460+0x12a0*i+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;
  uint32_t s=rx_rd32(v+0x234);if(!s||!m.mem.mapped(s))continue;
  uint32_t vt=rx_rd32(s);if(vt==0x93d24c||vt==0x93c93c){vehicle=v;sim=s;kind=vt;break;}
 }
 if(!vehicle){std::fprintf(stderr,"[fourwheel-leaves] ERROR no actual Buggy/Truck object\n");return reflex_vm::RUN_ORIGINAL;}
 const uint32_t bf[]={0x4ac560,0x4ac580,0x4ac5a0,0x4ac5c0,0x4ac5e0,0x4ac600,0x4ac620,0x4ac640,0x4ac650,0x4ac660,0x4ac670,0x4ac680,0x4ac6e0};
 const uint32_t tf[]={0x473490,0x4734b0,0x4734d0,0x4734f0,0x473510,0x473530,0x473550,0x473570,0x473580,0x473590,0x4735a0,0x4735b0,0x473610};
 const uint32_t *functions=kind==0x93d24c?bf:tf;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 uint32_t output=(m.r[4]-0x200)&~15u,saved[4];for(unsigned i=0;i<4;i++)saved[i]=rx_rd32(output+4*i);
 for(unsigned i=0;i<13;i++){
  rx_regs in{},out{};in.r[1]=vehicle;uint32_t args[]={output};
  rx_call_original_ex(functions[i],&in,&out,i<6?1:0,args);
 }
 for(unsigned i=0;i<4;i++)rx_wr32(output+4*i,saved[i]);
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 std::fprintf(stderr,"[fourwheel-leaves] complete13 getters V=%08x S=%08x\n",vehicle,sim);
 return reflex_vm::RUN_ORIGINAL;
}
