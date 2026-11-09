#ifndef RX_ORACLE
#include "career_completion_host.h"
#include "build_activity_util.h"
#include "career_properties.h"
#include "world_misc.h"
/* This service deliberately has no original address registration. It records an
 * owned host witness AFTER real typed dispatch; it does not emulate Lua or add
 * a fake guest proxy. The session unbinds it before teardown/static restoration. */
static struct {
 void*context;uint64_t generation;rx_addr channel,activity,handler,type_name;
 rx_career_post_witness witness;
} bound;
uint32_t rx_career_completion_bind(void*context,uint64_t generation,rx_addr channel,
 rx_addr activity,rx_career_post_witness witness){
 if(!context||!generation||!channel||!activity||!witness||bound.context)return 0;
 rx_addr handler;ACT_CALLR(handler,2,rx_world_registry_find(channel,0x941c48));
 const rx_addr type=crt_type_info_name(0x9cbd24,0x9e1a7c);
 if(!handler||!U32(handler,4)||crt_strcmp(U32(handler,4),type))return 0;
 bound.context=context;bound.generation=generation;bound.channel=channel;
 bound.activity=activity;bound.handler=handler;bound.type_name=type;bound.witness=witness;return 1;
}
void rx_career_completion_unbind(void*context){if(bound.context==context)memset(&bound,0,sizeof bound);}
void rx_career_completion_observe(rx_addr channel,rx_addr name,rx_addr payload,uint32_t size){
 if(!bound.context||channel!=bound.channel||size!=4||!payload||!name)return;
 if(crt_strcmp(name,0x941c48)||!U32(bound.handler,4)||
    crt_strcmp(U32(bound.handler,4),bound.type_name))return;
 const rx_addr activity=U32(payload,0);if(activity!=bound.activity)return;
 /* Caller is still the native owner thread. Callback only records immutable
  * identity/witness flags, never invokes another game action from dispatch. */
 bound.witness(bound.context,bound.generation,activity);
}
/* Semantic body inlined by original SWIG00735da0 at00735e7a..99. The Lua stack
 * conversion/error wrapper is not exposed or registered as ported. This is the
 * exact two native effects with existing owned property service. */
void rx_career_completion_enable_tour(rx_addr manager){
 SU32(manager,0x190,1);
 rx_owned_property_set_string(0x94c9f4,U32(0x9d2374,4));
}

#endif /* product-owned bridge; no original guest ABI */
