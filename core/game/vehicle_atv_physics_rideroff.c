/*004a0ab0 EDI=S, stackdt RET4: original rider-off control state. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_curve.h"
#include "phys_misc.h"
void rx_atv_rider_off_control(rx_addr s,float dt){
 rx_v4 velocity=rx_ld4(s+0x100),q=rx_ld4(s+0x730);
 float speed=phys_sqrt(bike_hsum_021(v4_mul(velocity,velocity)));
 float forward=bike_rotate_inv(q,velocity).v[2];
 rx_atv_steering_quaternions(s);
 for(unsigned i=0;i<4;i++)SF32(s,0x5888+4*i,0);
 SF32(s,0x5880,0);SF32(s,0x5884,0);SU32(s,0x5978,0);
 float time=F32(s,0x5a10),portion=time>dt?dt:time;
 float k=phys_div(portion,time),old=F32(s,0x5a0c);
 SF32(s,0x5a14,k);float clutch=phys_add(phys_mul(phys_sub(F32(s,0x4d74),old),k),old);
 SF32(s,0x5a0c,clutch);SF32(s,0x58d0,clutch);
 for(unsigned i=0;i<4;i++)SF32(s,0x5960+4*i,0);
 const uint32_t curve[]={0x4304,0x4444,0x47a4,0x48e4,0x4304,0x4444,0x47a4,0x48e4};
 const uint32_t target[]={0x91f0,0x91e0,0x9490,0x9480,0x9340,0x9330,0x95e0,0x95d0};
 for(unsigned i=0;i<8;i++)rx_st4(s+target[i],v4_splat(bike_param_curve(s+curve[i],forward)));
 rx_v4 drag=v4_sub(v4_zero(),bike_normalize(velocity));
 float resistance=bike_param_curve(s+(U32(s,0x99a8)?0x34ac:0x340c),speed);
 rx_st4(s+0x58c0,v4_mul(v4_splat(resistance),drag));
 phys_lag_filter_update(s+0x59e4,dt,0);phys_lag_filter_update(s+0x59f8,dt,0);
 rx_v4 output=v4_add(v4(0,-1,0,0),rx_ld4(s+0x57e0));
 rx_st4(s+0x9990,output);rx_st4(s+0x5dc0,output);
}
RX_FUNCTION(0x004a0ab0,rx_atv_rider_off_control)
