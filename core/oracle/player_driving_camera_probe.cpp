/* Real local-player DrivingCamera selection/restoration, registered dispatch.
 * RX_DRIVING_CAMERA_PROBE=1, RX_DRIVING_CAMERA_FRAME=20; group
 * rx_PlayerDrivingCamera, hook rx_PlayerDrivingCameraProbe. */
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
RX_PORT(0x0084ed20,PlayerDrivingCameraProbe,"thunk",0){
 if(!std::getenv("RX_DRIVING_CAMERA_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_DRIVING_CAMERA_FRAME")?uint32_t(std::atoi(std::getenv("RX_DRIVING_CAMERA_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=4096)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t g=rd(0x00a946b8),P=0,A=0,settings=rd(0x00d92efc);
 if(mapped(g+0x7460,0x12a0))for(unsigned i=0;i<16;i++){
  uint32_t p=g+0x7460+0x12a0*i;if(!mapped(p,0x12a0)||rd(p+4)==0||rd(p+0x10)!=0)continue;
  uint32_t act=rd(p+0x94);if(!mapped(act+0x21c,4))continue;
  bool valid=true;for(unsigned camera:{0u,1u,2u,4u})valid&=mapped(rd(p+0x974+4*camera),0x370);
  if(valid){P=p;A=act;break;}
 }
 if(!P||!mapped(settings,0x170)){std::fprintf(stderr,"[driving-camera] ERROR no actual local-player/settings/camera fixture\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[driving-camera] ERROR missing reversible host state\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned calls=0,negative=0,fallback=0,selected=0,bad=0;
 auto invoke=[&](uint32_t expected){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[1]=P;rx_call_original_ex(0x0054aef0,&in,&out,0,nullptr);if(out.r[1]!=expected)++bad;++calls;};
 for(uint32_t controller:{0xffffffffu,0x80000000u}){wr(P+0x1f0,controller);invoke(P);++negative;restore();}
 for(uint32_t controller:{0u,1u})for(uint32_t state:{0u,1u,2u,3u})for(uint32_t preset:{0u,1u,2u,3u,4u,0xffffffffu}){
  wr(P+0x1f0,controller);wr(P+0x988,1);wr(A+0x21c,state);
  wr(settings+0x16c,state==0?0xffffffffu:state%2);
  wr(settings+4+0x48*controller,preset);
  uint32_t expected=preset==0?0:preset==1?1:preset==2?4:preset==3?2:1;
  invoke(expected);
  if(rd(0x009c5410+4*controller)!=expected)++bad;
  if(state==0){++fallback;if(rd(P+0x988)!=1)++bad;}
  else {++selected;if(rd(P+0x988)!=expected)++bad;}
  restore();
 }
 restore();
 std::fprintf(stderr,"[driving-camera] %s calls=%u negative=%u refused=%u selected=%u live-ecx-errors=%u P=%08x; full guest/host/CPU restored\n",bad?"ERROR":"complete",calls,negative,fallback,selected,bad,P);
 return reflex_vm::RUN_ORIGINAL;
}
