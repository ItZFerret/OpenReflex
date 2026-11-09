/* Hand-C of three complete 196-byte vf11 methods. The shared sequence is
 * identical in their saved assembly; every family target/field stays explicit.
 * No product registration until the complete native dependency chain closes. */
#include "fourwheel_force_stage.h"
#include "fx_util.h"
#include "phys_vec.h"
typedef struct {
 rx_addr entry,power,coast,engine,axle,suspension,contact,tyre;
 unsigned throttle,timer,mode,wheel;
} ForceStage;
static void call(const ForceStage*p,rx_addr target,unsigned retOffset,
                 int n,const uint32_t*args,int*first,float timer){
 rx_addr expected=fx_push(n,args,p->entry+retOffset);
 rx_fourwheel_force_stage_dependency(target,expected,*first,timer);
 *first=0;
 /* Dependency must validate actual return/SP; do not normalize drift here. */
}
static void step(const ForceStage*p,rx_addr s,float dt){
 fx_frame f;fx_frame_enter(&f,0,8,FX_ESI,FX_EDI);fx_set_reg(FX_ESI,s);
 /* Original COMISS flags survive MOVSS/ADDSS and the timer store. Unordered
  * follows JBE into the coast arm; neither the timer nor dt selects the arm. */
 int power=F32(s,p->throttle)>F32(0,0x92e12c);
 float timer=phys_add(dt,F32(s,p->timer));SF32(s,p->timer,timer);int first=1;
 if(!U32(s,p->mode)){
  fx_set_reg(FX_EDI,s);
  call(p,power?p->power:p->coast,power?0x40:0x52,0,NULL,&first,timer);
 }
 fx_set_reg(FX_EDI,s);
 uint32_t delta=fx_x87_bits(rx_fb(dt));
 call(p,p->engine,0x60,1,&delta,&first,timer);
 call(p,p->axle,0x66,1,&s,&first,timer);
 call(p,p->suspension,0x6b,0,NULL,&first,timer);
 call(p,p->contact,0x70,0,NULL,&first,timer);
 const uint32_t args[]={s,delta};
 for(unsigned i=0;i<4;i++){
  fx_set_reg(FX_EDI,s+p->wheel+0x150*i);
  call(p,p->tyre,0x83+0x13*i,2,args,&first,timer);
 }
 fx_frame_leave(&f);
}
void rx_utv_force_stage_exact(rx_addr s,float dt){
 static const ForceStage p={0x4cdaa0,0x4d3350,0x4d35c0,0x4d3820,0x456060,0x4d0180,0x452a90,0x4d50e0,0x5600,0x8684,0x8700,0x8080};step(&p,s,dt);
}
void rx_buggy_force_stage_exact(rx_addr s,float dt){
 static const ForceStage p={0x4b32a0,0x4b8ba0,0x4b8e10,0x4b9070,0x4b9840,0x4b59d0,0x4b54c0,0x4bad20,0x55f0,0x8674,0x86f0,0x8070};step(&p,s,dt);
}
void rx_truck_force_stage_exact(rx_addr s,float dt){
 static const ForceStage p={0x450e20,0x455200,0x455470,0x4556d0,0x456060,0x452fa0,0x452a90,0x457070,0x5600,0x868c,0x8700,0x8080};step(&p,s,dt);
}
