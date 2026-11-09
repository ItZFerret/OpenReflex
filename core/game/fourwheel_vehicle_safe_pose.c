/* Complete 473B safe-pose algorithm. Three full originals have identical
 * instruction/control structure; only the explicitly audited family fields
 * below differ. It is one canonical algorithm, not an address-offset alias. */
#include "fourwheel_vehicle_runtime_leaves.h"
#include "fx_util.h"
typedef struct{rx_addr entry,sim_field;uint32_t wheel_delta;} safe_layout;
static const safe_layout layouts[3]={{0x4c6a60,0x1540,0},{0x4ac100,0x15c0,0x10},{0x473030,0x15d0,0}};
static void child(unsigned family,unsigned role,rx_addr target,rx_addr ra){
 const rx_addr before=fx_push(0,0,ra);rx_fourwheel_safe_pose_dependency(family,role,target,before);fx_pop(before);
}
static uint32_t safe_pose(rx_addr v,rx_addr position,rx_addr orientation,uint32_t threshold_bits,unsigned family){
 const safe_layout*l=&layouts[family];fx_frame frame;fx_frame_enter(&frame,0x44,16,FX_EBX,FX_ESI,FX_EDI);const rx_addr b=frame.E;
 fx_set_reg(FX_ESI,v);fx_set_reg(FX_ECX,v);fx_set_reg(FX_EAX,U32(v,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xb4));
 child(family,0,fx_reg(FX_EDX),l->entry+0x18);if(!fx_reg(FX_EAX))goto reject;
 fx_set_reg(FX_EAX,U32(v,0));fx_set_reg(FX_EDX,U32(fx_reg(FX_EAX),0xb8));fx_set_reg(FX_ECX,v);
 child(family,1,fx_reg(FX_EDX),l->entry+0x2c);if(fx_reg(FX_EAX))goto reject;
 fx_set_reg(FX_EDI,0);if(U32(v,0x280))goto reject;
 fx_set_reg(FX_EAX,U32(v,0x234));if(U32(fx_reg(FX_EAX),0xc))goto reject;
 fx_set_reg(FX_EDX,U32(U32(v,l->sim_field),0x1318));fx_set_reg(FX_ECX,0);
 if((int32_t)fx_reg(FX_EDX)>0){
  fx_set_reg(FX_EAX,U32(v,l->sim_field)+0x1af0);
  do{rx_addr at=fx_reg(FX_EAX);const uint32_t high=U32(at,0x64),low=U32(at,0);fx_set_reg(FX_EBX,high|low);fx_set_reg(FX_EAX,at+4);fx_set_reg(FX_ECX,fx_reg(FX_ECX)|fx_reg(FX_EBX));fx_set_reg(FX_EDX,fx_reg(FX_EDX)-1);}while(fx_reg(FX_EDX));
  if(fx_reg(FX_ECX))goto reject;
 }
 SU32(b,0xc,fx_reg(FX_EDI));
 if(!(U8(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,fx_reg(FX_EDI));}
 fx_set_reg(FX_EDX,0x9e30d4);child(family,2,0x402dc0,l->entry+0xab);
 fx_set_reg(FX_EDI,fx_reg(FX_EAX));fx_set_reg(FX_EBX,v+0xc0);fx_set_reg(FX_EAX,0);fx_set_reg(FX_EDX,fx_reg(FX_EBX));
 child(family,3,0x7c5120,l->entry+0xbc);if(fx_reg(FX_EAX)==6)goto reject;
 {
  const rx_addr terrain=fx_reg(FX_EDI),point=fx_reg(FX_EBX);const float x=F32(point,0),radius=F32(terrain,0x44f0);
  /* JA rejects only ordered greater; unordered bounds continue. */
  if(F32(terrain,0x44d8)>phys_add(x,radius)||phys_sub(x,radius)>F32(terrain,0x44e0))goto reject;
  const float z=F32(point,8);
  if(F32(terrain,0x44dc)>phys_add(z,radius)||phys_sub(z,radius)>F32(terrain,0x44e4))goto reject;
 }
 fx_set_reg(FX_EAX,U32(v,l->sim_field));
 rx_st4(b+0x10,rx_ld4(fx_reg(FX_EAX)+0x81b0-l->wheel_delta));
 rx_st4(b+0x20,rx_ld4(fx_reg(FX_EAX)+0x8300-l->wheel_delta));
 rx_st4(b+0x30,rx_ld4(fx_reg(FX_EAX)+0x8450-l->wheel_delta));
 rx_st4(b+0x40,rx_ld4(fx_reg(FX_EAX)+0x85a0-l->wheel_delta));
 {
  const float threshold=rx_bf(threshold_bits);
  /* COMISS/JBE includes unordered: every Y must be strictly greater. */
  if(!(F32(b,0x14)>threshold)||!(F32(b,0x24)>threshold)||!(F32(b,0x34)>threshold)||!(F32(b,0x44)>threshold)){
   fx_set_reg(FX_ECX,U32(b,0xc));fx_set_reg(FX_EAX,fx_reg(FX_ECX));fx_frame_leave(&frame);return fx_reg(FX_EAX);
  }
 }
 {
  const rx_v4 p=rx_ld4(fx_reg(FX_EAX)+0x180);
  fx_set_reg(FX_EDX,position);fx_set_reg(FX_EAX,orientation);rx_st4(position,p);
  fx_set_reg(FX_EDX,U32(v,l->sim_field));const rx_v4 q=rx_ld4(fx_reg(FX_EDX)+0x7c0);
  fx_set_reg(FX_ECX,1);rx_st4(orientation,q);fx_set_reg(FX_EAX,1);fx_frame_leave(&frame);return 1;
 }
reject:fx_set_reg(FX_EAX,0);fx_frame_leave(&frame);return 0;
}
uint32_t rx_utv_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t t,uint32_t ignored){(void)ignored;return safe_pose(v,p,q,t,0);}RX_FUNCTION(0x004c6a60,rx_utv_vehicle_safe_pose)
uint32_t rx_buggy_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t t,uint32_t ignored){(void)ignored;return safe_pose(v,p,q,t,1);}RX_FUNCTION(0x004ac100,rx_buggy_vehicle_safe_pose)
uint32_t rx_truck_vehicle_safe_pose(rx_addr v,rx_addr p,rx_addr q,uint32_t t,uint32_t ignored){(void)ignored;return safe_pose(v,p,q,t,2);}RX_FUNCTION(0x00473030,rx_truck_vehicle_safe_pose)
