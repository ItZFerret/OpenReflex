/* Restored actual four-wheel terrain/reset fixtures. RX_FOURWHEEL_GROUND_PROBE=1,
 * RX_FOURWHEEL_GROUND_FRAME=20 (default), group rx_FourWheelGroundPlace.
 * All five pointer arguments are distinct aligned real guest-stack buffers.
 * Original vehicle/simulation/terrain/body objects remain intact. Full guest
 * pages, added mappings, host allocator and CPU are restored after each call. */
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t p){return rx_rd32(p);}void wr(uint32_t p,uint32_t v){rx_wr32(p,v);}
uint32_t fb(float x){uint32_t b;std::memcpy(&b,&x,4);return b;}
float fl(uint32_t b){float x;std::memcpy(&x,&b,4);return x;}
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
RX_PORT(0x0084ed20,FourwheelGroundPlaceProbe,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_GROUND_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FOURWHEEL_GROUND_FRAME")?uint32_t(std::atoi(std::getenv("RX_FOURWHEEL_GROUND_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=4096)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t g=rd(0x00a946b8),v=0,s=0,fn=0;
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t player=g+0x7460+0x12a0*i;if(!mapped(player+0x1ac,4))continue;
  uint32_t a=rd(player+0x1ac);if(!mapped(a,0x1630))continue;
  uint32_t vt=rd(a),off=vt==0x0093d4d4?0x1540:vt==0x0093d2e4?0x15c0:vt==0x0093cee4?0x15d0:0;
  if(!off)continue;uint32_t sim=rd(a+off);if(!mapped(sim,0x8930))continue;
  uint32_t target=rd(rd(sim)+20*4);if(target!=0x004cc980&&target!=0x004b2180)continue;
  v=a;s=sim;fn=target;break;
 }
 uint32_t scratch=(m.r[4]-0x1000)&~15u;
 if(!v||!mapped(scratch-0x1000,0x2000)){std::fprintf(stderr,"[fourwheel-ground] ERROR no actual vehicle or stack fixture\n");return reflex_vm::RUN_ORIGINAL;}
 std::array<float,4> position{};for(unsigned i=0;i<4;i++)position[i]=fl(rd(s+0x180+4*i));
 for(float x:position)if(!std::isfinite(x)){std::fprintf(stderr,"[fourwheel-ground] ERROR nonfinite authored position\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[fourwheel-ground] ERROR missing reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 const float headings[][3]={{0,0,1},{1,0,0},{0,0,-1},{-1,0,0},{3,2,4},{0,1,0}};
 unsigned calls=0,changed[3]={0,0,0},grounded[3]={0,0,0};
 for(unsigned site=0;site<3;site++)for(unsigned h=0;h<6;h++){
  /* Same real track location, below/at/above its authored height: exercise
   * terrain raise and downward wheel queries without fabricating geometry. */
  float dy=site==0?-20.0f:site==1?0.0f:20.0f;
  for(unsigned j=0;j<4;j++){wr(scratch+4*j,fb(position[j]+(j==1?dy:0)));wr(scratch+16+4*j,fb(j<3?headings[h][j]:0));}
  for(unsigned j=0;j<12;j++)wr(scratch+32+4*j,0x4b123456);
  uint32_t args[]={scratch,scratch+16,scratch+32,scratch+48,scratch+64};
  rx_regs in{},out{};for(unsigned j=0;j<8;j++)in.r[j]=regs[j];in.r[1]=s;
  rx_call_original_ex(fn,&in,&out,5,args);
  for(unsigned output=0;output<3;output++)if(rd(scratch+32+16*output)!=0x4b123456)++changed[output];
  if(rd(scratch+32)!=0x4b123456&&rd(scratch+64)!=0x4b123456)++grounded[site];
  ++calls;restore();
 }
 restore();
 std::fprintf(stderr,"[fourwheel-ground] complete %u restored registered calls V=%08x S=%08x vf20=%08x position=(%g,%g,%g) outputwrites=%u/%u/%u grounded-below/at/above=%u/%u/%u; five pointers RET14h, pages/heap/CPU restored\n",calls,v,s,fn,position[0],position[1],position[2],changed[0],changed[1],changed[2],grounded[0],grounded[1],grounded[2]);
 return reflex_vm::RUN_ORIGINAL;
}
