/* Complete hand-C Buggy004be4c0 control phase from its own original assembly.
 * Layout, steering limits, retained slew increment and rear-only traction are
 * deliberately separate from the accepted UTV owner. No new tuning policy. */
#include "buggy_controls.h"
#include "fourwheel_controls_math.h"
static float fc(rx_addr p){return F32(p,0);}
static float call0(rx_addr b,rx_addr target,rx_addr ra,float x0,float x1,float x2){SU32(b-4,0,ra);rx_set_sp(b-4);return rx_buggy_controls_dependency(target,b,x0,x1,x2);}
static float call1(rx_addr b,rx_addr target,rx_addr ra,float arg,float x0,float x1,float x2){SF32(b-4,0,arg);SU32(b-8,0,ra);rx_set_sp(b-8);return rx_buggy_controls_dependency(target,b,x0,x1,x2);}
static float absolute(rx_addr b,rx_addr ra,float value){return call0(b,0x401000,ra,value,0,0);}
static float curve(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ESI,p);return call0(b,0x438ee0,ra,value,0,0);}
static float lag(rx_addr b,rx_addr p,rx_addr ra,float dt,float target){fx_set_reg(FX_EAX,p);return call1(b,0x42d6b0,ra,target,0,0,dt);}
static void setlag(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ECX,p);(void)call1(b,0x42d670,ra,fx_f32(fx_ld(0x977494)),0,value,0);}
static float signof(float x){return x>=0?fc(0x931f24):fc(0x9cd9e8);}
static float copyf(float x){return rx_bf(fx_x87_bits(rx_fb(x)));}
void rx_buggy_controls_step_exact(rx_addr s,float dt){
 fx_frame frame;fx_frame_enter(&frame,0x74,16,FX_EBX,FX_ESI,FX_EDI);rx_addr b=frame.E;fx_set_reg(FX_EDI,s);
 rx_v4 velocity=rx_ld4(s+0x100),rotation=rx_ld4(s+0x730),squared=v4_mul(velocity,velocity);
 rx_v4 local=fw9_body_velocity(velocity,rotation);rx_st4(b+0x50,squared);SF32(b,0x24,phys_sqrt(fw9_sum_021(squared).v[0]));
 rx_v4 horizontal=v4_movss(v4_shuf(rx_ld4(0xd61670),velocity,0xe5),velocity);
 rx_st4(b+0x70,velocity);rx_st4(b+0x30,local);SF32(b,0x2c,phys_sqrt(fw9_sum_021(v4_mul(horizontal,horizontal)).v[0]));
 if(U32(s,0x2de0))fx_x87_copy(s+0x5608,s+0x2e70);
 else{
  SF32(b,0x1c,curve(b,s+0x35ec,0x4be637,F32(b,0x38)));
  float input=F32(s,0x2c),old=F32(s,0x5718);
  float difference=phys_sub(phys_mul(phys_mul(input,signof(input)),input),old);
  SF32(b,0x28,old);SF32(b,0x20,signof(difference));
  float distance=absolute(b,0x4be69c,difference),step=phys_mul(dt,fc(0x9d78dc));SF32(b,0x18,step);
  if(!(step>distance))distance=step;
  float target=phys_add(phys_mul(distance,F32(b,0x20)),F32(b,0x28));
  float tau=F32(s,0x571c),k=phys_div(tau>dt?dt:tau,tau);SF32(s,0x5720,k);
  float filtered=phys_add(phys_mul(phys_sub(target,F32(s,0x5718)),k),F32(s,0x5718));
  SF32(s,0x5718,filtered);SF32(s,0x5608,phys_mul(filtered,F32(b,0x1c)));
  local=fw9_body_velocity(rx_ld4(s+0x100),rx_ld4(s+0x730));
  fw9_heading_result heading=fw9_heading(local,rx_ld4(s+0x100),b+0x60,b+0x40,b+0x20);
  float correction=0;
  if(heading.active){
   SF32(b,0x1c,heading.angle);SF32(b-4,0,fx_f32(fx_ld(0x9322cc)));SF32(b-8,0,0);SU32(b-12,0,0x4bea84);rx_set_sp(b-12);
   float blend=rx_buggy_controls_dependency(0x430810,b-8,0,0,heading.blend);correction=phys_mul(blend,F32(b,0x1c));rx_set_sp(b);
   SF32(b,0x20,signof(correction));correction=absolute(b,0x4beab3,correction);
   float bound=fc(0x9779f4);if(!(bound>correction))correction=bound;correction=phys_mul(correction,F32(b,0x20));
  }
  fx_set_reg(FX_ESI,s+0x5740);float oldCorrection=F32(s,0x5740);difference=phys_sub(correction,oldCorrection);
  SF32(b,0x1c,oldCorrection);SF32(b,0x20,signof(difference));distance=absolute(b,0x4beb06,difference);step=F32(b,0x18);
  SF32(b,0x28,step>distance?distance:step);
  /* FMUL precedes pushECX here. Its FADD/FSTP after the push still addresses
   * B+1c, unlike UTV's target intermediate at B+10. */
  float targetCorrection=fx_f32(fx_add(fx_mul(fx_ld(b+0x20),fx_ld(b+0x28)),fx_ld(b+0x1c)));
  SF32(b,0x1c,targetCorrection);correction=lag(b,s+0x5740,0x4beb43,dt,copyf(F32(b,0x1c)));
  float angle=phys_add(correction,F32(s,0x5608));SF32(s,0x5608,angle);float bound=fc(0x977a10);
  if(!(angle>=bound)){bound=fc(0x977b10);if(!(bound>=angle))bound=angle;}SF32(s,0x5608,bound);
 }
 fx_set_reg(FX_ESI,s);(void)call0(b,0x4b9cb0,0x4beb7f,0,0,0);
 fx_x87_copy(b+0x28,s+0x28);for(unsigned i=0;i<4;i++)SU32(s,0x55f8+4*i,0);
 SF32(b,0x18,F32(s,0x24));fx_set_reg(FX_ESI,s+0x56f0);
 float front=lag(b,s+0x56f0,0x4bebce,dt,copyf(F32(b,0x18)));fx_set_reg(FX_EBX,s+0x5704);SF32(b,0x20,front);
 float rear=lag(b,s+0x5704,0x4bebee,dt,copyf(F32(b,0x28)));SF32(b,0x1c,rear);
 if(F32(b,0x20)>F32(b,0x18)){setlag(b,s+0x56f0,0x4bec16,F32(b,0x18));SF32(b,0x20,F32(b,0x18));}
 if(F32(b,0x1c)>F32(b,0x28)){setlag(b,s+0x5704,0x4bec44,F32(b,0x28));SF32(b,0x1c,F32(b,0x28));}
 SF32(b,0x18,phys_mul(F32(s,0x3b5c),F32(b,0x20)));
 const rx_addr positiveRA[]={0x4bec87,0x4becdb,0x4bed49,0x4bed9d},negativeRA[]={0x4becb4,0x4bed08,0x4bed76,0x4bedca};
 for(unsigned i=0;i<4;i++){
  if(i==2)SF32(b,0x18,phys_mul(F32(s,0x3c44),F32(b,0x1c)));
  float spin=F32(s,0x85b0+4*i);int positive=spin>fc(0x92e12c);float value=positive?spin:phys_sub(fc(0x9774a8),spin);
  value=curve(b,s+(i<2?0x3ba4:0x3c8c),positive?positiveRA[i]:negativeRA[i],value);value=phys_mul(value,F32(b,0x18));if(positive)value=phys_sub(fc(0x9774a8),value);SF32(s,0x55f8+4*i,value);
 }
 SU32(s,0x55f0,0);if(!(F32(s,0x28)>fc(0x9322ac)))fx_x87_copy(s+0x55f0,s+0x1c);SU32(s,0x55f4,0);
 if(F32(s,0x20)>0&&fc(0x935890)>F32(b,0x38)&&!(F32(s,0x28)>fc(0x9322ac))){SU32(s,0x55f0,0);SF32(s,0x55f4,F32(s,0x20));}
 fx_set_reg(FX_EBX,0);
 if(U32(s,0x2eb0)){
  if(U32(s,0x56e0)){uint32_t gear=U32(s,0x8650)+1;if((int32_t)gear>=4)gear=4;fx_set_reg(FX_EAX,gear);SU32(s,0x8650,gear);SU32(s,0x56e0,0);}
  if(U32(s,0x56e4)){uint32_t gear=U32(s,0x8650)-1,mask=((int32_t)gear<=0?1u:0u)-1;fx_set_reg(FX_ECX,mask);gear&=mask;fx_set_reg(FX_EAX,gear);SU32(s,0x56e4,0);SU32(s,0x8650,gear);}
 }
 fx_set_reg(FX_ESI,U32(s,0x48));SU32(s,0x56e8,fx_reg(FX_ESI));float forwardAbs=absolute(b,0x4beea6,F32(b,0x38));
 uint32_t idle=fc(0x9322cc)>forwardAbs&&0>F32(s,0x55f0)&&0>F32(s,0x55f4);fx_set_reg(FX_EAX,idle);
 if(fx_reg(FX_ESI)||idle){float t=phys_div(phys_sub(F32(b,0x24),fc(0x935890)),fc(0x935aac));if(t>fc(0x931f24))t=fc(0x931f24);SU32(s,0x56ec,2);SF32(s,0x8648,phys_mul(F32(s,0x4f9c),t));}
 else if(0>F32(s,0x8648)||(!U32(s,0x8410)&&!U32(s,0x8560))){SU32(s,0x56ec,0);SF32(s,0x8648,fc(0x9cd9dc));}
 else{SU32(s,0x56ec,1);SF32(s,0x8648,phys_sub(F32(s,0x8648),dt));}
 if(U32(s,0x86f0))SF32(b,0x18,F32(s,0x4ec4));
 else{
  SF32(b,0x18,F32(s,0x4f0c));SF32(b,0x1c,0);rx_addr wheel=0;
  if(!U32(s,0x83d4)&&U32(s,0x8410))wheel=s+0x8390;else if(!U32(s,0x8524)&&U32(s,0x8560))wheel=s+0x84e0;
  if(wheel){rx_st4(b+0x30,rx_ld4(wheel));float slip=absolute(b,0x4befd4,F32(b,0x30));
   if(slip>fc(0x931f24)){slip=phys_div(phys_sub(slip,fc(0x931f24)),fc(0x92e134));float value=fc(0x931f24);if(!(slip>=value)){value=slip;if(0>=slip)value=0;}SF32(b,0x1c,value);}
  }
  float steer=absolute(b,0x4bf018,F32(s,0x56d0)),factor=F32(b,0x1c);if(!(factor>steer))factor=steer;factor=phys_sub(fc(0x931f24),factor);if(fc(0x92ff84)>factor)factor=fc(0x92ff84);
  SF32(b,0x18,phys_mul(factor,F32(b,0x18)));fx_set_reg(FX_EAX,U32(s,0x8410));
  const fw9_contact rearWheels[]={{0x8410,0x8390},{0x8560,0x84e0}};
  if(fw9_contact_average(s,rearWheels,2,b+0x40)){
   float fraction=phys_div(absolute(b,0x4bf0c5,F32(b,0x48)),fc(0x9cc37c));if(fraction>fc(0x931f24))fraction=fc(0x931f24);
   factor=phys_sub(fc(0x931f24),fraction);float bound=fc(0x931f24);if(!(factor>=bound)){bound=fc(0x931d94);if(!(bound>=factor))bound=factor;}SF32(b,0x18,phys_mul(bound,F32(b,0x18)));
  }
 }
 fx_set_reg(FX_ESI,s+0x5754);float traction=lag(b,s+0x5754,0x4bf124,dt,copyf(F32(b,0x18)));SF32(s,0x5640,traction);SU32(s,0x55f0,0);
 float throttle=0;if(!(F32(s,0x28)>fc(0x9322ac)))throttle=call0(b,0x42c1f0,0x4bf157,F32(s,0x1c),fc(0x931f24),0);SF32(s,0x55f0,throttle);
 if(U32(s,0x86f0)){fx_x87_copy(s+0x5640,s+0x4ec4);setlag(b,s+0x5754,0x4bf18c,F32(s,0x4f0c));}
 else{SF32(b,0x20,F32(s,0x4f0c));float a=absolute(b,0x4bf1a9,F32(s,0x56d0));a=phys_sub(fc(0x931f24),a);if(fc(0x92ff84)>a)a=fc(0x92ff84);traction=lag(b,s+0x5754,0x4bf1e0,dt,phys_mul(a,F32(b,0x20)));SF32(s,0x5640,traction);}
 for(unsigned i=0;i<4;i++)fx_x87_copy(s+0x56d0+4*i,s+0x2c+4*i);fx_set_reg(FX_EBX,s+0x440c);
 const uint32_t parameter[]={0x440c,0x454c,0x48ac,0x49ec,0x440c,0x454c,0x48ac,0x49ec};
 const uint32_t output[]={0x8120,0x8110,0x83c0,0x83b0,0x8270,0x8260,0x8510,0x8500};
 const rx_addr sites[]={0x4bf21f,0x4bf23b,0x4bf257,0x4bf273,0x4bf28b,0x4bf2a7,0x4bf2c3,0x4bf2df};
 for(unsigned i=0;i<8;i++)rx_st4(s+output[i],v4_splat(curve(b,s+parameter[i],sites[i],F32(b,0x24))));
 rx_st4(s+0x5630,v4_sub(v4_zero(),fw9_normalize_squared(rx_ld4(b+0x70),rx_ld4(b+0x50))));
 if(U32(s,0x86f0)){
  float strength=curve(b,s+0x34ac,0x4bf366,F32(b,0x24));rx_st4(s+0x5630,v4_mul(v4_splat(strength),rx_ld4(s+0x5630)));rx_st4(b+0x70,v4_splat(fc(0x92e12c)));
  float lift=curve(b,s+0x354c,0x4bf39d,F32(b,0x2c));rx_v4 addition=v4_splat(phys_sub(fc(0x9774a8),lift));addition=v4_shuf(addition,rx_ld4(b+0x70),0xee);addition=v4_movss(addition,rx_ld4(b+0x70));rx_st4(s+0x5630,v4_add(addition,rx_ld4(s+0x5630)));
 }else{float strength=curve(b,s+0x340c,0x4bf3e2,F32(b,0x24));rx_st4(s+0x5630,v4_mul(v4_splat(strength),rx_ld4(s+0x5630)));}
 fx_frame_leave(&frame);
}
