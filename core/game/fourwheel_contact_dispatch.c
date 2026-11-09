/* Handwritten complete 5117B declared-body / 5136B physical-span vf13 callers.
 * Family layout equivalence is separately checked against all three spans.
 * No port registration and no substitute private-call implementation. */
#include "fourwheel_contact_dispatch.h"
#include "fx_util.h"
#include "phys_vec.h"
typedef struct {rx_addr entry;unsigned delta,family;} DispatchLayout;
typedef struct {
 rx_addr query,collision,probe;
 rx_addr bounds[6];
} WheelSites;
static const WheelSites sites[4]={
 {0x4d1ea5,0x4d20db,0x4d222f,{0x4d1f2a,0x4d1f77,0x4d203b,0x4d2070,0x4d20a1,0x4d214b}},
 {0x4d235e,0x4d258b,0x4d26df,{0x4d23e7,0x4d2430,0x4d24eb,0x4d2520,0x4d2551,0x4d25fb}},
 {0x4d2811,0x4d2a3b,0x4d2b8f,{0x4d2897,0x4d28e0,0x4d299b,0x4d29d0,0x4d2a01,0x4d2aab}},
 {0x4d2cc1,0x4d2eeb,0x4d303f,{0x4d2d47,0x4d2d90,0x4d2e4b,0x4d2e80,0x4d2eb1,0x4d2f5b}}
};
static rx_addr relocated(const DispatchLayout*l,rx_addr original){return l->entry+(original-0x4d1d30);}
static rx_v4 dependency(const DispatchLayout*l,enum rx_contact_dispatch_role role,
 rx_addr entry,rx_addr expected,rx_addr original,rx_v4 xmm0){
 SU32(entry,0,relocated(l,original));rx_set_sp(entry);
 return rx_fourwheel_contact_dispatch_dependency(role,l->family,expected,xmm0);
}
/* Original signed magic division by80, including incidental EDX and the
 * destination-specific quotient register. Do not replace with unsigned /80. */
static uint32_t shape_count(rx_addr s,int destination){
 uint32_t span=U32(s,0x1310)-U32(s,0x130c);fx_set_reg(FX_ECX,span);
 int64_t wide=(int64_t)(int32_t)span*INT64_C(0x66666667);
 uint32_t high=(uint32_t)(wide>>32),shift=(uint32_t)((int32_t)high>>5);
 fx_set_reg(FX_EAX,(uint32_t)wide);fx_set_reg(FX_EDX,shift);
 uint32_t result=shift+(shift>>31);fx_set_reg(destination,result);return result;
}
static void empty_manifold(rx_addr b,int fresh){
 if(fresh){fx_set_reg(FX_EAX,0);SU32(b,0x740,0);SU32(b,0xa0,0);}
 fx_set_reg(FX_ECX,0xffffffffu);
 for(unsigned i=0;i<30;i++){SU32(b,0x650+4*i,0xffffffffu);SU32(b,0x6c8+4*i,0xffffffffu);}
 fx_set_reg(FX_EAX,30);
}
static rx_v4 bounds(const DispatchLayout*l,rx_addr b,const WheelSites*w,unsigned index,rx_v4 normal){
 if(!(fx_reg(FX_ESI)<fx_reg(index==3?FX_ECX:FX_EAX))){
  normal=dependency(l,RX_CD_BOUNDS,b-4,b,w->bounds[index],normal);
  if(index==0||index==1||index==5)normal=rx_ld4(b+0x30);
 }
 return normal;
}
/* Original quaternion-product association and multiply operand order. The
 * third inner product reverses the z multiply; outer products for wheel1/2
 * put the quaternion z lane on the left. NaN/signed-zero behavior is retained. */
static rx_v4 product(rx_v4 a,rx_v4 q,int z_left){
 rx_v4 reversed=v4_shuf(q,q,0x1b);
 rx_v4 x=v4_mul(v4_mul(v4_lane(a,0),reversed),rx_ld4(0x93a1f0));
 reversed=v4_shuf(reversed,reversed,0xb1);
 rx_v4 y=v4_mul(v4_mul(v4_lane(a,1),reversed),rx_ld4(0x93a200));
 reversed=v4_shuf(reversed,reversed,0x1b);
 rx_v4 z=z_left?v4_mul(v4_lane(a,2),reversed):v4_mul(reversed,v4_lane(a,2));
 z=v4_mul(z,rx_ld4(0x93a210));
 rx_v4 w=v4_mul(v4_lane(a,3),q);
 return v4_add(v4_add(z,y),v4_add(w,x));
}
static rx_v4 normal_direction(rx_v4 n){
 rx_v4 p=v4_mul(n,n);float sum=phys_add(phys_add(p.v[0],p.v[1]),p.v[2]);
 rx_v4 sums=v4_splat(sum),length=v4_sqrt(sums),divided=v4_div(n,length);
 rx_v4 finite=v4_mask(sum!=F32(0,0x92dc20),sum!=F32(0,0x92dc24),sum!=F32(0,0x92dc28),sum!=F32(0,0x92dc2c));
 rx_v4 nonzero=v4_mask(length.v[0]!=0,length.v[1]!=0,length.v[2]!=0,length.v[3]!=0);
 return v4_or(v4_and(v4_and(divided,nonzero),finite),v4_andn(finite,rx_ld4(0x92dc30)));
}
static void wheel_pass(rx_addr s,rx_addr b,unsigned i,const DispatchLayout*l){
 const WheelSites*w=&sites[i];uint32_t wheel=0x8080+0x150*i-l->delta,flag=wheel+0xfc;
 uint32_t shape_id=0x8028+4*i-l->delta;fx_set_reg(FX_EAX,U32(s,wheel));
 rx_st4(b+0x90,rx_ld4(s+0x730));
 fx_set_reg(FX_ESI,s+wheel);uint32_t stride=160u*fx_reg(FX_EAX);
 if(i)fx_set_reg(FX_EDX,stride);
 rx_v4 q=rx_ld4(s+stride+0x730),offset=v4_and(rx_ld4(s+wheel+0x20),rx_ld4(0x92db80));
 fx_set_reg(FX_EAX,s+stride);
 rx_v4 rotated=product(q,product(offset,v4_mul(q,rx_ld4(0x93a220)),i==2),i==1||i==2);
 rx_st4(b+0x70,q);rx_v4 position=v4_add(rotated,rx_ld4(s+stride+0xf0));
 rx_st4(s+0x8060-l->delta,rx_ld4(s+wheel+0x10));
 /* The simulation argument remains pushed during all four pending stores. */
 SU32(b-4,0,s);SU32(b,0x28,fx_reg(FX_EBX));rx_st4(b+0x60,position);
 rx_st4(b+0x50,rx_ld4(0xd61670));rx_st4(b+0x30,rx_ld4(0xd61670));
 (void)dependency(l,RX_CD_QUERY,b-8,b,w->query,v4_zero());
 rx_v4 normal;
 if(U32(s,flag)!=fx_reg(FX_EBX)){
  rx_st4(b+0x50,rx_ld4(s+wheel+0x50));normal=rx_ld4(s+wheel+0x40);
  SU32(b,0x28,1);rx_st4(b+0x30,normal);
 }else normal=rx_ld4(b+0x30);
 SU32(s,shape_id,fx_reg(FX_EBX));
 uint32_t initial=shape_count(s,i==0?FX_ECX:FX_EAX);fx_set_reg(FX_ESI,0);
 if(initial){
  SU32(b,0x10,i==0?fx_reg(FX_EBX):fx_reg(FX_ESI));
  do{
   shape_count(s,FX_EAX);normal=bounds(l,b,w,0,normal);
   fx_set_reg(FX_ECX,U32(s,0x130c));fx_set_reg(FX_EBX,U32(b,0x10));
   rx_v4 lower=v4_sub(rx_ld4(fx_reg(FX_ECX)+fx_reg(FX_EBX)+0x20),rx_ld4(s+wheel+0x10));
   shape_count(s,FX_EAX);rx_st4(b+0x80,lower);
   if(!(fx_reg(FX_ESI)<fx_reg(FX_EAX))){
    (void)dependency(l,RX_CD_BOUNDS,b-4,b,w->bounds[1],normal);lower=rx_ld4(b+0x80);normal=rx_ld4(b+0x30);
   }
   fx_set_reg(FX_ECX,U32(s,0x130c));
   rx_v4 upper=v4_add(rx_ld4(fx_reg(FX_EBX)+fx_reg(FX_ECX)+0x30),rx_ld4(s+wheel+0x10));
   rx_v4 mask=v4_cmplt(rx_ld4(b+0x60),upper);rx_st4(b+0x40,mask);
   fx_set_reg(FX_EDX,U32(b,0x44));fx_set_reg(FX_EAX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
   if(fx_reg(FX_EAX)==0xfffffffdu){
    mask=v4_cmplt(lower,rx_ld4(b+0x60));rx_st4(b+0x40,mask);
    fx_set_reg(FX_ECX,U32(b,0x44));fx_set_reg(FX_EDX,U32(b,0x48)+U32(b,0x44)+U32(b,0x40));
    if(fx_reg(FX_EDX)==fx_reg(FX_EAX)){
     empty_manifold(b,1);shape_count(s,FX_EAX);normal=bounds(l,b,w,2,normal);
     fx_set_reg(FX_EAX,U32(s,0x130c)+U32(b,0x10));
     fx_set_reg(FX_ECX,U32(s,0x1310)-U32(s,0x130c));SU32(b,0x2c,fx_reg(FX_EAX));
     shape_count(s,FX_ECX);normal=bounds(l,b,w,3,normal);
     fx_set_reg(FX_ECX,U32(s,0x1310)-U32(s,0x130c));
     fx_set_reg(FX_EBX,U32(s,0x130c)+U32(b,0x10));shape_count(s,FX_EAX);normal=bounds(l,b,w,4,normal);
     /* Seven arguments are pushed in the original order while ECX/EDX/EAX
      * are successively reused. Preserve those register values at entry. */
     SU32(b-4,0,0);fx_set_reg(FX_ECX,b+0x60);SU32(b-8,0,fx_reg(FX_ECX));
     fx_set_reg(FX_ECX,U32(b,0x2c));fx_set_reg(FX_EDX,b+0x70);SU32(b-12,0,fx_reg(FX_EDX));
     fx_set_reg(FX_EDX,U32(s,0x130c));fx_set_reg(FX_EAX,s+0x8040-l->delta);SU32(b-16,0,fx_reg(FX_EAX));
     fx_set_reg(FX_EAX,U32(b,0x10));SU32(b-20,0,fx_reg(FX_ECX));
     fx_set_reg(FX_ECX,U32(fx_reg(FX_EAX)+fx_reg(FX_EDX),0x44));fx_set_reg(FX_EDX,U32(fx_reg(FX_ECX),4));
     fx_set_reg(FX_EBX,fx_reg(FX_EBX)+0x10);SU32(b-24,0,fx_reg(FX_EBX));SU32(b-28,0,fx_reg(FX_EDX));fx_set_reg(FX_ECX,b+0xa0);
     (void)dependency(l,RX_CD_COLLISION,b-32,b,w->collision,v4_zero());
     fx_set_reg(FX_ECX,U32(b,0xa0));normal=rx_ld4(b+0x30);
     if((int32_t)fx_reg(FX_ECX)>0){
      SU32(b,0x28,U32(b,0x28)+fx_reg(FX_ECX));fx_set_reg(FX_EAX,b+0xb0);
      do{
       rx_v4 point=rx_ld4(fx_reg(FX_EAX)),n=rx_ld4(fx_reg(FX_EAX)+0x10),sum=rx_ld4(b+0x50);
       fx_set_reg(FX_EAX,fx_reg(FX_EAX)+0x30);fx_set_reg(FX_ECX,fx_reg(FX_ECX)-1);
       rx_st4(b+0x50,i==3?v4_add(sum,point):v4_add(point,sum));normal=i==3?v4_add(normal,n):v4_add(n,normal);
      }while(fx_reg(FX_ECX));
      rx_st4(b+0x30,normal);shape_count(s,FX_EAX);normal=bounds(l,b,w,5,normal);
      fx_set_reg(FX_ECX,U32(s,0x130c));fx_set_reg(FX_EDX,U32(b,0x10));
      fx_set_reg(FX_EAX,U32(fx_reg(FX_EDX)+fx_reg(FX_ECX),0x40));SU32(s,shape_id,fx_reg(FX_EAX));
     }
    }
   }
   /* The list may have changed during a real dispatcher call. Re-read the
    * endpoints on every original check, retaining unsigned JB loop semantics. */
   SU32(b,0x10,U32(b,0x10)+0x50);shape_count(s,FX_ECX);
   fx_set_reg(FX_ESI,fx_reg(FX_ESI)+1);
  }while(fx_reg(FX_ESI)<fx_reg(FX_ECX));
 }
 fx_set_reg(FX_EAX,U32(b,0x28));fx_set_reg(FX_EBX,0);
 if((int32_t)fx_reg(FX_EAX)>0){
  SU32(s,flag,1);rx_v4 mean=v4_div(rx_ld4(b+0x50),v4_splat((float)(int32_t)fx_reg(FX_EAX)));
  fx_set_reg(FX_EDX,b+0x90);SU32(b-4,0,fx_reg(FX_EDX));fx_set_reg(FX_EAX,b+0x70);rx_st4(b+0x50,mean);
  normal=normal_direction(normal);SU32(b-8,0,fx_reg(FX_EAX));fx_set_reg(FX_ECX,b+0x50);SU32(b-12,0,fx_reg(FX_ECX));
  fx_set_reg(FX_EAX,s+wheel);SU32(b-16,0,fx_reg(FX_EAX));fx_set_reg(FX_ECX,b+0x60);fx_set_reg(FX_EDX,b+0x30);rx_st4(b+0x30,normal);
  (void)dependency(l,RX_CD_PROBE_FRAME,b-20,b,w->probe,v4_zero());
 }
}
static void dispatch(rx_addr s,float dt,const DispatchLayout*l){
 fx_frame f;fx_frame_enter(&f,0x754,16,FX_EBX,FX_ESI,FX_EDI);rx_addr b=f.E;
 fx_set_reg(FX_EAX,U32(0,0x9b45e4)^(b+12));SU32(b,0x75c,fx_reg(FX_EAX));
 fx_set_reg(FX_EDI,s);fx_set_reg(FX_EBX,0);fx_set_reg(FX_EAX,0);empty_manifold(b,0);
 for(unsigned i=0;i<4;i++)wheel_pass(s,b,i,l);
 uint32_t a=U32(s,0x817c-l->delta),c=U32(s,0x82cc-l->delta),d=U32(s,0x841c-l->delta),e=U32(s,0x856c-l->delta);
 fx_set_reg(FX_EAX,a);fx_set_reg(FX_ECX,c);fx_set_reg(FX_EDX,d);fx_set_reg(FX_ESI,e);
 const uint32_t flags[4]={a,c,d,e};
 for(unsigned i=0;i<4;i++){
  uint32_t w=0x8080+0x150*i-l->delta;SU32(s,w+0x104,U32(s,w+0x104)|flags[i]);SU32(s,w+0x100,flags[i]);
  if(!flags[i]){SU32(s,w+0xc0,0);SU32(s,w+0xc4,0);}
 }
 if(!(a||c||d||e)){
  fx_set_reg(FX_EDX,0);uint32_t old=U32(s,0x8700-l->delta);SU32(s,0x8700-l->delta,1);
  fx_set_reg(FX_EDX,old==0);SU32(s,0x8704-l->delta,fx_reg(FX_EDX));
 }else{SU32(s,0x8700-l->delta,0);SU32(s,0x8704-l->delta,0);}
 fx_set_reg(FX_ESI,s);(void)dependency(l,RX_CD_BODY_PROBE,b-4,b,0x4d3107,v4_zero());
 fx_set_reg(FX_EDX,s);(void)dependency(l,RX_CD_CORRECTION,b-4,b,0x4d310e,v4_zero());
 /* Real FLD/FSTP dt copy preserves the original x87 NaN canonicalization. */
 fx_x87_copy(b-4,f.ebp+8);(void)dependency(l,RX_CD_FILTERED,b-8,b,0x4d311a,v4_zero());
 if(U32(s,0x2374)==fx_reg(FX_EBX)){
  fx_set_reg(FX_ECX,s);(void)dependency(l,RX_CD_PROJECT,b-4,b,0x4d3129,v4_zero());
 }
 fx_set_reg(FX_ECX,U32(b,0x75c));fx_set_reg(FX_EDI,f.in.edi);fx_set_reg(FX_ESI,f.in.esi);fx_set_reg(FX_EBX,f.in.ebx);
 fx_set_reg(FX_ECX,fx_reg(FX_ECX)^(b+12));(void)dependency(l,RX_CD_COOKIE,b+8,b+12,0x4d313a,v4_zero());fx_frame_leave(&f);(void)dt;
}
void rx_utv_contact_solve_dispatch(rx_addr s,float dt){static const DispatchLayout l={0x4d1d30,0,0};dispatch(s,dt,&l);}
void rx_buggy_contact_solve_dispatch(rx_addr s,float dt){static const DispatchLayout l={0x4b7580,0x10,1};dispatch(s,dt,&l);}
void rx_truck_contact_solve_dispatch(rx_addr s,float dt){static const DispatchLayout l={0x453be0,0,2};dispatch(s,dt,&l);}
