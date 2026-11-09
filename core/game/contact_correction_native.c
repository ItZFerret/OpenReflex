/* Complete typed dependency routes for the 417B caller. No original-byte
 * dispatcher; each known role checks its already-entered private frame. */
#include "fourwheel_contact_correction.h"
#include "contact_frames.h"
#include "fx_util.h"
#include "phys_crt.h"
#include <stdlib.h>
static rx_addr frame(rx_addr ra,unsigned pop){rx_addr e=rx_sp();if(U32(e,0)!=ra)abort();return e+pop;}
rx_addr rx_fourwheel_correction_wheel(rx_addr w,rx_addr s,rx_addr out,rx_addr ra){
 int valid=0;for(unsigned family=0;family<2;family++){uint32_t base=family?0x4b4210u:0x4cf310u;const unsigned off[]={0x57,0x93,0xcf,0x110};for(unsigned i=0;i<4;i++)if(ra==base+off[i])valid=1;}
 rx_addr e=rx_sp(),expected=frame(ra,8);if(!valid||fx_reg(FX_ECX)!=w||fx_reg(FX_EDX)!=s||fx_reg(FX_EAX)!=out||U32(e,4)!=out)abort();
 rx_addr result=rx_contact_wheel_correction_exact(w,s,out);if(rx_sp()!=e)abort();rx_set_sp(expected);return result;
}
void rx_fourwheel_correction_body_probe(rx_addr s,rx_addr ra){
 rx_addr e=rx_sp(),expected=frame(ra,4);if((ra!=0x4cf4a2&&ra!=0x4b43a2)||fx_reg(FX_ECX)!=s)abort();rx_contact_body_average_exact(s);if(rx_sp()!=e)abort();rx_set_sp(expected);
}
void rx_fourwheel_correction_cookie(uint32_t cookie,rx_addr ra){
 rx_addr e=rx_sp(),expected=frame(ra,4);if((ra!=0x4cf4ad&&ra!=0x4b43ad)||fx_reg(FX_ECX)!=cookie)abort();phys_security_check_cookie(cookie);if(rx_sp()!=e)abort();rx_set_sp(expected);
}
