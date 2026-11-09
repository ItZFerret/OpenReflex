/* Structural observers of the original677B/220B private frames. Canonical
 * bike_contact owns every math operation/branch; there is no second physics
 * implementation here and no address registration. */
#include "contact_frames.h"
#include "bike_contact.h"
#include "fx_util.h"
#include "phys_vec.h"
#include <stdlib.h>
typedef struct contact_frame {struct contact_frame*previous;rx_addr b;unsigned average,body_count;} contact_frame;
static _Thread_local contact_frame*current;
void* rx_contact_frames_bind(void*p){void*old=current;current=(contact_frame*)p;return old;}
int rx_contact_frames_all3(rx_v4 mask){
 if(current){rx_st4(current->b+0x10,mask);if(current->average){fx_set_reg(FX_EAX,rx_fb(mask.v[1]));fx_set_reg(FX_EDX,rx_fb(mask.v[2])+rx_fb(mask.v[1])+rx_fb(mask.v[0]));}}
 return rx_fb(mask.v[0])+rx_fb(mask.v[1])+rx_fb(mask.v[2])==0xfffffffdu;
}
void rx_contact_frames_average_begin(void){if(current&&current->average)fx_set_reg(FX_EDX,0);}
void rx_contact_frames_body_moved(rx_addr body){if(current){if(current->average){fx_set_reg(FX_EDX,++current->body_count);fx_set_reg(FX_EAX,body+0xa0);}else fx_set_reg(FX_ECX,body);}}
rx_addr rx_contact_wheel_correction_exact(rx_addr w,rx_addr s,rx_addr out){
 rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=w||fx_reg(FX_EDX)!=s||U32(entry,4)!=out)abort();
 fx_frame f;fx_frame_enter(&f,0x18,16,FX_ESI,FX_EDI);
 /* Accepted caller output is above the entered frame; arbitrary aliases with
  * its own consumed private frame need a distinct proof and are rejected. */
 if((uint64_t)out<f.A+8&&(uint64_t)out+16>f.E)abort();
 if(((uint64_t)out<s+0x8800ull&&(uint64_t)out+16>s)||((uint64_t)out<w+0x150ull&&(uint64_t)out+16>w))abort();
 contact_frame c={current,f.E,0,0};current=&c;rx_v4 value=bike_wheel_position_correction(w,s);rx_st4(out,value);fx_set_reg(FX_EAX,out);current=c.previous;fx_frame_leave(&f);if(rx_sp()!=entry)abort();return out;
}
void rx_contact_body_average_exact(rx_addr s){
 rx_addr entry=rx_sp();if(fx_reg(FX_ECX)!=s)abort();fx_frame f;fx_frame_enter(&f,0x1c,16,FX_ESI);contact_frame c={current,f.E,1,0};current=&c;
 bike_body_probe_apply_average_correction(s);current=c.previous;fx_frame_leave(&f);if(rx_sp()!=entry)abort();
}
