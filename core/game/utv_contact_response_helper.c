/* Complete handwritten004cea10. Ordered packed quaternion products and
 * actual wheel-local angular errors; no host physics or curve approximation. */
#include "utv_contact_response_helper.h"
#include "fx_util.h"
#include "phys_vec.h"
enum {REVERSE_X=1,REVERSE_Y=2,REVERSE_W=4,LANE_Z=8,X_THEN_W=16};
static rx_v4 product(rx_v4 a,rx_v4 b,unsigned order){
 rx_v4 r=v4_shuf(b,b,0x1b),x=order&REVERSE_X?v4_mul(r,v4_lane(a,0)):v4_mul(v4_lane(a,0),r);
 x=v4_mul(x,rx_ld4(0x93a1f0));r=v4_shuf(r,r,0xb1);
 rx_v4 y=order&REVERSE_Y?v4_mul(r,v4_lane(a,1)):v4_mul(v4_lane(a,1),r);y=v4_mul(y,rx_ld4(0x93a200));r=v4_shuf(r,r,0x1b);
 rx_v4 z=order&LANE_Z?v4_mul(v4_lane(a,2),r):v4_mul(r,v4_lane(a,2));z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=order&REVERSE_W?v4_mul(b,v4_lane(a,3)):v4_mul(v4_lane(a,3),b);
 return v4_add(v4_add(z,y),order&X_THEN_W?v4_add(x,w):v4_add(w,x));
}
static uint32_t mask_sum(rx_v4 m){return v4_lb(m,2)+v4_lb(m,1)+v4_lb(m,0);}
void rx_utv_contact_response_helper_exact(rx_addr s){
 fx_frame f;fx_frame_enter(&f,0xb8,16,FX_EBX,FX_ESI);rx_addr b=f.E;
 rx_st4(b+0x10,v4_splat(F32(0,0x92fd88)));rx_st4(b+0x30,v4_splat(F32(0,0x92e12c)));
 fx_set_reg(FX_EBX,0);SU32(s,0xa8,0);
 rx_v4 root=rx_ld4(s+0x730),rootAngular=rx_ld4(s+0x740);rx_st4(b+0xa0,rootAngular);
 rx_st4(b+0x40,v4_lane(root,2));rx_st4(b+0x20,v4_lane(root,3));rx_st4(b+0x90,v4_lane(root,1));
 rx_st4(b+0xb0,root);rx_st4(b+0x50,v4_lane(root,0));
 const uint32_t rotations[]={0x7d0,0x870,0x910,0x9b0},angular[]={0x7e0,0x880,0x920,0x9c0};
 const rx_addr curveRA[]={0x4cec82,0x4ceea1,0x4cf0bd,0x4cf2d4};
 for(unsigned wheel=0;wheel<4;wheel++){
  unsigned rootOrder=wheel==0?(REVERSE_X|REVERSE_Y|REVERSE_W|X_THEN_W):wheel==1?0:(REVERSE_X|REVERSE_Y|REVERSE_W);
  rx_v4 inner=product(v4_and(rx_ld4(b+0xa0),rx_ld4(0x92db80)),v4_mul(root,rx_ld4(0x93a220)),0);
  rx_v4 world=v4_and(product(root,inner,rootOrder),rx_ld4(0x92db80));
  rx_v4 rotation=rx_ld4(s+rotations[wheel]);inner=product(world,rotation,0);
  rx_v4 predicted=product(v4_mul(rotation,rx_ld4(0x93a220)),inner,wheel==3?LANE_Z:0);
  rx_v4 delta=v4_sub(rx_ld4(s+angular[wheel]),predicted),squared=v4_mul(delta,delta);
  rx_v4 length=v4_sqrt(v4_splat(phys_add(phys_add(squared.v[0],squared.v[2]),squared.v[1])));
  rx_st4(b+0x80,predicted);rx_st4(b+0x70,delta);int direct=0;
  /* Original COMISS/JBE: unordered or nonpositive gate takes the curve arm. */
  if(F32(s,wheel<2?0x24:0x28)>F32(0,0x92e12c)){
   rx_v4 mask=v4_cmplt(length,rx_ld4(b+0x10));rx_st4(b+(wheel==3?0x90:0x60),mask);
   uint32_t y=v4_lb(mask,1),sum=mask_sum(mask);
   if(wheel==0||wheel==3){fx_set_reg(FX_EAX,y);fx_set_reg(FX_ECX,sum);}
   else if(wheel==1){fx_set_reg(FX_EDX,y);fx_set_reg(FX_EAX,sum);}
   else{fx_set_reg(FX_ECX,y);fx_set_reg(FX_EDX,sum);}
   direct=sum==0xfffffffdu;
  }
  rx_v4 response;
  if(direct){
   /* Keep zero multiplication: NaN delta is not silently replaced by zero. */
   response=v4_add(v4_mul(rx_ld4(b+0x30),delta),predicted);
   if(!wheel)fx_set_reg(FX_EBX,fx_reg(FX_ECX)+4);else fx_set_reg(FX_EBX,fx_reg(FX_EBX)+1);
  }else{
   fx_set_reg(FX_ESI,s+0x3d2c);SU32(b-4,0,curveRA[wheel]);rx_set_sp(b-4);
   float factor=rx_utv_contact_response_curve(length.v[0],b);
   response=v4_add(v4_mul(v4_splat(factor),rx_ld4(b+0x70)),rx_ld4(b+0x80));
  }
  rx_st4(s+angular[wheel],response);
 }
 if(fx_reg(FX_EBX)==4)SU32(s,0xa8,1);
 fx_frame_leave(&f);
}
