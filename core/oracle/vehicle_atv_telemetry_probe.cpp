/* Real ATV telemetry output/axle-grip coverage; all vehicle/simulation state and
 * the borrowed dead-stack output record are restored between cases. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <utility>
using namespace reflex_port;
namespace {unsigned telemetry_frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}}
RX_PORT(0x0084ed20,AtvTelemetryProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_TELEMETRY_PROBE"))return reflex_vm::RUN_ORIGINAL;
 unsigned at=std::getenv("RX_ATV_TELEMETRY_FRAME")?unsigned(std::atoi(std::getenv("RX_ATV_TELEMETRY_FRAME"))):650;
 if(++telemetry_frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t a=rd(p+0x1ac);if(!a||!m.mem.mapped(a+0x234))continue;uint32_t b=rd(a+0x234);if(b&&m.mem.mapped(b)&&rd(b)==0x0093d054){v=a;s=b;break;}}
 if(!s){std::fprintf(stderr,"[atv-telemetry] ERROR no live ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t outp=(m.r[4]-0x2000)&~15u;
 if(!m.mem.mapped(outp)||!m.mem.mapped(outp+0x270)){std::fprintf(stderr,"[atv-telemetry] ERROR no scratch output range\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto fpu=m.fpu;uint32_t flags=m.flags;
 struct Range{uint32_t p;std::vector<uint32_t> words;};std::vector<Range> saved;
 for(auto pair:{std::pair<uint32_t,uint32_t>{s,0x9bd0},{v,6000},{outp,0x270}}){Range r{pair.first,{}};for(unsigned i=0;i<pair.second;i+=4)r.words.push_back(rd(pair.first+i));saved.push_back(r);}
 auto restore=[&](){for(auto& r:saved)for(unsigned i=0;i<r.words.size();i++)wr(r.p+4*i,r.words[i]);};
 auto invoke=[&](uint32_t fn,unsigned nargs,const uint32_t* args){rx_regs in{},out{};in.r[1]=v;rx_call_original_ex(fn,&in,&out,int(nargs),args);};
 invoke(0x00486620,0,nullptr);restore();
 const uint32_t contacts[]={0x9240,0x9390,0x94e0,0x9630};
 for(unsigned axle=0;axle<3;axle++)for(unsigned mask=0;mask<(axle==2?1u:4u);mask++){
  for(unsigned i=0;i<4;i++)wr(s+contacts[i],0);
  if(axle<2){wr(s+contacts[2*axle],mask&1);wr(s+contacts[2*axle+1],(mask>>1)&1);}
  invoke(0x00486850,1,&axle);restore();
 }
 // Distinct canary initialization exposes stores into holes as well as missed
 // stores; a dry-object alternative avoids fabricating a water object pointer.
 for(unsigned mode=0;mode<2;mode++){
  for(unsigned i=0;i<0x270;i+=4)wr(outp+i,0xdead0000u+i);
  if(mode)for(unsigned i=0;i<4;i++)wr(s+0x90e8+4*i,0);
  invoke(0x00486cc0,1,&outp);restore();
  for(unsigned i=0;i<0x270;i+=4)wr(outp+i,0xbeef0000u+i);
  if(mode)for(unsigned i=0;i<4;i++)wr(s+0x90e8+4*i,0);
  invoke(0x00487510,1,&outp);restore();
  if(mode){const uint32_t bits[]={0x80000000u,0x3f123456u,0x7fc12345u,0xbf654321u};for(unsigned i=0;i<4;i++)wr(s+0x740+4*i,bits[i]);}
  invoke(0x00430250,1,&outp);restore();
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.fpu=fpu;m.flags=flags;
 std::fprintf(stderr,"[atv-telemetry] complete: rpm1 grip9 snapshot-vectors2 snapshot-values2 angular-forward2; real V/S and scratch restored\n");
 return reflex_vm::RUN_ORIGINAL;
}
