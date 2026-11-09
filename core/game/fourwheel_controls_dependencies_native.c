#include "fourwheel_controls.h"
#if !defined(RX_ORACLE) || defined(RX_ORACLE_FOURWHEEL)
#include "fourwheel_controls_math.h"
#include "fourwheel_steering.h"
#include "bike_curve.h"
#include "phys_misc.h"
#include "phys_crt.h"
#include <stdlib.h>
#include <stdio.h>
/* Typed native calls only. Never treat the register ABI entries as stack ABI
 * dispatch. Existing semantic owners are reused once, with their actual saved
 * frame words and incidental curve outputs retained by this caller bridge. */
static void contract(int ok,rx_addr target,rx_addr entry,rx_addr expected,const char*role){
 if(!ok){fprintf(stderr,"UTV typed dependency contract target%08x entry%08x expected%08x actual%08x %s\n",target,entry,expected,rx_sp(),role);abort();}
}
static int return_site(rx_addr target,rx_addr ra){
 switch(target){
 case 0x401000:return ra==0x4d966c||ra==0x4d9a98||ra==0x4d9af1||ra==0x4d9e96||ra==0x4d9fc4||ra==0x4da008||ra==0x4da101||ra==0x4da1e5;
 case 0x438ee0:return ra==0x4d9607||ra==0x4d9c77||ra==0x4d9ca4||ra==0x4d9ccb||ra==0x4d9cf8||ra==0x4d9d39||ra==0x4d9d66||ra==0x4d9d8d||ra==0x4d9dba||ra==0x4da25b||ra==0x4da277||ra==0x4da293||ra==0x4da2af||ra==0x4da2c7||ra==0x4da2e3||ra==0x4da2ff||ra==0x4da31b||ra==0x4da39c||ra==0x4da3d3||ra==0x4da418;
 case 0x42d6b0:return ra==0x4d9b33||ra==0x4d9bbe||ra==0x4d9bde||ra==0x4da160||ra==0x4da21c;
 case 0x42d670:return ra==0x4d9c06||ra==0x4d9c34||ra==0x4da1c8;
 case 0x430810:return ra==0x4d9a4e;
 case 0x42c1f0:return ra==0x4da193;
 case 0x4d4190:return ra==0x4d9b6f;
 default:return 0;
 }
}
float rx_utv_controls_dependency(rx_addr target,rx_addr expected,float x0,float x1,float x2){
 rx_addr e=rx_sp(),typed_sp=e;float result=x0;
 uint32_t pop=target==0x42d6b0||target==0x42d670?8:4;
 contract(expected==e+pop,target,e,expected,"original RET0/RET4/cdecl caller relation");
 contract(return_site(target,U32(e,0)),target,e,expected,"exact static call site");
 switch(target){
 case 0x401000:
  SU32(e-4,0,fx_reg(FX_EBP));result=rx_bf(rx_fb(x0)&U32(0,0x977570));break;
 case 0x438ee0:{
  rx_addr curve=fx_reg(FX_ESI);int32_t n=I32(curve,0),last=n-1;
  SU32(e-4,0,fx_reg(FX_EBP));SU32(e-8,0,fx_reg(FX_EDI));
  if(last>=4)SU32(e-12,0,fx_reg(FX_EBX));
  float fraction;int32_t segment=bike_param_curve_segment(curve,x0,&fraction);
  result=bike_param_curve(curve,x0);int32_t i=0;uint32_t p=fx_reg(FX_ECX);
  if(last>=4){p=curve+8;do{i+=4;p+=16;}while(i<last-3);}
  if(i<last){p=curve+4u*(uint32_t)i+4;do{++i;p+=4;}while(i<last);}
  fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,p);fx_set_reg(FX_EDX,(uint32_t)segment);break;
 }
 case 0x42d6b0:result=phys_lag_filter_update(fx_reg(FX_EAX),x2,F32(e,4));break;
 case 0x42d670:{
  float tau=F32(e,4);phys_lag_filter_set(fx_reg(FX_ECX),x1,tau);
  /* Second UCOMISS/LAHF is the last EAX write (TEST changes flags only).
   * AH includes reserved bit1 and the three comparison flags. */
  float keep=F32(0,0x977494);uint32_t ah=2;
  if(tau!=tau||keep!=keep)ah|=0x45;else if(tau<keep)ah|=1;else if(tau==keep)ah|=0x40;
  fx_set_reg(FX_EAX,(fx_reg(FX_EAX)&~0xff00u)|(ah<<8));
  result=tau!=keep?F32(0,0x931f24):keep;break;
 }
 case 0x430810:result=bike_lerp_clamped(x2,F32(e,4),F32(e,8));break;
 case 0x42c1f0:{
  SU32(e-4,0,fx_reg(FX_EBP));rx_addr aligned=(e-4)&~7u;
  SU32(aligned-4,0,0x42c203);typed_sp=aligned-4;rx_set_sp(typed_sp);
  result=phys_powf(x0,x1);break;
 }
 case 0x4d4190:rx_fourwheel_steering_quaternions(fx_reg(FX_ESI));break;
 default:abort(); /* Closed static list; no fallback value or original execution. */
 }
 /* Typed semantic APIs preserve their own current caller SP; the steering
  * owner restores its explicit entry frame. Validate that before modeling the
  * original RET. Pow's nested semantic CRT call starts at its mirrored import
  * caller location, not at the outer helper's entry. No drift is normalized. */
 contract(rx_sp()==typed_sp,target,e,expected,"typed callee caller-SP preservation");
 rx_set_sp(expected);return result;
}
#endif
