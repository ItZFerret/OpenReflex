/* Complete RaceResultsScenario begin/step owners. The selected V8 canonical
 * shared post-step, animation and scenario drivers remain the single owners. */
#include "race_results_scenario.h"
#include "build_activity_util.h"
#include "race_finish_select.h"
#include "anim_scenario.h"
#include "anim_scenario_race_intro.h"
#include "waypoint_post_steps.h"
#include "phys_body.h"
static void hide_vehicles(rx_addr world) {
 const rx_addr cast=world+0x20150;
 const int32_t count=I32(U32(world,0x20190),0x26c);
 for(int32_t i=0;i<count;i++){
  const rx_addr player=U32(U32(U32(U32(cast,0x40),0x268),4u*(uint32_t)i),0x74);
  const rx_addr vehicle=U32(player,0x1ac);
  SU8(U32(vehicle,0x1328),0x14e,0);
  const rx_addr handle=U32(U32(player,0x1ac)+0x178,4);
  if(handle){rx_addr record;ACT_CALLR(record,0,phys_body_record(handle));SU32(U32(record,0x2c),0x14,0xffffffffu);}
 }
}
void rx_race_results_scenario_begin(rx_addr sc) {
 ACT_FRAME(f,0,4);
 ACT_CALL(0,rx_finish_select_local_flags(sc));
 const rx_addr world=U32(U32(sc,0x14),0x234),cast=world+0x20150;
 const int32_t count=I32(U32(world,0x20190),0x26c);
 for(int32_t i=0;i<count;i++){
  const rx_addr player=U32(U32(U32(U32(cast,0x40),0x268),4u*(uint32_t)i),0x74);
  SU8(U32(player,0x1b4),0xa2,0);
 }
 hide_vehicles(U32(U32(sc,0x14),0x234));
 ACT_CALL(0,rx_scenario_sim_flag(sc));
 ACT_CALL(0,rx_scenario_set_riders_paused(sc,1));
 if(U32(U32(sc,0x14),0x238)==8)
  SU32(sc,0x151b4,U32(sc,0x151c0)?8:14);
 else SU32(sc,0x151b4,U32(sc,0x151c0)?9:11);
 ACT_END(f);
}
RX_FUNCTION(0x0074a660,rx_race_results_scenario_begin)
void rx_race_results_scenario_step(rx_addr sc,uint32_t step) {
 ACT_FRAME(f,0,4);
 hide_vehicles(U32(U32(sc,0x14),0x234));
 ACT_CALL(1,rx_post_step_cameras(sc,step));
 ACT_CALL(0,rx_post_step_entities(sc,step));
 ACT_END(f);
}
RX_FUNCTION(0x0074a770,rx_race_results_scenario_step)
