/* Hand-written semantic C for the complete closed1623B004d1680 and004b6ed0.
 * Both403-instruction bodies have identical operations/control flow; exact
 * evidence comparison finds only16 family-owned memory operands, all -10h in
 * Buggy. Layout records below express those proven differences, not an alias
 * between simulation objects. No generated instruction interpreter is used. */
#include "fourwheel_pair_solve.h"
#include "fx_util.h"
#include "phys_vec.h"
typedef struct {
 uint32_t rear_marker[2],front_marker[2],active[4],rear_normal[2],front_rolling[2];
} PairLayout;
static const PairLayout utv_truck={{0x83e4,0x8534},{0x8144,0x8294},{0x8180,0x82d0,0x8420,0x8570},{0x8450,0x85a0},{0x81c0,0x8310}};
static const PairLayout buggy={{0x83d4,0x8524},{0x8134,0x8284},{0x8170,0x82c0,0x8410,0x8560},{0x8440,0x8590},{0x81b0,0x8300}};
/* All quaternion products use scalar-lane-left X/Y/W. Some Z multiplies use
 * the shuffled right quaternion on the left, and the first axis rotation adds
 * X+W rather than W+X. Preserve both choices explicitly. */
static rx_v4 product(rx_v4 a,rx_v4 b,int z_lane_left,int x_plus_w){
 rx_v4 reverse=v4_shuf(b,b,0x1b),x=v4_mul(v4_lane(a,0),reverse);x=v4_mul(x,rx_ld4(0x93a1f0));
 reverse=v4_shuf(reverse,reverse,0xb1);rx_v4 y=v4_mul(v4_lane(a,1),reverse);y=v4_mul(y,rx_ld4(0x93a200));
 reverse=v4_shuf(reverse,reverse,0x1b);rx_v4 z=z_lane_left?v4_mul(v4_lane(a,2),reverse):v4_mul(reverse,v4_lane(a,2));z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);return v4_add(v4_add(z,y),x_plus_w?v4_add(x,w):v4_add(w,x));
}
static uint32_t store_mask(rx_addr b,rx_v4 mask){
 rx_st4(b+0x20,mask);fx_set_reg(FX_ECX,U32(b,0x24));uint32_t sum=U32(b,0x28)+U32(b,0x24)+U32(b,0x20);fx_set_reg(FX_EDX,sum);return sum;
}
static rx_v4 normalized_velocity(rx_v4 velocity){
 rx_v4 square=v4_mul(velocity,velocity),sum=v4_splat(phys_hsum3(square)),length=v4_sqrt(sum);
 rx_v4 finite=v4_mask(sum.v[0]!=F32(0x92dc20,0),sum.v[1]!=F32(0x92dc20,4),sum.v[2]!=F32(0x92dc20,8),sum.v[3]!=F32(0x92dc20,12));
 rx_v4 nonzero=v4_mask(length.v[0]!=0,length.v[1]!=0,length.v[2]!=0,length.v[3]!=0);
 return v4_or(v4_and(v4_and(nonzero,v4_div(velocity,length)),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static void solve(rx_addr s,const PairLayout*l){
 fx_frame frame;fx_frame_enter0(&frame,0x40,16);rx_addr b=frame.E;fx_set_reg(FX_EAX,s);
 rx_v4 root=rx_ld4(s+0x730),inverse=v4_mul(root,rx_ld4(0x93a220));
 rx_v4 side_first=product(v4_and(rx_ld4(0x92db80),rx_ld4(0xd615c0)),inverse,0,0);
 rx_st4(b+0x20,v4_lane(root,2));rx_st4(b+0x10,v4_lane(root,3));
 rx_v4 side=product(root,side_first,0,1);rx_st4(b+0x30,side);
 rx_v4 forward_first=product(v4_and(rx_ld4(0xd61680),rx_ld4(0x92db80)),inverse,0,0);
 rx_v4 forward=product(root,forward_first,0,0),velocity=rx_ld4(s+0x100),square=v4_mul(velocity,velocity);
 float speed=phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]));
 float ratio=phys_div(speed,F32(0x935aac,0)),one=F32(0x931f24,0),half=F32(0x9322cc,0),limit=F32(0x9cd9ac,0);
 /* COMISS ratio,one/JBE keeps unordered ratio, rather than MAX/MIN. */
 if(ratio>one)ratio=one;SF32(b,0xc,ratio);
 float alignment=phys_hsum3(v4_mul(normalized_velocity(velocity),forward));
 rx_v4 negative_n=v4_sub(v4_zero(),rx_ld4(0xd61650));
 uint32_t front_negative=store_mask(b,v4_cmplt(v4_lane(rx_ld4(s+0x920),0),negative_n));int skip=0;
 if(front_negative==0xfffffffdu)skip=store_mask(b,v4_cmplt(v4_lane(rx_ld4(s+0x9c0),0),negative_n))==0xfffffffdu;
 if(!skip&&(U32(s,l->rear_marker[0])||U32(s,l->rear_marker[1]))&&(U32(s,l->front_marker[0])||U32(s,l->front_marker[1])))skip=1;
 if(!skip){
  float a,bias;
  if(0>=ratio||ratio>=one)a=one;else a=phys_add(phys_mul(ratio,0),one);
  if(0>=ratio)bias=one;else if(ratio>=one)bias=limit;else bias=phys_sub(one,phys_mul(ratio,F32(0x977874,0)));
  float weight=bias;if(alignment>0)weight=phys_add(phys_mul(phys_sub(one,alignment),bias),phys_mul(a,alignment));SF32(b,0xc,weight);
  rx_v4 omega=v4_and(rx_ld4(s+0x740),rx_ld4(0x92db80));
  rx_v4 world=product(root,product(omega,inverse,0,0),1,0);
  rx_v4 normal=rx_ld4(s+l->rear_normal[U32(s,l->active[2])?0:1]);
  rx_v4 component=v4_mul(v4_splat(phys_dot3(world,normal)),normal);
  rx_v4 adjusted=v4_and(v4_sub(world,v4_mul(v4_splat(phys_sub(one,weight)),component)),rx_ld4(0x92db80));
  rx_st4(s+0x740,product(inverse,product(adjusted,root,0,0),0,0));
 }
 /* Only exactly one complete left/right contact pair enters this correction.
  * ECX is the raw left-front flag on all skipped paths. */
 uint32_t left_front=U32(s,l->active[0]);fx_set_reg(FX_ECX,left_front);
 int left=left_front&&U32(s,l->active[2])&&!U32(s,l->active[1])&&!U32(s,l->active[3]);
 int right=!left_front&&!U32(s,l->active[2])&&U32(s,l->active[1])&&U32(s,l->active[3]);
 if(left||right){
  float direction=phys_dot3(rx_ld4(s+l->front_rolling[right?1:0]),rx_ld4(b+0x30));
  rx_v4 omega=rx_ld4(s+0x740),z=v4_lane(omega,2),mask=left?v4_cmplt(rx_ld4(0xd61670),z):v4_cmplt(z,rx_ld4(0xd61670));
  if(store_mask(b,mask)==0xfffffffdu){
   float gain=one;if(direction>0)gain=phys_add(phys_mul(phys_sub(one,direction),half),phys_mul(direction,limit));
   rx_v4 scaled=v4_mul(v4_splat(gain),z);rx_st4(s+0x740,v4_shuf(omega,scaled,0xa4));
  }
 }
 fx_frame_leave(&frame);
}
void rx_utv_truck_pair_angular_solve(rx_addr s){solve(s,&utv_truck);}
void rx_buggy_pair_angular_solve(rx_addr s){solve(s,&buggy);}
