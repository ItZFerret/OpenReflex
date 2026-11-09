/* 005485e0: gated stick throttle/brake axis (controller index in ESI). */
#include "controller_axis_query.h"
#include "loop_util.h"
float rx_controller_stick_drive_axis(uint32_t controller) {
 rx_addr action=U32(0,0x009c52c4u+4u*controller)+0x127c;
 uint32_t (*available)(rx_addr,uint32_t)=RX_FN(U32(U32(action,0),0x28),uint32_t (*)(rx_addr,uint32_t));
 const uint32_t a[]={controller};rx_addr saved;uint32_t yes;
 if(available){saved=loop_push(1,a,0x005485f5);yes=available(action,controller);loop_pop(saved);}
 else yes=rx_call_original(U32(U32(action,0),0x28),RX_THISCALL,action,1,a);
 if(!yes)return 0.0f;
 /* The first virtual call can mutate the pad table; reload it as the original. */
 action=U32(0,0x009c52c4u+4u*controller)+0x127c;
 float (*value)(rx_addr,uint32_t,uint32_t)=RX_FN(U32(U32(action,0),0x10),float (*)(rx_addr,uint32_t,uint32_t));
 const uint32_t b[]={7,controller};
 if(value){saved=loop_push(2,b,0x00548610);float result=value(action,7,controller);loop_pop(saved);return result;}
 rx_regs in={0},out;in.r[1]=action;in.r[6]=controller;
 rx_call_original_ex(U32(U32(action,0),0x10),&in,&out,2,b);return out.st0;
}
RX_FUNCTION(0x005485e0,rx_controller_stick_drive_axis)

/* 00811c90 CompoundKeyMap vf10: any nonnull child reports enabled. */
uint32_t rx_controller_compound_available(rx_addr action,uint32_t controller) {
 const rx_addr entry=rx_sp();rx_set_sp(entry-12);
 for(uint32_t i=0;i<4;i++) {
  rx_addr child=U32(action,0xa4+4*i);
  if(!child)continue;
  uint32_t (*enabled)(rx_addr,uint32_t)=RX_FN(U32(U32(child,0),0x28),uint32_t (*)(rx_addr,uint32_t));
  const uint32_t args[]={controller};uint32_t yes;
  if(enabled){rx_addr saved=loop_push(1,args,0x00811caf);yes=enabled(child,controller);loop_pop(saved);}
  else yes=rx_call_original(U32(U32(child,0),0x28),RX_THISCALL,child,1,args);
  if(yes){rx_set_sp(entry);return 1;}
 }
 rx_set_sp(entry);return 0;
}
RX_FUNCTION(0x00811c90,rx_controller_compound_available)
