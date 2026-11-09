// Development-only exact per-call Verify stimulus. No simulation bypass.
// REFLEX_PORT=rx_phys,rx_RodBoxProbeHook REFLEX_PORT_VERIFY=1 RX_RODBOX_PROBE=1
// RX_RODBOX_PROBE_FRAME=5 <CLI> <world-file> <frames> ... (same CLI arguments as other probes).
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>
using namespace reflex_port;
using rx_oracle::bits;
namespace {
uint32_t frame=0,rng=0x6a09e667;
uint32_t random32(){rng=rng*1664525u+1013904223u;return rng;}
float value(float lo,float hi){return lo+(hi-lo)*(float(random32()>>8)/16777216.0f);}
void word(uint32_t a,uint32_t v){rx_wr32(a,v);}
void number(uint32_t a,float v){word(a,bits(v));}
rx_regs regs(){rx_regs r;std::memset(&r,0,sizeof r);return r;}
void call(uint32_t fn,rx_regs r,int n,const uint32_t* args){rx_regs out=regs();rx_call_original_ex(fn,&r,&out,n,args);}
uint32_t alloc(uint32_t n){const uint32_t arg=n;uint32_t p=rx_call_original(0x0078fbe0,RX_CDECL,0,1,&arg);for(uint32_t i=0;i<n;i+=4)word(p+i,0);return p;}
void probes(){
 const uint32_t mem=alloc(0x1200),dir=mem,center=mem+0x10,ext=mem+0x20,plus=mem+0x40,minus=mem+0x50,vp=mem+0x60,vm=mem+0x64,sum=mem+0x70,count=mem+0x80;
 const uint32_t qa=mem+0x100,qb=mem+0x110,pa=mem+0x120,pb=mem+0x130,box=mem+0x200,radial=mem+0x280,M=mem+0x400;
 for(int t=0;t<192;t++){
  for(int k=0;k<4;k++){
   number(dir+4*k,value(-1.5f,1.5f));number(center+4*k,value(-2.0f,2.0f));number(ext+4*k,value(0.2f,4.0f));
   number(qa+4*k,k==3?1.0f:0.0f);number(qb+4*k,k==3?1.0f:0.0f);
   number(pa+4*k,value(-2.0f,2.0f));number(pb+4*k,0.0f);number(box+0x20+4*k,value(0.5f,3.0f));number(radial+0x20+4*k,1.5f);
  }
  // Axis-zero/signed-zero directions, exact tangencies and general rotations.
  if(t%8==0){word(dir,0);word(dir+4,0x80000000);number(dir+8,1.0f);}
  if(t%8==1){number(center,0);number(center+4,0);number(center+8,0);number(ext,1);number(ext+4,1);number(ext+8,1);}
  if(t%4==2){number(qa,0.3f);number(qa+4,0.4f);number(qa+8,0.0f);number(qa+12,0.8660254f);number(qb,0);number(qb+4,0.6f);number(qb+8,0);number(qb+12,0.8f);}
  const float radius=t%8==1?1.0f:value(0.1f,3.0f);
  uint32_t args[8]={bits(radius),bits(value(-4,4)),bits(value(0.1f,4)),bits(value(0.1f,4)),plus,minus,vp,vm};
  for(uint32_t fn:{0x0081da00u,0x0081dc30u,0x0081de70u}){
   for(uint32_t a=plus;a<vm+4;a+=4)word(a,0xa5a5a5a5);
   rx_regs r=regs();r.r[6]=dir;r.r[7]=center;call(fn,r,8,args);
  }
  number(sum,value(-1,1));number(sum+4,value(-1,1));number(sum+8,value(-1,1));number(count,value(0,3));
  {uint32_t a[4]={dir,bits(radius),sum,count};rx_regs r=regs();r.r[0]=ext;r.r[1]=center;call(0x0081e8e0,r,4,a);}
  for(uint32_t a=M;a<M+0x700;a+=4)word(a,0x5a5a5a5a);
  word(M,t%3==0?29:t%3==1?30:0);word(M+0x6a0,0);
  uint32_t a[6]={M,box,qb,pb,radial,uint32_t(t&1)};rx_regs r=regs();r.r[1]=qa;r.r[2]=pa;call(0x0086b7f0,r,6,a);
 }
 // The original captures count before its final weighted point stores.
 // Count is allowed to alias each sum component; these calls test that order.
 for(uint32_t lane=0;lane<3;lane++){
  number(dir,0.3f);number(dir+4,0.4f);number(dir+8,0.5f);
  number(center,0.1f);number(center+4,0.2f);number(center+8,0.3f);
  number(ext,2);number(ext+4,3);number(ext+8,4);
  number(sum,0.75f);number(sum+4,1.25f);number(sum+8,1.75f);
  uint32_t a[4]={dir,bits(1.25f),sum,sum+4*lane};rx_regs r=regs();r.r[0]=ext;r.r[1]=center;call(0x0081e8e0,r,4,a);
 }
 std::fprintf(stderr,"[rodboxprobe] 192 cases each faces x/y/z/full pair; 195 face sums including 3 count aliases (963 calls)\n");
}
}
RX_PORT(0x0084ed20,RodBoxProbeHook,"thunk",0){
 static const char* enabled=std::getenv("RX_RODBOX_PROBE");if(!enabled||!*enabled)return reflex_vm::RUN_ORIGINAL;
 static const uint32_t at=std::getenv("RX_RODBOX_PROBE_FRAME")?uint32_t(std::atoi(std::getenv("RX_RODBOX_PROBE_FRAME"))):5;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t saved[8];reflex_vm::Xmm xmm[8];std::memcpy(saved,m.r,sizeof saved);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;const size_t fpu=m.fpu.size();
 probes();std::memcpy(m.r,saved,sizeof saved);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu.resize(fpu);return reflex_vm::RUN_ORIGINAL;
}
