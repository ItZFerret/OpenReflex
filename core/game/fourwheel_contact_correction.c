/* Complete417B callers, ending at originalRET, not next-entry padding.
 * Original family deltas are explicit; shared physical semantics live in
 * bike_contact.c and are not duplicated here. Native observer closure is a
 * separate explicit boundary, never a partially registered caller. */
#include "fourwheel_contact_correction.h"
#include "fx_util.h"
#include "phys_vec.h"
#include <stdlib.h>
static rx_v4 wheel(rx_addr b,rx_addr s,rx_addr w,rx_addr ra){
 fx_set_reg(FX_EAX,b+0x30);fx_set_reg(FX_ECX,w);
 const uint32_t args[]={b+0x30};rx_addr expected=fx_push(1,args,ra);
 rx_addr out=rx_fourwheel_correction_wheel(w,s,b+0x30,ra);
 if(rx_sp()!=expected)abort();return rx_ld4(out);
}
static void correction(rx_addr s,unsigned delta,rx_addr base){
 fx_frame f;fx_frame_enter0(&f,0x70,16);rx_addr b=f.E;
 uint32_t cookie=U32(0,0x9b45e4)^b;fx_set_reg(FX_EAX,cookie);SU32(b,0x6c,cookie);
 rx_v4 zero=v4_splat(F32(0,0x92e12c)),first=zero,second=zero,third=zero,fourth=zero,count=zero;
 rx_st4(b,zero);rx_st4(b+0x20,zero);rx_st4(b+0x40,zero);rx_st4(b+0x50,zero);rx_st4(b+0x10,zero);
 if(U32(s,0x817c-delta)){first=wheel(b,s,s+0x8080-delta,base+0x57);count=v4_add(rx_ld4(b+0x10),rx_ld4(0xd61650));rx_st4(b,first);rx_st4(b+0x10,count);}
 if(U32(s,0x82cc-delta)){second=wheel(b,s,s+0x81d0-delta,base+0x93);count=v4_add(rx_ld4(b+0x10),rx_ld4(0xd61650));rx_st4(b+0x20,second);rx_st4(b+0x10,count);}
 if(U32(s,0x841c-delta)){third=wheel(b,s,s+0x8320-delta,base+0xcf);rx_st4(b+0x40,third);count=v4_add(rx_ld4(b+0x10),rx_ld4(0xd61650));rx_st4(b+0x10,count);}
 if(U32(s,0x856c-delta)){fourth=wheel(b,s,s+0x8470-delta,base+0x110);count=v4_add(rx_ld4(b+0x10),rx_ld4(0xd61650));}
 rx_v4 mask=v4_cmplt(rx_ld4(0xd61670),count);rx_st4(b,mask);fx_set_reg(FX_ECX,U32(b,4));fx_set_reg(FX_EAX,U32(b,8)+U32(b,4)+U32(b,0));
 if(fx_reg(FX_EAX)==0xfffffffdu){
  rx_v4 total=v4_add(v4_add(second,first),rx_ld4(b+0x40));fx_set_reg(FX_ECX,0);
  total=v4_add(total,fourth);rx_v4 mean=v4_div(total,count);
  if(I32(s,0xec)>0){
   fx_set_reg(FX_EAX,s+0xf0);
   do{rx_st4(fx_reg(FX_EAX),v4_add(rx_ld4(fx_reg(FX_EAX)),mean));fx_set_reg(FX_ECX,fx_reg(FX_ECX)+1);fx_set_reg(FX_EAX,fx_reg(FX_EAX)+0xa0);}while((int32_t)fx_reg(FX_ECX)<I32(s,0xec));
  }
 }
 fx_set_reg(FX_ECX,s);rx_addr expected=fx_push(0,NULL,base+0x192);rx_fourwheel_correction_body_probe(s,base+0x192);if(rx_sp()!=expected)abort();
 fx_set_reg(FX_ECX,U32(b,0x6c)^b);expected=fx_push(0,NULL,base+0x19d);rx_fourwheel_correction_cookie(fx_reg(FX_ECX),base+0x19d);if(rx_sp()!=expected)abort();fx_frame_leave(&f);
}
void rx_utv_truck_contact_correction(rx_addr s){correction(s,0,0x4cf310);}
void rx_buggy_contact_correction(rx_addr s){correction(s,0x10,0x4b4210);}
