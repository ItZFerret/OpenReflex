/* Isolated complete finite family vf15 typed role composition. No main registration,
 * generic ABI dispatch, original-code execution, or invented callback outcome. */
#include "family_contact_response.h"
#include "bike_response_observed.h"
#include "family_response_frames.h"
#include "fourwheel_nodes_exact.h"
#include "fourwheel_rest_frames.h"
#include "fx_util.h"
#include <stdio.h>
#include <stdlib.h>
static void bad(unsigned role){fprintf(stderr,"rxcore: family vf15 typed role contract %u\n",role);abort();}
#ifdef RX_FAMILY_RESPONSE_ALL_TYPED_TEST
static _Thread_local uint32_t roleCounts[7],sharedWords[8];
void rx_family_response_native_roles_reset(void){for(unsigned i=0;i<7;++i)roleCounts[i]=0;for(unsigned i=0;i<8;++i)sharedWords[i]=0;}
uint32_t rx_family_response_native_roles_count(unsigned i){if(i>=7)bad(i);return roleCounts[i];}
uint32_t rx_family_response_native_shared_word(unsigned i){if(i>=8)bad(i);return sharedWords[i];}
#define SHARED_WORDS(s) do{for(unsigned i=0;i<4;++i){sharedWords[i]=U32(s,0x100+4*i);sharedWords[i+4]=U32(s,0x740+4*i);}}while(0)
#define ROLE_COUNT(i) (++roleCounts[(unsigned)(i)])
#else
#define ROLE_COUNT(i) ((void)0)
#define SHARED_WORDS(s) ((void)0)
#endif
typedef struct {rx_addr entry;uint32_t ra,ebx,ebp,esi,edi;} Entered;
static void retained(const Entered*e,unsigned role,rx_addr expected){
 if(rx_sp()!=expected||U32(e->entry,0)!=e->ra||fx_reg(FX_EBX)!=e->ebx||fx_reg(FX_EBP)!=e->ebp||fx_reg(FX_ESI)!=e->esi||fx_reg(FX_EDI)!=e->edi)bad(role);
}
static void returned(const Entered*e,unsigned role,rx_addr expected){retained(e,role,e->entry);rx_set_sp(expected);}
void rx_family_contact_response_dependency(enum rx_family_response_role role,rx_addr expected){
 const rx_addr entry=rx_sp(),ra=U32(entry,0),s=fx_reg(FX_EDI);uint32_t result;
 const Entered saved={entry,ra,fx_reg(FX_EBX),fx_reg(FX_EBP),fx_reg(FX_ESI),fx_reg(FX_EDI)};
 const uint32_t vt=U32(s,0);const rx_addr base=vt==0x93d24c?0x4b3520:vt==0x93c93c?0x451030:0;
 if(!base||(unsigned)role>=7)bad(role);ROLE_COUNT(role);
 switch(role){
 case RX_FR_SHARED:
  if(ra!=base+0x13||expected!=entry+4||fx_reg(FX_ECX)!=s)bad(role);
  bike_contact_response_observed_exact(s);SHARED_WORDS(s);returned(&saved,role,expected);break;
 case RX_FR_CLAMP:
  if(ra!=base+0x19||expected!=entry+8||U32(entry,4)!=s)bad(role);
  rx_family_response_clamp_frame(s,expected);break;
 case RX_FR_NODES:
  if(ra!=base+0x25||expected!=entry+8||U32(entry,4)!=s+0xd0||fx_reg(FX_EBX)!=s+0xd0)bad(role);
  result=phys_mech_nodes_slot7_exact(s+0xd0);(void)result;returned(&saved,role,expected);break;
 case RX_FR_HELPER:
  if(ra!=base+0x2a||expected!=entry+4)bad(role);
  rx_family_contact_response_helper_exact(s);returned(&saved,role,expected);break;
 case RX_FR_ENTER:
  if(ra!=base+0x70||expected!=entry+4||fx_reg(FX_ECX)!=s||U32(U32(s,0),0x64)!=0x4324d0||fx_reg(FX_EDX)!=0x4324d0)bad(role);
  result=rx_fourwheel_enter_rest_frame(s);retained(&saved,role,entry);fx_set_reg(FX_EAX,result);rx_set_sp(expected);break;
 case RX_FR_LEAVE:
  if(ra!=base+0x54||expected!=entry+4||fx_reg(FX_ECX)!=s||U32(U32(s,0),0x68)!=0x432360||fx_reg(FX_EDX)!=0x432360)bad(role);
  result=rx_fourwheel_leave_rest_frame(s);retained(&saved,role,entry);fx_set_reg(FX_EAX,result);rx_set_sp(expected);break;
 case RX_FR_VELOCITY:
  if(ra!=base+0x8e||expected!=entry+4||fx_reg(FX_EDX)!=s+0xd0||fx_reg(FX_ESI)!=0xd61670)bad(role);
  rx_family_response_velocity_frame(s+0xd0,0xd61670,expected);break;
 default:bad(role);
 }
 retained(&saved,role,expected);
}

/* This wrapper retains the accepted family curve observer once; link the
 * unchanged native_curve_dependencies.c with its TEST macro for oracle proof. */
extern float rx_family_response_curve_native(float input,rx_addr expected);
float rx_family_contact_response_curve(float input,rx_addr expected){return rx_family_response_curve_native(input,expected);}
