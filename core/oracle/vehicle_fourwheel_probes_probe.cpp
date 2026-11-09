/* Development-only restored-state stimuli on a real garage-selected world.
 * REFLEX_PORT=rx_FourwheelProbes,rx_FourwheelProbesHook REFLEX_PORT_VERIFY=1
 * RX_FOURWHEEL_PROBES=1 RX_FOURWHEEL_PROBES_FRAME=20. */
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
RX_PORT(0x0084ed20,FourwheelProbesHook,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_PROBES"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FOURWHEEL_PROBES_FRAME")?uint32_t(std::atoi(std::getenv("RX_FOURWHEEL_PROBES_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rd(0xa946b8),vt=0;
 if(g)for(uint32_t i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&(rd(a)==0x0093d43c||rd(a)==0x0093d24c||rd(a)==0x0093c93c)){s=a;vt=rd(a);break;}}
 if(!s){std::fprintf(stderr,"[fourwheel-probes] ERROR no live fourwheel simulation\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  Region state(s,0x2200);uint32_t calls=0;
  uint32_t fn=vt==0x0093d43c?0x004da510:vt==0x0093d24c?0x004bf400:0x0045b8f0;
  auto invoke=[&](){rx_regs in{},out{};in.r[1]=s;rx_call_original_ex(fn,&in,&out,0,nullptr);++calls;state.restore();};
  // Actual live quaternion/body positions and authored probe count.
  invoke();
  const uint32_t speeds[]={0,0x80000000,0x411fffff,0x41200000,0x41200001,0x41a00000,0x7f800000,0x7fc12345,0x7fa12345};
  for(auto speed:speeds){wr(s+0x100,speed);wr(s+0x104,0);wr(s+0x108,0);invoke();}
  for(auto count:{0u,1u,21u,25u,0xffffffffu}){wr(s+0x1318,count);invoke();}
  // Preserve world pose but exercise identity, nontrivial normalized, zero,
  // and deliberately non-unit quaternions; all four vector lanes participate.
  const uint32_t quats[][4]={{0,0,0,0x3f800000},{0x3f000000,0xbf000000,0x3f000000,0x3f000000},{0,0,0,0},{0x3f800000,0x40000000,0xc0400000,0x40800000}};
  for(auto &q:quats){for(unsigned i=0;i<4;i++)wr(s+0x730+4*i,q[i]);invoke();}
  if(vt==0x0093c93c){
   // Cover BOTH class7 and nonclass7 branches independently of selected model.
   // Class metadata is outside saved output/body region, restore explicitly.
   uint32_t cls=rd(s+0x55f0);
   for(auto kind:{7u,2u})for(auto speed:{0u,0x41200000u,0x41a00000u,0x7fc12345u}){wr(s+0x55f0,kind);wr(s+0x100,speed);wr(s+0x104,0);wr(s+0x108,0);invoke();}
   wr(s+0x55f0,cls);
  }
  std::fprintf(stderr,"[fourwheel-probes] complete vt=%08x fn=%08x %u restored stimuli; inspect positive Verify counts\n",vt,fn,calls);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
