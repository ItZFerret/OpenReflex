/* TrophyTruck poststep/debug pass, hand transliterated from 00459fa0 and
 * 00473450. All retained effects are component vf4 calls and S+8020 reset.
 * The intervening debug draw calculations only write dead stack (the original
 * contains no draw calls); retain their point evaluation and scratch frame. */
#include "vehicle_truck_poststep.h"
#include "vehicle_poststep_helpers.h"
#include "world_veh_util.h"

static rx_v4 debug_inner(rx_v4 q,rx_v4 point){
 return veh_quat_mul(v4_xyz(point),v4_mul(q,v4_ld(0x0093a220)));
}
static rx_v4 debug_outer(rx_v4 q,rx_v4 inner,rx_v4 position){return v4_add(veh_quat_mul(q,inner),position);}
static void truck_poststep_body(rx_addr s){
 world_frame f=world_frame_enter(0x14c,1,1);rx_addr S=f.esp;
 WORLD_CALL_SP(4,rx_vehicle_mechanism_node_poststep(s+0xd0));
 for(unsigned i=0;i<4;i++)WORLD_CALL_SP(4,rx_vehicle_wheel_axis_debug(s,s+0x8080+0x150*i));
 const rx_v4 mirror=v4(-1,1,1,1),p=v4_ld(s+0x180),q=v4_ld(s+0x7c0),centre=v4_ld(s+0x36e0);
 v4_st(S+0x140,mirror);v4_st(S+0x10,p);
 for(unsigned i=0;i<10;i++)v4_st(S+0x70+16*i,v4_ld(s+0x8810+16*i));
 for(unsigned i=0;i<8;i++)v4_st(S+0x70+16*i,v4_ld(s+0x88b0+16*i));
 rx_v4 front=v4_ld(s+0x3f90),rear=v4_ld(s+0x41f0);
 v4_st(S+0x60,centre);v4_st(S+0x120,front);
 v4_st(S+0x50,v4_lane(q,0));v4_st(S+0x20,v4_lane(q,1));
 v4_st(S+0x30,v4_lane(q,3));v4_st(S+0x40,v4_lane(q,2));
 const rx_v4 anchors[]={front,v4_mul(front,mirror),rear,v4_mul(rear,mirror)};
 const uint32_t bodies[]={0x220,0x2c0,0x360,0x400};
 for(unsigned i=0;i<4;i++){
  rx_v4 local=v4_xyz(v4_sub(anchors[i],centre)),inner=debug_inner(q,local);
  /* Original displays the anchor and corresponding simulation body, storing
   * both in the same scratch vector; the body store wins. */
  v4_st(S+0x130,debug_outer(q,inner,p));v4_st(S+0x130,v4_ld(s+bodies[i]));
  if(i==1)v4_st(S+0x120,v4_mul(v4_lane(q,0),v4_perm(inner,3,2,1,0)));
  if(i==2)v4_st(S+0x110,rear);
  if(i==3)v4_st(S+0x110,v4_mul(v4_lane(local,0),v4_perm(v4_mul(q,v4_ld(0x0093a220)),3,2,1,0)));
 }
 /* Two optional indicator points have no drawing callback in this binary.
  * Their final scratch value is the body quaternion, after the evaluated point. */
 if(U32(s,0x90)){
  rx_v4 inner=debug_inner(q,v4(0,F32(0,0x009322c0),0,0));
  v4_st(S+0x120,v4_mul(v4_lane(v4(0,F32(0,0x009322c0),0,0),0),v4_perm(v4_mul(q,v4_ld(0x0093a220)),3,2,1,0)));
  v4_st(S+0x110,debug_outer(q,inner,p));v4_st(S+0x110,q);
 }
 if(U32(s,0x8020)){
  rx_v4 indicator=v4(0,F32(0,0x0092fe48),0,0),inner=debug_inner(q,indicator);
  v4_st(S+0x120,v4_mul(v4_lane(indicator,0),v4_perm(v4_mul(q,v4_ld(0x0093a220)),3,2,1,0)));
  v4_st(S+0x110,debug_outer(q,inner,p));v4_st(S+0x110,q);
 }
 SU32(s,0x8020,0);
 int32_t count=I32(s,0x1318);
 if(count>0){
  rx_v4 conjugate=v4_mul(q,v4_ld(0x0093a220));v4_st(S+0x110,v4_perm(conjugate,3,2,1,0));
  for(int32_t i=0;i<count;i++){
   v4_st(S+0x130,v4_ld(s+0x1320+16u*(uint32_t)i));
   rx_v4 a=debug_inner(q,v4_ld(s+0x1c20+32u*(uint32_t)i));
   rx_v4 b=debug_inner(q,v4_ld(s+0x1c30+32u*(uint32_t)i));
   v4_st(S+0x120,v4_perm(a,2,3,0,1));v4_st(S+0x140,v4_perm(b,2,3,0,1));
   v4_st(S+0x130,debug_outer(q,a,p));v4_st(S+0x130,debug_outer(q,b,p));
  }
 }
 world_frame_leave(&f);
}
/* The otherwise-discarded pointer registers are inherited by the vf61 tail
 * call. EDX is set to S before the wheel helpers and remains unchanged; EAX is
 * the last wheel argument unless the contact-pointer loop advances it. */
uint32_t rx_sim_truck_poststep_regs(rx_addr s,uint32_t *edx_out){
 truck_poststep_body(s);
 int32_t count=I32(s,0x1318);
 if(edx_out)*edx_out=s;
 return count>0?s+0x1c30+0x20u*(uint32_t)count:s+0x8470;
}
uint32_t rx_sim_truck_poststep(rx_addr s){return rx_sim_truck_poststep_regs(s,0);}
RX_FUNCTION(0x00459fa0,rx_sim_truck_poststep)
uint32_t rx_truck_poststep_regs(rx_addr v,uint32_t *edx_out){
 const int32_t counter=I32(0,0x00ad3ed4),remainder=counter%3;
 if(remainder==1||remainder==2)return rx_sim_truck_poststep_regs(U32(v,0x15d0),edx_out);
 if(edx_out)*edx_out=(uint32_t)counter-(uint32_t)remainder;
 return (uint32_t)remainder;
}
uint32_t rx_truck_poststep(rx_addr v){return rx_truck_poststep_regs(v,0);}
RX_FUNCTION(0x00473450,rx_truck_poststep)
