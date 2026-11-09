/* Source-only tuning/placement state fixtures; physical callbacks intentionally isolated, not a ride/oracle claim. */
#include "rx_mem.h"
#include "rx_fn.h"
#include "vehicle_atv_methods.h"
#include <stdio.h>
#include <string.h>
enum{S=0x2100000,P=0x2110000,Q=0x2110010,VT=0x2120000,C=0x2130000,ARR=0x2140000};
static int vf16,vf32;
static uint32_t update(rx_addr s){(void)s;vf16++;return 0;}
static uint32_t probes(rx_addr s){(void)s;vf32++;return 0;}
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static void put4(rx_addr a,float x,float y,float z,float w){SF32(a,0,x);SF32(a,4,y);SF32(a,8,z);SF32(a,12,w);}
static void setup(void){
 memset(rx_base+S,0,0x9bd0);memset(rx_base+S+0x9bd0,0xa5,16);
 SF32(S,0x2ef0,2);SF32(S,0x2ef4,.5f);SF32(S,0x2ef8,3);SF32(S,0x2efc,4);
 const uint32_t a[]={0x3f6c,0x3fb4,0x3ffc,0x3d0c,0x3d54,0x3d9c};for(int i=0;i<6;i++)SF32(S,a[i],(float)(i+1));
 SF32(S,0x508c,2);SF32(S,0x4edc,3);
 const uint32_t b[]={0x4f24,0x4f6c,0x4fb4,0x4ffc,0x5044};for(int i=0;i<5;i++)SF32(S,b[i],(float)(i+1));
 SF32(S,0x35ec,5);SF32(S,0x39c4,6);
}
int main(void){
 CHECK(rx_mem_init());rx_set_sp(0x3000000);
 setup();rx_sim_atv_apply_tuning(S);
 for(int i=0;i<6;i++)CHECK(F32(S,0x96d0+4*i)==2*(i+1));
 for(int i=0;i<5;i++)CHECK(F32(S,0x96f4+4*i)==3*(i+1));
 CHECK(F32(S,0x5a20)==15&&F32(S,0x5a24)==24&&F32(S,0x5984)==24&&F32(S,0x59c0)==15);
 setup();SF32(S,0x2ef8,1);SF32(S,0x2efc,1);SU32(S,0x35ec,0x7f7fffff);SU32(S,0x39c4,0x7f7fffff);SU32(S,0x5984,0x12345678);SU32(S,0x59ac,0x87654321);rx_sim_atv_apply_tuning(S);CHECK(U32(S,0x5984)==0x12345678&&U32(S,0x59ac)==0x87654321&&U32(S,0x5980)==0);
 setup();SU32(S,0x35ec,0x7fa12345);rx_sim_atv_apply_tuning(S);CHECK(U32(S,0x5a20)==0x7fe12345&&U32(S,0x59ac)==0x7fe12345);
 setup();SU32(S,0x39c4,0x80000000);rx_sim_atv_apply_tuning(S);CHECK(U32(S,0x5984)==0x80000000);
 setup();rx_fn_register(0xf100001,(void*)update,"fixture_atv16");rx_fn_register(0xf100002,(void*)probes,"fixture_atv32");
 SU32(S,0,VT);SU32(VT,0x40,0xf100001);SU32(VT,0x80,0xf100002);SU32(S,0x1300,C);
 for(int i=0;i<12;i++)SU32(S,0x9a14+4*i,ARR+16*i);
 put4(0xd61640,0,0,0,1);put4(0xd61670,0,0,0,0);put4(0xd615c0,1,0,0,0);put4(0xd615d0,1,1,1,1);put4(0xd61680,0,1,0,0);put4(0x93a220,-1,-1,-1,1);put4(0xa95d50,0,0,0,0);SF32(0,0x9cd9e8,-1);
 put4(P,10,20,30,1);put4(Q,0,0,0,1);put4(S+0x3e40,-2,0,3,0);put4(S+0x40a0,0,0,-3,0);SF32(S,0x3e94,.5f);SF32(S,0x40f4,1);SF32(S,0x413c,2);
 rx_sim_atv_reset(S,P,Q);
 CHECK(vf16==1&&vf32==1&&F32(S,0xf0)==10&&F32(S,0x190)==10);
 CHECK(F32(S,0x230)==7.5f&&F32(S,0x238)==33&&F32(S,0x2d0)==12.5f);
 CHECK(F32(S,0x370)==8&&F32(S,0x374)==19&&F32(S,0x410)==12&&F32(S,0x418)==27);
 CHECK(U32(S,0x94d8)==1&&U32(S,0x9238)==1&&U32(S,0x9628)==1&&U32(S,0x9388)==1&&U32(S,0xac)==1);
 CHECK(U32(S,0x9bd0)==0xa5a5a5a5&&rx_sp()==0x3000000);rx_mem_shutdown();puts("PASS ATV tuning4 branches and isolated six-body pose/reset fixture");return 0;
}
