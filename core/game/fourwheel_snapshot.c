/* Complete four-wheel effect snapshots. Layout and operand order derived
 * from all six original functions; intentional output holes remain untouched. */
#include "fourwheel_snapshot.h"
#include "world_veh_util.h"
#include "bike_math.h"
#include "fx_util.h"
typedef struct{uint32_t sim,delta,user,engine_state;}snapshot_layout;
static const snapshot_layout layouts[3]={{0x1540,0,0x1468,0x8678},{0x15c0,0x10,0x14b8,0x8668},{0x15d0,0,0x14f8,0x867c}};
static rx_addr simulation(rx_addr v,const snapshot_layout*l){return U32(v,l->sim);}
static const rx_addr return_sites[2][3][9]={{{5009549, 5009614, 5010129, 5010608, 5011128, 5011598}, {4900653, 4900718, 4901233, 4901712, 4902232, 4902702}, {4666973, 4667038, 4667553, 4668032, 4668552, 4669022}}, {{5011879, 5012165, 5012466, 5012767, 5012837, 5012894, 5012912, 5012966, 5012984}, {4902983, 4903269, 4903570, 4903871, 4903941, 4903998, 4904016, 4904070, 4904088}, {4669303, 4669589, 4669890, 4670191, 4670261, 4670318, 4670336, 4670390, 4670408}}};
/* kind: integer return0, vector output1, x87 scalar2. The dependency
 * receives the exact original frame and return site, and returns scalar ST0
 * only for kind2. Integer result travels through the live EAX model. */
typedef struct{uint32_t integer;rx_f80 scalar;}snapshot_result;
static snapshot_result snapshot_call(unsigned family,unsigned method,unsigned site,rx_addr object,unsigned slot,rx_addr output,unsigned kind){
 rx_addr vt=U32(object,0),target=U32(vt,slot);
 fx_set_reg(FX_ECX,object);
 int call_eax=(method==0?(site==2||site==3||site==5):(site==1||site==3||site==7||site==8));
 fx_set_reg(FX_EAX,call_eax?target:vt);fx_set_reg(FX_EDX,call_eax?vt:target);
 rx_addr saved=fx_push(kind==1?1:0,&output,return_sites[method][family][site]);
 snapshot_result result;result.scalar=rx_fourwheel_snapshot_dependency(family,method,site,target,kind);
 fx_pop(saved);
 result.integer=fx_reg(FX_EAX);
 return result;
}
static void telemetry_copy4(rx_addr out,rx_addr in){v4_st(out,v4_ld(in));}
static void telemetry_quiet(rx_addr out,rx_addr in){SU32(out,0,rx_fb(rx_f80_to_f32(rx_f80_from_f32(rx_bf(U32(in,0))),0)));}

/* Four ordered groups: rear left/right then front left/right. */
static void snapshot_vectors(rx_addr v,rx_addr out,unsigned family){
 const snapshot_layout*l=&layouts[family];fx_frame f;fx_frame_enter(&f,0x54,16,FX_EBX,FX_ESI,FX_EDI);rx_addr F=f.E;fx_set_reg(FX_ESI,v);fx_set_reg(FX_EDI,out);SU32(F,0x5c,U32(0,0x9b45e4)^(F+12));
 snapshot_call(family,0,0,v,0x94,F+0x30,1);telemetry_copy4(out+0x1f0,F+0x30);
 for(unsigned k=0;k<3;k++)telemetry_quiet(out+0x1d0+4*k,v+0xc0+4*k);
 snapshot_call(family,0,1,v,0x90,F+0x40,1);telemetry_copy4(out+0x1e0,F+0x40);
 const uint32_t contact[]={0x8320-l->delta,0x8470-l->delta,0x8080-l->delta,0x81d0-l->delta},qoff[]={0x9a0,0xa40,0x860,0x900};
 const uint32_t body[]={0x360,0x400,0x220,0x2c0},deform[]={0x8390-l->delta,0x84e0-l->delta,0x80f0-l->delta,0x8240-l->delta};
 const uint32_t flag[]={0x85c8-l->delta,0x85cc-l->delta,0x85c0-l->delta,0x85c4-l->delta},user[]={l->user+8,l->user+12,l->user,l->user+4};
 const uint32_t obj[]={0x8030-l->delta,0x8034-l->delta,0x8028-l->delta,0x802c-l->delta};
 rx_v4 offset=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_addr D=out+0x70*i;
  rx_addr s=simulation(v,l);telemetry_copy4(D,s+contact[i]+0x120);
  s=simulation(v,l);telemetry_copy4(D+0x10,s+contact[i]+0x140);
  s=simulation(v,l);telemetry_copy4(D+0x20,s+contact[i]+0x130);
  s=simulation(v,l);if(i==0||i==2)offset=v4(phys_mul(F32(s,i==0?0x4774:0x42d4),F32(0,0x9322cc)),0,0,0);
  rx_v4 rotation=veh_rotate(v4_ld(s+qoff[i]),offset),sum=i<2?v4_add(v4_ld(s+deform[i]),v4_ld(s+body[i])):v4_add(v4_ld(s+body[i]),v4_ld(s+deform[i]));
  v4_st(D+0x30,(i&1)?v4_sub(sum,rotation):v4_add(rotation,sum));
  s=simulation(v,l);v4_st(D+0x40,v4_mul(v4_splat(F32(s,contact[i]+0x80)),v4_ld(s+contact[i]+0x140)));
  s=simulation(v,l);v4_st(D+0x50,v4_mul(v4_splat(F32(s,contact[i]+0x88)),v4_ld(s+contact[i]+0x120)));
  telemetry_quiet(D+0x60,simulation(v,l)+contact[i]+0xe0);telemetry_quiet(D+0x64,simulation(v,l)+flag[i]);
  SU32(D,0x68,U32(simulation(v,l),contact[i]+0x100)==0);SU32(D,0x6c,U32(v,user[i]));
  rx_addr sim=simulation(v,l),object=U32(sim,obj[i]);
  fx_set_reg(FX_EBX,1);fx_set_reg(FX_EDI,i==3?D:out);
  if(i==3){fx_set_reg(FX_EAX,U32(D,0x68));fx_set_reg(FX_EDX,sim);}
  if(object&&snapshot_call(family,0,2+i,object,0x58,0,0).integer==5)SU32(D,0x68,1);
 }
 phys_security_check_cookie(U32(F,0x5c)^(F+12));fx_frame_leave(&f);
}

/* Family vf52 numeric/bit diagnostic values; gaps in the0x264-byte record are intentional. */
static void snapshot_values(rx_addr v,rx_addr out,unsigned family){
 const snapshot_layout*l=&layouts[family];fx_frame f;fx_frame_enter(&f,0x24,16,FX_EBX,FX_ESI,FX_EDI);rx_addr F=f.E;fx_set_reg(FX_ESI,v);fx_set_reg(FX_EDI,out);fx_set_reg(FX_EBX,0);SU32(F,0x2c,U32(0,0x9b45e4)^(F+12));
 const uint32_t contact[]={0x8320-l->delta,0x8470-l->delta,0x8080-l->delta,0x81d0-l->delta},body[]={0x360,0x400,0x220,0x2c0};
 const uint32_t flag[]={0x85c8-l->delta,0x85cc-l->delta,0x85c0-l->delta,0x85c4-l->delta},user[]={l->user+8,l->user+12,l->user,l->user+4},obj[]={0x8030-l->delta,0x8034-l->delta,0x8028-l->delta,0x802c-l->delta};
 for(unsigned i=0;i<4;i++){
  rx_addr D=out+0x70*i,s=simulation(v,l);
  rx_v4 a=v4_ld(s+contact[i]+0x70),b=v4_ld(s+body[i]);v4_st(D,i<2?v4_add(a,b):v4_add(b,a));
  s=simulation(v,l);telemetry_copy4(D+0x10,s+contact[i]+0x40);
  s=simulation(v,l);v4_st(D+0x20,v4_mul(v4_splat(F32(s,contact[i]+0x80)),v4_ld(s+contact[i]+0x140)));
  s=simulation(v,l);v4_st(D+0x30,v4_mul(v4_splat(F32(s,contact[i]+0x88)),v4_ld(s+contact[i]+0x120)));
  telemetry_quiet(D+0x40,simulation(v,l)+contact[i]+0xe0);telemetry_quiet(D+0x44,simulation(v,l)+flag[i]);
  SU32(D,0x48,U32(simulation(v,l),contact[i]+0x104)==0);
  SU32(D,0x50,U32(v,user[i]));SU32(D,0x54,U32(v,user[i]+0x10));telemetry_quiet(D+0x58,v+user[i]+0x20);
  rx_addr object=U32(simulation(v,l),obj[i]);if(object&&snapshot_call(family,1,i,object,0x58,0,0).integer==5){object=U32(simulation(v,l),obj[i]);SU32(D,0x60,U32(object,0x158));SU32(D,0x64,object);}else{SU32(D,0x60,0xffffffff);SU32(D,0x64,0);}
 }
 snapshot_call(family,1,4,v,0x94,F+0x10,1);rx_v4 velocity=v4_ld(F+0x10);SF32(out,0x220,phys_sqrt(bike_hsum_021(v4_mul(velocity,velocity))));
 SF32(out,0x224,rx_f80_to_f32(snapshot_call(family,1,5,v,0xbc,0,2).scalar,0));SF32(out,0x228,rx_f80_to_f32(snapshot_call(family,1,6,v,0xc0,0,2).scalar,0));
 telemetry_quiet(out+0x22c,simulation(v,l)+0x5600-l->delta);telemetry_quiet(out+0x230,simulation(v,l)+0x5604-l->delta);
 SU32(out,0x234,snapshot_call(family,1,7,v,0xa8,0,0).integer);SU32(out,0x244,snapshot_call(family,1,8,v,0xc8,0,0).integer);telemetry_quiet(out+0x240,simulation(v,l)+l->engine_state);
 rx_addr s=simulation(v,l);uint32_t airborne=!U32(s,0x8184-l->delta)&&!U32(s,0x82d4-l->delta)&&!U32(s,0x8424-l->delta)&&!U32(s,0x8574-l->delta);
 SU32(out,0x250,0);SU32(out,0x248,airborne);SU32(out,0x258,U32(v,0x1310));SU32(out,0x25c,U32(v,0x1314));telemetry_quiet(out+0x254,v+0x131c);SU32(out,0x260,0);
 fx_set_reg(FX_EAX,U32(v,0x1310));fx_set_reg(FX_EDX,s);phys_security_check_cookie(U32(F,0x2c)^(F+12));fx_frame_leave(&f);
}


void rx_utv_snapshot_vectors(rx_addr v,rx_addr out){snapshot_vectors(v,out,0);}
RX_FUNCTION(0x004c7060,rx_utv_snapshot_vectors)
void rx_utv_snapshot_values(rx_addr v,rx_addr out){snapshot_values(v,out,0);}
RX_FUNCTION(0x004c78b0,rx_utv_snapshot_values)
void rx_buggy_snapshot_vectors(rx_addr v,rx_addr out){snapshot_vectors(v,out,1);}
RX_FUNCTION(0x004ac700,rx_buggy_snapshot_vectors)
void rx_buggy_snapshot_values(rx_addr v,rx_addr out){snapshot_values(v,out,1);}
RX_FUNCTION(0x004acf50,rx_buggy_snapshot_values)
void rx_truck_snapshot_vectors(rx_addr v,rx_addr out){snapshot_vectors(v,out,2);}
RX_FUNCTION(0x00473630,rx_truck_snapshot_vectors)
void rx_truck_snapshot_values(rx_addr v,rx_addr out){snapshot_values(v,out,2);}
RX_FUNCTION(0x00473e80,rx_truck_snapshot_values)
