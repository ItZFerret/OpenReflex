#pragma once
#include "phys_vec.h"
#include "phys_coll_grid.h"
#include "phys_math.h"
#include "fx_util.h"
#include <stdlib.h>
/* These wrappers observe canonical owners once. Caller models RET0 and any
 * argument cleanup; no duplicated floor/store arithmetic or registration. */
static inline rx_v4 terrain_floor_frame(rx_v4 x,int integer){
 if(U32(rx_sp(),0)!=(integer?0x7c63aa:0x7c6318))abort();
 fx_frame f;fx_frame_enter0(&f,0x20,16);rx_st4(f.E,x);
 rx_v4 result=integer?phys_vec4_floor_to_int(x):phys_vec4_floor(x);rx_st4(f.E+0x10,result);
 if(integer){fx_set_reg(FX_EAX,v4_lb(result,3));fx_set_reg(FX_ECX,v4_lb(result,1));fx_set_reg(FX_EDX,v4_lb(result,2));}
 fx_frame_leave(&f);return result;
}
static inline rx_v4 terrain_store_frame(rx_v4 x,rx_addr dest){
 if(U32(rx_sp(),0)!=0x7c63b2||U32(rx_sp(),4)!=dest)abort();
 fx_frame f;fx_frame_enter0(&f,0x20,16);uint32_t cookie=U32(0,0x9b45e4)^f.E;SU32(f.E,0x1c,cookie);fx_set_reg(FX_EAX,dest);rx_st4(f.E,x);
 if(dest&15)abort();rx_v4 result=phys_vec4_store_aligned(x,dest);if(dest)fx_set_reg(FX_EAX,dest&~15u);
 fx_set_reg(FX_ECX,U32(f.E,0x1c)^f.E);SU32(f.E-4,0,0x69b446);rx_set_sp(f.E-4);phys_security_check_cookie(fx_reg(FX_ECX));rx_set_sp(f.E);fx_frame_leave(&f);return result;
}
