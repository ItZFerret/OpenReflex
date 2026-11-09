/* Handwritten complete451bf0, shared by authentic Buggy/Truck callsites.
 * Rear axle X synchronization is part of this algorithm, absent004cea10. */
#include "family_contact_response.h"
#include "fx_util.h"
#include "phys_vec.h"
enum {REVERSE_X=1,REVERSE_Y=2,REVERSE_W=4,X_THEN_W=8};
/* Optional spills retain already-computed intermediates, never recompute math. */
static rx_v4 product(rx_v4 a,rx_v4 q,unsigned order,rx_addr xs,rx_addr ys,rx_addr zs){
 rx_v4 r=v4_shuf(q,q,0x1b),x=order&REVERSE_X?v4_mul(r,v4_lane(a,0)):v4_mul(v4_lane(a,0),r);
 if(xs)rx_st4(xs,x);x=v4_mul(x,rx_ld4(0x93a1f0));r=v4_shuf(r,r,0xb1);
 rx_v4 y=order&REVERSE_Y?v4_mul(r,v4_lane(a,1)):v4_mul(v4_lane(a,1),r);
 if(ys)rx_st4(ys,y);y=v4_mul(y,rx_ld4(0x93a200));r=v4_shuf(r,r,0x1b);
 rx_v4 z=v4_mul(v4_mul(r,v4_lane(a,2)),rx_ld4(0x93a210));if(zs)rx_st4(zs,z);
 rx_v4 w=order&REVERSE_W?v4_mul(q,v4_lane(a,3)):v4_mul(v4_lane(a,3),q);
 return v4_add(v4_add(z,y),order&X_THEN_W?v4_add(x,w):v4_add(w,x));
}
static uint32_t sum_mask(rx_v4 m){return v4_lb(m,2)+v4_lb(m,1)+v4_lb(m,0);}
void rx_family_contact_response_helper_exact(rx_addr s){
 fx_frame frame;fx_frame_enter(&frame,0xd8,16,FX_EBX,FX_ESI);const rx_addr b=frame.E;
 rx_st4(b+0x10,v4_splat(F32(0,0x92fd88)));rx_st4(b+0x20,v4_splat(F32(0,0x92e12c)));
 fx_set_reg(FX_EBX,0);SU32(s,0xa8,0);
 const rx_v4 root=rx_ld4(s+0x730),omega=rx_ld4(s+0x740);
 rx_st4(b+0xb0,omega);rx_st4(b+0x30,v4_lane(root,2));rx_st4(b+0x50,v4_lane(root,0));
 rx_st4(b+0x40,v4_lane(root,1));rx_st4(b+0xa0,v4_lane(root,3));rx_st4(b+0xc0,root);
 const unsigned rotation[]={0x7d0,0x870,0x910,0x9b0},angular[]={0x7e0,0x880,0x920,0x9c0};
 const uint32_t returns[]={0x451ec2,0x45212f,0x4523ef,0x45261d};
 for(unsigned wheel=0;wheel<4;wheel++){
  if(wheel==2){
   /*452164..4521a8: original rightX+leftX order, runtime half,
    * MOVSS retaining each original Y/Z/W. No quaternion or axle alias. */
   rx_v4 left=rx_ld4(s+0x920),right=rx_ld4(s+0x9c0);
   rx_v4 mean=v4_mul(v4_add(v4_lane(right,0),v4_lane(left,0)),rx_ld4(0xd615d0));
   left.v[0]=right.v[0]=mean.v[0];rx_st4(s+0x920,left);rx_st4(s+0x9c0,right);
  }
  rx_v4 inverse=v4_mul(root,rx_ld4(0x93a220));
  if(wheel==1)rx_st4(b+0x60,inverse);else if(wheel==2)rx_st4(b+0x70,inverse);else if(wheel==3)rx_st4(b+0xd0,inverse);
  rx_v4 inner=product(v4_and(omega,rx_ld4(0x92db80)),inverse,0,wheel==1?b+0x70:wheel==2?b+0x60:0,0,0);
  unsigned root_order=wheel==0?(REVERSE_X|REVERSE_Y|X_THEN_W):wheel<3?(REVERSE_X|REVERSE_Y|REVERSE_W|X_THEN_W):0;
  rx_v4 world=v4_and(product(root,inner,root_order,wheel==0?b+0x60:wheel<3?b+0x70:0,0,0),rx_ld4(0x92db80));
  rx_v4 q=rx_ld4(s+rotation[wheel]);if(wheel==0)rx_st4(b+0x80,q);else if(wheel<3)rx_st4(b+0x60,q);
  inner=product(world,q,0,wheel==0?b+0x60:wheel<3?b+0x70:0,0,0);
  rx_v4 predicted=product(v4_mul(q,rx_ld4(0x93a220)),inner,0,wheel==0?b+0x90:wheel==1?b+0x60:wheel==2?b+0xd0:0,wheel<3?(wheel==0?b+0x60:b+0x70):0,wheel==0?b+0x80:wheel==2?b+0x60:0);
  rx_v4 delta=v4_sub(rx_ld4(s+angular[wheel]),predicted),square=v4_mul(delta,delta);
  rx_st4(b+0x80,predicted);rx_st4(b+0x90,delta);
  rx_v4 length=v4_sqrt(v4_splat(phys_add(phys_add(square.v[0],square.v[2]),square.v[1])));int direct=0;
  if(F32(s,wheel<2?0x24:0x28)>F32(0,0x92e12c)){
   rx_v4 mask=v4_cmplt(length,rx_ld4(b+0x10));rx_st4(b+(wheel==3?0xa0:0x60),mask);
   uint32_t y=v4_lb(mask,1),sum=sum_mask(mask);
   if(wheel==0||wheel==3){fx_set_reg(FX_EAX,y);fx_set_reg(FX_ECX,sum);}
   else if(wheel==1){fx_set_reg(FX_EDX,y);fx_set_reg(FX_EAX,sum);}
   else{fx_set_reg(FX_ECX,y);fx_set_reg(FX_EDX,sum);}
   direct=sum==0xfffffffdu;
  }
  rx_v4 response;
  if(direct){response=v4_add(v4_mul(delta,rx_ld4(b+0x20)),predicted);fx_set_reg(FX_EBX,wheel?fx_reg(FX_EBX)+1:fx_reg(FX_ECX)+4);}
  else{
   fx_set_reg(FX_ESI,s+0x3d2c);SU32(b-4,0,returns[wheel]);rx_set_sp(b-4);
   float gain=rx_family_contact_response_curve(length.v[0],b);
   response=v4_add(v4_mul(v4_splat(gain),rx_ld4(b+0x90)),rx_ld4(b+0x80));
  }
  rx_st4(s+angular[wheel],response);
 }
 if(fx_reg(FX_EBX)==4)SU32(s,0xa8,1);fx_frame_leave(&frame);
}
