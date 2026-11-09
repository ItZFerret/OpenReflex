/* rx_fn.h -- the core's function registry, keyed by original address.
 * Every ported function registers itself (RX_FUNCTION). The game stores original function addresses in its objects (vtables,
 * callbacks), so a virtual call reads the address from the object and looks up the core implementation:
 *     void (*f)(rx_addr self, float dt) = RX_FN(U32(vtable, 0x38), void (*)(rx_addr, float));
 *     if (f) f(self, dt); else rx_call_original(...)   -- or use the RX_VCALL helpers.
 * Direct calls to functions another agent has not ported yet: rx_call_original / rx_call_original_ex (src/rx_mem.h). In the
 * product an unregistered address is a fatal error naming it (coverage tells what is left). */
#pragma once
#include "rx_mem.h"

void  rx_fn_register(rx_addr address, void* fn, const char* name);
void* rx_fn(rx_addr address);                 /* the core implementation, or NULL */
const char* rx_fn_name(rx_addr address);
int   rx_fn_count(void);

#ifdef RX_FN_TRACE
/* -DRX_FN_TRACE (w-vtlog; OFF in every normal build): every typed path records (call site file:line, the C type as written, the target
 * address, the return addresses of the five frames above) in a small lock-free set, written to the file named by $RX_FN_TRACE_OUT at exit,
 * on SIGUSR1 or on a fatal signal (and by rx_fn_trace_dump). tools/vtrace_check.py compares each record with the target port's prototype.
 *   RX_FN(addr, type)          -> type = the stringified C type
 *   rx_fn(addr) in game code   -> type = NULL (a presence test, or a `void* f` cast later: the cast types come from the static scan)
 *   rx_tcall / rx_vcall        -> type = "rx_tcall/<n>" (integer path: n integer arguments)
 * The expansion without the switch is the plain ((type)rx_fn(address)) below. */
void* rx_fn_traced(rx_addr address, const char* file, int line, const char* type);
void rx_fn_trace_dump(const char* path);
#define RX_FN(address, type) ((type)rx_fn_traced((address), __FILE__, __LINE__, #type))
#define rx_fn(address) rx_fn_traced((address), __FILE__, __LINE__, NULL)
const char* rx_fn_trace_ints(int total);
/* the generic integer dispatchers (rr_vcall, act_call_fn, veh_vcall ... : `total` integer parameters, integer result, chosen at run time) */
#define RX_FN_INTS(address, total) rx_fn_traced((address), __FILE__, __LINE__, rx_fn_trace_ints((int)(total)))
#else
#define RX_FN(address, type) ((type)rx_fn(address))
#define RX_FN_INTS(address, total) rx_fn(address)       /* same expansion as before the switch existed */
#endif
/* RX_FUNCTION(0x0085d270, rx_mechanism_integrate_velocities); -- at file scope, after the function. */
#ifdef RX_FNSIG_SCAN
/* tools/gen_fnsig.py preprocesses every game C file with -DRX_FNSIG_SCAN: the registration stays a plain marker in the preprocessor
 * output, with the address / name arguments expanded, so registrations that come out of a macro family
 * (build_event_proxy_equals.c PROXY_EQ*, build_scene_start.c START_CTOR, race_ai_params.c AI_PARAM, ...) are visible to
 * the scan exactly like literal ones. Never compiled: the preprocessor output is only parsed. */
#define RX_FUNCTION(address, fn) RX_FNSIG_REG(address, fn)
#else
#define RX_FUNCTION(address, fn) \
    __attribute__((constructor)) static void rx_reg_##fn(void) { rx_fn_register((address), (void*)(fn), #fn); }
#endif
