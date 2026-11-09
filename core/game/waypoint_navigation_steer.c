/*0052e850: gate-plane correction and actual vehicle-frame steering gain.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
static float absf_bits(float x){return rx_bf(rx_fb(x)&0x7fffffffu);}
static float sqrt_argument(float x){return rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(x),0));}
void rx_waypoint_navigation_steer(rx_addr o){
 ACT_FRAME_EBP(F,16,0x48,2);rx_addr v=U32(U32(U32(o,8),0x74),0x1ac);
 float vx=F32(v,0x130),vz=F32(v,0x138),px=F32(v,0xc0),pz=F32(v,0xc8),tx=F32(o,0x50),tz=F32(o,0x58);
 float speed2=rx_adds(rx_muls(vz,vz),rx_muls(vx,vx));SF32(F,0x20,tx);SF32(F,0x40,tx);SF32(F,0x1c,tz);SF32(F,0x44,tz);SF32(F,0x48,px);SF32(F,0x4c,pz);SF32(F,0x38,vx);SF32(F,0x3c,vz);SF32(F,0x10,speed2);
 if(speed2!=0){float length;ACT_CALLR(length,1,sqrt_argument(speed2));float inv=rx_divs(F32(0,0x931f24),length);vx=rx_muls(inv,F32(F,0x38));SF32(F,0x38,vx);vz=rx_muls(F32(F,0x3c),inv);SF32(F,0x3c,vz);}
 rx_addr d=U32(o,8);v=U32(U32(d,0x74),0x1ac);float plane=rx_adds(rx_muls(F32(v,0xc4),F32(o,0x78)),rx_muls(F32(o,0x68),F32(v,0xc0)));plane=rx_adds(plane,rx_muls(F32(v,0xc8),F32(o,0x88)));plane=rx_adds(plane,F32(o,0x98));
 if(F32(0,0x9cd970)>absf_bits(plane)){tx=rx_adds(rx_muls(F32(o,0x40),plane),tx);tz=rx_adds(tz,rx_muls(F32(o,0x48),plane));SF32(F,0x40,tx);SF32(F,0x44,tz);}
 float ax=F32(o,0x20),az=F32(o,0x28),dx=rx_subs(px,F32(F,0x40)),dz=rx_subs(pz,F32(F,0x44));SF32(F,0x24,az);SF32(F,0x20,ax);float ahead=rx_adds(rx_muls(az,dz),rx_muls(ax,dx));SF32(F,0x10,dx);SF32(F,0x14,dz);
 if(ahead>0&&F32(0,0x9c4750)>absf_bits(ahead)){
  float align=rx_adds(rx_muls(F32(F,0x3c),az),rx_muls(ax,F32(F,0x38)));
  if(F32(0,0x9cd99c)>absf_bits(align)){float angular=rx_adds(rx_muls(F32(v,0x160),F32(v,0x160)),rx_muls(F32(v,0x164),F32(v,0x164)));angular=rx_adds(angular,rx_muls(F32(v,0x168),F32(v,0x168)));if(angular>F32(0,0x9c4750))SU32(d,0x64,U32(0,0x931f24));}
 }
 float gain;ACT_CALLR(gain,4,rx_race_pow_difference(0,F32(0,0x9cd9e4),F32(0,0x9cd9e8),0,F32(0,0x931f24)));SF32(F,0xc,gain);
 float distance2=rx_adds(rx_muls(dx,dx),rx_muls(dz,dz));SF32(F,0x10,distance2);if(distance2!=0){float length;ACT_CALLR(length,1,sqrt_argument(distance2));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(inv,dx);dz=rx_muls(inv,dz);}
 v=U32(U32(U32(o,8),0x74),0x1ac);SF32(F,0x20,rx_muls(dx,gain));SF32(F,0x24,0);SF32(F,0x28,rx_muls(dz,gain));ACT_CALL(0,rx_race_to_vehicle_frame(v,F+0x20));d=U32(o,8);float heading;ACT_CALLR(heading,0,rx_race_heading_factor(F+0x40,F+0x48,F+0x38));SF32(d,0x60,rx_adds(rx_muls(heading,F32(F,0x20)),F32(d,0x60)));
 d=U32(o,8);float end=F32(o,0xe0);if(end>F32(d,0x130)){float factor=rx_subs(F32(0,0x931f24),rx_divs(rx_subs(end,F32(d,0x130)),F32(o,0xdc)));SF32(d,0x60,rx_muls(factor,F32(d,0x60)));}ACT_END(F);
}
RX_FUNCTION(0x0052e850,rx_waypoint_navigation_steer)
