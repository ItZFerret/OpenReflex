/* Hand C from complete00456060 (1129 bytes), a closed packed-math leaf.
 * Keep diagnostic dead-frame writes and all four-lane additions, including W. */
#include "fourwheel_axle_torque.h"
#include "phys_vec.h"
#include "fx_util.h"
static rx_v4 axle_product(rx_v4 a,rx_v4 b,int z_left){
 rx_v4 reversed=v4_shuf(b,b,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),reversed),rx_ld4(0x93a1f0));
 reversed=v4_shuf(reversed,reversed,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),reversed),rx_ld4(0x93a200));
 reversed=v4_shuf(reversed,reversed,0x1b);
 rx_v4 z=z_left?v4_mul(v4_lane(a,2),reversed):v4_mul(reversed,v4_lane(a,2));z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);
 return v4_add(v4_add(z,y),v4_add(w,x));
}
static rx_v4 axle_reaction(rx_addr s,rx_v4 wheel_q,rx_v4 input,int first_side){
 rx_v4 negative=v4_and(v4_sub(v4_zero(),input),rx_ld4(0x92db80));
 rx_v4 conjugate=v4_mul(wheel_q,rx_ld4(0x93a220));
 rx_v4 world=axle_product(wheel_q,axle_product(negative,conjugate,0),0);
 world=v4_and(world,rx_ld4(0x92db80));
 rx_v4 root=rx_ld4(s+0x730),local=axle_product(world,root,0);
 /* Only00456254 has a.z as the first MULPS operand; the second side's
  *00456473 uses reverse(b) first. Preserve the NaN operand preference. */
 return axle_product(v4_mul(root,rx_ld4(0x93a220)),local,first_side);
}
static void axle(rx_addr s,unsigned control_delta){
 fx_frame frame;fx_frame_enter0(&frame,0x30,16);rx_addr b=frame.E;
 fx_set_reg(FX_EAX,s);uint32_t debug=U32(0,0xda68c0);fx_set_reg(FX_ECX,debug);
 rx_v4 empty=v4_splat(F32(0,0x92e12c));rx_st4(b+0x10,empty);
 for(unsigned side=0;side<2;side++){
  rx_v4 rear=v4_movss(empty,v4_splat(F32(s,0x5610-control_delta+4*side)));
  rx_addr rear_slot=s+0x960+0xa0*side;
  rx_st4(rear_slot,v4_add(rx_ld4(rear_slot),rear));
  if(debug)rx_st4(side?b:b+0x20,rear);
  rx_v4 counter=v4_sub(v4_zero(),rear);
  rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),counter));
  if(debug)rx_st4(side?b:b+0x20,counter);
  rx_v4 front=v4_movss(empty,v4_splat(F32(s,0x5608-control_delta+4*side)));rx_st4(b+0x20,front);
  rx_v4 reaction=axle_reaction(s,rx_ld4(s+0x7d0+0xa0*side),front,side==0);
  rx_addr front_slot=s+0x820+0xa0*side;
  rx_st4(front_slot,v4_add(rx_ld4(front_slot),front));
  if(debug)rx_st4(b,front);
  /* First reaction is the left ADDPS operand; second reaction is the right. */
  rx_v4 root=rx_ld4(s+0x780);
  rx_st4(s+0x780,side?v4_add(root,reaction):v4_add(reaction,root));
  if(debug)rx_st4(b,reaction);
 }
 fx_frame_leave(&frame);
}

void rx_fourwheel_apply_axle_torques(rx_addr s){axle(s,0);}
void rx_buggy_apply_axle_torques(rx_addr s){axle(s,0x10);}
