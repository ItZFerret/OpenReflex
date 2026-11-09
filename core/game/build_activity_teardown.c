/* build_activity_teardown.c -- Activity::vf3 00557790 (the activity's teardown, reached from MXActivityRaceStub::vf3 when the
 * career leaves a race for the next one) and the event-system destruction it reaches: the handler-unsubscription templates of the
 * activity's own subscriptions (the mirror of build_activity_start.c's act_subscribe calls), the activity's event channel removed
 * from the global channel registry (00593770) and destroyed (EventSystem::Channel / BaseChannel / Event destructors, their std::map
 * <std::string, T*> erase helpers). Each function is a hand transliteration of the original (address in its comment).
 *
 * The maps here are MSVC 9 std::map<std::string, T*> (node: +0 left, +4 parent, +8 right, +0xc the key string, +0x28 the value,
 * +0x2c colour, +0x2d isnil; map: +0 container proxy, +0x18 head, +0x1c size); the checked iterators are {proxy, node} pairs and
 * their tests call _invalid_parameter_noinfo (crt_invalid_parameter_noinfo). */
#include "build_activity_teardown.h"
#include "build_activity_int.h"
#include "build_vehicle_tree.h"
#include "race_activity_misc.h"
#include "race_driver.h"
#include "waypoint_activity.h"
#include "loop_platform.h"
#include "loop_world.h"
#include "world_misc.h"
#include "world_veh.h"
#include "rx_vcall.h"
#include "rx_os.h"
#include "anim.h"
#include "fx.h"

/* ======================================================================================================================== */
/* the handler-unsubscription templates                                                                                      */

void act_unlisten_at(rx_addr F, rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr self, rx_addr method) {
    rx_addr ev;
    SU32(F, -4, name); SU32(F, -8, registry);                    /* push name; push registry */
    ACT_CALLR(ev, 2, rx_world_registry_find(registry, name));    /* 00593220 */
    const rx_addr key = F + 4;
    SU32(key, 4, 0);
    SU32(key, 0, vtable);
    SU32(key, 0xc, self);
    SU32(key, 0x10, method);
    SU32(key, 8, crt_type_info_name(td, 0x009e1a7c));             /* push 0x9e1a7c; call [0x916388] (from the ESP before the push) */
    SU32(F, -4, key);
    ACT_CALL(1, rx_channel_remove_listener(ev, key));            /* 00592ef0 (EAX = the event) */
}

void act_unlisten(int aligned, rx_addr registry, rx_addr name, rx_addr vtable, rx_addr td, rx_addr self, rx_addr method) {
    if (aligned) {
        ACT_FRAME_EBP(F, 8, 0x14, 1);
        act_unlisten_at(F, registry, name, vtable, td, self, method);
        ACT_END(F);
    } else {
        ACT_FRAME(F, 0x14, 1);
        act_unlisten_at(F, registry, name, vtable, td, self, method);
        ACT_END(F);
    }
}

/* 00587330 / 00587390 / 005873f0 / 00587450 (EAX = the session registry [0xda6ca0]; stdcall activity; RET 4): the activity's network
 * listeners (the subscriptions of 00556ec0's 00586... templates) leave: "NetworkTimerEvents" -> 0055cda0, "NetworkSessionState" ->
 * 0055cdf0, "NetworkPositionListUpdate" -> 0055ce10, "NetworkPlayerModeChanged" -> 0055c970 */
void rx_activity_unlisten_net_timer(rx_addr registry, rx_addr act) {
    act_unlisten(1, registry, 0x00941d1c, 0x00945864, 0x009cf81c, act, 0x0055cda0);
}
RX_FUNCTION(0x00587330, rx_activity_unlisten_net_timer)
void rx_activity_unlisten_net_session(rx_addr registry, rx_addr act) {
    act_unlisten(1, registry, 0x00941d30, 0x00945884, 0x009cf8a8, act, 0x0055cdf0);
}
RX_FUNCTION(0x00587390, rx_activity_unlisten_net_session)
void rx_activity_unlisten_net_positions(rx_addr registry, rx_addr act) {
    act_unlisten(1, registry, 0x00941d44, 0x009458a4, 0x009cf940, act, 0x0055ce10);
}
RX_FUNCTION(0x005873f0, rx_activity_unlisten_net_positions)
void rx_activity_unlisten_net_mode(rx_addr registry, rx_addr act) {
    act_unlisten(1, registry, 0x00941d60, 0x009458c4, 0x009cf9a4, act, 0x0055c970);
}
RX_FUNCTION(0x00587450, rx_activity_unlisten_net_mode)
/* 005874b0 (EAX = the activity's channel; stdcall activity; RET 4): "Reset Ghost Overlays" -> 006137b0 */
void rx_activity_unlisten_ghost_reset(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00941cc4, 0x00945844, 0x009cbd24, act, 0x006137b0);
}
RX_FUNCTION(0x005874b0, rx_activity_unlisten_ghost_reset)
/* 00587510 (stdcall activity; RET 4): "VehicleEntered" (the trigger registry [0xa89bc8]) -> 0055e8b0 */
void rx_activity_unlisten_vehicle_entered(rx_addr act) {
    act_unlisten(1, U32(0, 0x00a89bc8), 0x00935e84, 0x009458e4, 0x009cbb7c, act, 0x0055e8b0);
}
RX_FUNCTION(0x00587510, rx_activity_unlisten_vehicle_entered)
/* 00587580 (EAX = the UI channel [[0xa93fcc] + 0x98]; stdcall activity; RET 4): "PageWipeTransition" -> 0055e9e0 */
void rx_activity_unlisten_page_wipe(rx_addr channel, rx_addr act) {
    act_unlisten(1, channel, 0x00941d7c, 0x00945904, 0x009cdb68, act, 0x0055e9e0);
}
RX_FUNCTION(0x00587580, rx_activity_unlisten_page_wipe)
/* 00755880 (ECX = channel, EAX = name; stdcall DataMiner, method; RET 8): a DataMiner's void(Activity*) handler leaves */
void rx_dataminer_unlisten(rx_addr channel, rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, channel, name, 0x0096d4b0, 0x009cbd24, self, method);
}
RX_FUNCTION(0x00755880, rx_dataminer_unlisten)
/* 007074d0 (ECX = channel, EAX = name; stdcall NetworkManager, method; RET 8): a NetworkManager's void(Activity*, int) handler leaves */
void rx_netmgr_unlisten(rx_addr channel, rx_addr name, rx_addr self, rx_addr method) {
    act_unlisten(1, channel, name, 0x00965638, 0x009cf5b4, self, method);
}
RX_FUNCTION(0x007074d0, rx_netmgr_unlisten)

/* 006deff0 (ESI = the session/stats block 0xda6ca0, EAX = activity): when the block listens to this activity (+4 its channel, +8 the
 * activity), its four state handlers leave ("Pre Entered" 006df100, "Post Entered" 006df0b0, "Post Exited" 005a4150 inlined,
 * "WindDown Entered" 006df0d0) and +4 / +8 are cleared. Frame: push ebp; and esp, -8; sub esp, 0x14; push edi. */
void rx_netmgr_activity_unlisten(rx_addr stats, rx_addr act) {
    ACT_FRAME_EBP(F, 8, 0x14, 1);
    if (U32(stats, 4) != 0 && act == U32(stats, 8)) {
        SU32(F, -4, 0x006df100); SU32(F, -8, stats);
        ACT_CALL(2, rx_netmgr_unlisten(U32(stats, 4), 0x009427a4 /* "Pre Entered" */, stats, 0x006df100));
        SU32(F, -4, 0x006df0b0); SU32(F, -8, stats);
        ACT_CALL(2, rx_netmgr_unlisten(U32(stats, 4), 0x00942830 /* "Post Entered" */, stats, 0x006df0b0));
        act_unlisten_at(F, U32(stats, 4), 0x00942840 /* "Post Exited" */, 0x00965638, 0x009cf5b4, stats, 0x005a4150);
        SU32(F, -4, 0x006df0d0); SU32(F, -8, stats);
        ACT_CALL(2, rx_netmgr_unlisten(U32(stats, 4), 0x0094280c /* "WindDown Entered" */, stats, 0x006df0d0));
        SU32(stats, 4, 0);
        SU32(stats, 8, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x006deff0, rx_netmgr_activity_unlisten)

/* ======================================================================================================================== */
/* the event system's destruction                                                                                            */

static const vt_tree k_name_map = {0x2c, 0, 0, 0};
static void name_node_dtor(rx_addr node) { crt_str_dtor(node + 0xc); }   /* ~basic_string of the key ([0x9161b4]) */

/* a checked iterator's dereference test: the iterator's container is set and the node is not the container's end */
static void checked_deref(rx_addr cont, rx_addr node) {
    rx_addr c = 0;
    if (cont == 0) crt_invalid_parameter_noinfo(); else c = U32(cont, 0);
    if (node == U32(c, 0x18)) crt_invalid_parameter_noinfo();
}

/* 00593a70 (stdcall map, out, iterator {cont, node}; RET 0x10) -> out {the map's proxy, the next node}; ECX = the proxy: erase(it)
 * of a std::map<std::string, T*>: erase(end()) throws out_of_range; the iterator (in its argument slots) is incremented
 * (TreeIterator_Increment 00593dc0), the node unlinked and the tree rebalanced (the rotations 00851f50 / 005bb590, the rightmost
 * node 0068f8f0), the key's string destroyed, the node freed (ReflexAlignedFree), the size decremented when non-zero.
 * Frame: sub esp, 0x4c; push ebx / ebp / esi / edi. */
rx_addr rx_channel_map_erase(rx_addr map, rx_addr out, rx_addr cont, rx_addr node) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(F, 0x4c, 4);
    SU32(entry, 0xc, cont); SU32(entry, 0x10, node);             /* the iterator argument */
    if (U8(node, 0x2d) == 0) ACT_CALL(0, rx_fx_tree_next(entry + 0xc));
    const rx_addr next = vt_erase(&k_name_map, map, node, name_node_dtor);
    SU32(out, 4, next);
    SU32(out, 0, U32(map, 0));
    ACT_END(F);
    return out;
}
RX_FUNCTION(0x00593a70, rx_channel_map_erase)

/* 00851f10 (ECX = map; stdcall node; RET 4): _Erase(node) of a std::map<std::string, T*>: the right subtree first (recursively),
 * then along the left chain: each key's string destroyed and the node freed. Frame: push ebx / esi / edi. */
void rx_channel_map_erase_subtree(rx_addr map, rx_addr node) {
    ACT_FRAME(F, 0, 3);
    while (U8(node, 0x2d) == 0) {
        SU32(F, -4, U32(node, 8));
        ACT_CALL(1, rx_channel_map_erase_subtree(map, U32(node, 8)));
        const rx_addr left = U32(node, 0);
        crt_str_dtor(node + 0xc);
        SU32(F, -4, node);
        ACT_CALL(1, rx_reflex_aligned_free(node));
        node = left;
    }
    ACT_END(F);
}
RX_FUNCTION(0x00851f10, rx_channel_map_erase_subtree)

/* 00705070 (EAX = map; stdcall out, first {cont, node}, last {cont, node}; RET 0x14) -> out: erase(first, last) of a std::map
 * <std::string, T*>: [begin, end) clears the tree (00851f10 on the root, the head linked to itself, size 0) and returns {proxy,
 * begin}; else erase(it) (00593a70) one by one, the first iterator (in its argument slots) incremented before each erase.
 * Frame: sub esp, 8; push ebx / ebp / esi / edi (the erase's out iterator at +0x10). */
rx_addr rx_channel_map_erase_range(rx_addr map, rx_addr out, rx_addr fcont, rx_addr fnode, rx_addr lcont, rx_addr lnode) {
    const rx_addr entry = rx_sp();
    ACT_FRAME(F, 8, 4);
    SU32(entry, 4, out); SU32(entry, 8, fcont); SU32(entry, 0xc, fnode); SU32(entry, 0x10, lcont); SU32(entry, 0x14, lnode);
    {
        const rx_addr begin = U32(U32(map, 0x18), 0);
        if (fcont == 0 || fcont != U32(map, 0)) crt_invalid_parameter_noinfo();
        if (fnode == begin) {
            const rx_addr end = U32(map, 0x18);
            if (lcont == 0 || lcont != U32(map, 0)) crt_invalid_parameter_noinfo();
            if (lnode == end) {
                SU32(F, -4, U32(U32(map, 0x18), 4));
                ACT_CALL(1, rx_channel_map_erase_subtree(map, U32(U32(map, 0x18), 4)));
                SU32(U32(map, 0x18), 4, U32(map, 0x18));
                SU32(map, 0x1c, 0);
                SU32(U32(map, 0x18), 0, U32(map, 0x18));
                SU32(U32(map, 0x18), 8, U32(map, 0x18));
                SU32(out, 4, U32(U32(map, 0x18), 0));
                SU32(out, 0, U32(map, 0));
                ACT_END(F);
                return out;
            }
        }
    }
    rx_addr cont = U32(entry, 8);                                /* ebx */
    for (;;) {
        if (cont == 0 || cont != U32(entry, 0x10)) crt_invalid_parameter_noinfo();
        const rx_addr node = U32(entry, 0xc);                    /* ebp */
        if (node == U32(entry, 0x14)) {
            SU32(out, 0, U32(map, 0));
            SU32(out, 4, node);
            ACT_END(F);
            return out;
        }
        ACT_CALL(0, rx_fx_tree_next(entry + 8));                 /* lea esi, [esp + 0x20] */
        SU32(F, -4, node); SU32(F, -8, cont); SU32(F, -12, F + 0x10); SU32(F, -16, map);
        ACT_CALL(4, rx_channel_map_erase(map, F + 0x10, cont, node));
        cont = U32(entry, 8);
    }
}
RX_FUNCTION(0x00705070, rx_channel_map_erase_range)

/* 00593770 (stdcall registry, channel; RET 8): under the registry's lock (+0x20) the registry's std::map<std::string, Channel*>
 * (at +0) is searched from begin for the channel; found: the channel deleted (vf0(1)) and its node erased (00593a70).
 * Frame: push ebp; and esp, -8; sub esp, 0x14; push ebx / esi / edi (+0x10 the proxy copy, +0x14 the lock, +0x18 the iterator). */
void rx_channel_registry_remove(rx_addr registry, rx_addr ch) {
    ACT_FRAME_EBP(F, 8, 0x14, 3);
    const rx_addr cs = registry + 0x20;
    SU32(F, 0x14, cs);
    loop_enter_cs(cs);
    const rx_addr head = U32(registry, 0x18);                    /* ebx */
    const rx_addr proxy = U32(registry, 0);
    rx_addr node = U32(head, 0);                                 /* edi */
    rx_addr cont = proxy;                                        /* esi */
    SU32(F, 0x1c, node); SU32(F, 0x10, proxy); SU32(F, 0x18, cont);
    for (;;) {
        if (cont == 0 || cont != U32(F, 0x10)) crt_invalid_parameter_noinfo();
        if (node == head) break;
        checked_deref(cont, node);
        if (U32(node, 0x28) == ch) {
            checked_deref(cont, node);
            const rx_addr v = U32(node, 0x28);
            if (v != 0) { const uint32_t one = 1; rx_vcall(v, 0, 1, &one); }
            SU32(F, -4, node); SU32(F, -8, cont); SU32(F, -12, F + 0x18); SU32(F, -16, registry);
            ACT_CALL(4, rx_channel_map_erase(registry, F + 0x18, cont, node));
            break;
        }
        ACT_CALL(0, rx_fx_tree_next(F + 0x18));
        node = U32(F, 0x1c);
        cont = U32(F, 0x18);
    }
    loop_leave_cs(U32(F, 0x14));
    ACT_END(F);
}
RX_FUNCTION(0x00593770, rx_channel_registry_remove)

/* 00593ef0 (ESI = std::list {+0 proxy, +0x14 head, +0x18 size}): clear(): the head linked to itself, size 0, every node freed
 * (ReflexAlignedFree; push edi only when the list held nodes) */
void rx_channel_list_clear(rx_addr list) {
    rx_addr n = U32(U32(list, 0x14), 0);
    SU32(U32(list, 0x14), 0, U32(list, 0x14));
    SU32(U32(list, 0x14), 4, U32(list, 0x14));
    SU32(list, 0x18, 0);
    if (n == U32(list, 0x14)) return;
    ACT_FRAME(F, 0, 1);
    do {
        const rx_addr next = U32(n, 0);
        SU32(F, -4, n);
        ACT_CALL(1, rx_reflex_aligned_free(n));
        n = next;
    } while (n != U32(list, 0x14));
    ACT_END(F);
}
RX_FUNCTION(0x00593ef0, rx_channel_list_clear)

/* 00592c40 (ECX = event): EventSystem::Event's destructor: under the event's lock (+0x24) every handler of the list (+8 proxy, +0x1c
 * head; node +0 next, +8 the handler) deleted (vf0(1)); the lock left and deleted; the list's nodes freed (_aligned_free under the
 * allocator lock 0xd61704, taken twice: the inlined allocator's own lock and ReflexAlignedFree's), then its head and its proxy.
 * Frame: push ebp; and esp, -8; sub esp, 8; push ebx / ebp / esi / edi (+0x10 the proxy copy, +0x14 the head). */
void rx_event_dtor(rx_addr ev) {
    ACT_FRAME_EBP(F, 8, 8, 4);
    const rx_addr cs = ev + 0x24;
    SU32(ev, 0, 0x00946248);
    SU32(F, -4, cs);
    loop_enter_cs(cs);
    const rx_addr proxy = U32(ev, 8);                            /* edi */
    rx_addr node = U32(U32(ev, 0x1c), 0);                        /* ebp */
    SU32(F, 0x14, U32(ev, 0x1c));
    SU32(F, 0x10, proxy);
    for (;;) {
        if (proxy == 0 || proxy != U32(F, 0x10)) crt_invalid_parameter_noinfo();
        if (node == U32(F, 0x14)) break;
        { rx_addr c = 0; if (proxy == 0) crt_invalid_parameter_noinfo(); else c = U32(proxy, 0); if (node == U32(c, 0x14)) crt_invalid_parameter_noinfo(); }
        const rx_addr h = U32(node, 8);
        if (h != 0) { const uint32_t one = 1; rx_vcall(h, 0, 1, &one); }
        { rx_addr c = 0; if (proxy == 0) crt_invalid_parameter_noinfo(); else c = U32(proxy, 0); if (node == U32(c, 0x14)) crt_invalid_parameter_noinfo(); }
        node = U32(node, 0);
    }
    loop_leave_cs(cs);
    RX_OS(0x00916010, cs);                                       /* DeleteCriticalSection */
    rx_addr n = U32(U32(ev, 0x1c), 0);
    SU32(U32(ev, 0x1c), 0, U32(ev, 0x1c));
    SU32(U32(ev, 0x1c), 4, U32(ev, 0x1c));
    SU32(ev, 0x20, 0);
    while (n != U32(ev, 0x1c)) {
        const rx_addr next = U32(n, 0);
        if (U8(0, 0x00da67d8) != 0) {
            loop_enter_cs(0x00d61704);
            if (U8(0, 0x00da67d8) != 0) loop_enter_cs(0x00d61704);
        }
        SU32(F, -4, n);
        rx_set_sp(F - 4);
        crt_aligned_free(n);                                     /* [0x916350] _aligned_free */
        rx_set_sp(F);
        if (U8(0, 0x00da67d8) != 0) {
            loop_leave_cs(0x00d61704);
            if (U8(0, 0x00da67d8) != 0) loop_leave_cs(0x00d61704);
        }
        n = next;
    }
    SU32(F, -4, U32(ev, 0x1c));
    ACT_CALL(1, rx_reflex_aligned_free(U32(ev, 0x1c)));
    const rx_addr p = U32(ev, 8);
    rx_set_sp(F - 4);                                            /* the first argument stays pushed (add esp, 8 after both) */
    SU32(F, -8, p);
    SU32(ev, 0x1c, 0);
    ACT_CALL(1, rx_reflex_aligned_free(p));
    rx_set_sp(F);
    ACT_END(F);
}
RX_FUNCTION(0x00592c40, rx_event_dtor)

/* 00592c20 EventSystem::Event::sdtor (thiscall; flags; RET 4) -> the event: the destructor, freed when bit 0 is set */
rx_addr rx_event_sdtor(rx_addr ev, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_event_dtor(ev));
    if (flags & 1) { SU32(F, -4, ev); ACT_CALL(1, rx_reflex_aligned_free(ev)); }
    ACT_END(F);
    return ev;
}
RX_FUNCTION(0x00592c20, rx_event_sdtor)

/* 00593120 (ECX = channel): EventSystem::BaseChannel's destructor: every event of the channel's event map (+8: std::map<std::string,
 * Event*>) deleted (vf0(1)); the map's tree erased (00851f10), its head linked to itself, size 0; the channel's lock (+0x28) deleted;
 * erase(begin, end) (00705070) and the map's head and proxy freed.
 * Frame: push ebp; and esp, -8; sub esp, 0x10; push ebx / ebp / esi / edi (+0x10 the iterator, +0x18 the proxy copy, +0x1c the
 * head; the erase's out iterator at +0x18). */
void rx_base_channel_dtor(rx_addr ch) {
    ACT_FRAME_EBP(F, 8, 0x10, 4);
    const rx_addr map = ch + 8;                                  /* edi */
    SU32(ch, 0, 0x00946250);
    const rx_addr head = U32(ch, 0x20);
    rx_addr cont = U32(ch, 8);                                   /* esi */
    rx_addr node = U32(head, 0);                                 /* ebx */
    SU32(F, 0x10, cont); SU32(F, 0x14, node); SU32(F, 0x1c, head); SU32(F, 0x18, cont);
    for (;;) {
        if (cont == 0 || cont != U32(F, 0x18)) crt_invalid_parameter_noinfo();
        if (node == U32(F, 0x1c)) break;
        checked_deref(cont, node);
        const rx_addr v = U32(node, 0x28);
        if (v != 0) { const uint32_t one = 1; rx_vcall(v, 0, 1, &one); }
        ACT_CALL(0, rx_fx_tree_next(F + 0x10));
        node = U32(F, 0x14);
        cont = U32(F, 0x10);
    }
    SU32(F, -4, U32(U32(map, 0x18), 4));
    ACT_CALL(1, rx_channel_map_erase_subtree(map, U32(U32(map, 0x18), 4)));
    SU32(U32(map, 0x18), 4, U32(map, 0x18));
    SU32(map, 0x1c, 0);
    SU32(U32(map, 0x18), 0, U32(map, 0x18));
    SU32(U32(map, 0x18), 8, U32(map, 0x18));
    SU32(F, -4, ch + 0x28);
    RX_OS(0x00916010, ch + 0x28);                                /* DeleteCriticalSection */
    {
        const rx_addr h = U32(map, 0x18), proxy = U32(map, 0), begin = U32(h, 0);
        SU32(F, -4, h); SU32(F, -8, proxy); SU32(F, -12, begin); SU32(F, -16, proxy); SU32(F, -20, F + 0x18);
        rx_addr r;
        ACT_CALLR(r, 5, rx_channel_map_erase_range(map, F + 0x18, proxy, begin, proxy, h));
        (void)r;
    }
    SU32(F, -4, U32(map, 0x18));
    ACT_CALL(1, rx_reflex_aligned_free(U32(map, 0x18)));
    {
        const rx_addr proxy = U32(map, 0);
        SU32(F, -4, proxy);
        SU32(map, 0x18, 0);
        SU32(map, 0x1c, 0);
        ACT_CALL(1, rx_reflex_aligned_free(proxy));
    }
    ACT_END(F);
}
RX_FUNCTION(0x00593120, rx_base_channel_dtor)

/* 00593330 (ECX = channel): EventSystem::Channel's destructor: under the lock (+0x28) the pending dispatch records run (005933a0)
 * while the pending list (+0x40: proxy, +0x54 head, +0x58 size) holds any; the lock left; the list cleared (00593ef0), its head and
 * proxy freed; then the base channel's destructor (00593120). Frame: push ebp; and esp, -8; push esi / edi. */
void rx_channel_dtor(rx_addr ch) {
    ACT_FRAME_EBP(F, 8, 0, 2);
    const rx_addr cs = ch + 0x28;                                /* esi */
    SU32(F, -4, cs);
    SU32(ch, 0, 0x00946258);
    loop_enter_cs(cs);
    while (U32(ch, 0x58) != 0) {
        SU32(F, -4, ch);
        ACT_CALL(1, rx_pending_handlers_run(ch));
    }
    SU32(F, -4, cs);
    loop_leave_cs(cs);
    const rx_addr list = ch + 0x40;
    ACT_CALL(0, rx_channel_list_clear(list));
    SU32(F, -4, U32(list, 0x14));
    ACT_CALL(1, rx_reflex_aligned_free(U32(list, 0x14)));
    const rx_addr proxy = U32(list, 0);
    rx_set_sp(F - 4);                                            /* add esp, 8 after both frees */
    SU32(F, -8, proxy);
    SU32(list, 0x14, 0);
    ACT_CALL(1, rx_reflex_aligned_free(proxy));
    rx_set_sp(F);
    ACT_CALL(0, rx_base_channel_dtor(ch));
    ACT_END(F);
}
RX_FUNCTION(0x00593330, rx_channel_dtor)

/* 00593310 EventSystem::Channel::sdtor (thiscall; flags; RET 4) -> the channel */
rx_addr rx_channel_sdtor(rx_addr ch, uint32_t flags) {
    ACT_FRAME(F, 0, 1);
    ACT_CALL(0, rx_channel_dtor(ch));
    if (flags & 1) { SU32(F, -4, ch); ACT_CALL(1, rx_reflex_aligned_free(ch)); }
    ACT_END(F);
    return ch;
}
RX_FUNCTION(0x00593310, rx_channel_sdtor)

/* ======================================================================================================================== */
/* Activity::vf3                                                                                                             */

/* 00557790 Activity::vf3 (thiscall): the activity's teardown.
 *   +0x1068 / +0xec4 = 0; the engine's frame mark (0084ea00, EDI = [core + 0x10]); the driver lists' counts +0x26c / +0x280 = 0;
 *   the audio logic (+0xfd8) told (vf30); the world's (+0x234) linked objects (+0x21dd8 head / +0x21ddc tail, links +0x310 prev /
 *   +0x314 next) unlinked one by one; +0x2cc = 0; for an activity of a kind (+0x238 != 0) "Activity UnInit" (0x941d90, void(Activity*))
 *   raised on the global manager [0xa93f74]; the state-name map (+0x240) emptied (00586060 on the root, the head linked to itself);
 *   unless kind 4 the network listeners leave the session registry [0xda6ca0] (00587330 / 00587390 / 005873f0); the
 *   "NetworkPlayerModeChanged" listener (00587450), the session block's listeners (006deff0, ESI = 0xda6ca0), "Reset Ghost
 *   Overlays" (005874b0), the DataMiner [0xd92f10]'s "Start" / "End" handlers (00755880: 00753760 / 007538c0) leave; the
 *   activity's channel (+0x290) is removed from the channel registry and deleted (00593900, 00593770); "VehicleEntered" (00587510)
 *   and "PageWipeTransition" (00587580, the UI's [[0xa93fcc] + 0x98]) leave. The session block's static initialiser (006de1f0
 *   under bit 0 of 0xdcf304) is checked before each use. Frame: push ecx / ebx / ebp / esi / edi (+0x10: the event payload). */
void rx_activity_vf3(rx_addr act) {
    ACT_FRAME(F, 0, 5);
    SU32(act, 0x1068, 0);
    SU32(act, 0xec4, 0);
    ACT_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));
    SU32(act, 0x26c, 0);
    SU32(act, 0x280, 0);
    if (U32(act, 0xfd8) != 0) rx_vcall(U32(act, 0xfd8), 0x78, 0, NULL);
    for (rx_addr n = U32(U32(act, 0x234), 0x21dd8); n != 0; n = U32(U32(act, 0x234), 0x21dd8)) {
        if (U32(n, 0x310) != 0) SU32(U32(n, 0x310), 0x314, U32(n, 0x314));
        else SU32(U32(act, 0x234), 0x21ddc, U32(n, 0x314));
        if (U32(n, 0x314) != 0) SU32(U32(n, 0x314), 0x310, U32(n, 0x310));
        else SU32(U32(act, 0x234), 0x21dd8, U32(n, 0x310));
        SU32(n, 0x314, 0);
        SU32(n, 0x310, 0);
    }
    SU32(act, 0x2cc, 0);
    if (U32(act, 0x238) != 0) {
        const rx_addr payload = F + 0x10;
        SU32(payload, 0, act);
        crt_type_info_name(0x009cbd24, 0x009e1a7c);              /* typeid(void (*)(Activity*)).name() */
        SU32(F, -4, 4); SU32(F, -8, payload);
        ACT_CALL(2, rx_world_event_dispatch(U32(0, 0x00a93f74), 0x00941d90 /* "Activity UnInit" */, payload, 4));
    }
    {
        const rx_addr map = act + 0x240;
        SU32(F, -4, U32(U32(act, 0x258), 4));
        ACT_CALL(1, rx_waypoint_state_nodes_destroy(U32(U32(act, 0x258), 4)));
        SU32(U32(map, 0x18), 4, U32(map, 0x18));
        SU32(map, 0x1c, 0);
        SU32(U32(map, 0x18), 0, U32(map, 0x18));
        SU32(U32(map, 0x18), 8, U32(map, 0x18));
    }
    if (U32(act, 0x238) != 4) {
        race_static_dcf304();
        SU32(F, -4, act);
        ACT_CALL(1, rx_activity_unlisten_net_timer(U32(0, 0x00da6ca0), act));
        race_static_dcf304();
        ACT_CALL(1, rx_activity_unlisten_net_session(U32(0, 0x00da6ca0), act));
        race_static_dcf304();
        ACT_CALL(1, rx_activity_unlisten_net_positions(U32(0, 0x00da6ca0), act));
    }
    race_static_dcf304();
    SU32(F, -4, act);
    ACT_CALL(1, rx_activity_unlisten_net_mode(U32(0, 0x00da6ca0), act));
    race_static_dcf304();
    ACT_CALL(0, rx_netmgr_activity_unlisten(0x00da6ca0, act));
    SU32(F, -4, act);
    ACT_CALL(1, rx_activity_unlisten_ghost_reset(U32(act, 0x290), act));
    {
        const rx_addr dm = U32(0, 0x00d92f10), ch = U32(act, 0x290);
        SU32(F, -4, 0x00753760); SU32(F, -8, dm);
        ACT_CALL(2, rx_dataminer_unlisten(ch, 0x00941b94 /* "Start" */, dm, 0x00753760));
        SU32(F, -4, 0x007538c0); SU32(F, -8, dm);
        ACT_CALL(2, rx_dataminer_unlisten(ch, 0x00941b9c /* "End" */, dm, 0x007538c0));
    }
    {
        const rx_addr ch = U32(act, 0x290);
        SU32(F, -4, ch);                                         /* push ecx: 00593900 takes none, it stays as 00593770's second argument */
        rx_set_sp(F - 4);
        rx_addr reg;
        ACT_CALLR(reg, 0, rx_event_channel_registry());
        rx_set_sp(F);
        SU32(F, -8, reg);
        ACT_CALL(2, rx_channel_registry_remove(reg, ch));
    }
    SU32(F, -4, act);
    ACT_CALL(1, rx_activity_unlisten_vehicle_entered(act));
    /* PRODUCT BOUNDARY (as 00556ec0's subscription, build_activity_start.c): without the UI manager ([0x00a93fcc] = 0: the product has no
     * UI) there is no UI event channel (+0x98) and the "PageWipeTransition" listener was never made; the oracle always has the manager */
    if (U32(0, 0x00a93fcc) != 0)
        ACT_CALL(1, rx_activity_unlisten_page_wipe(U32(U32(0, 0x00a93fcc), 0x98), act));
    ACT_END(F);
}
RX_FUNCTION(0x00557790, rx_activity_vf3)

/* ======================================================================================================================== */
/* the activity area's release (Activity::vf6, from the area update 007669f0 after vf3)                                       */

/* the game's thread gate (the engine frame object [core + 0x10]: owner thread +0xcc, event +0xdc): off the owning thread, wait for /
 * signal the event (as the scene functions write it inline) */
static void act_gate_wait(void) {
    const rx_addr g = U32(U32(0, 0x00a95844), 0x10);
    if (rx_os_import(0x0091607c, 0, 0) != U32(g, 0xcc)) {   /* GetCurrentThreadId */
        uint32_t a[2] = {U32(g, 0xdc), 0xffffffffu};
        rx_os_import(0x00916008, 2, a);                      /* WaitForSingleObject */
    }
}
static void act_gate_signal(void) {
    const rx_addr g = U32(U32(0, 0x00a95844), 0x10);
    if (rx_os_import(0x0091607c, 0, 0) != U32(g, 0xcc)) {
        uint32_t a = U32(g, 0xdc);
        if (rx_os_import(0x00916034, 1, &a) == 0) rx_os_import(0x00916078, 0, 0);   /* SetEvent, GetLastError */
    }
}

/* 007d1470 (stdcall subscene record, scene; RET 8): the record's entities (+0x104 array, +0x108 count) leave the scene
 * (SceneManager_RemoveEntity 007d3040), vf9 (released) and are deleted (vf0(1)); the array freed. Frame: push ebx / ebp / esi / edi. */
void rx_scene_subscene_release(rx_addr rec, rx_addr scene) {
    ACT_FRAME(F, 0, 4);
    for (uint32_t i = 0; i < U32(rec, 0x108); i++) {
        ACT_CALL(1, rx_scene_manager_remove_entity(U32(U32(rec, 0x104), 4 * i), scene));
        act_vcall(U32(U32(rec, 0x104), 4 * i), 0x24, 0, NULL);
        const rx_addr e = U32(U32(rec, 0x104), 4 * i);
        if (e != 0) { const uint32_t one = 1; act_vcall(e, 0, 1, &one); }
    }
    ACT_CALL(1, rx_reflex_aligned_free(U32(rec, 0x104)));
    ACT_END(F);
}
RX_FUNCTION(0x007d1470, rx_scene_subscene_release)

/* 007d19f0 (stdcall scene, subscene name; RET 8): behind the game's thread gate, every used subscene record of the scene (+0x38,
 * 0x18 x 0x110: +0 the name, +0x104 / +0x108 the entities, +0x10c in use) of that name released (007d1470) and cleared (+0, +0x10c,
 * +0x108). Without a name (the whole scene's unload: every record, the entity lists +0x2c00 .., 007cf450 / 007dfd10) not reached.
 * Frame: push ebp; mov ebp, esp; and esp, -8; sub esp, 0xc; push ebx / esi / edi (+0xc the record counter). */
void rx_scene_subscene_unload(rx_addr scene, rx_addr name) {
    ACT_FRAME_EBP(F, 8, 0xc, 3);
    act_gate_wait();
    if (name == 0) act_unreached(0x007d1a96, "the whole scene's unload (007d19f0 without a subscene name)");
    rx_addr rec = scene + 0x38;
    for (SU32(F, 0xc, 0x18); U32(F, 0xc) != 0; SU32(F, 0xc, U32(F, 0xc) - 1), rec += 0x110) {
        if (U32(rec, 0x10c) == 0 || crt_strcmp(name, rec) != 0) continue;
        ACT_CALL(2, rx_scene_subscene_release(rec, scene));
        SU8(rec, 0, 0);
        SU32(rec, 0x10c, 0);
        SU32(rec, 0x108, 0);
    }
    act_gate_signal();
    ACT_END(F);
}
RX_FUNCTION(0x007d19f0, rx_scene_subscene_unload)

/* 00767850 (stdcall world; RET 4): the world's racing line (+0x21df0) released: the engine's frame mark (0084ea00), its entity
 * (+0x10) leaves the scene (007d3040), vf9, vf0(1); +0x21df0 = 0. Frame: push ecx / ebx / esi / edi. */
void rx_world_racing_line_release(rx_addr w) {
    ACT_FRAME(F, 0, 4);
    if (U32(w, 0x21df0) != 0) {
        ACT_CALL(0, rx_engine_frame_mark(U32(U32(0, 0x00a95844), 0x10)));
        ACT_CALL(1, rx_scene_manager_remove_entity(U32(w, 0x21df0), U32(U32(w, 0x21df0), 0x10)));
        act_vcall(U32(w, 0x21df0), 0x24, 0, NULL);
        if (U32(w, 0x21df0) != 0) { const uint32_t one = 1; act_vcall(U32(w, 0x21df0), 0, 1, &one); }
        SU32(w, 0x21df0, 0);
    }
    ACT_END(F);
}
RX_FUNCTION(0x00767850, rx_world_racing_line_release)

/* Activity::vf6 00557a10 (thiscall; flag; RET 4): unless a series activity (type +0x23c > 0x1e) with flag 0, the activity area's
 * subscene (+0x118, loaded: +0xec8) unloaded from the world's scene ([+0x234] + 0x20138; 007d19f0) and +0xec8 = 0, then the world's
 * racing line released (tail jump to 00767850 with the world in the flag's slot). Frame: push esi. */
void rx_activity_vf6(rx_addr act, uint32_t flag) {
    const rx_addr sp0 = rx_sp();
    ACT_FRAME(F, 0, 1);
    if (U32(act, 0x23c) >= 0x1f && flag == 0) { ACT_END(F); return; }
    if (U32(act, 0xec8) != 0) {
        ACT_CALL(2, rx_scene_subscene_unload(U32(U32(act, 0x234), 0x20138), act + 0x118));
        SU32(act, 0xec8, 0);
    }
    ACT_END(F);
    SU32(sp0, 4, U32(act, 0x234));                               /* mov [esp + 4], eax; jmp 00767850 (entered at this ESP) */
    rx_world_racing_line_release(U32(act, 0x234));
}
RX_FUNCTION(0x00557a10, rx_activity_vf6)
