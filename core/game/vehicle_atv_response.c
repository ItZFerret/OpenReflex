/* Original ATV S15 contact response and its bounded rest/slip dependencies.
 * Source: disassembly at 0048d570,0048ef80,0048ee30/60/d0 and matching decompile.
 */
#include "vehicle_atv_response.h"
#include "world_veh_util.h"
#include "world_veh.h"
#include "bike_response.h"
#include "bike_curve.h"
#include "phys_mech.h"
#include "vehicle_atv_physics.h"
/* 0048ef80: suspension angular slip limiter, EDI=S, RET0. Staged. */
static rx_v4 response_wheel_velocity(rx_v4 q,rx_v4 body){
 const rx_v4 conj=v4_mul(q,v4(-1,-1,-1,1));
 return veh_quat_mul(conj,veh_quat_mul(v4_xyz(body),q));
}
void rx_atv_limit_wheel_slip(rx_addr s){
 world_frame frame=world_frame_enter(0xd8,2,1);
 const rx_v4 q=v4_ld(s+0x730),angular=v4_xyz(v4_ld(s+0x740));
 const rx_v4 world=veh_rotate(q,angular);unsigned stopped=0;SU32(s,0xa8,0);
 const uint32_t qoffset[]={0x870,0x910,0x9b0,0xa50},voffset[]={0x880,0x920,0x9c0,0xa60};
 for(unsigned i=0;i<4;i++){
  if(i==2){const float mean=phys_mul(phys_add(F32(s,0xa60),F32(s,0x9c0)),F32(0,0xd615d0));SF32(s,0x9c0,mean);SF32(s,0xa60,mean);}
  const rx_v4 base=response_wheel_velocity(v4_ld(s+qoffset[i]),world);
  const rx_v4 delta=v4_sub(v4_ld(s+voffset[i]),base),square=v4_mul(delta,delta);
  const float sum=phys_add(phys_add(square.v[0],square.v[2]),square.v[1]);
  const rx_v4 length=v4_sqrt(v4_splat(sum));float factor;
  if(F32(s,i<2?0x24:0x28)>0&&v4_all3(v4_cmplt(length,v4_splat(10)))){factor=0;stopped++;}
  else WORLD_CALL_SP(0,factor=bike_param_curve(s+0x3bdc,length.v[0]));
  v4_st(s+voffset[i],v4_add(v4_mul(v4_splat(factor),delta),base));
 }
 if(stopped==4)SU32(s,0xa8,1);
 world_frame_leave(&frame);
}
static rx_v4 response_local(rx_v4 q,rx_v4 v){return veh_quat_mul(v4_mul(q,v4(-1,-1,-1,1)),veh_quat_mul(v4_xyz(v),q));}
static void response_project(rx_addr s){rx_v4 axis=v4_ld(s+0x9720),v=v4_ld(s+0x100),p=v4_mul(axis,v);float dot=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);v4_st(s+0x100,v4_mul(v4_splat(dot),axis));}
static void response_enter_locked(rx_addr s){
 const rx_v4 conj=v4(-1,-1,-1,1),root=v4_ld(s+0x730),cr=v4_mul(root,conj);
 WORLD_CALL_SP(0,phys_mech_set_velocity(s+0xd0,WORLD_ZERO4));
 v4_st(s+0x9730,v4_ld(s+0xf0));
 const rx_v4 fl=veh_quat_mul(veh_quat_mul(v4_mul(v4_ld(s+0x58b0),conj),cr),v4_ld(s+0x870));
 v4_st(s+0x9750,veh_normalize4(fl));
 const rx_v4 fr=veh_quat_mul(veh_quat_mul(v4_mul(v4_ld(s+0x58a0),conj),cr),v4_ld(s+0x910));
 v4_st(s+0x9770,veh_normalize4(fr));
 v4_st(s+0x9740,veh_normalize4(veh_quat_mul(cr,v4_ld(s+0x9b0))));
 v4_st(s+0x9760,veh_normalize4(veh_quat_mul(cr,v4_ld(s+0xa50))));
 const rx_v4 axis=v4_add(v4_add(v4_add(v4_ld(s+0x9510),v4_ld(s+0x9270)),v4_ld(s+0x9660)),v4_ld(s+0x93c0));
 const rx_v4 p=v4_mul(axis,axis);const float len=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);v4_st(s+0x9720,veh_normalize_len2(axis,len));
}
void rx_atv_contact_response(rx_addr s,float dt){
 (void)dt; /* Original consumes the stack float (RET4), but does not read it. */
 world_frame frame=world_frame_enter(0x94,3,1);
 WORLD_CALL_SP(0,bike_contact_response(s));
 WORLD_CALL_SP(4,rx_atv_clamp_body_spin(s));
 if(U32(s,0x5c)){
  const rx_v4 q=v4_ld(s+0x70),root=v4_ld(s+0x730);rx_v4 velocity=response_local(q,v4_ld(s+0x100));
  const rx_v4 world=veh_rotate(root,v4_xyz(v4_ld(s+0x740)));const rx_v4 angular=response_local(q,world);
  if((v4_all3(v4_cmplt(v4_ld(WORLD_ZERO4),v4_splat(velocity.v[2])))&&U32(s,0x60))||(v4_all3(v4_cmplt(v4_splat(velocity.v[2]),v4_ld(WORLD_ZERO4)))&&U32(s,0x64))){velocity.v[1]=F32(WORLD_ZERO4,4);velocity.v[2]=F32(WORLD_ZERO4,8);}
  v4_st(s+0x100,veh_rotate(q,v4(F32(WORLD_ZERO4,0),velocity.v[1],velocity.v[2],0)));
  const rx_v4 world_axis=veh_rotate(q,v4(angular.v[0],F32(WORLD_ZERO4,4),F32(WORLD_ZERO4,8),0));v4_st(s+0x740,response_local(root,world_axis));
 }
 if(!U32(s,0x2de0)){
  WORLD_CALL_SP(4,(void)phys_mech_nodes_slot7(s+0xd0));
  WORLD_CALL_SP(0,rx_atv_limit_wheel_slip(s));
  if(!U32(s,0x90)){if(WORLD_VCALL0(s,0x64)){SU32(s,0x90,1);response_enter_locked(s);}}
  else if(WORLD_VCALL0(s,0x68))SU32(s,0x90,0);
  if(!U32(s,0x90)){world_frame_leave(&frame);return;}
  response_project(s);SF32(s,0x744,F32(WORLD_ZERO4,4));
 }else{
  SU32(s,0x90,0);
  if(U32(s,0x2de4)==2){response_project(s);v4_st(s+0x740,v4_ld(WORLD_ZERO4));v4_st(s+0x880,v4_ld(WORLD_ZERO4));v4_st(s+0x9c0,v4_ld(WORLD_ZERO4));v4_st(s+0x920,v4_ld(WORLD_ZERO4));v4_st(s+0xa60,v4_ld(WORLD_ZERO4));}
  WORLD_CALL_SP(4,(void)phys_mech_nodes_slot7(s+0xd0));
 }
 if(U32(s,0x90)){
  const rx_v4 root=v4_ld(s+0x730),omega=v4_xyz(v4_ld(s+0x740));const rx_v4 world=veh_rotate(root,omega);
  const uint32_t wheel[]={0x870,0x910,0x9b0,0xa50};for(unsigned i=0;i<4;i++)v4_st(s+wheel[i]+0x10,response_local(v4_ld(s+wheel[i]),world));
 }
 world_frame_leave(&frame);
}
/* Actual ATV rest-state query slots reached by inherited vf25/vf26. */
uint32_t rx_atv_all_contacts(rx_addr s){return U32(s,0x94e0)&&U32(s,0x9240)&&U32(s,0x9630)&&U32(s,0x9390);}
uint32_t rx_atv_all_contact_frames(rx_addr s){
 world_frame f=world_frame_flat(1,0);uint32_t r=0;
 if(WORLD_VCALL0(s,0x74))r=U32(s,0x94a0)&&U32(s,0x9200)&&U32(s,0x95f0)&&U32(s,0x9350)&&U32(s,0x94a4)&&U32(s,0x9204)&&U32(s,0x95f4)&&U32(s,0x9354);
 world_frame_leave(&f);return r;
}
rx_addr rx_atv_contact_normal(rx_addr s,rx_addr out){
 world_frame f=world_frame_enter(0,0,1);
 const rx_v4 sum=v4_add(v4_add(v4_add(v4_ld(s+0x9510),v4_ld(s+0x9270)),v4_ld(s+0x9660)),v4_ld(s+0x93c0));
 const rx_v4 p=v4_mul(sum,sum);const float len=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);v4_st(out,sum);v4_st(out,veh_normalize_len2(sum,len));
 world_frame_leave(&f);return out;
}

RX_FUNCTION(0x0048d570,rx_atv_contact_response)
RX_FUNCTION(0x0048ef80,rx_atv_limit_wheel_slip)
RX_FUNCTION(0x0048ee30,rx_atv_all_contacts)
RX_FUNCTION(0x0048ee60,rx_atv_all_contact_frames)
RX_FUNCTION(0x0048eed0,rx_atv_contact_normal)
