/*00493630 S7;0048e8a0 S33;004a1250 S34: original locked/relative poses. */
#include "vehicle_atv_physics_forces.h"
#include "vehicle_atv_methods.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "phys_mech.h"
void rx_atv_pin_and_water(rx_addr s,float dt){
 if(!U32(s,0x9838)){rx_st4(s+0x9840,rx_ld4(s+0x730));rx_st4(s+0x9850,rx_ld4(s+0xf0));rx_st4(s+0x9860,rx_ld4(s+0x100));}
 else{
  rx_v4 delta=v4_sub(rx_ld4(s+0x9850),rx_ld4(s+0xf0));rx_st4(s+0x100,rx_ld4(s+0x9860));
  rx_st4(s+0xf0,v4_add(rx_ld4(s+0xf0),delta));
  rx_st4(s+0x190,v4_add(delta,rx_ld4(s+0x190)));
  rx_st4(s+0x230,v4_add(rx_ld4(s+0x230),delta));rx_st4(s+0x2d0,v4_add(rx_ld4(s+0x2d0),delta));
 }
 float level;
 if(U32(s,0x60)&&U32(s,0x5c)){
  level=phys_add(phys_mul(dt,6),F32(s,0x68));if(level>=1.5f)level=1.5f;else if(0>=level)level=0;
 }else{
  if(U32(s,0x5c)){if(!(.2f>=F32(s,0x5880)))return;level=phys_sub(F32(s,0x68),phys_mul(dt,.2f));}
  else level=phys_sub(F32(s,0x68),dt);
  if(!(level>0))level=0;
 }
 SF32(s,0x68,level);
}
void rx_atv_begin_relative_pose(rx_addr s){
 SU32(s,0x2de0,1);SU32(s,0x2e70,0);rx_st4(s+0x2e50,rx_ld4(s+0x730));rx_st4(s+0x2e60,rx_ld4(s+0xf0));
 rx_st4(s+0x2df0,rx_ld4(0xd61670));rx_st4(s+0x2e00,rx_ld4(0xd61640));rx_st4(s+0x2e10,rx_ld4(0xd61640));
 for(unsigned i=0;i<3;i++)rx_st4(s+0x2e20+16*i,rx_ld4(s+0x2df0+16*i));
 if(U32(s,0x2de4)==2){
  rx_st4(s+0x9730,rx_ld4(s+0x2e60));rx_v4 conjugate=bike_quat_conj(rx_ld4(s+0x2e50));
  const uint32_t target[]={0x9750,0x9770,0x9740,0x9760};
  for(unsigned i=0;i<4;i++)rx_st4(s+target[i],bike_qnormalize(bike_quat_mul(conjugate,rx_ld4(s+0x870+0xa0*i))));
  rx_v4 q=bike_quat_mul(rx_ld4(s+0x2e50),v4(rx_bf(0x3e0a3d71),0,0,rx_bf(0x3f7da512)));
  rx_st4(s+0x9720,bike_rotate(q,rx_ld4(0xd616a0)));
 }
}
void rx_atv_apply_relative_pose(rx_addr s){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,B=A-0x38,F=B-8;rx_set_sp(F);veh_cookie(B,0x34);
 uint32_t mode=U32(s,0x2de4);rx_v4 saved=rx_ld4(s+0x2e50);
 if(mode==1){
  rx_v4 delta=bike_rotate(saved,v4_sub(rx_ld4(s+0x2df0),rx_ld4(s+0x2e20)));
  rx_v4 rotation=bike_quat_mul(bike_quat_conj(rx_ld4(s+0x2e30)),rx_ld4(s+0x2e00));
  rx_st4(F+0x20,delta);rx_st4(F+0x10,rotation);phys_mech_translate(s+0xd0,F+0x20);phys_mech_rotate(s+0xd0,F+0x10);
 }else if(mode==2){
  rx_v4 delta=v4_sub(v4_add(bike_rotate(saved,rx_ld4(s+0x2df0)),rx_ld4(s+0x2e60)),rx_ld4(s+0xf0)),normal=rx_ld4(s+0x9720);
  float amount=bike_dot3(normal,delta);delta=v4_sub(delta,v4_mul(v4_splat(amount),normal));rx_st4(F+0x10,delta);phys_mech_translate(s+0xd0,F+0x10);
  rx_v4 target=bike_quat_mul(saved,rx_ld4(s+0x2e00));rx_v4 rotation=bike_quat_mul(bike_quat_conj(rx_ld4(s+0x730)),target);rx_st4(F+0x10,rotation);phys_mech_rotate(s+0xd0,F+0x10);
 }else if(mode==3||mode==4){
  rx_st4(F+0x20,v4_add(bike_rotate(saved,rx_ld4(s+0x2df0)),rx_ld4(s+0x2e60)));rx_st4(F+0x10,bike_quat_mul(saved,rx_ld4(s+0x2e00)));
  rx_sim_atv_reset(s,F+0x20,F+0x10);SU32(s,0x5898,U32(s,0x2e70));SU32(s,0xb0,1);rx_sim_atv_update_frames(s);phys_mech_nodes_slot6(s+0xd0);
 }
 for(unsigned i=0;i<3;i++)rx_st4(s+0x2e20+16*i,rx_ld4(s+0x2df0+16*i));rx_set_sp(sp);
}
RX_FUNCTION(0x00493630,rx_atv_pin_and_water)
RX_FUNCTION(0x0048e8a0,rx_atv_begin_relative_pose)
RX_FUNCTION(0x004a1250,rx_atv_apply_relative_pose)
