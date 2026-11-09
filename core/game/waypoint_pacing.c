/*Waypoint pacing's actual setup/reset owners. Update methods remain unclosed.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "build_series_init.h"
#include "race.h"
#include "race_beh2.h"
#include "career_activity.h"
uint32_t rx_waypoint_pacing_kind(uint32_t type){return type==0?1u:type==3?2u:0u;}
RX_FUNCTION(0x0051f100,rx_waypoint_pacing_kind)
uint32_t rx_waypoint_pacing_vf1(rx_addr object,rx_addr driver){
 ACT_FRAME(F,0,1);SU32(object,8,driver);SU32(object,4,1);SU32(object,0x28,driver);uint32_t type=U32(U32(U32(driver,0x74),0x94),0x23c);
 if(type>=0x1f){if(!(U32(0,0x9e275c)&1)){SU32(0,0x9e275c,U32(0,0x9e275c)|1);ACT_CALL(0,(void)rx_series_manager_construct());}ACT_CALLR(type,1,rx_career_current_event_type(0x9e2188));}
 SU32(object,0x34,rx_waypoint_pacing_kind(type));ACT_END(F);return 1;
}
RX_FUNCTION(0x00525f90,rx_waypoint_pacing_vf1)
void rx_waypoint_pacing_vf3(rx_addr object){ACT_FRAME(F,0,1);rx_addr behavior;ACT_CALLR(behavior,1,rx_driver_behavior_14(U32(object,8)));SU32(object,0x2c,behavior);ACT_END(F);}
RX_FUNCTION(0x00526000,rx_waypoint_pacing_vf3)
void rx_waypoint_pacing_vf6(rx_addr object){
 rx_addr d=U32(object,0x28);if(F32(d,0x10ac)==0){SU32(object,4,0);return;}
 SU32(object,0x24,0);SU32(object,0x18,U32(0,0x931f24));SU32(object,0xc,0);SU32(object,0x10,0);
 float v=F32(d,0x10ac)-F32(d,0x10b4);SU32(object,0x1c,v>F32(0,0x92e330)?2u:v<F32(0,0x9d7b30)?0u:1u);SU32(object,0x14,0);SU32(object,0x30,0);
}
RX_FUNCTION(0x00526020,rx_waypoint_pacing_vf6)
extern float rx_waypoint_route_length(rx_addr activity);
void rx_waypoint_pacing_target(rx_addr object){
 ACT_FRAME(F,4,4);rx_addr a=U32(U32(U32(object,8),0x74),0x94),d=U32(object,0x28);float route;ACT_CALLR(route,0,rx_waypoint_route_length(a));SF32(F,0x10,route);float prefix=0;SF32(F,0x18,0);
 for(int32_t i=0;i<I32(d,0x10d4);i++){float length=i<I32(a,0xff074)?F32(U32(a,0xff09c),(uint32_t)i*4):0;prefix=length+prefix;}SF32(F,0x18,prefix);
 float distance;ACT_CALLR(distance,0,rx_waypoint_driver_distance_to_next(d));d=U32(object,0x28);int32_t index=I32(d,0x10d4),count=I32(a,0xff074);float length=index<count?F32(U32(a,0xff09c),(uint32_t)index*4):0;float diff=length-distance,partial=0;if(!(0>diff)){length=index<count?F32(U32(a,0xff09c),(uint32_t)index*4):0;partial=length-distance;}
 float covered=partial+F32(F,0x18),remaining=F32(F,0x10)-covered,time=F32(object,0x24);SF32(object,0x20,remaining);float budget=F32(d,0x10ac)-time;if(F32(0,0x931f24)>budget)budget=F32(0,0x931f24);SF32(object,0xc,remaining/budget);SF32(object,0x10,covered/time);ACT_END(F);
}
RX_FUNCTION(0x005262d0,rx_waypoint_pacing_target)
/*00526160: authored per-vehicle line overrides take precedence over pacing.*/
uint32_t rx_waypoint_pacing_override(rx_addr o){
 ACT_FRAME(F,0,2);rx_addr section=U32(U32(o,0x2c),0xa8);uint32_t result=0;
 if(section){rx_addr d=U32(o,8),v=U32(U32(d,0x74),0x1ac);uint32_t index=U32(v,0x198)+U32(v,0x194)*4;
  if(F32(section,0xe8+index*4)>F32(0,0x9cd9e8)){SU32(o,0x14,1);result=1;}
  else{section=U32(section,0x60);if(section){v=U32(U32(d,0x74),0x1ac);index=U32(v,0x198)+U32(v,0x194)*4;
   if(F32(section,0xe8+index*4)>F32(0,0x9cd9e8)){SU32(o,0x14,1);result=1;}
   else{v=U32(U32(d,0x74),0x1ac);index=U32(v,0x198)+U32(v,0x194)*4;
    if((U32(section,0x68+index*4)&0x4000)&&U32(o,0x30)==2){SU32(o,0x14,2);result=1;}
    else{v=U32(U32(d,0x74),0x1ac);index=U32(v,0x198)+U32(v,0x194)*4;if((U8(section,0x68+index*4)&4)&&U32(o,0x30)==1){SU32(o,0x14,3);result=1;}}
   }
  }}
 }
 if(!result)SU32(o,0x14,0);ACT_END(F);return result;
}
RX_FUNCTION(0x00526160,rx_waypoint_pacing_override)
void rx_waypoint_pacing_mode(rx_addr o){
 float ratio=F32(o,0x10)/F32(o,0xc),upper=F32(0,0x9774c8),lower=F32(0,0x9cd9ac);
 if(U32(o,0x1c)==2){upper=F32(0,0x931f24);lower=F32(0,0x9cd970);}else if(U32(o,0x1c)==0){upper=F32(0,0x9774c4);lower=F32(0,0x931f24);}
 SU32(o,0x30,lower>ratio?1u:ratio>upper?2u:0u);
}
RX_FUNCTION(0x00526260,rx_waypoint_pacing_mode)
void rx_waypoint_pacing_reduce(rx_addr o){
 float baseline=F32(0,0x9cc8a8),range=F32(0,0x9ddfdc);if(U32(o,0x1c)==0){range=F32(0,0x9774c0);baseline=F32(0,0x9774bc);}else if(U32(o,0x1c)==2){range=F32(0,0x9774b8);baseline=F32(0,0x9774b4);}
 float diff=F32(o,0x10)-F32(o,0xc);if(diff>range)diff=range;float one=F32(0,0x931f24),r=((one-diff/range)*(one-baseline))+baseline;SF32(o,0x18,one>r?r:one);
}
RX_FUNCTION(0x005263f0,rx_waypoint_pacing_reduce)
uint32_t rx_waypoint_pacing_vf4(rx_addr o,float dt){
 ACT_FRAME(F,0,1);if(I32(U32(o,0x28),0x10d4)>=0){SF32(o,0x24,F32(o,0x24)+dt);if(F32(0,0x92fe48)>F32(o,0x24)){ACT_END(F);return 0;}
  if(U32(o,4)&&!U32(U32(U32(o,8),0x10),0x1ec)){ACT_CALL(0,rx_waypoint_pacing_target(o));SU32(o,0x18,U32(0,0x931f24));ACT_CALL(0,rx_waypoint_pacing_mode(o));uint32_t override;ACT_CALLR(override,0,rx_waypoint_pacing_override(o));if(override)SU32(o,0x18,U32(0,0x931f24));else if(U32(o,0x30)==2)ACT_CALL(0,rx_waypoint_pacing_reduce(o));ACT_END(F);return 0;}
 }
 SU32(o,0x18,U32(0,0x931f24));ACT_END(F);return 0;
}
RX_FUNCTION(0x005260b0,rx_waypoint_pacing_vf4)
