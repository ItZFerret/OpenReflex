#include "rider_pose_override.h"
#include "world_wreck.h"
#include "fx_util.h"
#include <stdlib.h>
void rx_rider_override_listeners(rx_addr list,rx_addr value,rx_addr ra){
 const rx_addr e=rx_sp();const fx_saved in=fx_save();
 if((ra!=0x61c164&&ra!=0x61f08d)||U32(e,0)!=ra||U32(e,4)!=list||U32(e,8)!=value)abort();
 rx_world_float_listeners_set(list,value);
 if(rx_sp()!=e||U32(e,0)!=ra||fx_reg(FX_EBX)!=in.ebx||fx_reg(FX_EBP)!=in.ebp||fx_reg(FX_ESI)!=in.esi||fx_reg(FX_EDI)!=in.edi)abort();
 rx_set_sp(e+12);
}
