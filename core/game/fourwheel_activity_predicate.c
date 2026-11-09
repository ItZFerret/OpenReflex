/* Complete669B original four-wheel vf4 predicates. Explicit layout differs
 * only at the four final contact words. This is not the ATV OR-contact rule. */
#include "fourwheel_activity_predicate.h"
#include "bike_math.h"
#include "fx_util.h"
static const uint32_t explicit_flags[]={
 0x1af0,0x1af4,0x1af8,0x1afc,0x1b00,0x1b04,
 0x1b10,0x1b14,0x1b18,0x1b1c,0x1b20,0x1b24,
 0x1b34,0x1b38,0x1b44,0x1b48,0x1b4c,0x1b50,
 0x1b54,0x1b58,0x1b5c,0x1b60,0x1b64,0x1b68,
 0x1b74,0x1b78,0x1b7c,0x1b80,0x1b84,0x1b88,
 0x1b98,0x1b9c,0x1ba8,0x1bac,0x1bb0,0x1bb4
};
static uint32_t predicate(rx_addr s,uint32_t wheel_delta){
 fx_frame f;fx_frame_enter(&f,0x28,16,FX_ESI,FX_EDI);
 uint32_t has_flag=0;
 for(unsigned i=0;i<sizeof(explicit_flags)/sizeof(*explicit_flags);++i)
  if(U32(s,explicit_flags[i])){has_flag=1;break;}
 /* The original signed count is not normalized to an unsigned loop bound.
  * Even an explicit flag still executes this loop and both packed lengths. */
 rx_addr cursor=s+0x1af0;int32_t remaining=I32(s,0x1318);uint32_t aggregate=0;
 while(remaining>0){aggregate|=U32(cursor,0);cursor+=4;--remaining;}
 rx_v4 p=rx_ld4(s+0x2ec0);
 rx_v4 impulse=v4_splat(phys_sqrt(bike_hsum_021(v4_mul(p,p))));
 rx_v4 impulse_mask=v4_cmplt(rx_ld4(0x00d61670),impulse);
 rx_st4(f.E+0x10,impulse_mask);
 rx_v4 v=rx_ld4(s+0x100);
 rx_v4 speed=v4_splat(phys_sqrt(bike_hsum_021(v4_mul(v,v))));
 rx_v4 slow_mask=v4_cmplt(speed,v4_splat(F32(0,0x009322c0)));
 rx_st4(f.E+0x20,slow_mask);
 uint32_t result=1,edx=cursor;
 if(!has_flag){
  uint32_t mask_sum=U32(f.E,0x18)+U32(f.E,0x14);mask_sum+=U32(f.E,0x10);
  edx=(mask_sum==0xfffffffdu?1u:0u)|aggregate;
  if(edx==0)result=0;
  else{
   edx=U32(f.E,0x28)+U32(f.E,0x24);edx+=U32(f.E,0x20);
   if(edx!=0xfffffffdu)result=0;
   else{
    uint32_t contacts=U32(s,0x8570-wheel_delta)&U32(s,0x8420-wheel_delta);
    contacts&=U32(s,0x82d0-wheel_delta);
    if((U32(s,0x8180-wheel_delta)&contacts)!=0)result=0;
   }
  }
 }
 fx_set_reg(FX_EAX,result);fx_set_reg(FX_EDX,edx);fx_frame_leave(&f);return result;
}
uint32_t rx_fourwheel_activity_predicate(rx_addr self){return predicate(self,0);}
uint32_t rx_buggy_activity_predicate(rx_addr self){return predicate(self,0x10);}
uint32_t rx_fourwheel_contact_mode(rx_addr self){return U32(self,0x2e9c);}
RX_FUNCTION(0x0045cc10,rx_fourwheel_activity_predicate)
RX_FUNCTION(0x004bff90,rx_buggy_activity_predicate)
RX_FUNCTION(0x0045ceb0,rx_fourwheel_contact_mode)
