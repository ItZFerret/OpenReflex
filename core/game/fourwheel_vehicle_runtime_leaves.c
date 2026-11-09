/* Hand-written complete small Vehicle vtable owners. Only the original
 * volatile outputs are assigned; the entered stack/nonvolatiles are retained. */
#include "fourwheel_vehicle_runtime_leaves.h"
#include "fx_util.h"
uint32_t rx_utv_buggy_vehicle_kind(rx_addr v){uint32_t x=U32(v,0x280)!=0;uint32_t r=0x30u+2u*x;fx_set_reg(FX_EAX,r);return r;}
RX_FUNCTION(0x004ad530,rx_utv_buggy_vehicle_kind)
uint32_t rx_truck_vehicle_kind(rx_addr v){uint32_t r=U32(v,0x280)?0x2fu:0x22u;fx_set_reg(FX_EAX,r);return r;}
RX_FUNCTION(0x004744a0,rx_truck_vehicle_kind)
static uint32_t has_nodes(rx_addr v,uint32_t first){
 uint32_t r=U32(v,first)||U32(v,first+4)||U32(v,first-8)||U32(v,first-4);fx_set_reg(FX_EAX,r);return r;
}
uint32_t rx_utv_vehicle_has_nodes(rx_addr v){return has_nodes(v,0x1470);}RX_FUNCTION(0x004c0360,rx_utv_vehicle_has_nodes)
uint32_t rx_buggy_vehicle_has_nodes(rx_addr v){return has_nodes(v,0x14c0);}RX_FUNCTION(0x004a2520,rx_buggy_vehicle_has_nodes)
uint32_t rx_truck_vehicle_has_nodes(rx_addr v){return has_nodes(v,0x1500);}RX_FUNCTION(0x0046a3b0,rx_truck_vehicle_has_nodes)
static uint32_t has_id(rx_addr v,uint32_t id,uint32_t first){
 fx_set_reg(FX_EDX,id);uint32_t result=0;
 /* All four original reads occur even after the first matching field. */
 for(unsigned i=0;i<4;++i)if(U32(v,first+4*i)==id)result=1;
 fx_set_reg(FX_EAX,result);return result;
}
uint32_t rx_utv_vehicle_has_id(rx_addr v,uint32_t id){return has_id(v,id,0x1590);}RX_FUNCTION(0x004c7e90,rx_utv_vehicle_has_id)
uint32_t rx_buggy_vehicle_has_id(rx_addr v,uint32_t id){return has_id(v,id,0x1610);}RX_FUNCTION(0x004ad540,rx_buggy_vehicle_has_id)
uint32_t rx_truck_vehicle_has_id(rx_addr v,uint32_t id){return has_id(v,id,0x1620);}RX_FUNCTION(0x00474460,rx_truck_vehicle_has_id)
rx_f80 rx_fourwheel_vehicle_zero_value(rx_addr v,uint32_t ignored){(void)v;(void)ignored;return rx_f80_from_int(0);}RX_FUNCTION(0x004c0390,rx_fourwheel_vehicle_zero_value)
rx_f80 rx_vehicle_one_value(rx_addr v,uint32_t ignored){(void)v;(void)ignored;return rx_f80_from_int(1);}RX_FUNCTION(0x00430260,rx_vehicle_one_value)
