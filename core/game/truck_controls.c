/* Complete hand-C Truck0045a980 control phase from its own original assembly.
 * Distinct full-speed/local slots, mode-scaled correction and gear8664 are
 * retained alongside Truck rear-only traction. No new tuning policy. */
#include "truck_controls.h"
#include "fourwheel_controls_math.h"
static float fc(rx_addr p){return F32(p,0);}
static float call0(rx_addr b,rx_addr target,rx_addr ra,float x0,float x1,float x2){SU32(b-4,0,ra);rx_set_sp(b-4);return rx_truck_controls_dependency(target,b,x0,x1,x2);}
static float call1(rx_addr b,rx_addr target,rx_addr ra,float arg,float x0,float x1,float x2){SF32(b-4,0,arg);SU32(b-8,0,ra);rx_set_sp(b-8);return rx_truck_controls_dependency(target,b,x0,x1,x2);}
static float absolute(rx_addr b,rx_addr ra,float value){return call0(b,0x401000,ra,value,0,0);}
static float curve(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ESI,p);return call0(b,0x438ee0,ra,value,0,0);}
static float lag(rx_addr b,rx_addr p,rx_addr ra,float dt,float target){fx_set_reg(FX_EAX,p);return call1(b,0x42d6b0,ra,target,0,0,dt);}
static void setlag(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ECX,p);(void)call1(b,0x42d670,ra,fx_f32(fx_ld(0x977494)),0,value,0);}
static float signof(float x){return x>=0?fc(0x931f24):fc(0x9cd9e8);}
static float copyf(float x){return rx_bf(fx_x87_bits(rx_fb(x)));}
void rx_truck_controls_step_exact(rx_addr s,float dt){
 fx_frame frame;fx_frame_enter(&frame,0x74,16,FX_EBX,FX_ESI,FX_EDI);rx_addr b=frame.E;fx_set_reg(FX_EDI,s);
 rx_v4 velocity=rx_ld4(s+0x100),rotation=rx_ld4(s+0x730),squared=v4_mul(velocity,velocity);
 rx_v4 local=fw9_body_velocity(velocity,rotation);rx_st4(b+0x50,squared);SF32(b,0x28,phys_sqrt(fw9_sum_021(squared).v[0]));
 rx_v4 horizontal=v4_movss(v4_shuf(rx_ld4(0xd61670),velocity,0xe5),velocity);
 rx_st4(b+0x70,velocity);rx_st4(b+0x30,local);SF32(b,0x2c,phys_sqrt(fw9_sum_021(v4_mul(horizontal,horizontal)).v[0]));
 if(U32(s,0x2de0))fx_x87_copy(s+0x5618,s+0x2e70);
 else{
  SF32(b,0x1c,curve(b,s+0x35ec,0x45aaf7,F32(b,0x38)));
  float input=F32(s,0x2c),old=F32(s,0x5728);
  float difference=phys_sub(phys_mul(phys_mul(input,signof(input)),input),old);
  SF32(b,0x24,old);SF32(b,0x20,signof(difference));
  float distance=absolute(b,0x45ab5c,difference),step=phys_mul(dt,fc(0x9d78dc));SF32(b,0x18,step);
  if(!(step>distance))distance=step;
  float target=phys_add(phys_mul(distance,F32(b,0x20)),F32(b,0x24));
  float tau=F32(s,0x572c),k=phys_div(tau>dt?dt:tau,tau);SF32(s,0x5730,k);
  float filtered=phys_add(phys_mul(phys_sub(target,F32(s,0x5728)),k),F32(s,0x5728));
  SF32(s,0x5728,filtered);SF32(s,0x5618,phys_mul(filtered,F32(b,0x1c)));
  local=fw9_body_velocity(rx_ld4(s+0x100),rx_ld4(s+0x730));
  fw9_heading_result heading=fw9_heading(local,rx_ld4(s+0x100),b+0x60,b+0x40,b+0x20);
  float correction=0;
  if(heading.active){
   SF32(b,0x1c,heading.angle);SF32(b-4,0,fx_f32(fx_ld(0x9322cc)));SF32(b-8,0,0);SU32(b-12,0,0x45af44);rx_set_sp(b-12);
   float blend=rx_truck_controls_dependency(0x430810,b-8,0,0,heading.blend);correction=phys_mul(blend,F32(b,0x1c));rx_set_sp(b);
   SF32(b,0x20,U32(s,0x8700)?fc(0x9d789c):fc(0x931f24));
   SF32(b,0x24,signof(correction));correction=absolute(b,0x45af8e,correction);
   float bound=phys_mul(F32(b,0x20),fc(0x977678));if(!(bound>correction))correction=bound;correction=phys_mul(correction,F32(b,0x24));
  }
  fx_set_reg(FX_ESI,s+0x5750);float oldCorrection=F32(s,0x5750);difference=phys_sub(correction,oldCorrection);
  SF32(b,0x1c,oldCorrection);SF32(b,0x20,signof(difference));distance=absolute(b,0x45afe7,difference);step=F32(b,0x18);
  SF32(b,0x24,step>distance?distance:step);
  /* FMUL precedes pushECX here. Its FADD/FSTP after the push still addresses
   * B+1c, with retained slew distance in B+24. */
  float targetCorrection=fx_f32(fx_add(fx_mul(fx_ld(b+0x20),fx_ld(b+0x24)),fx_ld(b+0x1c)));
  SF32(b,0x1c,targetCorrection);correction=lag(b,s+0x5750,0x45b024,dt,copyf(F32(b,0x1c)));
  float angle=phys_add(correction,F32(s,0x5618));SF32(s,0x5618,angle);float bound=fc(0x977a10);
  if(!(angle>=bound)){bound=fc(0x977b10);if(!(bound>=angle))bound=angle;}SF32(s,0x5618,bound);
 }
 fx_set_reg(FX_ESI,s);(void)call0(b,0x4d4190,0x45b060,0,0,0);
 fx_x87_copy(b+0x24,s+0x28);for(unsigned i=0;i<4;i++)SU32(s,0x5608+4*i,0);
 SF32(b,0x18,F32(s,0x24));fx_set_reg(FX_ESI,s+0x5700);
 float front=lag(b,s+0x5700,0x45b0af,dt,copyf(F32(b,0x18)));fx_set_reg(FX_EBX,s+0x5714);SF32(b,0x20,front);
 float rear=lag(b,s+0x5714,0x45b0cf,dt,copyf(F32(b,0x24)));SF32(b,0x1c,rear);
 if(F32(b,0x20)>F32(b,0x18)){setlag(b,s+0x5700,0x45b0f7,F32(b,0x18));SF32(b,0x20,F32(b,0x18));}
 if(F32(b,0x1c)>F32(b,0x24)){setlag(b,s+0x5714,0x45b125,F32(b,0x24));SF32(b,0x1c,F32(b,0x24));}
 SF32(b,0x18,phys_mul(F32(s,0x3b5c),F32(b,0x20)));
 const rx_addr positiveRA[]={0x45b168,0x45b1bc,0x45b22a,0x45b27e},negativeRA[]={0x45b195,0x45b1e9,0x45b257,0x45b2ab};
 for(unsigned i=0;i<4;i++){
  if(i==2)SF32(b,0x18,phys_mul(F32(s,0x3c44),F32(b,0x1c)));
  float spin=F32(s,0x85c0+4*i);int positive=spin>fc(0x92e12c);float value=positive?spin:phys_sub(fc(0x9774a8),spin);
  value=curve(b,s+(i<2?0x3ba4:0x3c8c),positive?positiveRA[i]:negativeRA[i],value);value=phys_mul(value,F32(b,0x18));if(positive)value=phys_sub(fc(0x9774a8),value);SF32(s,0x5608+4*i,value);
 }
 SU32(s,0x5600,0);if(!(F32(s,0x28)>fc(0x9322ac)))fx_x87_copy(s+0x5600,s+0x1c);SU32(s,0x5604,0);
 if(F32(s,0x20)>0&&fc(0x935890)>F32(b,0x38)&&!(F32(s,0x28)>fc(0x9322ac))){SU32(s,0x5600,0);SF32(s,0x5604,F32(s,0x20));}
 fx_set_reg(FX_EBX,0);
 if(U32(s,0x2eb0)){
  if(U32(s,0x56f0)){uint32_t gear=U32(s,0x8664)+1;if((int32_t)gear>=4)gear=4;fx_set_reg(FX_EAX,gear);SU32(s,0x8664,gear);SU32(s,0x56f0,0);}
  if(U32(s,0x56f4)){uint32_t gear=U32(s,0x8664)-1,mask=((int32_t)gear<=0?1u:0u)-1;fx_set_reg(FX_ECX,mask);gear&=mask;fx_set_reg(FX_EAX,gear);SU32(s,0x56f4,0);SU32(s,0x8664,gear);}
 }
 fx_set_reg(FX_ESI,U32(s,0x48));SU32(s,0x56f8,fx_reg(FX_ESI));float forwardAbs=absolute(b,0x45b387,F32(b,0x38));
 uint32_t idle=fc(0x9322cc)>forwardAbs&&0>F32(s,0x5600)&&0>F32(s,0x5604);fx_set_reg(FX_EAX,idle);
 if(fx_reg(FX_ESI)||idle){float t=phys_div(phys_sub(F32(b,0x28),fc(0x935890)),fc(0x935aac));if(t>fc(0x931f24))t=fc(0x931f24);SU32(s,0x56fc,2);SF32(s,0x8658,phys_mul(F32(s,0x4f9c),t));}
 else if(0>F32(s,0x8658)||(!U32(s,0x8420)&&!U32(s,0x8570))){SU32(s,0x56fc,0);SF32(s,0x8658,fc(0x9cd9dc));}
 else{SU32(s,0x56fc,1);SF32(s,0x8658,phys_sub(F32(s,0x8658),dt));}
 if(U32(s,0x8700))SF32(b,0x18,F32(s,0x4ec4));
 else{
  SF32(b,0x18,F32(s,0x4f0c));SF32(b,0x1c,0);rx_addr wheel=0;
  if(!U32(s,0x83e4)&&U32(s,0x8420))wheel=s+0x83a0;else if(!U32(s,0x8534)&&U32(s,0x8570))wheel=s+0x84f0;
  if(wheel){rx_st4(b+0x30,rx_ld4(wheel));float slip=absolute(b,0x45b4b5,F32(b,0x30));
   if(slip>fc(0x931f24)){slip=phys_div(phys_sub(slip,fc(0x931f24)),fc(0x92e134));float value=fc(0x931f24);if(!(slip>=value)){value=slip;if(0>=slip)value=0;}SF32(b,0x1c,value);}
  }
  float steer=absolute(b,0x45b4f9,F32(s,0x56e0)),factor=F32(b,0x1c);if(!(factor>steer))factor=steer;factor=phys_sub(fc(0x931f24),factor);if(fc(0x92ff84)>factor)factor=fc(0x92ff84);
  SF32(b,0x18,phys_mul(factor,F32(b,0x18)));fx_set_reg(FX_EAX,U32(s,0x8420));
  const fw9_contact rearWheels[]={{0x8420,0x83a0},{0x8570,0x84f0}};
  if(fw9_contact_average(s,rearWheels,2,b+0x40)){
   float fraction=phys_div(absolute(b,0x45b5a6,F32(b,0x48)),fc(0x9cc37c));if(fraction>fc(0x931f24))fraction=fc(0x931f24);
   factor=phys_sub(fc(0x931f24),fraction);float bound=fc(0x931f24);if(!(factor>=bound)){bound=fc(0x931d94);if(!(bound>=factor))bound=factor;}SF32(b,0x18,phys_mul(bound,F32(b,0x18)));
  }
 }
 fx_set_reg(FX_ESI,s+0x5764);float traction=lag(b,s+0x5764,0x45b605,dt,copyf(F32(b,0x18)));SF32(s,0x5650,traction);SU32(s,0x5600,0);
 float throttle=0;if(!(F32(s,0x28)>fc(0x9322ac)))throttle=call0(b,0x42c1f0,0x45b638,F32(s,0x1c),fc(0x931f24),0);SF32(s,0x5600,throttle);
 if(U32(s,0x8700)){fx_x87_copy(s+0x5650,s+0x4ec4);setlag(b,s+0x5764,0x45b66d,F32(s,0x4f0c));}
 else{SF32(b,0x20,F32(s,0x4f0c));float a=absolute(b,0x45b68a,F32(s,0x56e0));a=phys_sub(fc(0x931f24),a);if(fc(0x92ff84)>a)a=fc(0x92ff84);traction=lag(b,s+0x5764,0x45b6c1,dt,phys_mul(a,F32(b,0x20)));SF32(s,0x5650,traction);}
 for(unsigned i=0;i<4;i++)fx_x87_copy(s+0x56e0+4*i,s+0x2c+4*i);fx_set_reg(FX_EBX,s+0x440c);
 const uint32_t parameter[]={0x440c,0x454c,0x48ac,0x49ec,0x440c,0x454c,0x48ac,0x49ec};
 const uint32_t output[]={0x8130,0x8120,0x83d0,0x83c0,0x8280,0x8270,0x8520,0x8510};
 const rx_addr sites[]={0x45b700,0x45b71c,0x45b738,0x45b754,0x45b76c,0x45b788,0x45b7a4,0x45b7c0};
 for(unsigned i=0;i<8;i++)rx_st4(s+output[i],v4_splat(curve(b,s+parameter[i],sites[i],F32(b,0x28))));
 rx_st4(s+0x5640,v4_sub(v4_zero(),fw9_normalize_squared(rx_ld4(b+0x70),rx_ld4(b+0x50))));
 if(U32(s,0x8700)){
  float strength=curve(b,s+0x34ac,0x45b847,F32(b,0x28));rx_st4(s+0x5640,v4_mul(v4_splat(strength),rx_ld4(s+0x5640)));rx_st4(b+0x70,v4_splat(fc(0x92e12c)));
  float lift=curve(b,s+0x354c,0x45b87e,F32(b,0x2c));rx_v4 addition=v4_splat(phys_sub(fc(0x9774a8),lift));addition=v4_shuf(addition,rx_ld4(b+0x70),0xee);addition=v4_movss(addition,rx_ld4(b+0x70));rx_st4(s+0x5640,v4_add(addition,rx_ld4(s+0x5640)));
 }else{float strength=curve(b,s+0x340c,0x45b8c3,F32(b,0x28));rx_st4(s+0x5640,v4_mul(v4_splat(strength),rx_ld4(s+0x5640)));}
 fx_frame_leave(&frame);
}
