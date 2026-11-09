/* Complete005b40e0 semantic owner. The world-entry call is not a user ack. */
#include "career_race_start.h"
#include "build_activity_util.h"
void rx_career_begin_race(rx_addr manager,
    rx_career_vehicle_model_resolver vehicle_model,
    void (*ui_int)(rx_addr,uint32_t),
    uint32_t (*event_call)(rx_addr,uint32_t,uint32_t)) {
    ACT_FRAME(F,4,0); /*original PUSH ECX, restored before RET0*/
    if(!ui_int||!vehicle_model||!event_call)
        act_unreached(0x005b40e0,"unbound career race-start composition");
    SU32(manager,0x184,0);
    ui_int(0x0094d9e4,1); /*SeriesHasRewards*/
    ACT_CALL(0,rx_career_refresh_player_models(manager,vehicle_model));
    ACT_CALL(1,rx_career_refresh_standing_models(manager,(int32_t)U32(manager,0x20)));
    ACT_CALL(1,rx_career_refresh_standing_models(manager,(U32(manager,0x190)!=1)-2));
    ACT_CALL(1,rx_career_refresh_standing_models(manager,-3));
    ACT_CALL(0,(void)event_call(U32(manager,0x18c),12,0));
    /*The virtual call can replace the active event/change phase: reload it.*/
    const rx_addr event=U32(manager,0x18c);
    if(U32(event,0xc)==1)
        ACT_CALL(1,(void)event_call(event,10,U32(manager,0x74)==0));
    ACT_END(F);
}
