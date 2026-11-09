/* Fabricated state only: leaf physics/control contracts, no original assets. */
#include "rx_mem.h"
#include "vehicle_atv_physics.h"
#include <stdio.h>
#include <string.h>
#define S 0x2100000u
#define P 0x2110000u
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#x);return 1;}}while(0)
static void put4(rx_addr a,float x,float y,float z,float w){SF32(a,0,x);SF32(a,4,y);SF32(a,8,z);SF32(a,12,w);}
int main(void){
 CHECK(rx_mem_init());rx_set_sp(0x3000000);memset(rx_base+S,0,0x9be0);
 SU32(S,0x5898,0x80000000);rx_f80 angle=rx_atv_steering_angle(S);CHECK(angle.sign==1&&angle.mant==0&&rx_fb(rx_f80_to_f32(angle,0))==0x80000000);
 SF32(S,0x5898,.5f);angle=rx_atv_steering_angle(S);CHECK(rx_f80_to_f32(angle,0)==.5f);
 SU32(S,0x4c,0x7fa12345);SU32(S,0x50,0x80000000);rx_atv_read_controller(S,0xffffffffu);CHECK(U32(S,0x5970)==0x7fa12345&&U32(S,0x5974)==0x80000000);
 const uint32_t fields[]={0x9964,0x995c,0x9984,0x9980,0x9974,0x9978,0x9958,0x9970};
 for(unsigned i=0;i<8;i++)SU32(S,fields[i],0x12345678);rx_atv_update_contact_filters(S,.01f);
 for(unsigned i=0;i<8;i++)CHECK(U32(S,fields[i])==0);CHECK(F32(S,0x9988)==1&&rx_sp()==0x3000000);
 SF32(0,0x9cd970,.1f);SF32(0,0x9cc37c,100);SF32(0,0x93005c,100);SF32(0,0x9cd9e8,-1);
 SU32(S,0x9834,1);SU32(S,0x3c,1);SF32(S,0x596c,-1);
 rx_atv_update_contact_filters(S,.01f);CHECK(U32(S,0x9958)==1&&F32(S,0x9964)==-1&&rx_fb(F32(S,0x995c))==0x3ba3d70a);
 memset(rx_base+S,0,0x9be0);SU32(S,0x9834,1);SU32(S,0x3c,1);SF32(S,0x5968,.5f);SF32(S,0x596c,-.5f);
 rx_atv_update_contact_filters(S,.01f);CHECK(F32(S,0x9988)==-1&&F32(S,0x9974)>.3f&&F32(S,0x9974)<1&&U32(S,0x9970)==1);
 put4(0xd616a0,0,0,1,0);put4(0xd61670,0,0,0,0);SF32(0,0x9d7b30,-2);
 put4(P,1,2,3,4);put4(P+16,5,6,7,8);put4(P+32,0,.5f,0,1);
 rx_atv_set_relative_pose(S,P);CHECK(!memcmp(rx_base+P,rx_base+S+0x2df0,48));/* The original second polynomial gives rotation x<0,w>0 here. With
  * forward +Z, input quaternion y>0 projects positively onto the rotated
  * forward vector; mirrored y reverses that sign, preserving angle size. */
 float a=F32(S,0x2e70);CHECK(a==a&&a>0);
 SF32(P,36,-.5f);rx_atv_set_relative_pose(S,P);CHECK(F32(S,0x2e70)==-a);
 CHECK(rx_sp()==0x3000000);rx_mem_shutdown();puts("PASS ATV physics leaf contracts");return 0;
}
