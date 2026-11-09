/* career_heat_events.c -- the subscriptions of MXSeriesEventHeat (vf7 005ad880 / vf8 005ad8f0) and the clones (vf3) of its three
 * member-proxy classes. The Heat event subscribes the same five manager events as the Moto event (career_moto_events.c), through the
 * per-class template instances 005bdd90 / 005bde50 (attach) and 005bdf00 / 005bdf60 (remove) that differ from the Moto event's only
 * in the proxy vtables: EventHandlerProxy<MXSeriesEventHeat, void (int)> 0x0094eaf8, <void ()> 0x0094eb18, <void (MXPlayer*)>
 * 0x0094eb38 (the holeshot, career_heat.c vf2). */
#include "career_heat.h"
#include "career_single_events.h"
#include "build_activity_util.h"
#include "crt.h"

#define PROXY_INT_VTABLE     0x0094eaf8u   /* EventHandlerProxy<MXSeriesEventHeat, void (int)> */
#define PROXY_VOID_VTABLE    0x0094eb18u   /* EventHandlerProxy<MXSeriesEventHeat, void ()> */
#define PROXY_PLAYER_VTABLE  0x0094eb38u   /* EventHandlerProxy<MXSeriesEventHeat, void (MXPlayer*)> */
#define TYPE_INT             0x009d24d0u   /* the RTTI type descriptors of the handlers' signatures */
#define TYPE_VOID            0x009cf3f0u
#define TYPE_PLAYER          0x009cbeb4u

/* vf7 005ad880 (attach) / vf8 005ad8f0 (remove) (thiscall event, channel; RET 4): AdvancePractice (005c3300, the practice popup's
 * answer), AdvanceEvent (00592ab0 -> vf12), AdvancePostEvent (00592b20 -> vf13), ProcessGateSelection (005a9a30), AdvanceEventPopup
 * (00592bc0), then the base MXSeriesEvent's (005a8950 / 005a89b0). */
void rx_career_heat_bind(rx_addr event, rx_addr channel, uint32_t attach, const rx_career_single_dependencies* d) {
    (void)d;
    ACT_FRAME(F, 0, 2);                                                   /* push esi; push edi */
    ACT_CALL(1, rx_career_member_proxy_bind(channel, 0x0094d6d8, event, 0x005c3300, PROXY_INT_VTABLE, TYPE_INT, attach));     /* 005bdd90 / 005bdf00 */
    ACT_CALL(2, rx_career_member_proxy_bind(channel, 0x0094d6fc, event, 0x00592ab0, PROXY_VOID_VTABLE, TYPE_VOID, attach));   /* 005bde50 / 005bdf60 */
    ACT_CALL(2, rx_career_member_proxy_bind(channel, 0x0094d71c, event, 0x00592b20, PROXY_VOID_VTABLE, TYPE_VOID, attach));
    ACT_CALL(2, rx_career_event_binding(channel, event, 0x0094d740, 0x005a9a30, attach));                                     /* 005be250 / 005be3c0 */
    ACT_CALL(2, rx_career_event_binding(channel, event, 0x0094d768, 0x00592bc0, attach));
    ACT_CALL(1, rx_career_base_binding(event, channel, attach));                                                               /* 005a8950 / 005a89b0 */
    ACT_END(F);
}

/* vf3 of the proxies (thiscall proxy; RET 0) -> a copy: ReflexAlignedAlloc(0x14), +4 0, the vtable, the object (+0xc) and the member
 * function (+0x10), +8 the signature's type name (type_info::_name_internal_method of the type, the name list 0x009e1a7c). */
static rx_addr clone(rx_addr proxy, rx_addr vtable, rx_addr type) {
    ACT_FRAME(F, 0, 2);
    rx_addr copy;
    ACT_CALLR(copy, 1, rx_reflex_aligned_alloc(0x14));
    if (copy) {
        SU32(copy, 4, 0);
        SU32(copy, 0, vtable);
        SU32(copy, 0xc, U32(proxy, 0xc));
        SU32(copy, 0x10, U32(proxy, 0x10));
        SU32(copy, 8, crt_type_info_name(type, 0x009e1a7c));
    }
    ACT_END(F);
    return copy;
}
static rx_addr clone_int(rx_addr proxy) { return clone(proxy, PROXY_INT_VTABLE, TYPE_INT); }          /* 005bfc40 */
static rx_addr clone_void(rx_addr proxy) { return clone(proxy, PROXY_VOID_VTABLE, TYPE_VOID); }       /* 005bfce0 */
static rx_addr clone_player(rx_addr proxy) { return clone(proxy, PROXY_PLAYER_VTABLE, TYPE_PLAYER); } /* 005bfda0 */
RX_FUNCTION(0x005bfc40, clone_int)
RX_FUNCTION(0x005bfce0, clone_void)
RX_FUNCTION(0x005bfda0, clone_player)
