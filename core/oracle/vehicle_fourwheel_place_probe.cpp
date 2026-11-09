/* Restored real-world V/S and tyre-track reset stimuli. Development only.
 * REFLEX_PORT=rx_FourwheelPlace,rx_FourwheelPlaceHook REFLEX_PORT_VERIFY=1
 * RX_FOURWHEEL_PLACE=1 RX_FOURWHEEL_PLACE_FRAME=20. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t x){rx_wr32(a,x);}
struct Region{uint32_t a;std::vector<uint32_t> v;Region(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)v.push_back(rd(p+i));}void restore()const{for(size_t i=0;i<v.size();i++)wr(a+uint32_t(4*i),v[i]);}~Region(){restore();}};
}
RX_PORT(0x0084ed20,FourwheelPlaceHook,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_PLACE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FOURWHEEL_PLACE_FRAME")?uint32_t(std::atoi(std::getenv("RX_FOURWHEEL_PLACE_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,vehicle=0,g=rd(0xa946b8),vt=0;
 if(g)for(uint32_t i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&(rd(a)==0x0093d43c||rd(a)==0x0093d24c||rd(a)==0x0093c93c)){s=a;vehicle=v;vt=rd(a);break;}}
 uint32_t simptr=vt==0x0093d43c?0x1540:vt==0x0093d24c?0x15c0:0x15d0;
 if(!s||rd(vehicle+simptr)!=s){std::fprintf(stderr,"[fourwheel-place] ERROR no real fourwheel V/S reference\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  uint32_t fn=vt==0x0093d43c?0x004c6c40:vt==0x0093d24c?0x004ac2e0:0x00473210;
  uint32_t handles=vt==0x0093d43c?0x13c4:vt==0x0093d24c?0x1410:0x144c;
  uint32_t contact=handles+16,extra=vt==0x0093d43c?0x1468:vt==0x0093d24c?0x14b8:0x14f8;
  uint32_t tail=vt==0x0093d43c?0x14a0:vt==0x0093d24c?0x14f0:0x1530;
  Region vehicleState(vehicle,simptr+4),state(s,vt==0x0093d43c?0x8850:vt==0x0093d24c?0x8840:0x8930),trackStatic(0x9e30dc,8),zeroVector(0xd61670,16);
  std::vector<Region*> records;
  auto saveStrip=[&](uint32_t pool,uint32_t idx){uint32_t strip=rd(pool+0x24)+0xc0*idx;records.push_back(new Region(strip,0xc0));uint32_t ring=rd(strip+0xc),count=rd(pool+4);if(ring&&count)records.push_back(new Region(ring,4*count));};
  for(unsigned i=0;i<4;i++){uint32_t h=rd(vehicle+handles+4*i),bit=(h&0x1000)?0x1000:(h&0x2000)?0x2000:0x4000;int32_t idx=int32_t(h^bit);if(idx>=0)saveStrip(rd(bit==0x1000?0xa95774:bit==0x2000?0xa95778:0xa9577c),uint32_t(idx));}
  for(unsigned i=0;i<3;i++)saveStrip(rd(0xa95774+4*i),0);
  auto restore=[&](){vehicleState.restore();state.restore();trackStatic.restore();zeroVector.restore();for(auto p:records)p->restore();};
  uint32_t calls=0,wraps=0;
  auto invoke=[&](){rx_regs in{},out{};in.r[1]=vehicle;rx_call_original_ex(fn,&in,&out,0,nullptr);++calls;restore();};
  invoke(); // Original live four handles, including any actual pending strips.
  // Invalid negative handles, both static guard paths; fill only reset-owned
  // fields so expected zero stores are observable while object pointers survive.
  for(auto guard:{0x10u,0x11u}){
   for(unsigned i=0;i<4;i++){wr(vehicle+handles+4*i,0xffffffff);wr(0xd61670+4*i,0x3f000000+0x10000*i);}
   for(unsigned i=0;i<40;i+=4)wr(vehicle+contact+i,0xdeadbeef);
   for(unsigned i=0;i<48;i+=4)wr(vehicle+extra+i,0xdeadbeef);
   for(unsigned i=0;i<16;i+=4)wr(vehicle+tail+i,0xdeadbeef);
   wr(0x9e30e0,guard);wr(0x9e30dc,0x12345678);invoke();
  }
  // Three real pools; preserve every strip descriptor and its entire ring.
  for(unsigned i=0;i<3;i++){
   uint32_t pool=rd(0xa95774+4*i),strip=rd(pool+0x24),bit=0x1000u<<i;
   for(unsigned mode=0;mode<5;mode++){
    bool wrap=mode>=3;uint32_t ring=rd(strip+0xc),count=rd(pool+4);
    if(wrap&&(!ring||!count||!m.mem.mapped(ring+4*(count-1))))continue;
    for(unsigned j=0;j<4;j++)wr(vehicle+handles+4*j,bit);
    wr(strip+0xa0,mode==1?1:0);wr(strip+8,mode<2?0:mode==2?5:mode==3?119:121);
    wr(strip+0x14,mode==3?0x3f800000:0);if(wrap){wr(strip+4,count-1);++wraps;}
    invoke();
   }
  }
  for(auto p:records)delete p;
  std::fprintf(stderr,"[fourwheel-place] complete vt=%08x fn=%08x %u restored stimuli (%u rollover cases); real V=%08x S=%08x\n",vt,fn,calls,wraps,vehicle,s);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
