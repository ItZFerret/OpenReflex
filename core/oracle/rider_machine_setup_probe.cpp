/* Real authored machine-tree setup, restored after each registered call.
 * RX_RIDER_MACHINE_PROBE=1, RX_RIDER_MACHINE_FRAME=20. Select
 * rx_RiderMachineSetup in Verify. Serial development probe, never product.
 * Setup is construction-only: clear the actual IK node's +64 to reestablish
 * its original constructor precondition, then restore heap/pages/CPU completely. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <set>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t p){return rx_rd32(p);}void wr(uint32_t p,uint32_t v){rx_wr32(p,v);}
struct Pages {
 reflex_vm::Memory& memory;
 struct Page{uint32_t address;std::array<uint8_t,4096> bytes;};
 std::vector<Page> saved;std::vector<uint8_t> mapped;
 explicit Pages(reflex_vm::Memory& m):memory(m),mapped(1u<<20,0){
  /* Fault in all backed pages before snapshot: later lazy touches then cannot
   * conceal a modified page. A captured world has bounded mapped memory. */
  for(uint32_t i=0;i<(1u<<20);i++)if(m.mapped(i<<12)){
   mapped[i]=1;saved.push_back(Page{});auto& p=saved.back();p.address=i<<12;
   std::memcpy(p.bytes.data(),m.page(p.address),4096);
  }
 }
 void restore(){
  for(auto& p:saved)std::memcpy(memory.page(p.address),p.bytes.data(),4096);
  for(uint32_t i=0;i<(1u<<20);i++)if(!mapped[i]&&memory.mapped(i<<12))memory.unmap(i<<12,4096);
 }
 ~Pages(){restore();}
};
}
RX_PORT(0x0084ed20,RiderMachineSetupProbe,"thunk",0){
 if(!std::getenv("RX_RIDER_MACHINE_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_RIDER_MACHINE_FRAME")?uint32_t(std::atoi(std::getenv("RX_RIDER_MACHINE_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t g=rd(0x00a946b8),v=0,ra=0;
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;
  uint32_t a=rd(p+0x1ac);if(!a||!m.mem.mapped(a+0x1a0))continue;
  uint32_t vt=rd(a);if(vt!=0x0093d2e4&&vt!=0x0093d4d4&&vt!=0x0093cee4)continue;
  uint32_t r=rd(a+0x1a0);if(r&&m.mem.mapped(r+0x5bc)){v=a;ra=r;break;}
 }
 if(!v){std::fprintf(stderr,"[rider-machine] ERROR no real four-wheel rider\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t ch=rd(ra+0x144),root=rd(ch+0x224),ik=rd(ra+0x5bc),expression=rd(ra+0x5c4);
 if(!ik||!expression||m.mem.read(expression+0x5c,1)!=0){std::fprintf(stderr,"[rider-machine] ERROR missing IK/expression or unsupported expression flag\n");return reflex_vm::RUN_ORIGINAL;}
 std::set<uint32_t> nodes;std::vector<uint32_t> queue{root};bool valid=true;
 for(size_t i=0;i<queue.size()&&i<1024;i++){
  uint32_t a=queue[i];if(!a||!m.mem.mapped(a+0xc8)||!nodes.insert(a).second)continue;
  uint32_t walk=rd(rd(a)+0x28);
  switch(walk){
  case 0x0083c570:break;
  case 0x00834530:queue.push_back(rd(a+0x58));queue.push_back(rd(a+0x5c));break;
  case 0x00834c30:queue.push_back(rd(a+0x2c));queue.push_back(rd(a+0x30));break;
  case 0x006ad770:queue.push_back(rd(a+0x28));break;
  case 0x006b5770:queue.push_back(rd(a+0x5c));break;
  case 0x006afa60:queue.push_back(rd(a+0x30));break;
  case 0x0083cd80:for(uint32_t p=rd(a+0xc4);p<rd(a+0xc8);p+=4)queue.push_back(rd(p));break;
  case 0x0083f5a0:for(uint32_t k=0;k<rd(a+0x8c);k++)queue.push_back(rd(rd(a+0x74)+4*k));break;
  default:std::fprintf(stderr,"[rider-machine] ERROR unaudited tree walk %08x\n",walk);valid=false;break;
  }
 }
 if(!valid||queue.size()>1024)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[rider-machine] ERROR no reversible host state; refusing allocating setup replay\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();if(host_restore)host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned calls=0;
 auto invoke=[&](uint32_t fn,uint32_t ecx,uint32_t eax,unsigned n,const uint32_t* args){rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[1]=ecx;in.r[0]=eax;rx_call_original_ex(fn,&in,&out,n,args);++calls;restore();};
 wr(ik+0x64,0);invoke(0x0061aa10,ra,v,0,nullptr);
 for(uint32_t a:nodes){
  uint32_t w=regs[4]-0x100;wr(w,0x00958924);wr(w+4,v);wr(w+8,ra);for(unsigned i=0xc;i<=0x18;i+=4)wr(w+i,0);
  if(rd(a+4)==5)wr(a+0x64,0);
  uint32_t arg=a;invoke(0x0061aaa0,w,regs[0],1,&arg);
  if(rd(a+4)==3){uint32_t args[]={ra,a};invoke(0x0061ab30,regs[1],regs[0],2,args);}
 }
 restore();
 std::fprintf(stderr,"[rider-machine] complete %u restored registered calls, %zu actual animators V=%08x rider=%08x; heap/pages/CPU restored\n",calls,nodes.size(),v,ra);
 return reflex_vm::RUN_ORIGINAL;
}
