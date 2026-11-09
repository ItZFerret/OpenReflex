/* Complete simulation teardown. All three original392-byte bodies differ only
 * in their derived vtable, Buggy's -16 member layout, and call return addresses.
 * Reuse native allocation/registry/wheel/vector/mechanism owners. */
#include "fourwheel_destroy.h"
#include "vehicle_atv_destroy.h"
#include "build_alloc.h"
#include "build_rider.h"
#include "build_vehicle.h"
#include "loop_vehicle.h"
#include "fx_util.h"
typedef struct {rx_addr entry,wrapper,vtable;uint32_t delta;} destroy_layout;
static const destroy_layout layouts[]={
 {0x4cc7f0,0x4cbf00,0x93d43c,0},
 {0x4b1ff0,0x4b16d0,0x93d24c,0x10},
 {0x450c90,0x450350,0x93c93c,0}
};
static void destruct(rx_addr sim,unsigned family){
 const destroy_layout*l=&layouts[family];fx_frame frame;fx_frame_flat(&frame,0,FX_ESI);fx_set_reg(FX_ESI,sim);SU32(sim,0,l->vtable);
 const uint32_t arrays[]={0x85f4,0x85f8,0x85fc,0x85e8,0x85ec,0x85f0,0x85dc,0x85e0,0x85e4,0x85d0,0x85d4,0x85d8};
 for(unsigned i=0;i<12;i++){
  rx_addr p=U32(sim,arrays[i]-l->delta);fx_set_reg(i%3==0?FX_EAX:i%3==1?FX_ECX:FX_EDX,p);
  /* cdecl frees accumulate12 caller arguments, then one add esp,0x30. */
  rx_set_sp(rx_sp()-4);SU32(rx_sp(),0,p);rx_addr before=fx_push(0,0,l->entry+0x15+12*i);rx_reflex_aligned_free(p);fx_pop(before);
 }
 rx_set_sp(frame.E);rx_addr part=sim+0x8040-l->delta;fx_set_reg(FX_ECX,part);SU32(part,0,0x976750);
 rx_addr before=fx_push(0,0,l->entry+0xad);rx_registry_remove(part);fx_pop(before);
 static const uint16_t wheels[]={0x76c0,0x6d60,0x6400,0x5aa0};
 for(unsigned i=0;i<4;i++){part=sim+wheels[i]-l->delta;fx_set_reg(FX_EAX,part);before=fx_push(0,0,l->entry+0xb8+11*i);rx_vehicle_wheel_block_destruct(part);fx_pop(before);}
 fx_set_reg(FX_EAX,0x93c9e0);static const uint16_t curves[]={0x5a10,0x59d0,0x5990,0x5950,0x5910,0x5900,0x58c0,0x5880,0x5840,0x5800,0x57f0};
 for(unsigned i=0;i<11;i++)SU32(sim,curves[i]-l->delta,0x93c9e0);
 fx_set_reg(FX_EAX,1);SU32(sim,0x5780-l->delta,0x93cb44);
 if(!(U32(0,0xd72ad4)&1)){
  SU32(0,0xd72ad4,U32(0,0xd72ad4)|1);before=fx_push(0,0,l->entry+0x142);rx_veh_tuning_registry_init();fx_pop(before);
  const uint32_t handler=0x911ed0;before=fx_push(1,&handler,l->entry+0x14c);
#ifdef RX_ORACLE
  (void)rx_call_original(0x8f9fbf,RX_CDECL,0,1,&handler);
#else
  extern uint32_t rx_crt_atexit(uint32_t);(void)rx_crt_atexit(handler);
#endif
  fx_pop(before);
 }
 SU32(0,0xd92cd4,0);SU32(sim,0,0x93cc14);fx_set_reg(FX_ECX,sim+0x20d0);before=fx_push(0,0,l->entry+0x16a);rx_registry_remove(sim+0x20d0);fx_pop(before);
 fx_set_reg(FX_EAX,sim+0x1300);before=fx_push(0,0,l->entry+0x175);rx_vehicle_sim_vector_destruct(sim+0x1300);fx_pop(before);
 fx_set_reg(FX_EAX,sim+0xd0);before=fx_push(0,0,l->entry+0x180);rx_vehicle_mechanism_destruct(sim+0xd0);fx_pop(before);fx_set_reg(FX_ECX,0x93c9e0);
 SU32(sim,0,0x93c9d0);fx_frame_leave(&frame);
}
static rx_addr deleting(rx_addr sim,uint32_t flags,unsigned family){
 const destroy_layout*l=&layouts[family];fx_frame frame;fx_frame_flat(&frame,0,FX_ESI);fx_set_reg(FX_ESI,sim);
 rx_addr before=fx_push(0,0,l->wrapper+8);destruct(sim,family);fx_pop(before);
 if(flags&1){before=fx_push(1,&sim,l->wrapper+0x15);rx_reflex_aligned_free(sim);fx_pop(before);}
 fx_set_reg(FX_EAX,sim);fx_frame_leave(&frame);return sim;
}
void rx_fourwheel_sim_destruct_utv(rx_addr s){destruct(s,0);}
void rx_fourwheel_sim_destruct_buggy(rx_addr s){destruct(s,1);}
void rx_fourwheel_sim_destruct_truck(rx_addr s){destruct(s,2);}
rx_addr rx_fourwheel_sim_delete_utv(rx_addr s,uint32_t f){return deleting(s,f,0);}
rx_addr rx_fourwheel_sim_delete_buggy(rx_addr s,uint32_t f){return deleting(s,f,1);}
rx_addr rx_fourwheel_sim_delete_truck(rx_addr s,uint32_t f){return deleting(s,f,2);}
RX_FUNCTION(0x004cc7f0,rx_fourwheel_sim_destruct_utv)
RX_FUNCTION(0x004b1ff0,rx_fourwheel_sim_destruct_buggy)
RX_FUNCTION(0x00450c90,rx_fourwheel_sim_destruct_truck)
RX_FUNCTION(0x004cbf00,rx_fourwheel_sim_delete_utv)
RX_FUNCTION(0x004b16d0,rx_fourwheel_sim_delete_buggy)
RX_FUNCTION(0x00450350,rx_fourwheel_sim_delete_truck)
