/* Complete handwritten Buggy004b9070, including backwards mode0 tail9787.
 * Distinct original rear-only distribution, not a UTV layout alias. */
#include "buggy_engine.h"
#include "fx_util.h"
#include "phys_vec.h"
static float constant(rx_addr p){return F32(p,0);}
static float cap_zero(float x,float maximum){if(x>=maximum)x=maximum;if(x<=0)x=0;return x;}
static float curve(rx_addr b,rx_addr table,rx_addr ra,float input){
 fx_set_reg(FX_ESI,table);SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_buggy_engine_dependency(0x438ee0,b,input,0);
}
static float retained_curve(rx_addr b,rx_addr ra,float input){
 SU32(b-4,0,ra);rx_set_sp(b-4);return rx_buggy_engine_dependency(0x438ee0,b,input,0);
}
static float lerp_frame(rx_addr b,rx_addr ra,float t){
 SU32(b-12,0,ra);rx_set_sp(b-12);return rx_buggy_engine_dependency(0x430810,b-8,0,t);
}
static void rear_inertias(rx_addr s,float scalar){
 rx_v4 added=v4(scalar,0,0,0),left=v4_add(added,rx_ld4(s+0x3a30));
 rx_st4(s+0x930,left);rx_st4(s+0x940,v4_div(rx_ld4(0xd61650),left));
 rx_v4 right=v4_add(added,rx_ld4(s+0x3a30));
 rx_st4(s+0x9d0,right);rx_st4(s+0x9e0,v4_div(rx_ld4(0xd61650),right));
}
void rx_buggy_engine_step_exact(rx_addr s,float dt){
 fx_frame f;fx_frame_enter(&f,0x5c,16,FX_ESI);rx_addr b=f.E;
 float maximum=phys_div(F32(s,0x4d4c),constant(0x9774d0));
 float idle=phys_div(F32(s,0x4d94),constant(0x9774d0));
 rx_st4(b+0x40,v4_mul(v4_add(rx_ld4(s+0x9c0),rx_ld4(s+0x920)),rx_ld4(0xd615d0)));
 SF32(b,0x3c,idle);SF32(b,0x2c,0);SF32(b,0x34,0);SF32(b,0x30,maximum);
 float speed=cap_zero(F32(s,0x864c),maximum);SF32(b,0x14,speed);SF32(s,0x864c,speed);
 float fraction=phys_div(phys_sub(speed,idle),phys_sub(maximum,idle));
 if(fraction>=constant(0x931f24))fraction=constant(0x931f24);else if(fraction<=0)fraction=0;
 float throttle=F32(s,0x55f0);SF32(b,0x20,throttle);
 SF32(s,0x8668,phys_mul(phys_add(phys_sub(throttle,fraction),constant(0x931f24)),constant(0x9322cc)));
 float blend=phys_div(phys_sub(F32(s,0x4f9c),F32(s,0x8648)),F32(s,0x4f9c));
 if(blend>=constant(0x931f24))blend=constant(0x931f24);else if(blend<=0)blend=0;
 SF32(b,8,blend);SU32(b-4,0,0x4b918b);rx_set_sp(b-4);
 rx_f80 powered=rx_buggy_engine_power_dependency(b,fx_ld(b+8),rx_f80_from_f64(rx_bd(rx_rd64(0x977868))));
 SF32(b,0x10,fx_f32(powered));fx_x87_copy(b+8,b+0x10);
 float base=phys_div(F32(s,0x4cbc),constant(0x9774d0));blend=F32(b,8);speed=base;
 /* Original FCOMPI zero,blend: unordered takes the same arm as JB. */
 if(!(0>=blend)){
  speed=F32(b,0x14);
  if(!(blend>=constant(0x931f24)))speed=phys_add(phys_mul(phys_sub(speed,base),blend),base);
 }
 SF32(s,0x864c,speed);speed=cap_zero(speed,F32(b,0x30));SF32(s,0x864c,speed);
 uint32_t gear=U32(s,0x8650);fx_set_reg(FX_EAX,gear);SF32(b,0x1c,speed);SF32(b,0x24,F32(s,0x8654+4*gear));
 SF32(b,0xc,curve(b,s+0x4c1c,0x4b9228,speed));
 SF32(b,0x28,F32(s,0x4f54));SF32(b,0x18,F32(s,0x4ddc));
 float grip=curve(b,s+0x4e24,0x4b925b,F32(b,0x1c));
 float resistance=phys_sub(constant(0x9774a8),phys_mul(grip,phys_mul(phys_mul(F32(b,0x28),F32(b,0x24)),F32(b,0x18))));
 rx_v4 rotation=v4_splat(F32(s,0x920));rx_st4(b+0x50,v4_cmplt(rotation,rx_ld4(0xd61670)));
 fx_set_reg(FX_ECX,U32(b,0x54));fx_set_reg(FX_EDX,U32(b,0x58)+U32(b,0x54)+U32(b,0x50));
 SF32(b,0x10,resistance);SF32(b,0x14,F32(b,0x20));
 if(fx_reg(FX_EDX)==0xfffffffdu){resistance=phys_sub(constant(0x9774a8),resistance);SF32(b,0x10,resistance);}
 float brake=F32(s,0x55f4);
 if(brake>0){
  SF32(b,0x24,F32(s,0x8654));SF32(b,0x14,brake);
  rx_st4(b+0x50,v4_cmplt(rotation,v4_splat(constant(0x977860))));
  fx_set_reg(FX_EAX,U32(b,0x54));fx_set_reg(FX_ECX,U32(b,0x58)+U32(b,0x54)+U32(b,0x50));
  SF32(b,0xc,fx_reg(FX_ECX)==0xfffffffdu?0:phys_sub(constant(0x9774a8),F32(s,0x4d04)));
 }
 float wheel=0,counter=0;uint32_t mode=U32(s,0x56ec);fx_set_reg(FX_EAX,mode<=2?0:mode-2);
 if(mode==2){
  float friction=retained_curve(b,0x4b9376,F32(b,0x1c));
  friction=phys_sub(constant(0x9774a8),phys_mul(friction,F32(b,0x18)));
  rear_inertias(s,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  float acceleration=phys_add(phys_mul(phys_sub(F32(b,0xc),friction),F32(b,0x14)),friction);
  speed=phys_add(phys_div(phys_mul(acceleration,dt),F32(s,0x3a84)),F32(s,0x864c));SF32(s,0x864c,speed);
 }else if(mode==1){
  float friction=retained_curve(b,0x4b9443,F32(b,0x1c));
  friction=phys_sub(constant(0x9774a8),phys_mul(friction,F32(b,0x18)));
  float ratio=F32(b,0x24),power=F32(b,0xc);
  float effort=phys_add(phys_mul(phys_sub(phys_mul(phys_mul(ratio,constant(0x92e330)),power),friction),F32(b,0x20)),friction);
  float clutch=F32(s,0x5640);SF32(b,0x18,clutch);
  float coupling=phys_sub(constant(0x9774a8),phys_mul(clutch,effort));
  SF32(b,0x20,phys_sub(constant(0x9774a8),effort));
  SF32(b,0x28,phys_mul(phys_add(phys_div(phys_mul(effort,dt),F32(s,0x3a84)),F32(b,0x1c)),F32(b,0x28)));
  float demand=phys_add(phys_mul(phys_sub(phys_mul(power,ratio),F32(b,0x10)),F32(b,0x14)),F32(b,0x10));
  SF32(b,0xc,coupling);SF32(b,0x38,F32(s,0x3a84));SF32(b,0x10,demand);
  if(0>demand){demand=0;SF32(b,0x10,demand);}
  fx_x87_copy(b-8,b+0x20);SF32(b-4,0,phys_sub(constant(0x9774a8),demand));
  SF32(b,0x2c,lerp_frame(b,0x4b9524,F32(b,8)));
  fx_x87_copy(b-8,b+0xc);SF32(b-4,0,phys_sub(constant(0x9774a8),phys_mul(F32(b,0x18),F32(b,0x10))));
  SF32(b,0x34,lerp_frame(b,0x4b955a,F32(b,8)));
  fx_x87_copy(b-8,b+0x28);SF32(b-4,0,phys_mul(F32(b,0x24),F32(b,0x40)));
  speed=lerp_frame(b,0x4b9584,F32(b,8));rx_set_sp(b);SF32(s,0x864c,speed);SF32(s,0x864c,cap_zero(speed,F32(b,0x30)));
  rx_st4(b+0x40,v4_zero());float inertia=phys_mul(phys_add(F32(b,0x38),F32(s,0x3acc)),constant(0x9322cc));
  SF32(b-4,0,inertia);SF32(b-8,0,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  inertia=lerp_frame(b,0x4b95fb,F32(b,8));rx_set_sp(b);rear_inertias(s,inertia);
  wheel=F32(b,0x2c);counter=F32(b,0x34);
 }else if(mode==0){
  /* Original9787..9838 tail returns backwards to9669, not an early exit. */
  float effort=phys_add(phys_mul(phys_sub(phys_mul(F32(b,0xc),F32(b,0x24)),F32(b,0x10)),F32(b,0x14)),F32(b,0x10));
  counter=phys_sub(constant(0x9774a8),phys_mul(F32(s,0x5640),effort));
  speed=phys_mul(F32(b,0x24),F32(b,0x40));
  rear_inertias(s,phys_add(phys_mul(F32(s,0x3acc),constant(0x9322cc)),F32(s,0x3a84)));
  wheel=phys_sub(constant(0x9774a8),effort);SF32(s,0x864c,speed);
 }else{wheel=F32(b,0x2c);counter=F32(b,0x34);}
 speed=F32(s,0x864c);if(speed>=F32(b,0x30))speed=F32(b,0x30);if(F32(b,0x3c)>=speed)speed=F32(b,0x3c);SF32(s,0x864c,speed);
 float half=constant(0x9322cc);
 rx_v4 wheelTorque=v4(phys_mul(phys_sub(constant(0x9774a8),wheel),half),0,0,0),rootTorque=v4(phys_mul(counter,half),0,0,0);
 /* Rear pair only. When both actual contact flags are0, the original applies
  * each rear wheel/root pair directly; no front or angular helper exists. */
 if(U32(s,0x8410)){
  rx_st4(s+0x960,v4_add(rx_ld4(s+0x960),wheelTorque));rx_st4(s+0x780,v4_add(rootTorque,rx_ld4(s+0x780)));
 }
 if(U32(s,0x8560)){
  rx_st4(s+0xa00,v4_add(wheelTorque,rx_ld4(s+0xa00)));rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),rootTorque));
 }
 if(!U32(s,0x8410)&&!U32(s,0x8560)){
  rx_st4(s+0x960,v4_add(rx_ld4(s+0x960),wheelTorque));rx_st4(s+0x780,v4_add(rootTorque,rx_ld4(s+0x780)));
  rx_st4(s+0xa00,v4_add(wheelTorque,rx_ld4(s+0xa00)));rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),rootTorque));
 }
 fx_frame_leave(&f);
}
