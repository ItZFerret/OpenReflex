#include "utv_force_gears.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "bike_curve.h"
#include "family_force_sites.h"
#include <stdlib.h>
/* Reuse the authoritative scalar curve owner with its actual structural
 * registers and frame words. This is not generic dispatch of its LTCG ABI. */
float rx_utv_force_gear_curve(rx_addr expected,float input){
 rx_addr entry=rx_sp(),ra=U32(entry,0);
 if(entry+4!=expected||!rx_family_gear_site(ra))abort();
 rx_addr curve=fx_reg(FX_ESI);int32_t last=I32(curve,0)-1;
 SU32(entry-4,0,fx_reg(FX_EBP));SU32(entry-8,0,fx_reg(FX_EDI));
 if(last>=4)SU32(entry-12,0,fx_reg(FX_EBX));
 float fraction;int32_t segment=bike_param_curve_segment(curve,input,&fraction);
 float value=bike_param_curve(curve,input);
 if(rx_sp()!=entry)abort(); /* Typed owner itself has no guest RET. */
 int32_t i=0;uint32_t p=fx_reg(FX_ECX);
 if(last>=4){p=curve+8;do{i+=4;p+=16;}while(i<last-3);}
 if(i<last){p=curve+4u*(uint32_t)i+4;do{++i;p+=4;}while(i<last);}
 fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,p);fx_set_reg(FX_EDX,(uint32_t)segment);
 rx_set_sp(expected);return value;
}
#endif
