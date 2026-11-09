/* Development-only ATV contact helper coverage. All six simulation bodies and
 * twelve external three-float rolling filters are restored between cases.
 * Full S13 world/terrain queries remain natural-drive coverage: this probe does
 * not pretend restoring S reverses shared world/contact-cache side effects. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace { unsigned contacts_frame; uint32_t rd(uint32_t a){return rx_rd32(a);} void wr(uint32_t a,uint32_t b){rx_wr32(a,b);} }
RX_PORT(0x0084ed20,AtvContactsProbeHook,"thunk",0){
 if(!std::getenv("RX_ATV_CONTACTS_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const unsigned target=std::getenv("RX_ATV_CONTACTS_FRAME")?unsigned(std::atoi(std::getenv("RX_ATV_CONTACTS_FRAME"))):650;
 if(++contacts_frame!=target)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a)&&rd(a)==0x0093d054){s=a;break;}}
 if(!s){std::fprintf(stderr,"[atv-contacts] ERROR no live ATV\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto fpu=m.fpu;uint32_t flags=m.flags;
 std::vector<uint32_t> state(0x9bd0/4);for(unsigned i=0;i<state.size();i++)state[i]=rd(s+4*i);
 struct Filter{uint32_t p,bits[3];};std::vector<Filter> filters;
 for(unsigned i=0;i<12;i++){Filter f{};f.p=rd(s+0x9a14+4*i);if(!f.p||!m.mem.mapped(f.p+8)){std::fprintf(stderr,"[atv-contacts] ERROR invalid rolling filter%u\n",i);return reflex_vm::RUN_ORIGINAL;}for(unsigned j=0;j<3;j++)f.bits[j]=rd(f.p+4*j);filters.push_back(f);}
 // The callbacks must be the known sliding-rotor projection with both output
 // records inside the saved six-body simulation. Otherwise skip, never guess.
 bool callbacks_safe=true;
 for(unsigned i=0;i<4;i++){uint32_t n=rd(s+0xd78+4*i);if(!n||!m.mem.mapped(n+0x34)){callbacks_safe=false;continue;}uint32_t vt=rd(n);if(!vt||!m.mem.mapped(vt+0x18)||rd(vt+0x18)!=0x0085a8f0)callbacks_safe=false;for(unsigned o:{0x30u,0x34u}){uint32_t p=rd(n+o);if(p<s||p>s+0x9bd0-16)callbacks_safe=false;}}
 auto restore=[&](){for(unsigned i=0;i<state.size();i++)wr(s+4*i,state[i]);for(auto f:filters)for(unsigned j=0;j<3;j++)wr(f.p+4*j,f.bits[j]);};
 auto invoke=[&](uint32_t fn){rx_regs in{},out{};in.r[1]=s;in.r[2]=s;in.r[6]=s;const uint32_t dt=0x3c888889;rx_call_original_ex(fn,&in,&out,fn==0x00492320?1:0,fn==0x00492320?&dt:nullptr);};
 for(unsigned on=0;on<2;on++){wr(s+0x90,on);invoke(0x0048edc0);restore();}
 // Actual contact normals/points remain in place; masks cover every wheel
 // separately, both diagonals and all four, as well as the no-contact return.
 for(unsigned mask:{0u,1u,2u,4u,8u,5u,10u,15u}){
  for(unsigned i=0;i<4;i++)wr(s+0x9140+0x150*i+0xfc,(mask>>i)&1);
  invoke(0x00492ff0);restore();
 }
 unsigned rolling_cases=0;
 if(callbacks_safe){
  for(unsigned mode=0;mode<7;mode++){
   wr(s+0x90,mode==0);wr(s+0x2de0,mode==1);wr(s+0x2de4,mode==1?2:0);
   for(unsigned i=0;i<4;i++){uint32_t w=s+0x9140+0x150*i;wr(w+0x100,mode>=3?1:0);wr(w+0xc0,mode==3?0:mode==4?(i==0):mode==5?(i==2):1);}
   invoke(0x00492320);restore();rolling_cases++;
  }
 }else std::fprintf(stderr,"[atv-contacts] rolling SKIPPED: callback output closure was not established\n");
 restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.fpu=fpu;m.flags=flags;
 std::fprintf(stderr,"[atv-contacts] complete: rest-pin2 position8 rolling%u; six-body S and12 external filters restored; no synthetic S13 world query\n",rolling_cases);
 return reflex_vm::RUN_ORIGINAL;
}
