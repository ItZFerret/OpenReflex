/* Native observer-only role seam. Existing typed owners execute once; these
 * observers model incidental stack/GPR effects, never duplicate response logic. */
#include "bike_response_observed.h"
#include "fx_util.h"
#include "world_veh.h"
#include "world_wreck.h"
#include "phys_body.h"
#include <stdio.h>
#include <stdlib.h>
#define R(r,v) fx_set_reg(FX_##r,(v))
static void bad(rx_addr t){fprintf(stderr,"shared response native dependency contract %08x\n",t);abort();}
#ifdef RX_SHARED_RESPONSE_DEPENDENCY_TEST
static _Thread_local uint32_t dependency_counts[3];
void rx_shared_response_dependency_reset(void){for(unsigned i=0;i<3;i++)dependency_counts[i]=0;}
uint32_t rx_shared_response_dependency_count(unsigned i){if(i>=3)abort();return dependency_counts[i];}
#define COUNT(i) (++dependency_counts[i])
#else
#define COUNT(i) ((void)0)
#endif
void rx_response_dependency_exact(rx_addr target,rx_addr ra,rx_addr caller){
 const rx_addr entry=rx_sp();if(entry!=caller-4||U32(entry,0)!=ra)bad(target);
 if(target==0x402dc0&&ra==0x43760f){
  COUNT(0);
  rx_addr list=fx_reg(FX_EDX);uint32_t n=U32(list,8);
  SU32(entry,-4,fx_reg(FX_EBX));SU32(entry,-8,fx_reg(FX_ESI));SU32(entry,-12,fx_reg(FX_EDI));
  rx_set_sp(entry-12);rx_addr found=rx_world_find_by_controller(list);if(rx_sp()!=entry-12)bad(target);
  if(n)R(ECX,found?found:U32(list,4u*(n-1u)));R(EAX,found);
 }else if(target==0x75c580&&ra==0x437ff5){
  COUNT(1);
  SU32(entry,-4,fx_reg(FX_ESI));rx_set_sp(entry-4);
  rx_addr world=rx_world_active_sound_table(fx_reg(FX_EDX));if(rx_sp()!=entry-4)bad(target);
  R(EAX,world);R(ECX,1);
 }else if(target==0x42bd90&&ra==0x438022){
  COUNT(2);
  rx_addr handle=fx_reg(FX_EAX),world=U32(handle,4);uint32_t bytes=U32(world,0x1964)-U32(world,0x1960),index=U32(handle,8);
  SU32(entry,-4,fx_reg(FX_ESI));SU32(entry,-8,fx_reg(FX_EDI));
  int64_t product=(int64_t)(int32_t)0x92492493u*(int64_t)(int32_t)bytes;
  uint32_t sum=(uint32_t)((uint64_t)product>>32)+bytes,quotient=(uint32_t)((int32_t)sum>>7),count=(quotient>>31)+quotient;
  R(ECX,bytes);R(EDX,quotient);
  /* The real checked-owner failure terminates. Preserve its actual import RA
   * before invoking it, without inventing a returning error branch. */
  if(index>=count)SU32(entry,-12,0x42bdc7);
  rx_set_sp(entry-8);rx_addr record=phys_body_record(handle);if(rx_sp()!=entry-8)bad(target);R(EAX,record);
 }else bad(target);
 rx_set_sp(caller);
}
