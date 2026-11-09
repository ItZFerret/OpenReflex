/* Complete hand-C control flow for2274B mode-active solve004d0d90/004b65e0.
 * Private dependency implementation/proof is explicitly separate. No native
 * address registration, unported return value or host quaternion shortcut. */
#include "fourwheel_active_solve.h"
#include "fx_util.h"
#include "phys_vec.h"
typedef struct{rx_addr entry;uint32_t controls,target,modifier;} ActiveLayout;
static const ActiveLayout utv_truck={0x4d0d90,0x56e0,0x86f0,0x8704},buggy={0x4b65e0,0x56d0,0x86e0,0x86f4};
static rx_addr site(const ActiveLayout*l,rx_addr original){return l->entry+(original-0x4d0d90);}
static rx_v4 product(rx_v4 a,rx_v4 b){
 rx_v4 rev=v4_shuf(b,b,0x1b),x=v4_mul(v4_lane(a,0),rev);x=v4_mul(x,rx_ld4(0x93a1f0));
 rev=v4_shuf(rev,rev,0xb1);rx_v4 y=v4_mul(v4_lane(a,1),rev);y=v4_mul(y,rx_ld4(0x93a200));
 rev=v4_shuf(rev,rev,0x1b);rx_v4 z=v4_mul(rev,v4_lane(a,2));z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),b);return v4_add(v4_add(z,y),v4_add(w,x));
}
static rx_v4 cross_shuffled(rx_v4 a,rx_v4 b){
 rx_v4 az=v4_shuf(a,a,0xd2),ay=v4_shuf(az,az,0xd2),by=v4_shuf(b,b,0xc9),bz=v4_shuf(by,by,0xc9);
 return v4_and(v4_sub(v4_mul(az,by),v4_mul(ay,bz)),rx_ld4(0x92db80));
}
static rx_v4 normalize(rx_v4 value,int four,rx_v4 zero){
 rx_v4 p=v4_mul(value,value);float sum=four?phys_add(phys_add(p.v[3],p.v[1]),phys_add(p.v[2],p.v[0])):phys_hsum3(p);
 rx_v4 sums=v4_splat(sum),length=v4_sqrt(sums);
 rx_v4 finite=v4_mask(sum!=F32(0x92dc20,0),sum!=F32(0x92dc20,4),sum!=F32(0x92dc20,8),sum!=F32(0x92dc20,12));
 rx_v4 nonzero=v4_mask(length.v[0]!=zero.v[0],length.v[1]!=zero.v[1],length.v[2]!=zero.v[2],length.v[3]!=zero.v[3]);
 return v4_or(v4_and(v4_and(nonzero,v4_div(value,length)),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static void mask_gprs(rx_addr b,rx_v4 mask){rx_st4(b+0x60,mask);fx_set_reg(FX_EAX,U32(b,0x64));fx_set_reg(FX_ECX,U32(b,0x68)+U32(b,0x64)+U32(b,0x60));}
static void lahf_compare(float a,float c){
 uint32_t ah=2;if(a!=a||c!=c)ah|=0x45;else if(a<c)ah|=1;else if(a==c)ah|=0x40;
 fx_set_reg(FX_EAX,(fx_reg(FX_EAX)&0xffff00ffu)|(ah<<8));
}
static float sqrt_argument(rx_addr b,const ActiveLayout*l,rx_addr source,rx_addr return_site){
 fx_x87_copy(b-4,source);SU32(b-8,0,site(l,return_site));rx_set_sp(b-8);
 return rx_fourwheel_active_dependency(0x42c230,b-4,v4_zero(),v4_zero(),v4_zero()).v[0];
}
static float abs_call(rx_addr b,const ActiveLayout*l,float value,rx_addr return_site){
 SU32(b-4,0,site(l,return_site));rx_set_sp(b-4);
 return rx_fourwheel_active_dependency(0x401000,b,v4(value,0,0,0),v4_zero(),v4_zero()).v[0];
}
static void matrix_rotation(rx_addr b,rx_addr s,const ActiveLayout*l,rx_v4 forward){
 rx_v4 perpendicular=normalize(cross_shuffled(forward,rx_ld4(0xd616a0)),0,v4_zero());
 rx_v4 up=normalize(cross_shuffled(perpendicular,forward),0,v4_zero());
 fx_set_reg(FX_EDX,b+0x10);SU32(b-4,0,b+0x10);fx_set_reg(FX_EAX,b+0x40);fx_set_reg(FX_ECX,b+0x60);
 rx_st4(b+0x40,forward);rx_st4(b+0x60,up);SU32(b-8,0,site(l,0x4d1025));rx_set_sp(b-8);
 (void)rx_fourwheel_active_dependency(0x401630,b-4,v4_zero(),v4_zero(),v4_zero());rx_set_sp(b);
 float x=F32(b,0x10),y=F32(b,0x14),z=F32(b,0x18);
 float square=phys_add(phys_add(phys_mul(y,y),phys_mul(x,x)),phys_mul(z,z)),one=F32(0x931f24,0);SF32(b,0x3c,square);
 if(square>F32(0x92e12c,0)){
  float length=sqrt_argument(b,l,b+0x3c,0x4d1078),reciprocal=phys_div(one,length);x=phys_mul(reciprocal,F32(b,0x10));
  y=phys_mul(reciprocal,F32(b,0x14));z=phys_mul(reciprocal,F32(b,0x18));rx_set_sp(b);SF32(b,0x14,y);SF32(b,0x18,z);
 }
 float up_y=F32(b,0x64),forward_z=F32(b,0x48),quarter=F32(0x92ff84,0);
 float xplus=phys_add(x,one),xminus=phys_sub(one,x);
 float qw=phys_mul(phys_add(phys_add(xplus,up_y),forward_z),quarter);
 float qx=phys_mul(phys_sub(phys_sub(xplus,up_y),forward_z),quarter);
 float qy=phys_mul(phys_sub(phys_add(xminus,up_y),forward_z),quarter);
 float qz=phys_mul(phys_add(phys_sub(xminus,up_y),forward_z),quarter);
 SF32(b,0x3c,qw);SF32(b,0x58,qx);SF32(b,0x5c,qy);SF32(b,0x38,qz);
 float maximum=qx>qy?qx:qy;maximum=maximum>qz?maximum:qz;maximum=maximum>qw?maximum:qw;lahf_compare(maximum,qw);
 if(maximum==qw){
  float length=sqrt_argument(b,l,b+0x3c,0x4d1151),scale=phys_div(quarter,length);SF32(b,0x2c,length);
  SF32(b,0x20,phys_mul(phys_sub(F32(b,0x68),F32(b,0x44)),scale));
  SF32(b,0x24,phys_mul(phys_sub(F32(b,0x40),F32(b,0x18)),scale));SF32(b,0x28,phys_mul(phys_sub(F32(b,0x14),F32(b,0x60)),scale));
 }else{
  maximum=qy>qz?qy:qz;maximum=maximum>qx?maximum:qx;lahf_compare(maximum,qx);
  /* The real PUSH ECX precedes this dominant-axis branch. Every branch then
   * overwrites its entire word through genuine FLD/FSTP before Sqrt. */
  SU32(b-4,0,fx_reg(FX_ECX));rx_set_sp(b-4);
  if(maximum==qx){
   float length=sqrt_argument(b,l,b+0x58,0x4d11d8),scale=phys_div(quarter,length);SF32(b,0x20,length);
   SF32(b,0x2c,phys_mul(phys_sub(F32(b,0x68),F32(b,0x44)),scale));
   SF32(b,0x24,phys_mul(phys_add(F32(b,0x60),F32(b,0x14)),scale));SF32(b,0x28,phys_mul(phys_add(F32(b,0x40),F32(b,0x18)),scale));
  }else if(qy>qz){
   float length=sqrt_argument(b,l,b+0x5c,0x4d1245),scale=phys_div(quarter,length);SF32(b,0x24,length);
   SF32(b,0x2c,phys_mul(phys_sub(F32(b,0x40),F32(b,0x18)),scale));
   SF32(b,0x20,phys_mul(phys_add(F32(b,0x60),F32(b,0x14)),scale));SF32(b,0x28,phys_mul(phys_add(F32(b,0x44),F32(b,0x68)),scale));
  }else{
   float length=sqrt_argument(b,l,b+0x38,0x4d12aa),scale=phys_div(quarter,length);SF32(b,0x28,length);
   SF32(b,0x2c,phys_mul(phys_sub(F32(b,0x14),F32(b,0x60)),scale));
   SF32(b,0x20,phys_mul(phys_add(F32(b,0x40),F32(b,0x18)),scale));SF32(b,0x24,phys_mul(phys_add(F32(b,0x44),F32(b,0x68)),scale));
  }
 }
 rx_st4(s+l->target,rx_ld4(b+0x20));rx_set_sp(b);
}
static void active_solve(rx_addr s,float dt,const ActiveLayout*l){
 fx_frame frame;fx_frame_enter(&frame,0x9c,16,FX_ESI);rx_addr b=frame.E;uint32_t cookie=U32(0,0x9b45e4)^(b+4);fx_set_reg(FX_EAX,cookie);SU32(b,0x9c,cookie);
 rx_st4(b+0x80,rx_ld4(s+0x740));fx_set_reg(FX_EDX,s+0x100);
 rx_v4 lateral=normalize(cross_shuffled(rx_ld4(s+0x100),rx_ld4(0xd616a0)),0,v4_zero());
 lateral=v4_mul(lateral,v4_splat(phys_mul(F32(s,0x526c),F32(s,l->controls))));rx_st4(b+0x70,v4_zero());
 lateral=v4_mul(lateral,rx_ld4(s+0x120));lateral=v4_mul(lateral,v4_splat(dt));fx_set_reg(FX_ECX,0);
 int32_t count=I32(s,0xec);if(count>0){rx_addr velocity=s+0x100;fx_set_reg(FX_EAX,velocity);int32_t i=0;
  do{rx_st4(velocity,v4_add(rx_ld4(velocity),lateral));++i;velocity+=160;fx_set_reg(FX_ECX,(uint32_t)i);fx_set_reg(FX_EAX,velocity);}while(i<I32(s,0xec));
 }
 rx_v4 forward=normalize(rx_ld4(s+0x100),0,v4_zero()),vertical=v4_lane(forward,1),square=v4_mul(vertical,vertical);
 float vertical_length=phys_sqrt(phys_add(phys_add(square.v[0],square.v[2]),square.v[1]));
 mask_gprs(b,v4_cmplt(v4_splat(vertical_length),v4_splat(F32(0x9d7894,0))));
 if(fx_reg(FX_ECX)==0xfffffffdu)matrix_rotation(b,s,l,forward);
 rx_v4 q=rx_ld4(s+0x730),inverse=v4_mul(q,rx_ld4(0x93a220));
 rx_v4 up=product(q,product(v4_and(rx_ld4(0x92db80),rx_ld4(0xd616a0)),inverse));
 mask_gprs(b,v4_cmplt(rx_ld4(0xd61670),v4_lane(up,1)));
 if(fx_reg(FX_ECX)==0xfffffffdu){
  rx_v4 relative=normalize(product(inverse,rx_ld4(s+l->target)),1,v4_zero());
  SU32(b-4,0,site(l,0x4d14c9));rx_set_sp(b-4);
  rx_v4 rotation=rx_fourwheel_active_dependency(0x42ca00,b,rx_ld4(0xd61640),relative,v4_splat(F32(0x977434,0)));
  rotation=normalize(rotation,1,rx_ld4(b+0x70));fx_set_reg(FX_EDX,s+0xd0);fx_set_reg(FX_ESI,b+0x70);rx_st4(b+0x70,rotation);
  SU32(b-4,0,site(l,0x4d1523));rx_set_sp(b-4);(void)rx_fourwheel_active_dependency(0x85c530,b,rotation,v4_zero(),v4_zero());
 }
 float first=F32(s,l->controls);SF32(b,0x38,first);int any=abs_call(b,l,first,0x4d1536)>F32(0x9d3bfc,0);
 if(!any)any=abs_call(b,l,F32(s,l->controls+4),0x4d154c)>F32(0x9d3bfc,0);
 if(!any)any=abs_call(b,l,F32(s,l->controls+8),0x4d1562)>F32(0x9d3bfc,0);
 if(!any)any=abs_call(b,l,F32(s,l->controls+12),0x4d1578)>F32(0x9d3bfc,0);fx_set_reg(FX_EAX,any?1:0);
 rx_v4 controls=v4_splat(F32(0x92e12c,0));
 if(any){
  controls=v4_add(v4_mul(v4_splat(phys_mul(F32(s,0x5374),F32(b,0x38))),rx_ld4(s+0x53d0)),controls);
  controls=v4_add(controls,v4_mul(v4_splat(phys_mul(F32(s,l->controls+4),F32(s,0x5424))),rx_ld4(s+0x5480)));
  controls=v4_add(controls,v4_mul(v4_splat(phys_mul(F32(s,l->controls+8),F32(s,0x54d4))),rx_ld4(s+0x5530)));
  controls=v4_add(controls,v4_mul(v4_splat(phys_mul(F32(s,l->controls+12),F32(s,0x5584))),rx_ld4(s+0x55e0)));
 }
 rx_v4 modifier=rx_ld4(s+0x52c0);if(U32(s,l->modifier))modifier=v4_mul(rx_ld4(s+0x5320),modifier);
 fx_set_reg(FX_ECX,U32(b,0x9c));fx_set_reg(FX_ESI,frame.in.esi);
 rx_v4 result=v4_mul(v4_add(v4_shuf(controls,controls,0xc9),rx_ld4(b+0x80)),v4_shuf(modifier,modifier,0xc9));
 fx_set_reg(FX_ECX,fx_reg(FX_ECX)^(b+4));rx_st4(s+0x740,result);
 SU32(b,0,site(l,0x4d166c));rx_set_sp(b);(void)rx_fourwheel_active_dependency(0x8f9efe,b+4,v4_zero(),v4_zero(),v4_zero());fx_frame_leave(&frame);
}
void rx_utv_truck_active_solve_draft(rx_addr s,float dt){active_solve(s,dt,&utv_truck);}
void rx_buggy_active_solve_draft(rx_addr s,float dt){active_solve(s,dt,&buggy);}
