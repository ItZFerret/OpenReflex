/* net_boundary.h -- ONLINE BOUNDARY (Quazal / the online content / the network session objects): calls of game code into the online
 * subsystems. The product has no online session: the objects these functions work on are made only by the Quazal initializer
 * (006de1f0's session block fields +0x44 lobby / +0x48 voice / +0x10 network session, the Quazal session object [0x00d60d54], the
 * XenonOnlineContent object [0x00a94330] / vtable 0x00961100), none of which the core constructs - every call site is behind a guard that
 * is false there. In the oracle the original runs (so Verify of the calling function sees the same memory, and the worlds that do have those
 * objects keep working); in the product a call that is reached anyway is fatal and names the address (rx_net_call*): there is no silent skip.
 * Each use names the function and the guard that keeps the product away from it. */
#pragma once
#include "rx_game.h"
#include <stdio.h>
#include <stdlib.h>

/* a path that needs an online session: the product (and the oracle worlds) never take it */
static inline void rx_net_unreached(rx_addr fn, const char* what) {
    fprintf(stderr, "rxcore: online boundary: %08x %s: the product has no online session (not ported)\n", (unsigned)fn, what);
    abort();
}

#ifdef RX_ORACLE
static inline uint32_t rx_net_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a, const char* what) {
    (void)what;
    return rx_call_original(fn, conv, ecx, n, a);
}
static inline void rx_net_call_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int n, const uint32_t* a, const char* what) {
    (void)what;
    rx_regs scratch;
    rx_call_original_ex(fn, in, out ? out : &scratch, n, a);
}
#else
static inline uint32_t rx_net_call(rx_addr fn, enum rx_conv conv, uint32_t ecx, int n, const uint32_t* a, const char* what) {
    (void)conv; (void)ecx; (void)n; (void)a;
    rx_net_unreached(fn, what);
    return 0;
}
static inline void rx_net_call_ex(rx_addr fn, const rx_regs* in, rx_regs* out, int n, const uint32_t* a, const char* what) {
    (void)in; (void)out; (void)n; (void)a;
    rx_net_unreached(fn, what);
}
#endif
