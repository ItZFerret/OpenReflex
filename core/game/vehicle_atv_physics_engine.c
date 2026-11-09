/*004985e0: original engine, clutch and rear axle inertia/torque update. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_curve.h"
#include "crt.h"
static float engine_clamp(float x,float lo,float hi){if(x>=hi)x=hi;if(x<=lo)x=lo;return x;}
static float engine_neg(float x){return phys_sub(rx_bf(0x80000000),x);}
static float engine_mix(float a,float b,float t){return phys_add(phys_mul(phys_sub(b,a),t),a);}
static void engine_inertia(rx_addr s,float added){
 rx_v4 delta=v4(added,0,0,0),base=rx_ld4(s+0x38e0),inertia=v4_add(delta,base);
 rx_st4(s+0x9d0,inertia);rx_st4(s+0x9e0,v4_div(rx_ld4(0x00d61650),inertia));
 inertia=v4_add(delta,rx_ld4(s+0x38e0));rx_st4(s+0xa70,inertia);rx_st4(s+0xa80,v4_div(rx_ld4(0x00d61650),inertia));
}
void rx_atv_update_engine(rx_addr s,float dt){
 float maximum=phys_div(F32(s,0x4bfc),rx_bf(0x4118cccd)),minimum=phys_div(F32(s,0x4c44),rx_bf(0x4118cccd));
 float average=v4_mul(v4_add(rx_ld4(s+0xa60),rx_ld4(s+0x9c0)),rx_ld4(0x00d615d0)).v[0];
 float rpm=engine_clamp(F32(s,0x96ec),0,maximum);SF32(s,0x96ec,rpm);
 float normalized=engine_clamp(phys_div(phys_sub(rpm,minimum),phys_sub(maximum,minimum)),0,1);
 float throttle=F32(s,0x5880);SF32(s,0x9708,phys_mul(phys_add(phys_sub(throttle,normalized),1),.5f));
 float blend=engine_clamp(phys_div(phys_sub(F32(s,0x4e4c),F32(s,0x96e8)),F32(s,0x4e4c)),0,1);
 blend=rx_f80_to_f32(crt_ci_pow(rx_f80_from_f32(blend),rx_f80_from_f64(.25),CRT_CW_MAIN),0);
 float idle=phys_div(F32(s,0x4bb4),rx_bf(0x4118cccd));
 if(blend<=0)rpm=idle;else if(!(blend>=1))rpm=engine_mix(idle,rpm,blend);
 SF32(s,0x96ec,rpm);rpm=engine_clamp(rpm,0,maximum);SF32(s,0x96ec,rpm);
 float ratio=F32(s,0x96f4+4*(uint32_t)I32(s,0x96f0));
 float power=bike_param_curve(s+0x4b14,rpm),multiplier=F32(s,0x4e04),drag=F32(s,0x4c8c);
 float loss=engine_neg(phys_mul(bike_param_curve(s+0x4cd4,rpm),phys_mul(phys_mul(multiplier,ratio),drag)));
 if(bike_all3(v4_cmplt(v4_splat(F32(s,0x9c0)),rx_ld4(0x00d61670))))loss=engine_neg(loss);
 float selectedThrottle=throttle,reverse=F32(s,0x5884);
 if(reverse>0){ratio=F32(s,0x96f4);selectedThrottle=reverse;power=F32(s,0x9c0)<-25?0:-20;}
 float drive=0,reaction=0;int32_t mode=I32(s,0x597c);
 if(mode==0){
  float coupled=engine_mix(loss,phys_mul(power,ratio),selectedThrottle);
  reaction=engine_neg(phys_mul(F32(s,0x58d0),coupled));drive=engine_neg(coupled);
  engine_inertia(s,phys_mul(phys_add(F32(s,0x397c),F32(s,0x3934)),.5f));
  SF32(s,0x96ec,phys_mul(ratio,average));
 }else if(mode==1){
  float freeLoss=engine_neg(phys_mul(bike_param_curve(s+0x4cd4,rpm),drag));
  float freeTorque=engine_mix(freeLoss,phys_mul(phys_mul(ratio,2),power),throttle);
  float clutch=F32(s,0x58d0),engineMass=F32(s,0x3934);
  float freeRPM=phys_mul(phys_add(phys_div(phys_mul(freeTorque,dt),engineMass),rpm),multiplier);
  float coupled=engine_mix(loss,phys_mul(power,ratio),selectedThrottle);if(coupled<0)coupled=0;
  drive=bike_lerp_clamped(blend,engine_neg(freeTorque),engine_neg(coupled));
  reaction=bike_lerp_clamped(blend,engine_neg(phys_mul(clutch,freeTorque)),engine_neg(phys_mul(clutch,coupled)));
  float linked=bike_lerp_clamped(blend,freeRPM,phys_mul(ratio,average));SF32(s,0x96ec,linked);linked=engine_clamp(linked,0,maximum);SF32(s,0x96ec,linked);
  engine_inertia(s,bike_lerp_clamped(blend,phys_mul(F32(s,0x397c),.5f),phys_mul(phys_add(engineMass,F32(s,0x397c)),.5f)));
 }else if(mode==2){
  float freeLoss=engine_neg(phys_mul(bike_param_curve(s+0x4cd4,rpm),drag));engine_inertia(s,phys_mul(F32(s,0x397c),.5f));
  float torque=engine_mix(freeLoss,power,selectedThrottle);
  SF32(s,0x96ec,phys_add(phys_div(phys_mul(torque,dt),F32(s,0x3934)),F32(s,0x96ec)));
 }
 SF32(s,0x96ec,engine_clamp(F32(s,0x96ec),minimum,maximum));
 rx_v4 axle=v4(phys_mul(engine_neg(drive),.5f),0,0,0),root=v4(phys_mul(reaction,.5f),0,0,0);
 int left=U32(s,0x94e0)!=0,right=U32(s,0x9630)!=0;
 if(left){rx_st4(s+0xa00,v4_add(axle,rx_ld4(s+0xa00)));rx_st4(s+0x780,v4_add(root,rx_ld4(s+0x780)));}
 if(right){rx_st4(s+0xaa0,v4_add(axle,rx_ld4(s+0xaa0)));rx_st4(s+0x780,v4_add(root,rx_ld4(s+0x780)));}
 if(!left&&!right){
  rx_st4(s+0xa00,v4_add(rx_ld4(s+0xa00),axle));rx_st4(s+0x780,v4_add(root,rx_ld4(s+0x780)));
  rx_st4(s+0xaa0,v4_add(axle,rx_ld4(s+0xaa0)));rx_st4(s+0x780,v4_add(root,rx_ld4(s+0x780)));
 }
}
RX_FUNCTION(0x004985e0,rx_atv_update_engine)
