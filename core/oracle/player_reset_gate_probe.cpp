#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {uint32_t frame;struct Saved{std::vector<std::pair<uint32_t,uint32_t>> words;void put(uint32_t a,uint32_t b){words.push_back({a,rx_rd32(a)});rx_wr32(a,b);}~Saved(){for(auto i=words.rbegin();i!=words.rend();++i)rx_wr32(i->first,i->second);}};}
RX_PORT(0x0084ed20,PlayerResetGateProbeHook,"thunk",0){
 if(!std::getenv("RX_PLAYER_RESET_GATE_PROBE")||++frame!=650)return reflex_vm::RUN_ORIGINAL;
 uint32_t g=rx_rd32(0xa946b8),p=0;if(g)for(unsigned i=0;i<16;i++){uint32_t q=g+0x7460+0x12a0*i;if(m.mem.mapped(q+0x94)&&rx_rd32(q+0x94)){p=q;break;}}
 if(!p){std::fprintf(stderr,"[reset-gate] no actual player; no coverage\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 uint32_t activity=rx_rd32(p+0x94),ui=rx_rd32(0xa93fcc);unsigned calls=0;
 auto call=[&](){rx_regs in{},out{};in.r[2]=p;rx_call_original_ex(0x54e470,&in,&out,0,nullptr);++calls;};
 {Saved save;save.put(p+0x94,0);save.put(0xaec048,0);call();save.put(0xaec048,1);call();}
 for(uint32_t state:{0u,1u,2u,3u})for(uint32_t sub:{0u,2u}) {
  Saved save;save.put(activity+0x21c,state);save.put(activity+0x238,sub);save.put(activity+0x23c,0);save.put(0xaec048,0);
  save.put(0x9c5430,0x40800000);save.put(0x9c5434,0x3f800000);
  for(uint32_t timer:{0u,0xbf800000u,0x3f800000u,0x40400000u,0x40800000u,0x7fc12345u,0x7f800000u}) {Saved t;t.put(p+0x1168,timer);call();}
 }
 if(ui&&m.mem.mapped(ui+0x298))for(uint32_t block:{0u,1u}){Saved save;save.put(activity+0x21c,2);save.put(activity+0x23c,8);save.put(ui+0x298,block);save.put(0xaec048,0);call();}
 std::fprintf(stderr,"[reset-gate] positive original calls=%u\n",calls);
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
