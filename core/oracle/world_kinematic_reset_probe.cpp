/* Actual keyed-body authored reset and damage-entity write-back, fully restored.
 * RX_KIN_RESET_PROBE=1, RX_KIN_RESET_FRAME=20; group rx_KinematicReset,
 * hook rx_KinematicResetProbe. Real world/grid/shape/entity pointers only. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t p){return rx_rd32(p);}void wr(uint32_t p,uint32_t v){rx_wr32(p,v);}
uint32_t fb(float x){uint32_t b;std::memcpy(&b,&x,4);return b;}
float fl(uint32_t b){float x;std::memcpy(&x,&b,4);return x;}
void wf(uint32_t p,float x){wr(p,fb(x));}
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
RX_PORT(0x0084ed20,KinematicResetProbe,"thunk",0){
 if(!std::getenv("RX_KIN_RESET_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_KIN_RESET_FRAME")?uint32_t(std::atoi(std::getenv("RX_KIN_RESET_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=4096)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t g=rd(0x00a946b8),W=0;std::vector<uint32_t> bodies;
 if(mapped(g,0x5a58))for(uint32_t off:{0x5a54u,0x20c8u}){
  uint32_t level=rd(g+off);if(!mapped(level+0x20134,4))continue;
  uint32_t world=rd(level+0x20134);if(!mapped(world,0xcce0))continue;
  uint32_t begin=rd(world+0x19a0),end=rd(world+0x19a4);
  if(end<begin||(end-begin)%0xe0||(end-begin)/0xe0>10000||!mapped(rd(world+0x19ac),16))continue;
  for(uint32_t O=begin;O<end&&bodies.size()<3;O+=0xe0){
   if(!mapped(O,0xe0))continue;uint32_t entity=rd(O+0x34);
   if(!mapped(entity,0x180)||!mapped(rd(entity)+0x78,4)||rd(rd(entity)+0x78)!=0x00430560)continue;
   bodies.push_back(O);
  }
  if(!bodies.empty()){W=world;break;}
 }
 if(!W||bodies.empty()){std::fprintf(stderr,"[kin-reset] ERROR no actual keyed damage-entity/grid fixture\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[kin-reset] ERROR missing reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned calls=0,reset_calls=0,entity_calls=0,bad=0;
 for(uint32_t O:bodies)for(unsigned variant=0;variant<4;variant++)for(unsigned kind=0;kind<2;kind++){
  uint32_t entity=rd(O+0x34);
  wf(O+0x60,fl(rd(O+0x60))+.25f*variant);wf(O+0x68,fl(rd(O+0x68))-.125f*variant);
  if(variant){float q[4]={0,0,0,1};if(variant==2){q[1]=.70710677f;q[3]=.70710677f;}if(variant==3)for(float& x:q)x=.5f;for(unsigned i=0;i<4;i++)wf(O+0x70+4*i,q[i]);}
  wf(O+0x40,fl(rd(O+0x60))+3);wf(O+0x48,fl(rd(O+0x68))+2);
  wr(O+0xb8,variant==0?0:variant==1?7:variant==2?0x38:0xa5a5a5a5);wr(O+0xb4,0xffffffff);
  uint8_t buoyant=variant==0?0:variant==1?1:variant==2?0x80:0xff;rx_wr8(O+0xd1,buoyant);rx_wr8(O+0xd0,uint8_t(~buoyant));
  wf(entity+0x170,variant? .25f*variant:0);wf(entity+0x174,variant?1.5f:0);wf(entity+0x178,variant?-.75f:0);wr(entity+0x17c,0x7fc12345);
  rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];
  if(kind==0){
   in.r[1]=W;in.r[2]=O;rx_call_original_ex(0x0076abb0,&in,&out,0,nullptr);++reset_calls;
   for(unsigned i=0;i<8;i++)if(rd(O+0x40+4*i)!=rd(O+0x60+4*i))++bad;
   if(rd(O+0xb4)!=rd(O+0xb8)||rx_rd8(O+0xd0)!=rx_rd8(O+0xd1))++bad;
  }else{
   in.r[1]=entity;uint32_t args[]={O+0x60,O+0x70};rx_call_original_ex(0x00430560,&in,&out,2,args);++entity_calls;
  }
  ++calls;restore();
 }
 restore();
 std::fprintf(stderr,"[kin-reset] %s calls=%u reset=%u entity=%u actual-bodies=%zu world=%08x publication-errors=%u real-grid=yes; full guest/host/CPU restored\n",bad?"ERROR":"complete",calls,reset_calls,entity_calls,bodies.size(),W,bad);
 return reflex_vm::RUN_ORIGINAL;
}
