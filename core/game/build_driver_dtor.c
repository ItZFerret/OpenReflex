/* build_driver_dtor.c -- the drivers' teardown (vf2): BaseDriver::vf2 00508ee0 (the behaviours' and the rules' destroyers) and FreeRideDriver::vf2 0050bd30 (the base, then the driver's two
 * activity-channel listeners removed: 0053a410 is the unsubscribe the constructor's subscribe (EventHandlerProxy<void (FreeRideDriver::*)(Activity*, RavenVehicle*)>) undoes). */
#include "build_activity_util.h"
#include "build_driver_dtor.h"
#include "race_activity_misc.h"
#include "world_misc.h"
#include "build_alloc.h"
#include "build_rider_util.h"
#include "anim.h"
#include "race.h"
#include "race_misc.h"

/* BaseDriver::vf2 00508ee0 (ECX = the driver): each behaviour (+0xe8 + 4 k, count +0xee0) then each rule (+0xa0 + 4 k, count +0xee4): its vf2 (+8) and then vf0 (0: the destructor
 * without freeing) */
void rx_base_driver_vf2(rx_addr d) {
    ACT_FRAME(L, 0, 3);
    for (uint32_t k = 0; k < U32(d, 0xee0); k++) {
        const rx_addr o = U32(d, 0xe8 + 4 * k);
        act_vcall(o, 8, 0, 0);
        { const uint32_t zero[1] = {0}; act_vcall(U32(d, 0xe8 + 4 * k), 0, 1, zero); }
        (void)o;
    }
    for (uint32_t k = 0; k < U32(d, 0xee4); k++) {
        const rx_addr o = U32(d, 0xa0 + 4 * k);
        act_vcall(o, 8, 0, 0);
        { const uint32_t zero[1] = {0}; act_vcall(U32(d, 0xa0 + 4 * k), 0, 1, zero); }
    }
    ACT_END(L);
}
RX_FUNCTION(0x00508ee0, rx_base_driver_vf2)

/* the listener removal both unsubscribes share (frame: push ebp; and esp, -8; sub esp, 0x14; push esi / eax / ecx): the channel of the name (00593220) loses the listener {the proxy
 * vtable, 0, the signature's name (type_info of td), the object, the member function} (00592ef0) */
static void session_static(void) {
    if (!(U8(0, 0x00dcf304) & 1)) act_unreached(0x006de1f0, "the session's static initialiser has not run");
}
static void unsubscribe(rx_addr registry, rx_addr name, rx_addr obj, rx_addr fn, rx_addr vtable, rx_addr td) {
    ACT_FRAME_EBP(L, 8, 0x14, 1);
    const rx_addr E = L + 4;
    SU32(L, -4, name); SU32(L, -8, registry);
    rx_addr ch;
    ACT_CALLR(ch, 2, rx_world_registry_find(registry, name));
    SU32(E, 4, 0);
    SU32(E, 0, vtable);
    SU32(E, 0xc, obj);
    SU32(E, 0x10, fn);
    SU32(E, 8, crt_type_info_name(td, 0x009e1a7c));
    ACT_CALL(1, rx_channel_remove_listener(ch, E));
    ACT_END(L);
}
/* 0053a410 (thiscall ECX = the event registry, EAX = the event name; stdcall object, member function; RET 8): void (FreeRideDriver::*)(Activity*, RavenVehicle*) */
void rx_free_ride_unsubscribe(rx_addr registry, rx_addr name, rx_addr obj, rx_addr fn) { unsubscribe(registry, name, obj, fn, 0x009402d0, 0x009cdb7c); }
RX_FUNCTION(0x0053a410, rx_free_ride_unsubscribe)
/* 0053a7a0 (same convention): void (NetworkDriver::*)(NetObjectType, Quazal::DuplicatedObject*) */
void rx_network_unsubscribe(rx_addr registry, rx_addr name, rx_addr obj, rx_addr fn) { unsubscribe(registry, name, obj, fn, 0x00940310, 0x009cdc7c); }
RX_FUNCTION(0x0053a7a0, rx_network_unsubscribe)

/* FUN_0050e2f0 (ECX = the driver; the NetworkDriver part of the destructor): the vtable = NetworkDriver's (0x0093f81c); under the session's static guard the session channel
 * [0x00da6ca0] loses "NetworkObjectRemoved" -> 0050eaa0 and "NetworkObjectMigrated" -> 00592ac0; the vtable = BaseDriver's (0x0093f7ac) */
void rx_network_driver_dtor(rx_addr d) {
    ACT_FRAME(L, 0, 2);
    SU32(d, 0, 0x0093f81c);
    session_static();
    ACT_CALL(2, rx_network_unsubscribe(U32(0, 0x00da6ca0), 0x0093ee98, d, 0x0050eaa0));
    session_static();
    ACT_CALL(2, rx_network_unsubscribe(U32(0, 0x00da6ca0), 0x0093eeb0, d, 0x00592ac0));
    SU32(d, 0, 0x0093f7ac);
    ACT_END(L);
}
RX_FUNCTION(0x0050e2f0, rx_network_driver_dtor)

/* FreeRideDriver::sdtor 0050bc90 (thiscall, flags; RET 4): the vtable = FreeRideDriver's (0x0093fa1c), the network driver part (0050e2f0), freed (_aligned_free under the allocation
 * lock) when flags & 1; -> this */
rx_addr rx_free_ride_driver_sdtor(rx_addr d, uint32_t flags) {
    ACT_FRAME(L, 0, 1);
    SU32(d, 0, 0x0093fa1c);
    ACT_CALL(0, rx_network_driver_dtor(d));
    if (flags & 1) {
        if (U8(0, 0x00da67d8) != 0) rr_os_cs_enter(0x00d61704);
        ACT_CALL(1, rx_crt_aligned_free(d));
        if (U8(0, 0x00da67d8) != 0) rr_os_cs_leave(0x00d61704);
    }
    ACT_END(L);
    return d;
}
RX_FUNCTION(0x0050bc90, rx_free_ride_driver_sdtor)

/* FreeRideDriver::vf2 0050bd30 (ECX = the driver): the base's vf2, then the two listeners of the player's activity's channel (+0x290): 0x0093ee38 -> 0050c8a0, 0x0093ee48 -> 0050c8d0 */
void rx_free_ride_driver_vf2(rx_addr d) {
    ACT_FRAME(L, 0, 1);
    ACT_CALL(0, rx_base_driver_vf2(d));
    ACT_CALL(2, rx_free_ride_unsubscribe(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ee38, d, 0x0050c8a0));
    ACT_CALL(2, rx_free_ride_unsubscribe(U32(U32(U32(d, 0x74), 0x94), 0x290), 0x0093ee48, d, 0x0050c8d0));
    ACT_END(L);
}
RX_FUNCTION(0x0050bd30, rx_free_ride_driver_vf2)

/* Rule::vf0 0051bd10 (thiscall, flags; RET 4; the base rule's scalar deleting destructor, shared by 14 rules): the vtable = Rule's (0x0093f868), freed when flags & 1; -> this */
rx_addr rx_rule_vf0(rx_addr self, uint32_t flags) {
    ACT_FRAME(L, 0, 1);
    SU32(self, 0, 0x0093f868);
    if (flags & 1) ACT_CALL(1, rx_reflex_aligned_free(self));
    ACT_END(L);
    return self;
}
RX_FUNCTION(0x0051bd10, rx_rule_vf0)

/* FUN_00558470 (stdcall activity; RET 4): the activity world's entities of type 0x106b (up to 0x4b): +0x140 = +0x174 = +0x170 = 0 */
void rx_activity_entities_reset_106b(rx_addr act) {
    ACT_FRAME(L, 0x130, 1);
    SU32(L, 8, 0);
    crt_memset(L + 0xc, 0, 0x128);
    uint32_t n;
    ACT_CALLR(n, 4, rx_scene_manager_collect_typed(U32(U32(act, 0x234), 0x20138), 0x106b, L + 8, 0x4b, L + 4));
    for (int32_t i = 0; i < (int32_t)n; i++) {
        const rx_addr e = U32(L, 8 + 4 * (uint32_t)i);
        SU32(e, 0x140, 0); SU32(e, 0x174, 0); SU32(e, 0x170, 0);
    }
    ACT_END(L);
}
RX_FUNCTION(0x00558470, rx_activity_entities_reset_106b)

/* Activity::vf51 0055a510 (ECX = the activity; shared by the activities): "Last Human Left Activity" (void(Activity*)) raised on the activity channel [0x00a93f74], the 0x106b
 * entities reset (00558470), then a tail jump to the pose overrides' clear (0047c3f0) */
void rx_activity_vf51(rx_addr act) {
    ACT_FRAME(L, 0, 4);
    SU32(L, 0xc, act);
    { const uint32_t payload[1] = {act}; race_raise_event(U32(0, 0x00a93f74), 0x00941e10 /* "Last Human Left Activity" */, 0x009cbd24, payload, 1); }
    ACT_CALL(1, rx_activity_entities_reset_106b(act));
    ACT_END(L);
    rx_pose_overrides_clear();
}
RX_FUNCTION(0x0055a510, rx_activity_vf51)
