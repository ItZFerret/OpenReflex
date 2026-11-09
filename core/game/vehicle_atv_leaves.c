/* Small original ATV control, contact, and presentation virtual methods. */
#include "tuning_reload_log.h"
#include "vehicle_atv_leaves.h"
#include "build_vehicle_util.h"
#include "bike_math.h"
#include "world_veh.h"
#include "rx_vcall.h"
#include "phys_crt.h"
#include "build_xml.h"
#include "build_xml_stream.h"
#include "build_vehicle_int.h"
#include "../src/rx_os.h"
#include "loop_jobs.h"
void rx_atv_vehicle_set_control_mode(rx_addr v,uint32_t mode){SU32(U32(v,0x1700),4,mode);}
RX_FUNCTION(0x0047cbb0,rx_atv_vehicle_set_control_mode)
void rx_atv_clear_contact_history(rx_addr s){for(unsigned i=0;i<4;i++)SU32(s,0x9244+0x150*i,0);}
RX_FUNCTION(0x0047c440,rx_atv_clear_contact_history)
rx_addr rx_atv_vehicle_body_angular_velocity(rx_addr v,rx_addr out){rx_st4(out,rx_ld4(U32(v,0x1700)+0x7a0));return out;}
RX_FUNCTION(0x00486600,rx_atv_vehicle_body_angular_velocity)
rx_f80 rx_atv_vehicle_max_rpm(rx_addr v){return rx_f80_from_f32(F32(U32(v,0x1700),0x4bfc));}
RX_FUNCTION(0x00486640,rx_atv_vehicle_max_rpm)
uint32_t rx_atv_vehicle_clutch_override(rx_addr v){return U32(U32(v,0x1700),0x597c)==2;}
RX_FUNCTION(0x00486790,rx_atv_vehicle_clutch_override)
rx_f80 rx_atv_vehicle_wheel_load(rx_addr v,uint32_t axle){
 rx_addr s=U32(v,0x1700);float x;
 if(axle==0)x=phys_mul(phys_add(F32(s,0x9370),F32(s,0x9220)),0.5f);
 else if(axle==1)x=phys_mul(phys_add(F32(s,0x9610),F32(s,0x94c0)),0.5f);
 else x=phys_mul(phys_add(phys_add(phys_add(F32(s,0x9610),F32(s,0x94c0)),F32(s,0x9370)),F32(s,0x9220)),0.25f);
 return rx_f80_from_f32(x);
}
RX_FUNCTION(0x004867b0,rx_atv_vehicle_wheel_load)
uint32_t rx_atv_vehicle_has_wheel_tag(rx_addr v,uint32_t tag){return U32(v,0x1750)==tag||U32(v,0x1754)==tag||U32(v,0x1758)==tag||U32(v,0x175c)==tag;}
RX_FUNCTION(0x00487af0,rx_atv_vehicle_has_wheel_tag)
uint32_t rx_atv_vehicle_collision_type(rx_addr v,uint32_t tag){
 const rx_addr sp=rx_sp();rx_set_sp(sp-4);
 uint32_t hit=rx_vcall(v,0xd4,1,&tag),r=hit?0x20:rx_vcall(v,0x30,0,NULL);rx_set_sp(sp);return r;
}
RX_FUNCTION(0x004744c0,rx_atv_vehicle_collision_type)
uint32_t rx_atv_body_contact_bits(rx_addr s){uint32_t r=0;for(unsigned i=0;i<6;i++){r|=U32(s,0x1af0+4*i);r|=U32(s,0x1b54+4*i);}return r;}
RX_FUNCTION(0x0048d2a0,rx_atv_body_contact_bits)
uint32_t rx_atv_needs_contact_resolution(rx_addr s){
 if(rx_atv_body_contact_bits(s))return 1;
 rx_v4 p=rx_ld4(s+0x2ec0),v=rx_ld4(s+0x100);
 int impulse=bike_all3(v4_cmplt(rx_ld4(0xd61670),v4_splat(phys_sqrt(bike_hsum_021(v4_mul(p,p))))));
 int slow=bike_all3(v4_cmplt(v4_splat(phys_sqrt(bike_hsum_021(v4_mul(v,v)))),v4_splat(4)));
 return impulse&&slow&&!(U32(s,0x9630)|U32(s,0x94e0));
}
RX_FUNCTION(0x0048d160,rx_atv_needs_contact_resolution)
uint32_t rx_atv_vehicle_outside_terrain(rx_addr v){
 const rx_addr sp=rx_sp(),F=((sp-4)&~15u)-0x20;rx_set_sp(F);veh_cookie(F,0x1c);
 rx_vcall(v,0x78,1,&F);
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 rx_addr w;PHYS_CALL_SP(0,w=rx_world_find_by_controller(0x9e30d4));
 float pad=F32(w,0x44f0),x=F32(F,0),z=F32(F,8);
 uint32_t r=F32(w,0x44d8)>phys_add(pad,x)||phys_sub(x,pad)>F32(w,0x44e0)||F32(w,0x44dc)>phys_add(pad,z)||phys_sub(z,pad)>F32(w,0x44e4);
 phys_security_check_cookie(U32(F,0x1c)^F);rx_set_sp(sp);return r;
}
RX_FUNCTION(0x0046a2c0,rx_atv_vehicle_outside_terrain)

/* 0048d320, S8. The product consumes imported tuning and has no developer
 * file watcher. Preserve the pending/retry flags nevertheless. The optional
 * guest CRT stream and log sink remain explicit development-only boundaries;
 * they must not silently discard a pending reload in a native build. */
void rx_atv_apply_pending_tuning(rx_addr s){
 const rx_addr sp=rx_sp(),B=((sp-4)&~7u)-0x46c,F=B-12,R=F+0x10;
 const rx_addr pending=0x9e3100,flag=0x9cc8b0;
 rx_set_sp(F);veh_cookie(B,0x468);
 if(U32(0,pending)&&U32(0,flag)){
  const uint32_t cs=0xad7538;
  rx_os_import(0x91600c,1,&cs);
  PHYS_CALL_SP(4,rx_xml_reader_construct(R));
  uint32_t loaded;
  /* 008252c0 (stdcall reader, the pending stream object; RET 8): build_xml_stream.c */
  PHYS_CALL_SP(8,loaded=rx_xml_reader_load_stream(R,U32(0,pending)));
  if(loaded){
   uint32_t ok;
   PHYS_CALL_SP(8,ok=rx_veh_params_apply_typed(s+0x2f10,R,0x93c560,0));
   if(ok){
    rx_tuning_reload_log(0,0x0048d3a2,0x0048d3ab);          /* the log object (0042fed0) and its type-10 packet (00790540) */
    void (*masses)(rx_addr)=RX_FN(0x48c750,void(*)(rx_addr));
    PHYS_CALL_SP(0,masses(s));
    rx_addr file=U32(0,pending);
    if(file){const uint32_t one=1;rx_vcall(file,0,1,&one);}
    SU32(0,pending,0);SU32(0,flag,1);
   }else{SU32(0,flag,0);SU32(U32(0,pending),0xc,0);}
  }
  rx_os_import(0x9160f8,1,&cs);
  PHYS_CALL_SP(0,rx_xml_reader_destroy(R));
 }
 if(!U32(0,pending))SU32J(0,flag,1);   /* (every ATV simulation stores it: SU32J, loop_jobs.h) */
 phys_security_check_cookie(U32(B,0x468)^B);rx_set_sp(sp);
}
RX_FUNCTION(0x0048d320,rx_atv_apply_pending_tuning)
