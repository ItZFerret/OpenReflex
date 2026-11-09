/* Shared four-wheel poststep callbacks and dormant visual-debug calculations.
 * Hand translated from disassembly: the debug helpers have no persistent writes
 * and no calls. Their scratch pointers never escape. Keep their original frame
 * and final scratch vectors, without the overwritten intermediate spills. */
#include "vehicle_poststep_helpers.h"
#include "world_veh_util.h"

void rx_vehicle_mechanism_node_poststep(rx_addr mechanism){
    world_frame f=world_frame_flat(2,0); /* PUSH EBX/ESI; EDI only if count>0. */
    if(I32(mechanism,0x1c)>0){
        rx_set_sp(rx_sp()-4);
        for(int32_t i=0;i<I32(mechanism,0x1c);i++)WORLD_VCALL(U32(mechanism,0xca0+4u*(uint32_t)i),0x10);
    }
    world_frame_leave(&f);
}
RX_FUNCTION(0x0085c6a0,rx_vehicle_mechanism_node_poststep)

/*004d7fa0..004d815a: the offset is transformed and discarded, followed by a
 * quaternion debug marker. Only the final scratch contents survive to RET8. */
void rx_vehicle_wheel_offset_debug(rx_addr sim,rx_addr wheel,float offset){
    world_frame f=world_frame_enter(0x40,0,1);rx_addr S=f.esp;
    rx_addr body=sim+0xa0u*U32(wheel,0);
    rx_v4 q=v4_ld(body+0x7c0),position=v4_ld(body+0x180);
    rx_v4 point=v4_add(veh_rotate(q,v4(offset,0,0,0)),position);
    v4_st(S,point);v4_st(S,q);
    rx_v4 marker=veh_quat_mul(q,v4(0,0,F32(0,0x977858),F32(0,0x977858)));
    v4_st(S,marker);SF32(S,0x1c,F32(wheel,0x10));
    v4_st(S+0x20,v4_lane(q,2));v4_st(S+0x30,v4_lane(q,3));
    v4_st(S,position);
    if(U32(wheel,0x100)){v4_st(S,v4_ld(wheel+0x50));v4_st(S,v4_ld(wheel+0x60));}
    world_frame_leave(&f);
}
RX_FUNCTION(0x004d7fa0,rx_vehicle_wheel_offset_debug)

/*00459de0..00459f8e: wheel position / debug orientation, then the transformed
 * wheel-local offset. This function also only stores inside its own 0x40 frame. */
void rx_vehicle_wheel_axis_debug(rx_addr sim,rx_addr wheel){
    world_frame f=world_frame_enter(0x40,0,1);rx_addr S=f.esp;
    rx_addr body=sim+0xa0u*U32(wheel,0);
    rx_v4 position=v4_ld(body+0x180),q=v4_ld(body+0x7c0);
    v4_st(S+0x30,position);v4_st(S,position);v4_st(S+0x30,q);
    v4_st(S+0x30,veh_quat_mul(q,v4(0,0,F32(0,0x977858),F32(0,0x977858))));
    SF32(S,0x1c,F32(wheel,0x10));v4_st(S+0x20,v4_lane(q,2));
    v4_st(S+0x30,v4_mul(v4(-1,-1,-1,1),q));
    v4_st(S+0x30,v4_add(veh_rotate(q,v4_ld(wheel+0x20)),position));
    if(U32(wheel,0x100)){v4_st(S,v4_ld(wheel+0x50));v4_st(S,v4_ld(wheel+0x60));}
    world_frame_leave(&f);
}
RX_FUNCTION(0x00459de0,rx_vehicle_wheel_axis_debug)
