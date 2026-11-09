/* Native ABI regression for the AI caller of ATV S19. This exercises the
 * production dispatch wrapper and registered rx_f80 callee together; per-call
 * oracle verification alone would not exercise this native cross-module ABI. */
#include "race_beh1_extern.h"
#include "rx_mem.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#c);return 1;}}while(0)
int main(void){
 CHECK(rx_mem_init());rx_set_sp(0x03000000);
 const rx_addr V=0x02000000,S=0x02001000,VT=0x0200b000;
 memset(rx_base+V,0,0xc000);SU32(V,0x234,S);SU32(S,0,VT);SU32(VT,0x4c,0x0047c460);
 const uint32_t inputs[]={0u,0x80000000u,0x3f123456u,0xbf654321u,0x7f7fffffu,0x7fc12345u,0x7fa12345u};
 for(unsigned i=0;i<sizeof inputs/sizeof *inputs;i++){
  SU32(S,0x5898,inputs[i]);rx_f80 expected=rx_f80_from_f32(rx_bf(inputs[i]));
  rx_f80 got=b1_vehicle_steer_angle_x87(V);
  CHECK(got.kind==expected.kind&&got.sign==expected.sign&&got.mant==expected.mant&&got.exp==expected.exp);
  CHECK(rx_fb(b1_vehicle_steer_angle(V))==rx_fb(rx_f80_to_f32(expected,0)));
  CHECK(U32(S,0x5898)==inputs[i]);CHECK(rx_sp()==0x03000000);
 }
 rx_mem_shutdown();puts("PASS native ATV S19 AI dispatch7 finite/zero/NaN cases; no oracle claim");return 0;
}
