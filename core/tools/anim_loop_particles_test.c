/* Source-evidenced006ba2e0 construction and attachable cleanup fixtures.
 * Link with current core objects. No oracle or asset-load acceptance claim. */
#include "rx_mem.h"
#include "rx_fn.h"
#include "rx_image.h"
#include <stdio.h>
#include <string.h>
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL line%d %s\n",__LINE__,#c);return 1;}}while(0)
static rx_addr called_self,called_arg;
static uint32_t remove_binding(rx_addr self,uint32_t arg){called_self=self;called_arg=arg;return 0;}
int main(void){
 CHECK(rx_mem_init());CHECK(rx_image_load(NULL)==0);CHECK(rx_image_load_crt(NULL)==0);rx_set_sp(0x3000000);
 const rx_addr e=0x2000010,manager=0x2100000,vt=0x2110000;
 memset(rx_base+e-16,0xa5,0xa80);
 rx_addr(*construct)(rx_addr)=RX_FN(0x006ba2e0,rx_addr(*)(rx_addr));CHECK(construct);CHECK(construct(e)==e);
 CHECK(U32(e,0)==0x95f73c&&U32(e,0x90)==0x95f7bc&&U32(e,0x140)==0x95f7fc);
 CHECK(U32(e,0x740)==1&&U32(e,0x744)==0&&U32(e,0x750)==0);
 for(unsigned i=0;i<35;i++)CHECK(U32(e,0x36c+0x1c*i+0x18)==0);
 CHECK(U32(e,-4)==0xa5a5a5a5&&U32(e,0xa60)==0xa5a5a5a5);
 rx_fn_register(0x0f000002,(void*)remove_binding,"fixture_remove_binding");SU32(manager,0,vt);SU32(vt,0x14,0x0f000002);
 void(*unbind)(rx_addr)=RX_FN(0x006c3a60,void(*)(rx_addr));CHECK(unbind);
 SU32(e,4,manager);SU32(e,8,123);unbind(e);
 CHECK(called_self==manager&&called_arg==e+0x80&&U32(e,4)==0&&U32(e,8)==0);
 const rx_addr dels[]={0x00696420,0x006c4e10,0x006c4e70};
 for(int i=0;i<3;i++){
  rx_addr(*del)(rx_addr,uint32_t)=RX_FN(dels[i],rx_addr(*)(rx_addr,uint32_t));CHECK(del);
  called_self=called_arg=0;SU32(e,4,manager);SU32(e,0x10,0x12345678);CHECK(del(e,0)==e);
  CHECK(called_self==manager&&called_arg==e+0x80&&U32(e,0)==0x9587fc&&U32(e,0xd0)==0x95df34&&U32(e,0x94)==0xdeaddead);
  CHECK(U32(e,0x10)==(i==1?0x95e074:0x12345678));
 }
 rx_mem_shutdown();puts("PASS single-loop-particles construction/unbind/three retained-allocation destructors (source-evidenced)");return 0;
}
