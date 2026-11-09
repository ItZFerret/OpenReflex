/* Development-only contact stimuli on the real terrain; no product dependency. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
extern "C" {
#include "../game/world_veh.h"
}
using namespace reflex_port;
using rx_oracle::bits;
namespace {
uint32_t frame;
uint32_t alloc(uint32_t n){uint32_t a[]={n};uint32_t p=rx_call_original(0x0078fbe0,RX_CDECL,0,1,a);for(uint32_t i=0;i<n;i+=4)rx_wr32(p+i,0);return p;}
}
RX_PORT(0x0084ed20,SphereTerrainProbeHook,"thunk",0){
 if(!std::getenv("RX_SPHERE_TERRAIN_PROBE")||++frame!=5)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 const uint32_t terrain=rx_world_find_by_controller(0x009e30d4), p=alloc(16), q=alloc(16), sphere=alloc(64), shape=alloc(64), manifold=alloc(0x700);
 rx_wr32(q+12,0x3f800000);rx_wr32(sphere+8,2);rx_wr32(shape+8,0x200);rx_wr32(shape+0x20,0x009e30d4);
 const uint32_t args[]={p};rx_wr32(p,bits(0.0f));rx_wr32(p+8,bits(0.0f));
 if(!terrain||!rx_call_original(0x007c5950,RX_THISCALL,terrain,1,args)){std::fprintf(stderr,"[sphere-terrain] ERROR missing terrain origin\n");return reflex_vm::RUN_ORIGINAL;}
 const float height=rx_oracle::f32(rx_rd32(p+4));unsigned hit=0,miss=0;
 for(unsigned t=0;t<64;t++){
  for(unsigned i=0;i<0x700;i+=4)rx_wr32(manifold+i,0);
  const uint32_t count=t%7==0?32:t%3;rx_wr32(manifold,count);
  const float radius=t%8==0?0.0f:0.25f+float(t%5)*.6f;
  for(unsigned i=0;i<4;i++)rx_wr32(sphere+0x20+4*i,bits(radius+float(i)*.05f));
  rx_wr32(p+4,bits(height+(float(t%8)-3.0f)*.5f));
  const uint32_t flag=t&1;
  if(t<32){rx_regs in{},out{};in.r[1]=sphere;in.r[7]=p;const uint32_t a[]={manifold,shape,flag};rx_call_original_ex(0x00864ec0,&in,&out,3,a);}
  else {const uint32_t a[]={t&2?shape:sphere,q,p,t&2?sphere:shape,q,p,flag};rx_call_original(0x0085ddd0,RX_THISCALL,manifold,7,a);}
  if(rx_rd32(manifold+0x6a0))hit++;else miss++;
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 std::fprintf(stderr,"[sphere-terrain]64 cases:32 direct,32 dispatch both orders; hits=%u misses=%u\n",hit,miss);
 return reflex_vm::RUN_ORIGINAL;
}
