/*00498110 /00498380: original torque-based gear selection. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_curve.h"
static void choose_gear(rx_addr s,int allow_up){
 rx_v4 v=bike_rotate_inv(rx_ld4(s+0x730),rx_ld4(s+0x100));
 float wheelSpeed=phys_div(v.v[2],F32(s,0x4624));if(.5f>wheelSpeed)wheelSpeed=.5f;
 float cap=phys_mul(F32(s,0x4bfc),rx_bf(0x3dd67344));
 if(U32(s,0x2eb0))return;
 int32_t current=I32(s,0x96f0),bestIndex=0;
 float ratio=F32(s,0x96f4+4*(uint32_t)current),drag=F32(s,0x4c8c),best=0;
 for(int32_t i=0;i<5;i++){
  float candidate=F32(s,0x96f4+4*(uint32_t)i),rpm=phys_mul(candidate,wheelSpeed);
  if(rpm>=cap)rpm=cap;if(rpm<=0)rpm=0;
  float loss=bike_param_curve(s+0x4cd4,rpm);loss=phys_mul(loss,drag);loss=phys_mul(loss,ratio);loss=phys_sub(rx_bf(0x80000000),loss);
  float score=phys_mul(bike_param_curve(s+0x4b14,rpm),candidate);
  score=phys_add(phys_sub(score,loss),loss);
  if(score>best||i==0){best=score;bestIndex=i;}
 }
 if(bestIndex>current){if(!allow_up||!(F32(s,0x9714)>F32(s,0x970c)))return;}
 else if(bestIndex<current){if(!(F32(s,0x9714)>F32(s,0x9710)))return;}
 else return;
 if(allow_up){SU32(s,0x9714,0);SU32(s,0x96f0,(uint32_t)bestIndex);}
 else{SU32(s,0x96f0,(uint32_t)bestIndex);SU32(s,0x9714,0);}
}
void rx_atv_choose_power_gear(rx_addr s){choose_gear(s,1);}RX_FUNCTION(0x00498110,rx_atv_choose_power_gear)
void rx_atv_choose_coast_gear(rx_addr s){choose_gear(s,0);}RX_FUNCTION(0x00498380,rx_atv_choose_coast_gear)
