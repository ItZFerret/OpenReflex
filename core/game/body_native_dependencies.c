/* Finite BODY435b00 native composition proposal. No registration, original
 * execution, generic calling convention dispatch or duplicate physics math.
 * Link only after every source/proof gate is satisfied. In particular the live
 * collision seam is a REQUIRED external canonical owner, not a success stub. */
#include "body_probe_observed.h"
#include "collision_live.h"
#include "utv_probes_observed.h"
#include "family_probes_observed.h"
#include "terrain_height_exact.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_BODY_NATIVE_DEPENDENCY_TEST)
static void bad(enum rx_body_probe_role role,rx_addr target,rx_addr ra){
 fprintf(stderr,"Unclosed BODY native role%u target%08x caller%08x\n",(unsigned)role,target,ra);abort();
}
static void retained(rx_addr entry,rx_addr ra){
 if(rx_sp()!=entry||U32(entry,0)!=ra)abort();
}
rx_v4 rx_body_probe_dependency(enum rx_body_probe_role role,rx_addr target,
 rx_addr expected,rx_v4 input_xmm0){
 const rx_addr entry=rx_sp(),ra=U32(entry,0),b=(fx_reg(FX_EBP)&~15u)-0x730u;
 const rx_addr s=fx_reg(FX_ESI);rx_v4 live=input_xmm0;
 switch(role){
 case RX_BP_PROBES:
  if(ra!=0x435b28||entry!=b-4||expected!=b||fx_reg(FX_ECX)!=s||
     target!=U32(U32(s,0),0x80))bad(role,target,ra);
  /* Vf32 inherits the actual incoming SIMD; no zeroing or substitute pose.
   * Each exact kernel exposes its already-computed final packed XMM0. */
  if(target==0x4da510)rx_sim_utv_update_probes_observed(s,&live);
  else if(target==0x4bf400)rx_sim_buggy_update_probes_observed(s,&live);
  else if(target==0x45b8f0)rx_sim_truck_update_probes_observed(s,&live);
  else bad(role,target,ra);
  break;
 case RX_BP_TERRAIN:
  if(target!=0x7c5e80||ra!=0x435cd8||entry!=b-12||expected!=b||
     fx_reg(FX_ECX)!=b+0x30||U32(entry,8)!=b+0x20)bad(role,target,ra);
  fx_set_reg(FX_EAX,phys_terrain_height_normal_exact(fx_reg(FX_ECX),
   U32(entry,4),U32(entry,8),&live));
  break;
 case RX_BP_COLLISION:
  if(target!=0x85ddd0||ra!=0x435ede||entry!=b-32||expected!=b||
     fx_reg(FX_ECX)!=b+0x70||U32(entry,16)!=s+0x20d0||
     U32(entry,20)!=b+0x50||U32(entry,24)!=b+0x60||U32(entry,28))bad(role,target,ra);
  /* This external exact owner includes finite pair providers. Unknown routes
   * terminate explicitly. live is real input AND output, including misses. */
  phys_contact_dispatch_observed_live(fx_reg(FX_ECX),U32(entry,4),
   U32(entry,8),U32(entry,12),U32(entry,16),U32(entry,20),U32(entry,24),
   U32(entry,28),&live);
  break;
 case RX_BP_BOUNDS:
  if(target!=U32(0,0x916224)||entry!=b-4||expected!=b||
     (ra!=0x435e3e&&ra!=0x435e73&&ra!=0x435ea4&&ra!=0x435f75))bad(role,target,ra);
  /* Original _invalid_parameter_noinfo reports/terminates (Watson). The
   * canonical product owner aborts; no modeled RET or fabricated success.
   * Preserve the existing pending RA and register frame into that owner. */
  crt_invalid_parameter_noinfo();abort();
 case RX_BP_COOKIE:
  if(target!=0x8f9efe||ra!=0x4360a2||entry!=b+4||expected!=b+8)bad(role,target,ra);
  /* Actual cookie equality is a bare return after CMP; no stack/GPR/XMM
   * writes. Mismatch delegates to canonical report_gsfailure fatal boundary;
   * current product names unported008fa43c and aborts, never returns. */
  phys_security_check_cookie(fx_reg(FX_ECX));
  break;
 default:bad(role,target,ra);
 }
 retained(entry,ra);rx_set_sp(expected);return live;
}
#endif
