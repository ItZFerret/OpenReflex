/* Native results completion owners. Presentation must invoke these only at
 * the actual PageWipe transition; no timer or automatic acknowledgement here. */
#include "race_results_continue.h"
#include "waypoint_playback.h"
#include "build_activity_int.h"
#include "build_series_init.h"
#include "race_finish.h"
#include "anim_scenario_race_intro.h"
#include "anim_scenario_misc.h"

static void series_guard(void) {
 if (!(U8(0,0x9e275c)&1)) {
  SU32(0,0x9e275c,U32(0,0x9e275c)|1);
  ACT_CALL(0,(void)rx_series_manager_construct());
  const uint32_t callback=0x911da0;
  act_call_fn(0x8f9fbf,RX_CDECL,0,1,&callback);
 }
}
void rx_race_results_done(rx_addr activity) {
 ACT_FRAME(f,4,3);
 ACT_CALL(0,rx_clip_recorder_stop_playback());
 series_guard();
 uint32_t career=U32(0,0x9e2318)==2;
 if(!career){series_guard();career=U32(0,0x9e2318)==1;}
 if(career){
  series_guard();
  const rx_addr event=U32(0,0x9e2314);
  uint32_t phase=U32(event,0xc);if(phase==2)phase=U32(event,0x10);
  if(phase==7)act_vcall(event,0x64,1,&activity);
 }
 uint32_t started=0;
 if(act_vcall(activity,0x34,0,NULL))
  ACT_CALLR(started,0,rx_scenario_start(activity+0x129430));
 if(started){
  ACT_CALL(1,rx_event_scenario(0x941fd8,0x941c04,U32(activity,0x290),activity));
  SU32(activity,0x13e640,3);
 }else{
  ACT_CALL(1,rx_podium_release(U32(activity,0x234)+0x20150,1));
  SU32(f,12,activity);
  act_raise(U32(activity,0x290),0x941c48,0x9cbd24,f+12,4);
 }
 ACT_END(f);
}
RX_FUNCTION(0x00569be0,rx_race_results_done)

#ifndef RX_ORACLE
/* Same product-only tail-call convention as build_event_proxies.c. The oracle
 * keeps the original jump; native verification cannot jump out of its wrapper. */
void rx_app_post_activity_results_page_done(void) {
 const rx_addr context=U32(0,0xa95844),game=U32(0,0xa946b8);
 const uint32_t alternate=U32(U32(context,0x20),0x34);
 rx_addr world=0;
 if(alternate==1)world=U32(game,0x5a54);
 else if(U32(U32(context,0x1c),0x34)==1)world=U32(game,0x20c8);
 if(I32(world,0x21da8)>0){
  if(alternate==1)world=U32(game,0x5a54);
  else if(U32(U32(context,0x1c),0x34)==1)world=U32(game,0x20c8);
  else world=0;
  const rx_addr activity=U32(U32(world,0x21da4),0),sp=rx_sp();
  rx_set_sp(sp+4);
  act_vcall(activity,0xe8,0,NULL);
  rx_set_sp(sp);
 }
}
RX_FUNCTION(0x00742aa0,rx_app_post_activity_results_page_done)
#endif
