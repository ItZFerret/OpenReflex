/* Original UTV/Buggy/TrophyTruck S slots35/2. Shared code below expresses only the
 * identical instruction sequences; class deltas are the original literal offsets.
 * Truck/Buggy rear suspension uses the vertical axis; UTV uses the lateral axis. */
#include "vehicle_fourwheel_methods.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "bike_slot13.h"
#include "phys_mech.h"
#include "world.h"
#include "world_misc.h"
#include "crt.h"
static void range0(rx_addr a,uint32_t rate){
 SU32(a,0,0);
 if(rx_bf(rate)!=rx_bf(0x7f7fffff)){SU32(a,4,rate);SU32(a,8,0x3f800000);}
}
static void tuning(rx_addr s,uint32_t d,uint32_t engine){
 float scale=F32(s,0x2ef0);
 const uint32_t src[]={0x40bc,0x4104,0x414c,0x3ea4,0x3eec};
 for(unsigned i=0;i<5;i++)SF32(s,0x8640-d+4*i,rx_muls(F32(s,src[i]),scale));
 float tail=rx_muls(F32(s,0x3f34),scale);
 float e=rx_muls(rx_muls(F32(s,0x5224),F32(s,0x5074)),F32(s,0x2ef4));
 SF32(s,0x8654-d,tail);
 const uint32_t esrc[]={0x50bc,0x5104,0x514c,0x5194};
 for(unsigned i=0;i<4;i++)SF32(s,engine+4*i,rx_muls(F32(s,esrc[i]),e));
 tail=rx_muls(F32(s,0x51dc),e);
 SF32(s,0x5778-d,rx_muls(F32(s,0x368c),F32(s,0x2ef8)));
 float rate=rx_muls(F32(s,0x3b14),F32(s,0x2efc));
 SF32(s,engine+16,tail);SF32(s,0x577c-d,rate);
 range0(s+0x5700-d,rx_fb(rate));range0(s+0x5714-d,U32(s,0x577c-d));
 range0(s+0x5728-d,U32(s,0x5778-d));range0(s+0x573c-d,U32(s,0x5778-d));
}
void rx_sim_utv_apply_tuning(rx_addr s){tuning(s,0,0x8664);} RX_FUNCTION(0x004cc610,rx_sim_utv_apply_tuning)
void rx_sim_buggy_apply_tuning(rx_addr s){tuning(s,0x10,0x8654);} RX_FUNCTION(0x004b1e10,rx_sim_buggy_apply_tuning)
void rx_sim_truck_apply_tuning(rx_addr s){tuning(s,0,0x8668);} RX_FUNCTION(0x00450ab0,rx_sim_truck_apply_tuning)
static rx_v4 rotate(rx_v4 q,rx_v4 v){return bike_quat_mul(q,bike_quat_mul(v4_xyz(v),v4_mul(q,rx_ld4(0x0093a220))));}
static void reset(rx_addr s,rx_addr p,rx_addr qp,uint32_t d,int truck){
 const rx_addr sp=rx_sp(),A=(sp-4)&~15u,F=A-0x70;
 rx_set_sp(F);veh_cookie(A-0x64,0x60);
 rx_addr v=s+0x1300;
 rx_world_record_vector_erase(v,F+0x10,U32(v,0),U32(v,0xc),U32(v,0),U32(v,0x10));
 SU32(s,0x2e98,1);phys_mech_set_velocity(s+0xd0,VEH_RT_ZERO);phys_mech_clear_forces(s+0xd0);
 const uint32_t on[]={0x8418,0x8178,0x8568,0x82c8};
 for(unsigned i=0;i<4;i++)SU32(s,on[i]-d,1);
 SU32(s,0xbc,0x3bf5c28f);SU32(s,0xc0,0);
 const uint32_t zero[]={0x8420,0x8180,0x8570,0x82d0,0x83e4,0x8144,0x8534,0x8294,0x83e0,0x8140,0x8530,0x8290,0x8028,0x802c,0x8030,0x8034};
 for(unsigned i=0;i<16;i++)SU32(s,zero[i]-d,0);
 SU32(s,0x90,0);SU32(s,0x8700-d,1);crt_memset(s+0x1bb8,0,100);
 range0(s+0x5700-d,U32(s,0x577c-d));range0(s+0x5714-d,U32(s,0x577c-d));
 range0(s+0x5728-d,U32(s,0x5778-d));range0(s+0x573c-d,U32(s,0x5778-d));
 SU32(s,0x5750-d,0);SU32(s,0x5754-d,U32(0,0x009d3bfc));SU32(s,0x5758-d,0x3f800000);range0(s+0x94,U32(s,0x33c4));range0(s+0x5764-d,0x3e19999a);
 for(unsigned i=0;i<12;i++){
  rx_addr r=U32(s,0x85d0-d+4*i);uint32_t value=U32(VEH_ZERO3,4*(i%3));
  if(rx_bf(value)!=rx_bf(0x7f7fffff))SU32(r,0,value);
  if(i==11){SU32(r,8,0x3f800000);SU32(r,4,0x3dcccccd);}
  else{SU32(r,4,0x3dcccccd);SU32(r,8,0x3f800000);}
 }
 rx_st4(s+0xf0,rx_ld4(p));rx_st4(s+0x730,rx_ld4(qp));
 const rx_v4 q=rx_ld4(s+0x730);
 const rx_v4 rearLen=v4_splat(F32(s,0x4244)),frontLen=v4_splat(F32(s,0x3fe4));
 for(unsigned i=0;i<4;i++){
  rx_v4 local=rx_ld4(s+(i<2?0x3f90:0x41f0));
  if(i&1)local=v4_mul(v4(-1,1,1,1),local);
  local=v4_sub(local,rx_ld4(s+0x36e0));
  rx_v4 step=v4_mul(i<2?frontLen:rearLen,rx_ld4(i>=2&&truck?0x00d61680:0x00d615c0));
  local=(i&1)&&!(i>=2&&truck)?v4_add(local,step):v4_sub(local,step);
  rx_st4(s+0x190+0xa0*i,v4_add(rotate(q,local),rx_ld4(s+0xf0)));
  rx_st4(s+0x7d0+0xa0*i,rx_ld4(qp));
 }
 SU32(s,0x5618-d,0);veh_v4(s+0x5620-d,VEH_RT_W1);veh_v4(s+0x5630-d,VEH_RT_W1);
 SU32(s,0x865c-d,0);for(unsigned i=0;i<4;i++)SU32(s,0x85c0-d+4*i,0);
 veh_vcall(s,0x40,0,0,0,0);phys_mech_nodes_slot6(s+0xd0);
 for(int32_t i=0;i<I32(s,0xec);i++){
  rx_addr a=s+0xf0+0xa0*(uint32_t)i;
  rx_st4(a+0x90,rx_ld4(a));rx_st4(a+0x80,rx_ld4(a));
  rx_st4(a+0x6d0,rx_ld4(a+0x640));rx_st4(a+0x6c0,rx_ld4(a+0x640));
 }
 SU32(s,0x8658-d,veh_x87_copy(U32(s,0x4f9c)));
 for(unsigned i=0;i<4;i++)veh_v4(s+0x8600-d+16*i,VEH_RT_ZERO);
 SU32(s,0x2f00,0);
 if(U32(s,0x2ebc)){
  rx_world_dynobj_take_contact_results(U32(s,0x2ebc),F+0x40,F+0x50,F+0x30);
  rx_world_dynobj_take_contact_results(U32(s,0x2ebc),F+0x40,F+0x50,F+0x30);
 }
 bike_body_probe_contacts(s);SU32(s,0x14,0);SU32(s,0x18,0);
 veh_v4(s+0x86f0-d,s+0x730);SU32(s,0xac,1);rx_set_sp(sp);
}
void rx_sim_utv_reset(rx_addr s,rx_addr p,rx_addr q){reset(s,p,q,0,0);} RX_FUNCTION(0x004d75b0,rx_sim_utv_reset)
void rx_sim_buggy_reset(rx_addr s,rx_addr p,rx_addr q){reset(s,p,q,0x10,1);} RX_FUNCTION(0x004bce80,rx_sim_buggy_reset)
void rx_sim_truck_reset(rx_addr s,rx_addr p,rx_addr q){reset(s,p,q,0,1);} RX_FUNCTION(0x00459400,rx_sim_truck_reset)
