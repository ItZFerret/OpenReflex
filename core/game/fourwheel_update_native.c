/* Finite native semantic composition. No registry lookup or original fallback.
 * Candidate only: canonical child semantic owners are reused once. Incidental
 * volatile/deep-private CRT effects require the subsequent composed proof. */
#include "fourwheel_vehicle_update.h"
#include "vehicle_saved_pose.h"
#include "fourwheel_wheel_effects.h"
#include "buggy_engine_phase.h"
#include "bike_sim.h"
#include "bike_vehicle.h"
#include "vehicle_utv_runtime.h"
#include "vehicle_fourwheel_leaves.h"
#include "vehicle_utv_pose.h"
#include "vehicle_buggy_runtime.h"
#include "vehicle_buggy_pose.h"
#include "vehicle_truck_pose.h"
#include "anim.h"
#include "world_node.h"
#include "world_rider.h"
#include "world_veh.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
const uint32_t fixedTargets[22]={0x434d00,0x460070,0x433120,0x433550,0,0,0x42c230,0x45d910,0x7de9e0,0x7db890,0x6243d0,0,0,0,0x4ad580,0x42c230,0x42c230,0x42c230,0x42c230,0x4617b0,0x4618e0,0x8f9efe};
const unsigned cleanups[22]={0,0,4,4,4,4,0,4,4,4,0,4,4,8,4,0,0,0,0,0,4,0};
const uint32_t returnSites[3][22]={
 {0x4c164a,0x4c1667,0x4c167f,0x4c1691,0x4c1787,0x4c179b,0x4c1802,0x4c1827,0x4c1d43,0x4c1d4e,0x4c1d9c,0x4c1dd1,0x4c1ded,0x4c1dfa,0,0x4c1e6a,0x4c1ea9,0x4c1ee8,0x4c1f27,0x4c1f74,0x4c1f81,0x4c1f92},
 {0x4a390a,0x4a3927,0x4a393f,0x4a3951,0x4a3a47,0x4a3a5b,0x4a3ac2,0x4a3ae7,0x4a4003,0x4a400e,0x4a405c,0x4a4091,0x4a40ad,0x4a40ba,0x4a40c8,0x4a4138,0x4a4177,0x4a41b6,0x4a41f5,0x4a4240,0x4a424d,0x4a425e},
 {0x46b6fa,0x46b717,0x46b72f,0x46b741,0x46b837,0x46b84b,0x46b8b2,0x46b8d7,0x46bdf3,0x46bdfe,0x46be4c,0x46be81,0x46be9d,0x46beaa,0,0x46bf1a,0x46bf59,0x46bf98,0x46bfd7,0x46c03b,0x46c048,0x46c059}
};

static uint32_t virtual_target(unsigned family,unsigned site){
 static const uint32_t targets[3][4]={
  {0x4c6ee0,0x4c6f00,0x4c1fa0,0x4c6780},
  {0x4ac580,0x4ac5a0,0x4a4270,0x4abe20},
  {0x4734b0,0x4734d0,0x46c090,0x472d50}};
 unsigned index=site==4?0:site==5?1:site==11?2:3;
 return targets[family][index];
}
static void retained(rx_addr entry,uint32_t ra,fx_saved old){
#ifdef RX_ORACLE
 /* Oracle build (RX_ORACLE_FOURWHEEL): children the product ports but the oracle build leaves out (#ifndef RX_ORACLE files) run as
  * originals through rx_call_original, which hands them the core's register model, not the caller's; the x86 callee restores the
  * callee-saved registers it was given, so the caller's are restored here (the product keeps the strict check below). */
 if(rx_sp()==entry&&U32(entry,0)==ra){fx_set_reg(FX_EBX,old.ebx);fx_set_reg(FX_EBP,old.ebp);fx_set_reg(FX_ESI,old.esi);fx_set_reg(FX_EDI,old.edi);return;}
#endif
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=old.ebx||fx_reg(FX_EBP)!=old.ebp||fx_reg(FX_ESI)!=old.esi||fx_reg(FX_EDI)!=old.edi){
  fprintf(stderr,"rxcore: four-wheel update dependency not retained: caller%08x sp%08x/%08x ra%08x EBX%08x/%08x EBP%08x/%08x ESI%08x/%08x EDI%08x/%08x\n",
          ra,rx_sp(),entry,U32(entry,0),fx_reg(FX_EBX),old.ebx,fx_reg(FX_EBP),old.ebp,fx_reg(FX_ESI),old.esi,fx_reg(FX_EDI),old.edi);abort();
 }
}
static void bad(unsigned family,unsigned site,rx_addr target,uint32_t ra){
 fprintf(stderr,"rxcore: four-wheel update dependency family%u site%u target%08x caller%08x ECX%08x EDX%08x\n",family,site,target,ra,fx_reg(FX_ECX),fx_reg(FX_EDX));abort();
}
rx_v4 rx_fourwheel_update_dependency(unsigned family,unsigned site,rx_addr target,rx_addr expected,rx_v4 x0){
 if(family>2||site>=22||(site==14&&family!=1))bad(family,site,target,0);
 const rx_addr entry=rx_sp();const uint32_t ra=U32(entry,0);const fx_saved old=fx_save();
 rx_addr required=fixedTargets[site];
 if(site==4||site==5||site==11||site==12){
  const unsigned offset=site==4?0x90:site==5?0x94:site==11?0xf0:0x8c;
  required=virtual_target(family,site);
  if(U32(U32(fx_reg(FX_ECX),0),offset)!=required||fx_reg(FX_EDX)!=required)bad(family,site,target,ra);
 }
 if(site==13){const uint32_t wheelTargets[3]={0x4c7ed0,0x4ad6b0,0x4744f0};required=wheelTargets[family];}
 if(!required||required!=target||ra!=returnSites[family][site]||expected!=entry+4+cleanups[site])bad(family,site,target,ra);
 switch(site){
 case 0:fx_set_reg(FX_EAX,bike_state_divergence_check(fx_reg(FX_ESI)));break;
 case 1:rx_vehicle_restore_saved_pose(fx_reg(FX_EAX));break;
 case 2:bike_get_replay_state(fx_reg(FX_ECX),U32(entry,4));break;
 case 3:bike_replay_record_encode(fx_reg(FX_ESI),U32(entry,4));break;
 case 4:case 5:{
  const rx_addr v=fx_reg(FX_ECX),out=U32(entry,4);
  if(family==0){if(site==4)rx_utv_rotation(v,out);else rx_utv_velocity(v,out);}
  else if(family==1){if(site==4)rx_buggy_rotation(v,out);else rx_buggy_velocity(v,out);}
  else{if(site==4)rx_truck_rotation(v,out);else rx_truck_velocity(v,out);}
  fx_set_reg(FX_EAX,out);break;
 }
 case 6:case 15:case 16:case 17:case 18:x0.v[0]=phys_sqrtf(rx_bf(U32(entry,4)));break;
 case 7:bike_vehicle_debug_view(fx_reg(FX_EDI),rx_bf(U32(entry,4)));break;
 case 8:fx_set_reg(FX_EAX,rx_hierarchy_find_node(fx_reg(FX_EDI),U32(entry,4)));break;
 case 9:rx_world_node_evaluate_world(fx_reg(FX_ECX),U32(entry,4));break;
 case 10:rx_world_rider_root_matrix(fx_reg(FX_ECX),fx_reg(FX_EAX));break;
 case 11:{
  const rx_addr v=fx_reg(FX_ECX);const float dt=rx_bf(U32(entry,4));
  if(family==0)rx_utv_update_model_pose_regs(v,old.ebx,old.edi);
  else if(family==1)rx_buggy_update_model_pose(v,dt);
  else rx_truck_update_model_pose_at(v,dt,old.ebx);break;
 }
 case 12:{
  const rx_addr v=fx_reg(FX_ECX);const float dt=rx_bf(U32(entry,4));
  if(family==0)rx_utv_publish_rider(v,dt);
  else if(family==1)rx_buggy_publish_rider(v,dt);
  else rx_truck_publish_rider(v,dt);break;
 }
 case 13:{
  const rx_addr v=U32(entry,4);const float dt=rx_bf(U32(entry,8));uint32_t cookie;
  if(family==0)cookie=rx_fourwheel_wheel_effects_utv(v,dt);
  else if(family==1)cookie=rx_fourwheel_wheel_effects_buggy(v,dt);
  else cookie=rx_fourwheel_wheel_effects_truck(v,dt);
  fx_set_reg(FX_ECX,cookie);break;
 }
 case 14:rx_buggy_engine_phase(fx_reg(FX_ESI),rx_bf(U32(entry,4)));break;
 case 19:rx_world_landing_rumble(fx_reg(FX_ESI),x0.v[0]);break;
 case 20:rx_world_vehicle_camera_update(fx_reg(FX_ESI),rx_bf(U32(entry,4)));break;
 case 21:phys_security_check_cookie(fx_reg(FX_ECX));break;
 default:abort();
 }
 retained(entry,ra,old);rx_set_sp(expected);return x0;
}
