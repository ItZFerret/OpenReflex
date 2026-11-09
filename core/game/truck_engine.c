/* Complete handwritten004556d0, physical2437B including backwards455f88/45603e
 * tails. Truck owns a distinct reaction/oscillation tail; no family alias. */
#include "truck_engine.h"
#include "fx_util.h"
#include "phys_vec.h"
static float constant(rx_addr a){return F32(a,0);}
static float cap_zero(float x,float max){if(x>=max)x=max;if(x<=0)x=0;return x;}
static float curve(rx_addr b,rx_addr table,rx_addr ra,float x){fx_set_reg(FX_ESI,table);SU32(b-4,0,ra);rx_set_sp(b-4);return rx_truck_engine_dependency(0x438ee0,b,x,0);}
static float retained_curve(rx_addr b,rx_addr ra,float x){SU32(b-4,0,ra);rx_set_sp(b-4);return rx_truck_engine_dependency(0x438ee0,b,x,0);}
static float lerp_frame(rx_addr b,rx_addr ra,float t){SU32(b-12,0,ra);rx_set_sp(b-12);return rx_truck_engine_dependency(0x430810,b-8,0,t);}
static void rear_inertias(rx_addr s,float inertia){
 rx_v4 added=v4(inertia,0,0,0),left=v4_add(added,rx_ld4(s+0x3a30));rx_st4(s+0x930,left);rx_st4(s+0x940,v4_div(rx_ld4(0xd61650),left));
 rx_v4 right=v4_add(added,rx_ld4(s+0x3a30));rx_st4(s+0x9d0,right);rx_st4(s+0x9e0,v4_div(rx_ld4(0xd61650),right));
}
void rx_truck_engine_step_exact(rx_addr s,float dt){
 fx_frame f;fx_frame_enter(&f,0x5c,16,FX_ESI);rx_addr b=f.E;
 float maximum=phys_div(F32(s,0x4d4c),constant(0x9774d0)),idle=phys_div(F32(s,0x4d94),constant(0x9774d0));
 rx_st4(b+0x50,v4_mul(v4_add(rx_ld4(s+0x9c0),rx_ld4(s+0x920)),rx_ld4(0xd615d0)));
 SF32(b,0x1c,idle);SF32(b,0x30,0);SF32(b,0x34,0);SF32(b,0x38,maximum);
 float speed=cap_zero(F32(s,0x865c),maximum);SF32(b,0x14,speed);SF32(s,0x865c,speed);
 float fraction=phys_div(phys_sub(speed,idle),phys_sub(maximum,idle));
 if(fraction>=constant(0x931f24))fraction=constant(0x931f24);else if(fraction<=0)fraction=0;
 float throttle=F32(s,0x5600);SF32(b,0x24,throttle);
 SF32(s,0x867c,phys_mul(phys_add(phys_sub(throttle,fraction),constant(0x931f24)),constant(0x9322cc)));
 float blend=phys_div(phys_sub(F32(s,0x4f9c),F32(s,0x8658)),F32(s,0x4f9c));
 if(blend>=constant(0x931f24))blend=constant(0x931f24);else if(blend<=0)blend=0;
 SF32(b,0xc,blend);SU32(b-4,0,0x4557eb);rx_set_sp(b-4);
 rx_f80 powered=rx_truck_engine_power_dependency(b,fx_ld(b+0xc),rx_f80_from_f64(rx_bd(rx_rd64(0x977868))));
 SF32(b,8,fx_f32(powered));fx_x87_copy(b+0xc,b+8);
 float base=phys_div(F32(s,0x4cbc),constant(0x9774d0));blend=F32(b,0xc);speed=base;
 /* FCOMPI zero,blend/JB includes unordered. */
 if(!(0>=blend)){speed=F32(b,0x14);if(!(blend>=constant(0x931f24)))speed=phys_add(phys_mul(phys_sub(speed,base),blend),base);}
 SF32(s,0x865c,speed);speed=cap_zero(speed,F32(b,0x38));SF32(s,0x865c,speed);
 uint32_t gear=U32(s,0x8664);fx_set_reg(FX_EAX,gear);SF32(b,0x20,speed);SF32(b,0x2c,F32(s,0x8668+4*gear));
 SF32(b,0x10,curve(b,s+0x4c1c,0x455888,speed));SF32(b,0x28,F32(s,0x4f54));SF32(b,0x18,F32(s,0x4ddc));
 float friction=curve(b,s+0x4e24,0x4558bb,F32(b,0x20));
 float resistance=phys_sub(constant(0x9774a8),phys_mul(friction,phys_mul(phys_mul(F32(b,0x28),F32(b,0x2c)),F32(b,0x18))));
 rx_v4 rotation=v4_splat(F32(s,0x920));rx_st4(b+0x40,v4_cmplt(rotation,rx_ld4(0xd61670)));
 fx_set_reg(FX_ECX,U32(b,0x44));fx_set_reg(FX_EDX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
 SF32(b,8,resistance);SF32(b,0x14,F32(b,0x24));
 if(fx_reg(FX_EDX)==0xfffffffdu){resistance=phys_sub(constant(0x9774a8),resistance);SF32(b,8,resistance);}
 float brake=F32(s,0x5604);
 if(brake>0){SF32(b,0x2c,F32(s,0x8668));SF32(b,0x14,brake);
  rx_st4(b+0x40,v4_cmplt(rotation,v4_splat(constant(0x977860))));fx_set_reg(FX_EAX,U32(b,0x44));fx_set_reg(FX_ECX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
  SF32(b,0x10,fx_reg(FX_ECX)==0xfffffffdu?0:phys_sub(constant(0x9774a8),F32(s,0x4d04)));}
 float wheel=0,counter=0;uint32_t mode=U32(s,0x56fc);fx_set_reg(FX_EAX,mode<=2?0:mode-2);
 if(mode==2){
  float drag=retained_curve(b,0x4559d6,F32(b,0x20));drag=phys_sub(constant(0x9774a8),phys_mul(drag,F32(b,0x18)));
  rear_inertias(s,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  float acceleration=phys_add(phys_mul(phys_sub(F32(b,0x10),drag),F32(b,0x14)),drag);
  speed=phys_add(phys_div(phys_mul(acceleration,dt),F32(s,0x3a84)),F32(s,0x865c));SF32(s,0x865c,speed);
 }else if(mode==1){
  float drag=retained_curve(b,0x455aa7,F32(b,0x20));drag=phys_sub(constant(0x9774a8),phys_mul(drag,F32(b,0x18)));
  float ratio=F32(b,0x2c),power=F32(b,0x10);
  float effort=phys_add(phys_mul(phys_sub(phys_mul(phys_mul(ratio,constant(0x92e330)),power),drag),F32(b,0x24)),drag);
  float clutch=F32(s,0x5650);SF32(b,0x18,clutch);
  float coupling=phys_sub(constant(0x9774a8),phys_mul(clutch,effort));SF32(b,0x24,phys_sub(constant(0x9774a8),effort));
  SF32(b,0x28,phys_mul(phys_add(phys_div(phys_mul(effort,dt),F32(s,0x3a84)),F32(b,0x20)),F32(b,0x28)));
  float demand=phys_add(phys_mul(phys_sub(phys_mul(power,ratio),F32(b,8)),F32(b,0x14)),F32(b,8));
  SF32(b,0x10,coupling);SF32(b,0x3c,F32(s,0x3a84));SF32(b,8,demand);if(0>demand){demand=0;SF32(b,8,demand);}
  fx_x87_copy(b-8,b+0x24);SF32(b-4,0,phys_sub(constant(0x9774a8),demand));SF32(b,0x30,lerp_frame(b,0x455b88,F32(b,0xc)));
  fx_x87_copy(b-8,b+0x10);SF32(b-4,0,phys_sub(constant(0x9774a8),phys_mul(F32(b,0x18),F32(b,8))));SF32(b,0x34,lerp_frame(b,0x455bbe,F32(b,0xc)));
  fx_x87_copy(b-8,b+0x28);SF32(b-4,0,phys_mul(F32(b,0x2c),F32(b,0x50)));
  speed=lerp_frame(b,0x455be8,F32(b,0xc));rx_set_sp(b);SF32(s,0x865c,speed);SF32(s,0x865c,cap_zero(speed,F32(b,0x38)));
  rx_st4(b+0x50,v4_zero());float inertia=phys_mul(phys_add(F32(b,0x3c),F32(s,0x3acc)),constant(0x9322cc));
  SF32(b-4,0,inertia);SF32(b-8,0,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  inertia=lerp_frame(b,0x455c5f,F32(b,0xc));rx_set_sp(b);rear_inertias(s,inertia);wheel=F32(b,0x30);counter=F32(b,0x34);
 }else if(mode==0){
  /* Physical tail455f88..456039 rejoins455ccd; no early return. */
  float effort=phys_add(phys_mul(phys_sub(phys_mul(F32(b,0x10),F32(b,0x2c)),F32(b,8)),F32(b,0x14)),F32(b,8));
  counter=phys_sub(constant(0x9774a8),phys_mul(F32(s,0x5650),effort));
  speed=phys_mul(F32(b,0x2c),F32(b,0x50));rear_inertias(s,phys_add(phys_mul(F32(s,0x3acc),constant(0x9322cc)),F32(s,0x3a84)));
  wheel=phys_sub(constant(0x9774a8),effort);SF32(s,0x865c,speed);
 }else{wheel=F32(b,0x30);counter=F32(b,0x34);}
 speed=F32(s,0x865c);if(speed>=F32(b,0x38))speed=F32(b,0x38);if(F32(b,0x1c)>=speed)speed=F32(b,0x1c);SF32(s,0x865c,speed);
 float half=constant(0x9322cc);rx_v4 torque=v4(phys_mul(phys_sub(constant(0x9774a8),wheel),half),0,0,0),reaction=v4(phys_mul(counter,half),0,0,0);
 if(U32(s,0x8420)){rx_st4(s+0x960,v4_add(torque,rx_ld4(s+0x960)));rx_st4(s+0x780,v4_add(reaction,rx_ld4(s+0x780)));}
 if(U32(s,0x8570)){rx_st4(s+0xa00,v4_add(rx_ld4(s+0xa00),torque));rx_st4(s+0x780,v4_add(reaction,rx_ld4(s+0x780)));}
 if(!U32(s,0x8420)&&!U32(s,0x8570)){
  rx_st4(s+0x960,v4_add(torque,rx_ld4(s+0x960)));rx_st4(s+0x780,v4_add(reaction,rx_ld4(s+0x780)));
  rx_st4(s+0xa00,v4_add(rx_ld4(s+0xa00),torque));rx_st4(s+0x780,v4_add(reaction,rx_ld4(s+0x780)));}
 /* Truck-only reaction from positive engine acceleration. */
 rx_v4 drag_axis=v4_mul(v4(0,0,constant(0x9776d8),constant(0x9776d8)),rx_ld4(s+0x38b0));rx_st4(b+0x50,drag_axis);
 if(U32(s,0x56f8)){
  float acceleration=phys_div(phys_sub(F32(s,0x865c),F32(s,0x8660)),dt);
  float positive=0;if(!(0>acceleration))positive=acceleration;
  positive=phys_div(positive,constant(0x977398));float gain=constant(0x931f24);if(!(positive>gain))gain=positive;
  rx_st4(s+0x780,v4_add(v4_mul(v4_splat(gain),drag_axis),rx_ld4(s+0x780)));
 }
 /* Real cdecl Float_Fmod frame; preserve its genuine x87 return/roundtrip. */
 fx_x87_copy(b-4,0x97749c);SF32(b-8,0,phys_add(phys_mul(dt,constant(0x931d98)),F32(s,0x8680)));SU32(b-12,0,0x455e94);rx_set_sp(b-12);
 rx_f80 phase=rx_truck_engine_fmod_dependency(b-8);SF32(b,8,fx_f32(phase));fx_x87_copy(s+0x8680,b+8);rx_set_sp(b);
 float change=phys_div(phys_sub(F32(s,0x865c),F32(b,0x1c)),constant(0x9c4750));
 if(change>=constant(0x931f24))change=constant(0x931f24);else if(change<=0)change=0;SF32(b,0x1c,change);
 SU32(b-4,0,0x455ee0);rx_set_sp(b-4);float angle=rx_truck_engine_dependency(0x401000,b,F32(b,8),0);
 int32_t turns=rx_cvtt(phys_mul(angle,constant(0x9774a0)));fx_set_reg(FX_EDX,(uint32_t)turns);
 float remainder=phys_sub(angle,phys_mul((float)turns,constant(0x97749c)));
 float distance=phys_sub(constant(0x977498),remainder),square=phys_mul(distance,distance);
 float first=phys_add(phys_mul(square,constant(0x93a71c)),constant(0x93a718));
 float fourth=phys_mul(square,square),second=phys_add(phys_mul(square,constant(0x93a714)),constant(0x93a710));
 float wave=phys_add(phys_mul(first,fourth),second);
 float gain=phys_mul(phys_sub(constant(0x931f24),F32(b,0x1c)),constant(0x9cd9b4));wave=phys_mul(wave,gain);
 fx_set_reg(FX_ESI,f.in.esi);rx_st4(s+0x780,v4_add(v4_mul(v4_splat(wave),rx_ld4(b+0x50)),rx_ld4(s+0x780)));
 fx_x87_copy(s+0x8660,s+0x865c);fx_frame_leave(&f);
}
