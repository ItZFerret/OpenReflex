/* Developer-only, real original ATV world; run separately from startup probe.
 * REFLEX_PORT=rx_AtvPhysics,rx_AtvPhysicsProbeHook REFLEX_PORT_VERIFY=1
 * RX_ATV_PHYSICS_PROBE=1 RX_ATV_PHYSICS_FRAME=650. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace{uint32_t frame;}
RX_PORT(0x0084ed20,AtvPhysicsProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_PHYSICS_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const uint32_t target=std::getenv("RX_ATV_PHYSICS_FRAME")?uint32_t(std::atoi(std::getenv("RX_ATV_PHYSICS_FRAME"))):650;
 if(++frame!=target)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rx_rd32(0xa946b8);
 if(g)for(uint32_t i=0;i<16;i++){
  uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;
  uint32_t v=rx_rd32(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;
  uint32_t a=rx_rd32(v+0x234);if(a&&m.mem.mapped(a)&&rx_rd32(a)==0x0093d054){s=a;break;}
 }
 if(!s){std::fprintf(stderr,"[atv-physics] ERROR no live ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 std::vector<uint32_t> saved;for(uint32_t i=0;i<0x9bd0;i+=4)saved.push_back(rx_rd32(s+i));
 struct Range{uint32_t p,v[4];};std::vector<Range> ranges;
 for(uint32_t i=0;i<12;i++){Range r{};r.p=rx_rd32(s+0x9a14+4*i);for(uint32_t j=0;j<4;j++)r.v[j]=rx_rd32(r.p+4*j);ranges.push_back(r);}
 auto restore=[&](){for(size_t i=0;i<saved.size();i++)rx_wr32(s+uint32_t(i*4),saved[i]);for(auto r:ranges)for(uint32_t j=0;j<4;j++)rx_wr32(r.p+4*j,r.v[j]);};
 auto invoke=[&](uint32_t fn,int n,const uint32_t* args){rx_regs in{},out{};in.r[1]=s;in.r[2]=s;in.r[7]=s;in.r[6]=s;rx_call_original_ex(fn,&in,&out,n,args);};
 for(uint32_t value:{0u,0x80000000u,0x3f000000u,0x7fa12345u}){rx_wr32(s+0x5898,value);invoke(0x0047c460,0,nullptr);restore();}
 const uint32_t none=0xffffffffu,controller=0;
 invoke(0x0049ee00,1,&none);restore();invoke(0x0049ee00,1,&controller);restore();
 for(uint32_t sign:{0u,0x3f000000u,0xbf000000u}){
  for(uint32_t i=0;i<48;i+=4)rx_wr32(s+0x2df0+i,0);
  rx_wr32(s+0x2e14,sign);rx_wr32(s+0x2e1c,0x3f800000);
  uint32_t p=s+0x2df0;invoke(0x004a0e60,1,&p);restore();
 }
 const uint32_t dt=0x3c888889;
 invoke(0x00496310,1,&dt);restore();
 rx_wr32(s+0x99a8,0);rx_wr32(s+0x9834,0);invoke(0x00496310,1,&dt);restore();
 for(uint32_t airborne:{0u,1u}){
  rx_wr32(s+0x99a8,0);rx_wr32(s+0x9834,1);rx_wr32(s+0x3c,1);
  for(uint32_t o:{0x9970u,0x996cu,0x9958u,0x9954u,0x9964u,0x9984u,0x9980u})rx_wr32(s+o,0);
  rx_wr32(s+0x5968,0x3f000000);rx_wr32(s+0x596c,airborne?0xbf000000:0xbf800000);
  invoke(0x00496310,1,&dt);restore();
 }
 for(uint32_t value:{0u,1u,0xffffffffu}){rx_wr32(s+0x90e0,value);invoke(0x0049d7c0,0,nullptr);restore();}
 for(uint32_t direction:{0u,1u,2u}){
  // Isolate external reset contact queues while retaining real six-body pose and world rays.
  rx_wr32(s+0x2ebc,0);rx_wr32(s+0x1318,0);rx_wr32(s+0x1310,rx_rd32(s+0x130c));
  for(uint32_t j=0;j<4;j++)rx_wr32(s+0x2e20+4*j,0);
  rx_wr32(s+0x2e20+(direction==1?0:8),direction==2?0xbf800000:0x3f800000);
  const uint32_t args[]={s+0xf0,s+0x2e20,s+0x2df0,s+0x2e00,s+0x2e10};
  invoke(0x0048f9e0,5,args);restore();
 }
 for(uint32_t dtbits:{0u,0x3c888889u,0x3e800000u}){
  uint32_t args[]={s,dtbits};invoke(0x004a17b0,2,args);restore();
  rx_wr32(s+0x99a0,0x3e800000);rx_wr32(s+0x99a4,0xbe000000);
  invoke(0x004931a0,1,&dtbits);restore();
 }
 for(uint32_t value:{0u,0x3f800000u,0xbf000000u}){
  for(uint32_t j=0;j<4;j++)rx_wr32(s+0x5888+4*j,value);
  invoke(0x00498db0,1,&s);restore();
 }
 invoke(0x0048d430,1,&s);restore();
 rx_wr32(s+0x740,0x41a80000);rx_wr32(s+0x7e4,0x469c4000);invoke(0x0048d430,1,&s);restore();
 invoke(0x00493770,0,nullptr);restore();
 for(uint32_t flag:{0x94e0u,0x9630u,0x9240u,0x9390u})rx_wr32(s+flag,1);
 invoke(0x00493770,0,nullptr);restore();
 for(uint32_t value:{0u,0x80000000u,0x3f000000u,0xbf000000u,0x3727c5acu,0x7fc12345u}){
  rx_wr32(s+0x5898,value);invoke(0x00499220,0,nullptr);restore();
 }
 // Each real wheel's live contact and airborne decay; then rear slide/reflex branches.
 for(uint32_t i=0;i<4;i++){
  uint32_t w=s+0x9140+0x150*i,args[]={dt,w};
  invoke(0x0049a340,2,args);restore();
  rx_wr32(w+0x100,0);invoke(0x0049a340,2,args);restore();
 }
 for(uint32_t i:{2u,3u})for(uint32_t reflex:{0x3f000000u,0xbf000000u}){
  uint32_t w=s+0x9140+0x150*i,body=2+i,args[]={dt,w};
  rx_wr32(w+0x100,1);rx_wr32(s+0x59e4,reflex);rx_wr32(s+0x28,0x3f000000);
  rx_wr32(s+0x2c,0x3f000000);rx_wr32(s+0x34,0x3e800000);
  rx_wr32(s+0x5968,0x3f000000);rx_wr32(s+0x5960,0xbe800000);
  for(uint32_t j=0;j<4;j++){
   float lateral=rx_bf(rx_rd32(w+0x120+4*j));
   rx_wr32(s+0x100+4*j,0);rx_wr32(s+0x740+4*j,0);
   float normal=rx_bf(rx_rd32(w+0x130+4*j)),rolling=rx_bf(rx_rd32(w+0x140+4*j));
   rx_wr32(s+0x100+0xa0*body+4*j,rx_fb((lateral*-5.0f+normal*-2.0f)+rolling*-20.0f));
   rx_wr32(s+0x740+0xa0*body+4*j,j==0?0x41000000u:0u);
  }
  invoke(0x0049a340,2,args);restore();
 }
 for(uint32_t timestep:{0u,0x3c888889u}){invoke(0x004a0ab0,1,&timestep);restore();}
 invoke(0x0049ee30,1,&dt);restore();
 rx_wr32(s+0x2de0,1);rx_wr32(s+0x2e70,0x3e800000);invoke(0x0049ee30,1,&dt);restore();
 for(uint32_t sign:{0u,0x80000000u}){
  rx_wr32(s+0x2de0,0);rx_wr32(s+0x99a8,0);
  rx_wr32(s+0x2c,0x3f000000^sign);rx_wr32(s+0x59a8,0x3f000000^sign);
  rx_wr32(s+0x5968,0xbf000000^sign);
  invoke(0x0049ee30,1,&dt);restore();
 }
 // Force the S9 rising-velocity terrain lookahead, preserving its static registry flags.
 {uint32_t oldFlags=rx_rd32(0x9e30e0),oldCount=rx_rd32(0x9e30dc);
  rx_wr32(s+0x2374,0);rx_wr32(s+0x104,0x41a00000);
  rx_wr32(s+0x34,0x3f400000);rx_wr32(s+0x2c,0x3f400000);
  invoke(0x0049ee30,1,&dt);restore();
  rx_wr32(0x9e30e0,oldFlags);rx_wr32(0x9e30dc,oldCount);
 }
 for(uint32_t pinned:{0u,1u})for(uint32_t water:{0u,1u}){
  rx_wr32(s+0x9838,pinned);rx_wr32(s+0x60,water);rx_wr32(s+0x5c,water);
  invoke(0x00493630,1,&dt);restore();
 }
 for(uint32_t mode:{0u,2u}){rx_wr32(s+0x2de4,mode);invoke(0x0048e8a0,0,nullptr);restore();}
 for(uint32_t mode:{0u,1u,2u,3u,4u}){
  rx_wr32(s+0x2de4,mode);rx_wr32(s+0x2ebc,0);rx_wr32(s+0x1318,0);rx_wr32(s+0x1310,rx_rd32(s+0x130c));
  invoke(0x004a1250,0,nullptr);restore();
 }
 for(uint32_t value:{0u,0xffffffffu}){rx_wr32(s+0x90,value);invoke(0x00432190,0,nullptr);restore();}
 // Real extracted tuning curves and body state, with isolated clutch/contact branches.
 invoke(0x004985e0,1,&dt);restore();
 for(uint32_t mode:{0u,1u,2u})for(uint32_t contact:{0u,1u,2u,3u}){
  rx_wr32(s+0x597c,mode);rx_wr32(s+0x5880,0x3f000000);
  rx_wr32(s+0x5884,contact==3?0x3e800000:0);
  rx_wr32(s+0x96e8,contact==0?0:rx_rd32(s+0x4e4c));
  rx_wr32(s+0x94e0,contact&1);rx_wr32(s+0x9630,(contact>>1)&1);
  invoke(0x004985e0,1,&dt);restore();
 }
 restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 std::fprintf(stderr,"[atv-physics] complete: steering4/controller2/relativepose3/filters4/debug3/ground3/spring6/torque3/clamp2/resistance2/engine13/ackermann6/traction12/rideroff2/control5/pin4/relativebegin2/apply5/sharedflag2, real S restored\n");
 return reflex_vm::RUN_ORIGINAL;
}
