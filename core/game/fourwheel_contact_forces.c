/* Complete handwritten00452a90. Native dependency composition is separate;
 * no registration or generic LTCG dispatch is supplied by this source draft. */
#include "fourwheel_contact_forces.h"
#include "phys_vec.h"
#include "fx_util.h"
static rx_v4 unequal(rx_v4 a,rx_v4 b){return v4_mask(a.v[0]!=b.v[0],a.v[1]!=b.v[1],a.v[2]!=b.v[2],a.v[3]!=b.v[3]);}
static float curve(rx_addr b,rx_addr p,rx_addr ra,float value){
 fx_set_reg(FX_ESI,p);SU32(b-4,0,ra);rx_set_sp(b-4);
 return rx_fourwheel_contact_dependency(0x4390d0,b,value);
}
typedef struct {unsigned delta;rx_addr entry;} ContactLayout;
static rx_addr site(const ContactLayout*l,rx_addr utv){return l->entry+(utv-0x452a90);}
static rx_v4 contact_force(rx_addr s,rx_addr b,unsigned side,const ContactLayout*l){
 static const uint32_t input[]={0x8434,0x8584,0x8194,0x82e4};
 static const uint32_t velocity[]={0x2e0,0x380,0x1a0,0x240};
 static const uint32_t load[]={0x8400,0x8550,0x8160,0x82b0};
 static const uint32_t ra0[]={0x452afe,0x452be8,0x452cd2,0x452dbc};
 static const uint32_t ra1[]={0x452b17,0x452c01,0x452ceb,0x452dd5};
 rx_addr table=s+(side<2?0x47bc:0x431c);
 SF32(b,8,curve(b,table,site(l,ra0[side]),F32(s,input[side]-l->delta)));
 float second=curve(b,table+0x50,site(l,ra1[side]),F32(s,input[side]-l->delta));
 rx_v4 v=rx_ld4(s+velocity[side]),squares=v4_mul(v,v);
 float sum=phys_add(phys_add(squares.v[0],squares.v[1]),squares.v[2]);
 rx_v4 square=v4_splat(sum),root=v4_sqrt(square);
 rx_v4 finite=unequal(square,rx_ld4(0x92dc20));
 rx_v4 nonzero=unequal(v4_zero(),root);
 rx_v4 direction=v4_or(v4_and(v4_and(nonzero,v4_div(v,root)),finite),v4_andn(finite,rx_ld4(0x92dc30)));
 rx_v4 drag=v4_mul(v4_sub(v4_zero(),v4_splat(second)),v);
 rx_v4 resistance=v4_mul(direction,v4_sub(v4_zero(),v4_splat(F32(b,8))));
 rx_v4 combined=v4_add(resistance,drag);
 float weight=phys_div(F32(s,load[side]-l->delta),F32(b,0xc));
 if(weight>=F32(0,0x92e330))weight=F32(0,0x92e330);
 else if(weight<=0)weight=0;
 return v4_mul(v4_splat(weight),combined);
}
static void add_force(rx_addr b,rx_addr force,rx_addr ra,uint32_t index){
 fx_set_reg(FX_ECX,force);SU32(b-4,0,index);SU32(b-8,0,ra);rx_set_sp(b-8);
 (void)rx_fourwheel_contact_dependency(0x85c770,b,0);
}
static void contact(rx_addr s,const ContactLayout*l){
 fx_frame frame;fx_frame_enter(&frame,0x7c,16,FX_ESI);rx_addr b=frame.E;
 uint32_t cookie=U32(0,0x9b45e4)^(b+4);fx_set_reg(FX_EAX,cookie);SU32(b,0x7c,cookie);
 rx_v4 zero=rx_ld4(0xd61670);rx_st4(b+0x30,zero);rx_st4(b+0x20,zero);rx_st4(b+0x40,zero);rx_st4(b+0x50,zero);
 SF32(b,0xc,phys_mul(phys_mul(F32(s,0x3734),F32(0,0x92ff84)),F32(0,0x977878)));
 if(U32(s,0x8420-l->delta))rx_st4(b+0x30,contact_force(s,b,0,l));
 if(U32(s,0x8570-l->delta))rx_st4(b+0x20,contact_force(s,b,1,l));
 if(U32(s,0x8180-l->delta))rx_st4(b+0x40,contact_force(s,b,2,l));
 rx_v4 fourth=U32(s,0x82d0-l->delta)?contact_force(s,b,3,l):rx_ld4(b+0x50);
 rx_st4(b+0x10,v4_zero());
 rx_v4 total=v4_mul(v4_sub(v4_zero(),rx_ld4(s+0x100)),v4_splat(F32(s,0x2f00)));
 total=v4_add(total,rx_ld4(s+0x5640-l->delta));total=v4_add(total,rx_ld4(b+0x30));
 total=v4_add(total,rx_ld4(b+0x20));total=v4_add(total,rx_ld4(b+0x40));
 /* Each authored per-body force is independently added to the global vector;
  * fourth contact remains a register value, never an extra B+50 spill. */
 rx_st4(b+0x30,v4_add(rx_ld4(s+0x8600-l->delta),zero));
 rx_st4(b+0x20,v4_add(rx_ld4(s+0x8610-l->delta),zero));
 rx_st4(b+0x40,v4_add(rx_ld4(s+0x8620-l->delta),zero));
 total=v4_add(total,fourth);rx_st4(b+0x60,total);
 rx_st4(b+0x50,v4_add(rx_ld4(s+0x8630-l->delta),zero));
 fx_set_reg(FX_EDX,s+0xd0);fx_set_reg(FX_ESI,0xd61670);
 add_force(b,b+0x60,site(l,0x452f22),0);add_force(b,b+0x30,site(l,0x452f2d),3);
 add_force(b,b+0x20,site(l,0x452f38),4);add_force(b,b+0x40,site(l,0x452f43),1);add_force(b,b+0x50,site(l,0x452f4e),2);
 rx_v4 reaction=v4_mul(v4_sub(rx_ld4(b+0x10),v4_splat(F32(0,0x931f24))),rx_ld4(s+0x740));
 rx_st4(s+0x780,v4_add(reaction,rx_ld4(s+0x780)));
 if(U32(0,0xda68c0))rx_st4(b+0x10,reaction);
 fx_set_reg(FX_ECX,U32(b,0x7c)^(b+4));fx_set_reg(FX_ESI,frame.in.esi);
 SU32(b,0,site(l,0x452f9a));rx_set_sp(b);(void)rx_fourwheel_contact_dependency(0x8f9efe,b+4,0);
 fx_frame_leave(&frame);
}

void rx_fourwheel_apply_contact_forces(rx_addr s){static const ContactLayout l={0,0x452a90};contact(s,&l);}
void rx_buggy_apply_contact_forces(rx_addr s){static const ContactLayout l={0x10,0x4b54c0};contact(s,&l);}
