/* Typed curve semantic owner reused once; exact private observer contract
 * follows canonical438ee0's actual saved registers and final scan cursor. */
#include "family_contact_response.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL) || defined(RX_FAMILY_RESPONSE_TYPED_CURVE_TEST)
#include "fx_util.h"
#include "bike_curve.h"
#include <stdlib.h>
#ifdef RX_FAMILY_RESPONSE_TYPED_CURVE_TEST
static uint32_t site_counts[4];
void rx_family_native_curve_reset(void){for(unsigned i=0;i<4;i++)site_counts[i]=0;}
uint32_t rx_family_native_curve_count(unsigned i){if(i>=4)abort();return site_counts[i];}
#endif
float rx_family_response_curve_native(float input,rx_addr expected){
 const uint32_t sites[]={0x451ec2,0x45212f,0x4523ef,0x45261d};rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);int found=0;
 for(unsigned i=0;i<4;i++)if(ra==sites[i])found=1;
 if(!found||expected!=entry+4)abort();
 /*451bf0: EBP=helperEntry-4; aligned16 D8 locals plus EBX/ESI.
  * This is not UTV004cea10's different local size/return-site contract. */
 const rx_addr base=(fx_reg(FX_EBP)&~15u)-0xd8u-8u;
 if(entry!=base-4u||expected!=base||fx_reg(FX_ESI)!=fx_reg(FX_EDI)+0x3d2c)abort();
#ifdef RX_FAMILY_RESPONSE_TYPED_CURVE_TEST
 for(unsigned i=0;i<4;i++)if(ra==sites[i])++site_counts[i];
#endif
 rx_addr curve=fx_reg(FX_ESI);int32_t last=I32(curve,0)-1;
 SU32(entry-4,0,fx_reg(FX_EBP));SU32(entry-8,0,fx_reg(FX_EDI));if(last>=4)SU32(entry-12,0,fx_reg(FX_EBX));
 float fraction;int32_t segment=bike_param_curve_segment(curve,input,&fraction);float result=bike_param_curve(curve,input);
 int32_t i=0;uint32_t cursor=fx_reg(FX_ECX);
 if(last>=4){cursor=curve+8;do{i+=4;cursor+=16;}while(i<last-3);}
 if(i<last){cursor=curve+4u*(uint32_t)i+4;do{++i;cursor+=4;}while(i<last);}
 fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,cursor);fx_set_reg(FX_EDX,(uint32_t)segment);
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return result;
}
#endif
