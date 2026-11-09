/*00493c70: four original suspension spring/damper forces and body reaction. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "phys_mech.h"
static rx_v4 suspension_cross(rx_v4 offset,rx_v4 angular){
 /* Original offset.zxy * angular.yzx - offset.yzx * angular.zxy. */
 return v4_xyz(v4_sub(v4_mul(v4_perm(offset,2,0,1,3),v4_perm(angular,1,2,0,3)),v4_mul(v4_perm(offset,1,2,0,3),v4_perm(angular,2,0,1,3))));
}
void rx_atv_apply_suspension(rx_addr s){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,B=A-0x118,F=B-8;rx_set_sp(F);veh_cookie(B,0x114);
 const rx_v4 velocity=rx_ld4(s+0x100),position=rx_ld4(s+0xf0),q=rx_ld4(s+0x730);
 const rx_v4 axis=bike_rotate(q,rx_ld4(0x00d616a0));
 const rx_v4 angular=bike_rotate(q,rx_ld4(s+0x740));
 const uint32_t flags[]={0x9248,0x9398,0x94e8,0x9638};
 for(unsigned i=0;i<4;i++)SU32(s,flags[i],0);
 for(unsigned i=0;i<4;i++){
  rx_v4 worldOffset=v4_sub(rx_ld4(s+0x230+0xa0*i),position);
  rx_v4 local=bike_rotate_inv(q,worldOffset);rx_st4(F+0xd0,local);
  rx_v4 anchor=rx_ld4(s+0x6bd0+0x960*i);
  /*0049448a/00494757 SHUFPS 0xa4: rear anchors retain x/y but
   * replace z/w with original constant009778a4 (-2.6), not frame z. */
  if(i>=2)anchor=v4_shuf(anchor,v4_splat(rx_bf(0xc0266666)),0xa4);
  rx_v4 difference=v4_sub(local,anchor);
  float length=phys_sqrt(bike_hsum_021(v4_mul(difference,difference)));
  float rest=F32(s,i<2?0x3c7c:0x3edc),stiff=F32(s,i<2?0x96dc:0x96d0);
  rx_v4 spring=v4_mul(v4_splat(phys_mul(phys_sub(length,rest),stiff)),axis);
  if(length<F32(s,i<2?0x3de4:0x4044))SU32(s,flags[i],1);
  rx_v4 rootVelocity=v4_add(suspension_cross(worldOffset,angular),velocity);
  rx_v4 relative=v4_sub(rx_ld4(s+0x240+0xa0*i),rootVelocity);
  float speed=bike_dot3(relative,axis);
  int compressing=bike_all3(v4_cmplt(v4_splat(speed),rx_ld4(0x00d61670)));
  float damping=F32(s,i<2?(compressing?0x96e4:0x96e0):(compressing?0x96d8:0x96d4));
  rx_v4 damper=v4_mul(v4_splat(phys_mul(phys_sub(0,speed),damping)),axis);
  rx_v4 force=v4_add(damper,spring);rx_st4(F+0xa0,force);rx_st4(F+0x80,v4_sub(v4_zero(),force));
  uint32_t body=2+i;rx_set_sp(F-8);SU32(F-8,0,0);SU32(F-4,0,body);
  phys_mech_add_force_at_point(F+0xa0,s+0xd0,0x00d61670,body);rx_set_sp(F);
  rx_set_sp(F-8);SU32(F-8,0,0);SU32(F-4,0,0);
  phys_mech_add_force_at_point(F+0x80,s+0xd0,F+0xd0,0);rx_set_sp(F);
  if(i>=2)SF32(s,0x9a44+4*(i-2),bike_dot3(damper,axis));
 }
 rx_set_sp(sp);
}
RX_FUNCTION(0x00493c70,rx_atv_apply_suspension)
