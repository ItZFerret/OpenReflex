/*Complete derived WaypointRaceGate ownership, isolated from the generic MXTrigger.*/
#include "waypoint_gate.h"
#include "build_scene_int.h"
#include "build_activity_util.h"
#include "build_activity_int.h"
#include "anim.h"
#include "fx.h"
#include "fx_util.h"
#include "world_rider.h"
#include "world_veh.h"
#include "world_misc.h"
#include "phys_crt.h"
/*00404fd1 factory arm: exact base constructor, derived VT, zero-based gate slot.*/
rx_addr rx_waypoint_gate_create(void){rx_addr e=rx_reflex_aligned_alloc(0x290);if(e){rx_trigger_construct(e);SU32(e,0,0x938cd4);SU32(e,0x280,0);}return e;}
void rx_waypoint_trigger_grow(rx_addr vector,uint32_t capacity){
 ACT_FRAME(F,0,2);uint32_t allocation=capacity>UINT32_MAX/8?UINT32_MAX:capacity*8;SU32(F,-4,allocation);rx_set_sp(F-4);rx_addr data;ACT_CALLR(data,0,rx_reflex_aligned_alloc(allocation));
 /*The allocator's cdecl argument remains while memcpy pushes three more.*/
 uint32_t bytes=U32(vector,8)*8;SU32(F,-8,bytes);SU32(F,-12,U32(vector,0));SU32(F,-16,data);ACT_CALL(3,crt_memcpy(data,U32(vector,0),bytes));
 /*All four prior arguments remain while aligned-free pushes its argument.*/
 rx_set_sp(F-16);SU32(F,-20,U32(vector,0));ACT_CALL(1,rx_reflex_aligned_free(U32(vector,0)));rx_set_sp(F);SU32(vector,0,data);SU32(vector,8,capacity);ACT_END(F);
}
RX_FUNCTION(0x0041a4e0,rx_waypoint_trigger_grow)
/*00409990 base destruction retains its conditional scene removal and both listeners.*/
void rx_waypoint_trigger_destroy(rx_addr e){
 ACT_FRAME(F,0,1);SU32(e,0,0x9376cc);if(U32(e,0x214))ACT_CALL(1,rx_scene_manager_remove_entity(e,U32(e,0x10)));SU32(F,-4,U32(e,0xa4));ACT_CALL(1,rx_reflex_aligned_free(U32(e,0xa4)));rx_set_sp(F-4);SU32(F,-8,U32(e,0x90));ACT_CALL(1,rx_reflex_aligned_free(U32(e,0x90)));rx_set_sp(F);SU32(e,0,0x95a2e4);uint32_t arg=e;rx_addr listener=U32(0,0xa95824);if(listener)act_vcall(listener,4,1,&arg);listener=U32(0,0xa95828);if(listener)act_vcall(listener,4,1,&arg);ACT_END(F);
}
RX_FUNCTION(0x00409990,rx_waypoint_trigger_destroy)
rx_addr rx_waypoint_gate_sdtor(rx_addr e,uint32_t flags){ACT_FRAME(F,0,1);SU32(e,0,0x938cd4);ACT_CALL(1,rx_scene_manager_remove_entity(e,U32(e,0x10)));ACT_CALL(0,rx_waypoint_trigger_destroy(e));if(flags&1)ACT_CALL(1,rx_reflex_aligned_free(e));ACT_END(F);return e;}
RX_FUNCTION(0x0040af20,rx_waypoint_gate_sdtor)
void rx_waypoint_gate_load(rx_addr e,uint32_t reader){
 (void)reader;ACT_FRAME_EBP(F,8,8,2);ACT_CALL(1,rx_entity_vf5(e));SU32(e,0xb8,0);SU32(e,0xbc,0);SU32(e,0x20c,1);uint32_t index;SU32(F,-4,0x9361e8);ACT_CALLR(index,1,rs_uint("GateIndex"));SU32(F,0xc,index);SU32(e,0x280,index-1);rx_addr existing;ACT_CALLR(existing,2,rx_world_registry_find(U32(0,0xa89bc8),0x9361f4));if(!existing)act_event_type(0x41a8d0,U32(0,0xa89bc8),0);ACT_END(F);
}
RX_FUNCTION(0x0040af50,rx_waypoint_gate_load)
/*00419940 exact payload is {vehicle, zero-based gate index, gate}.*/
void rx_waypoint_gate_post(rx_addr vehicle,uint32_t index,rx_addr gate){
 const rx_addr entry=rx_sp(),E=entry-0x10;SU32(E,0xc,U32(0,0x9b45e4)^E);SU32(E,-4,fx_reg(FX_ESI));SU32(E,-8,fx_reg(FX_EDI));const fx_saved saved=fx_save();rx_addr channel=U32(0,0xa89bc8);fx_set_reg(FX_ESI,channel);SU32(E,0,vehicle);SU32(E,4,index);SU32(E,8,gate);rx_set_sp(E-8);fx_type_name(0x9cbc00,0x419976);fx_set_reg(FX_EDI,0x9361f4);const uint32_t args[2]={E,12};rx_addr sp=fx_push(2,args,0x419989);rx_world_event_dispatch(channel,0x9361f4,E,12);fx_pop(sp);fx_restore(saved);rx_set_sp(E);SU32(E,-4,0x419996);phys_security_check_cookie(U32(E,0xc)^E);rx_set_sp(entry);
}
RX_FUNCTION(0x00419940,rx_waypoint_gate_post)
void rx_waypoint_gate_occupant(rx_addr player,rx_addr gate){
 ACT_FRAME(F,0,3);rx_addr found;ACT_CALLR(found,1,rx_fx_trigger_find(player,gate));if(found){rx_addr vehicle=U32(U32(found,0),0x1ac);if(vehicle)ACT_CALL(1,rx_fx_trigger_post(vehicle,gate,0x93609c));SU32(found,4,1);}else{ACT_CALL(1,rx_fx_trigger_add(player,gate));rx_addr vehicle=U32(player,0x1ac);if(vehicle)ACT_CALL(1,rx_waypoint_gate_post(vehicle,U32(gate,0x280),gate));}ACT_END(F);
}
RX_FUNCTION(0x0040b170,rx_waypoint_gate_occupant)
void rx_waypoint_gate_tick(rx_addr gate,float dt){
 (void)dt;ACT_FRAME_EBP(F,16,0x34,3);for(int32_t i=0;i<I32(gate,0x94);i++)SU32(U32(gate,0x90),(uint32_t)i*8+4,0);uint32_t count=U8(U32(0,0xa946b8),0x1541c);SU32(F,0x14,0);SU32(F,0x1c,count);
 for(int32_t i=0;i<(int32_t)count;i++){rx_addr p;ACT_CALLR(p,2,rx_world_players_find(U32(0,0xa946b8)+0x7460,(uint32_t)i,3));rx_addr v=U32(p,0x1ac);if(v&&!U32(v,0x280)){uint32_t out=F+0x20;act_vcall(v,0x44,1,&out);float x=F32(F,0x20),y=F32(F,0x24),z=F32(F,0x28);
 float lx=rx_adds(rx_adds(rx_adds(rx_muls(F32(gate,0x250),y),rx_muls(F32(gate,0x260),z)),rx_muls(F32(gate,0x240),x)),F32(gate,0x270));float ly=rx_adds(rx_adds(rx_adds(rx_muls(F32(gate,0x244),x),rx_muls(F32(gate,0x254),y)),rx_muls(F32(gate,0x264),z)),F32(gate,0x274));float lz=rx_adds(rx_adds(rx_adds(rx_muls(F32(gate,0x248),x),rx_muls(F32(gate,0x258),y)),rx_muls(F32(gate,0x268),z)),F32(gate,0x278));SF32(F,0x30,lx);SF32(F,0x34,ly);SF32(F,0x38,lz);uint32_t inside;ACT_CALLR(inside,0,rx_fx_trigger_inside(gate,F+0x30));if(inside&255){SU8(gate,0x218,1);ACT_CALL(0,rx_waypoint_gate_occupant(p,gate));}}
  SU32(F,0x14,(uint32_t)i+1);
 }
 ACT_CALL(0,rx_fx_trigger_prune(gate));ACT_END(F);
}
RX_FUNCTION(0x0040afd0,rx_waypoint_gate_tick)
