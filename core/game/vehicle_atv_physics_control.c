/*0049ee30 S9: original ATV control/steering, brake, clutch and material setup. */
#include "vehicle_atv_physics_forces.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_curve.h"
#include "phys_misc.h"
#include "phys_terrain.h"
#include "world_veh.h"
void rx_atv_rider_off_control(rx_addr s,float dt);
static float ac_neg(float x){return phys_sub(-0.0f,x);}
static float ac_absmax(float x){return phys_max(phys_sub(0,x),x);}
static float ac_sign(float x){return x>=0?1:-1;}
static float ac_rate(rx_addr f,float target,float rate,float dt){
 float old=F32(f,0),difference=phys_sub(target,old),sign=ac_sign(difference);
 float distance=bike_fabs(difference),step=phys_mul(dt,rate);if(step>distance)step=distance;
 return phys_lag_filter_update(f,dt,bike_x87_copyf(phys_add(phys_mul(sign,step),old)));
}
static float ac_asin_float(float x){
 float a=ac_absmax(x),v=phys_sqrt(ac_absmax(phys_sub(F32(0,0xd61690),phys_mul(x,x))));
 int high=a>=.707106f;float y=high?v:a,u=phys_mul(y,y),u4=phys_mul(u,u);
 float den=phys_add(phys_mul(phys_add(phys_mul(u,-.04992850869894028f),.5648093223571777f),u4),phys_add(phys_mul(u,-1.4430354833602905f),F32(0,0xd61690)));
 float num=phys_mul(phys_add(phys_mul(phys_add(phys_mul(u,-.0193784236907959f),.3993034064769745f),u4),phys_add(phys_mul(u,-1.2763687372207642f),F32(0,0xd61690))),y);
 float result=phys_mul(phys_div(1,den),num);if(high)result=phys_sub(rx_bf(0x3fc90fdb),result);
 return F32(0,0xd61660)<=x?result:phys_sub(0,result);
}
static void ac_air_flags(rx_addr s){
 rx_v4 velocity=rx_ld4(s+0x100);
 if(!(velocity.v[1]>15)){SU32(s,0x9834,0);SU32(s,0x9830,0);return;}
 float length2=phys_add(phys_add(phys_mul(velocity.v[1],velocity.v[1]),phys_mul(velocity.v[0],velocity.v[0])),phys_mul(velocity.v[2],velocity.v[2]));
 float dx=velocity.v[0],dz=velocity.v[2];if(length2>0){float inverse=phys_div(1,phys_sqrt(bike_x87_copyf(length2)));dx=phys_mul(dx,inverse);dz=phys_mul(inverse,dz);}
 float lift=(float)(((double)velocity.v[1]-15.0)/25.0);lift=(float)((double)lift*6.0+1.0);if(!(lift>1))lift=1;else if(!(7>lift))lift=7;
 rx_v4 point=rx_ld4(s+0x230),end=point;point.v[1]=phys_add(lift,point.v[1]);end.v[0]=phys_add(end.v[0],phys_mul(dx,25));end.v[1]=phys_add(lift,end.v[1]);end.v[2]=phys_add(end.v[2],phys_mul(dz,25));
 if(!(U32(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 rx_addr sp=rx_sp(),f=(sp-0x80)&~15u;rx_st4(f,point);rx_st4(f+0x10,end);rx_set_sp(f-16);uint32_t regs[2]={0};
 uint32_t hit=phys_terrain_segment_query(rx_world_find_by_controller(0x9e30d4),f,f+0x10,f+0x20,f+0x30,regs);rx_set_sp(sp);
 if(!hit){SU32(s,0x9834,1);if(bike_fabs(F32(s,0x5968))>.6f&&bike_fabs(F32(s,0x5960))>.6f)SU32(s,0x9830,1);}
 else{SU32(s,0x9834,0);SU32(s,0x9830,0);}
}
void rx_atv_control_step(rx_addr s,float dt){
 if(U32(s,0x2374))rx_atv_rider_off_control(s,bike_x87_copyf(dt));
 rx_v4 q=rx_ld4(s+0x730),velocity=rx_ld4(s+0x100),local=bike_rotate_inv(q,velocity);
 float forward=local.v[2],speed=phys_sqrt(bike_hsum_021(v4_mul(velocity,velocity))),wheel_speed[4];
 for(unsigned i=0;i<4;i++)wheel_speed[i]=ac_absmax(bike_dot3(bike_rotate(rx_ld4(s+0x870+0xa0*i),rx_ld4(0xd615c0)),velocity));
 if(U32(s,0x2de0))SF32(s,0x5898,bike_x87_copyf(F32(s,0x2e70)));
 else{
  float steer_curve=bike_param_curve(s+0x354c,forward),steer=F32(s,0x2c);
  float filtered=ac_rate(s+0x59a8,phys_mul(phys_mul(steer,ac_sign(steer)),steer),100,dt);
  float reflex=F32(s,0x5968),angle=phys_mul(filtered,steer_curve);
  if(phys_mul(reflex,filtered)<=-.05f)angle=phys_sub(angle,phys_mul(bike_fabs(reflex),phys_mul(angle,.2f)));
  else angle=phys_add(phys_mul(phys_mul(steer_curve,.2f),reflex),angle);
  SF32(s,0x5898,angle);
  rx_v4 carriage=bike_quat_mul(q,rx_ld4(0xad66b0)),car_velocity=bike_rotate_inv(carriage,rx_ld4(s+0x100));
  float wobble=0,car_forward=car_velocity.v[2];
  if(bike_all3(v4_cmplt(rx_ld4(0xd61670),v4_splat(car_forward)))){
   rx_v4 flat=v4_mul(car_velocity,v4(1,0,1,1));rx_v4 n=bike_normalize(flat);
   float cosine=phys_max(F32(0,0xd61670),bike_dot3(n,rx_ld4(0xd61680)));
   float heading=phys_sub(rx_bf(0x3fc90fdb),ac_asin_float(cosine));
   if(bike_all3(v4_cmplt(v4_splat(bike_cross(rx_ld4(0xd61680),n).v[1]),rx_ld4(0xd61670))))heading=phys_sub(0,heading);
   float blend=bike_lerp_clamped(bike_clamp01(phys_div(phys_sub(speed,5),15)),0,.5f);
   wobble=phys_mul(blend,heading);float limit=U32(s,0x99a8)?20:5;
   float bound=phys_mul(limit,rx_bf(0x3c8efa34));float sign=ac_sign(wobble);wobble=bike_fabs(wobble);if(wobble>=bound)wobble=bound;wobble=phys_mul(wobble,sign);
  }else if(U32(s,0x9240)||U32(s,0x9390)){
   float factor=phys_max(F32(0,0xd61670),phys_min(F32(0,0xd61650),phys_div(phys_sub(ac_absmax(car_forward),20),40)));
   float phase=F32(s,0x99bc);wobble=ac_neg(phys_mul(bike_cos_poly(phase),factor));
   SF32(s,0x99bc,rx_world_float_fmod(phys_add(phys_mul(dt,15),phase),rx_bf(0x40c90fdb)));
  }
  float rough=phys_mul(phys_add(F32(s,0x93a4),F32(s,0x9254)),.5f);
  if((!U32(s,0x9240)&&!U32(s,0x9390))||rough<=.25f)SF32(s,0x99c0,0);
  else{
   int above=!bike_all3(v4_cmplt(v4_splat(car_forward),v4_splat(10)));
   float factor=above?phys_sub(1,phys_div(phys_sub(car_forward,0),50)):phys_div(car_forward,10);
   factor=phys_max(F32(0,0xd61670),phys_min(F32(0,0xd61650),factor));
   wobble=phys_mul(bike_cos_poly(phys_add(F32(s,0x99c0),rx_bf(0x3fc90fdb))),phys_mul(phys_mul(phys_sub(rough,.25f),.18f),factor));
   float rate=above?bike_lerp_clamped(phys_sub(1,factor),12.5f,20):12.5f;
   if(factor<=0)SF32(s,0x99c0,0);else SF32(s,0x99c0,rx_world_float_fmod(phys_add(phys_mul(rate,dt),F32(s,0x99c0)),rx_bf(0x40c90fdb)));
  }
  float correction=ac_rate(s+0x59d0,wobble,50,dt);angle=phys_add(correction,F32(s,0x5898));
  if(angle>=rx_bf(0x3f1c61a9))angle=rx_bf(0x3f1c61a9);else if(angle<=rx_bf(0xbf1c61a9))angle=rx_bf(0xbf1c61a9);SF32(s,0x5898,angle);
 }
 rx_atv_steering_quaternions(s);
 float front_input=F32(s,0x24),rear_input=F32(s,0x28);if(!U32(s,0x40)&&U32(s,0x99a8))front_input=0;
 for(unsigned i=0;i<4;i++)SF32(s,0x5888+4*i,0);
 float fb=phys_lag_filter_update(s+0x5980,dt,bike_x87_copyf(front_input)),rb=phys_lag_filter_update(s+0x5994,dt,bike_x87_copyf(rear_input));
 if(front_input<fb){phys_lag_filter_set(s+0x5980,front_input,rx_bf(0x7f7fffff));fb=front_input;}
 if(rear_input<rb){phys_lag_filter_set(s+0x5994,rear_input,rx_bf(0x7f7fffff));rb=rear_input;}
 fb=phys_mul(F32(s,0x3a0c),fb);rb=phys_mul(F32(s,0x3af4),rb);
 for(unsigned i=0;i<4;i++){
  float spin=F32(s,0x9680+4*i),brake=i<2?fb:rb;
  float torque=bike_param_curve(s+(i<2?0x3a54:0x3b3c),spin>0?spin:ac_neg(spin));
  torque=phys_mul(torque,brake);if(spin>0)torque=ac_neg(torque);
  if(i<2){float reflex=phys_add(F32(s,0x38),1),factor=phys_sub(1,bike_fabs(F32(s,0x2c)));torque=i==0?phys_mul(phys_mul(reflex,torque),factor):phys_mul(phys_mul(torque,reflex),factor);}
  SF32(s,0x5888+4*i,torque);
 }
 float throttle=F32(s,0x28)<=.01f?F32(s,0x1c):0,water=0;
 if(F32(s,0x68)>0){float t=phys_add(phys_mul(phys_div(phys_sub(F32(s,0x68),.001f),1.499f),.99f),0);if(t>0)water=F32(0,0x9d7894)<=t?F32(0,0x9d7894):t;}
 throttle=phys_sub(throttle,phys_mul(water,throttle));SF32(s,0x5880,throttle);SF32(s,0x5884,0);
 if(F32(s,0x20)>0&&forward<5&&F32(s,0x28)<=.01f){SF32(s,0x5880,0);SF32(s,0x5884,F32(s,0x20));}
 if(U32(s,0x2eb0)){
  if(U32(s,0x5970)){int gear=I32(s,0x96f0)+1;if(gear>=4)gear=4;SU32(s,0x96f0,(uint32_t)gear);SU32(s,0x5970,0);}
  if(U32(s,0x5974)){int gear=I32(s,0x96f0)-1;SU32(s,0x5974,0);SU32(s,0x96f0,gear<1?0:(uint32_t)gear);}
 }
 SU32(s,0x5978,U32(s,0x48));
 int idle=!(bike_fabs(forward)>=.5f)&&!(F32(s,0x5880)>=0)&&!(F32(s,0x5884)>=0);
 if(U32(s,0x48)||idle){float t=phys_div(phys_sub(speed,5),20);SU32(s,0x597c,2);if(t>1)t=1;SF32(s,0x96e8,phys_mul(F32(s,0x4e4c),t));}
 else if(F32(s,0x96e8)<0||(!U32(s,0x94e0)&&!U32(s,0x9630))){SU32(s,0x597c,0);SU32(s,0x96e8,U32(0,0x9cd9dc));}
 else{SU32(s,0x597c,1);SF32(s,0x96e8,phys_sub(F32(s,0x96e8),dt));}
 float clutch;
 if(!U32(s,0x99a8)){
  clutch=F32(s,0x4dbc);float floor=.1f,reflex=F32(s,0x596c);
  if(!(reflex>=0)){clutch=phys_sub(clutch,phys_mul(phys_sub(F32(s,0x4d74),clutch),reflex));floor=.5f;}
  float slip=0,amount=0;
  if(!U32(s,0x94a4)&&U32(s,0x94e0))amount=bike_fabs(F32(s,0x9460));
  else if(!U32(s,0x95f4)&&U32(s,0x9630))amount=bike_fabs(F32(s,0x95b0));
  if(amount>1)slip=bike_clamp01(phys_div(phys_sub(amount,1),25));
  float steer=bike_fabs(F32(s,0x5960));if(slip<=steer)slip=steer;
  float scale=phys_sub(1,slip);if(scale<floor)scale=floor;clutch=phys_mul(clutch,scale);
  if(U32(s,0x94e0)||U32(s,0x9630)){
   rx_v4 sum=v4_zero(),count=v4_zero(),one=rx_ld4(0xd61650);
   if(U32(s,0x94e0)){sum=v4_add(rx_ld4(s+0x9460),sum);count=v4_add(one,count);}
   if(U32(s,0x9630)){sum=v4_add(rx_ld4(s+0x95b0),sum);count=v4_add(count,one);}
   float lateral=phys_mul(rx_rcp(count.v[2]),sum.v[2]);float t=phys_div(bike_fabs(lateral),40);if(t>1)t=1;t=phys_sub(1,t);
   float factor=t>=1?1:t>.1f?t:.1f;clutch=phys_mul(factor,clutch);
  }
 }else clutch=F32(s,0x4d74);
 SF32(s,0x58d0,phys_lag_filter_update(s+0x5a0c,dt,bike_x87_copyf(clutch)));
 for(unsigned i=0;i<4;i++)SF32(s,0x5960+4*i,bike_x87_copyf(F32(s,0x2c+4*i)));
 ac_air_flags(s);
 const uint32_t targetA[]={0x91f0,0x9490,0x9340,0x95e0},targetB[]={0x91e0,0x9480,0x9330,0x95d0};
 for(unsigned i=0;i<4;i++){
  rx_st4(s+targetA[i],v4_splat(bike_param_curve(s+((i&1)?0x47a4:0x4304),forward)));
  rx_st4(s+targetB[i],v4_splat(bike_param_curve(s+((i&1)?0x48e4:0x4444),wheel_speed[i])));
 }
 rx_v4 drag=v4_sub(v4_zero(),bike_normalize(velocity));float resistance=bike_param_curve(s+(U32(s,0x99a8)?0x34ac:0x340c),speed);rx_st4(s+0x58c0,v4_mul(v4_splat(resistance),drag));
 float filtered=phys_lag_filter_update(s+0x59e4,dt,bike_x87_copyf(F32(s,0x34)));
 float target=-1;if((bike_all3(v4_cmple(rx_ld4(0xd61670),v4_splat(local.v[0])))&&filtered>0)||(bike_all3(v4_cmplt(v4_splat(local.v[0]),rx_ld4(0xd61670)))&&filtered<0))target=.5f;
 float blend=phys_lag_filter_update(s+0x59f8,dt,bike_x87_copyf(target)),abs=bike_fabs(filtered),yz=phys_mul(F32(s,0x5834),F32(s,0x38));
 rx_v4 output=v4(phys_add(phys_mul(F32(s,0x587c),filtered),F32(s,0x57e0)),phys_add(phys_mul(blend,abs),F32(s,0x57e4)),phys_add(yz,F32(s,0x57e8)),phys_add(yz,F32(s,0x57ec)));
 rx_st4(s+0x9990,output);rx_st4(s+0x5dc0,output);
}
RX_FUNCTION(0x0049ee30,rx_atv_control_step)
