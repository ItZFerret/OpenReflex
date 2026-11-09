/* Development-only stimulus on an existing original UTV/Buggy/Truck world, never linked into product.
 * REFLEX_PORT=rx_vehicle_fourwheel,rx_FourwheelMethodsProbeHook REFLEX_PORT_VERIFY=1 RX_FOURWHEEL_METHODS_PROBE=1
 * RX_FOURWHEEL_METHODS_FRAME=650. Saves actual S/12 range records; isolates external contacts for reset compare. */
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
RX_PORT(0x0084ed20,FourwheelMethodsProbeHook,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_METHODS_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FOURWHEEL_METHODS_FRAME")?uint32_t(std::atoi(std::getenv("RX_FOURWHEEL_METHODS_FRAME"))):650;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rd(0xa946b8),vt=0;
 if(g)for(uint32_t i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&(rd(a)==0x0093d43c||rd(a)==0x0093d24c||rd(a)==0x0093c93c)){s=a;vt=rd(a);break;}}
 if(!s){std::fprintf(stderr,"[fourwheel-methods] ERROR no live fourwheel simulation\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  const uint32_t d=vt==0x0093d24c?0x10:0;
  const uint32_t tuning=vt==0x0093d43c?0x004cc610:vt==0x0093d24c?0x004b1e10:0x00450ab0;
  const uint32_t geometry=vt==0x0093d43c?0x004d4750:vt==0x0093d24c?0x004ba270:0x004564d0;
  const uint32_t reset=vt==0x0093d43c?0x004d75b0:vt==0x0093d24c?0x004bce80:0x00459400;
  Region state(s,vt==0x0093d43c?0x8850:vt==0x0093d24c?0x8840:0x8930);std::vector<Region*> ranges;
  for(unsigned i=0;i<12;i++)ranges.push_back(new Region(rd(s+0x85d0-d+4*i),16));
  auto restore=[&](){state.restore();for(auto p:ranges)p->restore();};
  auto invoke=[&](uint32_t fn){rx_regs in{},out{};in.r[1]=s;rx_call_original_ex(fn,&in,&out,0,nullptr);};
  invoke(tuning);restore();
  wr(s+0x368c,0x7f7fffff);wr(s+0x2ef8,0x3f800000);wr(s+0x3b14,0x7f7fffff);wr(s+0x2efc,0x3f800000);invoke(tuning);restore();
  wr(s+0x368c,0x7fa12345);wr(s+0x3b14,0x80000000);invoke(tuning);restore();
  invoke(geometry);restore();
  // Front radius clamp: force dy^2 beyond half-track^2 using actual body positions.
  wr(s+0x3fe4,0);wr(s+0x194,0x42c80000);wr(s+0x234,0xc2c80000);
  invoke(geometry);restore();
  // Rear direction fallback on both y signs, with original live pose/quaternion.
  wr(s+0x2d4,0x42c80000);wr(s+0x374,0xc2c80000);
  invoke(geometry);restore();
  // Keep real five bodies and pointers. Remove external contact queues/rays only for this reset fixture.
  wr(s+0x2ebc,0);wr(s+0x1318,0);wr(s+0x1310,rd(s+0x130c));
  rx_regs in{},out{};in.r[1]=s;const uint32_t args[]={s+0xf0,s+0x730};rx_call_original_ex(reset,&in,&out,2,args);
  for(auto p:ranges)delete p;
  std::fprintf(stderr,"[fourwheel-methods] complete vt=%08x:3 tuning +3 geometry stimuli + real five-body reset; state restored; check Verify call counts for registered geometry coverage\n",vt);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
