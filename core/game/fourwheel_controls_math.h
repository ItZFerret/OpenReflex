/* Private hand-C building blocks for the pending vf9 owners. No registrations.
 * These preserve the original packed operation order; they are not replacements
 * for the not-yet-complete control methods or independently accepted entries. */
#pragma once
#include "phys_vec.h"
#include "fx_util.h"

/* 004d94b9..004d951b and its second quaternion product. In particular, the
 * z product's operands are reverse-shuffled b first, a.z second. Replacing this
 * with a scalar quaternion formula changes NaN propagation and associations. */
static inline rx_v4 fw9_quat_product(rx_v4 a,rx_v4 b){
 rx_v4 reverse=v4_shuf(b,b,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),reverse),rx_ld4(0x93a1f0));
 reverse=v4_shuf(reverse,reverse,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),reverse),rx_ld4(0x93a200));
 reverse=v4_shuf(reverse,reverse,0x1b);
 rx_v4 z=v4_mul(v4_mul(reverse,v4_lane(a,2)),rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);
 return v4_add(v4_add(z,y),v4_add(w,x));
}

static inline rx_v4 fw9_body_velocity(rx_v4 velocity,rx_v4 rotation){
 rx_v4 first=fw9_quat_product(v4_and(velocity,rx_ld4(0x92db80)),rotation);
 return fw9_quat_product(v4_mul(rotation,rx_ld4(0x93a220)),first);
}

/* ADDSS changes only lane zero; retaining the other squared components matters
 * in the vf9 local spills and not merely in its scalar speed calculation. */
static inline rx_v4 fw9_sum_021(rx_v4 p){
 rx_v4 t=v4_shuf(p,p,0x66);
 p.v[0]=phys_add(p.v[0],t.v[0]);
 t=v4_shuf(t,t,0x55);
 p.v[0]=phys_add(p.v[0],t.v[0]);
 return p;
}
static inline rx_v4 fw9_sum_012(rx_v4 p){
 rx_v4 t=v4_shuf(p,p,0x99);
 p.v[0]=phys_add(p.v[0],t.v[0]);
 t=v4_shuf(t,t,0x55);
 p.v[0]=phys_add(p.v[0],t.v[0]);
 return p;
}
static inline rx_v4 fw9_cmpneq(rx_v4 a,rx_v4 b){
 return v4_mask(a.v[0]!=b.v[0],a.v[1]!=b.v[1],a.v[2]!=b.v[2],a.v[3]!=b.v[3]);
}

/* 004d9823..004d9878 and 004da330..004da375. All lanes retain the
 * original zero/infinity fallback rather than using a host normalize routine. */
static inline rx_v4 fw9_normalize_squared(rx_v4 value,rx_v4 squared){
 rx_v4 sum=fw9_sum_012(squared);
 sum=v4_lane(sum,0);
 rx_v4 length=v4_sqrt(sum),finite=fw9_cmpneq(sum,rx_ld4(0x92dc20));
 rx_v4 nonzero=fw9_cmpneq(length,v4_zero());
 rx_v4 quotient=v4_div(value,length);
 return v4_or(v4_and(v4_and(nonzero,quotient),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static inline rx_v4 fw9_normalize(rx_v4 value){return fw9_normalize_squared(value,v4_mul(value,value));}

/* 004d98b6..004d999f. The four-lane fourth-power spill is part of the
 * outer method's original frame; the caller supplies its genuine B+50 slot.
 * Output is the signed asin approximation before pi/2 subtraction and the
 * cross-product sign selection. No host asin/acos or reciprocal substitute. */
static inline rx_v4 fw9_asin(rx_v4 value,rx_addr fourth_power_slot){
 rx_v4 zero=v4_zero();
 rx_v4 squared=v4_mul(value,value);
 rx_v4 complement=v4_sub(rx_ld4(0xd61690),squared);
 rx_v4 magnitude_complement=v4_max(v4_sub(zero,complement),complement);
 rx_v4 magnitude=v4_max(v4_sub(zero,value),value);
 rx_v4 root=v4_sqrt(magnitude_complement);
 rx_v4 high=v4_cmple(v4_splat(F32(0,0x977780)),magnitude);
 rx_v4 reduced=v4_or(v4_and(root,high),v4_andn(high,magnitude));
 rx_v4 u=v4_mul(reduced,reduced),u4=v4_mul(u,u);
 rx_st4(fourth_power_slot,u4);
 rx_v4 denominator=v4_add(v4_mul(u,rx_ld4(0x935bc0)),rx_ld4(0x935bb0));
 denominator=v4_mul(denominator,u4);
 rx_v4 term=v4_add(v4_mul(u,rx_ld4(0x935ba0)),rx_ld4(0xd61690));
 denominator=v4_add(denominator,term);
 rx_v4 inverse=v4_div(rx_ld4(0x92dba0),denominator);
 rx_v4 numerator=v4_add(v4_mul(u,rx_ld4(0x935b70)),rx_ld4(0x935b80));
 numerator=v4_mul(numerator,rx_ld4(fourth_power_slot));
 term=v4_add(v4_mul(u,rx_ld4(0x935b90)),rx_ld4(0xd61690));
 numerator=v4_add(numerator,term);
 numerator=v4_mul(numerator,reduced);
 rx_v4 result=v4_mul(inverse,numerator);
 result=v4_or(v4_and(v4_sub(rx_ld4(0x935b00),result),high),v4_andn(high,result));
 rx_v4 positive=v4_cmple(rx_ld4(0xd61660),value);
 return v4_or(v4_and(result,positive),v4_andn(positive,v4_sub(zero,result)));
}

/* Pure packed portion of the forward-velocity heading correction, ending
 * before the Lerp_Clamped call. Callers provide the actual family frame slots;
 * this helper does not allocate or invent an original call frame. */
typedef struct { int active; float angle,blend; } fw9_heading_result;
static inline fw9_heading_result fw9_heading(rx_v4 body,rx_v4 world,
 rx_addr masks,rx_addr zero_slot,rx_addr speed_slot){
 fw9_heading_result result={0,0,0};
 rx_v4 comparison=v4_cmplt(rx_ld4(0xd61670),v4_lane(body,2));
 rx_st4(masks,comparison);
 uint32_t count=v4_lb(comparison,2)+v4_lb(comparison,1)+v4_lb(comparison,0);
 fx_set_reg(FX_EAX,v4_lb(comparison,1));fx_set_reg(FX_ECX,count);
 if(count!=0xfffffffdu)return result;
 rx_v4 square=fw9_sum_021(v4_mul(world,world));
 float speed=phys_sqrt(square.v[0]);SF32(speed_slot,0,speed);
 rx_v4 one=v4_splat(F32(0,0x931f24)),flat=v4_zero();
 flat=v4_shuf(flat,one,0xee);flat=v4_movss(flat,one);
 rx_v4 direction=fw9_normalize(v4_mul(flat,body));
 rx_v4 dot=fw9_sum_012(v4_mul(direction,rx_ld4(0xd61680)));
 rx_v4 cosine=v4_max(rx_ld4(0xd61670),v4_lane(dot,0));
 cosine=v4_lane(cosine,0);
 rx_st4(zero_slot,v4_zero());
 rx_v4 angle=v4_sub(rx_ld4(0x935b00),fw9_asin(cosine,masks));
 /* Original cross-product shuffles and operand order, not a host cross(). */
 rx_v4 forward=v4_shuf(rx_ld4(0xd61680),rx_ld4(0xd61680),0xc9);
 direction=v4_shuf(direction,direction,0xd2);
 rx_v4 right=v4_shuf(direction,direction,0xd2);
 rx_v4 other=v4_shuf(forward,forward,0xc9);
 rx_v4 cross=v4_sub(v4_mul(direction,forward),v4_mul(right,other));
 cross=v4_and(cross,rx_ld4(0x92db80));
 comparison=v4_cmplt(v4_lane(cross,1),rx_ld4(0xd61670));rx_st4(masks,comparison);
 fx_set_reg(FX_EDX,v4_lb(comparison,1));
 uint32_t cross_count=v4_lb(comparison,2)+v4_lb(comparison,1)+v4_lb(comparison,0);
 fx_set_reg(FX_EAX,cross_count);
 if(cross_count==count)angle=v4_sub(v4_zero(),angle);
 float blend=phys_div(phys_sub(speed,F32(0,0x935890)),F32(0,0x9322d0));
 float limit=F32(0,0x931f24);
 /* COMISS/JB treats unordered as the lower branch; zero>=NaN is false,
  * retaining NaN rather than silently selecting a numeric clamp endpoint. */
 if(blend>=limit)blend=limit;
 if(0>=blend)blend=0;
 result.active=1;result.angle=angle.v[0];result.blend=blend;return result;
}

/* Contact count is packed and uses the x86 RCPPS approximation. All original
 * additions have the newly loaded wheel vector/one on the left. UTV supplies
 * four contacts; the Buggy/Truck callers supply only the actual rear pair. */
typedef struct { uint32_t present,velocity; } fw9_contact;
static inline int fw9_contact_average(rx_addr s,const fw9_contact* wheel,
 unsigned count,rx_addr output_slot){
 rx_v4 sum=v4_splat(F32(0,0x92e12c)),n=sum,one=rx_ld4(0xd61650);
 int any=0;
 for(unsigned i=0;i<count;i++)if(U32(s,wheel[i].present)){
  sum=v4_add(rx_ld4(s+wheel[i].velocity),sum);
  n=v4_add(one,n);any=1;
 }
 if(!any)return 0;
 for(unsigned i=0;i<4;i++)n.v[i]=rx_rcp(n.v[i]);
 rx_st4(output_slot,v4_mul(n,sum));return 1;
}
