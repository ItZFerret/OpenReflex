/* Typed canonical owners once. No duplicate clamp/velocity math, no generic
 * dispatch, unknown callback substitute, or complete-chain registration. */
#include "utv_response_frames.h"
#include "fx_util.h"
#include "vehicle_utv_runtime.h"
#include "phys_mech.h"
#include "utv_contact_response_helper.h"
#include <stdlib.h>
void rx_utv_response_clamp_frame(rx_addr s,rx_addr expected){
 rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cdd39||U32(entry,4)!=s||expected!=entry+8)abort();
 /* Existing canonical world_frame_enter reserves the right addresses but
  * does not write saved GPRs. The original has no calls, so writing these
  * four exact words once supplies all missing prologue stack observation. */
 rx_addr ebp=entry-4,b=(ebp&~15u)-0x20;
 SU32(ebp,0,fx_reg(FX_EBP));SU32(b,8,fx_reg(FX_EBX));SU32(b,4,fx_reg(FX_ESI));SU32(b,0,fx_reg(FX_EDI));
 rx_utv_clamp_angular_velocity(s);
 if(rx_sp()!=entry)abort();
 int32_t count=I32(s,0xec);fx_set_reg(FX_EAX,s);
 if(count>1){fx_set_reg(FX_ECX,s+0x740+160u*(uint32_t)count);fx_set_reg(FX_EDX,(uint32_t)count);}
 else{fx_set_reg(FX_ECX,U32(b,0x14));fx_set_reg(FX_EDX,1);}
 rx_set_sp(expected);
}
void rx_utv_response_velocity_frame(rx_addr m,rx_addr vector,rx_addr expected){
 rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cddae||expected!=entry+4||fx_reg(FX_EDX)!=m||fx_reg(FX_ESI)!=vector||vector!=0xd61670)abort();
 /* Actual73B leaf has no frame/calls. Count and source are re-read by the
  * canonical owner each iteration; this vf15 site uses real shared zero. */
 phys_mech_set_velocity(m,vector);
 if(rx_sp()!=entry)abort();int32_t count=I32(m,0x1c);
 fx_set_reg(FX_ECX,count>0?(uint32_t)count:0);
 if(count>0)fx_set_reg(FX_EAX,m+0x90+160u*(uint32_t)count);
 /* EAX is genuinely untouched for nonpositive count; EDX/ESI unchanged. */
 rx_set_sp(expected);
}
void rx_utv_response_helper_frame(rx_addr s,rx_addr expected){
 rx_addr entry=rx_sp();if(U32(entry,0)!=0x4cdd4a||expected!=entry+4||fx_reg(FX_EDI)!=s)abort();
 rx_utv_contact_response_helper_exact(s);if(rx_sp()!=entry)abort();rx_set_sp(expected);
}
