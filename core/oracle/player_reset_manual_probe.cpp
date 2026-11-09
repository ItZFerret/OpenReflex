/* Opt-in real manual reset, not an invented receiver or patched callee.
 * Successful cases intentionally leave the actual player's legitimate reset
 * running, so its normal fade/placement runs on subsequent original frames. */
#include "oracle.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace reflex_port;
namespace {uint32_t frame;}
RX_PORT(0x0084ed20,PlayerResetManualProbeHook,"thunk",0){
 if(!std::getenv("RX_PLAYER_RESET_MANUAL_PROBE")||++frame!=900)return reflex_vm::RUN_ORIGINAL;
 uint32_t g=rx_rd32(0xa946b8),p=0;
 if(g)for(unsigned i=0;i<16;i++){uint32_t q=g+0x7460+0x12a0*i;if(!m.mem.mapped(q+0x1ac))continue;uint32_t a=rx_rd32(q+0x94),v=rx_rd32(q+0x1ac);if(a&&v&&rx_rd32(a+0x21c)==2&&rx_rd32(q+4)&&!rx_rd32(q+0x10)){p=q;break;}}
 if(!p){std::fprintf(stderr,"[manual-reset] no actual racing local player; no coverage\n");return reflex_vm::RUN_ORIGINAL;}
 uint32_t regs[8];reflex_vm::Xmm xmm[8];std::memcpy(regs,m.r,sizeof regs);std::memcpy(xmm,m.xmm,sizeof xmm);uint32_t flags=m.flags;auto fpu=m.fpu;
 auto call=[&](){rx_regs in{},out{};in.r[6]=p;rx_call_original_ex(0x550400,&in,&out,0,nullptr);};
 const uint32_t oldtimer=rx_rd32(p+0x116c),oldspot=rx_rd32(p+0x1130),oldrequest=rx_rd32(p+0x1138);uint32_t saved[8];for(unsigned i=0;i<8;i++)saved[i]=rx_rd32(p+0x1140+4*i);
 rx_wr32(p+0x116c,0x3f800000);call(); /* original JA early return */
 uint32_t v=rx_rd32(p+0x1ac);for(unsigned i=0;i<4;i++){rx_wr32(p+0x1140+4*i,rx_rd32(v+0x200+4*i));rx_wr32(p+0x1150+4*i,rx_rd32(v+0x210+4*i));}
 rx_wr32(p+0x116c,0);rx_wr32(p+0x1130,1);rx_wr32(p+0x1138,1);call();
 std::fprintf(stderr,"[manual-reset] saved spot reason=%u timer=%08x\n",rx_rd32(p+0x1170),rx_rd32(p+0x116c));
 rx_wr32(p+0x116c,0);rx_wr32(p+0x1130,0);call(); /* real activity point, or real vehicle fallback */
 std::fprintf(stderr,"[manual-reset] activity/fallback reason=%u timer=%08x original calls=3 (prior timer=%08x)\n",rx_rd32(p+0x1170),rx_rd32(p+0x116c),oldtimer);
 {rx_regs in{},out{};in.r[6]=p;rx_wr32(p+0x116c,0x3f800000);rx_call_original_ex(0x550a20,&in,&out,0,nullptr);rx_wr32(p+0x116c,0);rx_call_original_ex(0x550a20,&in,&out,0,nullptr);}
 std::fprintf(stderr,"[manual-reset] out-of-bounds original calls=2 reason=%u timer=%08x\n",rx_rd32(p+0x1170),rx_rd32(p+0x116c));
 rx_wr32(p+0x1130,oldspot);rx_wr32(p+0x1138,oldrequest);for(unsigned i=0;i<8;i++)rx_wr32(p+0x1140+4*i,saved[i]);
 std::memcpy(m.r,regs,sizeof regs);std::memcpy(m.xmm,xmm,sizeof xmm);m.flags=flags;m.fpu=fpu;return reflex_vm::RUN_ORIGINAL;
}
