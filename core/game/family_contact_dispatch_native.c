/* Finite explicit UTV/Buggy/Truck vf13 private dependency closure. One existing semantic owner
 * per role; no registrations, original gameplay execution or duplicate math.
 * Source-only: whole-composition proof is REQUIRED before activation. */
#include "fourwheel_contact_dispatch.h"
#include "fourwheel_terrain_query.h"
#include "query_private_dependencies.h"
#include "collision_live.h"
#include "collision_dispatch_observed.h"
#include "body_probe_observed.h"
#include "fourwheel_contact_correction.h"
#include "fourwheel_filtered_contact.h"
#include "fourwheel_project_frames.h"
#include "buggy_body_projection.h"
#include "family_contact_dispatch_native.h"
#include "fx_util.h"
#include "phys_crt.h"
#include "crt.h"
#include <stdio.h>
#include <stdlib.h>
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_FAMILY_VF13_NATIVE_TEST)
static void bad(enum rx_contact_dispatch_role role,rx_addr ra){
 fprintf(stderr,"rxcore: finite familyvf13 role%u caller%08x entry%08x EDI%08x ESI%08x\n",(unsigned)role,ra,rx_sp(),fx_reg(FX_EDI),fx_reg(FX_ESI));abort();
}
static int site(rx_addr ra,const uint32_t*sites,unsigned n){for(unsigned i=0;i<n;i++)if(ra==sites[i])return(int)i;return-1;}
#ifdef RX_FAMILY_VF13_COMPARE
static _Thread_local unsigned counts[9];
void rx_family_vf13_counts_reset(void){for(unsigned i=0;i<9;i++)counts[i]=0;}
unsigned rx_family_vf13_count(unsigned role){if(role>=9)abort();return counts[role];}
#define COUNT(role) (++counts[(unsigned)(role)])
#else
#define COUNT(role) ((void)0)
#endif
rx_v4 rx_fourwheel_contact_dispatch_dependency(enum rx_contact_dispatch_role role,
 unsigned family,rx_addr expected,rx_v4 input_xmm0){
 const rx_addr entry=rx_sp(),ra=U32(entry,0),b=(fx_reg(FX_EBP)&~15u)-0x760u;
 const rx_addr s=fx_reg(FX_EDI);rx_v4 live=input_xmm0;int index=-1;
 static const struct {rx_addr entry,vt;unsigned delta;} layouts[3]={
  {0x4d1d30,0x93d43c,0},{0x4b7580,0x93d24c,0x10},{0x453be0,0x93c93c,0}};
 if(family>=3||(unsigned)role>=9)bad(role,ra);
 const rx_addr base=layouts[family].entry;const unsigned delta=layouts[family].delta;
 /* Normalize ONLY an already identified complete family span. Every role
  * below still checks its exact finite original RA, arguments and frame. */
 if(ra<base||ra-base>=0x1410u)bad(role,ra);
 const rx_addr site_ra=0x4d1d30u+(ra-base);
 const uint32_t query[]={0x4d1ea5,0x4d235e,0x4d2811,0x4d2cc1};
 const uint32_t collision[]={0x4d20db,0x4d258b,0x4d2a3b,0x4d2eeb};
 const uint32_t frame[]={0x4d222f,0x4d26df,0x4d2b8f,0x4d303f};
 const uint32_t bounds[]={0x4d1f2a,0x4d1f77,0x4d203b,0x4d2070,0x4d20a1,0x4d214b,
  0x4d23e7,0x4d2430,0x4d24eb,0x4d2520,0x4d2551,0x4d25fb,
  0x4d2897,0x4d28e0,0x4d299b,0x4d29d0,0x4d2a01,0x4d2aab,
  0x4d2d47,0x4d2d90,0x4d2e4b,0x4d2e80,0x4d2eb1,0x4d2f5b};

 /* Cookie executes after POP EDI/ESI/EBX; their restored caller values are NOT
  * a simulation pointer. All other roles execute with original EDI=self. */
 if(role!=RX_CD_COOKIE&&(U32(s,0)!=layouts[family].vt||U32(layouts[family].vt,0x34)!=base))bad(role,ra);
 COUNT(role);
 switch(role){
 case RX_CD_QUERY:
  index=site(site_ra,query,4);
  if(index<0||entry!=b-8||expected!=b||U32(entry,4)!=s||
     fx_reg(FX_ESI)!=s+0x8080u-delta+0x150u*(unsigned)index)bad(role,ra);
  if(family==0)rx_utv_wheel_terrain_query_exact(fx_reg(FX_ESI),s);
  else if(family==1)rx_buggy_wheel_terrain_query_exact(fx_reg(FX_ESI),s);
  else rx_truck_wheel_terrain_query_exact(fx_reg(FX_ESI),s);break;
 case RX_CD_COLLISION:{
  index=site(site_ra,collision,4);const rx_addr record=U32(b,0x2c);
  if(index<0||entry!=b-32||expected!=b||fx_reg(FX_ECX)!=b+0xa0||
     U32(entry,8)!=record+0x10||U32(entry,12)!=record||
     U32(entry,16)!=s+0x8040-delta||U32(entry,20)!=b+0x70||
     U32(entry,24)!=b+0x60||U32(entry,28)!=0)bad(role,ra);
  const rx_addr begin=U32(s,0x130c),end=U32(s,0x1310);
  if(end<begin||record<begin||record>=end||(record-begin)%0x50||
     U32(entry,4)!=U32(U32(record,0x44),4))bad(role,ra);
  const uint32_t a_type=U32(U32(entry,4),8),b_type=U32(U32(entry,16),8);
  /* Genuine vf13 wheel callers overwrite ECX/XMM0 immediately and EAX/EDX
   * before reading them. Their sole collision result is the manifold memory.
   * Use the declared-output API for direct triangle/circle only; all other
   * existing exact packed paths remain unchanged, including BODY probes. */
  if((a_type==0x10&&b_type==0x400)||(a_type==0x400&&b_type==0x10))
   phys_contact_dispatch_observed(fx_reg(FX_ECX),U32(entry,4),U32(entry,8),
    U32(entry,12),U32(entry,16),U32(entry,20),U32(entry,24),U32(entry,28));
  else phys_contact_dispatch_observed_live(fx_reg(FX_ECX),U32(entry,4),U32(entry,8),
    U32(entry,12),U32(entry,16),U32(entry,20),U32(entry,24),U32(entry,28),&live);
  break;
 }
 case RX_CD_PROBE_FRAME:
  index=site(site_ra,frame,4);
  if(index<0||entry!=b-20||expected!=b||fx_reg(FX_ECX)!=b+0x60||
     fx_reg(FX_EDX)!=b+0x30||U32(entry,4)!=s+0x8080u-delta+0x150u*(unsigned)index||
     U32(entry,8)!=b+0x50||U32(entry,12)!=b+0x70||U32(entry,16)!=b+0x90)bad(role,ra);
  rx_query_probe_setup_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),U32(entry,4),
   U32(entry,8),U32(entry,12),U32(entry,16));break;
 case RX_CD_BODY_PROBE:
  if(site_ra!=0x4d3107||entry!=b-4||expected!=b||fx_reg(FX_ESI)!=s)bad(role,ra);
  bike_body_probe_contacts_observed(s);break;
 case RX_CD_CORRECTION:
  if(site_ra!=0x4d310e||entry!=b-4||expected!=b||fx_reg(FX_EDX)!=s)bad(role,ra);
  if(family==1)rx_buggy_contact_correction(s);else rx_utv_truck_contact_correction(s);break;
 case RX_CD_FILTERED:
  if(site_ra!=0x4d311a||entry!=b-8||expected!=b||fx_reg(FX_ESI)!=s)bad(role,ra);
  if(family==1)rx_buggy_filtered_contact(s,rx_bf(U32(entry,4)));else rx_utv_truck_filtered_contact(s,rx_bf(U32(entry,4)));break;
 case RX_CD_PROJECT:
  if(site_ra!=0x4d3129||entry!=b-4||expected!=b||fx_reg(FX_ECX)!=s||U32(s,0x2374)!=fx_reg(FX_EBX))bad(role,ra);
  if(family==1)rx_buggy_project_body_positions_exact(s);else rx_utv_project_body_positions_exact(s);break;
 case RX_CD_COOKIE:
  if(site_ra!=0x4d313a||entry!=b+8||expected!=b+12)bad(role,ra);
  phys_security_check_cookie(fx_reg(FX_ECX));break;
 case RX_CD_BOUNDS:
  if(site(site_ra,bounds,24)<0||entry!=b-4||expected!=b)bad(role,ra);
  /* Genuine checked-index failure remains canonical fatal behavior. No normal
   * RET is manufactured and this branch is not a claimed returning fixture. */
  crt_invalid_parameter_noinfo();abort();
 default:bad(role,ra);
 }
 if(rx_sp()!=entry||U32(entry,0)!=ra)bad(role,ra);
 rx_set_sp(expected);return live;
}
#endif
