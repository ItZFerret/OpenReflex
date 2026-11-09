/* Complete3263B physical spans through both original RET paths. Four inline
 * filters and orientation paths preserve their original data/operation order;
 * they are not replacements by an approximate lag/quaternion policy. */
#include "fourwheel_filtered_contact.h"
#include "fx_util.h"
#include "phys_vec.h"
#include <stdlib.h>
static rx_v4 product(rx_v4 a,rx_v4 b){
 rx_v4 reverse=v4_shuf(b,b,0x1b),x=v4_mul(v4_lane(a,0),reverse);x=v4_mul(x,rx_ld4(0x93a1f0));
 reverse=v4_shuf(reverse,reverse,0xb1);rx_v4 y=v4_mul(v4_lane(a,1),reverse);y=v4_mul(y,rx_ld4(0x93a200));
 reverse=v4_shuf(reverse,reverse,0x1b);rx_v4 z=v4_mul(reverse,v4_lane(a,2));z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);return v4_add(v4_add(z,y),v4_add(w,x));
}
static rx_v4 normalized(rx_v4 q){
 rx_v4 sq=v4_mul(q,q);
 /* Original ee/40/f0/aa chain yields (W+Y)+(Z+X), in that operand order. */
 rx_v4 sum=v4_splat(phys_add(phys_add(sq.v[3],sq.v[1]),phys_add(sq.v[2],sq.v[0]))),root=v4_sqrt(sum);
 rx_v4 finite=v4_mask(sum.v[0]!=F32(0x92dc20,0),sum.v[1]!=F32(0x92dc20,4),sum.v[2]!=F32(0x92dc20,8),sum.v[3]!=F32(0x92dc20,12));
 rx_v4 nonzero=v4_mask(root.v[0]!=0,root.v[1]!=0,root.v[2]!=0,root.v[3]!=0);
 return v4_or(v4_and(v4_and(v4_div(q,root),nonzero),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static float axis(rx_addr p,rx_addr target,float tau,float dt,int coefficient_before_add,int defer_coefficient,float*coefficient){
 float selected=tau>dt?dt:tau,k=phys_div(selected,tau),old=F32(p,0);
 float delta=phys_mul(phys_sub(F32(target,0),old),k);
 if(coefficient_before_add&&!defer_coefficient)SF32(p,8,k);
 float value=phys_add(delta,old);
 if(!coefficient_before_add&&!defer_coefficient)SF32(p,8,k);
 SF32(p,0,value);*coefficient=k;return value;
}
static void publish(rx_addr s,unsigned index,rx_addr ra){
 rx_addr body=U32(s,0xd74+4*index);fx_set_reg(FX_ECX,body);fx_set_reg(FX_EDX,U32(body,0));fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX),0x18));
 rx_addr expected=fx_push(0,NULL,ra);rx_fourwheel_filtered_publish(body,ra);if(rx_sp()!=expected)abort();
}
static void filtered(rx_addr s,float dt,unsigned delta,rx_addr base){
 fx_frame f;fx_frame_enter0(&f,0x50,16);rx_addr b=f.E;
 float oldX=0,oldY=0,newX=0,newY=0;int override=0;
 static const unsigned positions[]={0x190,0x230,0x2d0,0x370},zslots[]={0x48,0x38,0x18};
 for(unsigned wheel=0;wheel<4;wheel++){
  unsigned fields=0x85d0-delta+12*wheel;rx_addr zp=U32(s,fields+8);fx_set_reg(FX_EDX,zp);float oldZ=F32(zp,0);
  rx_addr xp=U32(s,fields);fx_set_reg(FX_EAX,xp);float tauX=0;
  if(!wheel)tauX=F32(xp,4);
  rx_addr yp=U32(s,fields+4);fx_set_reg(FX_ECX,yp);
  if(!wheel){oldX=F32(xp,0);oldY=F32(yp,0);}
  SF32(b,wheel==3?0x28:0x18,oldZ);rx_st4(b,rx_ld4(s+positions[wheel]));
  if(wheel){
   float dx=phys_sub(newX,oldX);oldX=F32(xp,0);float dy=phys_sub(newY,oldY);oldY=F32(yp,0);tauX=F32(xp,4);
   const unsigned xy[]={0x40,0x30,0x10};SF32(b,xy[wheel-1],phys_div(dx,dt));SF32(b,xy[wheel-1]+4,phys_div(dy,dt));
  }
  float k;newX=axis(xp,b,tauX,dt,0,0,&k);
  yp=U32(s,fields+4);fx_set_reg(FX_EAX,yp);newY=axis(yp,b+4,F32(yp,4),dt,wheel!=3,0,&k);
  zp=U32(s,fields+8);fx_set_reg(FX_EAX,zp);float newZ=axis(zp,b+8,F32(zp,4),dt,1,wheel==3,&k);
  float dz=phys_sub(newZ,F32(b,wheel==3?0x28:0x18));
  if(wheel<3)SF32(b,zslots[wheel],phys_div(dz,dt));
  else{
   SF32(zp,8,k);override=U32(s,0x90)!=0;
   SF32(b,0,phys_div(phys_sub(newX,oldX),dt));SF32(b,4,phys_div(phys_sub(newY,oldY),dt));SF32(b,8,phys_div(dz,dt));
  }
 }
 if(!override)override=U32(s,0x2de0)&&U32(s,0x2de4)==2;
 if(!override){
  const unsigned active[]={0x8180,0x82d0,0x8420,0x8570},enabled[]={0x8140,0x8290,0x83e0,0x8530},normal[]={0x81a0,0x82f0,0x8440,0x8590},inertia[]={0x8090,0x81e0,0x8330,0x8480};
  const unsigned velocity[]={0x40,0x30,0x10,0},work[]={0x40,0x30,0x10,0x10};
  const unsigned return_offset[]={0x5bc,0x72a,0x898,0xa05};
  for(unsigned wheel=0;wheel<4;wheel++)if(U32(s,active[wheel]-delta)&&U32(s,enabled[wheel]-delta)){
   float projection=phys_hsum3(v4_mul(rx_ld4(b+velocity[wheel]),rx_ld4(s+normal[wheel]-delta)));
   float halfdt=phys_mul(dt,F32(0,0x9322cc));rx_st4(b+0x20,rx_ld4(0xd61660));
   rx_v4 omega=rx_ld4(s+0x7e0+0xa0*wheel);uint32_t w=U32(b,0x2c);fx_set_reg(wheel?FX_ECX:FX_EAX,w);
   rx_v4 quotient=v4_div(v4_mul(v4_splat(projection),rx_ld4(0xd615c0)),rx_ld4(s+inertia[wheel]-delta));
   rx_v4 initial=rx_ld4(s+0x850+0xa0*wheel);omega=v4_movss(omega,quotient);rx_st4(b+work[wheel],omega);SU32(b+work[wheel],12,w);
   rx_v4 updated=v4_add(v4_mul(product(initial,rx_ld4(b+work[wheel])),v4_splat(halfdt)),initial);
   rx_st4(s+0x7d0+0xa0*wheel,updated);rx_st4(s+0x7d0+0xa0*wheel,normalized(rx_ld4(s+0x7d0+0xa0*wheel)));
   publish(s,wheel,base+return_offset[wheel]);
  }
 }else{
  rx_st4(s+0x7d0,product(product(rx_ld4(s+0x730),rx_ld4(s+0x5630-delta)),rx_ld4(s+0x86c0-delta)));
  rx_st4(s+0x870,product(product(rx_ld4(s+0x730),rx_ld4(s+0x5620-delta)),rx_ld4(s+0x86e0-delta)));
  rx_st4(s+0x910,product(rx_ld4(s+0x730),rx_ld4(s+0x86b0-delta)));
  rx_st4(s+0x9b0,product(rx_ld4(s+0x730),rx_ld4(s+0x86d0-delta)));
  const unsigned ra[]={0xc92,0xc9f,0xcac,0xcb9};for(unsigned wheel=0;wheel<4;wheel++)publish(s,wheel,base+ra[wheel]);
 }
 fx_frame_leave(&f);
}
void rx_utv_truck_filtered_contact(rx_addr s,float dt){filtered(s,dt,0,0x4cf4c0);}
void rx_buggy_filtered_contact(rx_addr s,float dt){filtered(s,dt,0x10,0x4b43c0);}
