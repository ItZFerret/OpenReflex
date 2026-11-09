/* Complete isolated native role closure. Canonical physics/world semantics
 * called exactly once; no copied quaternion or terrain implementation. */
#include "query_private_dependencies.h"
#include "fourwheel_terrain_query.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_FOURWHEEL_QUERY_TYPED_BRIDGE)
#include "fx_util.h"
#include "world_veh.h"
#include "phys_quat.h"
#include "phys_crt.h"
#include "terrain_triangles_exact.h"
#include "wheel_triangle_observed.h"
#include <stdio.h>
#include <stdlib.h>
rx_addr rx_query_lookup_frame(rx_addr registry){
 const rx_addr entry=rx_sp();const uint32_t n=U32(registry,8);
 /* Original PUSH EBX, PUSH ESI, PUSH EDI; no EBP/alignment/locals/callees. */
 SU32(entry-4,0,fx_reg(FX_EBX));SU32(entry-8,0,fx_reg(FX_ESI));SU32(entry-12,0,fx_reg(FX_EDI));
 rx_addr result=rx_world_find_by_controller(registry); /* Unique existing owner. */
 /* Every nonempty scan loads ECX from its last visited slot. Success's last
  * visited slot is result; an exhausted scan's is the original final slot.
  * Empty list does not assign ECX. EBX/ESI/EDI and EDX are restored/unchanged. */
 if(n)fx_set_reg(FX_ECX,result?result:U32(registry,4u*(n-1u)));
 fx_set_reg(FX_EAX,result);if(rx_sp()!=entry)abort();return result;
}
void rx_query_probe_setup_frame(rx_addr base,rx_addr up,rx_addr probe,rx_addr target,rx_addr q,rx_addr rootq){
 const rx_addr entry=rx_sp();
 /* Original PUSH EBP/MOV EBP,ESP/AND ESP,-16, but no local stack stores or
  * nested calls. Only saved EBP survives. SIMD arithmetic belongs to owner. */
 SU32(entry-4,0,fx_reg(FX_EBP));
 phys_probe_frame_setup(base,up,probe,target,q,rootq); /* Exactly once. */
 /* 42db26 loads probe;42db81 loads target;42dcc4 loads final root quaternion
  * pointer into ECX. Main EAX|EDX-only adapter did not observe this ECX. */
 fx_set_reg(FX_EAX,probe);fx_set_reg(FX_EDX,target);fx_set_reg(FX_ECX,rootq);
 if(rx_sp()!=entry)abort();
}
static int site(rx_addr ra,unsigned offset){
 return ra==0x4d3140u+offset||ra==0x4b8990u+offset||ra==0x454ff0u+offset;
}
#ifdef RX_QUERY_COMPLETE_DEPENDENCY_TEST
static _Thread_local unsigned calls[5];
unsigned rx_query_complete_native_calls(unsigned role){if(role>=5)abort();return calls[role];}
#define COUNT(role) (++calls[role])
#else
#define COUNT(role) ((void)0)
#endif
void rx_fourwheel_query_dependency(rx_addr target,rx_addr expected){
 const rx_addr entry=rx_sp();const uint32_t ra=U32(entry,0);uint32_t cleanup=4;
 if(target==0x402dc0){
  if(!site(ra,0x190)||expected!=entry+4)abort();
  COUNT(0);
  (void)rx_query_lookup_frame(fx_reg(FX_EDX));
 }else if(target==0x7c6160){
  cleanup=24;if(!site(ra,0x19a)||expected!=entry+cleanup)abort();
  COUNT(1);
  phys_terrain_triangles_exact(fx_reg(FX_ECX),U32(entry,4),U32(entry,8),U32(entry,12),(int32_t)U32(entry,16),U32(entry,20));
 }else if(target==0x438260){
  cleanup=24;if(!site(ra,0x1d7)||expected!=entry+cleanup)abort();
  COUNT(2);
  (void)bike_wheel_triangle_contact_observed(fx_reg(FX_ECX),fx_reg(FX_EDI),U32(entry,4),U32(entry,8),U32(entry,12),U32(entry,16),(int32_t)U32(entry,20),NULL);
 }else if(target==0x42db20){
  cleanup=20;if(!site(ra,0x1fc)||expected!=entry+cleanup)abort();
  COUNT(3);
  rx_query_probe_setup_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),U32(entry,4),U32(entry,8),U32(entry,12),U32(entry,16));
 }else if(target==0x8f9efe){
  if((!site(ra,0x1b3)&&!site(ra,0x209))||expected!=entry+4)abort();
  COUNT(4);phys_security_check_cookie(fx_reg(FX_ECX));
 }else{
  fprintf(stderr,"Unknown typed fourwheel query dependency %08x caller%08x\n",target,ra);abort();
 }
 if(rx_sp()!=entry||expected!=entry+cleanup)abort();rx_set_sp(expected);
}
#endif
