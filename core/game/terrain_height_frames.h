#pragma once
#include "terrain_height_exact.h"
#include "fx_util.h"
#include "world_rider.h"
#include "world_veh.h"
#include <stdlib.h>
/* Complete small canonical constructor owners, their actual EBP save and RET.
 * No duplicate lane arrangement/math and no original constructor fallback. */
static inline void height_vec_pair(rx_addr out,float first,float second,rx_addr ra){
 if(ra!=0x7c5ee7&&ra!=0x7c5f48&&ra!=0x7c5f68)abort();
 rx_addr caller=rx_sp();uint32_t arg=out;(void)fx_push(1,&arg,ra);rx_addr entry=rx_sp();
 fx_frame f;fx_frame_enter0(&f,0,16);if(out&15)abort();
 rx_world_vec4_from_scalars(out,second,first,second,first);fx_set_reg(FX_EAX,out);
 fx_frame_leave(&f);if(rx_sp()!=entry)abort();rx_set_sp(caller);
}
static inline void height_splat_entered(rx_addr out,rx_addr expected){
 rx_addr entry=rx_sp();if(U32(entry,0)!=0x7c609b||U32(entry,4)!=out||expected!=entry+12)abort();
 uint32_t raw=U32(entry,8);fx_frame f;fx_frame_enter0(&f,0,16);if(out&15)abort();
 rx_world_vec4_splat(out,rx_bf(raw));fx_set_reg(FX_EAX,out);fx_frame_leave(&f);rx_set_sp(expected);
}
static inline void height_cookie_leave(fx_frame*f,rx_addr F,int success){
 /* Original restores EDI/ESI/EBX before CALL cookie but retains original EBP.
  * Cookie is at aligned-locals+B0 = final F+BC, xor with final F+C. */
 fx_set_reg(FX_EDI,f->in.edi);fx_set_reg(FX_ESI,f->in.esi);fx_set_reg(FX_EBX,f->in.ebx);
 rx_set_sp(F+12);fx_set_reg(FX_ECX,U32(F,0xbc)^(F+12));fx_set_reg(FX_EAX,success?1:0);
 if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))abort(); /* never enter typed cookie failure fallback */
 SU32(F+8,0,success?0x7c6159:0x7c5fb3);rx_set_sp(F+8);phys_security_check_cookie(fx_reg(FX_ECX));
 rx_set_sp(F+12);fx_frame_leave(f);
}
