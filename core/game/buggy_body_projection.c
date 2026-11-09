/* Complete original Buggy108B projection leaf, no alias/registration. The
 * canonical UTV owner remains unique; this family has different saved axes. */
#include "buggy_body_projection.h"
#include "fx_util.h"
#include "phys_vec.h"
void rx_buggy_project_body_positions_exact(rx_addr s){
 if(U32(s,0x90)){
  rx_v4 delta=v4_sub(rx_ld4(s+0xf0),rx_ld4(s+0x8690));
  rx_v4 axis=rx_ld4(s+0x8680),p=v4_mul(axis,delta);
  float sum=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);
  fx_set_reg(FX_EDX,0);rx_v4 correction=v4_sub(v4_mul(v4_splat(sum),axis),delta);
  if(I32(s,0xec)>0){
   fx_set_reg(FX_EAX,s+0xf0);
   do{
    rx_addr body=fx_reg(FX_EAX);rx_st4(body,v4_add(rx_ld4(body),correction));
    fx_set_reg(FX_EDX,fx_reg(FX_EDX)+1);fx_set_reg(FX_EAX,body+0xa0);
   }while((int32_t)fx_reg(FX_EDX)<I32(s,0xec));
  }
 }
}
