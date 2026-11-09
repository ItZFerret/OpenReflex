/* Original 0054e470 predicate branches; no independent oracle claim. */
#include "rx_mem.h"
#include "player_reset_gate.h"
#include <stdio.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d\n",__LINE__);return 1;}}while(0)
int main(void){
 CHECK(rx_mem_init());const rx_addr p=0x2100000,a=0x2110000,u=0x2120000;
 SU32(0,0xaec048,0);CHECK(rx_player_reset_input_allowed(p)==0);SU32(0,0xaec048,1);CHECK(rx_player_reset_input_allowed(p)==1);
 SU32(0,0xaec048,0);SU32(p,0x94,a);SU32(a,0x21c,2);SU32(a,0x238,2);CHECK(rx_player_reset_input_allowed(p)==1);
 SU32(a,0x21c,1);CHECK(rx_player_reset_input_allowed(p)==0);SU32(a,0x21c,2);SU32(a,0x238,0);
 SF32(0,0x92e12c,0);SF32(0,0x9c5430,4);SF32(0,0x9c5434,1);
 const uint32_t timers[]={0,0x80000000,0xbf800000,0x3f800000,0x40400000,0x40800000,0x7fc12345,0x7f800000};
 for(unsigned i=0;i<8;i++){SU32(p,0x1168,timers[i]);CHECK(rx_player_reset_input_allowed(p)==(i==3));}
 SU32(a,0x23c,8);SU32(0,0xa93fcc,u);SU32(u,0x298,0);CHECK(rx_player_reset_input_allowed(p)==1);SU32(u,0x298,1);CHECK(rx_player_reset_input_allowed(p)==0);
 SU32(0,0xaec048,1);CHECK(rx_player_reset_input_allowed(p)==1);rx_mem_shutdown();puts("PASS reset eligibility branches");return 0;
}
