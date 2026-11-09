/* build_rider_misc.c -- small rider-unit helpers found while chaining the product (core/plan/QUEUE.md "chain"). */
#include "rx_game.h"
#include "crt.h"
#include "build_rider_misc.h"

/* 0063b360 / 0063b3f0 (stdcall vector; RET 4): every 8-byte element of the vector {+0 checked-iterator proxy, +0xc begin, +0x10 end} gets its
 * second word (+4) set to 1 / 0 (the animators' listener lists: the "valid" word of each {target, valid}). The checked iterator's validation
 * (the proxy must exist and be the vector's; the element address below the owner's end, checked before and after the store) is the original's
 * _invalid_parameter_noinfo calls. */
static void vec8_fill(rx_addr v, uint32_t value) {
    rx_addr it = U32(v, 0xc);
    if (((U32(v, 0x10) - it) & 0xfffffff8u) == 0) return;
    if (it > U32(v, 0x10)) crt_invalid_parameter_noinfo();
    for (;;) {
        const rx_addr proxy = U32(v, 0);
        const uint32_t e = U32(v, 0x10);
        if (U32(v, 0xc) > e) crt_invalid_parameter_noinfo();
        if (proxy == 0 || proxy != U32(v, 0)) crt_invalid_parameter_noinfo();
        if (it == e) break;
        for (int k = 0; k < 2; k++) {
            rx_addr owner = 0;
            if (proxy == 0) crt_invalid_parameter_noinfo(); else owner = U32(proxy, 0);
            if (it >= U32(owner, 0x10)) crt_invalid_parameter_noinfo();
            if (k == 0) SU32(it, 4, value);
        }
        it += 8;
    }
}
void rx_rider_vec8_mark_all(rx_addr v) { vec8_fill(v, 1); }
RX_FUNCTION(0x0063b360, rx_rider_vec8_mark_all)
void rx_rider_vec8_clear_all(rx_addr v) { vec8_fill(v, 0); }
RX_FUNCTION(0x0063b3f0, rx_rider_vec8_clear_all)
