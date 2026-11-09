/* Authored actual WaypointRaceDriver owners. Remaining vtable slots are not
 * registered here until their full original semantics/dependencies are closed. */
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "build_driver_dtor.h"
#include "build_rider_util.h"
#include "build_alloc.h"
#include "race_misc.h"
rx_addr rx_waypoint_driver_construct(rx_addr d,rx_addr player){
 ACT_FRAME(F,0,1);ACT_CALL(1,(void)rx_network_driver_construct(d,player));
 const uint32_t largest=U32(0,0x977494);
 SU32(d,0xf10,0);SU32(d,0x10d4,0);SU32(d,0x10d8,0);SU32(d,0,0x93ffe4);SU32(d,0xee8,0xb);
 SU32(d,0x10b0,largest);SU32(d,0x10a4,0);SU32(d,0x10ac,largest);SU32(d,0x10b8,largest);SU32(d,0xf14,0);SU32(d,0x10bc,0);SU32(d,0x10d0,largest);
 ACT_END(F);return d;
}
RX_FUNCTION(0x00516730,rx_waypoint_driver_construct)
rx_addr rx_waypoint_driver_sdtor(rx_addr d,uint32_t flags){
 ACT_FRAME(F,0,1);SU32(d,0,0x93ffe4);ACT_CALL(0,rx_network_driver_dtor(d));
 if(flags&1){if(U8(0,0xda67d8))rr_os_cs_enter(0xd61704);ACT_CALL(1,rx_crt_aligned_free(d));if(U8(0,0xda67d8))rr_os_cs_leave(0xd61704);}
 ACT_END(F);return d;
}
RX_FUNCTION(0x005167b0,rx_waypoint_driver_sdtor)
void rx_waypoint_driver_vf6(rx_addr d){
 rx_addr p=U32(d,0x74),activity=U32(p,0x94);
 if(U32(activity,0x238)!=1){SU32(d,0x138,2);return;}
 if(U32(p,0x10)==1){uint32_t seed=U32(0,0x9cb860)*0x41c64e6du+0x3039u;SU32(0,0x9cb864,U32(0,0x9cb864)+1);SU32(0,0x9cb860,seed);
  float v=(float)(int32_t)((seed>>16)&0x7fff);v=v*F32(0,0x92e054);v=v*F32(0,0x9322cc);v=v+F32(d,0x130);SF32(d,0x13c,v);SU32(d,0x138,0);}
}
RX_FUNCTION(0x00517800,rx_waypoint_driver_vf6)
uint32_t rx_waypoint_driver_vf12(rx_addr d){
 float t=F32(U32(d,0x24),0x10b0);if(F32(d,0x10d0)>t){SF32(d,0x10d0,t);return 1;}return 0;
}
RX_FUNCTION(0x005183d0,rx_waypoint_driver_vf12)
float rx_waypoint_driver_vf13(rx_addr d){ACT_FRAME(F,4,0);SU32(F,0,U32(d,U32(U32(d,0x74),0x10)==2?0x10d0:0x10b0));float v=rx_f80_to_f32(rx_f80_from_f32(F32(F,0)),0);ACT_END(F);return v;}
RX_FUNCTION(0x00518400,rx_waypoint_driver_vf13)
/*00517d20 actual scalar association is absolute value of summed axis deltas,
 * expressed by original SQRTSS(sum*sum), not geometric Euclidean distance.*/
float rx_waypoint_driver_distance_to_next(rx_addr d){
 ACT_FRAME_EBP(F,16,0x9c,1);const rx_addr player=U32(d,0x74),a=U32(player,0x94);uint32_t arg=F+0x60;
 act_vcall(U32(player,0x1ac),0x3c,1,&arg);
 SU32(F,0x10,U32(F,0x90));SU32(F,0x14,U32(F,0x94));SU32(F,0x18,U32(F,0x98));
 int32_t index=I32(d,0x10d4),last=I32(d,0x10d8)-1;rx_addr waypoint=0;
 if(index>=0&&index<last){index++;if(index<I32(a,0xff074))waypoint=U32(U32(a,0xff078),(uint32_t)index*4);}
 else if(I32(a,0xff074)>0)waypoint=U32(U32(a,0xff078),0);
 arg=F+0x20;act_vcall(waypoint,0x3c,1,&arg);
 float sum=F32(F,0x18)-F32(F,0x58);float y=F32(F,0x14)-F32(F,0x54);sum=sum+y;float x=F32(F,0x10)-F32(F,0x50);sum=sum+x;float result=rx_sqrts(sum*sum);ACT_END(F);return result;
}
RX_FUNCTION(0x00517d20,rx_waypoint_driver_distance_to_next)
/*0053b400: first actual behavior whose virtual type is0x16.*/
rx_addr rx_waypoint_driver_find_behavior22(rx_addr d){
 ACT_FRAME(F,0,3);rx_addr found=0;
 for(uint32_t i=0;i<U32(d,0xee0);i++){rx_addr b=U32(d,0xe8+i*4);if(act_vcall(b,0x14,0,0)==0x16){found=U32(d,0xe8+i*4);break;}}
 ACT_END(F);return found;
}
RX_FUNCTION(0x0053b400,rx_waypoint_driver_find_behavior22)
/*Shared activity owner in waypoint_activity.c, unique canonical summation.*/
extern float rx_waypoint_route_length(rx_addr activity);
float rx_waypoint_driver_progress(rx_addr d){
 ACT_FRAME(F,8,4);rx_addr a=U32(U32(d,0x74),0x94);int32_t index=I32(d,0x10d4),i=0,count=I32(a,0xff074);float even=0,odd=0,last=0;
 if(index>=2)do{float x=i<count?F32(U32(a,0xff09c),(uint32_t)i*4):0;even=x+even;int32_t j=i+1;x=j<count?F32(U32(a,0xff09c),(uint32_t)j*4):0;i+=2;odd=x+odd;}while(i<index-1);
 if(i<index)last=i<count?F32(U32(a,0xff09c),(uint32_t)i*4):0;
 SF32(F,0x14,(odd+even)+last);
 int32_t segment=index<0?I32(d,0x10d8)-1:index;
 SF32(F,0x10,segment<count?F32(U32(a,0xff09c),(uint32_t)segment*4):0);
 float distance;ACT_CALLR(distance,0,rx_waypoint_driver_distance_to_next(d));
 float partial=(F32(F,0x10)-distance)+F32(F,0x14);
 int32_t end=I32(d,0x10d8)-1;float closing=end<count?F32(U32(a,0xff09c),(uint32_t)end*4):0;
 float length;ACT_CALLR(length,0,rx_waypoint_route_length(a));float result=(length*(float)I32(d,0xf10)+closing)+partial;ACT_END(F);return result;
}
RX_FUNCTION(0x00517bb0,rx_waypoint_driver_progress)
float rx_waypoint_driver_lap_scaled_progress(rx_addr d){
 ACT_FRAME(F,4,0);float length;ACT_CALLR(length,0,rx_waypoint_route_length(U32(U32(d,0x74),0x94)));SF32(F,0,length*(float)I32(U32(U32(d,0x74),0x94),0xff090));
 float progress;ACT_CALLR(progress,0,rx_waypoint_driver_progress(d));float result=F32(F,0)-progress;ACT_END(F);return result;
}
RX_FUNCTION(0x00517b70,rx_waypoint_driver_lap_scaled_progress)
/*00518430: sum completed lap cells, network event publishes its own total.*/
float rx_waypoint_driver_total_time(rx_addr d){
 if(U32(U32(U32(d,0x74),0x94),0xfe8))return F32(d,0x10bc);
 int32_t n=I32(d,0xf10),used=0;float even=0,odd=0,last=0;
 if(n>=2){uint32_t pairs=((uint32_t)(n-2)>>1)+1;used=(int32_t)(pairs*2);for(uint32_t i=0;i<pairs;i++){even=F32(d,0xf18+i*8)+even;odd=F32(d,0xf1c+i*8)+odd;}}
 if(used<n)last=F32(d,0xf18+(uint32_t)used*4);return (odd+even)+last;
}
RX_FUNCTION(0x00518430,rx_waypoint_driver_total_time)
