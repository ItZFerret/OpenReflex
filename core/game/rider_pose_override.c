/* Full0061c0c0/0061efa0. Scalar SSE grouping/COMISS directions are intentional. */
#include "rider_pose_override.h"
#include "fx_util.h"
#include "phys_vec.h"
#include <stdlib.h>
static void listeners(rx_addr list,rx_addr value,rx_addr ra){
 const rx_addr before=fx_push(2,(uint32_t[]){list,value},ra);
 rx_rider_override_listeners(list,value,ra);
 if(rx_sp()!=before)abort();
}
/* JAE after COMISS(0,upper) excludes unordered; JB on(low,current) includes
 * unordered. Preserve both rules rather than using a host min/max routine. */
static float reset_blend(rx_addr s){
 float v=F32(s,0x588);const float lo=F32(s,0x584);
 if(!(0.0f>=v))v=0.0f;
 if(lo>=v)v=lo; /* JB retains v on unordered. */
 return v;
}
uint32_t rx_rider_override_select_pose(rx_addr s){
 fx_frame f;fx_frame_flat(&f,8,FX_ESI);fx_set_reg(FX_ESI,s);
 uint32_t result;
 if(U32(s,0x5bb54)==0){
  if(U32(s,0xaf54)!=0){
   SU32(s,0xaf54,0);
   rx_addr p=U32(s,0x564);fx_set_reg(FX_EAX,p);if(p)SU8(p,0,1);
   p=U32(s,0x568);fx_set_reg(FX_EAX,p);
   if(p){uint32_t dx=(fx_reg(FX_EDX)&0xffff0000u)|U16(s,0x5ac);fx_set_reg(FX_EDX,dx);SU16(p,0,(uint16_t)dx);}
   float v=reset_blend(s);p=U32(s,0x58c);fx_set_reg(FX_ESI,p);if(p)SF32(p,0,v);
  }
  result=0;
 }else{
  /* JBE takes unordered as well as <=. The > branch is ordered only. */
  if(F32(s,0xaf68)>F32(s,0x5bb30)){
   if(U32(s,0xaf54)!=0)SU32(s,0xaf54,0);
   rx_addr p=U32(s,0x568);fx_set_reg(FX_EAX,p);
   uint32_t id=p?U16(p,0):0;fx_set_reg(FX_EAX,id);
   if((uint16_t)id!=U16(s,0x5ac)){
    p=U32(s,0x564);fx_set_reg(FX_EAX,p);if(p)SU8(p,0,1);
    p=U32(s,0x568);fx_set_reg(FX_EAX,p);
    if(p){uint32_t cx=(fx_reg(FX_ECX)&0xffff0000u)|U16(s,0x5ac);fx_set_reg(FX_ECX,cx);SU16(p,0,(uint16_t)cx);}
   }
  }else if(U32(s,0xaf54)==0){
   SU32(s,0xaf54,1);
   float v=0.0f,hi=F32(s,0x53c);
   if(0.0f>hi)v=phys_sub(F32(0,0x9774a8),hi);
   else{
    float lo=F32(s,0x538);
    if(lo>0.0f)v=phys_add(phys_sub(F32(0,0x9774a8),lo),hi);
   }
   const rx_addr local=f.E+8;fx_set_reg(FX_EDX,local);fx_set_reg(FX_EAX,s+0x548);SF32(local,0,v);
   listeners(s+0x548,local,0x61f08d);
   v=reset_blend(s);rx_addr p=U32(s,0x58c);fx_set_reg(FX_EAX,p);if(p)SF32(p,0,v);
   p=U32(s,0x564);fx_set_reg(FX_EAX,p);if(p)SU8(p,0,1);
   p=U32(s,0x568);fx_set_reg(FX_EAX,p);
   if(p){uint32_t cx=(fx_reg(FX_ECX)&0xffff0000u)|U16(s,0x5b6);fx_set_reg(FX_ECX,cx);SU16(p,0,(uint16_t)cx);}
  }
  result=1;
 }
 fx_set_reg(FX_EAX,result);fx_frame_leave(&f);return result;
}
RX_FUNCTION(0x0061efa0,rx_rider_override_select_pose)
void rx_rider_pose_override(rx_addr s,float dt){
 const uint32_t entryCx=fx_reg(FX_ECX);fx_frame f;fx_frame_flat(&f,4,FX_ESI);
 SU32(f.sp0-4,0,entryCx);SU32(f.sp0,4,rx_fb(dt));fx_set_reg(FX_ESI,s);
 SF32(s,0x384,dt);
 float target=U32(s,0x5bb38)==0?F32(s,0x5b528):0.0f;
 const float duration=F32(s,0x380);float step=F32(s,0x384);
 /* JA(duration,step) false on unordered -> duration selected. */
 if(!(duration>step))step=duration;
 const float previous=F32(s,0x388);
 float current=phys_add(phys_mul(phys_div(step,duration),phys_sub(target,previous)),previous);
 SF32(s,0x388,current);
 const float hi=F32(s,0x37c),lo=F32(s,0x378);
 /* JB(current,hi), JB(lo,current): unordered retains current. */
 if(current>=hi)current=hi;
 if(lo>=current)current=lo;
 const rx_addr local=f.E+4;fx_set_reg(FX_EAX,local);fx_set_reg(FX_ECX,s+0x38c);
 SF32(local,0,phys_div(phys_sub(current,lo),phys_sub(hi,lo)));
 listeners(s+0x38c,local,0x61c164);
 fx_set_reg(FX_EAX,s);const rx_addr before=fx_push(0,(const uint32_t*)0,0x61c16b);
 (void)rx_rider_override_select_pose(s);
 if(rx_sp()!=before-4)abort();rx_set_sp(before);
 /* Original reloads caller dt after listeners; retain observable alias behavior. */
 rx_addr p=U32(s,0x58c);fx_set_reg(FX_EAX,p);
 const float rise=phys_mul(F32(f.sp0,4),F32(0,0x92fe48));
 float value=p?F32(p,0):0.0f;float upper=F32(s,0x588),lower=F32(s,0x584);
 value=phys_add(value,rise);if(value>=upper)value=upper;if(lower>=value)value=lower;
 if(p)SF32(p,0,value);
 p=U32(s,0x598);fx_set_reg(FX_EAX,p);value=p?F32(p,0):0.0f;
 upper=F32(s,0x594);lower=F32(s,0x590);value=phys_add(value,rise);
 if(value>=upper)value=upper;if(lower>=value)value=lower;
 fx_set_reg(FX_ESI,p);if(p)SF32(p,0,value);
 fx_set_reg(FX_ECX,U32(f.sp0-4,0));fx_frame_leave(&f);
}
RX_FUNCTION(0x0061c0c0,rx_rider_pose_override)
