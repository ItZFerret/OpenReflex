/* Restored-state stimuli on an actual garage-selected SuperBuggy world.
 * REFLEX_PORT=rx_BuggyPose REFLEX_PORT_VERIFY=1 RX_BUGGY_POSE_PROBE=1
 * RX_BUGGY_POSE_FRAME=20 (default). The 0084ed20 hook must be group thunk.
 * Development only: this file is never linked into the product. */
#include "oracle.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <set>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;
uint32_t rd(uint32_t a){return rx_rd32(a);}
void wr(uint32_t a,uint32_t x){rx_wr32(a,x);}
float rf(uint32_t a){uint32_t x=rd(a);float f;std::memcpy(&f,&x,4);return f;}
void wf(uint32_t a,float f){uint32_t x;std::memcpy(&x,&f,4);wr(a,x);}
struct Region {
 uint32_t address;std::vector<uint32_t> words;
 Region(uint32_t a,uint32_t bytes):address(a){for(uint32_t i=0;i<bytes;i+=4)words.push_back(rd(a+i));}
 void restore()const{for(size_t i=0;i<words.size();i++)wr(address+uint32_t(4*i),words[i]);}
 ~Region(){restore();}
};
}
RX_PORT(0x0084ed20,BuggyPoseProbe,"thunk",0){
 if(!std::getenv("RX_BUGGY_POSE_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const uint32_t at=std::getenv("RX_BUGGY_POSE_FRAME")?uint32_t(std::atoi(std::getenv("RX_BUGGY_POSE_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,s=0,g=rd(0x00a946b8);
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t player=g+0x7460+0x12a0*i;if(!m.mem.mapped(player+0x1ac))continue;
  uint32_t vehicle=rd(player+0x1ac);if(!vehicle||!m.mem.mapped(vehicle+0x15c0))continue;
  uint32_t sim=rd(vehicle+0x234);
  if(sim&&m.mem.mapped(sim+0x883f)&&rd(sim)==0x0093d24c&&rd(vehicle+0x15c0)==sim){v=vehicle;s=sim;break;}
 }
 if(!v){std::fprintf(stderr,"[buggy-pose] ERROR no live SuperBuggy vehicle/simulation\n");return reflex_vm::RUN_ORIGINAL;}
 auto mapped=[&](uint32_t p,uint32_t n){
  if(!p||!n||p+n<p)return false;
  for(uint32_t i=0;i<n;i+=0x1000)if(!m.mem.mapped(p+i))return false;
  return m.mem.mapped(p+n-1);
 };
 /* Snapshot whole node sets: invalidation walks parents and siblings, so saving
  * only the referenced nodes would leave flags and world matrices changed. */
 std::set<uint32_t> sets;
 auto add_node=[&](uint32_t node){if(!mapped(node,16))return false;uint32_t set=rd(node);if(!mapped(set,0x38))return false;sets.insert(set);return true;};
 bool valid=mapped(v,0x15c4)&&mapped(s,0x8840);
 for(uint32_t off=0x1360;valid&&off<=0x13d4;off+=4)valid=add_node(rd(v+off));
 uint32_t modelref=rd(v+0x1328);
 valid=valid&&mapped(modelref+0x10,4)&&add_node(rd(modelref+0x10));
 for(uint32_t set:sets){
  uint32_t count=rd(set+0xc);
  valid=valid&&count&&count<=256&&mapped(rd(set+8),16*count)&&mapped(rd(set+0x18),64*count)&&mapped(rd(set+0x1c),64*count);
 }
 if(!valid){std::fprintf(stderr,"[buggy-pose] ERROR invalid model snapshot bounds V=%08x S=%08x\n",v,s);return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 {
  Region vehicle(v,0x15c4),sim(s,0x8840);std::vector<std::unique_ptr<Region>> model;
  for(uint32_t set:sets){uint32_t count=rd(set+0xc);model.emplace_back(new Region(set,0x38));model.emplace_back(new Region(rd(set+8),16*count));model.emplace_back(new Region(rd(set+0x18),64*count));model.emplace_back(new Region(rd(set+0x1c),64*count));}
  auto restore=[&](){vehicle.restore();sim.restore();for(auto& p:model)p->restore();};
  unsigned calls=0;
  auto invoke=[&](float dt){
   uint32_t arg;std::memcpy(&arg,&dt,4);rx_regs in{},out{};
   in.r[1]=v;in.r[3]=regs[3];uint32_t args[]={arg};
   rx_call_original_ex(0x004a4270,&in,&out,1,args);++calls;restore();
  };
  invoke(1.0f/60.0f);
  /* Independent front and rear heights retain the live X/Z and quaternion,
   * keeping IK within a modest neighbourhood of the authored suspension. */
  for(uint32_t off:{0x220u,0x2c0u,0x360u,0x400u})for(float height:{-.25f,-.08f,.08f,.25f}){
   wf(s+off+4,rf(s+off+4)+height);invoke(1.0f/60.0f);
  }
  /* Rear steering quaternion and opposite corner articulation exercise both
   * independently solved linkages without editing any model/node pointers. */
  for(float steering:{-.06f,0.0f,.06f}){
   wf(s+0x5620,0);wf(s+0x5624,std::sin(steering));wf(s+0x5628,0);wf(s+0x562c,std::cos(steering));
   wf(s+0x224,rf(s+0x224)+.12f);wf(s+0x2c4,rf(s+0x2c4)-.12f);invoke(1.0f/60.0f);
  }
  /* Engine clamps: stationary/moving speed, rising/falling filtered RPM,
   * and dt below/above the lag time. Each starts from restored authored data. */
  for(float dt:{1.0f/240.0f,1.0f/60.0f,.1f})for(float speed:{0.0f,5.0f,10.0f,12.0f})for(float delta:{-100.0f,100.0f}){
   wf(s+0x100,speed);wf(v+0x13ec,rf(v+0x13d8)+delta);wf(v+0x13f0,.35f);invoke(dt);
  }
  std::fprintf(stderr,"[buggy-pose] complete %u restored stimuli V=%08x S=%08x nodeSets=%zu; require positive Verify counts\n",calls,v,s,sets.size());
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
