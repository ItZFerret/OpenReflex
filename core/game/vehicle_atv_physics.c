/* Hand translation of original S19/S23/S10. Source notes:
 * core/notes/atv-physics-2026-10-21.md. Keep original SSE associations and x87
 * conversion points; no replacement physics model is used here. */
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_sim.h"
#include "crt.h"

rx_f80 rx_atv_steering_angle(rx_addr s){return rx_f80_from_f32(rx_bf(U32(s,0x5898)));}
RX_FUNCTION(0x0047c460,rx_atv_steering_angle)

void rx_atv_read_controller(rx_addr s,uint32_t controller){
 bike_read_controller_input(controller,s+0x1c);
 SU32(s,0x5970,U32(s,0x4c));SU32(s,0x5974,U32(s,0x50));
}
RX_FUNCTION(0x0049ee00,rx_atv_read_controller)

void rx_atv_set_relative_pose(rx_addr s,rx_addr pose){
 /* Three separate MOVAPS pairs: preserve source/destination overlap behavior. */
 for(unsigned i=0;i<3;i++)rx_st4(s+0x2df0+16*i,rx_ld4(pose+16*i));
 const float w=phys_sub(rx_bf(0x80000000),bike_cos_poly2(rx_bf(0xbe926e98)));
 const float x=bike_cos_poly2(rx_bf(0x3fa47435));
 const rx_v4 rotation=v4(x,0,0,w),inverse=bike_quat_conj(rotation);
 const rx_v4 relative=bike_quat_mul(inverse,rx_ld4(s+0x2e10));
 const rx_v4 xyz=v4(relative.v[0],relative.v[1],relative.v[2],relative.v[0]);
 const float length=phys_sqrt(bike_hsum_021(v4_mul(xyz,xyz)));
 /* Original FLD length, FLD relative.w; CIatan2 returns atan2(length,w),
  * followed by two FSTP m32 round trips before the final SSE multiplication. */
 const float angle=rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(length),rx_f80_from_f32(relative.v[3]),CRT_CW_MAIN),0);
 const rx_v4 forward=bike_quat_mul(rotation,bike_quat_mul(v4_xyz(rx_ld4(0x00d616a0)),inverse));
 const float dot=bike_dot3(forward,xyz);
 const int positive=bike_all3(v4_cmplt(rx_ld4(0x00d61670),v4_splat(dot)));
 SF32(s,0x2e70,phys_mul(angle,positive?2.0f:F32(0,0x009d7b30)));
}
RX_FUNCTION(0x004a0e60,rx_atv_set_relative_pose)

#include "phys_quat.h"
#include "phys_misc.h"
static void peak(rx_addr s,uint32_t field,float value){
 float old=F32(s,field);if(old>value)value=old;SF32(s,field,value);
}
void rx_atv_update_contact_filters(rx_addr s,float dt){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x40;rx_set_sp(F);veh_cookie(F+8,0x34);
 if(U32(s,0x99a8)&&!U32(s,0x9954)&&!U32(s,0x996c)){
  rx_st4(F+0x20,rx_ld4(s+0x730));
  const uint32_t outputs[]={0x9870,0x9874,0x9878,0x98e0,0x98e4,0x98e8};
  const uint32_t limits[]={0x9880,0x98a0,0x98c0,0x98f0,0x9910,0x9930};
  for(unsigned i=0;i<6;i++){
   uint32_t result;
   if(U32(s,0x9838))result=(uint32_t)phys_quat_within_cone_limits(s+limits[i],F+0x20,s+0x9840);
   else result=U32(s,outputs[i])?1u:(uint32_t)phys_quat_within_cone_limits(s+limits[i],F+0x20,s+0x9810);
   SU32(s,outputs[i],result);
  }
 }
 if(!U32(s,0x9834)||!U32(s,0x3c)){
  if(!U32(s,0x99a8)){
   const uint32_t fields[]={0x9964,0x995c,0x9984,0x9980,0x9974,0x9978,0x9958,0x9970};
   for(unsigned i=0;i<8;i++)SU32(s,fields[i],0);SU32(s,0x9988,0x3f800000);
  }
  rx_set_sp(sp);return;
 }
 if(U32(s,0x99a8)){rx_set_sp(sp);return;}
 if(!U32(s,0x9970)&&!U32(s,0x996c)){
  float now=F32(s,0x596c),old=F32(s,0x9964),a=bike_fabs(now),b=bike_fabs(old),rate=0;
  int update=0;
  if(a>F32(0,0x009cd970)&&b==0.0f)update=1;
  else if(a>b){rate=phys_div(phys_sub(now,old),dt);update=1;}
  if(update)peak(s,0x995c,phys_remap_clamped(10,F32(0,0x009cc37c),bike_fabs(rate),rx_bf(0x3ba3d70a),0.75f));
  SF32(s,0x9964,now);
  if(rx_bf(0xbf666666)>now)SU32(s,0x9958,1);
  else{SU32(s,0x9958,0);SU32(s,0x995c,0);}
 }
 if(!U32(s,0x9958)&&!U32(s,0x9954)){
  float now=F32(s,0x5968),old=F32(s,0x9984);
  if(bike_fabs(now)>bike_fabs(old)){
   float rate=phys_div(phys_sub(now,old),dt);
   float sign=rate>=0?1.0f:F32(0,0x009cd9e8);
   SF32(s,0x9988,phys_sub(rx_bf(0x80000000),sign));
   peak(s,0x9974,phys_remap_clamped(10,F32(0,0x0093005c),bike_fabs(rate),rx_bf(0x3e99999a),1));
  }
  SF32(s,0x9984,now);
  now=F32(s,0x596c);old=F32(s,0x9980);
  if(bike_fabs(now)>bike_fabs(old)){
   float rate=phys_div(phys_sub(now,old),dt);
   peak(s,0x9978,phys_remap_clamped(10,F32(0,0x0093005c),bike_fabs(rate),rx_bf(0x3e99999a),1));
  }
  SF32(s,0x9980,now);
  if(rx_bf(0xbecccccd)>now&&F32(s,0x9974)>=rx_bf(0x3e99999a))SU32(s,0x9970,1);
 }
 rx_set_sp(sp);
}
RX_FUNCTION(0x00496310,rx_atv_update_contact_filters)
