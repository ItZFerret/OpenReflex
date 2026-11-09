/* Restored actual four-wheel controller-publication fixtures. RX_FOURWHEEL_BIND_PROBE=1,
 * RX_FOURWHEEL_BIND_FRAME=20 (default), group rx_FourWheelControllerBind.
 * Actual controller 0 actions and original simulation pointers remain intact. Full guest
 * pages, added mappings, host allocator and CPU are restored after each call. */
#include "oracle.h"
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t p){return rx_rd32(p);}void wr(uint32_t p,uint32_t v){rx_wr32(p,v);}
uint32_t fb(float x){uint32_t b;std::memcpy(&b,&x,4);return b;}
float fl(uint32_t b){float x;std::memcpy(&x,&b,4);return x;}
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
RX_PORT(0x0084ed20,FourwheelControllerBindProbe,"thunk",0){
 if(!std::getenv("RX_FOURWHEEL_BIND_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_FOURWHEEL_BIND_FRAME")?uint32_t(std::atoi(std::getenv("RX_FOURWHEEL_BIND_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=4096)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t g=rd(0x00a946b8),v=0,s=0,fn=0,dest=0;
 if(g)for(unsigned i=0;i<16;i++){
  uint32_t player=g+0x7460+0x12a0*i;if(!mapped(player+0x1ac,4))continue;
  uint32_t a=rd(player+0x1ac);if(!mapped(a,0x1630))continue;
  uint32_t vt=rd(a),off=vt==0x0093d4d4?0x1540:vt==0x0093d2e4?0x15c0:vt==0x0093cee4?0x15d0:0;
  if(!off)continue;uint32_t sim=rd(a+off);if(!mapped(sim,0x8930))continue;
  uint32_t target=rd(rd(sim)+23*4);if(target!=0x004d9460&&target!=0x004be490)continue;
  v=a;s=sim;fn=target;dest=target==0x004be490?0x56e0:0x56f0;break;
 }
 uint32_t pad=rd(0x009c52c4);
 if(!v||!mapped(pad,0x1200)){std::fprintf(stderr,"[fourwheel-bind] ERROR missing actual vehicle/controller0 fixture\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[fourwheel-bind] ERROR missing reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 unsigned calls=0,bad=0,axis_calls=0,available_calls=0;
 auto invoke=[&](uint32_t controller){
  rx_regs in{},out{};for(unsigned j=0;j<8;j++)in.r[j]=regs[j];in.r[1]=s;
  rx_call_original_ex(fn,&in,&out,1,&controller);
  if(rd(s+dest)!=rd(s+0x4c)||rd(s+dest+4)!=rd(s+0x50)||out.r[0]!=rd(s+0x4c)||out.r[1]!=rd(s+0x50))++bad;
  ++calls;restore();
 };
 /* -1 bypasses input sampling. Exercise exact raw publication, including
  * unusual float bits: these source fields are copied as integers. */
 const uint32_t values[][2]={{0,0},{0x3f800000,0xbf800000},{0x80000000,0x7fc12345},{0xdeadbeef,0x01234567}};
 for(auto& pair:values){wr(s+0x4c,pair[0]);wr(s+0x50,pair[1]);wr(s+dest,0x4b123456);wr(s+dest+4,0x4b123456);invoke(0xffffffffu);}
 /* Actual controller actions: stick throttle, brake-hold and emergency-brake
  * gate combinations. Action objects and values are never fabricated. */
 for(unsigned mask=0;mask<8;mask++){
  wr(s+0x44,(mask&1)!=0);wr(s+0x54,(mask&2)!=0);wr(s+0x58,(mask&4)!=0);
  wr(s+dest,0x4b123456);wr(s+dest+4,0x4b123456);invoke(0);
 }
 /* Audited real CompoundKeyMap (00974c10) and KeyMap (00974b74)
  * topology from all four RXWI captures. Leave binding/device values intact;
  * only slot0 enabled flags change, with restoration after each invocation. */
 uint32_t action=pad+0x127c,child0=rd(action+0xa4),child1=rd(action+0xa8);
 bool topology=mapped(child0,0x38)&&mapped(child1,0x38)&&rd(action)==0x00974c10&&rd(child0)==0x00974b74&&rd(child1)==0x00974b74;
 if(!topology){++bad;std::fprintf(stderr,"[fourwheel-bind] ERROR unaudited compound/keymap topology\n");}
 else for(unsigned mask=0;mask<4;mask++)for(unsigned kind=0;kind<3;kind++){
  wr(child0+0x34,(mask&1)!=0);wr(child1+0x34,(mask&2)!=0);
  /* Bind must actually call the gated axis sampler. */
  wr(s+0x44,1);
  if(kind==0){invoke(0);continue;}
  rx_regs in{},out{};for(unsigned j=0;j<8;j++)in.r[j]=regs[j];in.r[6]=0;
  if(kind==1){
   rx_call_original_ex(0x005485e0,&in,&out,0,nullptr);++axis_calls;
   if(out.fpu_depth!=1)++bad;
  }else{
   in.r[1]=action;uint32_t controller=0;
   rx_call_original_ex(0x00811c90,&in,&out,1,&controller);++available_calls;
   if(out.r[0]!=(mask?1u:0u))++bad;
  }
  ++calls;restore();
 }
 restore();
 std::fprintf(stderr,"[fourwheel-bind-axis] complete enabled-masks=0/1/2/3 bind=4 axis=%u compound-available=%u errors=%u; real action values preserved\n",axis_calls,available_calls,bad);
 std::fprintf(stderr,"[fourwheel-bind] %s %u restored registered calls V=%08x S=%08x vf23=%08x controller0=%08x raw=4 live=8 enabled-bind=4 direct-axis=4 direct-available=4 publication-errors=%u; pages/heap/CPU restored\n",bad?"ERROR":"complete",calls,v,s,fn,pad,bad);
 return reflex_vm::RUN_ORIGINAL;
}
