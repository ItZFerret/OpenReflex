/* Finite Single-only dispatch/registration. No fallback to unported event classes. */
#include "career_single.h"
#include "career_moto.h"
#include "career_heat.h"
#include "career_single_events.h"
#include "build_activity_util.h"
#include <string.h>
static rx_career_single_dependencies dependencies;
static uint32_t bound;
static const rx_career_single_dependencies* deps(void) {
    if(!bound)act_unreached(0x5afbd0,"Single native dependencies not installed");
    return &dependencies;
}
void rx_career_single_install_dependencies(const rx_career_single_dependencies* d) {
    if(!d||!d->manager||!d->player_table||!d->apply_player_states||
       !d->request_player_states||!d->refresh_standings||!d->standings||
       !d->clear_standings||!d->seed_standings||!d->ui_get_int||
       !d->ui_string||!d->ui_int||!d->ui_event||!d->ui_goto_hub)
        act_unreached(0x5afbd0,"incomplete Single native composition");
    dependencies=*d;bound=1;
}
void rx_career_single_clear_dependencies(void) {
    memset(&dependencies,0,sizeof dependencies);bound=0;
}
/*Optional factored inline initializer; whole manager construction retains its
 * existing canonical build_series_init.c owner. Do not register this separately.*/
void rx_career_single_construct_at(rx_addr event) {
    SU32(event,0x2c,0);SU32(event,0x30,0);
    SU32(event,8,10);SU32(event,0x20,1);
    SU32(event,0,0x94e36c);
    SU32(event,0xb0,0);SU32(event,0xb4,3);
}
rx_addr rx_career_event_scalar_delete(rx_addr event,uint32_t flags) {
    SU32(event,0,0x94e21c);
    if(flags&1)rx_reflex_aligned_free(event);
    return event;
}
uint32_t rx_career_single_event_call(rx_addr event,uint32_t slot,uint32_t argument) {
    if(U32(event,0)==0x94e2fc)return rx_career_moto_event_call(event,slot,argument,deps());
    if(U32(event,0)==0x94e28c)return rx_career_heat_event_call(event,slot,argument,deps());
    if(U32(event,0)!=0x94e36c)
        act_unreached(0x5b6950,"unsupported career event class (Single only)");
    const rx_career_single_dependencies* d=deps();
    switch(slot) {
    case 0:case 20:return rx_career_event_current_activity_type(event);
    case 1:return rx_career_single_player_state(argument);
    case 2:rx_career_single_holeshot(event,argument,d);break;
    case 3:return rx_career_event_scalar_delete(event,argument);
    case 4:rx_career_event_initialize(event);break;
    case 5:rx_career_single_reset(event,d);break;
    case 6:rx_career_event_uninitialize(event,d);break;
    case 7:rx_career_single_bind(event,argument,1,d);break;
    case 8:rx_career_single_bind(event,argument,0,d);break;
    case 9:rx_career_event_gate(event,d);break;
    case 10:rx_career_single_gate_result(event,argument,d);break;
    case 11:rx_career_event_post_result(event,d);break;
    case 12:rx_career_single_advance(event,d);break;
    case 13:rx_career_single_post_event(event,d);break;
    case 14:rx_career_single_set_holeshot_player(event,argument);break;
    case 15:rx_career_single_set_type(event,argument);break;
    case 16:rx_career_single_set_format(event,argument);break;
    case 17:return U32(event,0xec);
    case 18:{
        const int32_t a=(int32_t)rx_career_event_rider_limit(U32(event,0x1c),d);
        const int32_t b=(int32_t)rx_career_class_rider_limit(-1);
        return (uint32_t)(a<=b?a:b);
    }
    case 19:case 21:return 0; /*actual007a0760, not synthetic activity*/
    case 22:rx_career_event_result_sort_columns(argument);break;
    case 23:rx_career_event_prepare(event,d);break;
    default:act_unreached(0x5b6950,"unclosed Single event virtual slot");
    }
    return 0;
}

/* Addressed callbacks retain one semantic implementation. These are a source
 * composition candidate; oracle masks/entry adapters remain proof work. */
static uint32_t single_state(rx_addr e,uint32_t phase){(void)e;return rx_career_single_player_state(phase);}
static void single_sort_columns(rx_addr e,rx_addr vector){(void)e;rx_career_event_result_sort_columns(vector);}
static void single_fade_complete(rx_addr e,rx_addr message){(void)message;rx_career_event_fade_complete(e);}
static void single_initialize(rx_addr e){rx_career_event_initialize(e);}
static void single_reset(rx_addr e){rx_career_single_reset(e,deps());}
static void single_uninitialize(rx_addr e){rx_career_event_uninitialize(e,deps());}
static void single_bind(rx_addr e,rx_addr c){rx_career_single_bind(e,c,1,deps());}
static void single_unbind(rx_addr e,rx_addr c){rx_career_single_bind(e,c,0,deps());}
static void single_gate(rx_addr e){rx_career_event_gate(e,deps());}
static void single_gate_result(rx_addr e,uint32_t v){rx_career_single_gate_result(e,v,deps());}
static void single_post_result(rx_addr e){rx_career_event_post_result(e,deps());}
static void single_advance(rx_addr e){rx_career_single_advance(e,deps());}
static void single_post_event(rx_addr e){rx_career_single_post_event(e,deps());}
static void single_holeshot(rx_addr e,uint32_t v){rx_career_single_holeshot(e,v,deps());}
static void single_restart(rx_addr e){rx_career_event_restart(e,deps());}
static void single_next(rx_addr e){rx_career_event_next_sub_event(e,deps());}
static void single_victory(rx_addr e){rx_career_event_advance_victory(e,deps());}
static void single_process_gate(rx_addr e){rx_career_event_process_gate(e,deps());}
static void single_gate_advance(rx_addr e){rx_career_event_advance_gate(e,deps());}
static void single_prepare(rx_addr e){rx_career_event_prepare(e,deps());}
static uint32_t single_capacity(rx_addr e){return rx_career_single_event_call(e,18,0);}
static uint32_t single_holeshot_player(rx_addr e){return U32(e,0xec);}
static void single_assign_gate(rx_addr e,uint32_t device,int32_t gate){rx_career_event_assign_gate(e,device,gate);}
static uint32_t single_activity_type(rx_addr e){return rx_career_event_current_activity_type(e);}
static void single_set_holeshot(rx_addr e,rx_addr player){rx_career_single_set_holeshot_player(e,player);}
static void single_set_type(rx_addr e,uint32_t kind){rx_career_single_set_type(e,kind);}
static void single_set_format(rx_addr e,uint32_t format){rx_career_single_set_format(e,format);}
static void single_practice_callback(rx_addr e,uint32_t value){rx_career_event_practice_callback(e,value);}
RX_FUNCTION(0x005a8790,rx_career_event_scalar_delete)
RX_FUNCTION(0x005a8850,single_initialize)
RX_FUNCTION(0x005afbd0,single_reset)
RX_FUNCTION(0x005a8900,single_uninitialize)
RX_FUNCTION(0x005afc20,single_bind)
RX_FUNCTION(0x005afc90,single_unbind)
RX_FUNCTION(0x005a8a10,single_gate)
RX_FUNCTION(0x005afd00,single_gate_result)
RX_FUNCTION(0x005a9f40,single_post_result)
RX_FUNCTION(0x005aff00,single_advance)
RX_FUNCTION(0x005affb0,single_post_event)
RX_FUNCTION(0x005b03e0,single_holeshot)
RX_FUNCTION(0x005aa150,single_restart)
RX_FUNCTION(0x005aad40,single_next)
RX_FUNCTION(0x005aa1c0,single_victory)
RX_FUNCTION(0x005a9ce0,single_gate_advance)
RX_FUNCTION(0x005a9a30,single_process_gate)
RX_FUNCTION(0x005a9e00,single_assign_gate)
RX_FUNCTION(0x005ab3e0,single_prepare)
RX_FUNCTION(0x005aafa0,single_capacity)
RX_FUNCTION(0x00598630,single_holeshot_player)
RX_FUNCTION(0x005ad830,single_activity_type)
RX_FUNCTION(0x005afbf0,single_state)
RX_FUNCTION(0x005b0490,single_set_holeshot)
RX_FUNCTION(0x005b04a0,single_set_type)
RX_FUNCTION(0x005b04e0,single_set_format)
RX_FUNCTION(0x005aafc0,single_sort_columns)
RX_FUNCTION(0x005c3300,single_practice_callback)
RX_FUNCTION(0x005a9dd0,single_fade_complete)

/* Shared wrappers retain exactly the accepted installed dependency lifetime. */
const rx_career_single_dependencies* rx_career_event_dependencies(void){return deps();}
