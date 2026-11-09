/* Isolated typed dependency proposal for the accepted Truck engine body.
 * No registration. Original full-frame comparison does not yet cover this
 * provider: CRT private stack/register effects require separate validation. */
#include "truck_engine.h"
#include "fx_util.h"
#include "bike_curve.h"
#include "world_veh.h"
#include <stdlib.h>

static int site(uint32_t ra,const uint32_t *sites,unsigned count){
 for(unsigned i=0;i<count;++i)if(ra==sites[i])return 1;
 return 0;
}

float rx_truck_engine_dependency(rx_addr target,rx_addr expected,float input,float t){
 const rx_addr entry=rx_sp();const uint32_t ra=U32(entry,0);
 static const uint32_t curves[]={0x455888,0x4558bb,0x4559d6,0x455aa7};
 static const uint32_t lerps[]={0x455b88,0x455bbe,0x455be8,0x455c5f};
 if(expected!=entry+4)abort();
 float result;
 if(target==0x438ee0&&site(ra,curves,4)){
  /* Reuse the canonical cubic evaluation and knot search. These are the
   * same saved-register/cursor observations as the accepted UTV provider;
   * no second curve arithmetic implementation is introduced. */
  const rx_addr curve=fx_reg(FX_ESI);const int32_t last=I32(curve,0)-1;
  SU32(entry-4,0,fx_reg(FX_EBP));SU32(entry-8,0,fx_reg(FX_EDI));
  if(last>=4)SU32(entry-12,0,fx_reg(FX_EBX));
  float fraction;const int32_t segment=bike_param_curve_segment(curve,input,&fraction);
  result=bike_param_curve(curve,input);
  int32_t i=0;uint32_t cursor=fx_reg(FX_ECX);
  if(last>=4){cursor=curve+8;do{i+=4;cursor+=16;}while(i<last-3);}
  if(i<last){cursor=curve+4u*(uint32_t)i+4;do{++i;cursor+=4;}while(i<last);}
  fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,cursor);fx_set_reg(FX_EDX,(uint32_t)segment);
 }else if(target==0x430810&&site(ra,lerps,4)){
  result=bike_lerp_clamped(t,F32(entry,4),F32(entry,8));
 }else if(target==0x401000&&ra==0x455ee0){
  SU32(entry-4,0,fx_reg(FX_EBP));result=rx_world_float_abs(input);
 }else abort();
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return result;
}

rx_f80 rx_truck_engine_power_dependency(rx_addr expected,rx_f80 base,rx_f80 exponent){
 const rx_addr entry=rx_sp();
 if(expected!=entry+4||U32(entry,0)!=0x4557eb)abort();
 uint32_t regs[]={fx_reg(FX_EAX),fx_reg(FX_ECX),fx_reg(FX_EDX)};
 rx_f80 result=crt_ci_pow_ex(base,exponent,CRT_CW_MAIN,0x1f80,regs);
 if(rx_sp()!=entry)abort();
 for(unsigned i=0;i<3;++i)fx_set_reg((int)i,regs[i]);
 rx_set_sp(expected);return result;
}

rx_f80 rx_truck_engine_fmod_dependency(rx_addr expected){
 const rx_addr entry=rx_sp();
 if(expected!=entry+4||U32(entry,0)!=0x455e94)abort();
 /* 401020: PUSH EBP; align8; FLD a,b; CALL the CIfmod import; RET0.
  * Preserve the original result as F80 until the engine's actual FSTP.
  * Canonical CRT math owns classification/rounding; host fmod is never used.
  * This models the wrapper frame only, not unproved CRT internal dead stack
  * or volatile registers. Constructor testing must report that limit. */
 const rx_f80 a=fx_ld(entry+4),b=fx_ld(entry+8);
 fx_frame frame;fx_frame_enter(&frame,0,8);
 SU32(frame.E-4,0,0x401031);rx_set_sp(frame.E-4);
 rx_f80 result=crt_ci_fmod(a,b,CRT_CW_MAIN);
 if(rx_sp()!=frame.E-4)abort();
 rx_set_sp(frame.E);fx_frame_leave(&frame);
 if(rx_sp()!=entry)abort();rx_set_sp(expected);return result;
}
