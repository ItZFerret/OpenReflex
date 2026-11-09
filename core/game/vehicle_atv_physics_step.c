/*004a17b0: roll spring and004931a0 S17: relative wheel spin publication. */
#include "vehicle_atv_physics.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
void rx_atv_roll_spring(rx_addr s,float dt){
 rx_v4 q=rx_ld4(s+0x730),axis=bike_rotate(q,rx_ld4(0x00d616a0));
 rx_v4 torque=bike_rotate_inv(q,rx_ld4(s+0x160));
 float spring=F32(s,0x99a0);
 /* Source004a199c..19c7 multiplies ONLY projected force by zero. The
  * decompiler incorrectly groups spring under that zero multiplication. */
 float force=phys_add(phys_mul(spring,-75),phys_mul(bike_dot3(axis,rx_ld4(s+0xd0)),0));
 force=phys_add(force,phys_sub(rx_bf(0x80000000),torque.v[1]));
 float rate=phys_mul(phys_add(phys_mul(force,dt),F32(s,0x99a4)),rx_bf(0x3f75c28f));
 SF32(s,0x99a4,rate);SF32(s,0x99a0,phys_add(phys_mul(rate,dt),spring));
}
RX_FUNCTION(0x004a17b0,rx_atv_roll_spring)
void rx_atv_publish_wheel_spin(rx_addr s,float dt){
 rx_atv_roll_spring(s,rx_bf(veh_x87_copy(rx_fb(dt))));
 rx_v4 root=bike_rotate(rx_ld4(s+0x730),rx_ld4(s+0x740));
 for(unsigned i=0;i<4;i++){
  rx_v4 local=bike_rotate_inv(rx_ld4(s+0x870+0xa0*i),root);
  SF32(s,0x9680+4*i,phys_sub(F32(s,0x880+0xa0*i),local.v[0]));
 }
 rx_v4 p=rx_ld4(s+0xf0);for(unsigned i=0;i<3;i++)SF32(s,0x9ab0+4*i,p.v[i]);
}
RX_FUNCTION(0x004931a0,rx_atv_publish_wheel_spin)
