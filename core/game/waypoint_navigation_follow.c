/*00535b20: actual authored spline following and bounded fork selection.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
#include "crt.h"
#include <string.h>
static double dload(rx_addr p){uint64_t u=rx_rd64(p);double d;memcpy(&d,&u,8);return d;}
static float rooted(float x){return rx_sqrts(rx_f80_to_f32(rx_f80_from_f32(x),0));}
float rx_waypoint_priority_chance(rx_addr d){rx_addr p=U32(d,0x74);int32_t i=I32(p,0x8fc);return F32(U32(d,0xee8)*0xc7c+(i==-1?0xa8a3a4+(U32(p,0x8f4)*3+U32(p,0x8f8))*8:0xa8a404+(uint32_t)i*8),0);}
RX_FUNCTION(0x0051df90,rx_waypoint_priority_chance)
rx_addr rx_waypoint_navigation_follow(rx_addr o){
 ACT_FRAME_EBP(F,16,0x98,2);rx_addr v=U32(U32(U32(o,8),0x74),0x1ac);float px=F32(v,0xc0),pz=F32(v,0xc8),vx=F32(v,0x130),vz=F32(v,0x138);SF32(F,0x28,px);SF32(F,0x2c,pz);SF32(F,0x20,vx);SF32(F,0x24,vz);float n=rx_adds(rx_muls(vz,vz),rx_muls(vx,vx));SF32(F,0x10,n);if(n!=0){float length;ACT_CALLR(length,1,rooted(n));float inv=rx_divs(F32(0,0x931f24),length);vx=rx_muls(inv,F32(F,0x20));vz=rx_muls(F32(F,0x24),inv);SF32(F,0x20,vx);SF32(F,0x24,vz);}
 float ahead=F32(U32(o,0xa4),0x120);if(!(F32(0,0x931d98)>ahead))ahead=F32(0,0x931d98);SF32(F,0x10,ahead);ACT_CALL(2,rx_track_look_ahead(U32(o,0xa8),F+0x40,F+0x30,rx_f80_to_f32(rx_f80_from_f32(ahead),0)));
 float dx=rx_subs(F32(F,0x28),F32(F,0x40)),dz=rx_subs(F32(F,0x2c),F32(F,0x48));SF32(F,0x18,dx);SF32(F,0x1c,dz);v=U32(U32(U32(o,8),0x74),0x1ac);float numerator=F32(v,0x194)?F32(0,0x9778c0):F32(0,0x9d789c);SF32(F,0x10,numerator);double power;ACT_CALLR(power,0,crt_libm_sse2_pow(dload(0x977638),0));float first=rx_divs(0,(float)power);SF32(F,0x14,first);ACT_CALLR(power,0,crt_libm_sse2_pow(dload(0x977638),dload(0x977ae8)));float gain=rx_subs(F32(F,0x14),rx_divs(F32(F,0x10),(float)power));SF32(F,0x38,gain);
 n=rx_adds(rx_muls(dx,dx),rx_muls(dz,dz));if(n!=0){float length;ACT_CALLR(length,1,rooted(n));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(inv,dx);dz=rx_muls(inv,dz);}SF32(F,0x30,rx_muls(dx,gain));SF32(F,0x34,0);SF32(F,0x38,rx_muls(dz,gain));ACT_CALL(0,rx_race_to_vehicle_frame(v,F+0x30));dx=rx_subs(F32(F,0x40),F32(F,0x28));dz=rx_subs(F32(F,0x48),F32(F,0x2c));n=rx_adds(rx_muls(dz,dz),rx_muls(dx,dx));if(n!=0){float length;ACT_CALLR(length,1,rooted(n));float inv=rx_divs(F32(0,0x931f24),length);dx=rx_muls(dx,inv);dz=rx_muls(dz,inv);}
 float dot=rx_adds(rx_muls(dz,F32(F,0x24)),rx_muls(dx,F32(F,0x20))),half=F32(0,0x9322cc);if(!(dot>half)&&0>dot)dot=0;else if(dot>half)dot=half;rx_addr d=U32(o,8);SF32(d,0x60,rx_adds(rx_muls(rx_subs(F32(0,0x931f24),dot),F32(F,0x30)),F32(d,0x60)));d=U32(o,8);if(F32(o,0xe0)>F32(d,0x130))SF32(d,0x60,rx_muls(rx_subs(F32(0,0x931f24),rx_divs(rx_subs(F32(o,0xe0),F32(d,0x130)),F32(o,0xdc))),F32(d,0x60)));
 d=U32(o,8);rx_addr line=U32(o,0xa8);v=U32(U32(d,0x74),0x1ac);float crossed=rx_adds(rx_adds(rx_muls(F32(line,0x58),rx_subs(F32(v,0xc8),F32(line,8))),rx_muls(rx_subs(F32(v,0xc0),F32(line,0)),F32(line,0x50))),rx_muls(F32(line,0x54),0));if(!(crossed>0)){ACT_END(F);return line;}
 SU32(o,0xa8,U32(line,0x64));uint32_t offsets[]={0x2f0,0x2f4,0x30c,0x31c,0x324,0x318,0x320,0x2f8};for(uint32_t i=0;i<8;i++)SU32(d,offsets[i],0);
 rx_addr section=U32(o,0xa4);if(!U32(o,0xa8)&&U32(section,0x10)){
  for(uint32_t i=0;i<20;i++)SU32(F,0x50+i*4,0);uint32_t priority=0,normal=0;v=U32(U32(U32(o,8),0x74),0x1ac);uint32_t type=U32(v,0x198)+U32(v,0x194)*4;for(int32_t i=0;i<I32(U32(o,0xa4),0x10);i++){rx_addr candidate=U32(U32(section,8),(uint32_t)i*4);if(!(U8(candidate,0x18+type*4)&1)||U32(o,0xa0)!=U32(candidate,0x11c))continue;uint32_t count;if(U32(candidate,0x118)&0x4000){SU32(F,0x50+priority*4,candidate);count=++priority;}else{SU32(F,0x78+normal*4,candidate);count=++normal;}if(count==10)break;}
  rx_addr chosen=0,slot;if(!(U32(U32(o,0xa4),0x118)&0x4000)){uint32_t random=U32(0,0x9cb860)*0x41c64e6d+0x3039;SU32(0,0x9cb860,random);SU32(0,0x9cb864,U32(0,0x9cb864)+1);float chance;ACT_CALLR(chance,0,rx_waypoint_priority_chance(U32(o,8)));if(chance>(float)((random>>16&0x7fff)%100)&&priority)ACT_CALLR(chosen,4,rx_race_choose_section(U32(o,8),F+0x50,priority,0,&slot));}else if(priority)ACT_CALLR(chosen,4,rx_race_choose_section(U32(o,8),F+0x50,priority,0,&slot));
  if(!chosen&&normal)ACT_CALLR(chosen,4,rx_race_choose_section(U32(o,8),F+0x78,normal,0,&slot));if(chosen){SU32(o,0xa4,chosen);SU32(o,0xa8,U32(chosen,0));}
 }
 rx_addr result=U32(o,0xa8);ACT_END(F);return result;
}
RX_FUNCTION(0x00535b20,rx_waypoint_navigation_follow)
