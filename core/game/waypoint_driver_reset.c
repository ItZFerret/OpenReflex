/*Complete offline00516a30 reset; network serialization explicitly unclosed.*/
#include "waypoint_driver.h"
#include "waypoint_activity.h"
#include "build_activity_util.h"
#include "build_driver_reset.h"
#include "race_activity.h"
#include "race_driver.h"
#include "loop_player.h"
#include "race.h"
void rx_waypoint_driver_vf3(rx_addr d){
 ACT_FRAME_EBP(F,16,0x34,3);ACT_CALL(0,rx_network_driver_vf3(d));rx_addr player=U32(d,0x74),vehicle=U32(player,0x1ac),source=U32(d,0x144);
 uint32_t placement[5]={source,source+0x10,F+0x20,F+0x10,F+0x30};act_vcall(U32(vehicle,0x234),0x50,5,placement);ACT_CALL(3,rx_vehicle_place(vehicle,F+0x20,F+0x10,F+0x30));
 SU32(d,0x3c0,0);SU32(d,0x3cc,0);for(uint32_t i=0;i<0x18c;i++)SU8(d,0xf18+i,0);
 player=U32(d,0x74);rx_addr a=U32(player,0x94);const uint32_t largest=U32(0,0x977494);SU32(d,0xf10,0);SU32(d,0xf14,0);SU32(d,0x10a4,0);SU32(d,0x10b0,largest);SU32(d,0x10bc,0);SU32(d,0x10d8,U32(a,0xff074));SU32(d,0x10d4,U32(a,0x238)==3?U32(d,0x10d8)-1u:0xffffffffu);SU32(d,0x10c8,1);SU32(d,0x10cc,0);
 if(U32(player,0x10)==1&&(U32(a,0x238)==2||U32(a,0x238)==0))SU32(d,0x10d4,U32(U32(U32(d,0x144),0x20),0x11c));
 ACT_CALL(3,rx_event_driver_3int(U32(a,0x290),0x93eff0,d,0,1,U32(d,0x10d8)));
 player=U32(d,0x74);a=U32(player,0x94);rx_addr gate=rx_waypoint_gate_at(0,a);rx_addr matrix=act_vcall(gate,0x38,0,0);
 ACT_CALL(1,rx_waypoint_emit_pose(U32(U32(U32(d,0x74),0x94),0x290),d,matrix,1));
 ACT_CALL(1,rx_event_driver(U32(U32(U32(d,0x74),0x94),0x290),0x93ef30,d));SU32(d,0x310,0);SU32(d,0x314,0);SU32(d,0x10c0,0);
 player=U32(d,0x74);if(U32(player,0x10)==2){rx_addr human;ACT_CALLR(human,2,rx_activity_driver_of_kind(U32(player,0x94),0,0));rx_addr rider=U32(player,0x1b4);if(rider)SU8(rider,0xa2,0);SU32(d,0x10d0,largest);SU32(d,0x24,human);}
 player=U32(d,0x74);a=U32(player,0x94);ACT_CALL(3,rx_event_driver_3int(U32(a,0x290),0x93eefc,d,U32(d,0xf10),U32(a,0x238)!=2,U32(a,0xff090)));
 float lap=F32(d,0x10b0);if(lap==F32(0,0x977494))lap=0;ACT_CALL(1,rx_event_best_lap(U32(U32(U32(d,0x74),0x94),0x290),d,lap));
 race_static_dcf304();if(U32(0,0xda6d08)||U32(d,0xef0))act_unreached(0x516a30,"Waypoint reset network state serialization is not composed");
 SU32(F,0xc,0xda6cf0);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));
 player=U32(d,0x74);vehicle=U32(player,0x1ac);a=U32(player,0x94);uint32_t type=U32(vehicle,0x198)+U32(vehicle,0x194)*4;
 SF32(d,0x10ac,rx_f80_to_f32(rx_f80_from_f32(F32(a,0xbc4+(type*5+U32(player,0x8f4))*4)),0));
 player=U32(d,0x74);vehicle=U32(player,0x1ac);a=U32(player,0x94);type=U32(vehicle,0x198)+U32(vehicle,0x194)*4;
 SF32(d,0x10b4,rx_f80_to_f32(rx_f80_from_f32(F32(a,0xe44+type*4)),0));
 ACT_CALL(1,rx_driver_behaviors_vf6(d));ACT_CALL(1,rx_driver_rules_vf6(d));rx_addr pace;ACT_CALLR(pace,1,rx_driver_behavior_f(d));SU32(d,0x10dc,pace);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(U32(F,0xc)));ACT_END(F);
}
RX_FUNCTION(0x00516a30,rx_waypoint_driver_vf3)
