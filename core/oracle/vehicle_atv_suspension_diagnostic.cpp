/* Developer-only intermediate comparison; RX_ATV_SUSPENSION_DIAG=1.
 * Hooks observe original registers and resume the exact instruction. */
#include "oracle.h"
extern "C" {
#include "../game/bike_math.h"
}
#include <cstdio>
#include <cstdlib>
using namespace reflex_port;
static float df(rx_addr p){return rx_bf(rx_rd32(p));}
static void show(const char* tag,rx_v4 v){std::fprintf(stderr,"[atv-susp-diag] %s %08x %08x %08x %08x\n",tag,rx_fb(v.v[0]),rx_fb(v.v[1]),rx_fb(v.v[2]),rx_fb(v.v[3]));}
static rx_v4 xv(reflex_vm::Machine& m,unsigned i){rx_v4 v;std::memcpy(&v,&m.xmm[i],16);return v;}
static uint32_t original_speed(reflex_vm::Machine& m){rx_oracle::Bind bind(m);static unsigned n;if(++n<=2){show("original axis",xv(m,4));show("original spring",xv(m,0));show("original speed",xv(m,2));show("original local",rx_ld4(m.r[4]+0xd0));show("original angular",v4_perm(xv(m,3),2,0,1,3));}return reflex_vm::RUN_ORIGINAL;}
static uint32_t original_force(reflex_vm::Machine& m){static unsigned n;if(++n<=2)show("original force",xv(m,1));return reflex_vm::RUN_ORIGINAL;}
void atv_suspension_diagnostic(reflex_vm::Machine& m){
 if(!std::getenv("RX_ATV_SUSPENSION_DIAG"))return;
 static bool installed=false;if(!installed){runtime_native(m,0x0049407b,original_speed);runtime_native(m,0x004940d3,original_force);installed=true;}
 static unsigned count;if(++count>2)return;
 rx_addr s=m.r[7];rx_v4 q=rx_ld4(s+0x730),axis=bike_rotate(q,rx_ld4(0xd616a0));
 rx_v4 angular=bike_rotate(q,rx_ld4(s+0x740)),offset=v4_sub(rx_ld4(s+0x230),rx_ld4(s+0xf0));
 rx_v4 local=bike_rotate_inv(q,offset),diff=v4_sub(local,rx_ld4(s+0x6bd0));
 rx_v4 spring=v4_scale(axis,phys_mul(phys_sub(phys_sqrt(bike_hsum_021(v4_mul(diff,diff))),df(s+0x3c7c)),df(s+0x96dc)));
 rx_v4 cross=v4_xyz(v4_sub(v4_mul(v4_perm(offset,2,0,1,3),v4_perm(angular,1,2,0,3)),v4_mul(v4_perm(offset,1,2,0,3),v4_perm(angular,2,0,1,3))));
 float speed=bike_dot3(v4_sub(rx_ld4(s+0x240),v4_add(cross,rx_ld4(s+0x100))),axis);
 float damping=df(s+(speed<0?0x96e4:0x96e0));rx_v4 force=v4_add(v4_scale(axis,phys_mul(phys_sub(0,speed),damping)),spring);
 show("native axis",axis);show("native spring",spring);show("native speed",v4_splat(speed));show("native local",local);show("native angular",angular);show("native force",force);
}
