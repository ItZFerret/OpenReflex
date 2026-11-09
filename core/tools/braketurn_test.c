/* Source-evidenced branch contracts, not an oracle comparison. Parent links current source objects. */
#include "rx_mem.h"
#include "rx_fn.h"
#include "race_braketurn.h"
#include <stdio.h>
#include <string.h>
enum{B=0x2000000,D=0x2010000,P=0x2020000,V=0x2030000,VT=0x2040000};
static int resets,disables;
static void reset(rx_addr b){resets++;SU32(b,0x10,0);SU32(b,4,0);}
static void disable(rx_addr b,uint32_t en){disables++;rx_brake_turn_enable(b,en);}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d: %s\n",__LINE__,#x);return 1;}}while(0)
static void setup(void){
 memset(rx_base+B,0xa5,0x50000);memset(rx_base+B,0,0x40);memset(rx_base+D,0,0x400);memset(rx_base+P,0,0x900);memset(rx_base+V,0,0x160);
 SU32(B,0,VT);SU32(VT,0x18,0xf000001);SU32(VT,0x1c,0xf000002);SU32(B,8,D);SU32(D,0x74,P);SU32(P,0x1ac,V);
 SF32(0,0x9e3648,.25f);SF32(0,0x9e364c,.5f);SF32(0,0x9e3650,2);SF32(0,0x9e3654,3);SF32(0,0x9e3658,4);SF32(0,0x9e365c,5);SF32(0,0x9e3660,.75f);
 SF32(D,0x130,10);SF32(V,0x150,3);SF32(B,0x24,1);SF32(B,0x30,3);SF32(B,0x34,4);resets=disables=0;
}
int main(void){
 CHECK(rx_mem_init());rx_fn_register(0xf000001,(void*)reset,"fixture_reset");rx_fn_register(0xf000002,(void*)disable,"fixture_disable");rx_set_sp(0x3000000);
 setup();rx_brake_turn_enable(B,7);CHECK(U32(B,4)==7&&U32(B,0x10)==1&&U32(B,0x20)==0&&F32(B,0x24)==1);rx_brake_turn_enable(B,0);CHECK(resets==1&&U32(B,0x10)==0);
 setup();SU32(B,0x18,0xffffffff);CHECK(rx_brake_turn_steer(B)==1);CHECK(F32(B,0x20)==.25f&&F32(B,0x24)==.75f&&F32(D,0x60)==-.25f&&F32(D,0x3e0)==-.75f&&F32(D,0x64)==1&&U32(B,0x10)==0);
 setup();SF32(B,0x20,1.25f);rx_brake_turn_steer(B);CHECK(F32(B,0x20)==1.25f&&F32(B,0x24)==1);
 setup();SF32(V,0x130,1);rx_brake_turn_steer(B);CHECK(U32(B,0x10)==2&&F32(B,0x14)==13); /* normalized3/5 dot reaches angle */
 setup();SF32(V,0x150,1);rx_brake_turn_steer(B);CHECK(U32(B,0x10)==2); /* speed below */
 setup();SF32(V,0x150,2);rx_brake_turn_steer(B);CHECK(U32(B,0x10)==0); /* speed equality does not end */
 setup();SU32(B,0x20,0x7fc12345);SU32(V,0x150,0x7fc12345);rx_brake_turn_steer(B);CHECK(U32(B,0x20)==0x7fc12345&&U32(B,0x10)==0);
 setup();SU32(B,0x30,0);SU32(B,0x34,0);SU32(B,0x38,0);SF32(V,0x150,2);rx_brake_turn_steer(B);CHECK(U32(B,0x10)==0); /* zero delta skips sqrt */
 setup();SU32(B,0x20,0);SU32(B,0x24,0);SF32(0,0x9e3648,0);SU32(B,0x18,0xffffffff);rx_brake_turn_steer(B);CHECK(U32(D,0x60)==0x80000000&&U32(D,0x3e0)==0x80000000);
 setup();CHECK(rx_brake_turn_clutch(B)==0&&U32(D,0x3d0)==1&&disables==1&&resets==1);
 setup();SU32(B,0xc,1);rx_brake_turn_clutch(B);CHECK(U32(B,0x10)==4&&F32(B,0x14)==14&&disables==0);
 setup();SF32(B,0x14,10);rx_brake_turn_wait_pullback(B);CHECK(U32(B,0x10)==0);SF32(B,0x14,9);rx_brake_turn_wait_pullback(B);CHECK(U32(B,0x10)==5&&F32(B,0x14)==15);
 setup();SU32(B,0x14,0x7fc12345);rx_brake_turn_wait_pullback(B);CHECK(U32(B,0x10)==0);
 setup();SF32(B,0x14,10);rx_brake_turn_pullback(B);CHECK(F32(D,0x3e0)==-.75f&&F32(D,0x3d8)==-.75f&&!disables);SF32(B,0x14,9);rx_brake_turn_pullback(B);CHECK(disables==1);
 CHECK(U32(B,0x40)==0xa5a5a5a5&&rx_sp()==0x3000000);rx_mem_shutdown();puts("PASS BrakeTurn14 source branch fixtures");return 0;
}
