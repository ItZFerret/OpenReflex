/* Isolated restored-state TrophyTruck pose stimuli on original ProLite/Pro2.
 * REFLEX_PORT=rx_TruckPose,rx_TruckPoseProbe REFLEX_PORT_VERIFY=1
 * RX_TRUCK_POSE_PROBE=1 RX_TRUCK_POSE_FRAME=20. Development only. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
struct Region{uint32_t a;std::vector<uint32_t> v;Region(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)v.push_back(rd(p+i));}void restore()const{for(size_t i=0;i<v.size();i++)wr(a+uint32_t(i*4),v[i]);}~Region(){restore();}};
}
RX_PORT(0x0084ed20,TruckPoseProbe,"thunk",0){
 if(!std::getenv("RX_TRUCK_POSE_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_TRUCK_POSE_FRAME")?uint32_t(std::atoi(std::getenv("RX_TRUCK_POSE_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t v=0,s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t a=rd(p+0x1ac);if(!a||!m.mem.mapped(a+0x15d0))continue;uint32_t b=rd(a+0x234);if(b&&m.mem.mapped(b)&&rd(b)==0x0093c93c&&rd(a+0x15d0)==b){v=a;s=b;break;}}
 if(!v){std::fprintf(stderr,"[truck-pose] ERROR no live truck V/S reference\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto flags=m.flags;auto fpu=m.fpu;
 {
  Region vehicle(v,0x15d4),sim(s,0x8930);std::vector<Region*> model;std::set<uint32_t> sets;
  for(uint32_t off=0x1360;off<=0x13ec;off+=4)sets.insert(rd(rd(v+off)));
  for(auto set:sets){uint32_t count=rd(set+0xc);if(count>256){std::fprintf(stderr,"[truck-pose] ERROR invalid node count %u\n",count);continue;}model.push_back(new Region(set,0x38));model.push_back(new Region(rd(set+8),16*count));model.push_back(new Region(rd(set+0x18),64*count));model.push_back(new Region(rd(set+0x1c),64*count));}
  auto restore=[&](){vehicle.restore();sim.restore();for(auto p:model)p->restore();};
  unsigned calls=0;
  auto invoke=[&](uint32_t dt){rx_regs in{},out{};in.r[1]=v;in.r[3]=regs[3];uint32_t args[]={dt};rx_call_original_ex(0x0046c090,&in,&out,1,args);++calls;restore();};
  invoke(0x3c888889);
  for(uint32_t kind:{7u,2u})for(uint32_t dt:{0u,0x3c888889u,0x3dcccccdu}){wr(s+0x55f0,kind);invoke(dt);}
  for(uint32_t speed:{0x42dc0000u,0x43000000u,0x43480000u})for(uint32_t on:{0u,0x3f800000u}){wr(s+0x865c,speed);wr(v+0x13f0,0x3f000000);wr(v+0x13f4,on);invoke(0x3c888889);}
  for(auto p:model)delete p;
  std::fprintf(stderr,"[truck-pose] complete %u restored stimuli V=%08x S=%08x; check positive Verify counts\n",calls,v,s);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
