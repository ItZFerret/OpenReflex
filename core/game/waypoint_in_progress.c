/*005798a0 and the complete finite track-debug traversal it conditionally invokes.
 * The original debug sink539f90 is itself a ten-byte empty function; it is not
 * substituted for a substantive renderer or replay recorder.*/
#include "build_activity_util.h"
#include "race_activity.h"
#include "race.h"
#include "world_node.h"
static uint32_t copied_float(uint32_t v){return (v&0x7f800000u)==0x7f800000u&&(v&0x7fffffu)?v|0x400000u:v;}
void rx_waypoint_track_debug_sink(void){ACT_FRAME_EBP(F,16,0,0);ACT_END(F);}
RX_FUNCTION(0x00539f90,rx_waypoint_track_debug_sink)
void rx_waypoint_track_node_copy(rx_addr out,rx_addr node){
 ACT_FRAME(F,0,2);const uint32_t starts[]={0,0x10,0x20,0x30,0x40,0x50};for(uint32_t i=0;i<6;i++)for(uint32_t k=0;k<12;k+=4)SU32(out,starts[i]+k,copied_float(U32(node,starts[i]+k)));SU32(out,0x60,U32(node,0x60));SU32(out,0x64,U32(node,0x64));
 const uint32_t begin[]={0x68,0xe8,0x168,0x2e8,0x468},words[]={0x20,0x20,0x60,0x60,0x20};for(uint32_t i=0;i<5;i++)for(uint32_t k=0;k<words[i];k++)SU32(out,begin[i]+k*4,U32(node,begin[i]+k*4));SU32(out,0x4e8,U32(node,0x4e8));SU32(out,0x4ec,U32(node,0x4ec));SU32(out,0x4f0,copied_float(U32(node,0x4f0)));SU32(out,0x4f4,copied_float(U32(node,0x4f4)));ACT_END(F);
}
RX_FUNCTION(0x00539af0,rx_waypoint_track_node_copy)
static rx_addr local_driver(rx_addr track){rx_addr activity=U32(track,4);if(activity)for(int32_t i=0;i<I32(activity,0x26c);i++){rx_addr d=U32(U32(activity,0x268),(uint32_t)i*4);if(!U32(U32(d,0x74),8))return d;}return 0;}
static void sinks(uint32_t count){for(uint32_t i=0;i<count;i++)ACT_CALL(0,rx_waypoint_track_debug_sink());}
static void traverse_simple(rx_addr track,rx_addr frame,uint32_t kind){
 if(kind!=8)(void)local_driver(track);
 for(int32_t i=0;i<I32(track,0x1d8);i++){rx_addr section=U32(U32(track,0x130),(uint32_t)i*4),first=U32(section,0);ACT_CALL(0,rx_waypoint_track_node_copy(frame+0x10,first));SU32(frame,0x74,first);rx_addr cursor=frame+0x10;
  while(U32(cursor,0x64)){if(kind==6){if((!U32(section,0x14)||I32(U32(U32(section,0xc),0),0x10)<2||cursor!=U32(section,0))&&(U32(section,0x10)!=1||U32(U32(cursor,0x64),0x64)))sinks(4);}else if(kind==7)sinks(4);else sinks(1);sinks(1);cursor=U32(cursor,0x64);}
 }
}
void rx_waypoint_track_debug_type6(rx_addr track){ACT_FRAME_EBP(F,16,0x504,3);traverse_simple(track,F,6);ACT_END(F);}
RX_FUNCTION(0x00539a20,rx_waypoint_track_debug_type6)
void rx_waypoint_track_debug_type8(rx_addr track){ACT_FRAME_EBP(F,16,0x504,3);traverse_simple(track,F,8);ACT_END(F);}
RX_FUNCTION(0x00539c00,rx_waypoint_track_debug_type8)
void rx_waypoint_track_debug_type7(rx_addr track){ACT_FRAME_EBP(F,16,0x504,3);traverse_simple(track,F,7);ACT_END(F);}
RX_FUNCTION(0x00539c70,rx_waypoint_track_debug_type7)
/*The original ring searches have no writes, but retain their exact bounded-ring
 * reads and stop conditions rather than replacing the default traversal with no-op.*/
static void find_ring(rx_addr driver,rx_addr target,uint32_t count_offset,uint32_t cursor_offset,uint32_t entries){
 uint32_t count=U32(driver,count_offset);if(!count)return;uint32_t i=count==10?U32(driver,cursor_offset):0;
 do{if(U32(driver,entries+i*20)==target)break;i=(i+1)%10;}while(i!=U32(driver,cursor_offset));
}
void rx_waypoint_track_debug_default(rx_addr track){
 ACT_FRAME_EBP(F,16,0x514,3);rx_addr driver=local_driver(track),vehicle=driver?U32(U32(driver,0x74),0x1ac):0;SU32(F,0xc,vehicle);SU32(F,0x14,0);
 for(int32_t i=0;i<I32(track,0x1d8);i++){rx_addr section=U32(U32(track,0x130),(uint32_t)i*4),first=U32(section,0);SU32(F,0x18,section);ACT_CALL(0,rx_waypoint_track_node_copy(F+0x20,first));SU32(F,0x84,first);SU32(F,0x10,F+0x20);if(driver){rx_addr behavior;ACT_CALLR(behavior,1,rx_driver_behavior_5(driver));(void)behavior;}
  for(rx_addr cursor=F+0x20;U32(cursor,0x64);cursor=U32(cursor,0x64)){SU32(F,0x10,cursor);if(!(U8(section,0x118)&2))sinks(4);
   if(vehicle){rx_addr next=U32(cursor,0x64);uint32_t type=U32(vehicle,0x198)+U32(vehicle,0x194)*4;SU32(F,0x1c,type);
    if(F32(next,0x168+type*4)>0)find_ring(driver,next+0x168,0x2e0,0x2dc,0x14c);
    if(F32(next,0x2e8+type*4)>0)find_ring(driver,next+0x2e8,0x2e8,0x2e4,0x214);
    if(U32(next,0x68+type*4)&0x100)ACT_CALL(0,rx_world_shared_vf_empty());
    type=U32(vehicle,0x198)+U32(vehicle,0x194)*4;if(U8(U32(cursor,0x64),0x68+type*4)&0x80)ACT_CALL(0,rx_world_shared_vf_empty());
    if(!(U8(section,0x118)&2)){type=U32(vehicle,0x198)+U32(vehicle,0x194)*4;uint32_t flag=U32(U32(cursor,0x64),0x68+type*4)&0x200;if(U32(track,0x324)!=flag){SU32(track,0x324,flag);sinks(2);}}
   }sinks(1);SU32(F,0x10,U32(cursor,0x64));
  }SU32(F,0x14,(uint32_t)i+1);
 }ACT_END(F);
}
RX_FUNCTION(0x00539d20,rx_waypoint_track_debug_default)
void rx_waypoint_track_debug(rx_addr track){
 ACT_FRAME_EBP(F,8,4,0);SU32(F,0,track);/*Original PUSH ECX supplies each cdecl child's argument; no second push.*/
 switch(U32(track,8)){case 6:ACT_CALL(0,rx_waypoint_track_debug_type6(track));break;case 7:ACT_CALL(0,rx_waypoint_track_debug_type7(track));break;case 8:ACT_CALL(0,rx_waypoint_track_debug_type8(track));break;default:ACT_CALL(0,rx_waypoint_track_debug_default(track));break;}ACT_END(F);
}
RX_FUNCTION(0x005399e0,rx_waypoint_track_debug)
void rx_waypoint_activity_in_progress(rx_addr activity,float dt){
 ACT_FRAME_EBP(F,8,4,1);float loaded=rx_f80_to_f32(rx_f80_from_f32(dt),0);ACT_CALL(1,rx_activity_in_progress(activity,loaded));if(U32(activity,0x21c)==2&&(U32(activity,0x238)==1||U32(activity,0x238)==2)&&U32(activity,0xb8c))ACT_CALL(0,rx_waypoint_track_debug(activity+0x890));ACT_END(F);
}
RX_FUNCTION(0x005798a0,rx_waypoint_activity_in_progress)
