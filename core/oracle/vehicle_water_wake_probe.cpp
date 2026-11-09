/* Actual original wake queues only. No renderer factory or fabricated wake. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}void wf(uint32_t a,float f){uint32_t b;std::memcpy(&b,&f,4);wr(a,b);}struct Save{uint32_t a;std::vector<uint32_t>b;Save(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)b.push_back(rd(p+i));}void restore(){for(size_t i=0;i<b.size();i++)wr(a+4*uint32_t(i),b[i]);}~Save(){restore();}};}
RX_PORT(0x0084ed20,WaterWakeProbeHook,"thunk",0){
 if(!std::getenv("RX_WATER_WAKE_PROBE")||++frame!=650)return reflex_vm::RUN_ORIGINAL;
 const uint32_t wake=rd(0xa95790),g=rd(0xa95844);uint32_t v=0,game=rd(0xa946b8);
 if(game)for(unsigned i=0;i<16;i++){uint32_t p=game+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t x=rd(p+0x1ac);if(x&&m.mem.mapped(x)&&rd(x)==0x93d0ec){v=x;break;}}
 if(!wake||!m.mem.mapped(wake+0x30ec)||!g||!v){std::fprintf(stderr,"[water-wake] no actual wake/context/ATV; wake branches NOT COVERED\n");return reflex_vm::RUN_ORIGINAL;}
 const uint32_t context=rd(g+0x1c+4*rd(g+4)),qualityAt=context+0x19d0;
 if(!context||!m.mem.mapped(qualityAt)){std::fprintf(stderr,"[water-wake] missing actual quality context; NOT COVERED\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 {
  Save W(wake,0x30f0),V(v,6000),Q(qualityAt,4),single(0xd72aac,4);
  auto restore=[&](){W.restore();V.restore();Q.restore();single.restore();};
  auto call=[&](uint32_t fn,uint32_t eax,uint32_t ecx,uint32_t esi,uint32_t edi,float f,std::vector<uint32_t>a){rx_regs in{},out{};in.r[0]=eax;in.r[1]=ecx;in.r[6]=esi;in.r[7]=edi;in.xmm[0].v[0]=f;rx_call_original_ex(fn,&in,&out,int(a.size()),a.data());};
  auto clear=[&](){for(uint32_t off:{0x200u,0x388u,0x390u,0x394u,0x1a50u,0x30d4u})wr(wake+off,0);};
  const uint32_t a=v+0x1680,b=v+0x1690,c=v+0x16a0,d=v+0x16b0,velocity=v+0x16c0;
  auto corners=[&](){const float x=rx_bf(rd(v+0xc0)),y=rx_bf(rd(v+0xc4)),z=rx_bf(rd(v+0xc8));unsigned i=0;for(uint32_t p:{a,b,c,d}){wf(p,x+(i&1?1:-1));wf(p+4,y);wf(p+8,z+(i&2?1:-1));wf(p+12,1);i++;}};
  for(uint32_t q:{1u,2u})for(unsigned speed=0;speed<3;speed++){
   restore();clear();wr(qualityAt,q);corners();wf(velocity,speed==2?1:0);wf(velocity+4,0);wf(velocity+8,speed?2:0);wf(velocity+12,0);
   call(0x7ec280,d,c,0,wake,0,{a,b,velocity,0x3f000000,0x3c888889});
   std::fprintf(stderr,"[water-wake] quad quality%u velocity%u vertices=%u centers=%u\n",q,speed,rd(wake+0x1a50),rd(wake+0x200));
  }
  for(uint32_t q:{1u,2u})for(float size:{.5f,2.0f}){restore();clear();wr(qualityAt,q);corners();call(0x7ecd80,0,0,wake,0,size,{a,0x3f000000});}
  for(uint32_t q:{1u,2u}){restore();clear();wr(qualityAt,q);wr(wake+0x1a50,0x5a0);corners();call(0x7ec280,d,c,0,wake,0,{a,b,velocity,0x3f000000,0x3c888889});call(0x7ecd80,0,0,wake,0,1,{a,0x3f000000});}
  restore();clear();call(0x7f9510,wake+0x80,0,0,0,0,{0x80000000,0x7fc12345});
  for(uint32_t bits:{0u,0x80000000u,0x7f800000u,0x7fc12345u}){float f;std::memcpy(&f,&bits,4);call(0x7f9550,wake+0x3d0,0,0,0,f,{});}
  const bool finiteOnly=std::getenv("RX_WATER_WAKE_FINITE_ONLY")&&std::strcmp(std::getenv("RX_WATER_WAKE_FINITE_ONLY"),"1")==0;
  if(finiteOnly)std::fprintf(stderr,"[water-wake] FINITE_ONLY: skipping 2 signaling-NaN guarded-strength cases; NaN payload coverage NOT VERIFIED (default regression remains enabled)\n");
  for(uint32_t q:{1u,2u})for(unsigned mode=0;mode<(finiteOnly?2u:3u);mode++){
   restore();clear();wr(qualityAt,q);corners();wr(0xd72aac,mode?wake:0);
   call(0x7efed0,0xd72aac,a,0,0,.75f,{mode==2?0x7fa12345u:0x3f800000u});
  }
  restore();std::fprintf(stderr,"[water-wake] complete 8 quad,6 splat,1 center,4 float,%u guarded-entry cases; actual wake/quality/singleton/ATV restored\n",finiteOnly?4u:6u);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
