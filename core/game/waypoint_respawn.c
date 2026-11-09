/*Complete00579c70..00579e0a. Genuine navigation-node selection/terrain snap;
 * no guessed node, height fallback or class-flag relaxation.*/
#include "waypoint_respawn.h"
#include "build_activity_util.h"
#include "world_misc.h"
#include "race_beh2.h"
#include "world_board_handlers.h"
#include "phys_terrain.h"
#include "fx.h"
uint32_t rx_waypoint_respawn_point(rx_addr activity,rx_addr player,rx_addr position,rx_addr direction,rx_addr up){
 ACT_FRAME_EBP(F,16,0x24,3);SU32(F,0x1c,activity);
 int32_t index;ACT_CALLR(index,0,rx_world_group_rider_index(activity,player));
 rx_addr driver=U32(U32(activity,U32(player,0x908)?0x27c:0x268),4u*(uint32_t)index),navigation;
 ACT_CALLR(navigation,1,rx_driver_behavior_14(driver));
 if(!navigation)goto missing;
 rx_addr section=U32(navigation,0xa4);if(!section)goto missing;
 rx_addr vehicle=U32(U32(driver,0x74),0x1ac),node=U32(navigation,0xa8);
 uint32_t flag=0x68+4*(U32(vehicle,0x198)+4*U32(vehicle,0x194));
 /*The original tests the current node's class flag before its null check.*/
 if(!(U8(node,flag)&2))goto missing;
 rx_addr candidate=node;
 if(candidate){
  for(;;){
   if(!(U8(candidate,flag)&2)){node=candidate;break;}
   candidate=U32(candidate,0x64);if(candidate)continue;
   if(!U32(section,0x10))goto missing;
   section=U32(U32(section,8),0);candidate=U32(section,0);
   /*An empty successor section retains the previous node, exactly as original.*/
   if(!candidate)break;
  }
 }
 if(!section)goto missing;
 SF32(F,0x20,F32(node,0));SF32(F,0x28,F32(node,8));
 SU32(navigation,0xa4,section);SU32(navigation,0xa8,node);
 SF32(F,0x24,rx_adds(F32(node,4),F32(0,0x935890)));
 rx_addr scene=U32(U32(activity,0x234),0x20138),registry,terrain;ACT_CALLR(registry,0,rx_fx_terrain_registry());
 ACT_CALLR(terrain,0,rx_terrain_for_scene(registry,scene));
 /*This host output models returned volatile EDX, not a guest out parameter.*/
 uint32_t returned_edx=0;ACT_CALL(1,(void)phys_terrain_base_height_at(terrain,F+0x20,&returned_edx));
 for(uint32_t i=0;i<3;i++)SF32(position,4*i,F32(F,0x20+4*i));
 for(uint32_t i=0;i<3;i++)SF32(direction,4*i,rx_f80_to_f32(rx_f80_from_f32(F32(node,0x50+4*i)),0));
 SU32(up,0,0);SU32(up,4,U32(0,0x931f24));SU32(up,8,0);
 ACT_END(F);return 1;
 missing:ACT_END(F);return 0;
}
RX_FUNCTION(0x00579c70,rx_waypoint_respawn_point)
