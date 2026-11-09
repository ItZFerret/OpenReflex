#pragma once
#include "terrain_cell_frames.h"
#include "phys_coll_grid.h"
#ifdef __cplusplus
extern "C" {
#endif
/*007c5440 ESIwork/EDIcenter, stack{neighbors,grid,scale}, cdeclRET0.
 * Retains enteredESP. Caller alone models RET and pending arg cleanup. */
void phys_terrain_sample_window_exact(rx_addr work,rx_addr center,rx_addr neighbors,rx_addr grid,rx_addr scale,rx_v4*live_xmm0);
#ifdef __cplusplus
}
#endif
static inline rx_v4 window_floor_to_int_entered(rx_v4 value,rx_addr dest){
 rx_addr entry=rx_sp(),parent=(fx_reg(FX_EBP)&~15u)-0xd0u;
 if(U32(entry,0)!=0x7c54ee||entry!=parent-12||dest!=parent+0x10||U32(entry,4)!=dest||U32(entry,8)!=dest||fx_reg(FX_ECX)!=dest)abort();
 fx_frame f;fx_frame_enter0(&f,0x20,16);rx_st4(f.E,value);
 rx_v4 result=phys_vec4_floor_to_int(value);rx_st4(f.E+0x10,result);
 fx_set_reg(FX_EAX,v4_lb(result,3));fx_set_reg(FX_ECX,v4_lb(result,1));fx_set_reg(FX_EDX,v4_lb(result,2));
 fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(entry+4);return result;
}
static inline void window_store_entered(rx_v4 value,rx_addr dest){
 rx_addr entry=rx_sp(),parent=(fx_reg(FX_EBP)&~15u)-0xd0u;
 if(U32(entry,0)!=0x7c54f6||entry!=parent-8||dest!=parent+0x10||U32(entry,4)!=dest)abort();
 fx_frame f;fx_frame_enter0(&f,0x20,16);SU32(f.E,0x1c,U32(0,0x9b45e4)^f.E);
 fx_set_reg(FX_EAX,dest);rx_st4(f.E,value);if(dest&15)abort();
 (void)phys_vec4_store_aligned(value,dest);if(dest)fx_set_reg(FX_EAX,dest&~15u);
 fx_set_reg(FX_ECX,U32(f.E,0x1c)^f.E);if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort();
 SU32(f.E-4,0,0x69b446);rx_set_sp(f.E-4);phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(f.E);
 fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(entry+4); /* RET0, dest argument remains */
}
