/* Genuine known SlidingRotor publication; no generic vcall or original bytes. */
#include "fourwheel_filtered_contact.h"
#include "contact_pose_frames.h"
#include "fx_util.h"
#include <stdlib.h>
void rx_fourwheel_filtered_publish(rx_addr body,rx_addr ra){
 int valid=0;for(unsigned family=0;family<2;family++){const uint32_t base=family?0x4b43c0u:0x4cf4c0u;const unsigned off[]={0x5bc,0x72a,0x898,0xa05,0xc92,0xc9f,0xcac,0xcb9};for(unsigned i=0;i<8;i++)if(ra==base+off[i])valid=1;}
 rx_addr entry=rx_sp(),vt=U32(body,0);if(!valid||U32(entry,0)!=ra||fx_reg(FX_ECX)!=body||fx_reg(FX_EDX)!=vt||fx_reg(FX_EAX)!=U32(vt,0x18)||U32(vt,0x18)!=0x85a8f0)abort();
 (void)rx_contact_rotor_pose_exact(body);if(rx_sp()!=entry)abort();rx_set_sp(entry+4);
}
