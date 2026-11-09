/* ATV contact integration: original 00490c80 and its own four-wheel helpers.
 * Shared collision primitives retain their real ABI; ATV offsets/order are explicit. */
#include "vehicle_atv_contact_dispatch.h"
#include "build_vehicle_util.h"
#include "bike_contact.h"
#include "bike_slot13.h"
#include "phys_coll_terrain.h"
#include "phys_quat.h"
#include "phys_contact.h"
#include "world_veh.h"
#include "rx_vcall.h"
#include "phys_crt.h"
#include "loop_jobs.h"

void rx_atv_query_wheel_terrain(rx_addr W,rx_addr S){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,B=A-0x68,F=B-8;
 rx_set_sp(F);veh_cookie(B,0x64);
 const rx_addr body=S+0xa0*U32(W,0);
 rx_st4(F+0x50,rx_ld4(S+0x730));rx_st4(F+0x30,rx_ld4(body+0x730));
 rx_st4(F+0x10,v4_splat(phys_mul(F32(W,0x10),F32(0,0x9779e4))));
 rx_v4 p=v4_add(bike_rotate(rx_ld4(body+0x730),rx_ld4(W+0x20)),rx_ld4(body+0xf0));
 rx_st4(F+0x40,p);rx_st4(F+0x20,v4_add(v4_splat(F32(0,0x92e12c)),p));
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 rx_addr world;PHYS_CALL_SP(16,world=rx_world_find_by_controller(0x9e30d4));
 const rx_addr tri=rx_job_scratch(0xad6720,200*48);   /* the ATVs' one triangle buffer: a parallel simulation job's own copy */
 PHYS_CALL_SP(20,phys_terrain_triangles(F+0x20,world,tri,S+0x9130,200,F+0x10));
 uint32_t count=U32(S,0x9130),hit=0;
 rx_job_scratch_used(0xad6720,count*48u);
 if(count){PHYS_CALL_SP(20,hit=bike_wheel_triangle_contact(F+0x20,F+0x10,F+0x30,F+0x40,W+0x10,tri,(int32_t)count,NULL));}
 SU32(W,0xfc,hit);
 if(hit)PHYS_CALL_SP(16,phys_probe_frame_setup(F+0x40,F+0x10,W,F+0x20,F+0x30,F+0x50));
 phys_security_check_cookie(U32(B,0x64)^B);rx_set_sp(sp);
}
RX_FUNCTION(0x00497f00,rx_atv_query_wheel_terrain)

void rx_atv_correct_contact_positions(rx_addr S){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x70;rx_set_sp(F);veh_cookie(F,0x6c);
 rx_v4 corr[4],n=v4_splat(F32(0,0x92e12c));
 for(unsigned i=0;i<4;i++){
  corr[i]=v4_splat(F32(0,0x92e12c));rx_addr W=S+0x9140+0x150*i;
  if(U32(W,0xfc)){PHYS_CALL_SP(4,corr[i]=bike_wheel_position_correction(W,S));n=v4_add(n,rx_ld4(0xd61650));}
 }
 if(bike_all3(v4_cmplt(rx_ld4(0xd61670),n))){
  rx_v4 avg=v4_div(v4_add(v4_add(v4_add(corr[1],corr[0]),corr[2]),corr[3]),n);
  for(int32_t i=0;i<I32(S,0xec);i++){rx_addr p=S+0xf0+0xa0*i;rx_st4(p,v4_add(rx_ld4(p),avg));}
 }
 PHYS_CALL_SP(0,bike_body_probe_apply_average_correction(S));phys_security_check_cookie(U32(F,0x6c)^F);rx_set_sp(sp);
}
RX_FUNCTION(0x00492ff0,rx_atv_correct_contact_positions)

void rx_atv_pin_rest_position(rx_addr S){
 if(!U32(S,0x90))return;
 rx_v4 d=v4_sub(rx_ld4(S+0xf0),rx_ld4(S+0x9730)),n=rx_ld4(S+0x9720);
 rx_v4 move=v4_sub(v4_mul(v4_splat(bike_dot3(n,d)),n),d);
 for(int32_t i=0;i<I32(S,0xec);i++){rx_addr p=S+0xf0+0xa0*i;rx_st4(p,v4_add(rx_ld4(p),move));}
}
RX_FUNCTION(0x0048edc0,rx_atv_pin_rest_position)

void rx_atv_update_rolling_orientation(rx_addr S,float dt){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u;rx_set_sp(A-0x50);
 rx_v4 velocity[4];
 for(unsigned w=0;w<4;w++){
  float old[3];for(unsigned k=0;k<3;k++)old[k]=F32(U32(S,0x9a14+12*w+4*k),0);
  rx_v4 pos=rx_ld4(S+0x230+0xa0*w);float nv[3];
  for(unsigned k=0;k<3;k++){
   rx_addr f=U32(S,0x9a14+12*w+4*k);float T=F32(f,4),x=dt<T?dt:T;
   float factor=phys_div(x,T),v=phys_add(phys_mul(phys_sub(pos.v[k],F32(f,0)),factor),F32(f,0));
   SF32(f,8,factor);SF32(f,0,v);nv[k]=v;
  }
  velocity[w]=v4(phys_div(phys_sub(nv[0],old[0]),dt),phys_div(phys_sub(nv[1],old[1]),dt),phys_div(phys_sub(nv[2],old[2]),dt),0);
 }
 if(U32(S,0x90)||(U32(S,0x2de0)&&U32(S,0x2de4)==2)){
  rx_v4 q=rx_ld4(S+0x730);
  rx_st4(S+0x870,bike_quat_mul(bike_quat_mul(q,rx_ld4(S+0x58b0)),rx_ld4(S+0x9750)));
  q=rx_ld4(S+0x730);rx_st4(S+0x910,bike_quat_mul(bike_quat_mul(q,rx_ld4(S+0x58a0)),rx_ld4(S+0x9770)));
  q=rx_ld4(S+0x730);rx_st4(S+0x9b0,bike_quat_mul(q,rx_ld4(S+0x9740)));
  q=rx_ld4(S+0x730);rx_st4(S+0xa50,bike_quat_mul(q,rx_ld4(S+0x9760)));
  for(unsigned w=0;w<4;w++)rx_vcall(U32(S,0xd78+4*w),0x18,0,NULL);
 }else{
  for(unsigned w=0;w<4;w++){
   rx_addr W=S+0x9140+0x150*w,q=S+0x870+0xa0*w;
   if(!U32(W,0x100)||!U32(W,0xc0))continue;
   float speed=bike_dot3(velocity[w],rx_ld4(W+0x120));
   rx_v4 omega=rx_ld4(q+0x10);omega.v[0]=v4_div(v4_mul(v4_splat(speed),rx_ld4(0xd615c0)),rx_ld4(W+0x10)).v[0];omega.v[3]=F32(0,0xd6166c);
   rx_v4 prev=rx_ld4(q+0x80);
   rx_st4(q,bike_qnormalize(v4_add(v4_mul(bike_quat_mul(prev,omega),v4_splat(phys_mul(dt,0.5f))),prev)));
   rx_vcall(U32(S,0xd78+4*w),0x18,0,NULL);
  }
 }
 rx_set_sp(sp);
}
RX_FUNCTION(0x00492320,rx_atv_update_rolling_orientation)

static void atv_wheel_contacts(rx_addr S,rx_addr W,rx_addr surface,rx_addr F,unsigned w){
 rx_addr body=S+0xa0*U32(W,0);
 rx_st4(F+0x80,rx_ld4(S+0x730));rx_st4(F+0x70,rx_ld4(body+0x730));
 rx_st4(F+0x60,v4_add(bike_rotate(rx_ld4(body+0x730),rx_ld4(W+0x20)),rx_ld4(body+0xf0)));
 rx_st4(S+0x9120,rx_ld4(W+0x10));
 rx_st4(F+0x50,rx_ld4(0xd61670));rx_st4(F+0x30,rx_ld4(0xd61670));
 PHYS_CALL_SP(4,rx_atv_query_wheel_terrain(W,S));
 int32_t count=0;rx_v4 norm=rx_ld4(F+0x30);
 if(U32(W,0xfc)){rx_st4(F+0x50,rx_ld4(W+0x50));norm=rx_ld4(W+0x40);count=1;}
 SU32(surface,0,0);
 for(uint32_t i=0;i<(uint32_t)((I32(S,0x1310)-I32(S,0x130c))/0x50);i++){
  rx_addr desc=U32(S,0x130c)+0x50*i;rx_v4 ext=rx_ld4(W+0x10),pos=rx_ld4(F+0x60);
  if(!bike_all3(v4_cmplt(pos,v4_add(rx_ld4(desc+0x30),ext)))||!bike_all3(v4_cmplt(v4_sub(rx_ld4(desc+0x20),ext),pos)))continue;
  SU32(F,0x740,0);SU32(F,0xa0,0);
  for(unsigned j=0;j<30;j++){SU32(F,0x650+4*j,0xffffffff);SU32(F,0x6c8+4*j,0xffffffff);}
  PHYS_CALL_SP(28,phys_contact_dispatch(F+0xa0,U32(U32(desc,0x44),4),desc+0x10,desc,S+0x9100,F+0x70,F+0x60,0));
  int32_t n=I32(F,0xa0);
  if(n>0){count+=n;rx_v4 points=rx_ld4(F+0x50);
   for(int32_t j=0;j<n;j++){rx_addr c=F+0xb0+0x30*j;
    points=w<2?v4_add(rx_ld4(c),points):v4_add(points,rx_ld4(c));
    norm=w==0?v4_add(rx_ld4(c+0x10),norm):v4_add(norm,rx_ld4(c+0x10));
   }
   rx_st4(F+0x50,points);SU32(surface,0,U32(U32(S,0x130c)+0x50*i,0x40));
  }
 }
 if(count>0){SU32(W,0xfc,1);rx_st4(F+0x50,v4_div(rx_ld4(F+0x50),v4_splat((float)count)));rx_st4(F+0x30,bike_normalize(norm));
  PHYS_CALL_SP(16,phys_probe_frame_setup(F+0x60,F+0x30,W,F+0x50,F+0x70,F+0x80));}
}
void rx_atv_contact_dispatch(rx_addr S,float dt){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,B=A-0x754,F=B-12;rx_set_sp(F);veh_cookie(B,0x750);
 for(unsigned j=0;j<30;j++){SU32(F,0x650+4*j,0xffffffff);SU32(F,0x6c8+4*j,0xffffffff);}
 for(unsigned w=0;w<4;w++)atv_wheel_contacts(S,S+0x9140+0x150*w,S+0x90e8+4*w,F,w);
 uint32_t any=0;
 for(unsigned w=0;w<4;w++){rx_addr W=S+0x9140+0x150*w;uint32_t hit=U32(W,0xfc);any|=hit;
  SU32(W,0x104,U32(W,0x104)|hit);SU32(W,0x100,hit);if(!hit){SU32(W,0xc0,0);SU32(W,0xc4,0);}}
 if(!any){uint32_t old=U32(S,0x99a8);SU32(S,0x99a8,1);SU32(S,0x99ac,old==0);}else{SU32(S,0x99a8,0);SU32(S,0x99ac,0);}
 PHYS_CALL_SP(0,bike_body_probe_contacts(S));PHYS_CALL_SP(0,rx_atv_correct_contact_positions(S));
 PHYS_CALL_SP(4,rx_atv_update_rolling_orientation(S,dt));
 SU32(S,0x60,0);SU32(S,0x64,0);
 if(U32(S,0x5c)){
  rx_v4 q=rx_ld4(S+0x70),center=rx_ld4(S+0x80),pos=rx_ld4(S+0xf0);
  rx_v4 p=bike_rotate_inv(q,v4_sub(pos,center));p.v[0]=F32(0,0xd61670);
  rx_v4 limit=v4_splat(F32(0,0x930064)),margin=v4_splat(F32(0,0x9d3bfc));
  if(bike_all3(v4_cmplt(limit,v4_lane(p,2)))){SU32(S,0x60,1);p=v4_shuf(p,v4_sub(limit,margin),0xa4);}
  else{rx_v4 lo=v4_splat(F32(0,0x977888));if(bike_all3(v4_cmplt(v4_lane(p,2),lo))){SU32(S,0x64,1);p=v4_shuf(p,v4_add(margin,lo),0xa4);}}
  rx_v4 move=v4_sub(v4_add(bike_rotate(q,p),center),pos);
  for(int32_t i=0;i<I32(S,0xec);i++){rx_addr a=S+0xf0+0xa0*i;rx_st4(a,v4_add(move,rx_ld4(a)));}
 }
 if(!U32(S,0x2374))PHYS_CALL_SP(0,rx_atv_pin_rest_position(S));phys_security_check_cookie(U32(B,0x750)^B);rx_set_sp(sp);
}
RX_FUNCTION(0x00490c80,rx_atv_contact_dispatch)
