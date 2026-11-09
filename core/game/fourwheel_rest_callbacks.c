/* Complete actual45/97/130B callback bodies. Explicit proven Buggy-10 map;
 * UTV/Truck share exact original targets, not speculative address aliases. */
#include "fourwheel_rest_callbacks.h"
#include "fx_util.h"
#include "world_util.h"
static uint32_t all_contacts(rx_addr s,unsigned delta){
 uint32_t result=0;
 if(U32(s,0x8420-delta)&&U32(s,0x8180-delta)&&U32(s,0x8570-delta)&&U32(s,0x82d0-delta))result=1;
 fx_set_reg(FX_EAX,result);return result;
}
static uint32_t all_markers(rx_addr s,unsigned delta,unsigned family,rx_addr ra){
 fx_frame f;fx_frame_flat(&f,0,FX_ESI);rx_addr b=f.E;fx_set_reg(FX_ESI,s);
 fx_set_reg(FX_EAX,U32(s,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0x74));
 SU32(b-4,0,ra);rx_set_sp(b-4);rx_fourwheel_rest_contact_dependency(family,b);
 uint32_t result=0;
 if(fx_reg(FX_EAX)&&U32(s,0x83e0-delta)&&U32(s,0x8140-delta)&&U32(s,0x8530-delta)&&U32(s,0x8290-delta)
   &&U32(s,0x83e4-delta)&&U32(s,0x8144-delta)&&U32(s,0x8534-delta)&&U32(s,0x8294-delta))result=1;
 fx_set_reg(FX_EAX,result);fx_frame_leave(&f);return result;
}
static rx_addr rest_normal(rx_addr s,rx_addr output,unsigned delta){
 fx_frame f;fx_frame_enter0(&f,0,16);
 rx_v4 value=v4_add(rx_ld4(s+0x8450-delta),rx_ld4(s+0x81b0-delta));
 value=v4_add(value,rx_ld4(s+0x85a0-delta));value=v4_add(value,rx_ld4(s+0x8300-delta));
 fx_set_reg(FX_EAX,output);rx_st4(output,value);
 /* Existing canonical012 masked normalize: (x²+y²)+z², actual runtime
  * inf/NaN vectors, zero-length masks and all four result lanes preserved. */
 rx_st4(output,v4_normalize_or_nan(value));fx_frame_leave(&f);return output;
}
uint32_t rx_utv_truck_rest_all_contacts(rx_addr s){return all_contacts(s,0);}
uint32_t rx_buggy_rest_all_contacts(rx_addr s){return all_contacts(s,0x10);}
uint32_t rx_utv_truck_rest_all_markers(rx_addr s){return all_markers(s,0,0,0x4ce91a);}
uint32_t rx_buggy_rest_all_markers(rx_addr s){return all_markers(s,0x10,1,0x4b411a);}
rx_addr rx_utv_truck_rest_normal(rx_addr s,rx_addr out){return rest_normal(s,out,0);}
rx_addr rx_buggy_rest_normal(rx_addr s,rx_addr out){return rest_normal(s,out,0x10);}
