/* Handwritten family revision. Shared spring/damper algorithm, explicit
 * original operand order, family data fields, dead-frame slots and call sites.
 * Accepted UTV source remains untouched; no activation or generated C. */
#include "fourwheel_suspension.h"
#include "phys_vec.h"
#include "fx_util.h"
enum ProductOrder {X_REVERSED=1,Y_REVERSED=2,Z_LANE_FIRST=4,W_REVERSED=8,X_MASK_FIRST=16,X_PLUS_W=32};
typedef struct {
 unsigned family,delta,sub,cookie,rootVelocity,rootPosition,rootQuaternion,axis,arm,wheelVelocity,zero,omega,applied,opposite;
 unsigned rootX,rootY,rootZ,rootW,bareOmegaX,firstDisplacement,axisOrder,omegaOrder;
 unsigned gain[4],rebound[4],compression[4],limit[4],negative[4];
 rx_addr wheelRA[4],rootRA[4],cookieRA;
} SuspensionLayout;
static const SuspensionLayout utv={
 0,0,0x108,0x10c,0xd0,0xe0,0xa0,0xf0,0xc0,0xb0,0x20,0x80,0x90,0x70,
 0,0x70,0x80,0x30,0x60,0x60,0,X_PLUS_W,
 {0,0x10,0x40,0x40},{0x10,0x40,0x10,0x10},{0x90,0x50,0x50,0x50},{0x70,0x60,0x60,0xa0},{0x70,0x60,0x60,0x80},
 {0x4d05f4,0x4d0866,0x4d0ad8,0x4d0d5c},{0x4d0605,0x4d0877,0x4d0ae9,0x4d0d6d},0x4d0d7d};
static const SuspensionLayout buggy={
 1,0x10,0x108,0x10c,0xd0,0xe0,0xa0,0xf0,0xc0,0xb0,0x20,0x80,0x90,0x70,
 0,0x70,0x80,0x30,0x60,0x60,X_PLUS_W,X_PLUS_W,
 {0,0x10,0x40,0x40},{0x10,0x40,0x10,0x10},{0x90,0x50,0x50,0x50},{0x70,0x60,0x60,0xa0},{0x70,0x60,0x60,0x80},
 {0x4b5e44,0x4b60b9,0x4b632b,0x4b65af},{0x4b5e55,0x4b60ca,0x4b633c,0x4b65c0},0x4b65d0};
static const SuspensionLayout truck={
 2,0,0x118,0x11c,0xe0,0xf0,0xc0,0x100,0xb0,0xd0,0x10,0x90,0xa0,0x80,
 0x20,0x70,0x80,0x90,0,0x70,X_REVERSED|Y_REVERSED|W_REVERSED,X_REVERSED|Y_REVERSED|W_REVERSED,
 {0,0x50,0x30,0x30},{0x60,0x30,0x50,0x50},{0xa0,0x40,0x40,0x40},{0x80,0x70,0x70,0xc0},{0x80,0x70,0x70,0x90},
 {0x453433,0x4536b4,0x45392c,0x453bb6},{0x453447,0x4536c8,0x453940,0x453bca},0x453bda};
static rx_v4 product(rx_v4 a,rx_v4 b,unsigned order,rx_addr bareX){
 rx_v4 shuffle=v4_shuf(b,b,0x1b),lane=v4_lane(a,0);
 rx_v4 x=order&X_REVERSED?v4_mul(shuffle,lane):v4_mul(lane,shuffle);
 if(bareX)rx_st4(bareX,x);
 x=order&X_MASK_FIRST?v4_mul(rx_ld4(0x93a1f0),x):v4_mul(x,rx_ld4(0x93a1f0));
 shuffle=v4_shuf(shuffle,shuffle,0xb1);lane=v4_lane(a,1);
 rx_v4 y=order&Y_REVERSED?v4_mul(shuffle,lane):v4_mul(lane,shuffle);y=v4_mul(y,rx_ld4(0x93a200));
 shuffle=v4_shuf(shuffle,shuffle,0x1b);lane=v4_lane(a,2);
 rx_v4 z=order&Z_LANE_FIRST?v4_mul(lane,shuffle):v4_mul(shuffle,lane);z=v4_mul(z,rx_ld4(0x93a210));
 lane=v4_lane(a,3);rx_v4 w=order&W_REVERSED?v4_mul(b,lane):v4_mul(lane,b);
 return v4_add(v4_add(z,y),order&X_PLUS_W?v4_add(x,w):v4_add(w,x));
}
static rx_v4 length021(rx_v4 v){rx_v4 q=v4_mul(v,v);return v4_splat(phys_sqrt(phys_add(phys_add(q.v[0],q.v[2]),q.v[1])));}
static uint32_t mask_sum(rx_v4 a){return v4_lb(a,2)+v4_lb(a,1)+v4_lb(a,0);}
static void force(rx_addr b,rx_addr value,rx_addr point,uint32_t node,rx_addr ra,const SuspensionLayout*l){
 fx_set_reg(FX_ECX,value);fx_set_reg(FX_ESI,point);SU32(b-4,0,node);SU32(b-8,0,ra);rx_set_sp(b-8);rx_family_suspension_dependency(l->family,b);
}
static void suspension(rx_addr s,const SuspensionLayout*l){
 fx_frame frame;fx_frame_enter(&frame,l->sub,16,FX_EBX,FX_ESI);rx_addr b=frame.E;
 uint32_t cookie=U32(0,0x9b45e4)^(b+8);fx_set_reg(FX_EAX,cookie);SU32(b,l->cookie,cookie);
 rx_v4 root=rx_ld4(s+0x730),conjugate=v4_mul(root,rx_ld4(0x93a220));
 rx_st4(b+l->rootVelocity,rx_ld4(s+0x100));rx_st4(b+l->rootPosition,rx_ld4(s+0xf0));
 rx_v4 axis=product(root,product(v4_and(rx_ld4(0x92db80),rx_ld4(0xd616a0)),conjugate,0,0),l->axisOrder,0);
 if(l->rootX)rx_st4(b+l->rootX,v4_lane(root,0));
 rx_st4(b+l->rootY,v4_lane(root,1));rx_st4(b+l->rootZ,v4_lane(root,2));rx_st4(b+l->rootW,v4_lane(root,3));
 rx_v4 omega_first=product(v4_and(rx_ld4(s+0x740),rx_ld4(0x92db80)),conjugate,0,l->bareOmegaX?b+l->bareOmegaX:0);
 rx_st4(b+l->rootQuaternion,root);rx_st4(b+l->axis,axis);
 rx_v4 omega=product(root,omega_first,l->omegaOrder,0),omega_c9=v4_shuf(omega,omega,0xc9);
 rx_st4(b+l->rebound[0],v4_splat(F32(s,0x8650-l->delta)));rx_st4(b+l->compression[0],v4_splat(F32(s,0x8654-l->delta)));
 fx_set_reg(FX_EBX,0);const uint32_t markers[]={0x8188,0x82d8,0x8428,0x8578};for(unsigned i=0;i<4;i++)SU32(s,markers[i]-l->delta,0);
 const uint32_t position[]={0x190,0x230,0x2d0,0x370},velocity[]={0x1a0,0x240,0x2e0,0x380},anchor[]={0x5b10,0x6470,0x6dd0,0x7730};
 for(unsigned side=0;side<4;side++){
  if(side){
   /* Truck alone spills front rest length in B60 before loading three gains. */
   if(l->family==2&&side==1)rx_st4(b+0x60,v4_splat(F32(s,0x3dcc)));
   unsigned base=side==1?0x864c:0x8640;
   rx_st4(b+l->gain[side],v4_splat(F32(s,base-l->delta)));
   rx_st4(b+l->rebound[side],v4_splat(F32(s,base+4-l->delta)));
   rx_st4(b+l->compression[side],v4_splat(F32(s,base+8-l->delta)));
  }
  rx_st4(b+l->wheelVelocity,rx_ld4(s+velocity[side]));rx_v4 displacement=v4_sub(rx_ld4(s+position[side]),rx_ld4(b+l->rootPosition));
  if(!side)rx_st4(b+l->firstDisplacement,displacement);
  unsigned innerOrder=l->family==2&&side==1?Z_LANE_FIRST:0;
  unsigned outerOrder=l->family==1&&side==1?Z_LANE_FIRST:l->family==2&&side==1?X_MASK_FIRST:0;
  rx_v4 local_first=product(v4_and(displacement,rx_ld4(0x92db80)),root,innerOrder,0);
  if(!side)rx_st4(b+(l->family==2?0x20:l->rootW),v4_shuf(root,root,0x1b));
  rx_v4 arm=product(v4_mul(root,rx_ld4(0x93a220)),local_first,outerOrder,0);rx_st4(b+l->arm,arm);
  rx_v4 length=length021(v4_sub(arm,rx_ld4(s+anchor[side]-l->delta)));
  rx_v4 limit=v4_cmplt(length,v4_splat(F32(s,side<2?0x3e5c:0x4194)));rx_st4(b+l->limit[side],limit);
  if(!side){fx_set_reg(FX_EAX,v4_lb(limit,1));fx_set_reg(FX_ECX,mask_sum(limit));}
  else{fx_set_reg(FX_ECX,v4_lb(limit,1));fx_set_reg(FX_EAX,mask_sum(limit));}
  if(mask_sum(limit)==0xfffffffdu)SU32(s,markers[side]-l->delta,1);
  rx_v4 spring=v4_sub(length,v4_splat(F32(s,side<2?0x3dcc:0x402c)));
  rx_v4 gain=side?rx_ld4(b+l->gain[side]):v4_splat(F32(s,0x864c-l->delta));spring=v4_mul(v4_mul(spring,gain),axis);
  rx_v4 r1=v4_shuf(displacement,displacement,0xd2),r2=v4_shuf(r1,r1,0xd2),w2=v4_shuf(omega_c9,omega_c9,0xc9);
  rx_v4 cross=v4_and(v4_sub(v4_mul(r1,omega_c9),v4_mul(r2,w2)),rx_ld4(0x92db80));
  rx_v4 relative=v4_mul(v4_sub(rx_ld4(b+l->wheelVelocity),v4_add(cross,rx_ld4(b+l->rootVelocity))),axis);
  float dot=phys_add(phys_add(relative.v[0],relative.v[1]),relative.v[2]);rx_v4 projected=v4_splat(dot);
  rx_v4 negative=v4_cmplt(projected,rx_ld4(0xd61670));
  if(!side){rx_st4(b+l->omega,omega_c9);rx_st4(b+l->negative[side],negative);fx_set_reg(FX_EDX,v4_lb(negative,1));fx_set_reg(FX_EAX,mask_sum(negative));rx_st4(b+l->zero,v4_zero());}
  else{rx_st4(b+l->negative[side],negative);fx_set_reg(FX_ECX,v4_lb(negative,1));fx_set_reg(FX_EAX,mask_sum(negative));}
  rx_v4 damping=v4_sub(rx_ld4(b+l->zero),projected);
  rx_addr damper=b+(mask_sum(negative)==0xfffffffdu?l->compression[side]:l->rebound[side]);
  damping=v4_mul(v4_mul(damping,rx_ld4(damper)),axis);
  rx_v4 applied=v4_add(damping,spring),opposite=v4_sub(rx_ld4(b+l->zero),applied);
  rx_st4(b+l->applied,applied);rx_st4(b+l->opposite,opposite);
  if(!side)fx_set_reg(FX_EDX,s+0xd0);
  force(b,b+l->applied,0xd61670,side+1,l->wheelRA[side],l);force(b,b+l->opposite,b+l->arm,0,l->rootRA[side],l);
 }
 fx_set_reg(FX_ECX,U32(b,l->cookie)^(b+8));fx_set_reg(FX_ESI,frame.in.esi);fx_set_reg(FX_EBX,frame.in.ebx);
 SU32(b+4,0,l->cookieRA);rx_set_sp(b+4);rx_family_suspension_dependency(l->family,b+8);fx_frame_leave(&frame);
}
void rx_utv_suspension_step_exact(rx_addr s){suspension(s,&utv);}
void rx_buggy_suspension_step_exact(rx_addr s){suspension(s,&buggy);}
void rx_truck_suspension_step_exact(rx_addr s){suspension(s,&truck);}
