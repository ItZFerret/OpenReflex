/* Original ATV S slots35/2. Source reflex_001_00459400.c, disassembly0048cde0/0049c8d0.
 * SSE lane order retained via existing bike quaternion kernels. No motorcycle layout substitution. */
#include "vehicle_atv_methods.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_post.h"
#include "bike_slot13.h"
#include "phys_mech.h"
#include "world.h"
#include "world_misc.h"
#include "crt.h"

static void ranged(rx_addr a,uint32_t value,uint32_t rate){
 if(rx_bf(value)!=rx_bf(0x7f7fffff))SU32(a,0,value);
 if(rx_bf(rate)!=rx_bf(0x7f7fffff)){SU32(a,4,rate);SU32(a,8,0x3f800000);}
}
static void range_zero(rx_addr a,uint32_t rate){ranged(a,0,rate);}
void rx_sim_atv_apply_tuning(rx_addr s){
 float f0=F32(s,0x2ef0);
 const uint32_t src[]={0x3f6c,0x3fb4,0x3ffc,0x3d0c,0x3d54};
 for(unsigned i=0;i<5;i++)SF32(s,0x96d0+4*i,rx_muls(F32(s,src[i]),f0));
 float tail=rx_muls(F32(s,0x3d9c),f0);
 float f1=rx_muls(rx_muls(F32(s,0x508c),F32(s,0x4edc)),F32(s,0x2ef4));
 SF32(s,0x96e4,tail);
 const uint32_t src1[]={0x4f24,0x4f6c,0x4fb4,0x4ffc};
 for(unsigned i=0;i<4;i++)SF32(s,0x96f4+4*i,rx_muls(F32(s,src1[i]),f1));
 tail=rx_muls(F32(s,0x5044),f1);
 SF32(s,0x5a20,rx_muls(F32(s,0x35ec),F32(s,0x2ef8)));
 float rate=rx_muls(F32(s,0x39c4),F32(s,0x2efc));
 SF32(s,0x9704,tail);SF32(s,0x5a24,rate);
 range_zero(s+0x5980,rx_fb(rate));range_zero(s+0x5994,U32(s,0x5a24));
 range_zero(s+0x59a8,U32(s,0x5a20));range_zero(s+0x59bc,U32(s,0x5a20));
}
RX_FUNCTION(0x0048cde0,rx_sim_atv_apply_tuning)

static rx_v4 ld(rx_addr a){return rx_ld4(a);}
static rx_v4 rotate(rx_v4 q,rx_v4 v){return bike_quat_mul(q,bike_quat_mul(v4_xyz(v),v4_mul(q,ld(0x0093a220))));}
static void pose(rx_addr s,uint32_t node,rx_v4 offset,rx_addr p,rx_addr q){
 rx_st4(s+0xf0+0xa0*node,v4_add(rotate(ld(s+0x730),offset),ld(s+0xf0)));
 rx_st4(s+0x730+0xa0*node,ld(q));(void)p;
}
void rx_sim_atv_reset(rx_addr s,rx_addr p,rx_addr q){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x90;
 rx_set_sp(F);
 rx_addr vec=s+0x1300;
 rx_world_record_vector_erase(vec,F+0x10,U32(vec,0),U32(vec,0xc),U32(vec,0),U32(vec,0x10));
 SU32(s,0x2e98,1);phys_mech_set_velocity(s+0xd0,VEH_RT_ZERO);phys_mech_clear_forces(s+0xd0);
 const uint32_t on[]={0x94d8,0x9238,0x9628,0x9388,0x99a8};
 for(unsigned i=0;i<sizeof on/sizeof *on;i++)SU32(s,on[i],1);
 SU32(s,0xbc,0x3bf5c28f);
 const uint32_t zero[]={0xc0,0x94e0,0x9240,0x9630,0x9390,0x94a4,0x9204,0x95f4,0x9354,0x94a0,0x9200,0x95f0,0x9350,0x90e8,0x90ec,0x90f0,0x90f4,0x90,0x99a0,0x99a4};
 for(unsigned i=0;i<sizeof zero/sizeof *zero;i++)SU32(s,zero[i],0);
 crt_memset(s+0x1bb8,0,0x64);
 range_zero(s+0x5980,U32(s,0x5a24));range_zero(s+0x5994,U32(s,0x5a24));
 range_zero(s+0x59a8,U32(s,0x5a20));range_zero(s+0x59bc,U32(s,0x5a20));
 SU32(s,0x59d0,0);SU32(s,0x59d4,U32(0,0x009d3bfc));SU32(s,0x59d8,0x3f800000);
 range_zero(s+0x94,U32(s,0x33c4));SU32(s,0x5a0c,0);SU32(s,0x5a10,0x3e800000);SU32(s,0x5a14,0x3f800000);
 for(unsigned i=0;i<12;i++)ranged(U32(s,0x9a14+4*i),U32(VEH_ZERO3,4*(i%3)),0x3dcccccd);
 rx_v4 rates=ld(s+0x5730); /* MOVAPS snapshot before updating three ranged records */
 range_zero(s+0x97ac,rx_fb(rates.v[0]));range_zero(s+0x97c0,rx_fb(rates.v[1]));range_zero(s+0x97d4,rx_fb(rates.v[2]));range_zero(s+0x97e8,U32(s,0x5784));
 veh_v4(s+0x9810,VEH_RT_W1);rx_st4(s+0x9800,ld(q));rx_st4(s+0x9820,rotate(ld(s+0x9800),ld(0x00d615c0)));
 rx_st4(s+0xf0,ld(p));rx_st4(s+0x730,ld(q));rx_st4(s+0x190,ld(p));rx_st4(s+0x7d0,ld(q));
 rx_v4 rearDown=v4_splat(F32(s,0x40f4)),rearHalf=v4_splat(F32(s,0x413c)),frontHalf=v4_splat(F32(s,0x3e94));
 rx_v4 front=v4_sub(ld(s+0x3e40),ld(s+0x3640));
 pose(s,2,v4_sub(front,v4_mul(frontHalf,ld(0x00d615c0))),p,q);
 rx_v4 reflected=v4_mul(v4(F32(0,0x009cd9e8),1,1,1),ld(s+0x3e40));
 pose(s,3,v4_add(v4_sub(reflected,ld(s+0x3640)),v4_mul(frontHalf,ld(0x00d615c0))),p,q);
 rx_v4 rear=v4_sub(ld(s+0x40a0),ld(s+0x3640));
 rx_v4 down=v4_mul(rearDown,ld(0x00d61680));
 rx_v4 side=v4_mul(v4_mul(rearHalf,ld(0x00d615d0)),ld(0x00d615c0));
 pose(s,4,v4_sub(v4_sub(rear,down),side),p,q);pose(s,5,v4_add(v4_sub(rear,down),side),p,q);
 SU32(s,0x5898,0);veh_v4(s+0x58a0,VEH_RT_W1);veh_v4(s+0x58b0,VEH_RT_W1);
 SU32(s,0x96ec,0);for(unsigned i=0;i<4;i++)SU32(s,0x9680+4*i,0);
 veh_vcall(s,0x40,0,0,0,0); /* original ATVvf16: explicit boundary until its owner lands */
 phys_mech_nodes_slot6(s+0xd0);
 for(int32_t i=0;i<I32(s,0xec);i++){
  rx_addr a=s+0xf0+0xa0*(uint32_t)i;
  rx_st4(a+0x90,ld(a));rx_st4(a+0x80,ld(a));rx_st4(a+0x6d0,ld(a+0x640));rx_st4(a+0x6c0,ld(a+0x640));
 }
 SU32(s,0x5898,0);veh_v4(s+0x58a0,VEH_RT_W1);veh_v4(s+0x58b0,VEH_RT_W1);
 SU32(s,0x96e8,veh_x87_copy(U32(s,0x4e4c)));
 for(unsigned i=0;i<4;i++)veh_v4(s+0x9690+16*i,VEH_RT_ZERO);
 SU32(s,0x2f00,0);
 if(U32(s,0x2ebc)){
  rx_world_dynobj_take_contact_results(U32(s,0x2ebc),F+0x60,F+0x50,F+0x70);
  rx_world_dynobj_take_contact_results(U32(s,0x2ebc),F+0x60,F+0x50,F+0x70);
 }
 bike_body_probe_contacts(s);
 const uint32_t tailzero[]={0x14,0x18,0x9950,0x9954,0x9958,0x9960,0x9968,0x996c,0x9970,0x997c,0x68,0x995c,0x9964,0x9974,0x9978,0x9980,0x9984,0x9988};
 for(unsigned i=0;i<sizeof tailzero/sizeof *tailzero;i++)SU32(s,tailzero[i],0);
 SU32(s,0xac,1);rx_set_sp(sp);
}
RX_FUNCTION(0x0049c8d0,rx_sim_atv_reset)

/*004997f0: derive all four suspension constraint frames from the six-body pose.
 * Front arms intersect a radius circle; rear arms normalize in the YZ plane and
 * clamp their backward-pointing sector before normalizing again. No external calls. */
static void constraint_axes(rx_addr a,rx_addr orientation){
 rx_st4(a+0x10,v4_sub(v4(0,0,0,0),ld(0x00d616a0)));
 rx_st4(a+0x20,v4_sub(v4(0,0,0,0),ld(0x00d615c0)));
 rx_st4(a+0x30,v4_sub(v4(0,0,0,0),ld(0x00d61680)));
 rx_st4(a+0x40,ld(orientation));
}
void rx_sim_atv_update_frames(rx_addr s){
 rx_v4 rearLength=v4_splat(F32(s,0x40f4)),rearHalf=v4_splat(F32(s,0x413c));
 rx_v4 pivotFront=v4_sub(v4_splat(F32(s,0x3cc4)),v4_lane(ld(s+0x3640),1));
 rx_v4 q=ld(s+0x730);
 rx_v4 pivotRear=v4_sub(v4_splat(F32(s,0x3f24)),v4_lane(ld(s+0x3640),1));
 rx_v4 half2=v4_splat(rx_muls(F32(s,0x3e94),F32(s,0x3e94)));
 rx_v4 offsetSteer=v4(F32(s,0x41cc),0,0,0);
 for(unsigned i=0;i<2;i++){
  rx_v4 model=ld(s+0x3e40);
  if(i)model=v4_mul(model,v4(F32(0,0x009cd9e8),1,1,1));
  rx_v4 offset=v4_sub(model,ld(s+0x3640));
  rx_v4 delta=bike_rotate_inv(q,v4_sub(ld(s+0x230+0xa0*i),ld(s+0xf0)));
  rx_v4 dy=v4_sub(v4_lane(delta,1),v4_lane(offset,1)),dy2=v4_mul(dy,dy);
  rx_v4 reach=bike_all3(v4_cmplt(half2,dy2))?v4_splat(rx_bf(0x3dcccccd)):v4_sqrt(v4_sub(half2,dy2));
  rx_v4 x=i?v4_add(reach,v4_lane(offset,0)):v4_sub(v4_lane(offset,0),reach);
  rx_v4 anchor=v4_shuf(pivotFront,v4_lane(offset,2),0xa5);anchor.v[0]=x.v[0];
  rx_addr steer=s+(i?0x58a0:0x58b0),out=s+(i?0x7530:0x6bd0);
  rx_st4(out,anchor); /* original first writes the unsteered frame */
  rx_v4 adjustment=rotate(ld(steer),offsetSteer);
  rx_st4(out,i?v4_add(adjustment,ld(out)):v4_sub(ld(out),adjustment));
  constraint_axes(out,steer);
 }
 for(unsigned i=0;i<2;i++){
  rx_v4 side=v4_mul(v4_mul(rearHalf,ld(0x00d615c0)),ld(0x00d615d0));
  rx_v4 base=v4_sub(ld(s+0x40a0),ld(s+0x3640));
  rx_v4 offset=i?v4_add(side,base):v4_sub(base,side);
  rx_v4 delta=bike_rotate_inv(q,v4_sub(ld(s+0x370+0xa0*i),ld(s+0xf0)));
  rx_v4 arm=v4_sub(delta,offset);arm.v[0]=F32(0,0x00d61670);
  rx_v4 n=bike_normalize(arm);
  if(bike_all3(v4_cmplt(v4_splat(rx_bf(0xbf333333)),v4_lane(n,2)))){
   float y=bike_all3(v4_cmplt(ld(0x00d61670),v4_lane(n,1)))?rx_bf(0xbe19999a):rx_bf(0x3e19999a);
   arm=v4(y,y,F32(0,0x009cd9e8),F32(0,0x009cd9e8));arm.v[0]=0;
  }
  rx_v4 point=v4_add(v4_mul(bike_normalize(arm),rearLength),offset);
  rx_v4 anchor=v4_shuf(pivotRear,point,0xe5);anchor.v[0]=point.v[0];
  rx_addr out=s+(i?0x87f0:0x7e90);rx_st4(out,anchor);constraint_axes(out,VEH_RT_W1);
 }
}
RX_FUNCTION(0x004997f0,rx_sim_atv_update_frames)

/*004a1a10: thirteen body probes and per-probe displacement baselines. */
void rx_sim_atv_update_probes(rx_addr s){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x140;
 rx_set_sp(F);veh_cookie(F+8,0x134);
 float rearZ=F32(0,0x009cc8a8),lowZ=F32(0,0x009d789c);
 const rx_v4 fixed[]={
  v4(-1.5f,0,2,2),v4(-1.5f,1,-2.5f,-2.5f),v4(1.5f,0,2,2),v4(1.5f,1,-2.5f,-2.5f),
  v4(rx_bf(0xbf99999a),rx_bf(0x3fb33333),rearZ,rearZ),
  v4(rx_bf(0x3f99999a),rx_bf(0x3fb33333),rearZ,rearZ),
  v4(0,F32(0,0x009cd9e8),lowZ,lowZ),v4(0,rx_bf(0xbf99999a),0,0)};
 for(unsigned i=0;i<8;i++)rx_st4(s+0x14b0+16*i,fixed[i]);
 const rx_v4 q=ld(s+0x730),pos=ld(s+0xf0),offset=v4(rx_bf(0x3e99999a),0,0,0);
 /* Original adds rear-right then rear-left before halving, retaining all lanes. */
 rx_v4 midpoint=v4_mul(v4_add(ld(s+0x410),ld(s+0x370)),ld(0x00d615d0));
 for(unsigned i=0;i<4;i++){
  rx_v4 local=bike_rotate_inv(q,v4_sub(ld(s+0x230+0xa0*i),pos));
  rx_st4(s+0x1530+16*i,(i&1)?v4_add(local,offset):v4_sub(local,offset));
 }
 rx_st4(s+0x1570,bike_rotate_inv(q,v4_sub(midpoint,pos)));
 rx_v4 speed=v4_sqrt(v4_splat(bike_hsum_021(v4_mul(ld(s+0x100),ld(s+0x100)))));
 rx_v4 up=v4(0,1.5f,0,0),ten=v4_splat(10);
 rx_v4 lift=bike_all3(v4_cmplt(ten,speed))?up:v4_div(v4_mul(speed,up),ten);
 const uint32_t raised[]={0x1510,0x1520,0x1570};
 for(unsigned i=0;i<3;i++)rx_st4(s+raised[i],v4_add(lift,ld(s+raised[i])));
 for(unsigned i=0;i<8;i++)rx_st4(F+0x60+16*i,ld(s+0x14b0+16*i));
 for(unsigned i=0;i<4;i++){
  rx_v4 baseline=v4_sub(v4(0,0,0,0),bike_rotate_inv(q,ld(0x00d616a0)));
  rx_st4(F+0xe0+16*i,baseline);
 }
 rx_st4(F+0x120,ld(s+0x1570));
 for(int32_t i=0;i<I32(s,0x1318);i++){
  uint32_t u=(uint32_t)i;rx_v4 p=ld(s+0x14b0+16*u);
  rx_st4(s+0x1c20+32*u,v4_sub(p,ld(F+0x60+16*u)));
  rx_st4(s+0x1c30+32*u,ld(s+0x14b0+16*u));
 }
 rx_set_sp(sp);
}
RX_FUNCTION(0x004a1a10,rx_sim_atv_update_probes)
