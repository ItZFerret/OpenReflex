/* Development stimulus on an actual original ATV world. No fixture product calls.
 * REFLEX_PORT=rx_AtvRuntime,rx_AtvRuntimeProbeHook REFLEX_PORT_VERIFY=1 RX_ATV_RUNTIME_PROBE=1 */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace{
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
struct Region{uint32_t a;std::vector<uint32_t> data;Region(uint32_t p,uint32_t bytes):a(p){for(uint32_t i=0;i<bytes;i+=4)data.push_back(rd(p+i));}void restore(){for(size_t i=0;i<data.size();i++)wr(a+4*uint32_t(i),data[i]);}~Region(){restore();}};
}
RX_PORT(0x0084ed20,AtvRuntimeProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_RUNTIME_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_ATV_RUNTIME_FRAME")?uint32_t(std::atoi(std::getenv("RX_ATV_RUNTIME_FRAME"))):650;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t a=rd(p+0x1ac);if(a&&m.mem.mapped(a)&&rd(a)==0x93d0ec){v=a;break;}}
 if(!v){std::fprintf(stderr,"[atv-runtime] ERROR no live original ATV vehicle\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  uint32_t s=rd(v+0x1700);Region vehicle(v,6000),sim(s,0x9bd0),clock(0xad3ed4,4);
  auto restore=[&](){vehicle.restore();sim.restore();clock.restore();};
  auto invoke=[&](uint32_t fn,std::vector<uint32_t> args){rx_regs in{},out{};in.r[1]=v;rx_call_original_ex(fn,&in,&out,int(args.size()),args.data());return out.r[0];};
  const uint32_t get[]={0x486560,0x486580,0x4865a0,0x4865c0,0x4865e0};
  const uint32_t offset[]={0x180,0x7c0,0x100,0x740,0x160};
  for(unsigned i=0;i<5;i++){
   invoke(get[i],{v+0x1660});restore();
   const uint32_t bits[]={0x80000000,0x7fa12345,0x7f800000,0xdeadbeef};
   for(unsigned k=0;k<4;k++)wr(s+offset[i]+4*k,bits[k]);
   invoke(get[i],{v+0x1660});restore();
  }
  unsigned accepted=0;
  for(unsigned branch=0;branch<3;branch++){
   wr(v+0x280,0);wr(s+0xc,0);wr(s+0x1318,0);
   for(uint32_t a:{0x9240u,0x9390u,0x94e0u,0x9630u})wr(s+a,1);
   for(uint32_t a:{0x1630u,0x1634u,0x1628u,0x162cu})wr(v+a,0);
   if(branch==0)wr(v+0x280,1);
   accepted+=invoke(0x486100,{v+0x1660,v+0x1670,branch==1?0x7f800000u:0xff800000u,0})!=0;restore();
  }
  for(uint32_t phase:{0u,1u,2u,0xffffffffu}){wr(0xad3ed4,phase);wr(s+0x90e0,0x12345678);invoke(0x486520,{});restore();}
  std::fprintf(stderr,"[atv-runtime] complete 10 getter cases,3 safe-pose cases(%u accepted),4 schedule cases; actual state restored\n",accepted);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
