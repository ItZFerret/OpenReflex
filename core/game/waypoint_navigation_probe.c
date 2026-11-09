/*00516590/005166e0: incremental authored terrain-rise probe, shared by waypoint navigation states.*/
#include "waypoint_driver.h"
#include "build_activity_util.h"
#include "world_veh.h"
#include "phys_terrain.h"
static void registry_init(void){if(!(U32(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}}
void rx_waypoint_probe_reset(rx_addr probe,rx_addr origin){
 ACT_FRAME(F,4,0);SU32(probe,0,0);SU32(probe,0x18,0);
 for(uint32_t i=0;i<3;i++)SF32(probe,0x30+i*4,rx_f80_to_f32(rx_f80_from_f32(F32(origin,i*4)),0));
 registry_init();rx_addr terrain;ACT_CALLR(terrain,0,rx_world_find_by_controller(0x9e30d4));uint32_t returned_edx=0;ACT_CALL(1,(void)phys_terrain_base_height_at(terrain,probe+0x30,&returned_edx));ACT_END(F);
}
RX_FUNCTION(0x005166e0,rx_waypoint_probe_reset)
void rx_waypoint_probe_advance(rx_addr probe,rx_addr direction){
 ACT_FRAME_EBP(F,16,0x18,2);registry_init();rx_addr terrain;ACT_CALLR(terrain,0,rx_world_find_by_controller(0x9e30d4));uint32_t step=0;
 while(step<U32(probe,8)){
  float x=rx_adds(F32(probe,0x30),rx_muls(F32(direction,0),F32(probe,0xc)));
  float z=rx_adds(F32(probe,0x38),rx_muls(F32(direction,8),F32(probe,0xc)));
  SF32(F,0x14,F32(probe,0x34));SF32(F,0x10,x);SF32(F,0x18,z);
  if(F32(terrain,0x44d8)>x||x>F32(terrain,0x44e0)||F32(terrain,0x44dc)>z||z>F32(terrain,0x44e4))break;
  uint32_t returned_edx=0;ACT_CALL(1,(void)phys_terrain_base_height_at(terrain,F+0x10,&returned_edx));float y=F32(F,0x14),rise=rx_subs(y,F32(probe,0x34));
  if(rise>F32(probe,0x10)){SU32(probe,0x24,U32(F,0x14));SU32(probe,0x20,U32(F,0x10));SU32(probe,0x28,U32(F,0x18));SU32(probe,0x14,1);SU32(probe,0x18,1);break;}
  SU32(probe,0x34,U32(F,0x14));step++;SU32(probe,0x30,U32(F,0x10));SU32(probe,0x38,U32(F,0x18));
 }
 if(step==U32(probe,8))SU32(probe,0,U32(probe,0)+1);
 if(U32(probe,0)>=U32(probe,4)){SU32(probe,0x20,0);SU32(probe,0x24,0);SU32(probe,0x28,0);SU32(probe,0x14,0);SU32(probe,0x18,1);}
 ACT_END(F);
}
RX_FUNCTION(0x00516590,rx_waypoint_probe_advance)
