/*00538a60 unbounded 3D line projection, used by the actual waypoint follow-height guard.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
float rx_waypoint_line_project(rx_addr end,rx_addr begin,rx_addr point,rx_addr out){
 ACT_FRAME_EBP(F,16,0x20,0);float by=F32(begin,4),bz=F32(begin,8),bx=F32(begin,0);float py=rx_subs(F32(point,4),by);SF32(F,0x14,py);float pz=rx_subs(F32(point,8),bz),dz=rx_subs(F32(end,8),bz);SF32(F,0xc,bz);float dx=rx_subs(F32(end,0),bx),dy=rx_subs(F32(end,4),by),px=rx_subs(F32(point,0),bx);
 float dot=rx_adds(rx_adds(rx_muls(px,dx),rx_muls(F32(F,0x14),dy)),rx_muls(pz,dz));float length=rx_adds(rx_adds(rx_muls(dx,dx),rx_muls(dy,dy)),rx_muls(dz,dz)),t=rx_divs(dot,length);float x=rx_adds(bx,rx_muls(dx,t)),y=rx_adds(by,rx_muls(dy,t)),z=rx_adds(F32(F,0xc),rx_muls(dz,t));SF32(out,0,x);SF32(out,4,y);SF32(out,8,z);ACT_END(F);return t;
}
RX_FUNCTION(0x00538a60,rx_waypoint_line_project)
uint32_t rx_waypoint_navigation_follow_valid(rx_addr o){
 ACT_FRAME_EBP(F,16,0x18,2);uint32_t result=1;if(U32(o,0xa8)!=U32(U32(o,0xa4),0)){rx_addr line=U32(o,0xa8),v=U32(U32(U32(o,8),0x74),0x1ac);ACT_CALL(1,rx_waypoint_line_project(line,U32(line,0x60),v+0xc0,F+0x10));float delta=rx_subs(F32(F,0x14),F32(v,0xc4));if(delta>F32(0,0x9322d0))result=0;}ACT_END(F);return result;
}
RX_FUNCTION(0x00536c30,rx_waypoint_navigation_follow_valid)
