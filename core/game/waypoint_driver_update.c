/*00516e10: actual state table and caller ordering, offline product scope.*/
#include "waypoint_driver.h"
#include "waypoint_activity.h"
#include "build_activity_util.h"
#include "race_driver.h"
#include "loop_player.h"
#include "waypoint_driver_calls.h"
extern void rx_waypoint_driver_ai_update(rx_addr driver,float dt);
static float call_dt(float dt){return rx_f80_to_f32(rx_f80_from_f32(dt),0);}
void rx_waypoint_driver_update_rules(rx_addr d){
 ACT_FRAME(F,4,3);race_static_dcf304();if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x519660,"Waypoint rule update network ownership is not composed");
 if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));
 if(U32(U32(d,0x74),0x1ac)){for(uint32_t i=0;i<U32(d,0xee4);i++)waypoint_rule_update(U32(d,0xa0+i*4),0.0f);}if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(0xda6cf0));ACT_END(F);
}
RX_FUNCTION(0x00519660,rx_waypoint_driver_update_rules)
void rx_waypoint_driver_vf7(rx_addr d,float dt){
 ACT_FRAME_EBP(F,16,0xc4,3);rx_addr p=U32(d,0x74);if(U32(p,0x908)){ACT_CALL(1,rx_network_driver_update(d,call_dt(dt)));ACT_END(F);return;}
 uint32_t state=U32(U32(p,0x94),0x21c);
 if(state==2||state>5){
  if(state==2&&!U32(d,0x70)&&!U32(d,0x10cc))SF32(d,0xf14,F32(d,0xf14)+dt);
  if(U32(p,0x10)==0){float length;ACT_CALLR(length,0,rx_waypoint_route_length(U32(p,0x94)));SF32(F,0xc,(float)I32(U32(p,0x94),0xff090)*length);float progress;ACT_CALLR(progress,0,rx_waypoint_driver_progress(d));SF32(d,0x10b8,F32(F,0xc)-progress);ACT_CALL(1,rx_waypoint_driver_update_rules(d));}
  else if(U32(p,0x10)==1){float length;ACT_CALLR(length,0,rx_waypoint_route_length(U32(p,0x94)));SF32(F,0xc,(float)I32(U32(p,0x94),0xff090)*length);float progress;ACT_CALLR(progress,0,rx_waypoint_driver_progress(d));SF32(d,0x10b8,F32(F,0xc)-progress);p=U32(d,0x74);
   if(U32(p,4)){if(U32(d,0x138)==0){if(F32(d,0x130)>F32(d,0x13c))SU32(d,0x138,2);}else if(U32(d,0x138)==2)ACT_CALL(1,rx_waypoint_driver_ai_update(d,call_dt(dt)));}}
 }
 ACT_CALL(1,rx_network_driver_update(d,call_dt(dt)));race_static_dcf304();
 if(!U32(U32(0,0xda6cb0),0x1c3c)){race_static_dcf304();if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x516e10,"Waypoint remote-state receive is not composed");}
 ACT_END(F);
}
RX_FUNCTION(0x00516e10,rx_waypoint_driver_vf7)
