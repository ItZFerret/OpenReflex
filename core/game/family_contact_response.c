/* Explicit Buggy/Truck family revision; both complete3003B bodies. Exact shared owners
 * stay behind finite private roles; this body does not duplicate their math. */
#include "family_contact_response.h"
#include "fx_util.h"
#include "phys_vec.h"
/* Packed quaternion product. Each reversed scalar/vector MULPS is explicit
 * because original NaN/signed-zero operand preference must survive. */
static rx_v4 product(rx_v4 a,rx_v4 q,unsigned reverse,rx_addr xspill){
 rx_v4 r=v4_shuf(q,q,0x1b),ax=v4_lane(a,0),ay=v4_lane(a,1),az=v4_lane(a,2),aw=v4_lane(a,3);
 rx_v4 x=reverse&1?v4_mul(r,ax):v4_mul(ax,r);if(xspill)rx_st4(xspill,x);x=v4_mul(x,rx_ld4(0x93a1f0));
 r=v4_shuf(r,r,0xb1);rx_v4 y=reverse&2?v4_mul(r,ay):v4_mul(ay,r);y=v4_mul(y,rx_ld4(0x93a200));
 r=v4_shuf(r,r,0x1b);rx_v4 z=reverse&4?v4_mul(az,r):v4_mul(r,az);z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=reverse&8?v4_mul(q,aw):v4_mul(aw,q);
 return v4_add(v4_add(z,y),reverse&16?v4_add(x,w):v4_add(w,x));
}
static rx_v4 normalize(rx_v4 v,int quaternion){
 rx_v4 p=v4_mul(v,v);float sum=quaternion?phys_add(phys_add(p.v[3],p.v[1]),phys_add(p.v[2],p.v[0])):phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);
 rx_v4 sums=v4_splat(sum),root=v4_sqrt(sums),div=v4_div(v,root),inf=rx_ld4(0x92dc20);
 rx_v4 finite=v4_mask(sum!=inf.v[0],sum!=inf.v[1],sum!=inf.v[2],sum!=inf.v[3]);
 rx_v4 nonzero=v4_mask(root.v[0]!=0,root.v[1]!=0,root.v[2]!=0,root.v[3]!=0);
 return v4_or(v4_and(v4_and(nonzero,div),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static void call(enum rx_family_response_role role,rx_addr entry,rx_addr expected,rx_addr ra){SU32(entry,0,ra);rx_set_sp(entry);rx_family_contact_response_dependency(role,expected);}
typedef struct {rx_addr entry,anchor,relative_first,relative_second,relative_third,relative_fourth,normal;unsigned bind_first,bind_second;unsigned contact0,contact1,contact2,contact3;} ResponseLayout;
/* Only these exact fifteen instructions differ in Buggy's data operands.
 * Other simulation fields are not globally shifted or inferred aliases. */
static const ResponseLayout buggy={0x4b3520,0x8690,0x86b0,0x86d0,0x86a0,0x86c0,0x8680,0x5620,0x5610,0x8440,0x81a0,0x8590,0x82f0};
static const ResponseLayout truck={0x451030,0x86a0,0x86c0,0x86e0,0x86b0,0x86d0,0x8690,0x5630,0x5620,0x8450,0x81b0,0x85a0,0x8300};
static void bind_relative_poses(rx_addr s,const ResponseLayout*l){
 rx_v4 conjugate=rx_ld4(0x93a220),root=rx_ld4(s+0x730),rootInverse=v4_mul(root,conjugate);
 rx_v4 first=product(v4_mul(rx_ld4(s+l->bind_first),conjugate),rootInverse,0,0);
 rx_st4(s+l->relative_first,normalize(product(first,rx_ld4(s+0x7d0),4,0),1));
 rx_v4 second=product(v4_mul(rx_ld4(s+l->bind_second),conjugate),v4_mul(root,conjugate),0,0);
 rx_st4(s+l->relative_second,normalize(product(second,rx_ld4(s+0x870),0,0),1));
 rx_st4(s+l->relative_third,normalize(product(v4_mul(root,conjugate),rx_ld4(s+0x910),0,0),1));
 rx_st4(s+l->relative_fourth,normalize(product(v4_mul(root,conjugate),rx_ld4(s+0x9b0),0,0),1));
 rx_v4 normal=v4_add(rx_ld4(s+l->contact0),rx_ld4(s+l->contact1));normal=v4_add(normal,rx_ld4(s+l->contact2));normal=v4_add(normal,rx_ld4(s+l->contact3));
 rx_st4(s+l->normal,normal);rx_st4(s+l->normal,normalize(rx_ld4(s+l->normal),0));
}
static void propagate_angular(rx_addr s,rx_addr b){
 rx_v4 root=rx_ld4(s+0x730),angular=rx_ld4(s+0x740),mask=rx_ld4(0x92db80),conjugate=rx_ld4(0x93a220);
 /* Original rootsplats cached while first wheelq occupies B40. */
 rx_st4(b+0x30,v4_lane(root,1));rx_st4(b+0x20,v4_lane(root,2));rx_st4(b+0x10,v4_lane(root,3));rx_st4(b+0x40,rx_ld4(s+0x7d0));
 for(unsigned i=0;i<4;i++){
  rx_v4 inverse=v4_mul(root,conjugate);if(i==1||i==2)rx_st4(b+0x40,inverse);
  rx_v4 inner=product(v4_and(angular,mask),inverse,0,0);
  rx_v4 world=product(root,inner,(i==0||i==3)?27:0,(i==1||i==2)?b+0x40:0);
  world=v4_and(world,mask);
  rx_v4 wheel=rx_ld4(s+0x7d0+0xa0*i);
  rx_v4 local=product(world,wheel,0,(i==1||i==2)?b+0x40:0);
  rx_v4 result=product(v4_mul(wheel,conjugate),local,0,i<3?b+0x40:0);
  rx_st4(s+0x7e0+0xa0*i,result);
 }
}
static void response(rx_addr s,float dt,const ResponseLayout*l){
 fx_frame f;fx_frame_enter(&f,0x44,16,FX_EBX,FX_ESI,FX_EDI);rx_addr b=f.E;fx_set_reg(FX_EDI,s);
 call(RX_FR_SHARED,b-4,b,l->entry+0x13);
 SU32(b-4,0,s);call(RX_FR_CLAMP,b-8,b,l->entry+0x19);
 fx_set_reg(FX_EBX,s+0xd0);SU32(b-4,0,fx_reg(FX_EBX));call(RX_FR_NODES,b-8,b,l->entry+0x25);
 call(RX_FR_HELPER,b-4,b,l->entry+0x2a);
 if(U32(s,0x2de0))SU32(s,0x90,0);
 else{
  fx_set_reg(FX_EAX,U32(s,0));fx_set_reg(FX_ECX,s);
  if(U32(s,0x90)){
   fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0x68));call(RX_FR_LEAVE,b-4,b,l->entry+0x54);
   if(fx_reg(FX_EAX))SU32(s,0x90,0);
  }else{
   fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0x64));call(RX_FR_ENTER,b-4,b,l->entry+0x70);
   if(fx_reg(FX_EAX)){
    fx_set_reg(FX_ESI,0xd61670);fx_set_reg(FX_EDX,fx_reg(FX_EBX));SU32(s,0x90,1);call(RX_FR_VELOCITY,b-4,b,l->entry+0x8e);
    rx_st4(s+l->anchor,rx_ld4(s+0xf0));bind_relative_poses(s,l);
   }
  }
  if(!U32(s,0x90)){fx_frame_leave(&f);return;}
  rx_v4 axis=rx_ld4(s+l->normal),p=v4_mul(rx_ld4(s+0x100),axis);
  float dot=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);rx_st4(s+0x100,v4_mul(v4_splat(dot),axis));
  rx_v4 omega=rx_ld4(s+0x740),z=rx_ld4(0xd61670);rx_v4 result=v4_shuf(z,omega,0xe5);result.v[0]=omega.v[0];rx_st4(s+0x740,result);
 }
 if(U32(s,0x90))propagate_angular(s,b);
 fx_frame_leave(&f);(void)dt;
}

void rx_buggy_contact_response_exact(rx_addr s,float dt){response(s,dt,&buggy);}
void rx_truck_contact_response_exact(rx_addr s,float dt){response(s,dt,&truck);}
