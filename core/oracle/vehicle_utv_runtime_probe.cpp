/* Restored-state UTV getter/projection/angular-clamp stimuli.
 * REFLEX_PORT=rx_UtvRuntime REFLEX_PORT_VERIFY=1 RX_UTV_RUNTIME_PROBE=1
 * RX_UTV_RUNTIME_FRAME=20 (default). Hook remains group thunk.
 * These leaves never touch model/rider memory. Save actual V/S allocations,
 * caller stack (including the getter output) and full register/FPU state. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;
uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t x){rx_wr32(a,x);}
float rf(uint32_t a){uint32_t x=rd(a);float f;std::memcpy(&f,&x,4);return f;}
void wf(uint32_t a,float f){uint32_t x;std::memcpy(&x,&f,4);wr(a,x);}
void vec(uint32_t a,float x,float y,float z,float w){wf(a,x);wf(a+4,y);wf(a+8,z);wf(a+12,w);}
struct Region{uint32_t a;std::vector<uint32_t> words;Region(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)words.push_back(rd(p+i));}void restore()const{for(size_t i=0;i<words.size();i++)wr(a+uint32_t(i*4),words[i]);}~Region(){restore();}};
}
RX_PORT(0x0084ed20,UtvRuntimeProbe,"thunk",0){
 if(!std::getenv("RX_UTV_RUNTIME_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const uint32_t at=std::getenv("RX_UTV_RUNTIME_FRAME")?uint32_t(std::atoi(std::getenv("RX_UTV_RUNTIME_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=0x1000)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t v=0,s=0,g=rd(0x00a946b8);
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t player=g+0x7460+0x12a0*i;if(!mapped(player+0x1ac,4))continue;
  uint32_t candidate=rd(player+0x1ac);if(!mapped(candidate,0x15a0)||rd(candidate)!=0x0093d4d4)continue;
  uint32_t sim=rd(candidate+0x1540);
  if(mapped(sim,0x8850)&&rd(sim)==0x0093d43c&&rd(candidate+0x234)==sim){v=candidate;s=sim;break;}
 }
 uint32_t stack=m.r[4];
 if(!v||stack<0x4000||!mapped(stack-0x4000,0x4000)){std::fprintf(stderr,"[utv-runtime] ERROR no valid live UTV/stack snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 {
  Region vehicle(v,0x15a0),simulation(s,0x8850),scratch(stack-0x4000,0x4000);
  auto restore=[&](){vehicle.restore();simulation.restore();scratch.restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;};
  unsigned calls=0;uint32_t outvec=(stack-0x800)&~15u;
  auto invoke=[&](uint32_t fn,bool is_vehicle,int nargs,const uint32_t* args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[1]=is_vehicle?v:s;rx_call_original_ex(fn,&in,&out,nargs,args);++calls;restore();};
  const uint32_t vector_fn[]={0x004c6ec0,0x004c6ee0,0x004c6f00,0x004c6f20,0x004c6f40,0x004c6f60};
  const uint32_t vector_off[]={0x180,0x7c0,0x100,0x740,0x160,0x7a0};
  for(unsigned i=0;i<6;i++){
   uint32_t args[]={outvec};invoke(vector_fn[i],true,1,args);
   vec(s+vector_off[i],-123.5f,-0.0f,.25f,10000.0f);invoke(vector_fn[i],true,1,args);
  }
  for(uint32_t fn:{0x004c6f80u,0x004c6fa0u})for(float value:{0.0f,-1234.5f,1234.5f,1.0e20f}){wf(s+(fn==0x004c6f80?0x865c:0x4d4c),value);invoke(fn,true,0,nullptr);}
  for(uint32_t value:{0xffffffffu,0u,1u,0x7fffffffu}){wr(s+0x8660,value);invoke(0x004c6fb0,true,0,nullptr);}
  for(uint32_t fn:{0x004c6fc0u,0x004c6fd0u})for(uint32_t value:{0u,1u,0xffffffffu}){wr(s+(fn==0x004c6fc0?0x8700:0x8708),value);invoke(fn,true,0,nullptr);}
  for(uint32_t mask=0;mask<16;mask++){
   const uint32_t off[]={0x8180,0x82d0,0x8420,0x8570};for(unsigned i=0;i<4;i++)wr(s+off[i],(mask&(1u<<i))?0xffffffffu:0);invoke(0x004c6fe0,true,0,nullptr);
  }
  for(uint32_t value:{0u,1u,2u,3u,0xffffffffu}){wr(s+0x56fc,value);invoke(0x004c7040,true,0,nullptr);}
  /* Projection inactive/active, zero/one/five body counts, and both an axis and
   * a unit diagonal. Retain real body positions and offset the reference. */
  for(uint32_t active:{0u,1u})for(uint32_t count:{0u,1u,5u})for(unsigned diagonal=0;diagonal<2;diagonal++){
   wr(s+0x90,active);wr(s+0xec,count);
   vec(s+0x8690,diagonal?.6f:1.0f,diagonal?.8f:0.0f,0,0);
   vec(s+0x86a0,rf(s+0xf0)+.2f,rf(s+0xf4)-.3f,rf(s+0xf8)+.1f,rf(s+0xfc)+.25f);
   invoke(0x004cdb70,false,0,nullptr);
  }
  /* Clamp below/equal/above each original limit, including chassis-at-count0
   * and wheel iteration at count5. A nonzero fourth lane checks packed scaling. */
  for(uint32_t count:{0u,1u,5u})for(float factor:{0.0f,.5f,1.0f,1.001f,2.0f}){
   wr(s+0xec,count);wr(s+0x8020,0);
   for(unsigned i=0;i<5;i++){float limit=rf(i?0x009d7b34:0x00935aac);vec(s+0x740+0xa0*i,limit*factor,0,0,3.0f);}
   uint32_t args[]={s};invoke(0x004cdbe0,false,1,args);
  }
  std::fprintf(stderr,"[utv-runtime] complete %u restored stimuli V=%08x S=%08x; check positive rx_UtvRuntime Verify counts\n",calls,v,s);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
