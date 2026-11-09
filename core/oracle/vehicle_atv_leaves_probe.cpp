/* Development-only leaf method coverage on a real ATV. Full simulation state
 * is restored after each input/return-value edge case. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace { unsigned leaves_frame; uint32_t rd(uint32_t a){return rx_rd32(a);} void wr(uint32_t a,uint32_t b){rx_wr32(a,b);} }
RX_PORT(0x0084ed20,AtvLeavesProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_LEAVES_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const unsigned target=std::getenv("RX_ATV_LEAVES_FRAME")?unsigned(std::atoi(std::getenv("RX_ATV_LEAVES_FRAME"))):650;
 if(++leaves_frame!=target)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,v=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&rd(a)==0x0093d054){s=a;break;}}
 if(!s){std::fprintf(stderr,"[atv-leaves] ERROR no live ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto fpu=m.fpu;uint32_t flags=m.flags;
 std::vector<uint32_t> state(0x9bd0/4);for(unsigned i=0;i<state.size();i++)state[i]=rd(s+4*i);
 auto restore=[&](){for(unsigned i=0;i<state.size();i++)wr(s+4*i,state[i]);};
 auto call=[&](uint32_t fn,uint32_t obj,std::vector<uint32_t> a){rx_regs in{},out{};in.r[1]=obj;rx_call_original_ex(fn,&in,&out,int(a.size()),a.data());};
 for(uint32_t mode:{0u,1u,0xffffffffu}){call(0x47cbb0,v,{mode});restore();}
 call(0x47c440,s,{});restore();call(0x486600,v,{s+0x2df0});restore();
 for(uint32_t bits:{0u,0x80000000u,0x45bb8000u,0x7fa12345u}){wr(s+0x4bfc,bits);call(0x486640,v,{});restore();}
 for(uint32_t mode:{0u,1u,2u,3u}){wr(s+0x597c,mode);call(0x486790,v,{});restore();}
 for(uint32_t axle:{0u,1u,2u,0xffffffffu}){call(0x4867b0,v,{axle});restore();}
 for(uint32_t tag:{rd(v+0x1750),rd(v+0x1754),rd(v+0x1758),rd(v+0x175c),0xffffffffu}){call(0x487af0,v,{tag});call(0x4744c0,v,{tag});restore();}
 for(unsigned k=0;k<14;k++){
  for(unsigned i=0;i<6;i++){wr(s+0x1af0+4*i,0);wr(s+0x1b54+4*i,0);}
  if(k<12)wr(s+(k<6?0x1af0:0x1b54)+4*(k%6),1u<<(k%4));
  if(k==13){wr(s+0x2ec0,0x3f800000);for(unsigned i=0;i<4;i++)wr(s+0x100+4*i,0);wr(s+0x94e0,0);wr(s+0x9630,0);}
  call(0x48d2a0,s,{});call(0x48d160,s,{});restore();
 }
 call(0x46a2c0,v,{});restore();
 const uint32_t pending=rd(0x9e3100),retry=rd(0x9cc8b0);
 for(uint32_t initial:{0u,1u,0xffffffffu}){wr(0x9e3100,0);wr(0x9cc8b0,initial);call(0x48d320,s,{});restore();}
 // A blocked pending reload must retain both the stream and zero retry flag.
 wr(0x9e3100,s);wr(0x9cc8b0,0);call(0x48d320,s,{});restore();
 wr(0x9e3100,pending);wr(0x9cc8b0,retry);
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.fpu=fpu;m.flags=flags;
 std::fprintf(stderr,"[atv-leaves] complete: 12 methods, mode/axle/tag/contact and float edge cases; actual S restored\n");return reflex_vm::RUN_ORIGINAL;
}
