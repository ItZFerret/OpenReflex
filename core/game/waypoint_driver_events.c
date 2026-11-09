/*Actual finite driver listener instances. Network gameplay remains fail-closed.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "build_driver_dtor.h"
#include "build_mxgame_int.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "race_driver.h"
#include "loop_player.h"
#include "race.h"
static void remove_binding(rx_addr registry,rx_addr name,rx_addr object,rx_addr callback,rx_addr vt,rx_addr type){
 ACT_FRAME(F,0x14,1);rx_addr channel;ACT_CALLR(channel,2,rx_world_registry_find(registry,name));SU32(F,4,vt);SU32(F,8,0);SU32(F,0x10,object);SU32(F,0x14,callback);SU32(F,0xc,crt_type_info_name(type,0x9e1a7c));ACT_CALL(1,rx_channel_remove_listener(channel,F+4));ACT_END(F);
}
void rx_waypoint_driver_ghost_subscribe(rx_addr channel,rx_addr driver){rx_mx_handler_register(channel,0x93ee68,0x14,0x9402f0,driver,0x5a4150,0x9cda18,4);}
RX_FUNCTION(0x0053a5b0,rx_waypoint_driver_ghost_subscribe)
void rx_waypoint_driver_ghost_unsubscribe(rx_addr channel,rx_addr driver){remove_binding(channel,0x93ee68,driver,0x5a4150,0x9402f0,0x9cda18);}
RX_FUNCTION(0x0053a670,rx_waypoint_driver_ghost_unsubscribe)
void rx_waypoint_driver_network_subscribe(rx_addr channel,rx_addr driver){rx_mx_handler_register(channel,0x93eed8,0x14,0x9403b0,driver,0x5184b0,0x9cdd3c,4);}
RX_FUNCTION(0x0053b2e0,rx_waypoint_driver_network_subscribe)
void rx_waypoint_driver_network_unsubscribe(rx_addr channel,rx_addr driver){remove_binding(channel,0x93eed8,driver,0x5184b0,0x9403b0,0x9cdd3c);}
RX_FUNCTION(0x0053b3a0,rx_waypoint_driver_network_unsubscribe)
static void offline_driver(rx_addr driver,uint32_t pc){race_static_dcf304();if(U32(0,0xda6d08)||U32(driver,0xef0))act_unreached(pc,"Waypoint driver network state serialization is not composed");}
void rx_waypoint_driver_vf1(rx_addr d){
 ACT_FRAME_EBP(F,8,4,3);rx_addr p=U32(d,0x74);if(U32(p,0x10)==2)ACT_CALL(1,rx_waypoint_driver_ghost_subscribe(U32(U32(p,0x94),0x290),d));
 offline_driver(d,0x5167e0);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));
 race_static_dcf304();ACT_CALL(1,rx_waypoint_driver_network_subscribe(U32(0,0xda6ca0),d));ACT_CALL(0,rx_network_driver_vf1(d));if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(0xda6cf0));ACT_END(F);
}
RX_FUNCTION(0x005167e0,rx_waypoint_driver_vf1)
void rx_waypoint_driver_vf2(rx_addr d){
 ACT_FRAME_EBP(F,8,4,3);rx_addr p=U32(d,0x74);if(U32(p,0x10)==2)ACT_CALL(1,rx_waypoint_driver_ghost_unsubscribe(U32(U32(p,0x94),0x290),d));
 offline_driver(d,0x516920);if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_enter(0xda6cf0));race_static_dcf304();ACT_CALL(1,rx_waypoint_driver_network_unsubscribe(U32(0,0xda6ca0),d));ACT_CALL(0,rx_base_driver_vf2(d));if(!U8(0,0x9df580))ACT_CALL(0,rx_lock_leave(0xda6cf0));ACT_END(F);
}
RX_FUNCTION(0x00516920,rx_waypoint_driver_vf2)
/*0053b010 payload driver, matrix, flag; actual event/signature unchanged.*/
void rx_waypoint_emit_pose(rx_addr channel,rx_addr driver,rx_addr matrix,uint32_t flag){uint32_t payload[3]={driver,matrix,flag};race_raise_event(channel,0x93ef90,0x9cdb00,payload,3);}
RX_FUNCTION(0x0053b010,rx_waypoint_emit_pose)
