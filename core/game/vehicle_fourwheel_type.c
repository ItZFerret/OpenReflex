/* Four-wheel vehicle vf55. Each original is the same 16-byte leaf except
 * for its simulation-pointer offset. Raw type bits are stored unchanged. */
#include "vehicle_fourwheel_type.h"
#include "../src/rx_fn.h"
void rx_buggy_set_simulation_type(rx_addr v,uint32_t type){SU32(U32(v,0x15c0),4,type);}
RX_FUNCTION(0x004a2d00,rx_buggy_set_simulation_type)
void rx_utv_set_simulation_type(rx_addr v,uint32_t type){SU32(U32(v,0x1540),4,type);}
RX_FUNCTION(0x004c0a00,rx_utv_set_simulation_type)
void rx_truck_set_simulation_type(rx_addr v,uint32_t type){SU32(U32(v,0x15d0),4,type);}
RX_FUNCTION(0x0046acf0,rx_truck_set_simulation_type)
