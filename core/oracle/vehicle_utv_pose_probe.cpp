/* Isolated restored-state UTV pose stimuli on an actual loaded UTV world.
 * REFLEX_PORT=rx_UtvPose REFLEX_PORT_VERIFY=1 RX_UTV_POSE_PROBE=1
 * RX_UTV_POSE_FRAME=20. 0084ed20 is an instrumentation thunk, never verified.
 * No manufactured node hierarchy or asset state; every stimulus restores all
 * vehicle/simulation/model data touched by V60 and its node setter callees. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;
uint32_t rd(uint32_t a){return rx_rd32(a);}
void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
struct Region {
 uint32_t a;std::vector<uint32_t> words;
 Region(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)words.push_back(rd(p+i));}
 void restore()const{for(size_t i=0;i<words.size();i++)wr(a+uint32_t(i*4),words[i]);}
 ~Region(){restore();}
};
void add_height(uint32_t a,float delta){uint32_t bits=rd(a);float f;std::memcpy(&f,&bits,4);f+=delta;std::memcpy(&bits,&f,4);wr(a,bits);}
}
RX_PORT(0x0084ed20,UtvPoseProbe,"thunk",0){
 if(!std::getenv("RX_UTV_POSE_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const uint32_t at=std::getenv("RX_UTV_POSE_FRAME")?uint32_t(std::atoi(std::getenv("RX_UTV_POSE_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;
  uint32_t a=rd(p+0x1ac);if(!a||!m.mem.mapped(a+0x1540)||rd(a)!=0x0093d4d4)continue;
  uint32_t b=rd(a+0x1540);
  if(b&&m.mem.mapped(b)&&rd(b)==0x0093d43c&&rd(a+0x234)==b){v=a;s=b;break;}
 }
 if(!v){std::fprintf(stderr,"[utv-pose] ERROR no live UTV vehicle/simulation\n");return reflex_vm::RUN_ORIGINAL;}
 /* The UTV constructors allocate V=15a0 and S=8850 (not truck's 8930).
  * V60 writes S+8750/+8760. Node setters can evaluate parents, invalidate whole
  * subtrees and change the hierarchy dirty flag, so save whole actual sets and
  * both matrix tables, including the root ModelNodeRef's set. */
 std::set<uint32_t> sets;
 auto add_node=[&](uint32_t node){if(!node||!m.mem.mapped(node))return false;uint32_t set=rd(node);if(!set||!m.mem.mapped(set+0x34))return false;sets.insert(set);return true;};
 bool valid=true;
 uint32_t model=rd(v+0x1328);
 valid=model&&m.mem.mapped(model+0x10)&&add_node(rd(model+0x10));
 for(uint32_t off=0x1360;off<=0x13c0;off+=4)valid=add_node(rd(v+off))&&valid;
 for(uint32_t set:sets){
  uint32_t n=rd(set+0xc);
  if(!n||n>256)valid=false;
  else for(uint32_t off:{8u,0x18u,0x1cu}){
   uint32_t base=rd(set+off),size=n*(off==8?16:64);
   if(!base||!m.mem.mapped(base)||!m.mem.mapped(base+size-1))valid=false;
  }
 }
 if(!valid){std::fprintf(stderr,"[utv-pose] ERROR invalid live model set; stimuli skipped\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 {
  Region vehicle(v,0x15a0),simulation(s,0x8850);
  std::vector<Region*> state;
  for(uint32_t set:sets){uint32_t n=rd(set+0xc);state.push_back(new Region(set,0x38));state.push_back(new Region(rd(set+8),16*n));state.push_back(new Region(rd(set+0x18),64*n));state.push_back(new Region(rd(set+0x1c),64*n));}
  /* Preserve the actual caller stack too: the unsupported visual modes read
   * an uninitialized local, and no probe should change later stale-stack reads. */
  if(regs[4]>=0x4000&&m.mem.mapped(regs[4]-0x4000)&&m.mem.mapped(regs[4]-1))state.push_back(new Region(regs[4]-0x4000,0x4000));
  auto restore=[&](){vehicle.restore();simulation.restore();for(auto p:state)p->restore();};
  unsigned calls=0;
  auto invoke=[&](){rx_regs in{},out{};in.r[1]=v;in.r[3]=regs[3];in.r[7]=regs[7];uint32_t args[]={0x3c888889};rx_call_original_ex(0x004c1fa0,&in,&out,1,args);++calls;restore();};
  invoke();
  for(uint32_t mode:{0u,1u}){
   wr(s+0x55f0,mode);invoke();
   for(uint32_t steer:{0x3e05a8a8u,0xbe05a8a8u}){
    wr(s+0x55f0,mode);wr(s+0x5630,0);wr(s+0x5634,steer);wr(s+0x5638,0);wr(s+0x563c,0x3f7dcf55);invoke();
   }
   for(float dy:{-.10f,-.05f,.05f,.10f}){
    wr(s+0x55f0,mode);
    add_height(s+0x224,dy);add_height(s+0x2c4,-dy);
    add_height(s+0x364,-dy);add_height(s+0x404,dy);invoke();
   }
  }
  /* RX_UTV_POSE_BEYOND=1 (w-online5): a rear wheel moved past its arm's reach (|dy| > the 1.8 ft arm), as a remote copy's wheel body
   * after a hard hit: the original's sqrtps of the negative span gives NaN arm / shock nodes; the port gives the simulation's own
   * out-of-reach span (004d4750's 0.1), so these calls are expected to differ exactly in the NaN lanes */
  if(std::getenv("RX_UTV_POSE_BEYOND"))for(float dy:{-2.5f,2.5f}){wr(s+0x55f0,0);add_height(s+0x404,dy);invoke();}
  for(auto p:state)delete p;
  std::fprintf(stderr,"[utv-pose] complete %u restored stimuli V=%08x S=%08x sets=%zu; check positive rx_UtvPose Verify counts\n",calls,v,s,sets.size());
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
