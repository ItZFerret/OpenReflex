/* Shared restored-state poststep stimuli on actual UTV/Buggy/ProLite/Pro2
 * worlds. RX_POSTSTEP_PROBE=1 RX_POSTSTEP_FRAME=20 (default).
 * Select rx_UtvPoststep,rx_BuggyPoststep,rx_TruckPoststep and
 * rx_VehiclePoststepHelpers and rx_FourwheelType in Verify mode. All calls enter registered oracle
 * dispatch through rx_call_original_ex; no direct core invocation hides counts. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;
uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t x){rx_wr32(a,x);}
struct Region{uint32_t a;std::vector<uint32_t> words;Region(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)words.push_back(rd(p+i));}void restore()const{for(size_t i=0;i<words.size();i++)wr(a+uint32_t(i*4),words[i]);}~Region(){restore();}};
struct Layout{uint32_t vtable,simtable,voff,vsize,ssize,wrapper,helper,latch,contacts,wheels;bool esi;};
const Layout layouts[]={
 {0x0093d4d4,0x0093d43c,0x1540,0x15a0,0x8850,0x004c6e80,0x004d8160,0x8020,0x1318,0x8080,true},
 {0x0093d2e4,0x0093d24c,0x15c0,0x1620,0x8840,0x004ac520,0x004bd860,0x8010,0x1318,0x8070,true},
 {0x0093cee4,0x0093c93c,0x15d0,0x1630,0x8930,0x00473450,0x00459fa0,0x8020,0x1318,0x8080,false}
};
}
RX_PORT(0x0084ed20,VehiclePoststepProbe,"thunk",0){
 if(!std::getenv("RX_POSTSTEP_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_POSTSTEP_FRAME")?uint32_t(std::atoi(std::getenv("RX_POSTSTEP_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=0x1000)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t stack=m.r[4],g=rd(0x00a946b8);
 if(!g||stack<0x4000||!mapped(stack-0x4000,0x4000)){std::fprintf(stderr,"[poststep] ERROR invalid live game/stack\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 unsigned vehicles=0,total=0;
 for(const Layout& l:layouts){
  uint32_t v=0,s=0;
  for(unsigned i=0;i<16;i++){
   uint32_t player=g+0x7460+0x12a0*i;if(!mapped(player+0x1ac,4))continue;
   uint32_t candidate=rd(player+0x1ac);if(!mapped(candidate,l.vsize)||rd(candidate)!=l.vtable)continue;
   uint32_t sim=rd(candidate+l.voff);
   if(mapped(sim,l.ssize)&&rd(sim)==l.simtable&&rd(candidate+0x234)==sim){v=candidate;s=sim;break;}
  }
  if(!v)continue;
  uint32_t node_count=rd(s+0xec);
  if(node_count>256){std::fprintf(stderr,"[poststep] ERROR invalid node count %u V=%08x\n",node_count,v);continue;}
  std::set<uint32_t> nodes;bool valid=true;
  for(uint32_t i=0;i<node_count;i++){
   uint32_t node=rd(s+0xd70+4*i); /* mechanism S+d0, node array M+ca0 */
   if(!mapped(node,16)||!mapped(rd(node)+0x10,4)){valid=false;break;}
   uint32_t callback=rd(rd(node)+0x10);
   /* These are the actual mechanism-node classes. Source/disassembly audit:
    * vf4 callbacks and859b30 draw math write stack only (fixed-body helper
    * calls also receive stack matrices). No model/rider/global state escapes.
    * Unknown callback classes are refused, rather than assumed read-only. */
   if(callback!=0x00859e40&&callback!=0x0085b180&&callback!=0x0085bed0){std::fprintf(stderr,"[poststep] ERROR unaudited vf4 %08x node=%08x\n",callback,node);valid=false;break;}
   nodes.insert(node);
  }
  if(!valid)continue;
  {
   Region vehicle(v,l.vsize),simulation(s,l.ssize),counter(0x00ad3ed4,4),scratch(stack-0x4000,0x4000);
   std::vector<std::unique_ptr<Region>> node_state;
   for(uint32_t node:nodes)node_state.emplace_back(new Region(node,16));
   auto restore=[&](){vehicle.restore();simulation.restore();counter.restore();scratch.restore();for(auto& n:node_state)n->restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;};
   unsigned calls=0;
   auto invoke=[&](uint32_t fn,uint32_t ecx,uint32_t edx,uint32_t esi,int nargs,const uint32_t* args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[1]=ecx;in.r[2]=edx;in.r[6]=esi;rx_call_original_ex(fn,&in,&out,nargs,args);++calls;restore();};
   /* vf55 is selected from the actual class vtable, not a guessed common
    * simulation offset. Raw setter values must survive unchanged. */
   uint32_t setter=rd(rd(v)+55u*4u),current_type=rd(s+4);
   uint32_t expected_setter=l.voff==0x1540?0x004c0a00:l.voff==0x15c0?0x004a2d00:0x0046acf0;
   if(setter!=expected_setter){std::fprintf(stderr,"[poststep] ERROR unexpected vf55 %08x V=%08x\n",setter,v);}
   else for(uint32_t type:{0u,1u,current_type,0xffffffffu}){
    uint32_t args[]={type};invoke(setter,v,regs[2],regs[6],1,args);
   }
   /* Phase gate, including signed-remainder branches. Real mechanism count and
    * all real node pointers remain intact for each phase that takes the call. */
   for(int32_t phase:{0,1,2,-1,-2,-3,3,4}){
    wr(0x00ad3ed4,uint32_t(phase));wr(s+l.latch,1);invoke(l.wrapper,v,regs[2],regs[6],0,nullptr);
   }
   std::set<uint32_t> counts={0,node_count};if(node_count)counts.insert(1);
   /* Direct registered simulation helper calls guarantee independent compare
    * counts even when the wrapper itself calls its C helper directly. */
   for(uint32_t latch:{0u,1u})for(uint32_t contacts:{0u,1u})for(uint32_t count:counts){
    wr(s+l.latch,latch);wr(s+l.contacts,contacts);wr(s+0xec,count);
    invoke(l.helper,l.esi?regs[1]:s,regs[2],l.esi?s:regs[6],0,nullptr);
   }
   for(uint32_t count:counts){wr(s+0xec,count);uint32_t args[]={s+0xd0};invoke(0x0085c6a0,regs[1],regs[2],regs[6],1,args);}
   /* Direct callbacks guarantee positive comparison counts for the original
    * actual node classes, independent of nested C calls in the mechanism loop.
    * +4 is the original debug enable gate; all authored body pointers remain.
    * Node headers are in node_state, so every disabled/enabled stimulus restores
    * the real gate before the next callback or common-helper invocation. */
   for(uint32_t node:nodes){
    uint32_t callback=rd(rd(node)+0x10);
    for(uint32_t enabled:{0u,1u}){
     wr(node+4,enabled);invoke(callback,node,regs[2],regs[6],0,nullptr);
    }
    uint32_t args[]={node};invoke(0x00859b30,regs[1],regs[2],regs[6],1,args);
   }
   /* Contact-flag branches in both shared visual helpers, always using real
    * wheel records and valid authored body indices. No synthetic vtables. */
   for(unsigned i=0;i<4;i++)for(uint32_t contact:{0u,1u}){
    uint32_t wheel=s+l.wheels+0x150*i;
    wr(wheel+0x100,contact);uint32_t offset_args[]={wheel,(i&1)?0xbe000000u:0x3e000000u};
    invoke(0x004d7fa0,regs[1],s,regs[6],2,offset_args);
    wr(wheel+0x100,contact);uint32_t axis_args[]={wheel};invoke(0x00459de0,regs[1],s,regs[6],1,axis_args);
   }
   std::fprintf(stderr,"[poststep] complete %u restored calls V=%08x S=%08x vt=%08x actualnodes=%u; check all selected poststep Verify counts\n",calls,v,s,l.vtable,node_count);
   total+=calls;++vehicles;
  }
 }
 /* Supplemental actual MX fixed-body node: four-wheel mechanisms contain
  * main-body/rotor nodes, while the authored MX rider is a FixedBody node.
  * This is deliberately node-only: no four-wheel offsets/callbacks apply. */
 for(unsigned i=0;i<16;i++){
  uint32_t player=g+0x7460+0x12a0*i;if(!mapped(player+0x1ac,4))continue;
  uint32_t v=rd(player+0x1ac);if(!mapped(v+0x16d4,4))continue;
  uint32_t sim=rd(v+0x16d4);
  if(!mapped(sim,0x8fc0)||rd(sim)!=0x0093c8a4)continue;
  uint32_t count=rd(sim+0xec),fixed=0;
  if(count>256)continue;
  for(uint32_t n=0;n<count;n++){
   uint32_t node=rd(sim+0xd70+4*n);
   if(mapped(node,0x80)&&rd(node)==0x0093cbc8&&mapped(rd(node)+0x10,4)&&rd(rd(node)+0x10)==0x0085bed0){fixed=node;break;}
  }
  if(!fixed)continue;
  bool pointers=true;
  for(uint32_t off:{0x28u,0x2cu,0x30u,0x34u})pointers=pointers&&mapped(rd(fixed+off)+0x90,16);
  if(!pointers)continue;
  {
   Region simulation(sim,0x8fc0),node(fixed,0x80),scratch(stack-0x4000,0x4000);
   auto restore=[&](){simulation.restore();node.restore();scratch.restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;};
   auto invoke=[&](uint32_t fn,int nargs,const uint32_t* args){rx_regs in{},out{};for(unsigned r=0;r<8;r++)in.r[r]=regs[r];in.r[1]=fixed;rx_call_original_ex(fn,&in,&out,nargs,args);restore();++total;};
   for(uint32_t enabled:{0u,1u}){wr(fixed+4,enabled);invoke(0x0085bed0,0,nullptr);}
   uint32_t args[]={fixed};invoke(0x00859b30,1,args);
   ++vehicles;std::fprintf(stderr,"[poststep] MX supplemental complete 3 restored node calls V=%08x S=%08x fixed=%08x; check FixedBody/axes Verify counts\n",v,sim,fixed);
  }
  break;
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 if(!vehicles)std::fprintf(stderr,"[poststep] ERROR no audited live four-wheel/MX fixture\n");
 else std::fprintf(stderr,"[poststep] complete vehicles=%u calls=%u; memory/CPU restored\n",vehicles,total);
 return reflex_vm::RUN_ORIGINAL;
}
