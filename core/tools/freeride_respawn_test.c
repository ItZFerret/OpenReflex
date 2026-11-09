/* 00561650 source-evidenced branch fixtures. No original/oracle equivalence claim.
 * Link with current core objects, as rider_stance_test. Real rider/behavior/terrain lookup;
 * fixture vf5 returns the roam type. Empty base terrain intentionally preserves node.y+5. */
#include "rx_mem.h"
#include "rx_fn.h"
#include "race_activity.h"
#include <stdio.h>
#include <string.h>
static uint32_t roam_type(rx_addr self){(void)self;return 14;}
enum {A=0x2000000,P=0x2010000,D=0x2020000,R=0x2030000,V=0x2040000,L=0x2050000,
 S=0x2060000,S2=0x2060100,N=0x2070000,N2=0x2071000,N3=0x2072000,
 T=0x2080000,W=0x2090000,O=0x20a0000,VT=0x20b0000};
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#c);return 1;}}while(0)
static void setup(void){
 memset(rx_base+A,0,0xc0000);
 SU32(A,0x268,L);SU32(A,0x26c,1);SU32(A,0x27c,L);SU32(A,0x280,1);SU32(L,0,D);
 SU32(D,0x74,P);SU32(P,0x1ac,V);SU32(D,0xe8,R);SU32(D,0xee0,1);
 SU32(R,0,VT);SU32(VT,20,0x0f000001);SU32(R,0x30,S);SU32(R,0x34,N);SU32(S,0,N);
 SF32(N,0,1);SF32(N,4,2);SF32(N,8,3);SF32(N,0x50,4);SF32(N,0x54,5);SF32(N,0x58,6);
 SF32(N2,0,11);SF32(N2,4,12);SF32(N2,8,13);SF32(N2,0x50,14);SF32(N2,0x54,15);SF32(N2,0x58,16);
 SU32(A,0x234,W);SU32(W,0x20138,0x1234);
 SU32(0,0x009e30e0,1);SU32(0,0x009e30d4,T);SU32(0,0x009e30dc,1);SU32(T,0x44a8,0x1234);
 memset(rx_base+O,0xa5,0x40);
}
static uint32_t invoke(void){return rx_free_ride_respawn_point(A,P,O+4,O+0x14,O+0x24);}
static int success(rx_addr node){
 CHECK(invoke()==1);CHECK(U32(R,0x34)==node);CHECK(U32(O,4)==U32(node,0));CHECK(F32(O,8)==F32(node,4)+5);CHECK(U32(O,12)==U32(node,8));
 CHECK(U32(O,0x24)==0&&U32(O,0x28)==0x3f800000&&U32(O,0x2c)==0);
 CHECK(U32(O,0)==0xa5a5a5a5&&U32(O,0x10)==0xa5a5a5a5&&U32(O,0x20)==0xa5a5a5a5&&U32(O,0x30)==0xa5a5a5a5);return 0;
}
int main(void){
 CHECK(rx_mem_init());rx_fn_register(0x0f000001,(void*)roam_type,"fixture_roam_type");rx_set_sp(0x3000000);
 setup();CHECK(!success(N));CHECK(F32(O,0x14)==4&&F32(O,0x18)==5&&F32(O,0x1c)==6);
 setup();SU32(P,0x908,1);CHECK(!success(N)); /* waiting driver list */
 setup();SU32(D,0xee0,0);CHECK(invoke()==0);CHECK(U32(O,4)==0xa5a5a5a5);
 setup();SU32(R,0x30,0);CHECK(invoke()==0);CHECK(U32(O,4)==0xa5a5a5a5);
 setup();SU8(N,0x68,2);SU32(N,0x64,N2);CHECK(!success(N2));
 setup();SU32(V,0x194,1);SU32(V,0x198,2);SU8(N,0x80,2);SU32(N,0x64,N2);CHECK(!success(N2)); /* class-specific flag index */
 setup();SU8(N,0x68,2);CHECK(invoke()==0);CHECK(U32(O,4)==0xa5a5a5a5&&U32(R,0x34)==N);
 setup();SU8(N,0x68,2);SU32(S,0x10,1);SU32(S,8,L+4);SU32(L,4,S2);SU32(S2,0,N2);CHECK(!success(N2));CHECK(U32(R,0x30)==S2);
 setup();SU8(N,0x68,2);SU32(S,0x10,1);SU32(S,8,L+4);SU32(L,4,S2);CHECK(!success(N));CHECK(U32(R,0x30)==S2); /* empty next section keeps original node */
 setup();SU32(N,0x50,0x7fa12345);CHECK(invoke()==1);CHECK(U32(O,0x14)==0x7fe12345);
 setup();SU32(N,0x50,0x7fa12345);CHECK(rx_free_ride_respawn_point(A,P,O+4,N+0x54,O+0x24)==1);CHECK(U32(N,0x54)==0x7fe12345&&U32(N,0x58)==0x7fe12345&&U32(N,0x5c)==0x7fe12345); /* sequential overlap */
 CHECK(rx_sp()==0x3000000);rx_mem_shutdown();puts("PASS FreeRide respawn11 branch fixtures (source-evidenced; no oracle/full-lap claim)");return 0;
}
