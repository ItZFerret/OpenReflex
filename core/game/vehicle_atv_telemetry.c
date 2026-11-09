/* Original four-wheel grip and diagnostic snapshots. Output holes remain
 * untouched; integer flags/pointers are bits, never float conversions. */
#include "vehicle_atv_telemetry.h"
#include "world_veh_util.h"
#include "bike_math.h"
#include "phys_crt.h"
static rx_addr telemetry_sim(rx_addr v){return U32(v,0x1700);}
static float telemetry_abs(float a){return phys_max(phys_sub(0,a),a);}
rx_f80 rx_atv_grip_factor(rx_addr v,uint32_t axle){
 world_frame f=world_frame_enter(0x10,0,1);rx_addr s=telemetry_sim(v);
 rx_v4 q=veh_quat_mul(v4_ld(s+0x7c0),v4_ld(0xad66b0));
 rx_v4 forward=veh_rotate(q,v4_ld(0xd61680));
 float a=F32(0,0x931f24),b=a;
 uint32_t left=axle==0?0x9240:0x94e0,right=axle==0?0x9390:0x9630;
 if(axle<=1){
  if(U32(s,left)){
   float x=telemetry_abs(phys_dot3(v4_ld(s+left+0x20),forward));
   if(U32(s,right)){
    float y=telemetry_abs(phys_dot3(v4_ld(s+right+0x20),forward));a=phys_max(x,y);
    b=phys_min(phys_dot3(v4_ld(s+left+0x30),v4_ld(0xd616a0)),phys_dot3(v4_ld(s+right+0x30),v4_ld(0xd616a0)));
   }else{a=x;if(axle!=0)b=phys_dot3(v4_ld(s+left+0x30),v4_ld(0xd616a0));}
  }else if(U32(s,right)){a=telemetry_abs(phys_dot3(v4_ld(s+right+0x20),forward));b=phys_dot3(v4_ld(s+right+0x30),v4_ld(0xd616a0));}
  if(0>a)a=0;
 }
 float t=phys_sub(phys_mul(a,F32(0,0x9774c4)),F32(0,0x931d94));
 float low=F32(0,0x9cc8a8),one=F32(0,0x931f24);
 if(t>=one){t=one;low=phys_max(low,t);}
 else if(!(0>=t))low=phys_max(low,t);else t=0;
 // Only the positive branch updates the lower threshold. NaN follows the
 // COMISS/JB zero branch, exactly as the original's ordered ladder.
 
 float x;
 if(0>b||!(0<b||b!=b))x=low;else if(b>=one)x=t;else x=phys_add(phys_mul(phys_sub(t,low),b),low);
 SF32(f.esp,0xc,x);rx_f80 result=veh_ci_pow(rx_f80_from_f32(x),rx_f80_from_f64(.5));world_frame_leave(&f);return result;
}
RX_FUNCTION(0x00486850,rx_atv_grip_factor)
static void telemetry_copy4(rx_addr out,rx_addr in){v4_st(out,v4_ld(in));}
static void telemetry_quiet(rx_addr out,rx_addr in){SU32(out,0,world_x87_pass(U32(in,0)));}
static int telemetry_water(rx_addr object){return object&&WORLD_VCALL0(object,0x58)==5;}
/* Four ordered groups: rear left/right then front left/right. */
void rx_atv_snapshot_vectors(rx_addr v,rx_addr out){
 world_frame f=world_frame_enter(0x54,3,1);rx_addr F=f.esp;SU32(F,0x5c,U32(0,0x9b45e4)^(F+12));
 WORLD_VCALL(v,0x94,F+0x30);telemetry_copy4(out+0x1f0,F+0x30);
 for(unsigned k=0;k<3;k++)telemetry_quiet(out+0x1d0+4*k,v+0xc0+4*k);
 WORLD_VCALL(v,0x90,F+0x40);telemetry_copy4(out+0x1e0,F+0x40);
 const uint32_t contact[]={0x93e0,0x9530,0x9140,0x9290},qoff[]={0xa40,0xae0,0x900,0x9a0};
 const uint32_t body[]={0x400,0x4a0,0x2c0,0x360},deform[]={0x9450,0x95a0,0x91b0,0x9300};
 const uint32_t flag[]={0x9688,0x968c,0x9680,0x9684},user[]={0x1630,0x1634,0x1628,0x162c};
 const uint32_t obj[]={0x90f0,0x90f4,0x90e8,0x90ec};
 rx_v4 offset=v4_zero();
 for(unsigned i=0;i<4;i++){
  rx_addr D=out+0x70*i;
  rx_addr s=telemetry_sim(v);telemetry_copy4(D,s+contact[i]+0x120);
  s=telemetry_sim(v);telemetry_copy4(D+0x10,s+contact[i]+0x140);
  s=telemetry_sim(v);telemetry_copy4(D+0x20,s+contact[i]+0x130);
  s=telemetry_sim(v);if(i==0||i==2)offset=v4(phys_mul(F32(s,i==0?0x466c:0x41cc),F32(0,0x9322cc)),0,0,0);
  rx_v4 rotation=veh_rotate(v4_ld(s+qoff[i]),offset),sum=i==0||i==3?v4_add(v4_ld(s+deform[i]),v4_ld(s+body[i])):v4_add(v4_ld(s+body[i]),v4_ld(s+deform[i]));
  v4_st(D+0x30,(i&1)?v4_sub(sum,rotation):v4_add(rotation,sum));
  s=telemetry_sim(v);v4_st(D+0x40,v4_mul(v4_splat(F32(s,contact[i]+0x80)),v4_ld(s+contact[i]+0x140)));
  s=telemetry_sim(v);v4_st(D+0x50,v4_mul(v4_splat(F32(s,contact[i]+0x88)),v4_ld(s+contact[i]+0x120)));
  telemetry_quiet(D+0x60,telemetry_sim(v)+contact[i]+0xe0);telemetry_quiet(D+0x64,telemetry_sim(v)+flag[i]);
  SU32(D,0x68,U32(telemetry_sim(v),contact[i]+0x100)==0);SU32(D,0x6c,U32(v,user[i]));
  rx_addr object=U32(telemetry_sim(v),obj[i]);if(telemetry_water(object))SU32(D,0x68,1);
 }
 phys_security_check_cookie(U32(F,0x5c)^(F+12));world_frame_leave(&f);
}
RX_FUNCTION(0x00486cc0,rx_atv_snapshot_vectors)

rx_f80 rx_atv_engine_rpm(rx_addr v){return rx_f80_mul(rx_f80_from_f32(F32(telemetry_sim(v),0x96ec)),rx_f80_from_f32(F32(0,0x9774d0)),53,0);}
RX_FUNCTION(0x00486620,rx_atv_engine_rpm)
static float telemetry_virtual_float(rx_addr v,uint32_t off){
 rx_addr target=U32(U32(v,0),off);rx_f80 (*fn)(rx_addr)=RX_FN(target,rx_f80(*)(rx_addr));
 if(fn){rx_f80 r;WORLD_CALL_SP(0,r=fn(v));return rx_f80_to_f32(r,0);}
 rx_regs in={0},out={0};in.r[1]=v;rx_call_original_ex(target,&in,&out,0,NULL);return rx_f80_to_f32(out.st[0],0);
}
/*00487510: numeric/bit diagnostic values; gaps in the0x264-byte record are intentional. */
void rx_atv_snapshot_values(rx_addr v,rx_addr out){
 world_frame f=world_frame_enter(0x24,3,1);rx_addr F=f.esp;SU32(F,0x2c,U32(0,0x9b45e4)^(F+12));
 const uint32_t contact[]={0x93e0,0x9530,0x9140,0x9290},body[]={0x400,0x4a0,0x2c0,0x360};
 const uint32_t flag[]={0x9688,0x968c,0x9680,0x9684},user[]={0x1630,0x1634,0x1628,0x162c},obj[]={0x90f0,0x90f4,0x90e8,0x90ec};
 for(unsigned i=0;i<4;i++){
  rx_addr D=out+0x70*i,s=telemetry_sim(v);
  rx_v4 a=v4_ld(s+contact[i]+0x70),b=v4_ld(s+body[i]);v4_st(D,i==0||i==3?v4_add(a,b):v4_add(b,a));
  s=telemetry_sim(v);telemetry_copy4(D+0x10,s+contact[i]+0x40);
  s=telemetry_sim(v);v4_st(D+0x20,v4_mul(v4_splat(F32(s,contact[i]+0x80)),v4_ld(s+contact[i]+0x140)));
  s=telemetry_sim(v);v4_st(D+0x30,v4_mul(v4_splat(F32(s,contact[i]+0x88)),v4_ld(s+contact[i]+0x120)));
  telemetry_quiet(D+0x40,telemetry_sim(v)+contact[i]+0xe0);telemetry_quiet(D+0x44,telemetry_sim(v)+flag[i]);
  SU32(D,0x48,U32(telemetry_sim(v),contact[i]+0x104)==0);
  SU32(D,0x50,U32(v,user[i]));SU32(D,0x54,U32(v,user[i]+0x10));telemetry_quiet(D+0x58,v+user[i]+0x20);
  rx_addr object=U32(telemetry_sim(v),obj[i]);if(telemetry_water(object)){object=U32(telemetry_sim(v),obj[i]);SU32(D,0x60,U32(object,0x158));SU32(D,0x64,object);}else{SU32(D,0x60,0xffffffff);SU32(D,0x64,0);}
 }
 WORLD_VCALL(v,0x94,F+0x10);rx_v4 velocity=v4_ld(F+0x10);SF32(out,0x220,phys_sqrt(bike_hsum_021(v4_mul(velocity,velocity))));
 SF32(out,0x224,telemetry_virtual_float(v,0xbc));SF32(out,0x228,telemetry_virtual_float(v,0xc0));
 telemetry_quiet(out+0x22c,telemetry_sim(v)+0x5880);telemetry_quiet(out+0x230,telemetry_sim(v)+0x5884);
 SU32(out,0x234,WORLD_VCALL0(v,0xa8));SU32(out,0x244,WORLD_VCALL0(v,0xc8));telemetry_quiet(out+0x240,telemetry_sim(v)+0x9708);
 rx_addr s=telemetry_sim(v);uint32_t airborne=!U32(s,0x9244)&&!U32(s,0x9394)&&!U32(s,0x94e4)&&!U32(s,0x9634);
 SU32(out,0x250,0);SU32(out,0x248,airborne);SU32(out,0x258,U32(v,0x1310));SU32(out,0x25c,U32(v,0x1314));telemetry_quiet(out+0x254,v+0x131c);SU32(out,0x260,0);
 phys_security_check_cookie(U32(F,0x2c)^(F+12));world_frame_leave(&f);
}
RX_FUNCTION(0x00487510,rx_atv_snapshot_values)

/*00430250 RavenVehicle::vf39 is a tail JMP to virtual vf38, not a new
 * call frame. Keep entry ESP intact for the already-native concrete getter. */
void rx_vehicle_forward_angular_velocity(rx_addr v,rx_addr out){
 rx_addr target=U32(U32(v,0),0x98);
 void (*getter)(rx_addr,rx_addr)=RX_FN(target,void(*)(rx_addr,rx_addr));
 if(getter){getter(v,out);return;}
 // An unported concrete target remains an explicit boundary. The original
 // bridge constructs a call below its baseline; raise it so its entry ESP is
 // the inherited tail-jump entry, rather than inserting a second frame.
 rx_addr entry=rx_sp();rx_set_sp(entry+8);(void)rx_call_original(target,RX_THISCALL,v,1,&out);rx_set_sp(entry);
}
RX_FUNCTION(0x00430250,rx_vehicle_forward_angular_velocity)
