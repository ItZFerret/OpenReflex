/* Buggy poststep: original004ac520 and004bd860..004be48b. */
#include "vehicle_buggy_poststep.h"
#include "vehicle_poststep_helpers.h"
#include "world_veh_util.h"

void rx_buggy_simulation_poststep(rx_addr sim){
    world_frame f=world_frame_enter(0x110,0,1);
    WORLD_CALL_SP(4,rx_vehicle_mechanism_node_poststep(sim+0xd0));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(sim,sim+0x8070,phys_mul(F32(sim,0x42d4),F32(0,0x9322cc))));
    WORLD_CALL_SP(8,rx_vehicle_wheel_offset_debug(sim,sim+0x81c0,phys_mul(F32(sim,0x42d4),F32(0,0x9774b0))));
    WORLD_CALL_SP(4,rx_vehicle_wheel_axis_debug(sim,sim+0x8310));
    WORLD_CALL_SP(4,rx_vehicle_wheel_axis_debug(sim,sim+0x8460));
    /*004bd8d8..004be1bc and004be1d3..004be3c8 compute dormant debug
     * geometry into this frame; no calls or externally addressed stores occur.
     * Its only persistent store is this latch reset. Neither the overwritten
     * scratch vectors nor the probe loop's arithmetic have escaping addresses. */
    SU32(sim,0x8010,0);
    /* Preserve the tail's final scratch vector, useful in stack audits. */
    for(unsigned off=0x8740;off<=0x8830;off+=0x10)v4_st(f.esp+0x50,v4_ld(sim+off));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004bd860,rx_buggy_simulation_poststep)

void rx_buggy_poststep(rx_addr vehicle){
    world_frame f=world_frame_flat(1,0);
    int32_t phase=I32(0,0x00ad3ed4)%3;
    if(phase==1||phase==2)WORLD_CALL_SP(0,rx_buggy_simulation_poststep(U32(vehicle,0x15c0)));
    world_frame_leave(&f);
}
RX_FUNCTION(0x004ac520,rx_buggy_poststep)
