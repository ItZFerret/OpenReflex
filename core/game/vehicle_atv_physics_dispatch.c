/*00490b80 S11: original force dispatch order and four dedicated wheel contacts. */
#include "vehicle_atv_physics_forces.h"
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
void rx_atv_step_forces(rx_addr s,float dt){
 /* COMISS throttle,0 / JBE selects coast for unordered as well. */
 const float throttle=F32(s,0x5880);
 SF32(s,0x9714,phys_add(dt,F32(s,0x9714)));
 if(!U32(s,0x99a8)){
  if(throttle>0)rx_atv_choose_power_gear(s);else rx_atv_choose_coast_gear(s);
 }
 rx_atv_update_engine(s,bike_x87_copyf(dt));rx_atv_apply_axle_torques(s);
 rx_atv_apply_suspension(s);rx_atv_apply_resistance(s);
 rx_st4(s+0x9210,rx_ld4(0x00d61670));rx_st4(s+0x9360,rx_ld4(0x00d61670));
 rx_st4(s+0x94b0,rx_ld4(0x00d61670));rx_st4(s+0x9600,rx_ld4(0x00d61670));
 for(unsigned i=0;i<4;i++)rx_atv_tire_contact_force(s,bike_x87_copyf(dt),s+0x9140+0x150*i);
}
RX_FUNCTION(0x00490b80,rx_atv_step_forces)
