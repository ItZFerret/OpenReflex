/* 004d4750 UTV S vf16: four suspension frames. Hand translation of the
 * inlined quaternion products and circular suspension-travel construction.
 * The z lane is deliberately repeated in w by the original unpack sequence. */
#include "vehicle_fourwheel_methods.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
static void axes(rx_addr out){
 rx_st4(out+0x10,v4_sub(v4_zero(),rx_ld4(0x00d616a0)));
 rx_st4(out+0x20,v4_sub(v4_zero(),rx_ld4(0x00d615c0)));
 rx_st4(out+0x30,v4_sub(v4_zero(),rx_ld4(0x00d61680)));
}
static float travel(float radius2,float delta){
 float delta2=rx_muls(delta,delta);
 if(radius2<delta2)return rx_bf(0x3dcccccd);
 return phys_sqrt(phys_sub(radius2,delta2));
}
void rx_sim_utv_geometry(rx_addr s){
 const rx_v4 q=rx_ld4(s+0x730);
 const float frontR=rx_muls(F32(s,0x3fe4),F32(s,0x3fe4));
 const float rearR=rx_muls(F32(s,0x4244),F32(s,0x4244));
 const float steering=F32(s,0x42d4);
 const float frontY=phys_sub(F32(s,0x3e14),F32(s,0x36e4));
 const float rearY=phys_sub(F32(s,0x4074),F32(s,0x36e4));
 for(unsigned i=0;i<4;i++){
  const unsigned right=i&1,rear=i>>1;
  const uint32_t anchor=rear?0x41f0:0x3f90;
  rx_v4 a=rx_ld4(s+anchor);
  if(right)a=v4_mul(v4(-1,1,1,1),a);
  a=v4_sub(a,rx_ld4(s+0x36e0));
  rx_v4 delta=v4_sub(rx_ld4(s+0x190+0xa0*i),rx_ld4(s+0xf0));
  float localY=bike_rotate_inv(q,delta).v[1];
  float dy=phys_sub(localY,a.v[1]);
  const int clipped=(rear?rearR:frontR)<rx_muls(dy,dy);
  float r=travel(rear?rearR:frontR,dy);
  float x=right?(clipped?phys_add(a.v[0],r):phys_add(r,a.v[0])):phys_sub(a.v[0],r);
  rx_v4 frame=v4(x,rear?rearY:frontY,a.v[2],a.v[2]);
  const uint32_t out=0x5b10+0x960*i;
  rx_st4(s+out,frame);
  if(!rear){
   rx_v4 adjust=bike_rotate(rx_ld4(s+(right?0x5620:0x5630)),v4(steering,0,0,0));
   frame=right?v4_add(adjust,frame):v4_sub(frame,adjust);
   rx_st4(s+out,frame);axes(s+out);
   rx_st4(s+out+0x40,rx_ld4(s+(right?0x5620:0x5630)));
  }else{
   rx_v4 adjust=v4(F32(s,0x4774),0,0,0);
   rx_st4(s+out,right?v4_add(frame,adjust):v4_sub(frame,adjust));
   axes(s+out);rx_st4(s+out+0x40,rx_ld4(VEH_RT_W1));
  }
 }
}
RX_FUNCTION(0x004d4750,rx_sim_utv_geometry)
