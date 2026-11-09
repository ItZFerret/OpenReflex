/*Actual RuleWaypointAdvance and its canonical-registry listener instances.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "build_mxgame_int.h"
#include "race_activity_misc.h"
#include "world_misc.h"
static void attach(rx_addr object){rx_mx_handler_register(U32(0,0xa89bc8),0x9361f4,0x14,0x940410,object,0x51da60,0x9cbc00,2);}
static void detach(rx_addr object){
 ACT_FRAME_EBP(F,8,0x14,1);rx_addr channel;ACT_CALLR(channel,2,rx_world_registry_find(U32(0,0xa89bc8),0x9361f4));
 SU32(F,4,0x940410);SU32(F,8,0);SU32(F,0x10,object);SU32(F,0x14,0x51da60);SU32(F,0xc,crt_type_info_name(0x9cbc00,0x9e1a7c));ACT_CALL(1,rx_channel_remove_listener(channel,F+4));ACT_END(F);
}
void rx_waypoint_rule_subscribe(rx_addr object){attach(object);}
RX_FUNCTION(0x0053b6c0,rx_waypoint_rule_subscribe)
void rx_waypoint_rule_unsubscribe(rx_addr object){detach(object);}
RX_FUNCTION(0x0053b780,rx_waypoint_rule_unsubscribe)
uint32_t rx_waypoint_rule_vf1(rx_addr object,rx_addr driver){
 ACT_FRAME(F,0,1);SU32(object,4,driver);rx_addr channel;ACT_CALLR(channel,2,rx_world_registry_find(U32(0,0xa89bc8),0x9361f4));if(channel)ACT_CALL(1,rx_waypoint_rule_subscribe(object));ACT_END(F);return 1;
}
RX_FUNCTION(0x0051da00,rx_waypoint_rule_vf1)
void rx_waypoint_rule_vf2(rx_addr object){
 ACT_FRAME(F,0,1);rx_addr channel;ACT_CALLR(channel,2,rx_world_registry_find(U32(0,0xa89bc8),0x9361f4));if(channel)ACT_CALL(1,rx_waypoint_rule_unsubscribe(object));ACT_END(F);
}
RX_FUNCTION(0x0051da30,rx_waypoint_rule_vf2)
void rx_waypoint_rule_on_enter(rx_addr object,rx_addr vehicle,int32_t index,rx_addr gate){
 (void)gate;rx_addr driver=U32(object,4);if(U32(U32(driver,0x74),0x1ac)!=vehicle)return;
 ACT_FRAME(F,0,1);int32_t next=(int32_t)(U32(driver,0x10d4)+1u);
 if(index==next||(next>=I32(driver,0x10d8)&&index==0)){uint32_t argument=object;act_vcall(driver,0x24,1,&argument);}ACT_END(F);
}
RX_FUNCTION(0x0051da60,rx_waypoint_rule_on_enter)
