/* Original sparse obstacle copies and nearest-track projection. No collision fallback. */
#include "waypoint_avoid_objects.h"
#include "build_activity_util.h"
#include "race.h"
#include "phys_vec.h"
static float x87_store(float value){return rx_f80_to_f32(rx_f80_from_f32(value),0);}
/* 508430: integer exponentiation loop at exponent2 still multiplies the square by1. */
float rx_waypoint_avoid_square(float value){return rx_muls(F32(0,0x931f24),rx_muls(value,value));}
RX_FUNCTION(0x00508430,rx_waypoint_avoid_square)
void rx_waypoint_obstacle_copy_initial(rx_addr dst,rx_addr src){
 static const uint32_t fields[]={0,4,8,0x10,0x30,0x34};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)SF32(dst,fields[i],x87_store(F32(src,fields[i])));
}
RX_FUNCTION(0x0050baa0,rx_waypoint_obstacle_copy_initial)
void rx_waypoint_obstacle_copy(rx_addr dst,rx_addr src){
 static const uint32_t fields[]={0,4,8,0x10,0x20,0x24,0x28};
 for(unsigned i=0;i<sizeof fields/sizeof fields[0];i++)SF32(dst,fields[i],x87_store(F32(src,fields[i])));
 SU32(dst,0x30,U32(src,0x30));SF32(dst,0x34,x87_store(F32(src,0x34)));
}
RX_FUNCTION(0x00521880,rx_waypoint_obstacle_copy)
static float projection(float ax,float az,float dx,float dz,float px,float pz){
 float t=rx_divs(rx_adds(rx_muls(rx_subs(pz,az),dz),rx_muls(rx_subs(px,ax),dx)),rx_adds(rx_muls(dz,dz),rx_muls(dx,dx)));
 const float one=F32(0,0x931f24);if(one>t){if(0.0f>t)t=0.0f;}else t=one;return t;
}
/* 509c80 differs509e30 by finding the actual nearest node before edge projection. */
float rx_waypoint_avoid_track_lateral(rx_addr driver,rx_addr pos){
 ACT_FRAME_EBP(F,16,0x1c,1);
 ACT_CALL(3,rx_track_nearest_node_all(U32(driver,0x10),0,0,pos,F+0x10));
 rx_addr a=U32(F,0x14),b=U32(a,0x64);if(!b)b=U32(U32(U32(U32(U32(F,0x10),8),0),0),0x64);
 float px=F32(pos,0),pz=F32(pos,8),ax=F32(a,0x10),az=F32(a,0x18);
 float dx=rx_subs(F32(b,0x10),ax),dz=rx_subs(F32(b,0x18),az),t=projection(ax,az,dx,dz,px,pz);
 float lx=rx_adds(rx_muls(dx,t),ax),lz=rx_adds(rx_muls(dz,t),az);
 ax=F32(a,0x20);az=F32(a,0x28);dx=rx_subs(F32(b,0x20),ax);dz=rx_subs(F32(b,0x28),az);t=projection(ax,az,dx,dz,px,pz);
 float wx=rx_subs(rx_adds(rx_muls(dx,t),ax),lx),wz=rx_subs(rx_adds(rx_muls(dz,t),az),lz);
 float result=rx_divs(rx_adds(rx_muls(wz,rx_subs(pz,lz)),rx_muls(wx,rx_subs(px,lx))),rx_adds(rx_muls(wz,wz),rx_muls(wx,wx)));
 ACT_END(F);return result;
}
RX_FUNCTION(0x00509c80,rx_waypoint_avoid_track_lateral)
