/* Isolated native private bridge. No address registration or generic LTCG
 * dispatch. Existing semantic owners remain unique; exact observers only add
 * the original frame words/incidental GPR effects used by this active caller. */
#include "fourwheel_active_solve.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_FOURWHEEL_ACTIVE_TYPED_BRIDGE)
#include "phys_math.h"
#include "fx_util.h"
#include "phys_mech.h"
#include "phys_crt.h"
#include "world_veh.h"
#include <stdlib.h>
static int family_site(uint32_t ra,uint32_t utv){return ra==utv||ra==0x4b65e0+(utv-0x4d0d90);}
rx_v4 rx_fourwheel_active_dependency(rx_addr target,rx_addr expected,rx_v4 x0,rx_v4 x1,rx_v4 x2){
 const rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);rx_v4 result=x0;
 if(expected!=entry+4)abort(); /* Every audited private target is RET0. */
 if(target==0x401630){
  if(!family_site(ra,0x4d1025))abort();SU32(entry-4,0,fx_reg(FX_EBP));
  rx_addr destination=U32(entry,4);fx_set_reg(FX_EDX,destination);
  rx_world_vec3_cross(destination,fx_reg(FX_ECX),fx_reg(FX_EAX));
  /* OriginalXMM0={out.z,0,0,0}; only its scalar loads are later used. */
  result=v4(F32(destination,8),0,0,0);
 }else if(target==0x42c230){
  if(!family_site(ra,0x4d1078)&&!family_site(ra,0x4d1151)&&!family_site(ra,0x4d11d8)&&!family_site(ra,0x4d1245)&&!family_site(ra,0x4d12aa))abort();
  SU32(entry-4,0,fx_reg(FX_EBP));result.v[0]=phys_sqrtf(F32(entry,4));
 }else if(target==0x401000){
  if(!family_site(ra,0x4d1536)&&!family_site(ra,0x4d154c)&&!family_site(ra,0x4d1562)&&!family_site(ra,0x4d1578))abort();
  SU32(entry-4,0,fx_reg(FX_EBP));result=v4(rx_world_float_abs(x0.v[0]),0,0,0);
 }else if(target==0x42ca00){
  if(!family_site(ra,0x4d14c9))abort();result=phys_quat_slerp(x0,x1,x2);
  /* Unique existing exact observer, exposed in native only by the reviewed
   * guard-only canonical variant. Nested original trig frames/RAs included. */
  phys_quat_slerp_dead_stack(entry,fx_reg(FX_EBP),x0,x1,x2);
 }else if(target==0x85c530){
  if(!family_site(ra,0x4d1523))abort();rx_addr mechanism=fx_reg(FX_EDX),rotation=fx_reg(FX_ESI);
  SU32(entry-4,0,fx_reg(FX_EDI));phys_mech_rotate(mechanism,rotation);
  int32_t count=I32(mechanism,0x1c);fx_set_reg(FX_ECX,count>0?(uint32_t)count:0);
  if(count>0)fx_set_reg(FX_EAX,mechanism+0x20+160u*(uint32_t)count);
  /* OriginalEDX/ESI/EDI unchanged; EAX also unchanged for nonpositive count.
   * Its XMM6 root-position value is genuinely unused by004d0d90/004b65e0.
   * This void role's returned vector is discarded by the caller, not a game
   * result or a generic replacement for MechanismRotate's full SIMD ABI. */
 }else if(target==0x8f9efe){
  if(!family_site(ra,0x4d166c))abort();phys_security_check_cookie(fx_reg(FX_ECX));
 }else abort();
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return result;
}
#endif
