/* Original Moto proxy identities, actual registry lifecycle; no host fake events. */
#include "career_moto.h"
#include "career_single_events.h"
#include "build_activity_util.h"
#include "build_mxgame_int.h"
#include "world_misc.h"
#include "race_activity_misc.h"
static void bind(rx_addr registry, rx_addr name, rx_addr object, rx_addr callback,
                 rx_addr vtable, rx_addr type, uint32_t attach) {
    if (attach) {
        /*The original subscription instances all save EBX/ESI before lookup. */
        rx_mx_handler_register(registry, name, 0x14, vtable, object, callback, type, 2);
        return;
    }
    ACT_FRAME(B, 0x1c, 1);
    const rx_addr key = B + 8;
    rx_addr channel;
    ACT_CALLR(channel, 2, rx_world_registry_find(registry, name));
    SU32(key, 4, 0); SU32(key, 0, vtable);
    SU32(key, 0xc, object); SU32(key, 0x10, callback);
    SU32(key, 8, crt_type_info_name(type, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(channel, key));
    ACT_END(B);
}

/* The member-proxy subscription every series event class makes (the per-class template instances 005bdd90 / 005be0e0 ... differ only
 * in the proxy's vtable): attach = the registry's handler add, else the remove by an equal key built on the stack. */
void rx_career_member_proxy_bind(rx_addr registry, rx_addr name, rx_addr object, rx_addr callback, rx_addr vtable, rx_addr type,
                                 uint32_t attach) {
    bind(registry, name, object, callback, vtable, type, attach);
}

void rx_career_moto_bind(rx_addr e,rx_addr channel,uint32_t attach,const rx_career_single_dependencies*d){
 (void)d;
 bind(channel,0x94d6d8,e,0x5c3300,0x94eb58,0x9d24d0,attach);
 bind(channel,0x94d6fc,e,0x592ab0,0x94eb78,0x9cf3f0,attach);
 bind(channel,0x94d71c,e,0x592b20,0x94eb78,0x9cf3f0,attach);
 rx_career_event_binding(channel,e,0x94d740,0x5a9a30,attach);
 rx_career_event_binding(channel,e,0x94d768,0x592bc0,attach);
 rx_career_base_binding(e,channel,attach);
}
void rx_career_moto_holeshot(rx_addr e,uint32_t enable,const rx_career_single_dependencies*d){
 if(enable){if(U32(e,0xf0))return;}else if(!U32(e,0xf0))return;
 if(!d||!d->manager)act_unreached(0x413d20,"Moto holeshot manager missing");
 rx_addr m=d->manager();bind(U32(m,0x180),0x9420d0,e,0x5c3310,0x94eb98,0x9cbeb4,enable!=0);
 SU32(e,0xf0,enable?1:0);if(!enable)SU32(e,0xec,0);
}
/* Actual proxy clone leaves; equality/invoke/removal already canonical. */
static rx_addr clone(rx_addr proxy,rx_addr vt,rx_addr type){
 ACT_FRAME(F,0,2);rx_addr out;ACT_CALLR(out,1,rx_reflex_aligned_alloc(0x14));
 if(out){SU32(out,4,0);SU32(out,0,vt);SU32(out,0xc,U32(proxy,0xc));SU32(out,0x10,U32(proxy,0x10));SU32(out,8,crt_type_info_name(type,0x9e1a7c));}
 ACT_END(F);return out;
}
static rx_addr clone_int(rx_addr p){return clone(p,0x94eb58,0x9d24d0);}
static rx_addr clone_void(rx_addr p){return clone(p,0x94eb78,0x9cf3f0);}
static rx_addr clone_player(rx_addr p){return clone(p,0x94eb98,0x9cbeb4);}
RX_FUNCTION(0x005bfe40,clone_int)
RX_FUNCTION(0x005bfef0,clone_void)
RX_FUNCTION(0x005bff90,clone_player)
