/*004d9490 complete hand-written UTV control phase. Source candidate only.
 * Frame offsets and call sites follow vf9_utv_full.asm; no host trig/physics
 * replacement, fabricated parameter defaults, or original-caller delegation. */
#include "fourwheel_controls.h"
#include "fourwheel_controls_math.h"
static float fc(rx_addr p){return F32(p,0);}
static float call0(rx_addr b,rx_addr target,rx_addr ra,float a,float c,float d){
 SU32(b-4,0,ra);rx_set_sp(b-4);return rx_utv_controls_dependency(target,b,a,c,d);
}
static float call1(rx_addr b,rx_addr target,rx_addr ra,float arg,float a,float c,float d){
 SF32(b-4,0,arg);SU32(b-8,0,ra);rx_set_sp(b-8);return rx_utv_controls_dependency(target,b,a,c,d);
}
static float absolute(rx_addr b,rx_addr ra,float value){return call0(b,0x401000,ra,value,0,0);}
static float curve(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ESI,p);return call0(b,0x438ee0,ra,value,0,0);}
static float lag(rx_addr b,rx_addr p,rx_addr ra,float dt,float target){fx_set_reg(FX_EAX,p);return call1(b,0x42d6b0,ra,target,0,0,dt);}
static void setlag(rx_addr b,rx_addr p,rx_addr ra,float value){fx_set_reg(FX_ECX,p);(void)call1(b,0x42d670,ra,fx_f32(fx_ld(0x977494)),0,value,0);}
static float signof(float value){return value>=0?fc(0x931f24):fc(0x9cd9e8);}
static float copyf(float value){return rx_bf(fx_x87_bits(rx_fb(value)));}
void rx_utv_controls_step_exact(rx_addr s,float dt){
 fx_frame frame;fx_frame_enter(&frame,0x64,16,FX_EBX,FX_ESI,FX_EDI);
 rx_addr b=frame.E;fx_set_reg(FX_EDI,s);
 rx_v4 velocity=rx_ld4(s+0x100),rotation=rx_ld4(s+0x730);
 rx_v4 local=fw9_body_velocity(velocity,rotation),squared=v4_mul(velocity,velocity);
 rx_st4(b+0x40,squared);SF32(b,0x1c,phys_sqrt(fw9_sum_021(squared).v[0]));
 rx_v4 horizontal=v4_shuf(rx_ld4(0xd61670),velocity,0xe5);
 horizontal=v4_movss(horizontal,velocity);
 rx_st4(b+0x60,velocity);rx_st4(b+0x20,local);
 SF32(b,0x14,phys_sqrt(fw9_sum_021(v4_mul(horizontal,horizontal)).v[0]));
 if(U32(s,0x2de0))fx_x87_copy(s+0x5618,s+0x2e70);
 else{
  float gain=curve(b,s+0x35ec,0x4d9607,F32(b,0x28));SF32(b,0x10,gain);
  float input=F32(s,0x2c),old=F32(s,0x5728);
  float difference=phys_sub(phys_mul(phys_mul(input,signof(input)),input),old);
  SF32(b,0x18,old);SF32(b,0x14,signof(difference));
  float distance=absolute(b,0x4d966c,difference),step=phys_mul(dt,fc(0x977b0c));
  if(!(step>distance))distance=step;
  float target=phys_add(phys_mul(distance,F32(b,0x14)),F32(b,0x18));
  float tau=F32(s,0x572c),k=phys_div(tau>dt?dt:tau,tau);SF32(s,0x5730,k);
  float filtered=phys_add(phys_mul(phys_sub(target,F32(s,0x5728)),k),F32(s,0x5728));
  SF32(s,0x5728,filtered);SF32(s,0x5618,phys_mul(filtered,F32(b,0x10)));
  local=fw9_body_velocity(rx_ld4(s+0x100),rx_ld4(s+0x730));
  fw9_heading_result heading=fw9_heading(local,rx_ld4(s+0x100),b+0x50,b+0x30,b+0x14);
  float correction=0;
  if(heading.active){
   SF32(b,0x10,heading.angle);
   SF32(b-4,0,fx_f32(fx_ld(0x9322cc)));SF32(b-8,0,0);SU32(b-12,0,0x4d9a4e);rx_set_sp(b-12);
   float blend=rx_utv_controls_dependency(0x430810,b-8,0,0,heading.blend);
   correction=phys_mul(blend,F32(b,0x10));rx_set_sp(b);
   SF32(b,0x14,U32(s,0x8700)?fc(0x9d789c):fc(0x931f24));
   SF32(b,0x18,signof(correction));
   correction=absolute(b,0x4d9a98,correction);
   float bound=phys_mul(F32(b,0x14),fc(0x977678));if(!(bound>correction))correction=bound;
   correction=phys_mul(correction,F32(b,0x18));
  }
  fx_set_reg(FX_ESI,s+0x5750);float oldCorrection=F32(s,0x5750);
  difference=phys_sub(correction,oldCorrection);SF32(b,0x10,oldCorrection);SF32(b,0x14,signof(difference));
  distance=absolute(b,0x4d9af1,difference);step=phys_mul(dt,fc(0x9d78dc));
  SF32(b,0x18,step>distance?distance:step);
  /* push ECX occurs between FLD and FMUL: its ESP-relative operands still
   * address B+18 and B+10. Preserve the intermediate fstp at B+10. */
  float targetCorrection=fx_f32(fx_add(fx_mul(fx_ld(b+0x14),fx_ld(b+0x18)),fx_ld(b+0x10)));
  SF32(b,0x10,targetCorrection);targetCorrection=copyf(F32(b,0x10));
  correction=lag(b,s+0x5750,0x4d9b33,dt,targetCorrection);
  float angle=phys_add(correction,F32(s,0x5618));SF32(s,0x5618,angle);
  float bound=fc(0x977b08);if(!(angle>=bound)){
   bound=fc(0x977b04);if(!(bound>=angle))bound=angle;
  }
  SF32(s,0x5618,bound);
 }
 fx_set_reg(FX_ESI,s);(void)call0(b,0x4d4190,0x4d9b6f,0,0,0);
 fx_x87_copy(b+0x18,s+0x28);for(unsigned i=0;i<4;i++)SU32(s,0x5608+4*i,0);
 SF32(b,0xc,F32(s,0x24));fx_set_reg(FX_ESI,s+0x5700);
 float front=lag(b,s+0x5700,0x4d9bbe,dt,copyf(F32(b,0xc)));
 fx_set_reg(FX_EBX,s+0x5714);SF32(b,0x14,front);
 float rear=lag(b,s+0x5714,0x4d9bde,dt,copyf(F32(b,0x18)));SF32(b,0x10,rear);
 front=F32(b,0x14);
 if(front>F32(b,0xc)){setlag(b,s+0x5700,0x4d9c06,F32(b,0xc));SF32(b,0x14,F32(b,0xc));}
 if(F32(b,0x10)>F32(b,0x18)){setlag(b,s+0x5714,0x4d9c34,F32(b,0x18));SF32(b,0x10,F32(b,0x18));}
 /* Each polarity has its original call return address. The front/rear gains
  * and curve ESI remain real memory slots, not default or sampled replacements. */
 SF32(b,0xc,phys_mul(F32(s,0x3b5c),F32(b,0x14)));
 const rx_addr positiveRA[4]={0x4d9c77,0x4d9ccb,0x4d9d39,0x4d9d8d};
 const rx_addr negativeRA[4]={0x4d9ca4,0x4d9cf8,0x4d9d66,0x4d9dba};
 for(unsigned i=0;i<4;i++){
  if(i==2)SF32(b,0xc,phys_mul(F32(s,0x3c44),F32(b,0x10)));
  float spin=F32(s,0x85c0+4*i);int positive=spin>fc(0x92e12c);
  float value=positive?spin:phys_sub(fc(0x9774a8),spin);
  value=curve(b,s+(i<2?0x3ba4:0x3c8c),positive?positiveRA[i]:negativeRA[i],value);
  value=phys_mul(value,F32(b,0xc));if(positive)value=phys_sub(fc(0x9774a8),value);
  SF32(s,0x5608+4*i,value);
 }
 SU32(s,0x5600,0);
 if(!(F32(s,0x28)>fc(0x9322ac)))fx_x87_copy(s+0x5600,s+0x1c);
 SU32(s,0x5604,0);
 if(F32(s,0x20)>0&&fc(0x935890)>F32(b,0x28)&&!(F32(s,0x28)>fc(0x9322ac))){SU32(s,0x5600,0);SF32(s,0x5604,F32(s,0x20));}
 fx_set_reg(FX_EBX,0);
 if(U32(s,0x2eb0)){
  if(U32(s,0x56f0)){uint32_t gear=U32(s,0x8660)+1;if((int32_t)gear>=4)gear=4;fx_set_reg(FX_EAX,gear);SU32(s,0x8660,gear);SU32(s,0x56f0,0);}
  if(U32(s,0x56f4)){uint32_t gear=U32(s,0x8660)-1;uint32_t mask=((int32_t)gear<=0?1u:0u)-1;fx_set_reg(FX_ECX,mask);gear&=mask;fx_set_reg(FX_EAX,gear);SU32(s,0x56f4,0);SU32(s,0x8660,gear);}
 }
 fx_set_reg(FX_ESI,U32(s,0x48));SU32(s,0x56f8,fx_reg(FX_ESI));
 float forwardAbs=absolute(b,0x4d9e96,F32(b,0x28));
 uint32_t idle=fc(0x9322cc)>forwardAbs&&0>F32(s,0x5600)&&0>F32(s,0x5604);fx_set_reg(FX_EAX,idle);
 if(fx_reg(FX_ESI)||idle){
  float t=phys_div(phys_sub(F32(b,0x1c),fc(0x935890)),fc(0x935aac));if(t>fc(0x931f24))t=fc(0x931f24);
  SU32(s,0x56fc,2);SF32(s,0x8658,phys_mul(F32(s,0x4f9c),t));
 }else if(0>F32(s,0x8658)||(!U32(s,0x8420)&&!U32(s,0x8570))){SU32(s,0x56fc,0);SF32(s,0x8658,fc(0x9cd9dc));}
 else{SU32(s,0x56fc,1);SF32(s,0x8658,phys_sub(F32(s,0x8658),dt));}
 if(U32(s,0x8700))SF32(b,0xc,F32(s,0x4ec4));
 else{
  SF32(b,0xc,F32(s,0x4f0c));SF32(b,0x10,0);
  rx_addr wheel=0;if(!U32(s,0x83e4)&&U32(s,0x8420))wheel=s+0x83a0;else if(!U32(s,0x8534)&&U32(s,0x8570))wheel=s+0x84f0;
  if(wheel){rx_st4(b+0x50,rx_ld4(wheel));float slip=absolute(b,0x4d9fc4,F32(b,0x50));
   if(slip>fc(0x931f24)){slip=phys_div(phys_sub(slip,fc(0x931f24)),fc(0x92e134));
    float v=fc(0x931f24);if(!(slip>=v)){v=slip;if(0>=slip)v=0;}SF32(b,0x10,v);
   }
  }
  float steer=absolute(b,0x4da008,F32(s,0x56e0)),factor=F32(b,0x10);if(!(factor>steer))factor=steer;
  factor=phys_sub(fc(0x931f24),factor);if(fc(0x92ff84)>factor)factor=fc(0x92ff84);
  SF32(b,0xc,phys_mul(factor,F32(b,0xc)));fx_set_reg(FX_EAX,U32(s,0x8420));
  const fw9_contact wheels[4]={{0x8420,0x83a0},{0x8570,0x84f0},{0x8180,0x8100},{0x82d0,0x8250}};
  if(fw9_contact_average(s,wheels,4,b+0x30)){
   float fraction=phys_div(absolute(b,0x4da101,F32(b,0x38)),fc(0x9cc37c));if(fraction>fc(0x931f24))fraction=fc(0x931f24);
   factor=phys_sub(fc(0x931f24),fraction);float bound=fc(0x931f24);
   if(!(factor>=bound)){bound=fc(0x931d94);if(!(bound>=factor))bound=factor;}
   SF32(b,0xc,phys_mul(bound,F32(b,0xc)));
  }
 }
 fx_set_reg(FX_ESI,s+0x5764);
 float traction=lag(b,s+0x5764,0x4da160,dt,copyf(F32(b,0xc)));SF32(s,0x5650,traction);SU32(s,0x5600,0);
 float throttle=0;if(!(F32(s,0x28)>fc(0x9322ac)))throttle=call0(b,0x42c1f0,0x4da193,F32(s,0x1c),fc(0x931f24),0);SF32(s,0x5600,throttle);
 if(U32(s,0x8700)){fx_x87_copy(s+0x5650,s+0x4ec4);setlag(b,s+0x5764,0x4da1c8,F32(s,0x4f0c));}
 else{
  SF32(b,0x14,F32(s,0x4f0c));float a=absolute(b,0x4da1e5,F32(s,0x56e0));a=phys_sub(fc(0x931f24),a);if(fc(0x92ff84)>a)a=fc(0x92ff84);
  traction=lag(b,s+0x5764,0x4da21c,dt,phys_mul(a,F32(b,0x14)));SF32(s,0x5650,traction);
 }
 for(unsigned i=0;i<4;i++)fx_x87_copy(s+0x56e0+4*i,s+0x2c+4*i);
 fx_set_reg(FX_EBX,s+0x440c);
 const uint32_t parameter[8]={0x440c,0x454c,0x48ac,0x49ec,0x440c,0x454c,0x48ac,0x49ec};
 const uint32_t output[8]={0x8130,0x8120,0x83d0,0x83c0,0x8280,0x8270,0x8520,0x8510};
 const rx_addr sites[8]={0x4da25b,0x4da277,0x4da293,0x4da2af,0x4da2c7,0x4da2e3,0x4da2ff,0x4da31b};
 for(unsigned i=0;i<8;i++)rx_st4(s+output[i],v4_splat(curve(b,s+parameter[i],sites[i],F32(b,0x1c))));
 /* Tail normalizes the original velocity with sum012, not the earlier speed
  * sum021; its zero/infinite masks remain packed. */
 rx_v4 drag=v4_sub(v4_zero(),fw9_normalize_squared(rx_ld4(b+0x60),rx_ld4(b+0x40)));rx_st4(s+0x5640,drag);
 if(U32(s,0x8700)){
  float strength=curve(b,s+0x34ac,0x4da39c,F32(b,0x1c));rx_st4(s+0x5640,v4_mul(v4_splat(strength),rx_ld4(s+0x5640)));
  rx_st4(b+0x60,v4_splat(fc(0x92e12c)));float lift=curve(b,s+0x354c,0x4da3d3,F32(b,0x28));
  rx_v4 addition=v4_splat(phys_sub(fc(0x9774a8),lift));addition=v4_shuf(addition,rx_ld4(b+0x60),0xee);addition=v4_movss(addition,rx_ld4(b+0x60));
  rx_st4(s+0x5640,v4_add(addition,rx_ld4(s+0x5640)));
 }else{float strength=curve(b,s+0x340c,0x4da418,F32(b,0x1c));rx_st4(s+0x5640,v4_mul(v4_splat(strength),rx_ld4(s+0x5640)));}
 fx_frame_leave(&frame);
}
