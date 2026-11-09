/* Development-only original oracle stimulus. No game logic; save/restore a live BrakeTurn and the fields each leaf reads.
 * REFLEX_PORT=rx_BrakeTurn,rx_BrakeTurnProbeHook REFLEX_PORT_VERIFY=1 RX_BRAKETURN_PROBE=1
 * RX_BRAKETURN_PROBE_FRAME=650 (default). Existing reference world with live AI drivers required.
 * Each rx_call_original_ex reaches the adapter's per-call Verify comparison against the original body. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
using namespace reflex_port;
namespace {
uint32_t frame;
uint32_t rd(uint32_t a){return rx_rd32(a);} void wr(uint32_t a,uint32_t v){rx_wr32(a,v);}
uint32_t fb(float f){uint32_t b;std::memcpy(&b,&f,4);return b;}
struct Region {
 uint32_t a;std::vector<uint32_t> words;
 Region(uint32_t p,uint32_t bytes):a(p){for(uint32_t k=0;k<bytes;k+=4)words.push_back(rd(p+k));}
 void restore()const{for(size_t k=0;k<words.size();k++)wr(a+uint32_t(4*k),words[k]);}
 ~Region(){restore();}
};
void call(uint32_t fn,uint32_t b,uint32_t enabled=0){
 rx_regs in{},out{};in.r[1]=b;in.r[2]=b;in.r[6]=b;
 if(fn==0x00522eb0)rx_call_original_ex(fn,&in,&out,1,&enabled);
 else rx_call_original_ex(fn,&in,&out,0,nullptr);
}
}
RX_PORT(0x0084ed20,BrakeTurnProbeHook,"thunk",0){
 if(!std::getenv("RX_BRAKETURN_PROBE"))return reflex_vm::RUN_ORIGINAL;
 const uint32_t at=std::getenv("RX_BRAKETURN_PROBE_FRAME")?uint32_t(std::atoi(std::getenv("RX_BRAKETURN_PROBE_FRAME"))):650;
 if(++frame!=at)return reflex_vm::RUN_ORIGINAL;
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);const uint32_t flags=m.flags;auto fpu=m.fpu;
 uint32_t b=0,d=0;const uint32_t g=rd(0x00a946b8);
 if(g)for(uint32_t slot=0;slot<16&&!b;slot++){
  uint32_t p=g+0x7460+0x12a0*slot;if(!m.mem.mapped(p+0x94))continue;
  uint32_t act=rd(p+0x94);if(!act||!m.mem.mapped(act+0x26c))continue;
  for(uint32_t i=0;i<rd(act+0x26c)&&i<32&&!b;i++){
   uint32_t candidate=rd(rd(act+0x268)+4*i);if(!candidate||!m.mem.mapped(candidate+0xee0))continue;
   for(uint32_t k=0;k<rd(candidate+0xee0)&&k<40;k++){
    uint32_t o=rd(candidate+0xe8+4*k);if(o&&m.mem.mapped(o)&&rd(o)==0x00940050){b=o;d=candidate;break;}
   }
  }
 }
 if(!b){std::fprintf(stderr,"[braketurn] ERROR no live BrakeTurn object\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t p=rd(d+0x74),v=rd(p+0x1ac),index=rd(p+0x8f4)+4*(3*rd(d+0x328)+rd(d+0x32c)),t=0x009e3648+0x1c*index;
 {
  Region sb(b,0x40),sd(d,0x400),sv(v,0x160),st(t,28);
  auto setup=[&](){
   sb.restore();sd.restore();sv.restore();st.restore();
   wr(b+0x20,0);wr(b+0x24,fb(1));wr(b+0x30,fb(3));wr(b+0x34,fb(4));wr(b+0x38,0);wr(b+0x18,0xffffffff);
   wr(v+0xc0,0);wr(v+0xc4,0);wr(v+0xc8,0);wr(v+0x130,0);wr(v+0x134,0);wr(v+0x138,0);wr(v+0x150,fb(3));wr(d+0x130,fb(10));
   const float row[]={.25f,.5f,2,3,4,5,.75f};for(unsigned i=0;i<7;i++)wr(t+4*i,fb(row[i]));
  };
  setup();call(0x00522eb0,b,7);call(0x00522eb0,b,0);
  setup();call(0x00522fc0,b);                        // normalized target, keep steering
  setup();wr(v+0x130,fb(1));call(0x00522fc0,b);      // angle completes
  setup();wr(v+0x150,fb(1));call(0x00522fc0,b);      // speed completes
  setup();wr(v+0x150,fb(2));call(0x00522fc0,b);      // speed equality stays
  setup();wr(b+0x20,fb(1.25f));call(0x00522fc0,b);   // no ramp update
  setup();wr(b+0x20,0x7fc12345);wr(v+0x150,0x7fc12345);call(0x00522fc0,b);
  setup();wr(b+0x30,0);wr(b+0x34,0);call(0x00522fc0,b); // no normalization
  setup();wr(t,0);wr(b+0x24,0);call(0x00522fc0,b);  // signed zero outputs
  setup();wr(b+0xc,0);call(0x00523230,b);
  setup();wr(b+0xc,1);call(0x00523230,b);
  setup();wr(b+0x14,fb(10));call(0x005232a0,b);wr(b+0x14,fb(9));call(0x005232a0,b);
  setup();wr(b+0x14,0x7fc12345);call(0x005232a0,b);
  setup();wr(b+0x14,fb(10));call(0x00523300,b);wr(b+0x14,fb(9));call(0x00523300,b);
  std::fprintf(stderr,"[braketurn] complete: 17 leaf calls on live behaviour%08x driver%08x; state restored\n",b,d);
 }
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;
 return reflex_vm::RUN_ORIGINAL;
}
