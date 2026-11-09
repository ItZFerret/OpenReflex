/* Original intrusive proxy-handle lifetime reached by vehicle detach. */
#include "vehicle_proxy_handle.h"
#include "world_veh_util.h"
#include "world_veh_transfer.h"
#include "build_alloc.h"
/* Intrusive proxy-handle release reached by ATV V9; original ownership only. */
static void proxy_record_remove_and_free(rx_addr record){
 world_frame frame=world_frame_flat(1,0); /* original inline push esi */
 WORLD_CALL_SP(4,rx_dyn_records_remove(U32(record,4)+0x1954,U32(record,8)));
 WORLD_CALL_SP(4,rx_reflex_aligned_free(record));world_frame_leave(&frame);
}
void rx_vehicle_proxy_handle_release(rx_addr handle){
 world_frame frame=world_frame_flat(2,0);rx_addr record=U32(handle,4);
 if(record){SU32(record,0,U32(record,0)-1);if(!U32(record,0)){rx_set_sp(rx_sp()-4);record=U32(handle,4);if(record)proxy_record_remove_and_free(record);rx_set_sp(rx_sp()+4);}}
 SU32(handle,4,0);world_frame_leave(&frame);
}
rx_addr rx_vehicle_proxy_handle_delete(rx_addr handle,uint32_t flags){
 world_frame frame=world_frame_flat(1,0);rx_addr record=U32(handle,4);SU32(handle,0,0x93a650);
 if(record){SU32(record,0,U32(record,0)-1);if(!U32(record,0)){rx_set_sp(rx_sp()-4);record=U32(handle,4);if(record)proxy_record_remove_and_free(record);rx_set_sp(rx_sp()+4);}}
 if(flags&1)WORLD_CALL_SP(4,rx_reflex_aligned_free(handle));world_frame_leave(&frame);return handle;
}

RX_FUNCTION(0x0042af20,rx_vehicle_proxy_handle_release)
RX_FUNCTION(0x0042b090,rx_vehicle_proxy_handle_delete)
