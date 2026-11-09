/* build_vcall_thunks.c -- the member-function-pointer thunks of the event system (0x00592a60 .. 0x00592bc0: `mov eax, [ecx]; jmp [eax + slot * 4]`): a delegate (EventHandlerProxy
 * +0x10) that names a virtual method points at one of them; the thunk tail-jumps to the object's virtual with the delegate's argument dwords still on the stack. The core's
 * call is the object's registered virtual with those dwords (up to four: the callee's own signature takes what it needs). Product only: the oracle runs the original code
 * (0x00592a60 / 0x00592ba0 / 0x005c3310 are ported with adapters in build_activity_freeride_run.c / build_event_proxies.c).
 *
 * The functions and their RX_FUNCTION registrations are written out one by one (no macro): a product event dispatch reaches a delegate's virtual through
 * rx_call_original, which may only call the core functions tools/gen_fnsig.py lists, and that scan reads a literal `RX_FUNCTION(0x...)` AND the function definition. With the
 * macro form neither was visible, so a delegate naming any of these thunks stopped the product with "unported game function" (first hit: 00592b60, the "PhysicsStuntBegin"
 * handler of RaceActivityAudioLogic -> slot 6 vf6 0x005818c0, builds/stuntaudio). Oracle build: no change (it runs the originals). */
#include "build_activity_util.h"
#include "rx_vcall.h"

#ifndef RX_ORACLE
/* the thunk body: the object's virtual at byte offset `off` (the delegate's dwords as stack arguments) */
#define VCALL_THUNK_BODY(off) \
    const uint32_t a[4] = {a0, a1, a2, a3}; \
    return rx_vcall(obj, (off), 4, a);

uint32_t rx_vcall_thunk_592a70(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x4c) }
uint32_t rx_vcall_thunk_592a80(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x5c) }
uint32_t rx_vcall_thunk_592a90(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x6c) }
uint32_t rx_vcall_thunk_592aa0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x20) }
uint32_t rx_vcall_thunk_592ab0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x30) }
uint32_t rx_vcall_thunk_592ac0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x40) }
uint32_t rx_vcall_thunk_592ad0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x50) }
uint32_t rx_vcall_thunk_592ae0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x60) }
uint32_t rx_vcall_thunk_592af0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x70) }
uint32_t rx_vcall_thunk_592b00(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x14) }
uint32_t rx_vcall_thunk_592b10(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x24) }
uint32_t rx_vcall_thunk_592b30(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x44) }
uint32_t rx_vcall_thunk_592b40(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x54) }
uint32_t rx_vcall_thunk_592b50(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x64) }
uint32_t rx_vcall_thunk_592b60(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x18) }
uint32_t rx_vcall_thunk_592b80(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x48) }
uint32_t rx_vcall_thunk_592b90(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x58) }
uint32_t rx_vcall_thunk_592bb0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x98) }
uint32_t rx_vcall_thunk_592bc0(rx_addr obj, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) { VCALL_THUNK_BODY(0x2c) }
#undef VCALL_THUNK_BODY

RX_FUNCTION(0x00592a70, rx_vcall_thunk_592a70)
RX_FUNCTION(0x00592a80, rx_vcall_thunk_592a80)
RX_FUNCTION(0x00592a90, rx_vcall_thunk_592a90)
RX_FUNCTION(0x00592aa0, rx_vcall_thunk_592aa0)
RX_FUNCTION(0x00592ab0, rx_vcall_thunk_592ab0)
RX_FUNCTION(0x00592ac0, rx_vcall_thunk_592ac0)
RX_FUNCTION(0x00592ad0, rx_vcall_thunk_592ad0)
RX_FUNCTION(0x00592ae0, rx_vcall_thunk_592ae0)
RX_FUNCTION(0x00592af0, rx_vcall_thunk_592af0)
RX_FUNCTION(0x00592b00, rx_vcall_thunk_592b00)
RX_FUNCTION(0x00592b10, rx_vcall_thunk_592b10)
RX_FUNCTION(0x00592b30, rx_vcall_thunk_592b30)
RX_FUNCTION(0x00592b40, rx_vcall_thunk_592b40)
RX_FUNCTION(0x00592b50, rx_vcall_thunk_592b50)
RX_FUNCTION(0x00592b60, rx_vcall_thunk_592b60)
RX_FUNCTION(0x00592b80, rx_vcall_thunk_592b80)
RX_FUNCTION(0x00592b90, rx_vcall_thunk_592b90)
RX_FUNCTION(0x00592bb0, rx_vcall_thunk_592bb0)
RX_FUNCTION(0x00592bc0, rx_vcall_thunk_592bc0)
#endif
