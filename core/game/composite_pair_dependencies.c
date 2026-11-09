/* Complete finite typed dependencies of872860, no original game dispatcher.
 * Uses entered callee frames already materialized by the canonical owner. */
#include "composite_pair_observed.h"
#include "composite_child_observed.h"
#include "collision_live.h"
#include "phys_crt.h"
#include "crt.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
static void unclosed(rx_addr target,rx_addr ra){fprintf(stderr,"Unclosed native composite dependency %08x caller%08x\n",target,ra);abort();}
void rx_composite_pair_dependency(rx_addr target,rx_addr ra,unsigned n,rx_addr expected,rx_v4*live){
 const rx_addr entry=rx_sp();
 const int rotation=target==0x848650&&ra==0x8728ee&&n==2,position=target==0x8486f0&&ra==0x872902&&n==3;
 const int dispatch=target==0x85ddd0&&ra==0x87292b&&n==7,cookie=target==0x8f9efe&&ra==0x872965&&n==0,bounds=target==0x916224&&ra==0x8728d4&&n==0;
 if(!live||!(rotation||position||dispatch||cookie||bounds)||U32(entry,0)!=ra||expected!=entry+4+4*n)unclosed(target,ra);
#ifdef RX_COLLISION_NATIVE_COUNTS
 {extern void rx_collision_native_provider_count(uint32_t,uint32_t);rx_collision_native_provider_count(target,ra);}
#endif
 const fx_saved saved=fx_save();
 if(rotation)phys_composite_child_rotation_observed(fx_reg(FX_EDI),fx_reg(FX_ECX),U32(entry,4),U32(entry,8),live);
 else if(position)phys_composite_child_position_observed(fx_reg(FX_EDI),fx_reg(FX_ECX),U32(entry,4),U32(entry,8),U32(entry,12),live);
 else if(dispatch)phys_contact_dispatch_observed_live(fx_reg(FX_ECX),U32(entry,4),U32(entry,8),U32(entry,12),U32(entry,16),U32(entry,20),U32(entry,24),U32(entry,28),live);
 else if(cookie){if(fx_reg(FX_ECX)!=U32(0,0x9b45e4))unclosed(target,ra);phys_security_check_cookie(fx_reg(FX_ECX));}
 else{crt_invalid_parameter_noinfo();unclosed(target,ra);} /* Watson domain never returns normally. */
 if(rx_sp()!=entry||U32(entry,0)!=ra||fx_reg(FX_EBX)!=saved.ebx||fx_reg(FX_EBP)!=saved.ebp||fx_reg(FX_ESI)!=saved.esi||fx_reg(FX_EDI)!=saved.edi)unclosed(target,ra);
 rx_set_sp(expected); /* Model only actual RET after validating no stack repair. */
}
