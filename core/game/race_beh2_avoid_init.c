/* BehaviorAvoidObjects lifecycle (00520a70 / 00520b90). FreeRide binds this
 * alongside the local human driver's other dormant AI behaviors. The broad AI
 * avoidance update remains its own explicit unported boundary. */
#include "race_beh2_extern.h"
#include "build_activity_freeride_run.h"

/* 00520b90 (ECX = behavior): vf6 clears the pending avoidance count. */
void rx_behavior_avoid_objects_reset(rx_addr self) { SU32(self,0x350,0); }
RX_FUNCTION(0x00520b90, rx_behavior_avoid_objects_reset)

/* 00520a70 (ECX = behavior, stack driver; RET 4): clear the twelve candidate
 * records, scene's AvoidObject regions in <level>Base, and the driver's existing rules. */
void rx_behavior_avoid_objects_init(rx_addr self,rx_addr driver) {
    SU32(self,8,driver);SU32(self,4,1);
    crt_memset(self+0x50,0,0x300);SU32(self,0x350,0);
    const rx_addr activity=U32(U32(driver,0x74),0x94);
    SU32(self,0x35c,beh2_activity_kind_inline(activity));
    const rx_addr world=U32(activity,0x234);
    RX_FRAME_ENTER(F,0x110);
    uint32_t n=0;
    while(U8(world+0x20030,n)&&n<0xff) {SU8(F,n,U8(world+0x20030,n));n++;}
    static const char suffix[]="Base";
    for(uint32_t i=0;i<sizeof suffix;i++)SU8(F,n+i,(uint8_t)suffix[i]);
    const uint32_t count=rx_scene_group_query(U32(world,0x20138),0x1073,self+0x14,0xc,F,0);
    SU32(self,0x44,count);
    SU32(self,0xc,rx_driver_rule_1(driver));SU32(self,0x10,rx_driver_offtrack_penalty_rule(driver));
    RX_FRAME_LEAVE(F,0x110);
}
RX_FUNCTION(0x00520a70, rx_behavior_avoid_objects_init)
