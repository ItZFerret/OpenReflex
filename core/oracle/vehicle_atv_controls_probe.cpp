/* Development-only controls stimulus on an actual original ATV. Full S is
 * restored between cases; all six bodies/curves and genuine pointers retained. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {unsigned frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}}
RX_PORT(0x0084ed20,AtvControlsProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_CONTROLS_PROBE"))return reflex_vm::RUN_ORIGINAL;
 unsigned at=std::getenv("RX_ATV_CONTROLS_FRAME")?unsigned(std::atoi(std::getenv("RX_ATV_CONTROLS_FRAME"))):650;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&rd(a)==0x0093d054){s=a;break;}}
 if(!s){std::fprintf(stderr,"[atv-controls] ERROR no live ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto fpu=m.fpu;uint32_t flags=m.flags;
 std::vector<uint32_t> state(0x9bd0/4);for(unsigned i=0;i<state.size();i++)state[i]=rd(s+4*i);
 auto restore=[&](){for(unsigned i=0;i<state.size();i++)wr(s+4*i,state[i]);};
 auto invoke=[&](uint32_t fn){rx_regs in{},out{};if(fn==0x00494910){uint32_t args[]={s,0x3c888889};rx_call_original_ex(fn,&in,&out,2,args);}else{in.r[fn==0x00496880?6:1]=s;uint32_t arg=0x3c888889;rx_call_original_ex(fn,&in,&out,1,&arg);}};
 wr(s+0x2374,1);invoke(0x00497a70);restore();
 wr(s+0x2374,0);wr(s+0x99a8,0);invoke(0x00497a70);restore();
 // Raw helper coverage includes grounded contact alternatives and both tipping directions.
 const uint32_t contacts[]={0x9240,0x9390,0x94e0,0x9630,0x9204,0x9354,0x94a4,0x95f4};
 for(unsigned pattern=0;pattern<5;pattern++){
  for(auto o:contacts)wr(s+o,0);
  if(pattern==1){wr(s+0x94e0,1);wr(s+0x740,0xbf800000);}
  if(pattern==2){wr(s+0x9240,1);wr(s+0x740,0x3f800000);}
  if(pattern==3){wr(s+0x9240,1);wr(s+0x94e0,1);wr(s+0x748,0x3f800000);}
  if(pattern==4){wr(s+0x9390,1);wr(s+0x9630,1);wr(s+0x748,0xbf800000);}
  invoke(0x00496880);restore();
 }
 for(unsigned pattern=0;pattern<7;pattern++){
  wr(s+0x2374,0);wr(s+0x99a8,1);wr(s+0x3c,0);wr(s+0x9958,0);wr(s+0x9970,0);
  wr(s+0x9954,0);wr(s+0x996c,0);wr(s+0x97a4,0x3f800000);
  for(unsigned i=0;i<4;i++)wr(s+0x5960+4*i,pattern==1?0x3f000000:0);
  if(pattern==2){wr(s+0x9958,1);wr(s+0x9960,600);wr(s+0x995c,0x3f000000);}
  if(pattern==3||pattern==5||pattern==6){wr(s+0x9970,1);wr(s+0x997c,pattern==3?500:pattern==5?375:300);wr(s+0x9974,0x3f000000);wr(s+0x9978,0x3f000000);}
  if(pattern==4){wr(s+0x9870,1);wr(s+0x5960,0xbf800000);}
  invoke(0x00494910);restore();
 }
 wr(s+0x2374,0);wr(s+0x99a8,1);invoke(0x00497a70);restore();
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.fpu=fpu;m.flags=flags;
 std::fprintf(stderr,"[atv-controls] complete: dispatcher3 ground5 air7; actual six-body S restored\n");
 return reflex_vm::RUN_ORIGINAL;
}
