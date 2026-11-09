/* tuning_reload_log.h -- the "tuning reloaded" log line of the developer XML watch (00646a50 camera, 0043b640 moto, 0048d320 ATV): after a pending
 * tuning file was applied, the callers publish a type-10 packet with no payload to the tuning registry's sink:
 *     push 0 / 0 / 10 / ecx; call 0042fed0 (cdecl, no arguments: it ignores the four pushed words); add esp, 4;
 *     push eax (the sink); call 00790540 (stdcall sink, 10, 0, 0; RET 16).
 * Both callees are entered at the original's ESP (16 bytes of arguments and the return address below the caller's), with the words on the stack. */
#pragma once
#include "tuning_registry.h"
#include "fourwheel_xml_packet.h"

static inline void rx_tuning_reload_log(uint32_t ecx, rx_addr ret_registry, rx_addr ret_publish) {
    const rx_addr sp = rx_sp();
    rx_addr sink;
    SU32(sp, -4, 0);
    SU32(sp, -8, 0);
    SU32(sp, -12, 10);
    SU32(sp, -16, ecx);
    SU32(sp, -20, ret_registry);
    rx_set_sp(sp - 20u);
    sink = rx_tuning_registry_get();                        /* 0042fed0 */
    rx_set_sp(sp);
    SU32(sp, -16, sink);                                    /* add esp, 4; push eax: the sink replaces the ECX word */
    SU32(sp, -20, ret_publish);
    rx_set_sp(sp - 20u);
    rx_tuning_packet_publish(sink, 10, 0, 0);               /* 00790540 */
    rx_set_sp(sp);
}
