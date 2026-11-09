/* Authored Single subscriptions/emission, reusing the existing canonical event
 * registry, proxy registration, listener removal and typed event dispatch.
 * No synthetic completion callback or original-code execution is introduced. */
#include "career_single_events.h"
#ifndef RX_ORACLE
#include "career_presentation_binding.h"
#endif
#include "build_ui_world.h"
#include "build_activity_util.h"
#include "build_mxgame_int.h"
#include "world_misc.h"
#include "race_activity_misc.h"
#include "race.h"

static rx_addr career_ui_channel(void) {
    const rx_addr channel=rx_ui_event_channel();
    if(!channel)act_unreached(0x5bd9e0,"career UI channel not constructed");
    return channel;
}

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
void rx_career_practice_binding(rx_addr channel, rx_addr event, uint32_t attach) {
    bind(channel, 0x0094d6d8, event, 0x005c3300, 0x0094ebb8, 0x009d24d0, attach);
}
void rx_career_advance_binding(rx_addr channel, rx_addr event, rx_addr name, rx_addr callback, uint32_t attach) {
    bind(channel, name, event, callback, 0x0094ebd8, 0x009cf3f0, attach);
}
void rx_career_event_binding(rx_addr channel, rx_addr event, rx_addr name, rx_addr callback, uint32_t attach) {
    bind(channel, name, event, callback, 0x0094ea58, 0x009cf3f0, attach);
}
void rx_career_holeshot_binding(rx_addr channel, rx_addr event, uint32_t attach) {
    bind(channel, 0x009420d0, event, 0x005c3310, 0x0094ebf8, 0x009cbeb4, attach);
}
void rx_career_base_binding(rx_addr event, rx_addr channel, uint32_t attach) {
    rx_career_event_binding(channel, event, 0x0094c3ac, 0x005aa150, attach);
    rx_career_event_binding(channel, event, 0x0094d580, 0x005a9ce0, attach);
    rx_career_event_binding(channel, event, 0x0094d5a8, 0x005aa1c0, attach);
    rx_career_event_binding(channel, event, 0x0094d5d0, 0x005aad40, attach);
    bind(channel, 0x0094d5fc, event, 0x005a9e00, 0x0094ea78, 0x009d24bc, attach);
}
void rx_career_subscribe_fade(rx_addr event) {
    bind(career_ui_channel(), 0x0094d174, event,
         0x005a9dd0, 0x0094ea98, 0x009cdb68, 1);
}
void rx_career_emit_fade_out(void) {
    const uint32_t payload[2] = {U32(0, 0x009322cc), 0xff000000u};
    race_raise_event(career_ui_channel(), 0x0094d65c, 0x009d2590, payload, 2);
#ifndef RX_ORACLE
    rx_career_presentation_observe_fade(payload[0],payload[1]);
#endif
}
void rx_career_unsubscribe_page_wipe(rx_addr channel, rx_addr event) {
    /*Original unregisters PageWipeTransition here, not FadeComplete. */
    bind(channel, 0x00941d7c, event, 0x005a9dd0, 0x0094ea98, 0x009cdb68, 0);
}
void rx_career_event_fade_complete(rx_addr event) {
    rx_career_unsubscribe_page_wipe(career_ui_channel(), event);
    act_vcall(event, 0x30, 0, 0);
}
void rx_career_subscribe_page_wipe(rx_addr event) {
    bind(career_ui_channel(),0x941d7c,event,
         0x5a9dd0,0x94ea98,0x9cdb68,1);
}
void rx_career_emit_series_page_wipe(void) {
    const uint32_t text=0x94d664; /*SeriesEvent*/
    race_raise_event(career_ui_channel(),0x941e9c,0x9cdb68,&text,1);
#ifndef RX_ORACLE
    rx_career_presentation_observe_page_wipe(text);
#endif
}
void rx_career_event_practice_callback(rx_addr event,uint32_t value) {
    act_vcall(event,0x28,1,&value); /*005c3300 tail jump, same actual argument*/
}
/*005bfee0 proxy<void(Class::*)()>: payload is ignored, actual member is
 * called (not tail-jumped). Product callback must already be registered.*/
void rx_career_proxy_call_member_void(rx_addr proxy,rx_addr payload) {
    (void)payload;
    const rx_addr target=U32(proxy,0x10),object=U32(proxy,0xc);
    void (*call)(rx_addr)=RX_FN(target,void(*)(rx_addr));
    if(!call)act_unreached(target,"unregistered native career event callback");
    ACT_CALL(0,call(object));
}
RX_FUNCTION(0x005bfee0,rx_career_proxy_call_member_void)
