/*Actual WaypointGate navigation initialization, no invented route/AI state.*/
#include "waypoint_driver.h"
#include "waypoint_activity.h"
#include "build_activity_util.h"
extern rx_addr rx_driver_behavior_b(rx_addr driver); /*existing canonical0053b910*/
void rx_waypoint_navigation_vf3(rx_addr object){ACT_FRAME(F,0,1);rx_addr b;ACT_CALLR(b,1,rx_driver_behavior_b(U32(object,8)));SU32(object,0x1b0,b);ACT_END(F);}
RX_FUNCTION(0x0052da60,rx_waypoint_navigation_vf3)
void rx_waypoint_navigation_vf6(rx_addr o){
 ACT_FRAME(F,0,3);SU32(o,0xdc,U32(0,0x9322ac));const uint32_t scale=U32(0,0x9d789c),six=U32(0,0x92fe4c);
 SU32(o,0xfc,scale);SU32(o,0x100,six);SU32(o,0x104,0);SU32(o,0x108,1);SU32(o,0xf4,10);SU32(o,0xf8,9);SU32(o,0x138,9);SU32(o,0x134,10);SU32(o,0x13c,scale);SU32(o,0x148,1);SU32(o,0x140,six);SU32(o,0x144,0);SU32(o,0x178,9);SU32(o,0x174,10);SU32(o,0x17c,scale);SU32(o,0x188,1);SU32(o,0x180,six);SU32(o,0x184,0);SU32(o,0xc,2);
 for(uint32_t k=0xac;k<0xd4;k+=4)SU32(o,k,0);SU32(o,0xd4,0);SU32(o,0xa4,0);SU32(o,0xa8,0);
 rx_addr driver=U32(o,8),activity=U32(U32(driver,0x74),0x94);int32_t count=I32(activity,0xff074),next=(int32_t)(U32(driver,0x10d4)+1u);
 if(count==0||(next==INT32_MIN&&count==-1))act_unreached(0x52db88,"actual waypoint navigation IDIV fault");
 int32_t index=next%count;SU32(o,0xa0,(uint32_t)index);rx_addr gate=rx_waypoint_gate_at(index,activity);uint32_t output=o+0x20;act_vcall(gate,0x3c,1,&output);
 gate=rx_waypoint_gate_at(0,activity);for(uint32_t k=0;k<0x40;k+=4)SU32(o,0x60+k,U32(gate,0x240+k));ACT_END(F);
}
RX_FUNCTION(0x0052da80,rx_waypoint_navigation_vf6)
/*0052dbf0: actual selected waypoint updates the existing navigation behavior.*/
void rx_waypoint_navigation_select(rx_addr gate,int32_t index,rx_addr object){
 ACT_FRAME(F,0,3);SU32(object,0xa0,(uint32_t)index);uint32_t out=object+0x20;act_vcall(gate,0x3c,1,&out);rx_addr d=U32(object,8);for(uint32_t k=0;k<0x40;k+=4)SU32(object,0x60+k,U32(gate,0x240+k));SF32(object,0xe0,F32(d,0x130)+F32(object,0xdc));ACT_END(F);
}
RX_FUNCTION(0x0052dbf0,rx_waypoint_navigation_select)
