/*00517e20 actual offline gate/lap advancement; no synthesized earned result.*/
#include "waypoint_driver.h"
#include "waypoint_activity.h"
#include "build_activity_util.h"
#include "race.h"
#include "race_driver.h"
#include "race_beh2.h"
#include "loop_player.h"
/*004023d0/00402400 scoped lock, preserving their actual guest frame writes.*/
static rx_addr scope_begin(rx_addr object,rx_addr lock){ACT_FRAME(F,0,1);SU8(object,0,1);SU32(object,4,lock);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(lock));ACT_END(F);return object;}
static void scope_end(rx_addr object){ACT_FRAME(F,0,1);if(U8(object,0)){if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(U32(object,4)));SU8(object,0,0);}ACT_END(F);}
static void end_outer(void){if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(0xda6cf0));}
extern void rx_waypoint_navigation_select(rx_addr gate,int32_t index,rx_addr object);
extern void rx_waypoint_pacing_target(rx_addr object);
static rx_addr activity(rx_addr d){return U32(U32(d,0x74),0x94);}
static float loaded(float x){return rx_f80_to_f32(rx_f80_from_f32(x),0);}
void rx_waypoint_driver_vf9(rx_addr d,rx_addr rule){
 ACT_FRAME_EBP(F,8,0x14,3);const rx_addr original_activity=activity(d);SU32(F,0x14,original_activity);race_static_dcf304();
 if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x517e20,"Waypoint advancement network state serialization is not composed");
 SU32(F,0x10,0xda6cf0);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));
 if(act_vcall(rule,0xc,0,0)!=8||I32(d,0xf10)>=I32(activity(d),0xff090)||U32(activity(d),0x21c)!=2){end_outer();ACT_END(F);return;}
 rx_addr pace;ACT_CALLR(pace,1,rx_waypoint_driver_find_behavior22(d));SU32(F,0x1c,pace);SU32(d,0x10d4,U32(d,0x10d4)+1);ACT_CALL(1,rx_waypoint_advance_markers(activity(d),I32(d,0x10d4),d));
 rx_addr a=activity(d);float delta=0;if(I32(d,0xf10)<I32(a,0xff090)){uint32_t index=U32(d,0x10d8)*U32(d,0xf10)+U32(d,0x10d4);rx_addr cell=U32(a,0xff098)+index*4;float prior=F32(cell,0);if(!(0>=prior))delta=F32(d,0x134)-prior;else SF32(cell,0,loaded(F32(d,0x134)));SF32(d,0x10a4,delta);}
 ACT_CALL(3,rx_event_driver_3int(U32(activity(d),0x290),0x93eff0,d,U32(d,0x10d4)+1,1,U32(d,0x10d8)));
 int32_t index=I32(d,0x10d4),count=I32(d,0x10d8);int wrapped=index+1>=count;if(wrapped)index-=count;index++;
 rx_addr gate=rx_waypoint_gate_at(index,activity(d));rx_addr matrix=act_vcall(gate,0x38,0,0);ACT_CALL(1,rx_waypoint_emit_pose(U32(activity(d),0x290),d,matrix,1));rx_addr navigation;ACT_CALLR(navigation,1,rx_driver_behavior_14(d));
 /*Original reloads current index after notification; do not reuse stale index.*/
 int32_t next=I32(d,0x10d4);if(wrapped)next-=I32(d,0x10d8);next++;
 gate=rx_waypoint_gate_at(next,activity(d));ACT_CALL(1,rx_waypoint_navigation_select(gate,next,navigation));
 race_static_dcf304();if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x518066,"Waypoint gate network notification is not composed");
 if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));
 if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(0xda6cf0));
 int32_t finish=I32(d,0x10d8);if(U32(original_activity,0xff090)==1)finish--;
 if(I32(d,0x10d4)>=finish){
  ACT_CALL(3,rx_event_driver_3int(U32(activity(d),0x290),0x93eff0,d,U32(d,0x10d4)+1,1,U32(d,0x10d8)));
  if(!U32(d,0x10c8)&&!U32(d,0x70)&&F32(d,0x10b0)>F32(d,0xf14)){SU32(d,0x10b0,U32(d,0xf14));ACT_CALL(1,rx_event_best_lap(U32(activity(d),0x290),d,F32(d,0xf14)));}
  if(I32(d,0xf10)>=0){SF32(d,0xf18+U32(d,0xf10)*4,loaded(F32(d,0xf14)));SF32(d,0x10c0,F32(d,0x130)+F32(0,0x92fe48));}
  SF32(d,0x10c4,loaded(F32(d,0xf14)));SU32(d,0x10c8,0);SU32(d,0x10cc,0);SU32(d,0xf14,0);
  if(U32(activity(d),0x238)==3){ACT_CALL(1,rx_event_activity_driver(U32(activity(d),0x290),0x93ee68,activity(d),d));SU32(d,0x134,0);}
  else{uint32_t laps=U32(d,0xf10)+1;if(laps>=0x61)laps=0x61;SU32(d,0xf10,laps);ACT_CALL(3,rx_event_driver_3int(U32(activity(d),0x290),0x93eefc,d,laps,U32(activity(d),0x238)!=2,U32(original_activity,0xff090)));if(I32(d,0xf10)>=I32(original_activity,0xff090))ACT_CALL(1,rx_event_driver(U32(activity(d),0x290),0x93eef0,d));}
  float remaining;ACT_CALLR(remaining,0,rx_waypoint_driver_lap_scaled_progress(d));SF32(d,0x10b8,remaining);if(U32(d,0xf10)==U32(original_activity,0xff090)){SU32(d,0x70,1);SU32(d,0x10b8,0);}
  float total;ACT_CALLR(total,0,rx_waypoint_driver_total_time(d));SF32(d,0x10bc,total);race_static_dcf304();if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x5182d8,"Waypoint lap network notification is not composed");
  ACT_CALL(1,(void)scope_begin(F+0x14,0xda6cf0));ACT_CALL(0,scope_end(F+0x14));
  pace=U32(F,0x1c);if(!pace){end_outer();ACT_END(F);return;}SU32(pace,0x24,0);
 }
 pace=U32(F,0x1c);if(pace)ACT_CALL(1,rx_waypoint_pacing_target(pace));end_outer();ACT_END(F);
}
RX_FUNCTION(0x00517e20,rx_waypoint_driver_vf9)
