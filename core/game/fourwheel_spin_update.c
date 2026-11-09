/* Complete original452650/4b5080,1088 physical bytes/279 instructions each.
 * Canonical bike_quat_mul preserves the original vector-on-left Z product
 * and (Z+Y)+(W+X) association. No substitute angular or tyre physics. */
#include "fourwheel_spin_update.h"
#include "bike_math.h"
#include "fx_util.h"
void rx_fourwheel_spin_update_observed(rx_addr self,unsigned buggy,rx_v4 *live){
    fx_frame frame;fx_frame_enter0(&frame,0,16);
    fx_set_reg(FX_EAX,self);
    const rx_v4 q=rx_ld4(self+0x730);
    const rx_v4 omega=v4_and(rx_ld4(self+0x740),rx_ld4(0x92db80));
    const rx_v4 conjugate=v4_mul(q,rx_ld4(0x93a220));
    const rx_v4 world_omega=bike_quat_mul(q,bike_quat_mul(omega,conjugate));
    const rx_v4 masked=v4_and(world_omega,rx_ld4(0x92db80));
    rx_v4 result=v4_zero();
    for(unsigned i=0;i<4;++i){
        const rx_addr wheel=self+0x7d0+0xa0u*i;
        const rx_v4 wheel_q=rx_ld4(wheel);
        const rx_v4 inner=bike_quat_mul(masked,wheel_q);
        const rx_v4 wheel_conjugate=v4_mul(wheel_q,rx_ld4(0x93a220));
        const rx_v4 angular=rx_ld4(wheel+0x10);
        result=v4_sub(angular,bike_quat_mul(wheel_conjugate,inner));
        SF32(self,0x85c0u-(buggy?0x10u:0)+4u*i,result.v[0]);
    }
    if(live)*live=result;
    fx_frame_leave(&frame);
}
void rx_fourwheel_spin_update_truck(rx_addr self,float dt){(void)dt;rx_fourwheel_spin_update_observed(self,0,0);}
void rx_fourwheel_spin_update_buggy(rx_addr self,float dt){(void)dt;rx_fourwheel_spin_update_observed(self,1,0);}
RX_FUNCTION(0x00452650,rx_fourwheel_spin_update_truck)
RX_FUNCTION(0x004b5080,rx_fourwheel_spin_update_buggy)
