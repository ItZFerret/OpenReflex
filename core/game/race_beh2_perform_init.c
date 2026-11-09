/* BehaviorPerformStunt's lifecycle virtuals. Single-player FreeRide constructs this
 * behavior alongside its human driver's other behaviors. The AI stunt decision and
 * execution state machine remains a separate port; no update behavior is substituted. */
#include "race.h"

/* 0052aa80 (ECX = behavior): vf6 resets the two state machines and their flags. */
void rx_behavior_perform_stunt_reset(rx_addr self) {
    SU32(self,0x44,1); SU32(self,0x48,1);
    SU32(self,0x5c,0); SU32(self,0x90,0); SU32(self,0x94,0);
    SU32(self,0xc,0); SU32(self,0x10,0); SU32(self,0x54,0);
    SU32(self,0x58,0xffffffffu); SU32(self,0x50,0);
}
RX_FUNCTION(0x0052aa80, rx_behavior_perform_stunt_reset)

/* 0052aa20 (ECX = behavior, stack driver; RET 4): attach, enabled only for
 * motorcycle / ATV, then its original vf6 reset. This class has no derived vtable. */
uint32_t rx_behavior_perform_stunt_init(rx_addr self,rx_addr driver) {
    SU32(self,8,driver); SU32(self,4,1);
    const uint32_t type=U32(U32(U32(driver,0x74),0x1ac),0x194);
    if(type!=1&&type!=0) SU32(self,4,0);
    rx_behavior_perform_stunt_reset(self);
    return 1;
}
RX_FUNCTION(0x0052aa20, rx_behavior_perform_stunt_init)

/* 0052aa60 (ECX = behavior): vf3 keeps the driver's existing air-leveling behavior. */
void rx_behavior_perform_stunt_activate(rx_addr self) {
    SU32(self,0xa4,rx_driver_air_leveling_behavior(U32(self,8)));
}
RX_FUNCTION(0x0052aa60, rx_behavior_perform_stunt_activate)

/* FreeRide's roam lifecycle uses the same original activity-name enum as the
 * scene's activity-area records. 00508920 (stack C string; RET 4). */
#include "race_extern.h"
#include "crt.h"
uint32_t rx_roam_activity_type(rx_addr name) {
    for(uint32_t i=0;i<0x3e;i++) if(!crt_stricmp(name,U32(0,0x009cd858+4*i)))return i;
    return 0x3e;
}
RX_FUNCTION(0x00508920, rx_roam_activity_type)

/* 0052eec0 BehaviorRoamWorld::vf6: the three ranged control blocks and destination state. */
void rx_behavior_roam_world_reset(rx_addr self) {
    SU32(self,0x68,0x40c00000);
    const uint32_t rate=U32(0,0x009d789c);
    for(uint32_t off=0x74;off<=0xf4;off+=0x40) {
        SU32(self,off+8,rate); SU32(self,off,10); SU32(self,off+4,9);
        SU32(self,off+0xc,0x4059999a); SU32(self,off+0x10,0); SU32(self,off+0x14,1);
    }
    SU32(self,0xc,4);
    for(uint32_t off=0x38;off<=0x60;off+=4) SU32(self,off,0);
    SU32(self,0x30,0); SU32(self,0x34,0);
}
RX_FUNCTION(0x0052eec0, rx_behavior_roam_world_reset)

/* 0052ed50 (stack behavior; RET 4): an AI player chooses another non-FreeRide
 * activity area. Human players (+0x10 == 0) return without consuming random state. */
uint32_t rx_behavior_roam_world_pick_area(rx_addr self) {
    const rx_addr p=U32(U32(self,8),0x74);
    if(!U32(p,0x10))return 0;
    const rx_addr activity=U32(p,0x94);
    const uint32_t current=U32(self,0x10)?rx_roam_activity_type(U32(self,0x10)+0x108):U32(p,0x9c);
    RX_FRAME_ENTER(F,0x80);
    for(uint32_t off=0;off<0x60;off+=4) SU32(F,off,0);
    uint32_t count=0;
    for(uint32_t i=0;i<0x18;i++) {
        const rx_addr area=U32(activity,0xff190+4*i);
        if(area) { const uint32_t type=rx_roam_activity_type(area+0x108); if(type!=current&&type!=6) {SU32(F,4*count,area);count++;} }
    }
    if(count) {
        const rx_addr area=U32(F,4*(race_rand()%count));
        if(area) {
            SU32(self,0x10,area);
            const uint32_t arg[1]={F}; race_thiscall(U32(U32(area,0),0x3c),area,1,arg[0],0,0,0);
            SU32(self,0x20,U32(F,0x30));SU32(self,0x24,U32(F,0x34));SU32(self,0x28,U32(F,0x38));
            SU32(self,0x30,0);SU32(self,0x34,0);
            RX_FRAME_LEAVE(F,0x80);return 1;
        }
    }
    RX_FRAME_LEAVE(F,0x80);return 0;
}
RX_FUNCTION(0x0052ed50, rx_behavior_roam_world_pick_area)

/* 0052ee80 BehaviorRoamWorld::vf1 (ECX = behavior, stack driver; RET 4). */
uint32_t rx_behavior_roam_world_init(rx_addr self,rx_addr driver) {
    SU32(self,8,driver);SU32(self,4,1);SU32(self,0x10,0);
    rx_behavior_roam_world_reset(self);
    if(!rx_behavior_roam_world_pick_area(self))SU32(self,4,0);
    return 1;
}
RX_FUNCTION(0x0052ee80, rx_behavior_roam_world_init)
