// Development-only overlay copies and blend branches; no product linkage.
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
RX_PORT(0x0084ed20,OverlayProbeHook,"thunk",0) {
 static unsigned frame=0;if(!std::getenv("RX_OVERLAY_PROBE")||++frame!=5)return reflex_vm::RUN_ORIGINAL;
 uint32_t saved[8];reflex_vm::Xmm xmm[8];std::memcpy(saved,m.r,sizeof saved);std::memcpy(xmm,m.xmm,sizeof xmm);const auto fpu=m.fpu;auto flags=m.flags;
 const uint32_t size=0x1000,mem=rx_call_original(0x0078fbe0,RX_CDECL,0,1,&size),scene=mem,e=mem+0x800;
 auto reset=[&](){for(unsigned i=0;i<size;i+=4)rx_wr32(mem+i,0);for(unsigned i=0;i<12;i++)rx_wr32(e+0x90+4*i,rx_oracle::bits(float(i)/16));};
 for(unsigned test=0;test<3;test++) {
  reset();if(test==1){rx_wr32(e+0x90,0x7fa12345);rx_wr32(e+0xa0,0xff812345);rx_wr32(e+0xb0,0x80000000);}
  rx_regs in{},out{};in.r[6]=test==2?scene+0x69c:e+0x90;in.r[2]=e+0xa0;in.r[1]=e+0xb0;rx_call_original_ex(0x0040b1d0,&in,&out,1,&scene);
 }
 const uint32_t ctx=rx_rd32(0xa95844),oldscene=rx_rd32(ctx+0x14);rx_wr32(ctx+0x14,scene);
 for(unsigned test=0;test<3;test++) {
  reset();rx_wr32(e+0x10c,test!=0);rx_wr32(e+0x110,rx_oracle::bits(test==1?.25f:-.25f));
  for(unsigned i=0;i<14;i++){rx_wr32(e+0x120+4*i,rx_oracle::bits(float(i)/32));rx_wr32(e+0x160+4*i,rx_oracle::bits(float(i)/64));}
  rx_regs in{},out{};in.r[1]=e;const uint32_t dt=rx_oracle::bits(1.f/60);rx_call_original_ex(0x0040b770,&in,&out,1,&dt);
 }
 rx_wr32(ctx+0x14,oldscene);rx_call_original(0x0078fc50,RX_CDECL,0,1,&mem);
 std::memcpy(m.r,saved,sizeof saved);std::memcpy(m.xmm,xmm,sizeof xmm);m.fpu=fpu;m.flags=flags;
 std::fprintf(stderr,"[overlayprobe] complete:3 copy cases (NaN/overlap) +3 blend branches\n");return reflex_vm::RUN_ORIGINAL;
}
