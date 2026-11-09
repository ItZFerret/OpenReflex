/*00498db0: apply the four axle drive/brake torques and equal root reaction. */
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
void rx_atv_apply_axle_torques(rx_addr s){
 for(unsigned side=0;side<2;side++){
  float rear=F32(s,0x5890+4*side);
  rx_v4 torque=v4(rear,0,0,0);
  rx_st4(s+0xa00+0xa0*side,v4_add(rx_ld4(s+0xa00+0xa0*side),torque));
  rx_v4 counter=v4(phys_sub(0,rear),0,0,0);
  rx_st4(s+0x780,v4_add(rx_ld4(s+0x780),counter));
  float front=F32(s,0x5888+4*side);
  rx_v4 q=rx_ld4(s+0x870+0xa0*side);
  rx_v4 world=bike_rotate(q,v4(phys_sub(0,front),0,0,0));
  rx_v4 reaction=bike_rotate_inv(rx_ld4(s+0x730),world);
  rx_st4(s+0x8c0+0xa0*side,v4_add(rx_ld4(s+0x8c0+0xa0*side),v4(front,0,0,0)));
  rx_st4(s+0x780,side?v4_add(rx_ld4(s+0x780),reaction):v4_add(reaction,rx_ld4(s+0x780)));
 }
}
RX_FUNCTION(0x00498db0,rx_atv_apply_axle_torques)
