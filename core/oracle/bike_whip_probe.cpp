/* Development-only paired-stick takeoff comparison. Synthetic branch stimuli on
 * a genuine MX simulation; this is not evidence of a naturally ridden jump. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
unsigned frame;
uint32_t rd(uint32_t a){return rx_rd32(a);}
void wr(uint32_t a,uint32_t b){rx_wr32(a,b);}
void wf(uint32_t a,float f){uint32_t b;std::memcpy(&b,&f,4);wr(a,b);}
struct Save {uint32_t a;std::vector<uint32_t> words;Save(uint32_t p,uint32_t n):a(p){for(uint32_t i=0;i<n;i+=4)words.push_back(rd(p+i));}void restore()const{for(unsigned i=0;i<words.size();i++)wr(a+4*i,words[i]);}~Save(){restore();}};
}
RX_PORT(0x0084ed20,BikeWhipProbeHook,"thunk",0){
 if(!std::getenv("RX_BIKE_WHIP_PROBE")||++frame!=900)return reflex_vm::RUN_ORIGINAL;
 uint32_t s=0,g=rd(0xa946b8);
 if(g)for(unsigned i=0;i<16;i++){uint32_t p=g+0x7460+0x12a0*i;if(!m.mem.mapped(p+0x1ac))continue;uint32_t v=rd(p+0x1ac);if(!v||!m.mem.mapped(v+0x234))continue;uint32_t a=rd(v+0x234);if(a&&m.mem.mapped(a+0x8fbc)&&rd(a)==0x93c8a4){s=a;break;}}
 if(!s){std::fprintf(stderr,"[bike-whip] ERROR no genuine MX simulation; NOT COVERED\n");return reflex_vm::RUN_ORIGINAL;}
 const uint32_t c=rd(s+0x5c30);
 if(!c||!m.mem.mapped(c+0x84)){std::fprintf(stderr,"[bike-whip] ERROR missing controls; NOT COVERED\n");return reflex_vm::RUN_ORIGINAL;}
 for(unsigned i=0;i<3;i++){uint32_t f=rd(s+0x8b40+4*i);if(!f||!m.mem.mapped(f+8)){std::fprintf(stderr,"[bike-whip] ERROR missing actual ground filter; NOT COVERED\n");return reflex_vm::RUN_ORIGINAL;}}
 // Pointer-free payloads allow comparison with the separately initialized core.
 for(unsigned i=0;i<4;i++){
  std::fprintf(stderr,"[mx-body] index=%u",i);
  for(uint32_t off:{0x40u,0x50u,0x680u,0x690u})for(unsigned k=0;k<4;k++)std::fprintf(stderr," %08x",rd(s+0xd0+0xa0*i+off+4*k));
  std::fprintf(stderr,"\n");
 }
 const uint32_t pbase=s+0x2f10;
 for(unsigned i=0;i<rd(pbase+0x400)&&i<256;i++){
  uint32_t p=rd(pbase+4*i),kind=rd(p+0x40);char name[64]={};
  for(unsigned k=0;k<63;k++){name[k]=char(m.mem.read(p+k,1));if(!name[k])break;}
  std::fprintf(stderr,"[mx-param] %s kind=%u",name,kind);
  if(kind==2){unsigned n=rd(p+0x44);std::fprintf(stderr," %08x",n);for(unsigned k=0;k<n&&k<10;k++)std::fprintf(stderr," %08x %08x",rd(p+0x48+4*k),rd(p+0x70+4*k));std::fprintf(stderr," %08x %08x",rd(p+0x98),rd(p+0x9c));}
  else{unsigned n=kind==1?4:kind==3?3:1,off=kind==1?0x50:0x44;for(unsigned k=0;k<n;k++)std::fprintf(stderr," %08x",rd(p+off+4*k));}
  std::fprintf(stderr,"\n");
 }
 std::fprintf(stderr,"[mx-param-meta] fixedDt=%08x maxSteps=%08x rollT=%08x pitchT=%08x yawT=%08x steerT=%08x steep=%08x vertical=%08x noseDown=%08x\n",rd(s+0xbc),rd(s+0xb8),rd(s+0x8b7c),rd(s+0x8b90),rd(s+0x8ba4),rd(s+0x5c60),rd(0x9cd9ac),rd(0x9d7894),rd(0x9cc8a8));
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);auto fpu=m.fpu;uint32_t flags=m.flags;
 {
  Save sim(s,0x8fc0),ctl(c,0x88);
  // Ground correction mutates these three separate lag filters.
  Save f0(rd(s+0x8b40),12),f1(rd(s+0x8b44),12),f2(rd(s+0x8b48),12);
  auto restore=[&](){sim.restore();ctl.restore();f0.restore();f1.restore();f2.restore();};
  auto call=[&](uint32_t fn,float dt){rx_regs in{},out{};uint32_t bits;std::memcpy(&bits,&dt,4);if(fn==0x43f2e0){uint32_t args[]={s,bits};rx_call_original_ex(fn,&in,&out,2,args);}else{in.r[fn==0x440da0?7:1]=s;rx_call_original_ex(fn,&in,&out,1,&bits);}};
  unsigned airCalls=0,groundCalls=0,latchCalls=0;
  for(unsigned steep=0;steep<2;steep++)for(unsigned side=0;side<2;side++){
   restore();wr(s+0x2374,0);wr(s+0x8d98,0);wr(s+0x40,0);
   wf(s+0x100,0);wf(s+0x104,steep?20.f:3.f);wf(s+0x108,20.f);
   wf(s+0x2c,side?-.85f:.85f);wf(s+0x30,-.3f);wf(s+0x34,side?.85f:-.85f);wf(s+0x38,-.6f);
   call(0x44c170,.0075f);
   std::fprintf(stderr,"[bike-whip] controls steep%u side%u jumpAssist=%u pairedTakeoff=%u (terrain query retained)\n",steep,side,rd(s+0x8c24),rd(s+0x8c20));
  }
  for(unsigned slope=0;slope<2;slope++)for(unsigned first=0;first<2;first++)for(unsigned pattern=0;pattern<6;pattern++){
   restore();wr(s+0x2374,0);wr(s+0x3c,0);wr(s+0x8d98,1);wr(s+0x8d9c,first);wr(s+0x8da4,1);
   wr(s+0x8d44,0);wr(s+0x8d48,0);wr(s+0x8d5c,0);wr(s+0x8d60,0);wr(s+0x8c28,0);
   for(uint32_t off:{0x8c60u,0x8c64u,0x8c68u,0x8cd0u,0x8cd4u,0x8cd8u})wr(s+off,0);
   wf(s+0x100,0);wf(s+0x104,slope?20.f:3.f);wf(s+0x108,20);
   const float sign=pattern&1?-1.f:1.f;
   wf(c+0x68,.85f*sign);wf(c+0x6c,-.3f);wf(c+0x70,(pattern>=2?-.85f:.85f)*sign);wf(c+0x74,pattern>=4?-.85f:.3f);
   for(uint32_t off:{0x8bbcu,0x8bc0u,0x8bc4u,0x8bc8u,0x8bccu})wf(s+off,0);
   wf(s+0x8bd4,1);wf(s+0x8bd8,1);
   call(0x43f2e0,.0075f);airCalls++;
  }
  // The steep/level takeoff-reference alternative lives in the grounded slot.
  for(unsigned steep=0;steep<2;steep++){
   restore();wr(s+0x2374,0);wr(s+0x8d98,0);wf(s+0x100,0);wf(s+0x104,steep?20.f:0.f);wf(s+0x108,steep?1.f:20.f);
   wf(c+0x68,.8f);wf(c+0x6c,-.25f);wf(c+0x70,-.8f);wf(c+0x74,-.6f);
   call(0x443610,.0075f);groundCalls++;
  }
  for(unsigned side=0;side<2;side++)for(unsigned pattern=0;pattern<4;pattern++){
   restore();wr(s+0x8d98,0);wr(s+0x8c24,1);wr(s+0x3c,1);wr(s+0x8d48,0);wr(s+0x8d44,0);wr(s+0x8d60,0);wr(s+0x8d5c,0);
   for(uint32_t off:{0x8d54u,0x8d74u,0x8d70u,0x8d64u,0x8d68u})wf(s+off,0);
   wf(c+0x70,side?-.9f:.9f);wf(c+0x74,pattern&1?-.95f:-.6f);
   call(0x440da0,pattern>=2?.015f:.0075f);latchCalls++;
  }
  std::fprintf(stderr,"[bike-whip] 4 paired-stick controls, %u paired-stick air, %u level/steep grounded-reference, %u flick-latch comparisons; genuine MX + controls + external filters restored; synthetic branch coverage only\n",airCalls,groundCalls,latchCalls);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}

/* Deterministic, button-free genuine XInput stimulus. Select WhipPadHook
 * explicitly instead of PadSimHook; both replace the same imported provider.
 * The game still performs its normal pad/keymap/controller processing. */
RX_PORT(0x008ea364,WhipPadHook,"thunk",0){
 static bool installed=false;
 if(!installed&&std::getenv("RX_BIKE_WHIP_PAD")){
  installed=true;
  m.imports["xinput1_3.dll!#4"]=[](reflex_vm::Machine& mm){
   if(mm.arg(0)!=0)return mm.ret(12,1167);
   const uint8_t caps[20]={1,1,0,0,0xff,0xff,0xff,0xff,0xc0,0xff,0xc0,0xff,0xc0,0xff,0xc0,0xff,0xff,0xff,0xff,0xff};
   mm.mem.write_bytes(mm.arg(2),caps,sizeof caps);return mm.ret(12,0);
  };
  m.imports["xinput1_3.dll!#2"]=[](reflex_vm::Machine& mm){
   if(mm.arg(0)!=0)return mm.ret(8,1167);
   const uint32_t out=mm.arg(1),ctx=uint32_t(mm.mem.read(0xa95844,4));
   const uint32_t timing=uint32_t(mm.mem.read(ctx+0x10,4)),clock=uint32_t(mm.mem.read(timing+0x3c,4));
   float seconds;std::memcpy(&seconds,&clock,4);
   uint32_t game=uint32_t(mm.mem.read(0xa946b8,4));bool racing=false;
   if(game&&mm.mem.mapped(game+0x74f4)){
    uint32_t activity=uint32_t(mm.mem.read(game+0x74f4,4));
    racing=activity&&mm.mem.mapped(activity+0x21c)&&int32_t(mm.mem.read(activity+0x21c,4))>=2;
   }
   static bool anchored=false;static float start=0;
   if(racing&&!anchored){start=seconds;anchored=true;std::fprintf(stderr,"[whip-pad] racing anchor %.9g; RT255; paired500ms pulses; no buttons\n",seconds);}
   const int phase=racing?int((seconds-start)*2)%6:0;
   int16_t lx=0,ly=0,rx=0,ry=0;
   if(racing&&phase>=1&&phase<=4){int sign=phase<=2?1:-1;lx=int16_t(sign*26214);ly=int16_t(sign*19660);rx=int16_t(-sign*26214);ry=int16_t(sign*19660);}
   mm.mem.write(out,4,clock);mm.mem.write(out+4,2,0);mm.mem.write(out+6,1,0);mm.mem.write(out+7,1,racing?255:0);
   mm.mem.write(out+8,2,uint16_t(lx));mm.mem.write(out+10,2,uint16_t(ly));mm.mem.write(out+12,2,uint16_t(rx));mm.mem.write(out+14,2,uint16_t(ry));
   return mm.ret(8,0);
  };
  m.imports["xinput1_3.dll!#3"]=[](reflex_vm::Machine& mm){return mm.ret(8,mm.arg(0)==0?0:1167);};
  std::fprintf(stderr,"[whip-pad] installed deterministic XInput provider (exclusive with PadSimHook)\n");
 }
 return reflex_vm::RUN_ORIGINAL;
}
