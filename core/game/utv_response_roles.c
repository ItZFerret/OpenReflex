/* Isolated complete finite UTVvf15 typed role composition. No main registration,
 * generic ABI dispatch, original-code execution, or invented callback outcome. */
#include "utv_contact_response.h"
#include "bike_response_observed.h"
#include "utv_response_frames.h"
#include "fourwheel_nodes_exact.h"
#include "fourwheel_rest_frames.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
static void bad(unsigned role){fprintf(stderr,"rxcore: UTVvf15 typed role contract %u\n",role);abort();}
#ifdef RX_UTV_RESPONSE_ALL_TYPED_TEST
static _Thread_local uint32_t roleCounts[7];
void rx_response_native_roles_reset(void){for(unsigned i=0;i<7;++i)roleCounts[i]=0;}
uint32_t rx_response_native_roles_count(unsigned i){if(i>=7)bad(i);return roleCounts[i];}
#define ROLE_COUNT(i) (++roleCounts[(unsigned)(i)])
#else
#define ROLE_COUNT(i) ((void)0)
#endif
void rx_utv_contact_response_dependency(enum rx_utv_response_role role,rx_addr expected){
 const rx_addr entry=rx_sp(),ra=U32(entry,0),s=fx_reg(FX_EDI);uint32_t result;
 if((unsigned)role>=7||U32(s,0)!=0x93d43c)bad(role);ROLE_COUNT(role);
 switch(role){
 case RX_UR_SHARED:
  if(ra!=0x4cdd33||expected!=entry+4||fx_reg(FX_ECX)!=s)bad(role);
  bike_contact_response_observed_exact(s);if(rx_sp()!=entry)bad(role);rx_set_sp(expected);break;
 case RX_UR_CLAMP:
  if(ra!=0x4cdd39||expected!=entry+8||U32(entry,4)!=s)bad(role);
  rx_utv_response_clamp_frame(s,expected);break;
 case RX_UR_NODES:
  if(ra!=0x4cdd45||expected!=entry+8||U32(entry,4)!=s+0xd0||fx_reg(FX_EBX)!=s+0xd0)bad(role);
  result=phys_mech_nodes_slot7_exact(s+0xd0);(void)result;if(rx_sp()!=entry)bad(role);rx_set_sp(expected);break;
 case RX_UR_HELPER:
  if(ra!=0x4cdd4a||expected!=entry+4)bad(role);
  rx_utv_response_helper_frame(s,expected);break;
 case RX_UR_ENTER:
  if(ra!=0x4cdd90||expected!=entry+4||fx_reg(FX_ECX)!=s||U32(U32(s,0),0x64)!=0x4324d0||fx_reg(FX_EDX)!=0x4324d0)bad(role);
  result=rx_fourwheel_enter_rest_frame(s);if(rx_sp()!=entry)bad(role);fx_set_reg(FX_EAX,result);rx_set_sp(expected);break;
 case RX_UR_LEAVE:
  if(ra!=0x4cdd74||expected!=entry+4||fx_reg(FX_ECX)!=s||U32(U32(s,0),0x68)!=0x432360||fx_reg(FX_EDX)!=0x432360)bad(role);
  result=rx_fourwheel_leave_rest_frame(s);if(rx_sp()!=entry)bad(role);fx_set_reg(FX_EAX,result);rx_set_sp(expected);break;
 case RX_UR_VELOCITY:
  if(ra!=0x4cddae||expected!=entry+4||fx_reg(FX_EDX)!=s+0xd0||fx_reg(FX_ESI)!=0xd61670)bad(role);
  rx_utv_response_velocity_frame(s+0xd0,0xd61670,expected);break;
 default:bad(role);
 }
 if(rx_sp()!=expected)bad(role);
}
