/* Isolated source-only family typed dependency revision. One helper kernel,
 * explicit original family return sites; no original dispatch or RA translation.
 * Not accepted until UTV regression and corrected Buggy80/Truck128 typed proofs. */
#include "family_tyre_dependencies.h"
#include "contact_dependencies.h"
#include "fx_util.h"
#include "bike_curve.h"
#include "world_veh.h"
#include "phys_misc.h"
#include "phys_crt.h"
#include <stdlib.h>
typedef struct {unsigned family;uint32_t target,ra;} TyreSite;
static const TyreSite sites[]={
 {0,0x401000,0x4d5418},
 {0,0x401000,0x4d6c3a},
 {0,0x401000,0x4d6fe4},
 {0,0x401000,0x4d701e},
 {0,0x401040,0x4d52a1},
 {0,0x401040,0x4d52de},
 {0,0x42c230,0x4d52e9},
 {0,0x42d6b0,0x4d7594},
 {0,0x430810,0x4d6ec0},
 {0,0x438ee0,0x4d6fef},
 {0,0x438ee0,0x4d7029},
 {0,0x4390d0,0x4d54da},
 {0,0x4390d0,0x4d54f9},
 {0,0x4390d0,0x4d5518},
 {0,0x4390d0,0x4d5533},
 {0,0x4390d0,0x4d5552},
 {0,0x4390d0,0x4d5571},
 {0,0x4390d0,0x4d558a},
 {0,0x4390d0,0x4d6c12},
 {0,0x4390d0,0x4d7008},
 {0,0x4390d0,0x4d7042},
 {0,0x85c770,0x4d7539},
 {0,0x85c770,0x4d754e},
 {0,0x8f9efe,0x4d516e},
 {0,0x8f9efe,0x4d75a4},
 {1,0x401000,0x4bb02a},
 {1,0x401000,0x4bc83b},
 {1,0x401000,0x4bcb3c},
 {1,0x401000,0x4bcb76},
 {1,0x401040,0x4baeb3},
 {1,0x401040,0x4baef0},
 {1,0x42c230,0x4baefb},
 {1,0x42d6b0,0x4bce66},
 {1,0x430810,0x4bca66},
 {1,0x438ee0,0x4bcb47},
 {1,0x438ee0,0x4bcb81},
 {1,0x4390d0,0x4bb0ec},
 {1,0x4390d0,0x4bb10b},
 {1,0x4390d0,0x4bb12a},
 {1,0x4390d0,0x4bb145},
 {1,0x4390d0,0x4bb164},
 {1,0x4390d0,0x4bb183},
 {1,0x4390d0,0x4bb19c},
 {1,0x4390d0,0x4bc813},
 {1,0x4390d0,0x4bcb60},
 {1,0x4390d0,0x4bcb9a},
 {1,0x85c770,0x4bce0e},
 {1,0x85c770,0x4bce23},
 {1,0x8f9efe,0x4badae},
 {1,0x8f9efe,0x4bce76},
 {2,0x401000,0x45737d},
 {2,0x401000,0x458be9},
 {2,0x401000,0x458ee5},
 {2,0x401000,0x458f1f},
 {2,0x401040,0x457206},
 {2,0x401040,0x457243},
 {2,0x42c230,0x45724e},
 {2,0x42d6b0,0x459242},
 {2,0x430810,0x458e13},
 {2,0x438ee0,0x458ef0},
 {2,0x438ee0,0x458f2a},
 {2,0x4390d0,0x45743f},
 {2,0x4390d0,0x45745e},
 {2,0x4390d0,0x45747d},
 {2,0x4390d0,0x457498},
 {2,0x4390d0,0x4574b7},
 {2,0x4390d0,0x4574d6},
 {2,0x4390d0,0x4574ef},
 {2,0x4390d0,0x458bc1},
 {2,0x4390d0,0x458f09},
 {2,0x4390d0,0x458f43},
 {2,0x85c770,0x4591ea},
 {2,0x85c770,0x4591ff},
 {2,0x8f9efe,0x4570fe},
 {2,0x8f9efe,0x4592df},
 {2,0x8f9efe,0x45933f},
 {2,0x8f9efe,0x45939f},
 {2,0x8f9efe,0x4593fa},
};
static int allowed(unsigned family,uint32_t target,uint32_t ra){
 for(unsigned i=0;i<sizeof(sites)/sizeof(sites[0]);++i)
  if(sites[i].family==family&&sites[i].target==target&&sites[i].ra==ra)return 1;
 return 0;
}
static float dependency(unsigned family,rx_addr target,rx_addr expected,float input,float t){
 rx_addr entry=rx_sp();uint32_t cleanup=(target==0x85c770||target==0x42d6b0)?8u:4u;float result=input;
 if(!allowed(family,target,U32(entry,0))||expected!=entry+cleanup)abort();
 if(target==0x401040){
  
  SU32(entry-4,0,fx_reg(FX_ECX));SU32(entry-8,0,0x401046);SU32(entry-12,0,fx_reg(FX_EBP));
  result=rx_world_float_abs_thunk(input);
 }else if(target==0x401000){
  SU32(entry-4,0,fx_reg(FX_EBP));result=rx_world_float_abs(input);
 }else if(target==0x42c230){
  SU32(entry-4,0,fx_reg(FX_EBP));result=phys_sqrtf(F32(entry,4));
 }else if(target==0x4390d0){
  result=rx_contact_indexed_frame(fx_reg(FX_ESI),input);
 }else if(target==0x438ee0){
  
  rx_addr curve=fx_reg(FX_ESI);int32_t last=I32(curve,0)-1;
  SU32(entry-4,0,fx_reg(FX_EBP));SU32(entry-8,0,fx_reg(FX_EDI));if(last>=4)SU32(entry-12,0,fx_reg(FX_EBX));
  float fraction;int32_t segment=bike_param_curve_segment(curve,input,&fraction);result=bike_param_curve(curve,input);
  int32_t i=0;uint32_t p=fx_reg(FX_ECX);
  if(last>=4){p=curve+8;do{i+=4;p+=16;}while(i<last-3);}
  if(i<last){p=curve+4u*(uint32_t)i+4;do{++i;p+=4;}while(i<last);}
  fx_set_reg(FX_EAX,(uint32_t)i);fx_set_reg(FX_ECX,p);fx_set_reg(FX_EDX,(uint32_t)segment);
 }else if(target==0x430810){
  result=bike_lerp_clamped(t,F32(entry,4),F32(entry,8));
 }else if(target==0x85c770){
  cleanup=8;
  (void)rx_contact_force_at_point_frame(fx_reg(FX_ECX),fx_reg(FX_EDX),fx_reg(FX_ESI),U32(entry,4));
 }else if(target==0x42d6b0){
  cleanup=8;result=phys_lag_filter_update(fx_reg(FX_EAX),t,F32(entry,4));
 }else if(target==0x8f9efe){
  phys_security_check_cookie(fx_reg(FX_ECX));
 }else abort(); /* No value substituted for an unknown/private ABI. */
 if(rx_sp()!=entry||expected!=entry+cleanup)abort();rx_set_sp(expected);return result;
}
float rx_utv_tyre_dependency(rx_addr target,rx_addr expected,float input,float t){return dependency(0,target,expected,input,t);}
float rx_buggy_tyre_dependency(rx_addr target,rx_addr expected,float input,float t){return dependency(1,target,expected,input,t);}
float rx_truck_tyre_dependency(rx_addr target,rx_addr expected,float input,float t){return dependency(2,target,expected,input,t);}
