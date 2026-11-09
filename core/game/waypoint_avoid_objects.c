/* 00520ba0 complete BehaviorAvoidObjects::vf4. The float argument is unused,
 * but retains actual virtual-call native FP ABI. Collision collection remains
 * a required separate canonical owner: no empty-query or geometry fallback. */
#include "waypoint_avoid_objects.h"
#include "build_activity_util.h"
#include "race.h"
#include "phys_vec.h"
#include "phys_crt.h"
#include "waypoint_avoid_collect.h"
static float xf(float x){return rx_f80_to_f32(rx_f80_from_f32(x),0);}
/* Original SIMD association includes tx+tw at the outer rotation product. */
static rx_v4 product(rx_v4 a,rx_v4 b,int x_first){
 rx_v4 rev=v4_perm(b,3,2,1,0),r2=v4_perm(rev,1,0,3,2),r3=v4_perm(r2,3,2,1,0);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),rev),v4_ld(0x93a1f0));
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),r2),v4_ld(0x93a200));
 rx_v4 z=v4_mul(v4_mul(r3,v4_lane(a,2)),v4_ld(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);return v4_add(v4_add(z,y),x_first?v4_add(x,w):v4_add(w,x));
}
static float sqrt_call(float x){float result;ACT_CALLR(result,1,phys_sqrtf(xf(x)));return result;}
static float square_call(float x){float result;ACT_CALLR(result,0,rx_waypoint_avoid_square(x));return result;}
static float norm3(float x,float y,float z){return rx_adds(rx_adds(rx_muls(x,x),rx_muls(y,y)),rx_muls(z,z));}
static int lateral_kind(uint32_t kind){return kind==0||kind==1||kind==3||kind==4||kind==7||kind==17||kind==18||kind==19;}
static int steered_kind(uint32_t kind){return kind==0||kind==1||kind==3||kind==4||kind==7||kind==18||kind==19;}
static int vector_kind(uint32_t kind){return kind==2||kind==5||kind==6||kind==8||kind==17||kind==20;}
static rx_addr vehicle(rx_addr b){return U32(U32(U32(b,8),0x74),0x1ac);}
uint32_t rx_waypoint_avoid_objects_update(rx_addr b,float dt){
 (void)dt;ACT_FRAME_EBP(F,16,0x3a4,3);
 if(!U32(b,4))goto done;
 SU32(b,0x354,0);if(U32(vehicle(b),0x280))goto done;
 SU32(F,0x18,0);SU32(0,0xdcf2a0,U32(0,0xdcf2a0)|7u);
 rx_addr v=vehicle(b);SF32(b,0x358,rx_adds(rx_muls(F32(v,0x150),F32(0,0x977e3c)),F32(0,0x935aac)));
 rx_v4 q=v4_ld(v+0xe0),conjugate=v4_mul(q,v4_ld(0x93a220));
 rx_v4 local=v4_xyz(v4(0,F32(0,0x931f24),F32(0,0x92fe48),F32(0,0x92fe48)));
 rx_v4 origin=product(q,product(local,conjugate,0),1);
 origin=v4_add(origin,v4(F32(v,0xc0),F32(v,0xc4),F32(v,0xc8),F32(v,0xc8)));
 rx_v4 direction=product(q,product(v4_and(v4_ld(0xd61680),v4_ld(0x92db80)),conjugate,0),1);
 v4_st(0xdcf050,origin);v4_st(0xdcf040,v4_add(v4_mul(direction,v4_splat(F32(b,0x358))),origin));
 rx_addr d=U32(b,8),a=U32(U32(d,0x74),0x94);int32_t collected;
 ACT_CALLR(collected,1,rx_waypoint_avoid_collect(U32(U32(a,0x234),0x20134),0xdcf060));
 if(collected>0){
  const uint32_t route_kind=U32(d,0xee8);int32_t count=0;
  for(int32_t i=0;i<collected&&count<12;i++,count++){
   rx_addr src=0xdcf060+(uint32_t)i*0x30,dst=F+0xa0+(uint32_t)count*0x40;
   SU32(dst,4,U32(src,4));SU32(dst,0,U32(src,0));SU32(dst,8,U32(src,8));
   float speed=sqrt_call(norm3(F32(src,0x20),F32(src,0x24),F32(src,0x28)));
   SF32(dst,0x10,speed);SU32(dst,0x30,2);
   float side=0;if(route_kind==3){ACT_CALLR(side,0,rx_waypoint_avoid_track_lateral(d,dst));side=rx_subs(side,F32(0,0x9322cc));}
   SF32(dst,0x34,side);
  }
  SU32(F,0x18,(uint32_t)count);int32_t appended=I32(b,0x350);
  for(int32_t i=0;i<count;i++){
   rx_addr src=F+0xa0+(uint32_t)i*0x40;int overlaps=0;
   if(I32(b,0x350)>0){float r2=square_call(F32(src,0x10));
    for(int32_t j=0;j<I32(b,0x350);j++){
     rx_addr prior=b+0x50+(uint32_t)j*0x40;
     float y=rx_subs(F32(prior,4),F32(src,4)),x=rx_subs(F32(prior,0),F32(src,0)),z=rx_subs(F32(prior,8),F32(src,8));
     float dist=rx_adds(rx_adds(rx_muls(y,y),rx_muls(x,x)),rx_muls(z,z));float other=square_call(F32(prior,0x10));
     if(rx_adds(other,r2)>dist){overlaps=1;break;}
    }
   }
   if(!overlaps){if(appended>=12)break;rx_addr dst=b+0x50+(uint32_t)appended++*0x40;
    SF32(dst,0,xf(F32(src,0)));SF32(dst,4,xf(F32(src,4)));SF32(dst,8,xf(F32(src,8)));
    SU32(dst,0x30,U32(src,0x30));SF32(dst,0x10,xf(F32(src,0x10)));SF32(dst,0x34,xf(F32(src,0x34)));
   }
  }
  SU32(b,0x350,(uint32_t)appended);
  /* Nearby pairs merge into their first record; removals replace with the last
   * sparse record, and recheck that same slot (the original does not memmove). */
  if(I32(b,0x350)>0)SU32(F,0x18,1);
  for(int32_t i=0;i<I32(b,0x350);i++){
   rx_addr first=b+0x50+(uint32_t)i*0x40;ACT_CALL(0,rx_waypoint_obstacle_copy_initial(F+0x60,first));
   int32_t j=i+1;float merged=F32(0,0x931f24);
   while(j<I32(b,0x350)){
    rx_addr next=b+0x50+(uint32_t)j*0x40;
    float x=rx_subs(F32(first,0),F32(next,0)),y=rx_subs(F32(first,4),F32(next,4)),z=rx_subs(F32(first,8),F32(next,8));
    float dist=rx_adds(rx_adds(rx_muls(y,y),rx_muls(x,x)),rx_muls(z,z));
    if(F32(0,0x9cd998)>dist){
     if(dist>F32(0,0x931f24)){
      SF32(F,0x60,rx_adds(F32(F,0x60),F32(next,0)));SF32(F,0x64,rx_adds(F32(next,4),F32(F,0x64)));
      SF32(F,0x68,rx_adds(F32(next,8),F32(F,0x68)));SF32(F,0x70,rx_adds(F32(next,0x10),F32(F,0x70)));
      SF32(F,0x94,rx_adds(F32(next,0x34),F32(F,0x94)));merged=rx_adds(merged,F32(0,0x931f24));
     }
     ACT_CALL(0,rx_waypoint_obstacle_copy(next,b+0x10+(uint32_t)I32(b,0x350)*0x40));SU32(b,0x350,U32(b,0x350)-1);
    }else j++;
    if(j==I32(b,0x350)&&merged>F32(0,0x931f24)){
     SF32(first,0,rx_divs(F32(F,0x60),merged));SF32(first,4,rx_divs(F32(F,0x64),merged));SF32(first,8,rx_divs(F32(F,0x68),merged));
     SU32(first,0x10,U32(F,0x70));SF32(first,0x34,rx_divs(F32(F,0x94),merged));
     SU32(F,0x60,U32(first,0));SU32(F,0x64,U32(first,4));SU32(F,0x68,U32(first,8));merged=F32(0,0x931f24);j=i+1;
    }
   }
   SU32(F,0x18,(uint32_t)i+2u);
  }
 }
 if(I32(b,0x350)!=0){
  v=vehicle(b);d=U32(b,8);SU32(F,0x3a0,U32(v,0xc0));SU32(F,0x3a4,U32(v,0xc8));
  float x=F32(v,0x130),z=F32(v,0x138),n=rx_adds(rx_muls(z,z),rx_muls(x,x));
  if(n!=0){float inv=rx_divs(F32(0,0x931f24),sqrt_call(n));x=rx_muls(inv,x);z=rx_muls(z,inv);}
  SF32(F,0x40,x);SF32(F,0x44,z);
  uint32_t kind=U32(b,0x35c);if(lateral_kind(kind)){float side;ACT_CALLR(side,0,rx_race_track_lateral(d,v+0xc0));SF32(F,0x24,side);}
  else if(kind==2||kind==5||kind==6||kind==8||kind==20)SU32(F,0x24,0);
  for(int32_t i=0;i<I32(b,0x350);i++){
   rx_addr record=b+0x50+(uint32_t)i*0x40;float dx=rx_subs(F32(record,0),F32(F,0x3a0)),dz=rx_subs(F32(record,8),F32(F,0x3a4));
   SF32(F,0x50,F32(record,0));SF32(F,0x54,F32(record,8));SF32(F,0x28,dx);SF32(F,0x2c,dz);
   float distance=sqrt_call(rx_adds(rx_muls(dz,dz),rx_muls(dx,dx))),gain=F32(F,0x18);
   uint32_t obstacle=U32(record,0x30),base=0;
   if(obstacle==0)base=0x9e32e8;else if(obstacle==1)base=0x9e46b8;else if(obstacle==2)base=0x9e4058;else if(obstacle==3)base=0x9e3de8;
   if(base){uint32_t index=U32(U32(d,0x74),0x8f4)+(U32(d,0x328)*3+U32(d,0x32c))*4;base+=index*16;
    ACT_CALLR(gain,4,rx_race_pow_difference(F32(0,base+12),xf(F32(0,base)),xf(F32(0,base+4)),xf(F32(0,base+8)),xf(distance)));
   }
   float magnitude=rx_adds(rx_muls(rx_muls(rx_divs(F32(record,0x10),F32(0,0x9322d0)),F32(0,0x931d94)),gain),gain);
   SF32(F,0x18,magnitude);float add=magnitude;int apply=1;
   if(steered_kind(kind)){
    float side=F32(record,0x34),abs_side=rx_bf(rx_fb(side)&0x7fffffff);
    if(!(F32(0,0x92ff84)>abs_side)){if(!(0.0f>side))add=rx_subs(F32(0,0x9774a8),magnitude);}
    else{float ab=rx_bf(rx_fb(magnitude)&0x7fffffff);add=!(F32(0,0x9322cc)>F32(F,0x24))?ab:rx_subs(F32(0,0x9774a8),ab);}
   }else if(vector_kind(kind)){
    rx_addr vector=act_vcall(vehicle(b),0x38,0,0);float vx=F32(vector,0),vz=F32(vector,8),len=rx_adds(rx_muls(vz,vz),rx_muls(vx,vx));
    if(len!=0){float inv=rx_divs(F32(0,0x931f24),sqrt_call(len));vx=rx_muls(inv,vx);vz=rx_muls(vz,inv);}
    float dot=rx_adds(rx_muls(vz,dz),rx_muls(vx,dx));if(!(0.0f>dot))add=rx_subs(F32(0,0x9774a8),magnitude);
   }else apply=0;
   if(apply)SF32(b,0x354,rx_adds(add,F32(b,0x354)));
   float attenuation;ACT_CALLR(attenuation,0,rx_race_heading_factor(F+0x50,F+0x3a0,F+0x40));
   SF32(b,0x354,rx_muls(rx_subs(F32(0,0x931f24),attenuation),F32(b,0x354)));
  }
  SF32(d,0x60,rx_adds(F32(d,0x60),F32(b,0x354)));
 }
 for(int32_t i=0;i<I32(b,0x350);){
  rx_addr record=b+0x50+(uint32_t)i*0x40;v=vehicle(b);
  float x=rx_muls(rx_subs(F32(record,0),F32(v,0xc0)),F32(v,0x130));
  float y=rx_muls(F32(v,0x134),rx_subs(F32(record,4),F32(v,0xc4))),z=rx_muls(F32(v,0x138),rx_subs(F32(record,8),F32(v,0xc8)));
  float dot=rx_adds(rx_adds(y,x),z);if(!(0.0f>dot))i++;
  else{if(i!=I32(b,0x350)-1)ACT_CALL(0,rx_waypoint_obstacle_copy(record,b+0x10+U32(b,0x350)*0x40));SU32(b,0x350,U32(b,0x350)-1);}
 }
 done:ACT_END(F);return 0;
}
RX_FUNCTION(0x00520ba0,rx_waypoint_avoid_objects_update)
