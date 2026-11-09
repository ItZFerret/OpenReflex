// Development-only Verify stimulus for0084f700. Select
// REFLEX_PORT=rx_phys,rx_BuoyancyProbeHook REFLEX_PORT_VERIFY=1 RX_BUOYANCY_PROBE=1.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
using rx_oracle::bits;
namespace {
uint32_t frame=0,rng=0xbb67ae85;
uint32_t random32(){rng=rng*1664525u+1013904223u;return rng;}
float value(){return (float(random32()>>8)/16777216.0f)*2.0f-1.0f;}
void word(uint32_t a,uint32_t x){rx_wr32(a,x);}
void number(uint32_t a,float x){word(a,bits(x));}
rx_regs regs(){rx_regs r;std::memset(&r,0,sizeof r);return r;}
uint32_t alloc(uint32_t n){return rx_call_original(0x0078fbe0,RX_CDECL,0,1,&n);}
void probes(){
 const uint32_t state=alloc(0x50),body=alloc(0xe0);
 uint32_t saved[3];for(int i=0;i<3;i++)saved[i]=rx_rd32(0x009df494+4*i);
 number(0x009df494,2.5f);number(0x009df498,0.2f);number(0x009df49c,0.95f);
 for(int t=0;t<160;t++){
  for(uint32_t i=0;i<0x50;i+=4)word(state+i,0xa5a5a5a5);
  for(uint32_t i=0;i<0xe0;i+=4)word(body+i,0x5a5a5a5a);
  for(int i=0;i<4;i++){
   number(state+0x10+4*i,value());number(state+0x20+4*i,value());
   number(body+0x50+4*i,value());number(body+0x70+4*i,value());
  }
  if(t%8==0)for(int i=0;i<4;i++){number(body+0x50+4*i,i==3?1.0f:0.0f);number(body+0x70+4*i,i==3?1.0f:0.0f);}
  if(t%8==1)for(int i=0;i<4;i++){number(body+0x50+4*i,0.0f);number(body+0x70+4*i,0.0f);}
  if(t%8==2){number(body+0x50,0);number(body+0x54,0);number(body+0x58,0.70710677f);number(body+0x5c,0.70710677f);number(body+0x70,0);number(body+0x74,0);number(body+0x78,0);number(body+0x7c,1);}
  if(t%16==3)word(body+0x50,0x7fa12345); //signalingNaN through SSE and doublefit
  if(t%16==4)word(body+0x70,0x7f800000); //infinite normalization guard
  if(t%16==5)word(body+0x54,0x80000000); //signedzero
  const uint32_t arg=body;rx_regs in=regs(),out=regs();in.r[6]=state;
  rx_call_original_ex(0x0084f700,&in,&out,1,&arg);
 }
 for(int i=0;i<3;i++)word(0x009df494+4*i,saved[i]);
 std::fprintf(stderr,"[buoyancyprobe] 160 original calls (finite,identity,zero,tilted,SNaN,infinity,signedzero)\n");
}
}
RX_PORT(0x0084ed20,BuoyancyProbeHook,"thunk",0){
 static const char* enabled=std::getenv("RX_BUOYANCY_PROBE");if(!enabled||!*enabled)return reflex_vm::RUN_ORIGINAL;
 static const uint32_t at=std::getenv("RX_BUOYANCY_PROBE_FRAME")?uint32_t(std::atoi(std::getenv("RX_BUOYANCY_PROBE_FRAME"))):5;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t saved[8];reflex_vm::Xmm xmm[8];std::memcpy(saved,m.r,sizeof saved);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;const size_t fpu=m.fpu.size();
 probes();std::memcpy(m.r,saved,sizeof saved);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu.resize(fpu);return reflex_vm::RUN_ORIGINAL;
}
