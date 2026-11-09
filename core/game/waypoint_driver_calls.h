#pragma once
#include "build_activity_util.h"
/* Float virtual arguments must use the native FP register class, not integer dispatch. */
static inline uint32_t waypoint_behavior_update(rx_addr object,float dt){
 rx_addr target=U32(U32(object,0),0x10);
 uint32_t (*f)(rx_addr,float)=RX_FN(target,uint32_t (*)(rx_addr,float));
 if(!f)act_unreached(target,"Waypoint behavior float update owner is not composed");
 uint32_t result;ACT_CALLR(result,1,f(object,dt));return result;
}
static inline void waypoint_rule_update(rx_addr object,float dt){
 rx_addr target=U32(U32(object,0),0x14);
 void (*f)(rx_addr,float)=RX_FN(target,void (*)(rx_addr,float));
 if(!f)act_unreached(target,"Waypoint rule float update owner is not composed");
 ACT_CALL(1,f(object,dt));
}
