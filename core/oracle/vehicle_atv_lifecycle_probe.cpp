/* Original constructor creates valid base objects for bounded deleting-wrapper tests. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using namespace reflex_port;
namespace {uint32_t frame;}
RX_PORT(0x0084ed20,AtvLifecycleProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_LIFECYCLE_PROBE")||++frame!=650)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 auto call=[&](uint32_t fn,uint32_t ecx,std::initializer_list<uint32_t>a){rx_regs in{},out{};in.r[1]=ecx;rx_call_original_ex(fn,&in,&out,int(a.size()),a.begin());return out.r[0];};
 for(uint32_t flag:{0u,1u,2u}){
  uint32_t p=call(0x78fbe0,0,{6000});if(!p){std::fprintf(stderr,"[atv-lifecycle] ERROR original allocation failed\n");break;}
  call(0x45cec0,0,{p,0}); /* exact original base constructor owns valid tree/three buffer allocations */
  call(0x42af20,p+0x178,{}); /* actual original-constructed empty handle, callback's null branch */
  call(0x42b090,p+0x178,{0}); /* embedded handle: non-deleting flag only */
  call(0x47c780,p,{flag});
  if(!(flag&1))call(0x78fc50,0,{p}); /* source wrapper did not delete; don't read a freed allocation */
 }
 std::fprintf(stderr,"[atv-lifecycle] flags0/1/2 original-constructed base wrapper cases complete; V9 teardown uses natural cleanup only\n");
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
