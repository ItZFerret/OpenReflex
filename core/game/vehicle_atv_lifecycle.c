/* Original ATV lifecycle/listener methods. Explicit unported simulation cleanup
 * and optional timer-effect boundaries are retained, never replaced by skipped cleanup. */
#include "vehicle_atv_lifecycle.h"
#include "world_veh_util.h"
#include "world_veh.h"
#include "world_veh_transfer.h"
#include "anim_scenario.h"
#include "anim_scenario_misc.h"
#include "anim.h"
#include "build_alloc.h"
#include "bike_vehicle.h"
#include "phys_terrain.h"
#include "../src/rx_os.h"
/* Staged ATV lifecycle source; no cleanup or optional effect is silently skipped. */
static void life_entity_remove(rx_addr v){for(unsigned i=0;i<2;i++){rx_addr manager=U32(0,0xa95824+4*i);if(manager)WORLD_VCALL(manager,4,v);}}
void rx_vehicle_base_destruct(rx_addr v){
 world_frame f=world_frame_flat(3,0);
 SU32(v,0,0x93c74c);SU32(v,0x90,0x93c84c);SU32(v,0x94,0x93c85c);SU32(v,0x9c,0x93c89c);
 rx_addr tree=v+0x6a0;WORLD_CALL_SP(4,rx_tree_free_subtree(tree,U32(U32(v,0x6b8),4)));
 SU32(U32(tree,0x18),4,U32(tree,0x18));SU32(tree,0x1c,0);SU32(U32(tree,0x18),0,U32(tree,0x18));SU32(U32(tree,0x18),8,U32(tree,0x18));
 WORLD_CALL_SP(0,rx_rider_tree_release(tree));
 rx_addr handle=U32(v,0x17c);SU32(v,0x178,0x93a650);
 if(handle){SU32(handle,0,U32(handle,0)-1);if(!U32(handle,0)){handle=U32(v,0x17c);if(handle){WORLD_CALL_SP(4,rx_dyn_records_remove(U32(handle,4)+0x1954,U32(handle,8)));WORLD_CALL_SP(4,rx_reflex_aligned_free(handle));}}}
 for(uint32_t off=0xa0;off<=0xa8;off+=4)WORLD_CALL_SP(4,rx_reflex_aligned_free(U32(v,off)));
 SU32(v,0x94,0x93708c);SU32(v,0x90,0x93cbf4);SU32(v,0,0x95a2e4);life_entity_remove(v);world_frame_leave(&f);
}
rx_addr rx_atv_vehicle_delete(rx_addr v,uint32_t flags){
 world_frame f=world_frame_flat(1,0);
 SU32(v,0,0x93d0ec);SU32(v,0x90,0x93d1f0);SU32(v,0x94,0x93d204);SU32(v,0x9c,0x93d244);
 WORLD_CALL_SP(0,rx_vehicle_base_destruct(v));
 if(flags&1){if(U8(0,0xda67d8))RX_OS(0x91600c,0xd61704);WORLD_CALL_SP(4,rx_crt_aligned_free(v));if(U8(0,0xda67d8))RX_OS(0x9160f8,0xd61704);}
 world_frame_leave(&f);return v;
}
void rx_vehicle_base_detach(rx_addr v){
 world_frame f=world_frame_flat(1,0);SU32(v,0x190,0);
 if(U32(v,0x188)){WORLD_CALL_SP(4,rx_world_physics_remove_listener(U32(U32(v,0x188),0x20134),v+0x90));SU32(v,0x188,0);}
 rx_set_sp(rx_sp()-4);const rx_addr model=U32(v,0x1328);
 if(model){WORLD_CALL_SP(0,rx_model_instance_destroy(model,0));WORLD_CALL_SP(4,rx_reflex_aligned_free(model));}
 SU32(v,0x70,0);rx_set_sp(rx_sp()+4);life_entity_remove(v);WORLD_CALL_SP(4,rx_reflex_aligned_free(U32(v,0x18)));world_frame_leave(&f);
}
static void life_free_slot(uint32_t word,rx_addr t1,rx_addr t2,rx_addr t3){if(word&0x1000)WORLD_CALL_SP(0,rx_tracks_free_slot(t1,(int32_t)(word^0x1000)));else if(word&0x2000)WORLD_CALL_SP(0,rx_tracks_free_slot(t2,(int32_t)(word^0x2000)));else WORLD_CALL_SP(0,rx_tracks_free_slot(t3,(int32_t)(word^0x4000)));}
static void life_cut_strip(rx_addr tracks,uint32_t word){if((int32_t)word>=0){const int32_t index=word^0x4000;if(index>=0&&index<I32(tracks,0xc)){WORLD_CALL_SP(0,phys_tracks_cut_strip(tracks,index));SU32(U32(tracks,0x24)+(uint32_t)index*0xc0,0x98,1);SU32(tracks,0x28,U32(tracks,0x28)-1);}}}
void rx_atv_vehicle_detach(rx_addr v){
 world_frame f=world_frame_flat(2,8);const uint32_t scene=U32(U32(v,0x188),0x20138);SU32(f.esp,0xc,v);
 if(!(U32(0,0x9e30e0)&1)){SU32(0,0x9e30e0,U32(0,0x9e30e0)|1);SU32(0,0x9e30dc,0);}
 rx_addr terrain=0;for(uint32_t i=0;i<U32(0,0x9e30dc);i++){rx_addr t=U32(0,0x9e30d4+4*i);if(t&&scene==U32(t,0x44a8)){terrain=t;break;}}
 if(terrain){rx_set_sp(rx_sp()-8);const rx_addr t1=U32(0,0xa95774),t2=U32(0,0xa95778),t3=U32(0,0xa9577c);
  for(unsigned i=0;i<4;i++)life_free_slot(U32(v,0x1584+4*i),t1,t2,t3);
  SU32(rx_sp(),0x10,25);for(unsigned i=0;i<25;i++){life_cut_strip(t3,U32(v,0x14c0+4*i));life_cut_strip(t3,U32(v,0x11b0+4*i));SU32(rx_sp(),0x10,24-i);}rx_set_sp(rx_sp()+8);
 }
 WORLD_CALL_SP(4,rx_world_physics_remove_simulation(U32(U32(v,0x188),0x20134),U32(v,0x1700)));
 if(U32(v,0x1700))WORLD_VCALL(U32(v,0x1700),0,1); /* Actual S destructor boundary retained until owned cleanup is ported. */
 SU32(v,0x1700,0);SU32(v,0x234,0);
 if(U8(v,0x174))WORLD_VCALL0(v+0x178,4);
 world_frame_leave(&f);rx_vehicle_base_detach(v); /* original tail-jump, same entry ESP */
}
void rx_atv_listener_pre_step(rx_addr listener,float dt){
 world_frame f=world_frame_flat(1,0);
 WORLD_CALL_SP(4,bike_vehicle_pre_step(listener,rx_bf(world_x87_pass(rx_fb(dt)))));
 if(U32(listener,0x1674)){SF32(listener,0x1684,phys_add(F32(listener,0x1684),dt));if(U32(listener,0x1680))WORLD_CALL_SP(4,rx_world_key_track_block_sample(listener+0x1680));}
 rx_addr player=U32(listener,0x100);
 if(U32(listener,0x1f0)&&player&&F32(player,0xb38)>0){rx_addr sim=U32(listener,0x1670);SF32(sim,0x28,0);SF32(sim,0x24,0);uint32_t mode=U32(U32(listener,0x100),0xb44);float delta=0;int lean=0;
  switch(mode){case 2:case 3:SF32(U32(listener,0x1670),0x1c,1);break;case 5:case 6:SF32(U32(listener,0x1670),0x1c,0);break;case 8:delta=.25f;lean=1;break;case 9:delta=.5f;lean=1;break;case 11:delta=-.25f;lean=1;break;case 12:delta=-.5f;lean=1;break;default:break;}
  if(lean){rx_addr p=U32(listener,0x1670)+0x1c;float val=delta>0?phys_add(F32(p,0x10),delta):phys_sub(F32(p,0x10),-delta);SF32(p,0x10,val);if(val>1)val=1;SF32(p,0x10,val);float lo=F32(0,0x9cd9e8);if(val<lo)val=lo;SF32(p,0x10,val);}
 }
 for(unsigned i=0;i<2;i++){rx_addr input=WORLD_VCALL0(U32(0,0xa95844),8);if(WORLD_VCALL(input,0x7c,i?0x33:0x23,0)){rx_addr sim=U32(listener,0x1670);if(!i)SU32(sim,0x9838,U32(sim,0x9838)==0);SU32(U32(listener,0x1670),0x983c,1);}}
 world_frame_leave(&f);
}

RX_FUNCTION(0x0045d3c0,rx_vehicle_base_destruct)
RX_FUNCTION(0x0047c780,rx_atv_vehicle_delete)
RX_FUNCTION(0x00460230,rx_vehicle_base_detach)
RX_FUNCTION(0x0047cbc0,rx_atv_vehicle_detach)
RX_FUNCTION(0x0047d670,rx_atv_listener_pre_step)
