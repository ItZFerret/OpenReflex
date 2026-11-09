/*Actual00517880, consumes canonical line/control records without recomputation.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "race.h"
#include "race_driver.h"
#include "race_beh2.h"
#include "phys_crt.h"
static uint32_t vehicle_index(rx_addr p){rx_addr v=U32(p,0x1ac);return U32(v,0x198)+U32(v,0x194)*4;}
void rx_waypoint_driver_line_assist(rx_addr d){
 ACT_FRAME_EBP(F,16,0x24,3);rx_addr behavior;ACT_CALLR(behavior,1,rx_driver_behavior_14(d));rx_addr section=U32(behavior,0xa8);if(!section){ACT_END(F);return;}
 rx_addr p=U32(d,0x74),v=U32(p,0x1ac),line=U32(section,0x60);uint32_t index=vehicle_index(p);
 if(line){float limit=F32(line,0xe8+index*4);if(limit!=F32(0,0x9cd9e8)){
  float speed=F32(v,0x150);if(limit>speed)SF32(d,0x3c0,F32(d,0x3c0)+F32(0,0x9322cc));
  else if(speed>limit){float delta=speed-limit;ACT_CALL(1,rx_track_project_clamped_xz(line,U32(line,0x64),v+0xc0,F+0x20));float dz=F32(F,0x28)-F32(v,0xc8),dy=F32(F,0x24)-F32(v,0xc4),dx=F32(F,0x20)-F32(v,0xc0);float sum=(dz*dz+dy*dy)+dx*dx,dist;ACT_CALLR(dist,1,phys_sqrtf(sum));float force=dist/F32(line,0x4f0);force=force*delta;force=force/delta;force=force+F32(d,0x3c8);SF32(d,0x3c8,force);SF32(d,0x3c4,force);SF32(d,0x3c0,F32(d,0x3c0)-F32(0,0x9322cc));}
 }}else{float limit=F32(section,0xe8+index*4);if(limit!=F32(0,0x9cd9e8)&&F32(v,0x150)>limit){if(act_vcall(v,0xac,0,0)==0){SU32(d,0x3c0,0);SU32(d,0x3c8,U32(0,0x931f24));SU32(d,0x3c4,U32(0,0x931f24));}}}
 line=U32(section,0x60);if(!line){ACT_END(F);return;}p=U32(d,0x74);index=vehicle_index(p);
 if((U8(line,0x68+index*4)&0x20)&&!U32(d,0x30c)){SU32(d,0x3d0,1);SU32(d,0x30c,1);}
 index=vehicle_index(p);if(F32(line,0x168+index*4)>0&&!U32(d,0x2f0))SU32(d,0x2f0,1);
 index=vehicle_index(p);if(F32(line,0x2e8+index*4)>0&&!U32(d,0x2f4)){ACT_CALL(0,(void)rx_ai_start_lean(d,line+0x2e8));SU32(d,0x2f4,1);ACT_END(F);return;}
 index=vehicle_index(p);if((U8(line,0x68+index*4)&8)&&!U32(d,0x31c)){uint32_t slot=U32(p,0x8f4)+(U32(d,0x328)*3+U32(d,0x32c))*4;SF32(d,0x318,F32(0,0x9774a8)-F32(0,0x9e43b8+slot*4));SU32(d,0x31c,1);}ACT_END(F);
}
RX_FUNCTION(0x00517880,rx_waypoint_driver_line_assist)
