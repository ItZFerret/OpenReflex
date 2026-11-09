/* Hand C from308-instruction original helpers, not host trig. Their instruction
 * sequences are identical except angle/output offsets and relative code sites.
 * Preserve SSE operation grouping, signed-zero/NaN branches, CVTTSS2SI registers,
 * original aligned locals and Float_Abs call/dead-frame stores. Unregistered main. */
#include "fourwheel_steering.h"
#include "phys_vec.h"
#include "fx_util.h"
static float c(rx_addr a){return F32(a,0);}
static float abs_at(rx_addr b,rx_addr ra,float x){
 /*00401000 pushes EBP and aligns8, then only reads the sign mask. */
 SU32(b-4,0,ra);SU32(b-8,0,fx_reg(FX_EBP));rx_set_sp(b);
 return rx_bf(rx_fb(x)&U32(0,0x977570));
}
static float reduced_square(float positive,int reg){
 float scaled=phys_mul(positive,c(0x9774a0));int32_t n=rx_cvtt(scaled);fx_set_reg(reg,(uint32_t)n);
 float reduced=phys_sub(positive,phys_mul((float)n,c(0x97749c)));
 float u=phys_sub(c(0x977498),reduced);return phys_mul(u,u);
}
static float cosine(float u2,float u4){
 float a=phys_add(phys_mul(u2,c(0x935d50)),c(0x935d48));
 a=phys_mul(a,u4);
 float b=phys_add(phys_mul(u2,c(0x935d44)),c(0x935d4c));
 a=phys_add(a,b);a=phys_mul(a,u4);
 b=phys_add(phys_mul(u2,c(0x935d40)),c(0x935d3c));return phys_add(a,b);
}
/* Scalar addss changes only lane0. The retained squared upper lanes are
 * observable in original aligned local spills, not reduced to scalar buffers. */
static rx_v4 squared_distance(rx_v4 a,rx_v4 b){
 rx_v4 d=v4_sub(a,b),q=v4_mul(d,d);
 q.v[0]=phys_add(q.v[0],q.v[2]);q.v[0]=phys_add(q.v[0],q.v[1]);return q;
}
static void steering(rx_addr s,rx_addr code,uint32_t angle_offset,uint32_t out_offset){
 const rx_addr e=rx_sp(),bp=e-4,b=(bp&~15u)-0x50;uint32_t oldbp=fx_reg(FX_EBP);SU32(bp,0,oldbp);fx_set_reg(FX_EBP,bp);rx_set_sp(b);
 float angle=F32(s,angle_offset);SF32(b,0xc,angle);
 float half=phys_mul(angle,c(0x9322cc));SF32(b,0,half);
 float plus=abs_at(b,code+0x31,phys_add(half,c(0x977508)));
 float direct2=reduced_square(plus,FX_EAX),direct4=phys_mul(direct2,direct2);SF32(b,4,direct2);SF32(b,8,direct4);
 float base=abs_at(b,code+0x7d,F32(b,0));float base2=reduced_square(base,FX_ECX),base4=phys_mul(base2,base2);
 float directCos=phys_sub(c(0x9774a8),cosine(base2,base4));
 float directSin=cosine(F32(b,4),F32(b,8));rx_v4 direct=v4(0,directSin,0,directCos);rx_st4(b+0x20,direct);
 SF32(b,0,F32(b,0xc));float magnitude=abs_at(b,code+0x188,F32(b,0));float adjusted;
 if(magnitude>c(0x9774a4)){
  float a=F32(b,0),sign=c(0x931f24);rx_v4 front,other,rear,otherRear;
  /* Original JB includes unordered; epsilon guard normally excludes NaN, but
   * preserve this selection independently rather than assuming ordered input. */
  if(!(a>=0)){
   sign=c(0x9cd9e8);front=rx_ld4(s+0x190);other=rx_ld4(s+0x230);rear=rx_ld4(s+0x2d0);otherRear=rx_ld4(s+0x370);
  }else{front=rx_ld4(s+0x230);other=rx_ld4(s+0x190);rear=rx_ld4(s+0x370);otherRear=rx_ld4(s+0x2d0);}
  SF32(b,0,sign);rx_st4(b+0x10,rear);rx_v4 q=squared_distance(front,rear);float length=phys_sqrt(q.v[0]);rx_st4(b+0x30,v4(length,length,length,length));
  float positive=phys_mul(sign,a);int32_t turns=rx_cvtt(phys_mul(positive,c(0x9774a0)));fx_set_reg(FX_EDX,(uint32_t)turns);
  float u=phys_sub(c(0x977498),phys_sub(positive,phys_mul((float)turns,c(0x97749c))));float u2=phys_mul(u,u),u4=phys_mul(u2,u2);
  float negativeCosine=phys_sub(c(0x9774a8),cosine(u2,u4));
  rx_v4 outerDistance=squared_distance(other,otherRear);rx_st4(b+0x40,outerDistance);
  rx_v4 trackDistance=squared_distance(otherRear,rear);rx_st4(b+0x10,trackDistance);
  float sine=phys_add(phys_mul(u2,c(0x935d68)),c(0x935d64));sine=phys_mul(sine,u4);
  float t=phys_add(phys_mul(u2,c(0x935d60)),c(0x935d5c));sine=phys_add(sine,t);sine=phys_mul(sine,u4);
  t=phys_add(phys_mul(u2,c(0x935d58)),c(0x935d54));sine=phys_add(sine,t);sine=phys_mul(sine,u);sine=phys_mul(sine,sign);
  float tangent=abs_at(b,code+0x35a,phys_div(sine,negativeCosine));
  float denominator=phys_add(phys_div(F32(b,0x30),tangent),phys_sqrt(F32(b,0x10)));
  float ratio=phys_div(phys_sqrt(F32(b,0x40)),denominator);
  float direction=ratio>=c(0x92e12c)?c(0x931f24):c(0x9cd9e8);SF32(b,4,direction);
  float x=abs_at(b,code+0x3aa,ratio),one=c(0x931f24),z=x>=one?phys_div(one,x):x,z2=phys_mul(z,z);
  float divisor=phys_add(phys_mul(z2,c(0x93a728)),c(0x93a724));divisor=phys_mul(divisor,z2);divisor=phys_add(divisor,one);
  float numerator=phys_add(phys_mul(z2,c(0x93a720)),one);numerator=phys_mul(numerator,z);
  float atan=phys_mul(phys_div(one,divisor),numerator);
  if(x>=one)atan=phys_sub(c(0x977508),atan);
  adjusted=phys_mul(phys_mul(atan,F32(b,4)),F32(b,0));
 }else adjusted=F32(b,0);
 half=phys_mul(adjusted,c(0x9322cc));SF32(b,8,half);
 plus=abs_at(b,code+0x44e,phys_add(half,c(0x977508)));
 float indirect2=reduced_square(plus,FX_EAX),indirect4=phys_mul(indirect2,indirect2);SF32(b,0,indirect2);SF32(b,4,indirect4);
 base=abs_at(b,code+0x49a,F32(b,8));base2=reduced_square(base,FX_ECX);base4=phys_mul(base2,base2);
 float indirectCos=phys_sub(c(0x9774a8),cosine(base2,base4));float indirectSin=cosine(F32(b,0),F32(b,4));rx_v4 indirect=v4(0,indirectSin,0,indirectCos);
 direct=rx_ld4(b+0x20);
 if(!(F32(b,0xc)>=0)){rx_st4(s+out_offset+16,direct);rx_st4(s+out_offset,indirect);}
 else{rx_st4(s+out_offset,direct);rx_st4(s+out_offset+16,indirect);}
 fx_set_reg(FX_EBP,oldbp);rx_set_sp(e);
}
void rx_fourwheel_steering_quaternions(rx_addr s){steering(s,0x4d4190,0x5618,0x5620);}
RX_FUNCTION(0x004d4190,rx_fourwheel_steering_quaternions)
void rx_buggy_steering_quaternions(rx_addr s){steering(s,0x4b9cb0,0x5608,0x5610);}
RX_FUNCTION(0x004b9cb0,rx_buggy_steering_quaternions)
