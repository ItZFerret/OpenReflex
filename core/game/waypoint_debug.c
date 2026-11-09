/*Actual diagnostic-only virtual methods. Shared007e3be0 is a canonical empty
 * print sink; original argument computations and guest stack stores remain.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "world_node.h"
#include <string.h>
static void emit_number(float x){double d=(double)x;uint64_t bits;memcpy(&bits,&d,8);rx_addr sp=rx_sp();rx_set_sp(sp-8);rx_wr64(sp-8,bits);ACT_CALL(0,rx_world_shared_vf_empty());rx_set_sp(sp);}
void rx_waypoint_pacing_debug(rx_addr o){
 ACT_FRAME_EBP(F,64,0x3c,1);ACT_CALL(0,rx_world_shared_vf_empty());rx_addr d=U32(o,0x28);
 if(I32(d,0x10d4)>=0&&!(F32(0,0x92fe48)>F32(o,0x24))&&U32(o,4)&&!U32(U32(U32(U32(o,8),0x74),0x94),0xa7c)){
  SF32(F,0x3c,rx_adds(rx_divs(F32(o,0x20),F32(o,0x10)),F32(o,0x24)));emit_number(F32(d,0x10b4));emit_number(F32(U32(o,0x28),0x10ac));emit_number(F32(F,0x3c));uint32_t mode=U32(o,0x30);
  if(mode==2){ACT_CALL(0,rx_world_shared_vf_empty());if(!U32(o,0x14))emit_number(F32(o,0x18));}else if(mode==0||mode==1)ACT_CALL(0,rx_world_shared_vf_empty());
 }else ACT_CALL(0,rx_world_shared_vf_empty());ACT_END(F);
}
RX_FUNCTION(0x00526480,rx_waypoint_pacing_debug)
void rx_waypoint_navigation_debug(rx_addr o){
 ACT_FRAME(F,0,1);if(U32(o,4)){ACT_CALL(0,rx_world_shared_vf_empty());if(F32(o,0xe0)>F32(U32(o,8),0x130))emit_number(rx_divs(rx_subs(F32(o,0xe0),F32(U32(o,8),0x130)),F32(o,0xdc)));if(U32(o,0xc)<=5)ACT_CALL(0,rx_world_shared_vf_empty());if(F32(o,0xd8)>F32(U32(o,8),0x130)){
  rx_addr sp=rx_sp();rx_set_sp(sp-8);SU32(sp-8,0,U32(o,0xd4));ACT_CALL(0,rx_world_shared_vf_empty());rx_set_sp(sp-4);rx_addr section=U32(o,0xa4);if(section){const uint32_t masks[]={2,8,4,0x10,0x20};for(uint32_t i=0;i<5;i++){rx_addr v=U32(U32(U32(o,8),0x74),0x1ac);uint32_t index=U32(v,0x198)+U32(v,0x194)*4;if(U8(section,0x18+index*4)&masks[i])ACT_CALL(0,rx_world_shared_vf_empty());}}rx_set_sp(sp);
 }}ACT_END(F);
}
RX_FUNCTION(0x0052ebd0,rx_waypoint_navigation_debug)
