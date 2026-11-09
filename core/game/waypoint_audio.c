/* Waypoint audio cue subscriptions and ActivityAudioLogic's owned cue teardown.
 * Handwritten from the complete original instruction spans saved in ../evidence.
 * Shared map helpers below are private full-range implementations; they do not
 * register an incomplete general range-erase implementation. */
#include "waypoint_audio.h"
#include "build_activity_int.h"
#include "build_vehicle_tree.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "world_wreck.h"

static int cue_present(rx_addr logic, rx_addr frame, rx_addr name) {
    rx_addr s = frame + 0x1c, it = frame + 0x14;
    crt_str_ctor_cstr(s, name);
    rx_addr map = U32(logic, 0x8c), head = U32(map, 0x18), proxy = U32(map, 0), result;
    ACT_CALLR(result, 2, rx_world_string_map_find(map, it, s));
    if (!U32(result, 0) || U32(result, 0) != proxy) crt_invalid_parameter_noinfo();
    int found = U32(result, 4) != head;
    crt_str_dtor(s);
    return found;
}
#define CHANNEL(l) U32(U32((l), 0x50), 0x290)

void rx_waypoint_audio_vf3(rx_addr logic) {
    ACT_FRAME_EBP(f, 8, 0x28, 4);
    if (U32(logic, 0x8c)) {
        if (cue_present(logic, f, 0x0093ef0c))
            ACT_CALL(1, act_subscribe(0x0058bbb0, 1, 1, 4, CHANNEL(logic), 0x0093ef0c, 0x00945f04, 0x009cda88, logic, 0x00582e00));
        if (cue_present(logic, f, 0x0093eff0))
            ACT_CALL(2, act_subscribe(0x0058bc70, 6, 2, 3, CHANNEL(logic), 0x0093eff0, 0x00945f24, 0x009cda64, logic, 0x005834f0));
        if (cue_present(logic, f, 0x0093eefc))
            ACT_CALL(2, act_subscribe(0x0058bc70, 6, 2, 3, CHANNEL(logic), 0x0093eefc, 0x00945f24, 0x009cda64, logic, 0x00583520));
        if (cue_present(logic, f, 0x00941ca8))
            ACT_CALL(1, act_subscribe(0x0058bd30, 1, 1, 4, CHANNEL(logic), 0x00941ca8, 0x00945f44, 0x009cf630, logic, 0x00583fb0));
    }
    ACT_END(f);
}
RX_FUNCTION(0x00583190, rx_waypoint_audio_vf3)

/* 0058bdf0 / 0058be50 / 0058beb0 share the same frame and key layout. */
static void unsubscribe(rx_addr channel, rx_addr name, rx_addr logic, rx_addr method, rx_addr vtable, rx_addr type) {
    ACT_FRAME(f, 0x14, 1);
    rx_addr event;
    ACT_CALLR(event, 2, rx_world_registry_find(channel, name));
    rx_addr key = f + 4;
    SU32(key, 0, vtable); SU32(key, 4, 0);
    SU32(key, 0xc, logic); SU32(key, 0x10, method);
    SU32(key, 8, crt_type_info_name(type, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(event, key));
    ACT_END(f);
}
void rx_waypoint_audio_vf4(rx_addr logic) {
    ACT_FRAME_EBP(f, 8, 0x28, 4);
    if (U32(logic, 0x8c)) {
        if (cue_present(logic, f, 0x0093ef0c))
            ACT_CALL(1, unsubscribe(CHANNEL(logic), 0x0093ef0c, logic, 0x00582e00, 0x00945f04, 0x009cda88));
        if (cue_present(logic, f, 0x0093eff0))
            ACT_CALL(2, unsubscribe(CHANNEL(logic), 0x0093eff0, logic, 0x005834f0, 0x00945f24, 0x009cda64));
        if (cue_present(logic, f, 0x0093eefc))
            ACT_CALL(2, unsubscribe(CHANNEL(logic), 0x0093eefc, logic, 0x00583520, 0x00945f24, 0x009cda64));
        if (cue_present(logic, f, 0x00941ca8))
            ACT_CALL(1, unsubscribe(CHANNEL(logic), 0x00941ca8, logic, 0x00583fb0, 0x00945f44, 0x009cf630));
    }
    ACT_END(f);
}
RX_FUNCTION(0x00583340, rx_waypoint_audio_vf4)

/* 00851f10: erase right subtree, destroy string key, free node, continue left. */
static void cue_nodes_erase(rx_addr map, rx_addr node) {
    ACT_FRAME(f, 0, 3);
    while (!U8(node, 0x2d)) {
        ACT_CALL(1, cue_nodes_erase(map, U32(node, 8)));
        rx_addr left = U32(node, 0);
        crt_str_dtor(node + 0xc);
        ACT_CALL(1, rx_reflex_aligned_free(node));
        node = left;
    }
    ACT_END(f);
}
static void cue_map_clear(rx_addr map) {
    ACT_CALL(1, cue_nodes_erase(map, U32(U32(map, 0x18), 4)));
    rx_addr head = U32(map, 0x18);
    SU32(head, 4, head); SU32(map, 0x1c, 0); SU32(head, 0, head); SU32(head, 8, head);
}
/* 00705070's exact begin/end branch, the only branch 00618c80 requests. */
static void cue_map_erase_all(rx_addr map, rx_addr out) {
    ACT_FRAME(f, 8, 4);
    if (!U32(map, 0)) crt_invalid_parameter_noinfo();
    cue_map_clear(map);
    SU32(out, 0, U32(map, 0)); SU32(out, 4, U32(U32(map, 0x18), 0));
    ACT_END(f);
}
static void cue_map_destroy(rx_addr map) {
    ACT_FRAME_EBP(f, 8, 8, 0); /* 00618c80 */
    ACT_CALL(5, cue_map_erase_all(map, f));
    ACT_CALL(1, rx_reflex_aligned_free(U32(map, 0x18)));
    rx_addr proxy = U32(map, 0);
    SU32(map, 0x18, 0); SU32(map, 0x1c, 0);
    ACT_CALL(1, rx_reflex_aligned_free(proxy));
    ACT_END(f);
}
static void checked_node(rx_addr proxy, rx_addr node) {
    if (!proxy) crt_invalid_parameter_noinfo();
    if (node == U32(U32(proxy, 0), 0x18)) crt_invalid_parameter_noinfo();
}
void rx_activity_audio_destroy(rx_addr logic) {
    ACT_FRAME_EBP(f, 8, 0x14, 3);
    SU32(logic, 0, 0x009429fc);
    if (U32(logic, 0x90) && U32(logic, 0x8c)) {
        rx_addr map = U32(logic, 0x8c), head = U32(map, 0x18), proxy = U32(map, 0), node = U32(head, 0);
        static const vt_tree tree = {0x2c, 0, 0, 0};
        for (;;) {
            if (!proxy) crt_invalid_parameter_noinfo();
            if (node == head) break;
            checked_node(proxy, node);
            if (U32(node, 0x28)) {
                checked_node(proxy, node);
                uint32_t flags = 1;
                act_vcall(U32(node, 0x28), 0, 1, &flags);
                checked_node(proxy, node);
                SU32(node, 0x28, 0);
            }
            node = vt_next(&tree, node);
        }
        cue_map_clear(U32(logic, 0x8c));
        map = U32(logic, 0x8c);
        if (map) {
            ACT_CALL(0, cue_map_destroy(map));
            ACT_CALL(1, rx_reflex_aligned_free(map));
            SU32(logic, 0x8c, 0);
        }
    }
    ACT_END(f);
}
RX_FUNCTION(0x00580d30, rx_activity_audio_destroy)
