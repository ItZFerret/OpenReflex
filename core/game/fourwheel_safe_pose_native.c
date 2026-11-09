/* Finite typed safe-pose dependencies. Reuse the existing canonical leaf,
 * lookup observer and material sampler once; no generic/original fallback. */
#include "fourwheel_vehicle_runtime_leaves.h"
#include "vehicle_utv_runtime.h"
#include "vehicle_fourwheel_leaves.h"
#include "query_private_dependencies.h"
#include "phys_terrain.h"
#include "fx_util.h"
#include <stdlib.h>
void rx_fourwheel_safe_pose_dependency(unsigned family,unsigned role,rx_addr target,rx_addr expected){
 static const rx_addr entries[3]={0x4c6a60,0x4ac100,0x473030};
 static const rx_addr targets[3][4]={{0x4c6fe0,0x4c0360,0x402dc0,0x7c5120},{0x4ac680,0x4a2520,0x402dc0,0x7c5120},{0x4735b0,0x46a3b0,0x402dc0,0x7c5120}};
 static const unsigned offsets[4]={0x18,0x2c,0xab,0xbc};
 if(family>2||role>3)abort();const rx_addr entry=rx_sp();const uint32_t ra=U32(entry,0);const fx_saved old=fx_save();
 if(target!=targets[family][role]||ra!=entries[family]+offsets[role]||expected!=entry+4)abort();
 if(role<2){
  rx_addr v=fx_reg(FX_ECX);if(U32(U32(v,0),role?0xb8:0xb4)!=target||fx_reg(FX_EDX)!=target)abort();uint32_t result;
  if(role==0)result=family==0?rx_utv_all_wheels_contact(v):family==1?rx_buggy_all_wheels_contact(v):rx_truck_all_wheels_contact(v);
  else result=family==0?rx_utv_vehicle_has_nodes(v):family==1?rx_buggy_vehicle_has_nodes(v):rx_truck_vehicle_has_nodes(v);
  fx_set_reg(FX_EAX,result);
 }else if(role==2){if(fx_reg(FX_EDX)!=0x9e30d4)abort();(void)rx_query_lookup_frame(fx_reg(FX_EDX));}
 else{
  if(fx_reg(FX_EAX)!=0)abort();rx_regs in=fx_regs(),out;
  (void)phys_terrain_material_at(fx_reg(FX_EDI),fx_reg(FX_EDX),0,&in,&out);
  fx_set_reg(FX_EAX,out.r[0]);fx_set_reg(FX_ECX,out.r[1]);fx_set_reg(FX_EDX,out.r[2]);
 }
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=old.ebx||fx_reg(FX_EBP)!=old.ebp||fx_reg(FX_ESI)!=old.esi||fx_reg(FX_EDI)!=old.edi)abort();
 rx_set_sp(expected);
}
