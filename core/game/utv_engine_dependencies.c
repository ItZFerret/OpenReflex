#include "utv_engine.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fx_util.h"
#include "bike_curve.h"
#include "mechanism_angular_torque.h"
#include <stdlib.h>
static int site(uint32_t ra,const uint32_t*sites,unsigned n){for(unsigned i=0;i<n;i++)if(ra==sites[i])return 1;return 0;}
float rx_utv_engine_dependency(rx_addr target,rx_addr expected,float input,float t){
 rx_addr entry=rx_sp();uint32_t ra=U32(entry,0);float result=input;uint32_t cleanup=4;
 static const uint32_t curves[]={0x4d39e9,0x4d3a1c,0x4d3b37,0x4d3c08};
 static const uint32_t lerps[]={0x4d3ce9,0x4d3d1f,0x4d3d49,0x4d3dc0};
 static const uint32_t torques[]={0x4d4115,0x4d4123,0x4d4131,0x4d413f,0x4d414d,0x4d415b,0x4d4169,0x4d4177};
 if(target==0x438ee0){
  if(!site(ra,curves,4)||expected!=entry+4)abort();
  rx_addr curve=fx_reg(FX_ESI);int32_t last=I32(curve,0)-1;
  SU32(entry-4,0,fx_reg(FX_EBP));SU32(entry-8,0,fx_reg(FX_EDI));if(last>=4)SU32(entry-12,0,fx_reg(FX_EBX));
  float fraction;int32_t segment=bike_param_curve_segment(curve,input,&fraction);result=bike_param_curve(curve,input);
  int32_t i=0;uint32_t p=fx_reg(FX_ECX);
  if(last>=4){p=curve+8;do{i+=4;p+=16;}while(i<last-3);}
  if(i<last){p=curve+4u*(uint32_t)i+4;do{++i;p+=4;}while(i<last);}
  fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,p);fx_set_reg(FX_EDX,(uint32_t)segment);
 }else if(target==0x430810){
  if(!site(ra,lerps,4)||expected!=entry+4)abort();
  result=bike_lerp_clamped(t,F32(entry,4),F32(entry,8));
 }else if(target==0x85c9a0){
  cleanup=12;if(!site(ra,torques,8)||expected!=entry+cleanup)abort();
  (void)rx_mechanism_add_body_torque(fx_reg(FX_ECX),U32(entry,4),U32(entry,8));
 }else if(target==0x8f9efe){
  if(ra!=0x4d4186||expected!=entry+4)abort();phys_security_check_cookie(fx_reg(FX_ECX));
 }else abort(); /* Closed audited static sites; never a substitute return. */
 if(rx_sp()!=entry||expected!=entry+cleanup)abort();
 rx_set_sp(expected);return result;
}
rx_f80 rx_utv_engine_power_dependency(rx_addr expected,rx_f80 base,rx_f80 exponent){
 rx_addr entry=rx_sp();if(expected!=entry+4||U32(entry,0)!=0x4d394c)abort();
 /*9009d2 is an import jump, not another CALL. Reuse the existing exact CRT
  * CIpow semantic owner (main-thread CW/MXCSR contract), never host pow.
  * Unsupported CRT routes retain its existing explicit unavailable boundary. */
 uint32_t regs[]={fx_reg(FX_EAX),fx_reg(FX_ECX),fx_reg(FX_EDX)};
 rx_f80 result=crt_ci_pow_ex(base,exponent,CRT_CW_MAIN,0x1f80,regs);
 if(rx_sp()!=entry)abort();for(unsigned i=0;i<3;i++)fx_set_reg((int)i,regs[i]);
 rx_set_sp(expected);return result;
}
#endif
