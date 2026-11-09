/* Fresh original-created S objects only; the active world's S is never destroyed. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using namespace reflex_port;
namespace {uint32_t frame;}
RX_PORT(0x0084ed20,AtvDestroyProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_DESTROY_PROBE")||++frame!=650)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags,callback=rx_rd32(0xd92cd4);auto fpu=m.fpu;
 auto call=[&](uint32_t fn,uint32_t ecx,std::initializer_list<uint32_t>a){rx_regs in{},out{};in.r[1]=ecx;rx_call_original_ex(fn,&in,&out,int(a.size()),a.begin());return out.r[0];};
 unsigned completed=0,canaryErrors=0;
 for(uint32_t flag:{0u,1u,2u}){
  uint32_t p=call(0x78fbe0,0,{0x9bd8});if(!p){std::fprintf(stderr,"[atv-destroy] ERROR original allocation failed\n");break;}
  rx_wr32(p+0x9bd0,0x1badb002);rx_wr32(p+0x9bd4,0xc0defeed);
  call(0x48bde0,0,{p}); /* original RavenSimulationATV constructor, actual loaded tuning resources */
  if(rx_rd32(p+0x9bd0)!=0x1badb002||rx_rd32(p+0x9bd4)!=0xc0defeed){canaryErrors++;std::fprintf(stderr,"[atv-destroy] ERROR constructor guard changed\n");}
  call(0x48c730,p,{flag});
  if(!(flag&1)){
   if(rx_rd32(p+0x9bd0)!=0x1badb002||rx_rd32(p+0x9bd4)!=0xc0defeed){canaryErrors++;std::fprintf(stderr,"[atv-destroy] ERROR destructor guard changed\n");}
   call(0x78fc50,0,{p});
  } /* flags1 frees original allocation; never inspect that freed storage. */
  rx_wr32(0xd92cd4,callback);completed++;
 }
 std::fprintf(stderr,"[atv-destroy] original constructor/delete flags0/1/2 cases=%u,canaryErrors=%u; active S untouched, tuning callback restored\n",completed,canaryErrors);
 rx_wr32(0xd92cd4,callback);std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
