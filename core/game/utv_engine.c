/* Complete hand C for004d3820. Original frame stores are retained because
 * curve/CRT/lerp calls consume those words and all-resident proof sees them.
 * Engine coupling modes are independent authored paths, not approximations. */
#include "utv_engine.h"
#include "fx_util.h"
#include "phys_vec.h"
static float constant(rx_addr p){return F32(p,0);}
static float cap_zero(float x,float maximum){
 /* COMISS x,max; JB keeps x on unordered. COMISS 0,x; JB also keeps NaN. */
 if(x>=maximum)x=maximum;if(x<=0)x=0;return x;
}
static float curve(rx_addr b,rx_addr s,rx_addr ra,float input){
 fx_set_reg(FX_ESI,s);SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_utv_engine_dependency(0x438ee0,b,input,0);
}
static float retained_curve(rx_addr b,rx_addr ra,float input){
 SU32(b-4,0,ra);rx_set_sp(b-4);return rx_utv_engine_dependency(0x438ee0,b,input,0);
}
static float lerp_frame(rx_addr b,rx_addr ra,float t){
 /* B-8/B-4 arguments are reused across a sequence of cdecl calls. */
 SU32(b-12,0,ra);rx_set_sp(b-12);return rx_utv_engine_dependency(0x430810,b-8,0,t);
}
static void set_inertias(rx_addr s,float scalar){
 rx_v4 v=v4(scalar,0,0,0),authored=rx_ld4(s+0x3a30);
 rx_v4 left=v4_add(v,authored);rx_st4(s+0x930,left);rx_st4(s+0x940,v4_div(rx_ld4(0xd61650),left));
 rx_v4 right=v4_add(v,rx_ld4(s+0x3a30));rx_st4(s+0x9d0,right);rx_st4(s+0x9e0,v4_div(rx_ld4(0xd61650),right));
}
static void angular(rx_addr b,rx_addr mechanism,rx_addr vector,uint32_t index,rx_addr ra,int pointer_in_eax){
 if(pointer_in_eax)fx_set_reg(FX_EAX,vector);
 fx_set_reg(FX_ECX,mechanism);SU32(b-4,0,vector);SU32(b-8,0,index);SU32(b-12,0,ra);rx_set_sp(b-12);
 (void)rx_utv_engine_dependency(0x85c9a0,b,0,0);
}
void rx_utv_engine_step_exact(rx_addr s,float dt){
 fx_frame frame;fx_frame_enter(&frame,0x8c,16,FX_ESI);rx_addr b=frame.E;
 uint32_t cookie=U32(0,0x9b45e4)^(b+4);fx_set_reg(FX_EAX,cookie);SU32(b,0x8c,cookie);
 float maximum=phys_div(F32(s,0x4d4c),constant(0x9774d0));
 float idle=phys_div(F32(s,0x4d94),constant(0x9774d0));
 rx_st4(b+0x50,v4_mul(v4_add(rx_ld4(s+0x9c0),rx_ld4(s+0x920)),rx_ld4(0xd615d0)));
 SF32(b,0x38,idle);SF32(b,0x34,0);SF32(b,0x30,0);SF32(b,0x2c,maximum);
 float speed=cap_zero(F32(s,0x865c),maximum);SF32(b,0x18,speed);SF32(s,0x865c,speed);
 float fraction=phys_div(phys_sub(speed,idle),phys_sub(maximum,idle));
 if(fraction>=constant(0x931f24))fraction=constant(0x931f24);else if(fraction<=0)fraction=0;
 float throttle=F32(s,0x5600);SF32(b,0x20,throttle);
 SF32(s,0x8678,phys_mul(phys_add(phys_sub(throttle,fraction),constant(0x931f24)),constant(0x9322cc)));
 float blend=phys_div(phys_sub(F32(s,0x4f9c),F32(s,0x8658)),F32(s,0x4f9c));
 if(blend>=constant(0x931f24))blend=constant(0x931f24);else if(blend<=0)blend=0;
 SF32(b,0xc,blend);SU32(b-4,0,0x4d394c);rx_set_sp(b-4);
 rx_f80 powered=rx_utv_engine_power_dependency(b,fx_ld(b+0xc),rx_f80_from_f64(rx_bd(rx_rd64(0x977868))));
 SF32(b,0x10,fx_f32(powered));fx_x87_copy(b+0xc,b+0x10);
 float base=phys_div(F32(s,0x4cbc),constant(0x9774d0));blend=F32(b,0xc);
 speed=base;
 if(!(0>=blend)){
  speed=F32(b,0x18);
  if(!(blend>=constant(0x931f24)))speed=phys_add(phys_mul(phys_sub(F32(b,0x18),base),blend),base);
 }
 SF32(s,0x865c,speed);speed=cap_zero(speed,F32(b,0x2c));SF32(s,0x865c,speed);
 uint32_t gear=U32(s,0x8660);fx_set_reg(FX_EAX,gear);SF32(b,8,speed);SF32(b,0x24,F32(s,0x8664+4*gear));
 SF32(b,0x14,curve(b,s+0x4c1c,0x4d39e9,speed));
 SF32(b,0x28,F32(s,0x4f54));SF32(b,0x1c,F32(s,0x4ddc));
 float grip=curve(b,s+0x4e24,0x4d3a1c,F32(b,8));
 float resistance=phys_sub(constant(0x9774a8),phys_mul(grip,phys_mul(phys_mul(F32(b,0x28),F32(b,0x24)),F32(b,0x1c))));
 rx_v4 rotation=v4_splat(F32(s,0x920));rx_v4 negative=v4_cmplt(rotation,rx_ld4(0xd61670));rx_st4(b+0x40,negative);
 fx_set_reg(FX_ECX,U32(b,0x44));fx_set_reg(FX_EDX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
 SF32(b,0x10,resistance);SF32(b,0x18,F32(b,0x20));
 if(fx_reg(FX_EDX)==0xfffffffdu){resistance=phys_sub(constant(0x9774a8),resistance);SF32(b,0x10,resistance);}
 float brake=F32(s,0x5604);
 if(brake>0){
  SF32(b,0x24,F32(s,0x8664));SF32(b,0x18,brake);
  rx_v4 reverse=v4_cmplt(rotation,v4_splat(constant(0x977860)));rx_st4(b+0x40,reverse);
  fx_set_reg(FX_EAX,U32(b,0x44));fx_set_reg(FX_ECX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
  SF32(b,0x14,fx_reg(FX_ECX)==0xfffffffdu?0:phys_sub(constant(0x9774a8),F32(s,0x4d04)));
 }
 float wheel=0,counter=0;uint32_t mode=U32(s,0x56fc);fx_set_reg(FX_EAX,mode<=2?0:mode-2);
 if(mode==2){
  float friction=retained_curve(b,0x4d3b37,F32(b,8));
  friction=phys_sub(constant(0x9774a8),phys_mul(friction,F32(b,0x1c)));
  set_inertias(s,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  float acceleration=phys_add(phys_mul(phys_sub(F32(b,0x14),friction),F32(b,0x18)),friction);
  speed=phys_add(phys_div(phys_mul(acceleration,dt),F32(s,0x3a84)),F32(s,0x865c));SF32(s,0x865c,speed);
 }else if(mode==1){
  float friction=retained_curve(b,0x4d3c08,F32(b,8));
  friction=phys_sub(constant(0x9774a8),phys_mul(friction,F32(b,0x1c)));
  float ratio=F32(b,0x24),power=F32(b,0x14);
  float effort=phys_add(phys_mul(phys_sub(phys_mul(phys_mul(ratio,constant(0x92e330)),power),friction),F32(b,0x20)),friction);
  float clutch=F32(s,0x5650);SF32(b,0x1c,clutch);
  float coupling=phys_sub(constant(0x9774a8),phys_mul(clutch,effort));
  SF32(b,0x20,phys_sub(constant(0x9774a8),effort));
  SF32(b,0x28,phys_mul(phys_add(phys_div(phys_mul(effort,dt),F32(s,0x3a84)),F32(b,8)),F32(b,0x28)));
  float demand=phys_add(phys_mul(phys_sub(phys_mul(power,ratio),F32(b,0x10)),F32(b,0x18)),F32(b,0x10));
  SF32(b,0x14,coupling);SF32(b,0x3c,F32(s,0x3a84));SF32(b,8,demand);
  if(0>demand){demand=0;SF32(b,8,demand);}
  fx_x87_copy(b-8,b+0x20);SF32(b-4,0,phys_sub(constant(0x9774a8),demand));
  SF32(b,0x34,lerp_frame(b,0x4d3ce9,F32(b,0xc)));
  fx_x87_copy(b-8,b+0x14);SF32(b-4,0,phys_sub(constant(0x9774a8),phys_mul(F32(b,0x1c),F32(b,8))));
  SF32(b,0x30,lerp_frame(b,0x4d3d1f,F32(b,0xc)));
  fx_x87_copy(b-8,b+0x28);SF32(b-4,0,phys_mul(F32(b,0x24),F32(b,0x50)));
  speed=lerp_frame(b,0x4d3d49,F32(b,0xc));rx_set_sp(b);SF32(s,0x865c,speed);
  SF32(s,0x865c,cap_zero(speed,F32(b,0x2c)));rx_st4(b+0x60,v4_zero());
  float inertia=phys_mul(phys_add(F32(b,0x3c),F32(s,0x3acc)),constant(0x9322cc));
  SF32(b-4,0,inertia);SF32(b-8,0,phys_mul(F32(s,0x3acc),constant(0x9322cc)));
  inertia=lerp_frame(b,0x4d3dc0,F32(b,0xc));rx_set_sp(b);set_inertias(s,inertia);
  wheel=F32(b,0x34);counter=F32(b,0x30);
 }else if(mode==0){
  /* Mode0 retains the current cubic/brake result; no extra curve call. */
  float effort=phys_add(phys_mul(phys_sub(phys_mul(F32(b,0x14),F32(b,0x24)),F32(b,0x10)),F32(b,0x18)),F32(b,0x10));
  SF32(b,8,effort);counter=phys_sub(constant(0x9774a8),phys_mul(F32(s,0x5650),effort));
  set_inertias(s,phys_add(phys_mul(F32(s,0x3acc),constant(0x9322cc)),F32(s,0x3a84)));
  wheel=phys_sub(constant(0x9774a8),F32(b,8));SF32(s,0x865c,phys_mul(F32(b,0x24),F32(b,0x50)));
 }else{wheel=F32(b,0x34);counter=F32(b,0x30);}
 /* Shared engine-speed bounds and wheel/contact load distribution. */
 speed=F32(s,0x865c);if(speed>=F32(b,0x2c))speed=F32(b,0x2c);if(F32(b,0x38)>=speed)speed=F32(b,0x38);SF32(s,0x865c,speed);
 float half=constant(0x9322cc),front=phys_mul(phys_add(F32(s,0x82b0),F32(s,0x8160)),half);
 float rear=phys_mul(phys_add(F32(s,0x8550),F32(s,0x8400)),half);
 wheel=phys_sub(constant(0x9774a8),wheel);
 SF32(b,0x10,half>front?half:front);
 float total=phys_add(rear,front),denominator=constant(0x931f24)>total?constant(0x931f24):total;
 float frontLoad=phys_div(F32(b,0x10),denominator);if(half>rear)rear=half;
 denominator=constant(0x931f24)>total?constant(0x931f24):total;
 float rearLoad=phys_div(rear,denominator),scale=constant(0x92ff84);
 frontLoad=phys_mul(frontLoad,scale);rearLoad=phys_mul(rearLoad,scale);
 rx_v4 frontWheel=v4(phys_mul(frontLoad,wheel),0,0,0),frontRoot=v4(phys_mul(frontLoad,counter),0,0,0);
 rx_v4 rearWheel=v4(phys_mul(rearLoad,wheel),0,0,0),rearRoot=v4(phys_mul(rearLoad,counter),0,0,0);
 rx_st4(b+0x40,frontRoot);rx_st4(b+0x60,frontWheel);rx_st4(b+0x50,rearWheel);rx_st4(b+0x70,rearRoot);
 if(U32(s,0x8180)){rx_st4(s+0x820,v4_add(rx_ld4(s+0x820),frontWheel));rx_st4(s+0x780,v4_add(frontRoot,rx_ld4(s+0x780)));}
 if(U32(s,0x82d0)){rx_st4(s+0x8c0,v4_add(frontWheel,rx_ld4(s+0x8c0)));rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),frontRoot));}
 if(U32(s,0x8420)){rx_st4(s+0x960,v4_add(rearWheel,rx_ld4(s+0x960)));rx_st4(s+0x780,v4_add(rearRoot,rx_ld4(s+0x780)));}
 if(U32(s,0x8570)){rx_st4(s+0xa00,v4_add(rearWheel,rx_ld4(s+0xa00)));rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),rearRoot));}
 if(!U32(s,0x8420)&&!U32(s,0x8570)&&!U32(s,0x8180)&&!U32(s,0x82d0)){
  fx_set_reg(FX_EDX,s+0xd0);
  angular(b,s+0xd0,b+0x50,3,0x4d4115,1);angular(b,s+0xd0,b+0x70,0,0x4d4123,0);
  angular(b,s+0xd0,b+0x50,4,0x4d4131,1);angular(b,s+0xd0,b+0x70,0,0x4d413f,0);
  angular(b,s+0xd0,b+0x60,1,0x4d414d,1);angular(b,s+0xd0,b+0x40,0,0x4d415b,0);
  angular(b,s+0xd0,b+0x60,2,0x4d4169,1);angular(b,s+0xd0,b+0x40,0,0x4d4177,0);
 }
 fx_set_reg(FX_ECX,U32(b,0x8c)^(b+4));fx_set_reg(FX_ESI,frame.in.esi);
 SU32(b,0,0x4d4186);rx_set_sp(b);(void)rx_utv_engine_dependency(0x8f9efe,b+4,0,0);fx_frame_leave(&frame);
}
