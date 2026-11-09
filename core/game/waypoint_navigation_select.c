/*0052dc50: real authored route candidates, separating flagged priority paths from ordinary paths.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
void rx_waypoint_navigation_candidates(rx_addr o,rx_addr out){
 ACT_FRAME_EBP(F,16,0xd4,3);for(uint32_t j=0;j<8;j++)SU32(F,0x6c+j*4,0);for(uint32_t j=0;j<8;j++)SU32(F,0x94+j*4,0);SU32(out,8,U32(0,0x977494));SU32(out,4,0);SU32(out,0,0);SU32(F,0x68,0);SU32(F,0x8c,0);SU32(F,0x18,0);SU32(F,0x90,0);SU32(F,0xb4,0);SU32(F,0x1c,0);for(uint32_t j=0;j<10;j++)SU32(F,0x40+j*4,U32(0,0x977494));SU32(F,0x24,0);
 uint32_t normal=0,priority=0;for(int32_t i=0;i<I32(U32(U32(o,8),0x10),0x1d8);i++){
  rx_addr d=U32(o,8),section=U32(U32(U32(d,0x10),0x130),(uint32_t)i*4);if(U32(section,0x14)||(U8(section,0x118)&4))continue;rx_addr v=U32(U32(d,0x74),0x1ac);uint32_t type=U32(v,0x198)+U32(v,0x194)*4;if(!(U8(section,0x18+type*4)&1)||U32(o,0xa0)!=U32(section,0x11c))continue;
  rx_addr line=U32(section,0);float dx=rx_subs(F32(line,0),F32(v,0xc0)),dz=rx_subs(F32(line,8),F32(v,0xc8)),dy=rx_subs(F32(line,4),F32(v,0xc4));float dist=rx_adds(rx_adds(rx_muls(dz,dz),rx_muls(dy,dy)),rx_muls(dx,dx));if(!(F32(F,0x40+normal*4)>dist)||!(F32(0,0x9cd9e0)>dist))continue;
  dx=rx_subs(F32(line,0),F32(v,0xc0));dy=rx_subs(F32(line,4),F32(v,0xc4));dz=rx_subs(F32(line,8),F32(v,0xc8));float n=rx_adds(rx_adds(rx_muls(dz,dz),rx_muls(dy,dy)),rx_muls(dx,dx));if(n>0){float length;ACT_CALLR(length,1,rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(n),0)));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(dx,inv);dy=rx_muls(dy,inv);dz=rx_muls(dz,inv);}
  float facing=rx_adds(rx_adds(rx_muls(F32(line,0x58),dz),rx_muls(F32(line,0x54),dy)),rx_muls(dx,F32(line,0x50)));if(!(facing>0))continue;SU32(o,0xac+(normal+priority)*4,line);
  uint32_t ncount;if(!(U32(section,0x118)&0x4000)){SU32(F,0x68+normal*4,section);SF32(F,0x40+normal*4,dist);normal++;ncount=normal;}else{SU32(F,0x90+priority*4,section);priority++;ncount=priority;}if(ncount==10)break;
 }
 SU32(F,0x18,normal);SU32(F,0x1c,priority);uint32_t random=U32(0,0x9cb860)*0x41c64e6d+0x3039;SU32(0,0x9cb864,U32(0,0x9cb864)+1);SU32(0,0x9cb860,random);rx_addr d=U32(o,8),p=U32(d,0x74);int32_t explicit_index=I32(p,0x8fc);rx_addr tune=U32(d,0xee8)*0xc7c+(explicit_index==-1?0xa8a3a4+(U32(p,0x8f4)*3+U32(p,0x8f8))*8:0xa8a404+(uint32_t)explicit_index*8);rx_addr chosen=0,slot;
 if(F32(tune,0)>(float)((random>>16&0x7fff)%100)&&priority)ACT_CALLR(chosen,4,rx_race_choose_section(d,F+0x90,priority,0,&slot));if(!chosen&&normal)ACT_CALLR(chosen,4,rx_race_choose_section(U32(o,8),F+0x68,normal,0,&slot));if(chosen){SU32(out,0,chosen);SU32(out,4,U32(chosen,0));}SF32(o,0xd8,rx_adds(F32(U32(o,8),0x130),F32(0,0x92e134)));SU32(o,0xd4,normal+priority);ACT_END(F);
}
RX_FUNCTION(0x0052dc50,rx_waypoint_navigation_candidates)
uint32_t rx_waypoint_navigation_reselect(rx_addr o){ACT_FRAME(F,0xc,0);ACT_CALL(2,rx_waypoint_navigation_candidates(o,F));uint32_t result=0;if(U32(F,0)){SU32(o,0xa4,U32(F,0));SU32(o,0xa8,U32(F,4));SF32(o,0xe0,rx_adds(F32(U32(o,8),0x130),F32(o,0xdc)));SU32(o,0xc,0);result=1;}ACT_END(F);return result;}
RX_FUNCTION(0x0052e060,rx_waypoint_navigation_reselect)
