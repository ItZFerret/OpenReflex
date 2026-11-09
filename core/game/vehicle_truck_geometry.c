/* 004564d0 TrophyTruck S vf16. Original independent front travel directions,
 * shared rear axle direction and rear frame quaternion. */
#include "vehicle_fourwheel_methods.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "crt.h"
static void axes(rx_addr out){
 rx_st4(out+0x10,v4_sub(v4_zero(),rx_ld4(0x00d616a0)));
 rx_st4(out+0x20,v4_sub(v4_zero(),rx_ld4(0x00d615c0)));
 rx_st4(out+0x30,v4_sub(v4_zero(),rx_ld4(0x00d61680)));
}
void rx_sim_truck_geometry(rx_addr s){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0xb0;
 rx_set_sp(F);
 const rx_v4 q=rx_ld4(s+0x730);
 const float frontLength=F32(s,0x3fe4),rearLength=F32(s,0x4244);
 const float frontY=phys_sub(F32(s,0x3e14),F32(s,0x36e4));
 const float rearY=phys_sub(F32(s,0x4074),F32(s,0x36e4));
 for(unsigned right=0;right<2;right++){
  rx_v4 a=rx_ld4(s+0x3f90);
  if(right)a=v4_mul(v4(-1,1,1,1),a);
  a=v4_sub(a,rx_ld4(s+0x36e0));
  rx_v4 local=bike_rotate_inv(q,v4_sub(rx_ld4(s+0x190+0xa0*right),rx_ld4(s+0xf0)));
  rx_v4 dir=v4_sub(local,a);dir.v[2]=F32(VEH_RT_ZERO,8);dir.v[3]=dir.v[2];
  rx_v4 n=bike_normalize(dir);
  if(right?n.v[0]<0.5f:-0.5f<n.v[0]){
   const int pro2=I32(s,0x55f0)==7;
   dir=v4(rx_bf(right?(pro2?0x3f7851ec:0x3f7ae148):(pro2?0xbf7851ec:0xbf7ae148)),rx_bf(pro2?0xbe6147ae:0xbe428f5c),0,0);
  }
  rx_v4 frame=v4_add(v4_mul(bike_normalize(dir),v4_splat(frontLength)),a);frame.v[1]=frontY;
  const uint32_t out=0x5b10+0x960*right;
  rx_st4(s+out,frame);axes(s+out);rx_st4(s+out+0x40,rx_ld4(s+(right?0x5620:0x5630)));
 }
 rx_v4 anchor=v4_sub(rx_ld4(s+0x41f0),rx_ld4(s+0x36e0));anchor.v[0]=F32(VEH_RT_ZERO,0);
 rx_v4 left=bike_rotate_inv(q,v4_sub(rx_ld4(s+0x2d0),rx_ld4(s+0xf0)));
 rx_v4 right=bike_rotate_inv(q,v4_sub(rx_ld4(s+0x370),rx_ld4(s+0xf0)));
 rx_v4 middle=v4_mul(v4_add(right,left),rx_ld4(0x00d615d0));middle.v[0]=F32(VEH_RT_ZERO,0);
 rx_v4 dir=v4_sub(middle,anchor);
 rx_v4 n=bike_normalize(dir);
 if(rx_bf(0xbf333333)<n.v[2])dir=v4(0,rx_bf(0xbe051eb8),-1,-1);
 rx_v4 offset=v4_mul(bike_normalize(dir),v4_splat(rearLength));
 rx_v4 axis=bike_normalize(offset),side=v4_sub(left,middle);
 side=v4_sub(side,v4_mul(v4_splat(bike_dot3(axis,side)),axis));
 side=bike_normalize(side);
 rx_v4 negative=v4_sub(v4_zero(),side);
 float angle=rx_f80_to_f32(crt_ci_atan2(rx_f80_from_f32(negative.v[1]),rx_f80_from_f32(negative.v[0]),CRT_CW_MAIN),0);
 angle=phys_mul(angle,0.5f);
 const float sinNegative=bike_cos_poly2(phys_add(angle,rx_bf(0x3fc90fdb)));
 const float cosNegative=phys_sub(rx_bf(0x80000000),bike_cos_poly2(angle));
 const rx_v4 orientation=v4(0,0,sinNegative,cosNegative);
 const rx_v4 width=v4_sub(v4_zero(),v4_splat(F32(s,0x41f0)));
 rx_v4 center=v4_add(anchor,offset);
 rx_v4 frame=v4_add(v4_mul(side,width),center);frame.v[1]=rearY;
 rx_st4(s+0x6dd0,frame);axes(s+0x6dd0);rx_st4(s+0x6e10,orientation);
 frame=v4_add(v4_mul(negative,width),center);frame.v[1]=rearY;
 rx_st4(s+0x7730,frame);axes(s+0x7730);rx_st4(s+0x7770,orientation);
 rx_set_sp(sp);
}
RX_FUNCTION(0x004564d0,rx_sim_truck_geometry)
