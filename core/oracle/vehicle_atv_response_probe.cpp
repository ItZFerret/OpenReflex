#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}void wf(uint32_t a,float f){uint32_t b;std::memcpy(&b,&f,4);wr(a,b);}struct Save{uint32_t a;std::vector<uint32_t>b;Save(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)b.push_back(rd(p+i));}void restore(){for(size_t i=0;i<b.size();i++)wr(a+4*uint32_t(i),b[i]);}~Save(){restore();}};}
RX_PORT(0x0084ed20,AtvResponseProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_RESPONSE_PROBE")||++frame!=650)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,g=rd(0xa946b8);if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t x=rd(p+0x1ac);if(x&&m.mem.mapped(x)&&rd(x)==0x93d0ec){v=x;break;}}
 if(!v){std::fprintf(stderr,"[atv-response] ERROR no actual ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 {uint32_t s=rd(v+0x1700);Save saved(s,0x9bd0);auto call=[&](uint32_t fn,std::vector<uint32_t>a){rx_regs in{},out{};in.r[1]=s;in.r[7]=s;rx_call_original_ex(fn,&in,&out,int(a.size()),a.data());};
  const uint32_t contact[]={0x94e0,0x9240,0x9630,0x9390,0x94a0,0x9200,0x95f0,0x9350,0x94a4,0x9204,0x95f4,0x9354};
  for(unsigned k=0;k<14;k++){for(uint32_t off:contact)wr(s+off,k==0?0:1);if(k>=2)wr(s+contact[k-2],0);call(0x48ee30,{});call(0x48ee60,{});saved.restore();}
  for(unsigned k=0;k<4;k++){for(uint32_t off:{0x9510u,0x9270u,0x9660u,0x93c0u})for(unsigned i=0;i<4;i++){uint32_t val=k==0?0:k==1?(i==1?0x3f800000u:0u):k==2?(i==0?0x7f800000u:0u):(i==2?0x7fc12345u:0u);wr(s+off+4*i,val);}call(0x48eed0,{s+0x9720});saved.restore();}
  for(unsigned k=0;k<5;k++){
   for(uint32_t q:{0x730u,0x870u,0x910u,0x9b0u,0xa50u}){wf(s+q,0);wf(s+q+4,0);wf(s+q+8,0);wf(s+q+12,1);}
   for(unsigned i=0;i<4;i++)wf(s+0x740+4*i,i<3?float(i+1):0);
   for(uint32_t a:{0x880u,0x920u,0x9c0u,0xa60u})for(unsigned i=0;i<4;i++)wf(s+a+4*i,i<3?float(i+1)+(k==4?30.0f:.1f):0);
   wf(s+0x24,k&1?1:0);wf(s+0x28,k&2?1:0);call(0x48ef80,{});saved.restore();
  }
  std::fprintf(stderr,"[atv-response] 28 contact predicates,4 normal edge cases,5 slip limiter cases compared; actual S restored. S15 uses natural frame calls.\n");
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
