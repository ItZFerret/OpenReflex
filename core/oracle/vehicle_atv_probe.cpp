/* Development-only stimulus on an existing original ATV world, never linked into product.
 * REFLEX_PORT=rx_AtvMethods,rx_AtvMethodsProbeHook REFLEX_PORT_VERIFY=1 RX_ATV_METHODS_PROBE=1
 * RX_ATV_METHODS_FRAME=650. Saves actual S/12 range records; isolates external contacts for reset compare. */
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
RX_PORT(0x0084ed20,AtvMethodsProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_METHODS_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_ATV_METHODS_FRAME")?uint32_t(std::atoi(std::getenv("RX_ATV_METHODS_FRAME"))):650;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,vehicle=0,g=rd(0xa946b8);
 if(g)for(uint32_t i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&rd(a)==0x0093d054){s=a;vehicle=v;break;}}
 if(!s){std::fprintf(stderr,"[atv-methods] ERROR no live ATV simulation\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  Region vehicleState(vehicle,0x1760), trackStatic(0x9e30dc,8);
  std::vector<Region*> trackRecords;
  for(unsigned i=0;i<4;i++){
   uint32_t h=rd(vehicle+0x1584+4*i), bit=(h&0x1000)?0x1000:((h&0x2000)?0x2000:0x4000);
   int32_t index=int32_t(h^bit);
   if(index<0)continue;
   uint32_t pool=rd(bit==0x1000?0xa95774:(bit==0x2000?0xa95778:0xa9577c));
   uint32_t strip=rd(pool+0x24)+0xc0*uint32_t(index);
   trackRecords.push_back(new Region(strip,0xc0));
   uint32_t ring=rd(strip+0xc),count=rd(pool+4);
   if(ring&&count)trackRecords.push_back(new Region(ring,4*count));
  }
  Region state(s,0x9bd0);std::vector<Region*> ranges;
  for(unsigned i=0;i<12;i++)ranges.push_back(new Region(rd(s+0x9a14+4*i),16));
  auto restore=[&](){state.restore();for(auto p:ranges)p->restore();};
  auto invoke=[&](uint32_t fn){rx_regs in{},out{};in.r[1]=s;rx_call_original_ex(fn,&in,&out,0,nullptr);};
  {rx_regs in{},out{};in.r[1]=vehicle;rx_call_original_ex(0x004862e0,&in,&out,0,nullptr);}
  restore();vehicleState.restore();trackStatic.restore();
  for(auto p:trackRecords)p->restore();
  // Exercise all three pools with real strip zero, including the 0x1000 priority.
  for(unsigned i=0;i<3;i++){
   uint32_t pool=rd(0xa95774+4*i),strip=rd(pool+0x24);
   Region record(strip,0xc0);wr(strip+0xa0,0);wr(strip+8,0);
   for(unsigned j=0;j<4;j++)wr(vehicle+0x1584+4*j,(0x1000u<<i));
   rx_regs in{},out{};in.r[1]=vehicle;rx_call_original_ex(0x004862e0,&in,&out,0,nullptr);
   restore();vehicleState.restore();trackStatic.restore();
  }
  for(auto p:trackRecords)delete p;
  invoke(0x0048cde0);restore();
  wr(s+0x35ec,0x7f7fffff);wr(s+0x2ef8,0x3f800000);wr(s+0x39c4,0x7f7fffff);wr(s+0x2efc,0x3f800000);invoke(0x0048cde0);restore();
  wr(s+0x35ec,0x7fa12345);wr(s+0x39c4,0x80000000);invoke(0x0048cde0);restore();
  invoke(0x004997f0);restore();
  // Front radius clamp: force dy^2 beyond half-track^2 using actual body positions.
  wr(s+0x3e94,0);wr(s+0x234,0x42c80000);wr(s+0x2d4,0xc2c80000);
  invoke(0x004997f0);restore();
  // Rear direction fallback on both y signs, with original live pose/quaternion.
  wr(s+0x374,0x42c80000);wr(s+0x414,0xc2c80000);
  invoke(0x004997f0);restore();
  wr(s+0x1318,13);invoke(0x004a1a10);restore();
  wr(s+0x1318,13);wr(s+0x100,0);wr(s+0x104,0);wr(s+0x108,0);invoke(0x004a1a10);restore();
  wr(s+0x1318,13);wr(s+0x100,0x41a00000);wr(s+0x104,0);wr(s+0x108,0);invoke(0x004a1a10);restore();
  // Keep real six bodies and pointers. Remove external contact queues/rays only for this reset fixture.
  wr(s+0x2ebc,0);wr(s+0x1318,0);wr(s+0x1310,rd(s+0x130c));
  rx_regs in{},out{};in.r[1]=s;const uint32_t args[]={s+0xf0,s+0x730};rx_call_original_ex(0x0049c8d0,&in,&out,2,args);
  for(auto p:ranges)delete p;
  std::fprintf(stderr,"[atv-methods] complete:4 V ground resets +3 tuning +3 suspension-frame +3 thirteen-probe speed cases + real six-body reset; state restored\n");
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
