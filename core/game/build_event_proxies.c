/* build_event_proxies.c -- slot 2 of EventHandlerProxy templates (the proxy {+0 vtable, +8 the signature's type name, +0xc object / function, +0x10 member
 * function}; core/game/build_scene_props.c registers the handlers). The handler runs through the core's function when one is registered at its address,
 * else the original (oracle; the product aborts naming it). */
#include "build_activity_util.h"

/* 0041a3b0 EventHandlerProxy<void (*)(Activity*)>::vf2 (thiscall proxy, stack payload pointer; RET 4): the plain function in +0xc called with the activity
 * the payload points to (**payload) */
void rx_event_proxy_call_fn_activity(rx_addr proxy, rx_addr payload) {
    const uint32_t a[1] = {U32(U32(payload, 0), 0)};
    act_call_fn(U32(proxy, 0xc), RX_CDECL, 0, 1, a);
}
RX_FUNCTION(0x0041a3b0, rx_event_proxy_call_fn_activity)

/* 007606d0 EventHandlerProxy<void (Activity::*)(NetTimerEventType, float)>::vf2 (thiscall proxy, stack payload pointer; RET 4): the member function +0x10 on the object +0xc
 * with the payload's two words (the event type and a float passed through the x87: a signalling NaN comes out quiet); the callee cleans its two arguments */
void rx_event_proxy_call_member_int_float(rx_addr proxy, rx_addr payload) {
    const rx_addr args = U32(payload, 0);
    const uint32_t kind = U32(args, 0);
    uint32_t fb = U32(args, 4);
    if ((fb & 0x7f800000u) == 0x7f800000u && (fb & 0x7fffffu)) fb |= 0x400000u;
    const rx_addr fn = U32(proxy, 0x10), obj = U32(proxy, 0xc);
    void (*f)(rx_addr, uint32_t, float) = RX_FN(fn, void (*)(rx_addr, uint32_t, float));
    if (f) { ACT_CALL(2, f(obj, kind, rx_bf(fb))); return; }
    const uint32_t a[2] = {kind, fb};
    rx_call_original(fn, RX_THISCALL, obj, 2, a);
}
RX_FUNCTION(0x007606d0, rx_event_proxy_call_member_int_float)

#ifndef RX_ORACLE   /* the tail-jump thunks run in the product only: the oracle runs the original `jmp` (a native cannot reproduce a jump out of the verified call) */
/* a tail jump (`jmp [vtable + n]` / `jmp edx` with the caller's arguments still on the stack): the target is entered at the thunk's own stack pointer, so the core call
 * starts 4 * n + 4 bytes higher than act_call_fn's pushes and lands where the jump does */
static uint32_t tail_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a) {
    const rx_addr sp = rx_sp();
    rx_set_sp(sp + 4u * (uint32_t)n + 4u);
    const uint32_t r = act_call_fn(fn, conv, ecx, n, a);
    rx_set_sp(sp);
    return r;
}

/* 005c3310 (thiscall ECX = the object; a pointer to a member function that is the virtual slot 14: `mov eax, [ecx]; jmp [eax + 0x38]`): the virtual with the
 * delegate's two argument dwords (the delegate invoker 00554fc0 passes two; the original's tail jump leaves the callee's own RET 8 to clean them) */
uint32_t rx_vslot14_thunk(rx_addr obj, uint32_t a0, uint32_t a1) {
    const uint32_t a[2] = {a0, a1};
    return tail_call(U32(U32(obj, 0), 0x38), RX_THISCALL, obj, 2, a);
}
RX_FUNCTION(0x005c3310, rx_vslot14_thunk)

/* 005bfd80 EventHandlerProxy<void (Class::*)(Activity*)>::vf2 (thiscall proxy, stack payload pointer; RET 4; shared by 22 classes): the member function +0x10 on the object
 * +0xc with the activity the payload points to (**payload) */
void rx_event_proxy_call_member_activity(rx_addr proxy, rx_addr payload) {
    const rx_addr sp = rx_sp();
    const uint32_t a[1] = {U32(U32(payload, 0), 0)};
    SU32(sp, 4, a[0]);                                                  /* mov [esp + 4], ecx: the argument slot now holds the activity */
    tail_call(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0xc), 1, a);
}
RX_FUNCTION(0x005bfd80, rx_event_proxy_call_member_activity)

/* 006c55a0 (thiscall proxy, stack payload pointer; the same with the member function's this-adjustor): the object is +0xc + +0x14 */
void rx_event_proxy_call_member_adj_activity(rx_addr proxy, rx_addr payload) {
    const rx_addr sp = rx_sp();
    const uint32_t a[1] = {U32(U32(payload, 0), 0)};
    SU32(sp, 4, a[0]);
    tail_call(U32(proxy, 0x10), RX_THISCALL, U32(proxy, 0xc) + U32(proxy, 0x14), 1, a);
}
RX_FUNCTION(0x006c55a0, rx_event_proxy_call_member_adj_activity)
#endif
