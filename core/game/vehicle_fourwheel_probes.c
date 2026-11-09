/* Four-wheel vf32 contact-ray construction. Original reflex_002_004b32a0.c
 * 004da510/004bf400 and reflex_001_00459400.c0045b8f0, checked against x86
 * disassembly. These are data/geometry leaves, with no external calls. */
#include "vehicle_fourwheel_probes.h"
#include "bike_math.h"
#include "utv_probes_observed.h"
#include "family_probes_observed.h"
#include "fx_util.h"
#include <stdlib.h>
static rx_v4 ld(rx_addr a){return rx_ld4(a);}
static void put(rx_addr s,unsigned i,rx_v4 v){rx_st4(s+0x14b0+16*i,v);}
static rx_v4 xyz(float x,float y,float z){return v4(x,y,z,z);}
/* Original sites multiply q by the conjugation sign vector in this order. */
static rx_v4 rotate_inv(rx_v4 q,rx_v4 v){return bike_quat_mul(v4_mul(q,v4(-1,-1,-1,1)),bike_quat_mul(v4_xyz(v),q));}
static void wheel_probes(rx_addr s,rx_v4 q,rx_v4 pos,const float side[4]){
 for(unsigned i=0;i<4;i++){
  rx_v4 local=rotate_inv(q,v4_sub(ld(s+0x190+0xa0*i),pos));
  rx_st4(s+0x1600+16*i,v4_add(local,v4(side[i],-.5f,0,0)));
 }
}
/* The speed threshold is strict; all four division/multiplication lanes remain
 * active below it, including NaNs and signed zero. */
static rx_v4 lift(rx_addr s){
 rx_v4 speed=v4_sqrt(v4_splat(bike_hsum_021(v4_mul(ld(s+0x100),ld(s+0x100)))));
 rx_v4 up=v4(0,1.5f,0,0),ten=v4_splat(10);
 return bike_all3(v4_cmplt(ten,speed))?up:v4_div(v4_mul(speed,up),ten);
}
static void raise_probes(rx_addr s,rx_v4 up,unsigned reverse_mask){
 const unsigned raised[]={6,7,14,15,16,19,20};
 for(unsigned i=0;i<7;i++){
  rx_addr p=s+0x14b0+16*raised[i];rx_v4 v=ld(p);
  rx_st4(p,(reverse_mask&(1u<<i))?v4_add(up,v):v4_add(v,up));
 }
}
static void finish_probes(rx_addr s,rx_v4 q){
 /* Copy before replacing four wheel baselines. Count is reread in both loops. */
 for(int32_t i=0;i<I32(s,0x1318);i++)rx_st4(s+0x1f40+16*(uint32_t)i,ld(s+0x14b0+16*(uint32_t)i));
 for(unsigned i=0;i<4;i++)rx_st4(s+0x2090+16*i,v4_sub(v4_zero(),rotate_inv(q,ld(0x00d616a0))));
 for(int32_t i=0;i<I32(s,0x1318);i++){
  uint32_t n=(uint32_t)i;rx_v4 p=ld(s+0x14b0+16*n);
  rx_st4(s+0x1c20+32*n,v4_sub(p,ld(s+0x1f40+16*n)));
  rx_st4(s+0x1c30+32*n,ld(s+0x14b0+16*n));
 }
}
static void utv_probe_kernel(rx_addr s,int observed,rx_v4*live){
 const rx_addr entry=rx_sp(),oldbp=observed?fx_reg(FX_EBP):0,oldsi=observed?fx_reg(FX_ESI):0,F=((entry-4)&~15u)-0x9c;
 if(observed){if(!live||fx_reg(FX_ECX)!=s)abort();
  /* Real original owners must not alias this private aligned frame. */
  if(s<entry&&s+0x2100>F-4)abort();
  SU32(entry-4,0,oldbp);SU32(F-4,0,oldsi);fx_set_reg(FX_EBP,entry-4);rx_set_sp(F-4);
 }
 rx_v4 q=ld(s+0x730),pos=ld(s+0xf0);
 float z=F32(0,0x009d789c),far=F32(0,0x009cd9e4),minus2=F32(0,0x009d7b30),minus1=F32(0,0x009cd9e8);
 const rx_v4 body[]={
  xyz(z,-.5f,far),xyz(z,1,far),xyz(2,z,1),xyz(2,3,minus2),xyz(z,1,-5),xyz(z,0,-5),
  xyz(rx_bf(0x40333333),minus1,rx_bf(0x3fcccccd)),xyz(rx_bf(0x40333333),minus1,-1.5f),
  xyz(-2.5f,-.5f,far),xyz(-2.5f,1,far),xyz(minus2,z,1),xyz(minus2,3,minus2),xyz(-2.5f,1,-5),xyz(-2.5f,0,-5),
  xyz(rx_bf(0xc0333333),minus1,rx_bf(0x3fcccccd)),xyz(rx_bf(0xc0333333),minus1,-1.5f),
  xyz(0,minus1,5.5f),xyz(0,.5f,5),xyz(0,.5f,-5.5f),xyz(0,minus1,0),xyz(0,minus1,-4)};
 for(unsigned i=0;i<21;i++)put(s,i,body[i]);
 const float side[]={rx_bf(0xbe99999a),rx_bf(0x3e99999a),rx_bf(0xbe99999a),rx_bf(0x3e99999a)};
 rx_v4 lastWheelRaw=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_v4 local=v4_xyz(v4_sub(ld(s+0x190+0xa0*i),pos));
  rx_v4 first=bike_quat_mul(local,q),raw;
  rx_v4 rotated=bike_quat_mul_observed(v4_mul(q,v4(-1,-1,-1,1)),first,&raw);
  if(i==3)lastWheelRaw=raw;
  rx_st4(s+0x1600+16*i,v4_add(rotated,v4(side[i],-.5f,0,0)));
 }
 rx_v4 square=v4_mul(ld(s+0x100),ld(s+0x100));
 rx_v4 speed=v4_sqrt(v4_splat(bike_hsum_021(square)));
 rx_v4 mask=v4_cmplt(v4_splat(F32(0,0x92fd88)),speed);
 rx_v4 up=v4(0,F32(0,0x9322a4),0,0);
 if(!bike_all3(mask))up=v4_div(v4_mul(speed,up),v4_splat(F32(0,0x92fd88)));
 raise_probes(s,up,0);
 /* Preserve each source loop's signed count reread and exact register
  * progression. Up directions are recomputed as in the original four sites. */
 uint32_t ecx=s+0x14b0,edx=v4_lb(mask,1);
 for(int32_t i=0;i<I32(s,0x1318);i++){
  rx_st4(s+0x1f40+16*(uint32_t)i,ld(s+0x14b0+16*(uint32_t)i));edx=s+0x1f40+16*((uint32_t)i+1);
 }
 rx_v4 firstUp=v4_zero(),return0=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_v4 first=bike_quat_mul(v4_xyz(ld(0xd616a0)),q);
  rx_v4 rotated=bike_quat_mul(v4_mul(q,v4(-1,-1,-1,1)),first);
  rx_st4(s+0x2090+16*i,v4_sub(v4_zero(),rotated));
  firstUp=first;
 }
 return0=firstUp;
 for(int32_t i=0;i<I32(s,0x1318);i++){
  uint32_t n=(uint32_t)i;rx_v4 p=ld(ecx),old=ld(ecx+0xa90);
  rx_st4(s+0x1c20+32*n,v4_sub(p,old));p=ld(ecx);rx_st4(s+0x1c30+32*n,p);
  return0=p;ecx+=16;edx=s+0x1c30+32*(n+1);
 }
 if(observed){
  /* Final private spill map: all originally written words, no holes filled. */
  rx_st4(F+0x0c,v4(F32(0,0x92fe48),F32(0,0x92fe48),minus2,minus2));
  rx_st4(F+0x1c,v4(z,z,F32(0,0x931f24),F32(0,0x931f24)));
  rx_st4(F+0x2c,v4(F32(0,0x931f24),F32(0,0x931f24),F32(0,0x97739c),F32(0,0x97739c)));
  rx_st4(F+0x3c,v4(minus1,minus1,F32(0,0x9774bc),F32(0,0x9774bc)));
  rx_st4(F+0x4c,v4(F32(0,0x92e12c),F32(0,0x92e12c),F32(0,0x97739c),F32(0,0x97739c)));
  rx_st4(F+0x5c,v4_zero());rx_st4(F+0x6c,lastWheelRaw);
  rx_st4(F+0x7c,v4(side[1],F32(0,0x9774b0),0,0));rx_st4(F+0x8c,v4(side[0],F32(0,0x9774b0),0,0));
  fx_set_reg(FX_EAX,s);fx_set_reg(FX_ECX,ecx);fx_set_reg(FX_EDX,edx);fx_set_reg(FX_ESI,oldsi);fx_set_reg(FX_EBP,oldbp);rx_set_sp(entry);*live=return0;
 }
}
void rx_sim_utv_update_probes(rx_addr s){utv_probe_kernel(s,0,0);}
void rx_sim_utv_update_probes_observed(rx_addr s,rx_v4*live){utv_probe_kernel(s,1,live);}
RX_FUNCTION(0x004da510,rx_sim_utv_update_probes)
/* Exact leaves retain entryESP; the enclosing adapter alone performs RET0.
 * Ordinary and exact routes share all geometry; observation only records
 * already-computed products, actual private spill bytes and escaping registers. */
typedef struct probe_frame {
 rx_addr entry,F;uint32_t oldbp,oldsi;int observed;
} probe_frame;
static probe_frame probe_enter(rx_addr s,unsigned locals,int observed,rx_v4*live){
 probe_frame f={rx_sp(),0,0,0,observed};f.F=((f.entry-4)&~15u)-locals;
 if(observed){
  if(!live||fx_reg(FX_ECX)!=s)abort();
  /* Authored simulation and immutable constants cannot alias private stack. */
  if(s<f.entry&&s+0x5600>f.F-4)abort();
  f.oldbp=fx_reg(FX_EBP);f.oldsi=fx_reg(FX_ESI);
  SU32(f.entry-4,0,f.oldbp);SU32(f.F-4,0,f.oldsi);
  fx_set_reg(FX_EBP,f.entry-4);rx_set_sp(f.F-4);
 }
 return f;
}
static rx_v4 probe_lift(rx_addr s,rx_v4*mask){
 rx_v4 velocity=ld(s+0x100),square=v4_mul(velocity,velocity);
 rx_v4 speed=v4_sqrt(v4_splat(bike_hsum_021(square)));
 rx_v4 ten=v4_splat(F32(0,0x92fd88)),up=v4(0,F32(0,0x9322a4),0,0);
 *mask=v4_cmplt(ten,speed);
 return bike_all3(*mask)?up:v4_div(v4_mul(speed,up),ten);
}
/* The helper exposes actual A0 before sign application; no quaternion
 * arithmetic is reimplemented here. */
static rx_v4 probe_wheel(rx_addr s,unsigned i,rx_v4 q,rx_v4 pos,rx_v4 offset,
 rx_v4*inner,rx_v4*rawOuter){
 rx_v4 delta=v4_xyz(v4_sub(ld(s+0x190+0xa0*i),pos));
 *inner=bike_quat_mul(delta,q);
 rx_v4 rotated=bike_quat_mul_observed(v4_mul(q,ld(0x93a220)),*inner,rawOuter);
 rx_v4 result=v4_add(rotated,offset);rx_st4(s+0x1600+16*i,result);return result;
}
/* Both actual count loops reread signed self+1318. EDX retains speed mask.Y
 * if both loops are skipped; ECX starts at the local-probe array regardless. */
static void probe_finish(rx_addr s,rx_v4 q,probe_frame*f,rx_v4 mask,int truck,rx_v4*live){
 uint32_t ecx=s+0x14b0,edx=v4_lb(mask,1);
 for(int32_t i=0;i<I32(s,0x1318);i++){
  rx_st4(s+0x1f40+16*(uint32_t)i,ld(s+0x14b0+16*(uint32_t)i));
  edx=s+0x1f40+16*((uint32_t)i+1);
 }
 rx_v4 return0=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_v4 first=bike_quat_mul(v4_xyz(ld(0xd616a0)),q),wx;
  rx_v4 rotated=bike_quat_mul_observed_parts(v4_mul(q,ld(0x93a220)),first,0,&wx);
  rx_st4(s+0x2090+16*i,v4_sub(v4_zero(),rotated));
  return0=truck?wx:first;
 }
 for(int32_t i=0;i<I32(s,0x1318);i++){
  uint32_t n=(uint32_t)i;rx_v4 old=ld(ecx+0xa90),p=ld(ecx);
  rx_st4(s+0x1c20+32*n,v4_sub(p,old));p=ld(ecx);rx_st4(s+0x1c30+32*n,p);
  return0=p;ecx+=16;edx=s+0x1c30+32*(n+1);
 }
 if(f->observed){fx_set_reg(FX_EAX,s);fx_set_reg(FX_ECX,ecx);fx_set_reg(FX_EDX,edx);
  fx_set_reg(FX_ESI,f->oldsi);fx_set_reg(FX_EBP,f->oldbp);rx_set_sp(f->entry);*live=return0;}
}

static void buggy_probe_kernel(rx_addr s,int observed,rx_v4*live){
 probe_frame f=probe_enter(s,0x7c,observed,live);
 rx_v4 q=ld(s+0x730),pos=ld(s+0xf0);
 float low=F32(0,0x009d789c),rear=F32(0,0x009cc8a8),minus2=F32(0,0x009d7b30),back=F32(0,0x009cc380),mid=F32(0,0x009c4b4c);
 /* Four upper corner probes are derived from wheel positions below. */
 put(s,1,xyz(rx_bf(0x3fcccccd),rear,5.5f));put(s,2,xyz(2,low,mid));put(s,3,xyz(2,low,back));put(s,4,xyz(2,1,-8));
 put(s,6,xyz(rx_bf(0x3fe66666),minus2,rx_bf(0x3fcccccd)));put(s,7,xyz(rx_bf(0x3fe66666),minus2,-1.5f));
 put(s,9,xyz(rx_bf(0xbfcccccd),rear,5.5f));put(s,10,xyz(minus2,low,mid));put(s,11,xyz(minus2,low,back));put(s,12,xyz(minus2,1,-8));
 put(s,14,xyz(rx_bf(0xbfe66666),minus2,rx_bf(0x3fcccccd)));put(s,15,xyz(rx_bf(0xbfe66666),minus2,-1.5f));
 put(s,16,xyz(0,minus2,5.5f));put(s,17,xyz(0,.5f,5));put(s,18,xyz(0,-1.5f,-8));put(s,19,xyz(0,minus2,0));put(s,20,xyz(0,minus2,-4));
 const float side[]={rx_bf(0xbe99999a),rx_bf(0x3e99999a),rx_bf(0xbf666666),F32(0,0x009cd970)};
 rx_v4 lastInner=v4_zero(),lastRaw=v4_zero();
 for(unsigned i=0;i<4;i++)probe_wheel(s,i,q,pos,v4(side[i],F32(0,0x9774b0),0,0),&lastInner,&lastRaw);
 rx_v4 up=v4(0,2,0,0);put(s,0,v4_add(ld(s+0x1600),up));put(s,8,v4_add(up,ld(s+0x1610)));
 put(s,5,v4_add(up,ld(s+0x1620)));put(s,13,v4_add(up,ld(s+0x1630)));
 rx_v4 mask;raise_probes(s,probe_lift(s,&mask),1u<<1);
 if(observed){const rx_addr B=f.F-4;
  rx_st4(B+0x10,v4(F32(0,0x931f24),F32(0,0x931f24),F32(0,0x977884),F32(0,0x977884)));
  rx_st4(B+0x20,v4(low,low,back,back));
  rx_st4(B+0x30,v4(minus2,minus2,F32(0,0x977850),F32(0,0x977850)));
  rx_st4(B+0x40,lastInner);rx_st4(B+0x50,v4_zero());rx_st4(B+0x60,lastRaw);rx_st4(B+0x70,pos);
 }
 probe_finish(s,q,&f,mask,0,live);
}
void rx_sim_buggy_update_probes(rx_addr s){buggy_probe_kernel(s,0,0);}
void rx_sim_buggy_update_probes_observed(rx_addr s,rx_v4*live){buggy_probe_kernel(s,1,live);}
RX_FUNCTION(0x004bf400,rx_sim_buggy_update_probes)
static void truck_probe_kernel(rx_addr s,int observed,rx_v4*live){
 probe_frame f=probe_enter(s,0x8c,observed,live);
 rx_v4 q=ld(s+0x730),pos=ld(s+0xf0);int cls7=I32(s,0x55f0)==7;
 float minus2=F32(0,0x009d7b30),minus1=F32(0,0x009cd9e8),rear=F32(0,0x009d78b8),low=F32(0,0x009d789c),back=F32(0,0x009cc380);
 float frontX=cls7?rx_bf(0x40533333):3,frontZ=cls7?7:rx_bf(0x40d9999a),midX=cls7?rx_bf(0x40333333):low;
 float midFront=cls7?1:1.5f,midBack=cls7?minus2:rx_bf(0xbfa66666),backZ=cls7?rear:-7.5f;
 float sideX=cls7?rx_bf(0x40666666):rx_bf(0x40533333),sideFront=cls7?rx_bf(0x3fa66666):2,sideBack=cls7?-2.5f:-1.5f;
 float leftFront=cls7?rx_bf(0xc0533333):back,leftMid=cls7?rx_bf(0xc0333333):-2.5f,leftSide=cls7?rx_bf(0xc0666666):rx_bf(0xc0533333);
 const rx_v4 body[]={
  xyz(frontX,-.5f,frontZ),xyz(frontX,1,frontZ),xyz(midX,3,midFront),xyz(midX,3,midBack),
  xyz(frontX,cls7?2:1.5f,backZ),xyz(frontX,cls7?-1.5f:minus1,backZ),xyz(sideX,minus2,sideFront),xyz(sideX,minus2,sideBack),
  xyz(leftFront,-.5f,frontZ),xyz(leftFront,1,frontZ),xyz(leftMid,3,midFront),xyz(leftMid,3,midBack),
  xyz(leftFront,cls7?2:1.5f,backZ),xyz(leftFront,cls7?-1.5f:minus1,backZ),xyz(leftSide,minus2,sideFront),xyz(leftSide,minus2,sideBack),
  xyz(0,minus2,5.5f),xyz(0,.5f,cls7?8.5f:7.5f),xyz(0,0,cls7?-10.5f:-8.5f),xyz(0,minus2,0)};
 for(unsigned i=0;i<20;i++)put(s,i,body[i]);
 float x=cls7?minus1:F32(0,0x0097781c),r=cls7?1:F32(0,0x009c4b4c);
 const float side[]={x,r,x,r};rx_v4 lastInner=v4_zero(),lastRaw=v4_zero();
 for(unsigned i=0;i<4;i++)probe_wheel(s,i,q,pos,v4(side[i],F32(0,0x9774b0),0,0),&lastInner,&lastRaw);
 put(s,20,v4_mul(v4_add(ld(s+0x1620),ld(s+0x1630)),ld(0x00d615d0)));
 /* Class7 left side probes load the lift into the other SSE operand. */
 rx_v4 mask;raise_probes(s,probe_lift(s,&mask),cls7?((1u<<2)|(1u<<4)|(1u<<6)):0x7f);
 if(observed){const rx_addr B=f.F-4;
  if(cls7){
   rx_st4(B+0x10,v4(3,3,minus2,minus2));
   rx_st4(B+0x20,v4(2,2,rear,rear));
   rx_st4(B+0x30,v4(F32(0,0x977850),F32(0,0x977850),rear,rear));
   rx_st4(B+0x40,lastRaw);
   rx_st4(B+0x60,v4(r,F32(0,0x9774b0),0,0));
   rx_st4(B+0x70,v4(minus2,minus2,F32(0,0x97784c),F32(0,0x97784c)));
   rx_st4(B+0x80,v4(x,F32(0,0x9774b0),0,0));
  }else{
   rx_st4(B+0x10,v4(minus1,minus1,F32(0,0x9778b0),F32(0,0x9778b0)));
   rx_st4(B+0x20,v4(F32(0,0x9322a4),F32(0,0x9322a4),F32(0,0x9778b0),F32(0,0x9778b0)));
   rx_st4(B+0x30,v4(3,3,F32(0,0x9778b4),F32(0,0x9778b4)));
   rx_st4(B+0x40,lastInner);rx_st4(B+0x60,v4(x,F32(0,0x9774b0),0,0));
   rx_st4(B+0x70,lastRaw);rx_st4(B+0x80,v4(r,F32(0,0x9774b0),0,0));
  }
  rx_st4(B+0x50,v4_zero());
 }
 probe_finish(s,q,&f,mask,1,live);
}
void rx_sim_truck_update_probes(rx_addr s){truck_probe_kernel(s,0,0);}
void rx_sim_truck_update_probes_observed(rx_addr s,rx_v4*live){truck_probe_kernel(s,1,live);}
RX_FUNCTION(0x0045b8f0,rx_sim_truck_update_probes)
