/* Ten direct restored constructor cases; no FMX runtime claim. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
struct Pages {
 reflex_vm::Memory& memory;
 struct Page{uint32_t address;std::array<uint8_t,4096> bytes;};
 std::vector<Page> saved;std::vector<uint8_t> mapped;
 explicit Pages(reflex_vm::Memory& m):memory(m),mapped(1u<<20,0){
  /* Fault in all backed pages before snapshot: later lazy touches then cannot
   * conceal a modified page. A captured world has bounded mapped memory. */
  for(uint32_t i=0;i<(1u<<20);i++)if(m.mapped(i<<12)){
   mapped[i]=1;saved.push_back(Page{});auto& p=saved.back();p.address=i<<12;
   std::memcpy(p.bytes.data(),m.page(p.address),4096);
  }
 }
 void restore(){
  for(auto& p:saved)std::memcpy(memory.page(p.address),p.bytes.data(),4096);
  for(uint32_t i=0;i<(1u<<20);i++)if(!mapped[i]&&memory.mapped(i<<12))memory.unmap(i<<12,4096);
 }
 ~Pages(){restore();}
};
}
RX_PORT(0x0084ed20,FreestyleConstructorProbe,"thunk",0){
 if(!std::getenv("RX_FREESTYLE_CONSTRUCT_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FREESTYLE_CONSTRUCT_FRAME")?uint32_t(std::atoi(std::getenv("RX_FREESTYLE_CONSTRUCT_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t world=0; const uint32_t game=rd(0x00a946b8);
 if(game&&m.mem.mapped(game+0x7460))for(unsigned i=0;i<16;i++){
  uint32_t p=game+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x94)||!rd(p+4)||rd(p+0x10)!=0)continue;
  uint32_t a=rd(p+0x94);if(a&&m.mem.mapped(a+0x234)){world=rd(a+0x234);if(world)break;}
 }
 if(!world||!m.mem.mapped(world)||!m.mem.mapped(world+0x21dc0)){
  std::fprintf(stderr,"[freestyle-construct] ERROR no actual MXGame/world fixture\n");return reflex_vm::RUN_ORIGINAL;
 }
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[freestyle-construct] ERROR missing reversible host state\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned calls=0,audio=0,stub=0,concrete=0,bad=0;
 auto allocate=[&](uint32_t n,uint8_t pattern){
  uint32_t a=rx_call_original(0x0078fbe0,RX_CDECL,0,1,&n);
  if(a)for(uint32_t i=0;i<n;i++)rx_wr8(a+i,pattern);
  return a;
 };
 auto eq=[&](uint32_t a,uint32_t expected){if(rd(a)!=expected)++bad;};
 auto audio_check=[&](uint32_t l,uint8_t pattern,bool dirty){
  eq(l,0x00942bbc);if(rx_rd8(l+4)!=0)++bad;
  if(dirty)for(unsigned i=5;i<8;i++)if(rx_rd8(l+i)!=pattern)++bad;
  for(uint32_t o:{0x50u,0x54u,0x8cu,0x90u,0x94u})eq(l+o,0);
  for(uint32_t o=0x5c;o<=0x88;o+=4)eq(l+o,0xffffffffu);
 };
 auto invoke=[&](uint32_t fn,uint32_t A,uint32_t type,bool stack){
  rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];
  in.r[6]=A;in.r[1]=type;in.r[0]=world;
  rx_call_original_ex(fn,&in,&out,stack?1:0,stack?&A:nullptr);
  if(fn!=0x00562280&&out.r[0]!=A)++bad;
  ++calls;
 };
 for(uint8_t pattern:{uint8_t(0xa5),uint8_t(0x5a)}){
  uint32_t l=allocate(0x98,pattern);
  if(!l){++bad;restore();continue;}
  invoke(0x005623b0,l,0,false);audio_check(l,pattern,true);++audio;restore();
 }
 for(bool derived:{false,true})for(uint32_t type:{5u,20u})for(uint8_t pattern:{uint8_t(0xa5),uint8_t(0x5a)}){
  uint32_t n=derived?0x13e610:0x13e600,A=allocate(n,pattern);
  if(!A){++bad;restore();continue;}
  invoke(derived?0x00561910:0x00562280,A,type,!derived);
  eq(A,derived?0x009442cc:0x009441cc);eq(A+0x234,world);eq(A+0x23c,type);
  eq(A+0xff080,0x0094410c);eq(A+0x114250,0x0094414c);eq(A+0x129420,0x0094418c);
  for(uint32_t o:{0xff094u,0x114264u,0x129434u})eq(A+o,A);
  for(uint32_t o:{0x114234u,0x129404u,0x13e5d4u})eq(A+o,15);
  eq(A+0x2c8,0);eq(A+0x22c,1);eq(A+0x13e5f0,0);
  /* Audio is the original constructor output, never synthesized. */
  uint32_t l=rd(A+0xfd8);if(!l)++bad;else audio_check(l,0,false);
  if(derived){eq(A+0x13e600,0);eq(A+0x13e604,100);eq(A+0x13e608,5);++concrete;}
  else ++stub;
  restore();
 }
 restore();
 std::fprintf(stderr,"[freestyle-construct] %s calls=%u audio=%u stub=%u concrete=%u errors=%u world=%08x; full guest/host/CPU restored\n",bad?"ERROR":"complete",calls,audio,stub,concrete,bad,world);
 return reflex_vm::RUN_ORIGINAL;
}
