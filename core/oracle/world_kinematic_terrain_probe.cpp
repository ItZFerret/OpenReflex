/* Restored finite terrain-response fixtures using the world's authored material
 * registry and actual registered0076f2f0 dispatch. RX_KIN_TERRAIN_PROBE=1,
 * RX_KIN_TERRAIN_FRAME=20; select rx_KinematicTerrainContact in Verify.
 * No allocations/fabricated vtables or collision-query stand-ins. This function
 * consumes a point/normal and body state, so controlled contact inputs exercise
 * its real impulse kernel. Forty cases restore all guest/host/CPU state. */
#include "oracle.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;uint32_t rd(uint32_t a){return rx_rd32(a);}void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
uint32_t fb(float x){uint32_t b;std::memcpy(&b,&x,4);return b;}
void wf(uint32_t a,float x){wr(a,fb(x));}
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
RX_PORT(0x0084ed20,KinematicTerrainContactProbe,"thunk",0){
 if(!std::getenv("RX_KIN_TERRAIN_PROBE"))return reflex_vm::RUN_ORIGINAL;
 uint32_t at=std::getenv("RX_KIN_TERRAIN_FRAME")?uint32_t(std::atoi(std::getenv("RX_KIN_TERRAIN_FRAME"))):20;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 auto mapped=[&](uint32_t a,uint32_t n){if(!a||!n||a+n<a)return false;for(uint32_t i=0;i<n;i+=4096)if(!m.mem.mapped(a+i))return false;return m.mem.mapped(a+n-1);};
 uint32_t count=(rd(0x00da6ae0)-rd(0x00da6adc))/208,table=rd(0x00da6af4),end=rd(0x00da6af8);
 uint32_t W=(m.r[4]-0xa000)&~15u,O=W+0x6200,C=W+0x6300;
 if(!(rd(0x00dce7d4)&1)||!count||count>1024||end<table||end-table<12*count||!mapped(table,12*count)||!mapped(W,0x6400)){
  std::fprintf(stderr,"[kin-terrain] ERROR no initialized material registry or mapped stack fixture\n");return reflex_vm::RUN_ORIGINAL;
 }
 uint32_t regs[8];reflex_vm::Xmm xmm[8];uint64_t mmx[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);std::memcpy(mmx,m.mmx,sizeof mmx);
 auto flags=m.flags;auto fpu=m.fpu;auto cw=m.fpu_cw,sw=m.fpu_sw;auto mxcsr=m.mxcsr;bool mmx_active=m.mmx_active;
 auto host_restore=snapshot_host_state(m);
 if(!host_restore){std::fprintf(stderr,"[kin-terrain] ERROR missing reversible host snapshot\n");return reflex_vm::RUN_ORIGINAL;}
 Pages memory(m.mem);
 auto restore=[&](){memory.restore();host_restore();std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);std::memcpy(m.mmx,mmx,sizeof mmx);m.flags=flags;m.fpu=fpu;m.fpu_cw=cw;m.fpu_sw=sw;m.mxcsr=mxcsr;m.mmx_active=mmx_active;};
 const float velocities[][2]={{-3,0},{-3,.005f},{-3,.02f},{-3,2},{3,0},{3,2},{0,0},{0,2}};
 const float mass[]={.5f,1,100,1200},inertia[]={.2f,1,100,1000};
 uint32_t materials[]={0,count/2,count-1};unsigned calls=0,changed=0,unchanged=0;
 for(unsigned group=0;group<5;group++)for(unsigned variant=0;variant<8;variant++){
  uint32_t material=group<3?materials[group]:materials[1],pair=table+12*material;
  /* Last two groups vary valid coefficients in the actual registry record;
   * restore returns every material word before the next call. */
  if(group==3){wf(pair,0);wf(pair+4,0);}
  if(group==4){wf(pair,.8f);wf(pair+4,1.2f);}
  uint32_t restitution=rd(pair),friction=rd(pair+4);
  for(unsigned i=0;i<0x90;i+=4)wr(O+i,0);
  unsigned k=variant%4;wr(O+0x80,k);wr(O+0x24,material);
  wf(O,mass[variant%4]);wf(O+0x10,inertia[variant%4]);wf(O+0x14,inertia[variant%4]*1.5f);wf(O+0x18,inertia[variant%4]*2);
  wf(O+0x40,2);wf(O+0x44,4);wf(O+0x48,6);
  if(variant&1){wf(O+0x54,.70710677f);wf(O+0x5c,.70710677f);}else wf(O+0x5c,1);
  uint32_t vel=W+0x1d10+0x50*k,ang=W+0x5b90+0x50*k;
  wf(vel,velocities[variant][1]);wf(vel+4,velocities[variant][0]);wf(vel+8,variant==3?.7f:0);wf(vel+12,0);
  for(unsigned i=0;i<4;i++)wf(ang+4*i,0);
  /* Off-centre contacts couple rotational inertia; variants0/4 use a centred
   * normal to isolate the restitution/normal-velocity and separating paths. */
  wf(C,variant%4?2.35f:2);wf(C+4,3.5f);wf(C+8,variant%4?6.2f:6);wf(C+12,0);
  wf(C+16,0);wf(C+20,-1);wf(C+24,0);wf(C+28,0);
  uint32_t before[8];for(unsigned i=0;i<4;i++){before[i]=rd(vel+4*i);before[4+i]=rd(ang+4*i);}
  rx_regs in{},out{};for(unsigned i=0;i<8;i++)in.r[i]=regs[i];in.r[1]=O;
  uint32_t args[]={W,C};rx_call_original_ex(0x0076f2f0,&in,&out,2,args);
  bool delta=false;for(unsigned i=0;i<4;i++)delta|=before[i]!=rd(vel+4*i)||before[4+i]!=rd(ang+4*i);
  if(delta)++changed;else++unchanged;++calls;
  if(variant==0)std::fprintf(stderr,"[kin-terrain] material=%u restitution=%08x friction=%08x group=%u\n",material,restitution,friction,group);
  restore();
 }
 restore();
 std::fprintf(stderr,"[kin-terrain] complete calls=%u changed=%u unchanged=%u authored-materials=%u registry=initialized coefficients=authored/zero/high masses=4 inertias=4; full guest/host/CPU restored\n",calls,changed,unchanged,count);
 return reflex_vm::RUN_ORIGINAL;
}
